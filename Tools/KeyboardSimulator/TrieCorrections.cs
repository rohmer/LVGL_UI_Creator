using System.Buffers;
using System.Text;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal static class TrieCorrections
{
    // Score edits relative to the corresponding number of correct keystrokes.
    private static readonly double CorrectCost = -Math.Log(KeyboardNeighborMap.CorrectKeyProbability);
    private static readonly double Deletion = -Math.Log(KeyboardNeighborMap.DeletionProbability) - CorrectCost;
    private static readonly double Insertion = -Math.Log(KeyboardNeighborMap.InsertionProbability /
        (2 * KeyboardNeighborMap.QwertyAlphabetSize)) - CorrectCost;
    // The thesis models individual keystrokes. Give an adjacent swap its own penalty.
    private static readonly double Swap = -Math.Log(.02) - 2 * CorrectCost;
    private static readonly UTF8Encoding Utf8 = new(false, true);

    public static IReadOnlyList<string> Find(TrieModel trie, KeyboardNeighborMap keyboard, string typed, string previous,
        string older, int limit, Func<string, string> display, HashSet<string> seen, CancellationToken cancellation)
    {
        if (limit == 0 || typed.Length > 128 || trie.GetNode((int)TrieRoot.Unigrams).Mass == 0) return [];
        var observed = typed.EnumerateRunes().ToArray();
        if (observed.Length is < 2 or > 64 || (trie.FindPrefix(typed, TrieRoot.Unigrams) is int exact &&
            trie.GetTerminalCount(exact) != 0)) return [];

        var context = new ContextScore(trie, previous, older);
        var best = new List<Match>(limit);
        Search(trie, keyboard, observed, AddCandidate, cancellation);
        return best.Select(match => match.Text).ToArray();

        void AddCandidate(ReadOnlySpan<byte> word, uint count, double distance)
        {
            string text = display(Utf8.GetString(word));
            if (seen.Contains(text)) return;
            double score = context.Score(word, count, distance);

            int place = 0;
            while (place < best.Count && (best[place].Score > score ||
                best[place].Score == score && StringComparer.Ordinal.Compare(best[place].Text, text) < 0)) place++;
            if (place == limit) return;
            int duplicate = best.FindIndex(match => match.Text == text);
            if (duplicate >= 0)
            {
                if (duplicate < place) return;
                best.RemoveAt(duplicate);
            }
            if (best.Count == limit) best.RemoveAt(limit - 1);
            best.Insert(place, new(text, score));
        }
    }

    public static (string Word, double Probability)? BestAutocorrection(TrieModel trie, KeyboardNeighborMap keyboard,
        string typed, string previous, string older, CancellationToken cancellation)
    {
        cancellation.ThrowIfCancellationRequested();
        if (typed.Length is 0 or > 128 || NumericText.IsProtectedWord(typed) ||
            trie.GetNode((int)TrieRoot.Unigrams).Mass == 0) return null;
        var observed = typed.EnumerateRunes().ToArray();
        if (observed.Length > 64) return null;

        byte[] typedBytes = Utf8.GetBytes(typed);
        var context = new ContextScore(trie, previous, older);
        uint typedCount = trie.FindPrefix(typedBytes, TrieRoot.Unigrams) is int exact ? trie.GetTerminalCount(exact) : 0;
        double bestScore = typedCount == 0 ? 0 : context.Score(typedBytes, typedCount, distance: 0);
        double totalWeight = 1;
        string? bestWord = null;
        Search(trie, keyboard, observed, AddCandidate, cancellation);
        return bestWord is null ? null : (bestWord, 1 / totalWeight);

        void AddCandidate(ReadOnlySpan<byte> word, uint count, double distance)
        {
            if (word.SequenceEqual(typedBytes)) return;
            double score = context.Score(word, count, distance);
            if (score > bestScore)
            {
                // Keep all weights scaled by the best log score observed so far.
                totalWeight = totalWeight * Math.Exp(bestScore - score) + 1;
                bestScore = score;
                bestWord = Utf8.GetString(word);
                return;
            }
            totalWeight += Math.Exp(score - bestScore);
            if (score == bestScore && bestWord is not null)
            {
                string text = Utf8.GetString(word);
                if (StringComparer.Ordinal.Compare(text, bestWord) < 0) bestWord = text;
            }
        }
    }

    private delegate void Candidate(ReadOnlySpan<byte> word, uint count, double distance);

    private static void Search(TrieModel trie, KeyboardNeighborMap keyboard, Rune[] observed,
        Candidate candidate, CancellationToken cancellation)
    {
        double budget = observed.Length switch { 1 => 7, 2 => 8.5, <= 4 => 13.5, _ => 17 };
        int maxDeletions = (int)(budget / Deletion);
        int maxInsertions = (int)(budget / Insertion);
        int longest = observed.Length + maxDeletions;
        // A prefix-length gap beyond the budget cannot be paid for by edits. Keep only
        // the possible diagonals, with an unreachable cell on either side of each row.
        int rowWidth = maxDeletions + maxInsertions + 3;
        int diagonal = maxDeletions + 1;
        byte[] path = new byte[longest * 4];
        double[] rows = ArrayPool<double>.Shared.Rent((longest + 1) * rowWidth);
        int visited = 0;
        try
        {
            for (int depth = 0; depth <= longest; depth++)
            {
                rows[depth * rowWidth] = double.PositiveInfinity;
                rows[(depth + 1) * rowWidth - 1] = double.PositiveInfinity;
            }
            for (int index = 0; index <= Math.Min(observed.Length, maxInsertions); index++)
                rows[diagonal + index] = index * Insertion;
            Walk(trie.GetNode((int)TrieRoot.Unigrams), 0, 0, 0, default, 0);
        }
        finally { ArrayPool<double>.Shared.Return(rows); }

        void Walk(TrieNode node, int depth, int runeStart, int length, Rune priorRune, double priorMinimum)
        {
            if ((visited++ & 255) == 0) cancellation.ThrowIfCancellationRequested();
            int end = node.FirstChild + node.ChildCount;
            for (int childIndex = node.FirstChild; childIndex < end; childIndex++)
            {
                var child = trie.GetNode(childIndex);
                byte label = child.Label;
                if (label == (byte)' ') continue;
                path[length] = label;
                if (length == runeStart && label < 0x80)
                {
                    ScoreRune(childIndex, child, depth, length + 1, new Rune(label), priorRune, priorMinimum);
                }
                else
                {
                    var decoded = Rune.DecodeFromUtf8(path.AsSpan(runeStart, length + 1 - runeStart), out Rune rune, out _);
                    if (decoded == OperationStatus.NeedMoreData)
                        Walk(child, depth, runeStart, length + 1, priorRune, priorMinimum);
                    else if (decoded == OperationStatus.Done)
                        ScoreRune(childIndex, child, depth, length + 1, rune, priorRune, priorMinimum);
                }
            }
        }

        void ScoreRune(int nodeIndex, TrieNode node, int depth, int byteLength, Rune rune, Rune priorRune, double priorMinimum)
        {
            int newDepth = depth + 1;
            int current = newDepth * rowWidth;
            int first = Math.Max(0, newDepth - maxDeletions);
            int last = Math.Min(observed.Length, newDepth + maxInsertions);
            int cell = current + diagonal + first - newDepth;
            double minimum = double.PositiveInfinity;
            if (first == 0)
            {
                minimum = rows[cell - rowWidth + 1] + Deletion;
                rows[cell++] = minimum;
                first = 1;
            }

            // From the same diagonal in the parent row we can substitute, from the
            // next diagonal we can delete, and from the previous cell we can insert.
            for (int index = first; index <= last; index++, cell++)
            {
                int prior = cell - rowWidth;
                var observedRune = observed[index - 1];
                double substitution = keyboard.RelativeSubstitutionCost(rune, observedRune);
                double cost = Math.Min(rows[prior] + substitution,
                    Math.Min(rows[prior + 1] + Deletion, rows[cell - 1] + Insertion));
                if (depth > 0 && index > 1 && priorRune == observedRune && rune == observed[index - 2])
                    cost = Math.Min(cost, rows[prior - rowWidth] + Swap);
                rows[cell] = cost;
                minimum = Math.Min(minimum, cost);
            }

            if (last == observed.Length && rows[cell - 1] <= budget)
            {
                uint count = node.ChildCount == 0 ? node.Mass : trie.GetTerminalCount(nodeIndex);
                if (count > 0) candidate(path.AsSpan(0, byteLength), count, rows[cell - 1]);
            }

            // The next row can skip this row via a transposition, so keep both bounds.
            if (node.ChildCount > 0 && newDepth < longest && (minimum <= budget || priorMinimum + Swap <= budget))
                Walk(node, newDepth, byteLength, byteLength, rune, minimum);
        }
    }

    private readonly struct ContextScore
    {
        private readonly TrieModel trie;
        private readonly int? bigram;
        private readonly int? trigram;

        public ContextScore(TrieModel trie, string previous, string older)
        {
            this.trie = trie;
            bigram = previous.Length == 0 ? null : trie.FindPrefix($"{previous} ");
            trigram = older.Length == 0 ? null : trie.FindPrefix($"{older} {previous} ");
        }

        public double Score(ReadOnlySpan<byte> word, uint count, double distance)
        {
            double score = .55 * Math.Log(count) - distance;
            if (CountAfter(trigram, word) is uint threeWordCount and > 0)
                score += .9 * Math.Log(1.0 + threeWordCount);
            else if (CountAfter(bigram, word) is uint twoWordCount and > 0)
                score += .55 * Math.Log(1.0 + twoWordCount);
            return score;
        }

        private uint CountAfter(int? prefix, ReadOnlySpan<byte> word) =>
            prefix is int start && trie.FollowFrom(start, word) is int end ? trie.GetTerminalCount(end) : 0;
    }

    private readonly record struct Match(string Text, double Score);
}
