using System.Net;
using System.Text;

namespace WikipediaCorpus;

internal static class WikiText
{
    private static readonly HashSet<string> MetadataSections = new(StringComparer.OrdinalIgnoreCase)
    {
        "References", "Notes", "Notes and references", "Footnotes", "Sources", "Bibliography", "Further reading", "External links", "See also",
        "Einzelnachweise", "Anmerkungen", "Nachweise", "Belege", "Quellen", "Literatur", "Weblinks", "Siehe auch",
        "Notes et références", "Références", "Bibliographie", "Liens externes", "Voir aussi", "Articles connexes",
        "Referencias", "Notas", "Bibliografía", "Fuentes", "Enlaces externos", "Véase también", "Lecturas adicionales",
        "Referências", "Referências e notas", "Bibliografia", "Fontes", "Ligações externas", "Links externos", "Ver também",
        "Παραπομπές", "Σημειώσεις", "Πηγές", "Βιβλιογραφία", "Εξωτερικοί σύνδεσμοι", "Δείτε επίσης", "Βλέπε επίσης",
    };

    private static readonly HashSet<string> MetadataNamespaces = new(StringComparer.OrdinalIgnoreCase)
    {
        "file", "image", "media", "category", "template", "special", "help", "wikipedia", "user", "talk", "portal", "draft", "module",
        "commons", "wikidata",
        "datei", "bild", "medium", "kategorie", "vorlage", "spezial", "hilfe", "benutzer", "diskussion", "modul",
        "fichier", "média", "catégorie", "modèle", "spécial", "aide", "utilisateur", "discussion", "portail",
        "archivo", "imagen", "categoría", "plantilla", "especial", "ayuda", "usuario", "discusión", "módulo",
        "ficheiro", "arquivo", "imagem", "categoria", "predefinição", "ajuda", "usuário", "utilizador", "discussão",
        "αρχείο", "εικόνα", "μέσο", "κατηγορία", "πρότυπο", "ειδικό", "βοήθεια", "χρήστης", "συζήτηση", "πύλη",
    };

    private static readonly HashSet<string> BlockTags = new(StringComparer.OrdinalIgnoreCase)
    {
        "address", "article", "blockquote", "br", "div", "hr", "p", "section", "ul", "ol", "dl", "li", "dt", "dd",
        "h1", "h2", "h3", "h4", "h5", "h6", "center", "figure", "figcaption",
    };

    private static readonly HashSet<string> HiddenBlocks = new(StringComparer.OrdinalIgnoreCase)
    {
        "references", "gallery", "timeline", "table", "imagemap", "mapframe", "maplink", "graph", "score", "syntaxhighlight", "source",
        "script", "style", "pre", "includeonly", "templatedata", "figure", "ul", "ol", "dl",
    };

    private static readonly HashSet<string> HiddenInline = new(StringComparer.OrdinalIgnoreCase)
    {
        "ref", "math", "chem", "ce", "code", "nowiki", "indicator",
    };

    public static IEnumerable<string> Paragraphs(string markup, CancellationToken cancellation = default)
    {
        var cleaned = new StringBuilder(Math.Min(markup.Length, 4096));
        AppendMarkup(markup, 0, markup.Length, cleaned, cancellation);
        using var reader = new StringReader(cleaned.ToString());
        var paragraph = new StringBuilder();
        int metadataLevel = 0;
        while (reader.ReadLine() is { } raw)
        {
            cancellation.ThrowIfCancellationRequested();
            string line = raw.Trim();
            int level = HeadingLevel(line);
            if (level > 0)
            {
                if (metadataLevel != 0 && level <= metadataLevel) metadataLevel = 0;
                if (metadataLevel == 0 && MetadataSections.Contains(WebUtility.HtmlDecode(line[level..^level].Trim())))
                    metadataLevel = level;
            }

            if (metadataLevel != 0 || level > 0 || IsNonProseLine(line))
            {
                if (TakeParagraph(paragraph) is { } completed) yield return completed;
                continue;
            }

            if (paragraph.Length != 0) paragraph.Append(' ');
            paragraph.Append(line);
        }

        if (TakeParagraph(paragraph) is { } final) yield return final;
    }

    private static int HeadingLevel(string line)
    {
        int level = 0;
        while (level < line.Length && line[level] == '=') level++;
        if (level is < 1 or > 6 || line.Length <= level * 2) return 0;
        for (int suffix = 1; suffix <= level; suffix++)
            if (line[^suffix] != '=') return 0;
        return level;
    }

    private static bool IsNonProseLine(string line) => line.Length == 0 ||
        line[0] is '*' or '#' or ';' or ':' or '|' ||
        line.StartsWith("----", StringComparison.Ordinal) ||
        (line.StartsWith("__", StringComparison.Ordinal) && line.EndsWith("__", StringComparison.Ordinal));

    private static string? TakeParagraph(StringBuilder paragraph)
    {
        if (paragraph.Length == 0) return null;
        string decoded = WebUtility.HtmlDecode(paragraph.ToString());
        paragraph.Clear();
        bool spacePending = false;
        foreach (char character in decoded)
        {
            if (char.IsWhiteSpace(character))
            {
                spacePending = paragraph.Length > 0;
                continue;
            }
            if (char.IsControl(character) || character is '\u00ad' or '\u200b' or '\u200e' or '\u200f' or '\ufeff') continue;
            if (spacePending) paragraph.Append(' ');
            spacePending = false;
            paragraph.Append(character);
        }
        string? result = paragraph.Length == 0 ? null : paragraph.ToString();
        paragraph.Clear();
        return result;
    }

    private static void AppendMarkup(string text, int start, int end, StringBuilder output, CancellationToken cancellation, int linkDepth = 0)
    {
        for (int index = start; index < end;)
        {
            if ((index & 4095) == 0) cancellation.ThrowIfCancellationRequested();
            char current = text[index];
            if (current == '<' && Matches(text, index, end, "<!--"))
            {
                index = EndOfComment(text, index, end);
                continue;
            }
            if (Opener(text, index, end) is { } construct)
            {
                int close = FindClosing(text, index, end, construct, cancellation, out int separator);
                if (close < 0) return;
                int after = close + Closer(construct).Length;
                if (construct == Construct.Link)
                    AppendLink(text, index + 2, close, separator, output, cancellation, linkDepth);
                else if (construct == Construct.Table || text.AsSpan(index, after - index).ContainsAny('\r', '\n'))
                    output.Append("\n\n");
                else
                    SeparateWords(text, after, end, output);
                index = after;
                continue;
            }
            if (current == '<' && TryReadTag(text, index, end, out HtmlTag tag))
            {
                bool block = HiddenBlocks.Contains(tag.Name);
                bool hidden = block || HiddenInline.Contains(tag.Name);
                index = hidden && !tag.Closing && !tag.SelfClosing ? EndOfElement(text, tag, end, cancellation) : tag.End;
                if (block || BlockTags.Contains(tag.Name)) output.Append("\n\n");
                else if (hidden) SeparateWords(text, index, end, output);
                continue;
            }
            if (current == '[' && IsExternalUrl(text, index + 1, end))
            {
                int close = text.IndexOf(']', index + 1, end - index - 1);
                int label = index + 1;
                while (close >= 0 && label < close && !char.IsWhiteSpace(text[label])) label++;
                if (close >= 0)
                {
                    while (label < close && char.IsWhiteSpace(text[label])) label++;
                    if (label < close && linkDepth < 8)
                        AppendMarkup(text, label, close, output, cancellation, linkDepth + 1);
                    else
                        SeparateWords(text, close + 1, end, output);
                    index = close + 1;
                    continue;
                }
            }
            if (current == '\'' && Matches(text, index, end, "''"))
            {
                do { index++; } while (index < end && text[index] == '\'');
                continue;
            }
            output.Append(current);
            index++;
        }
    }

    private static void AppendLink(string text, int start, int end, int separator, StringBuilder output,
        CancellationToken cancellation, int linkDepth)
    {
        var target = text.AsSpan(start, (separator < 0 ? end : separator) - start).Trim().TrimStart(':').TrimStart();
        int colon = target.IndexOf(':');
        if (colon > 0 && IsMetadataNamespace(target[..colon].Trim()))
        {
            output.Append("\n\n");
            return;
        }
        if (separator >= 0 && separator + 1 < end && linkDepth < 8)
        {
            AppendMarkup(text, separator + 1, end, output, cancellation, linkDepth + 1);
            return;
        }

        int anchor = target.IndexOf('#');
        if (anchor >= 0) target = anchor == 0 ? target[1..] : target[..anchor];
        if (target.Contains("{{", StringComparison.Ordinal)) return;
        foreach (char character in target)
            output.Append(character == '_' ? ' ' : character);
    }

    private static bool IsMetadataNamespace(ReadOnlySpan<char> prefix)
    {
        if (MetadataNamespaces.Contains(prefix.ToString())) return true;
        // Interlanguage prefixes start with a two- or three-letter language code.
        int hyphen = prefix.IndexOf('-');
        var code = hyphen < 0 ? prefix : prefix[..hyphen];
        if (code.Length is < 2 or > 3) return false;
        foreach (char character in prefix)
            if (!char.IsAsciiLetterLower(character) && character != '-') return false;
        return true;
    }

    private static bool IsExternalUrl(string text, int start, int end) =>
        Matches(text, start, end, "https://", StringComparison.OrdinalIgnoreCase) ||
        Matches(text, start, end, "http://", StringComparison.OrdinalIgnoreCase) ||
        Matches(text, start, end, "ftp://", StringComparison.OrdinalIgnoreCase) || Matches(text, start, end, "//");

    private static void SeparateWords(string text, int index, int end, StringBuilder output)
    {
        if (output.Length != 0 && index < end && char.IsLetterOrDigit(output[^1]) && char.IsLetterOrDigit(text[index]))
            output.Append(' ');
    }

    private enum Construct { Link, Table, Template, Parameter }

    private static Construct? Opener(string text, int start, int end)
    {
        if (text[start] == '[' && Matches(text, start, end, "[[")) return Construct.Link;
        if (text[start] != '{') return null;
        if (Matches(text, start, end, "{{{")) return Construct.Parameter;
        if (Matches(text, start, end, "{{")) return Construct.Template;
        return Matches(text, start, end, "{|") ? Construct.Table : null;
    }

    private static string Closer(Construct construct) => construct switch
    {
        Construct.Link => "]]",
        Construct.Table => "|}",
        Construct.Parameter => "}}}",
        _ => "}}",
    };

    private static int FindClosing(string text, int start, int end, Construct construct, CancellationToken cancellation, out int separator)
    {
        var current = construct;
        string close = Closer(current);
        Stack<Construct>? parents = null;
        separator = -1;
        for (int index = start + close.Length; index < end;)
        {
            if ((index & 4095) == 0) cancellation.ThrowIfCancellationRequested();
            if (Matches(text, index, end, close))
            {
                if (parents is null || parents.Count == 0) return index;
                index += close.Length;
                current = parents.Pop();
                close = Closer(current);
                continue;
            }
            if (text[index] == '<')
            {
                if (Matches(text, index, end, "<!--"))
                {
                    index = EndOfComment(text, index, end);
                    continue;
                }
                if (TryReadTag(text, index, end, out HtmlTag tag))
                {
                    bool hidden = HiddenInline.Contains(tag.Name) || HiddenBlocks.Contains(tag.Name);
                    index = hidden && !tag.Closing && !tag.SelfClosing ? EndOfElement(text, tag, end, cancellation) : tag.End;
                    continue;
                }
            }
            if (Opener(text, index, end) is { } nested)
            {
                (parents ??= new()).Push(current);
                current = nested;
                close = Closer(current);
                index += close.Length;
                continue;
            }
            if (construct == Construct.Link && separator < 0 && text[index] == '|' && (parents is null || parents.Count == 0))
                separator = index;
            index++;
        }
        return -1;
    }

    private static int EndOfComment(string text, int start, int end)
    {
        int close = text.IndexOf("-->", start + 4, end - start - 4, StringComparison.Ordinal);
        return close < 0 ? end : close + 3;
    }

    private static bool TryReadTag(string text, int start, int end, out HtmlTag tag)
    {
        tag = default;
        int index = start + 1;
        bool closing = index < end && text[index] == '/';
        if (closing) index++;
        if (index == end || !char.IsAsciiLetter(text[index])) return false;
        int nameStart = index;
        while (index < end && (char.IsAsciiLetterOrDigit(text[index]) || text[index] == '-')) index++;
        int nameEnd = index;
        char quote = '\0';
        for (; index < end; index++)
        {
            char current = text[index];
            if (quote != '\0')
            {
                if (current == quote) quote = '\0';
            }
            else if (current is '\'' or '"') quote = current;
            else if (current == '<') return false;
            else if (current == '>')
            {
                int last = index - 1;
                while (last >= nameEnd && char.IsWhiteSpace(text[last])) last--;
                tag = new(text[nameStart..nameEnd], closing, text[last] == '/', index + 1);
                return true;
            }
        }
        return false;
    }

    private static int EndOfElement(string text, HtmlTag opening, int end, CancellationToken cancellation)
    {
        int depth = 1;
        for (int offset = opening.End; offset < end;)
        {
            cancellation.ThrowIfCancellationRequested();
            int next = text.IndexOf('<', offset, end - offset);
            if (next < 0) break;
            if (Matches(text, next, end, "<!--"))
            {
                offset = EndOfComment(text, next, end);
                continue;
            }
            offset = next + 1;
            if (!TryReadTag(text, next, end, out HtmlTag tag)) continue;
            offset = tag.End;
            if (!string.Equals(tag.Name, opening.Name, StringComparison.OrdinalIgnoreCase)) continue;
            if (tag.Closing && --depth == 0) return offset;
            if (!tag.Closing && !tag.SelfClosing) depth++;
        }
        return end;
    }

    private static bool Matches(string text, int start, int end, string value, StringComparison comparison = StringComparison.Ordinal) =>
        end - start >= value.Length && text.AsSpan(start, value.Length).Equals(value, comparison);

    private readonly record struct HtmlTag(string Name, bool Closing, bool SelfClosing, int End);
}
