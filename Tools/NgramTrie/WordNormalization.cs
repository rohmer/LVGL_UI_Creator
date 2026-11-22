using System.Globalization;
using System.Text;

namespace NgramTrie;

internal static class WordNormalization
{
    private static readonly Lazy<Dictionary<int, string>> Foldings = new(LoadFoldings);

    public static bool UsesListedSpelling(string? language) => language is "fr" or "de" or "el";
    public static bool UsesUnicodeTokens(string? language) => language is "ar" or "hi" or "zh" or "bn" or "id" or "ur" or "ru";
    public static bool SupportsLanguage(string language) => language is "en" or "es" or "fr" or "de" or "el" || UsesUnicodeTokens(language);
    public static bool IsSentenceBoundary(Rune rune) => rune.Value is '.' or '\r' or '\n' or 0x061f or 0x06d4 or 0x0964 or 0x0965 or 0x3002 or 0xff01 or 0xff1f;
    public static bool IsHan(Rune rune) => rune.Value is >= 0x3400 and <= 0x4dbf or >= 0x4e00 and <= 0x9fff or
        >= 0xf900 and <= 0xfaff or >= 0x20000 and <= 0x2ee5f or >= 0x2f800 and <= 0x2fa1f or >= 0x30000 and <= 0x323af;

    public static string PrepareText(string text, string? language) => language switch
    {
        "fr" or "de" or "el" => text.Normalize(NormalizationForm.FormC),
        "es" => Fold(text).Normalize(NormalizationForm.FormC),
        "ar" or "hi" or "zh" or "bn" or "id" or "ur" or "ru" => Fold(text).Normalize(NormalizationForm.FormC),
        _ => Fold(text),
    };

    public static string NormalizePreparedWord(string word, string? language, bool isPrefix = false)
    {
        if (!UsesListedSpelling(language)) return word;

        string lower = word.ToLowerInvariant();
        // A terminal capital sigma in an unfinished prefix can be followed by more letters.
        if (language == "el" && !isPrefix && word.Length > 1 && word[^1] == 'Σ')
            lower = string.Concat(lower.AsSpan(0, lower.Length - 1), "ς");

        return language switch
        {
            "fr" => lower.Replace('’', '\''),
            // Lowercasing capitals such as Ϊ with a tonos can enable NFC composition.
            "el" => lower.Normalize(NormalizationForm.FormC),
            _ => lower,
        };
    }

    public static string NormalizeWord(string word, string? language, bool isPrefix = false) =>
        NormalizePreparedWord(PrepareText(word, language), language, isPrefix);

    public static bool IsWordRune(Rune rune, bool includeMarks = false) =>
        Rune.IsLetter(rune) || Rune.IsNumber(rune) ||
        (includeMarks && Rune.GetUnicodeCategory(rune) is UnicodeCategory.NonSpacingMark or
            UnicodeCategory.SpacingCombiningMark or UnicodeCategory.EnclosingMark);

    // A null token resets n-gram context. Mandarin uses character n-grams for
    // Han text; this deliberately does not claim dictionary word segmentation.
    public static IEnumerable<string?> UnicodeTokens(string line, string language)
    {
        string text = PrepareText(line, language);
        bool han = language == "zh", joiners = language is "ar" or "hi" or "bn" or "ur";
        int index = 0;
        while (index < text.Length)
        {
            Rune rune = Rune.GetRuneAt(text, index);
            if (IsSentenceBoundary(rune)) { index += rune.Utf16SequenceLength; yield return null; continue; }
            if (han && IsHan(rune)) { index += rune.Utf16SequenceLength; yield return rune.ToString(); continue; }
            if (!IsWordRune(rune)) { index += rune.Utf16SequenceLength; continue; }
            int start = index;
            index += rune.Utf16SequenceLength;
            while (index < text.Length)
            {
                rune = Rune.GetRuneAt(text, index);
                if (han && IsHan(rune)) break;
                if (IsWordRune(rune, includeMarks: true)) { index += rune.Utf16SequenceLength; continue; }
                int nextIndex = index + rune.Utf16SequenceLength;
                bool apostrophe = rune.Value is '\'' or 0x2019;
                bool joiner = joiners && rune.Value is 0x200c or 0x200d;
                if ((apostrophe || joiner) && nextIndex < text.Length &&
                    IsWordRune(Rune.GetRuneAt(text, nextIndex), includeMarks: joiner))
                { index = nextIndex; continue; }
                break;
            }
            yield return text[start..index];
        }
    }

    public static string Fold(string text)
    {
        if (Ascii.IsValid(text)) return text.ToLowerInvariant();
        var folded = new StringBuilder(text.Length);
        var mappings = Foldings.Value;
        foreach (var rune in text.EnumerateRunes())
        {
            if (rune.IsAscii) folded.Append(char.ToLowerInvariant((char)rune.Value));
            else if (mappings.TryGetValue(rune.Value, out string? replacement)) folded.Append(replacement);
            else if (rune.IsBmp) folded.Append((char)rune.Value);
            else folded.Append(rune.ToString());
        }
        return folded.ToString();
    }

    private static Dictionary<int, string> LoadFoldings()
    {
        using var reader = new StreamReader(typeof(WordNormalization).Assembly.GetManifestResourceStream("NgramTrie.CaseFold.txt")
            ?? throw new InvalidOperationException("Word case-fold mappings are missing."));
        var mappings = new Dictionary<int, string>();
        while (reader.ReadLine() is { } line)
        {
            if (line.StartsWith('#')) continue;
            var parts = line.Split(' ');
            mappings.Add(ParseHex(parts[0]), string.Concat(parts.Skip(1).Select(value => char.ConvertFromUtf32(ParseHex(value)))));
        }
        return mappings;
    }

    private static int ParseHex(string value) => int.Parse(value, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
}
