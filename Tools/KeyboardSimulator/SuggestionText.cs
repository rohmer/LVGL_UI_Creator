using System.Buffers;
using System.Text;
using NgramTrie;

namespace KeyboardSimulator;

internal sealed record SuggestionText(string Text, int Caret, int SelectionLength, int Start, int End)
{
    public string Context => Text[..Start];
    public string Prefix => Text[Start..Caret];

    public static SuggestionText Capture(string text, int caret, int selectionLength, string? language = null)
    {
        if (selectionLength != 0) return new(text, caret, selectionLength, caret, caret + selectionLength);

        bool includeMarks = language == "es" || WordNormalization.UsesListedSpelling(language) || WordNormalization.UsesUnicodeTokens(language);
        bool han = language == "zh";
        int start = caret, end = caret;
        while (Previous(start, out var rune, out int width))
        {
            if (han && WordNormalization.IsHan(rune)) break;
            if (!WordNormalization.IsWordRune(rune, includeMarks) && !((IsApostrophe(rune) || IsJoiner(rune, language)) &&
                Previous(start - width, out var before, out _) && WordNormalization.IsWordRune(before, includeMarks))) break;
            start -= width;
        }
        // A combining mark belongs to a word only when a letter or number precedes it.
        while (includeMarks && start < caret && Next(start, out var first, out int firstWidth) &&
            !WordNormalization.IsWordRune(first)) start += firstWidth;
        while (Next(end, out var following, out int followingWidth))
        {
            if (han && WordNormalization.IsHan(following)) break;
            if (!WordNormalization.IsWordRune(following, includeMarks && end > start) &&
                !((IsApostrophe(following) || IsJoiner(following, language)) && end > start && Next(end + followingWidth, out var after, out _) &&
                WordNormalization.IsWordRune(after, includeMarks))) break;
            end += followingWidth;
        }
        return new(text, caret, 0, start, end);

        bool Previous(int index, out Rune rune, out int width) =>
            Rune.DecodeLastFromUtf16(text.AsSpan(0, index), out rune, out width) == OperationStatus.Done;

        bool Next(int index, out Rune rune, out int width) =>
            Rune.DecodeFromUtf16(text.AsSpan(index), out rune, out width) == OperationStatus.Done;
    }

    private static bool IsApostrophe(Rune rune) => rune.Value is '\'' or '’';
    internal static bool IsJoiner(Rune rune, string? language) =>
        language is "ar" or "hi" or "bn" or "ur" && rune.Value is 0x200c or 0x200d;
}
