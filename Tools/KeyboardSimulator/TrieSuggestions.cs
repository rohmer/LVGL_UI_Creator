using System.Buffers;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using NgramTrie;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal static class TrieSuggestions
{
    private const int SuggestionCount = 3;
    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
    private static readonly IComparer<Branch> BranchOrder = Comparer<Branch>.Create((left, right) =>
    {
        int byMass = left.Node.Mass.CompareTo(right.Node.Mass);
        return byMass != 0 ? byMass : right.Node.Label.CompareTo(left.Node.Label);
    });

    public static IReadOnlyList<string> Predict(TrieModel trie, string textBeforeWord, string prefix,
        CancellationToken cancellation, KeyboardNeighborMap? keyboard = null, string? language = null)
    {
        cancellation.ThrowIfCancellationRequested();
        bool includeMarks = language == "es" || WordNormalization.UsesListedSpelling(language) || WordNormalization.UsesUnicodeTokens(language);
        foreach (Rune rune in prefix.EnumerateRunes())
        {
            if (!WordNormalization.IsWordRune(rune, includeMarks) && rune.Value is not ('\'' or '’') && !SuggestionText.IsJoiner(rune, language)) return [];
        }

        var (previous, older) = ReadContext(textBeforeWord.AsSpan(), language);
        string normalizedPrefix = WordNormalization.NormalizeWord(prefix, language);
        byte[] prefixBytes = Utf8.GetBytes(normalizedPrefix);
        // A capital sigma at the caret can end this word or precede more letters.
        string? continuation = language == "el" && prefix.Length > 1 && prefix[^1] == 'Σ'
            ? WordNormalization.NormalizeWord(prefix, language, isPrefix: true) : null;
        byte[]? continuationBytes = continuation is not null ? Utf8.GetBytes(continuation) : null;
        var seen = new HashSet<string>(StringComparer.Ordinal);
        var result = new List<string>(SuggestionCount);
        bool upperSuffix = IsAllCapsPrefix(prefix);

        if (older.Length != 0 && previous.Length != 0)
            AddSuggestions($"{older} {previous} ", TrieRoot.Combined);
        if (previous.Length != 0)
            AddSuggestions($"{previous} ", TrieRoot.Combined);
        AddSuggestions("", TrieRoot.Unigrams);
        if (keyboard is not null && result.Count < SuggestionCount)
            result.AddRange(TrieCorrections.Find(trie, keyboard, normalizedPrefix, previous, older,
                SuggestionCount - result.Count, word => DisplayCorrection(word, prefix, upperSuffix, language), seen, cancellation));
        return result;

        void AddSuggestions(string context, TrieRoot root)
        {
            AddPrefix(context, root, normalizedPrefix, prefixBytes);
            if (continuation is not null && continuationBytes is not null)
                AddPrefix(context, root, continuation, continuationBytes);
        }

        void AddPrefix(string context, TrieRoot root, string term, byte[] bytes)
        {
            int remaining = SuggestionCount - result.Count;
            if (remaining == 0 || trie.FindPrefix(context + term, root) is not int index) return;
            foreach (var match in FindBestWords(trie, index, bytes, seen, remaining,
                         word => ApplyCase(word, prefix, term.Length, upperSuffix, previous.Length == 0, language), cancellation))
            {
                seen.Add(match.Text);
                result.Add(match.Text);
            }
        }
    }

    internal static int LastSentenceSeparator(ReadOnlySpan<char> text) => text.LastIndexOfAny(".!?\r\n\u2028\u2029\u061f\u06d4\u0964\u0965\u3002\uff01\uff1f");

    internal static (string Previous, string Older) ReadContext(ReadOnlySpan<char> textBeforeWord, string? language = null)
    {
        var sentence = textBeforeWord[(LastSentenceSeparator(textBeforeWord) + 1)..];
        bool includeMarks = language == "es" || WordNormalization.UsesListedSpelling(language) || WordNormalization.UsesUnicodeTokens(language);
        string previous = WordNormalization.NormalizeWord(TakeLastWord(ref sentence, includeMarks, language), language);
        return (previous, WordNormalization.NormalizeWord(TakeLastWord(ref sentence, includeMarks, language), language));
    }

    internal static string DisplayCorrection(string word, string typed, string? language = null) =>
        DisplayCorrection(word, typed, IsAllCapsPrefix(typed), language);

    private static string DisplayCorrection(string word, string typed, bool upperSuffix, string? language) =>
        upperSuffix ? Uppercase(word, language) :
        ApplyCase(word, "", 0, upperSuffix: false, typed.Length > 0 && Rune.IsUpper(Rune.GetRuneAt(typed, 0)), language);

    private static List<Match> FindBestWords(TrieModel trie, int start, byte[] prefix, HashSet<string> seen, int count,
        Func<string, string> display, CancellationToken cancellation)
    {
        var path = new List<byte>(prefix);
        var stack = new List<Branch> { new(trie.GetNode(start), prefix.Length, AppendLabel: false) };
        var best = new List<Match>(count);
        while (stack.Count != 0)
        {
            cancellation.ThrowIfCancellationRequested();
            var branch = stack[^1];
            stack.RemoveAt(stack.Count - 1);
            uint lowerBound = best.Count == count ? best[^1].Mass : 0;
            if (branch.Node.Mass < lowerBound) continue;

            // The stack performs depth-first traversal. Siblings share their parent's path.
            path.RemoveRange(branch.ParentLength, path.Count - branch.ParentLength);
            if (branch.AppendLabel) path.Add(branch.Node.Label);
            if (best.Count == count && Compare(branch.Node.Mass, CollectionsMarshal.AsSpan(path), best[^1]) >= 0)
                continue;

            // Terminal mass plus space-child mass is the whole-word mass. Other
            // children continue the current word and are bounded by their node mass.
            uint wordMass = branch.Node.Mass;
            int childrenStart = stack.Count;
            int childEnd = branch.Node.FirstChild + branch.Node.ChildCount;
            for (int index = branch.Node.FirstChild; index < childEnd; index++)
            {
                var child = trie.GetNode(index);
                if (child.Label == (byte)' ') continue;
                wordMass -= child.Mass;
                if (child.Mass >= lowerBound)
                    stack.Add(new(child, path.Count, AppendLabel: true));
            }
            if (stack.Count - childrenStart > 1)
                stack.Sort(childrenStart, stack.Count - childrenStart, BranchOrder);
            if (wordMass == 0 || path.Count == 0) continue;

            ReadOnlySpan<byte> bytes = CollectionsMarshal.AsSpan(path);
            int position = 0;
            while (position < best.Count && Compare(wordMass, bytes, best[position]) > 0) position++;
            if (position == count) continue;

            // The reader validates structural records, so isolate malformed UTF-8
            // words without treating bytes from different paths as the same word.
            string word;
            try { word = Utf8.GetString(bytes); }
            catch (DecoderFallbackException) { continue; }
            string text = display(word);
            if (seen.Contains(text)) continue;
            int duplicate = best.FindIndex(match => match.Text == text);
            if (duplicate >= 0)
            {
                if (position > duplicate) continue;
                best.RemoveAt(duplicate);
            }
            if (best.Count == count) best.RemoveAt(count - 1);
            best.Insert(position, new(text, bytes.ToArray(), wordMass));
        }
        return best;
    }

    private static int Compare(uint mass, ReadOnlySpan<byte> path, Match other)
    {
        int byMass = other.Mass.CompareTo(mass);
        return byMass != 0 ? byMass : path.SequenceCompareTo(other.Path);
    }

    private static string TakeLastWord(ref ReadOnlySpan<char> text, bool includeMarks, string? language)
    {
        int end = text.Length;
        while (end > 0 && !ReadWordRune(text[..end], includeMarks, out int width)) end -= Math.Max(width, 1);

        if (language == "zh" && Rune.DecodeLastFromUtf16(text[..end], out var last, out int hanWidth) == OperationStatus.Done && WordNormalization.IsHan(last)) {
            string character = text[(end - hanWidth)..end].ToString();
            text = text[..(end - hanWidth)];
            return character;
        }

        int start = end;
        while (start > 0)
        {
            if (language == "zh" && Rune.DecodeLastFromUtf16(text[..start], out var rune, out _) == OperationStatus.Done && WordNormalization.IsHan(rune)) break;
            if (ReadWordRune(text[..start], includeMarks, out int width))
                start -= width;
            else if ((text[start - 1] is '\'' or '’' ||
                language is "ar" or "hi" or "bn" or "ur" && text[start - 1] is '\u200c' or '\u200d') &&
                ReadWordRune(text[..(start - 1)], includeMarks, out width))
                start -= width + 1;
            else
                break;
        }

        string word = text[start..end].ToString();
        text = text[..start];
        return word;
    }

    private static bool ReadWordRune(ReadOnlySpan<char> text, bool includeMarks, out int width) =>
        Rune.DecodeLastFromUtf16(text, out var rune, out width) == OperationStatus.Done && WordNormalization.IsWordRune(rune, includeMarks);

    private static string ApplyCase(string word, string prefix, int normalizedPrefixLength, bool upperSuffix, bool sentenceStart, string? language)
    {
        if (prefix.Length != 0)
        {
            string suffix = word[normalizedPrefixLength..];
            return prefix + (upperSuffix ? Uppercase(suffix, language) : suffix);
        }
        if (!sentenceStart) return word;
        var initial = Rune.GetRuneAt(word, 0);
        return Uppercase(initial.ToString(), language) + word[initial.Utf16SequenceLength..];
    }

    private static string Uppercase(string word, string? language) => language switch
    {
        "de" => word.ToUpperInvariant().Replace('ß', 'ẞ'),
        "el" => word.ToUpperInvariant().Replace("ΐ", "Ϊ́").Replace("ΰ", "Ϋ́"),
        _ => word.ToUpperInvariant(),
    };

    private static bool IsAllCapsPrefix(string prefix)
    {
        int capitalLetters = 0;
        foreach (Rune rune in prefix.EnumerateRunes())
        {
            switch (Rune.GetUnicodeCategory(rune))
            {
                case UnicodeCategory.LowercaseLetter:
                    return false;
                case UnicodeCategory.UppercaseLetter or UnicodeCategory.TitlecaseLetter:
                    capitalLetters++;
                    break;
            }
        }
        return capitalLetters > 1;
    }

    internal static string Fold(string value) => WordNormalization.Fold(value);

    private readonly record struct Branch(TrieNode Node, int ParentLength, bool AppendLabel);
    private readonly record struct Match(string Text, byte[] Path, uint Mass);
}
