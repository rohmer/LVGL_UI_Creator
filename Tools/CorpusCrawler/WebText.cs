using System.Text;
using System.Text.RegularExpressions;
using AngleSharp.Dom;
using AngleSharp.Html.Dom;
using Lingua;

namespace CorpusCrawler;

internal static class WebText
{
    private static readonly HashSet<string> Hidden = new(StringComparer.OrdinalIgnoreCase)
    {
        "script", "style", "noscript", "template", "svg", "canvas", "nav", "footer", "aside", "form",
    };

    private static readonly HashSet<string> Blocks = new(StringComparer.OrdinalIgnoreCase)
    {
        "address", "article", "blockquote", "br", "dd", "div", "dl", "dt", "figcaption", "figure",
        "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "li", "main", "ol", "p", "pre",
        "section", "table", "td", "th", "tr", "ul",
    };

    private static readonly Regex Whitespace = new(@"\s+");
    private static readonly Regex SentenceEnding = new("(?<!\\.)[.!?][\\\"'’”»)}\\]]*\\z");
    private static readonly Regex GreekSentenceEnding = new("(?<!\\.)[.!?;\u037e][\\\"'’”»)}\\]]*\\z");
    private static readonly HashSet<string> NavigationRoles = new(StringComparer.OrdinalIgnoreCase)
    {
        "banner", "complementary", "contentinfo", "menu", "menubar", "menuitem", "navigation", "search",
    };

    public static bool HasSentenceEnding(string text, Language language) =>
        !text.AsSpan().ContainsAny('\r', '\n') &&
        (language is Language.Arabic or Language.Hindi or Language.Chinese or Language.Bengali or Language.Indonesian or Language.Urdu or Language.Russian
            ? UnicodeSentenceFallback.HasEnding(text)
            : (language == Language.Greek ? GreekSentenceEnding : SentenceEnding).IsMatch(text));

    public static IEnumerable<string> TextBlocks(IHtmlDocument document)
    {
        if (document.Body is null) yield break;
        var text = new StringBuilder();
        var nodes = new Stack<(INode Node, bool Exit)>();
        nodes.Push((document.Body, false));
        while (nodes.TryPop(out var item))
        {
            if (item.Node is IText literal)
            {
                // HTML source line breaks are whitespace within the running text.
                text.Append(Whitespace.Replace(literal.Data, " "));
                continue;
            }
            if (item.Node is not IElement element || Hidden.Contains(element.LocalName) ||
                element.HasAttribute("hidden") || element.GetAttribute("aria-hidden") == "true" ||
                NavigationRoles.Contains(element.GetAttribute("role")?.Trim() ?? "")) continue;

            // Keep standalone links separate while preserving links embedded in prose.
            bool blockBoundary = Blocks.Contains(element.LocalName) ||
                (element.LocalName == "a" && element.Closest("p, blockquote, figcaption, dd, li") is null);
            if (blockBoundary && text.Length > 0)
            {
                var block = text.ToString().Trim();
                if (block.Length > 0) yield return block;
                text.Clear();
            }
            if (item.Exit) continue;
            if (blockBoundary) nodes.Push((element, true));
            for (var child = element.LastChild; child is not null; child = child.PreviousSibling)
                nodes.Push((child, false));
        }
        var lastBlock = text.ToString().Trim();
        if (lastBlock.Length > 0) yield return lastBlock;
    }
}
