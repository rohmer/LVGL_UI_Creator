using System.Text;
using System.Text.RegularExpressions;

namespace CorpusCrawler;

// A transparent boundary-based fallback, not an OpenNLP or POS grammar model.
// Unfinished trailing fragments are discarded; punctuation and script marks
// are preserved in UTF-8 corpus output.
internal static class UnicodeSentenceFallback
{
    private static readonly Regex Whitespace = new(@"\s+", RegexOptions.CultureInvariant);
    public static bool Supports(string code) => code is "ar" or "hi" or "zh" or "bn" or "id" or "ur" or "ru";
    private static bool Ending(char value) => value is '.' or '!' or '?' or '؟' or '۔' or '।' or '॥' or '。' or '！' or '？';
    private static bool Closing(char value) => value is '\"' or '\'' or '’' or '”' or '»' or ')' or '}' or ']' or '」' or '』' or '）' or '】';
    public static bool HasEnding(string text)
    {
        int end = text.Length - 1;
        while (end >= 0 && (Closing(text[end]) || char.IsWhiteSpace(text[end]))) end--;
        return end >= 0 && Ending(text[end]) && (text[end] != '.' || end == 0 || text[end - 1] != '.');
    }
    public static string[] Extract(IEnumerable<string> blocks, CancellationToken cancellation)
    {
        var sentences = new List<string>();
        foreach (string block in blocks)
        {
            cancellation.ThrowIfCancellationRequested();
            string text = Whitespace.Replace(block.Normalize(NormalizationForm.FormC), " ").Trim();
            int start = 0;
            for (int index = 0; index < text.Length; index++)
            {
                if ((index & 1023) == 0) cancellation.ThrowIfCancellationRequested();
                if (!Ending(text[index])) continue;
                if (text[index] == '.' && ((index > 0 && text[index - 1] == '.') ||
                    (index + 1 < text.Length && (text[index + 1] == '.' ||
                     (index > 0 && char.IsDigit(text[index - 1]) && char.IsDigit(text[index + 1])))))) continue;
                int end = index + 1;
                while (end < text.Length && (Ending(text[end]) || Closing(text[end]))) end++;
                string sentence = text[start..end].Trim();
                if (HasEnding(sentence) && sentence.EnumerateRunes().Any(rune => Rune.IsLetter(rune) ||
                    rune.Value is >= 0x3400 and <= 0x9fff or >= 0x20000 and <= 0x323af)) sentences.Add(sentence);
                start = end; index = end - 1;
            }
        }
        return [.. sentences];
    }
}
