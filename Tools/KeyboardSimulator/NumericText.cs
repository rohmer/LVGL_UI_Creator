using System.Buffers;
using System.Collections.Frozen;
using System.Text;

namespace KeyboardSimulator;

internal static class NumericText
{
    private static readonly FrozenSet<string> NumberAndCalendarWords = """
        zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen
        twenty thirty forty fifty sixty seventy eighty ninety hundred thousand million billion trillion quadrillion quintillion
        hundreds thousands millions billions trillions quadrillions quintillions
        first second third fourth fifth sixth seventh eighth ninth tenth eleventh twelfth thirteenth fourteenth fifteenth sixteenth
        seventeenth eighteenth nineteenth twentieth thirtieth fortieth fiftieth sixtieth seventieth eightieth ninetieth
        zeroth hundredth thousandth millionth billionth trillionth quadrillionth quintillionth
        monday tuesday wednesday thursday friday saturday sunday january february april june july september october november december
        """.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).ToFrozenSet(StringComparer.OrdinalIgnoreCase);

    public static bool IsProtectedWord(string word)
    {
        if (word.EnumerateRunes().Any(Rune.IsNumber)) return true;
        if (word.Length > 2 && (word[^2] is '\'' or '’') && (word[^1] is 's' or 'S')) word = word[..^2];
        return NumberAndCalendarWords.Contains(word);
    }

    public static bool TouchesNumber(ReadOnlySpan<char> before, ReadOnlySpan<char> after)
    {
        // A regex word match can stop at a supplementary Unicode digit in the same token.
        if (!before.IsEmpty && before[^1] is '\'' or '’') before = before[..^1];
        if (!after.IsEmpty && after[0] is '\'' or '’') after = after[1..];
        return (Rune.DecodeLastFromUtf16(before, out var prior, out _) == OperationStatus.Done && Rune.IsNumber(prior)) ||
               (Rune.DecodeFromUtf16(after, out var next, out _) == OperationStatus.Done && Rune.IsNumber(next));
    }
}
