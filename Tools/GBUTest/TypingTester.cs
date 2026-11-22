using System.Buffers;
using System.Globalization;
using System.Text;
using KeyboardSimulator;
using NgramTrie.Reader;

namespace GBUTest;

public enum TypingOutcome { Good, Bad, Ugly, KeptCorrect, Missed }

public sealed record TypingWordResult(int PairNumber, int WordNumber, string Expected, string Typed, string Result,
    double? Probability, TypingOutcome Outcome, int Points)
{
    public string? Proposed { get; init; }
}

public sealed record TypingTestProgress(int CompletedPairs, int TotalPairs);

public sealed record TypingTestReport(double Threshold, int Pairs, long Score, int Good, int Bad, int Ugly,
    int KeptCorrect, int Missed, IReadOnlyList<TypingWordResult> Words)
{
    public int TotalWords => Words.Count;
    public double? CompositeScore => TotalWords == 0 ? null : (double)Score / TotalWords;
}

public sealed record TypingRegressionResult(double Threshold, long Score, int TotalWords, int Good, int Bad, int Ugly,
    int KeptCorrect, int Missed)
{
    public double? CompositeScore => TotalWords == 0 ? null : (double)Score / TotalWords;
}

public sealed record TypingRegressionReport(int Pairs, int TotalWords,
    IReadOnlyList<TypingRegressionResult> Results, IReadOnlyList<TypingRegressionResult> BestResults);

public static class TypingTester
{
    public const double DefaultThreshold = 0.9;
    public const int UglyPoints = -100;
    public const decimal RegressionStart = 0.5m;
    public const decimal DefaultRegressionStep = 0.05m;
    public const decimal MinimumRegressionStep = 0.001m;

    public static TypingTestReport Run(string pairsPath, string triePath, double threshold = DefaultThreshold,
        Action<TypingTestProgress>? progress = null, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        if (!double.IsFinite(threshold) || threshold is < 0 or > 1)
            throw new ArgumentOutOfRangeException(nameof(threshold), "Enter an autocorrect threshold from 0 to 1.");

        var run = new ThresholdScore(threshold);
        var words = new List<TypingWordResult>();
        int pairs = Evaluate(pairsPath, triePath, [run], words, progress, cancellation);
        var result = run.ToResult();
        return new(threshold, pairs, result.Score, result.Good, result.Bad, result.Ugly, result.KeptCorrect,
            result.Missed, words.AsReadOnly());
    }

    public static TypingRegressionReport RunRegression(string pairsPath, string triePath, decimal step = DefaultRegressionStep,
        Action<TypingTestProgress>? progress = null, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        if (step is < MinimumRegressionStep or > 1)
            throw new ArgumentOutOfRangeException(nameof(step), "Enter a regression step from 0.001 to 1.");

        var runs = new List<ThresholdScore>();
        for (decimal threshold = RegressionStart; threshold < 1; threshold += step)
            runs.Add(new((double)threshold));
        runs.Add(new(1));

        int pairs = Evaluate(pairsPath, triePath, runs, wordResults: null, progress, cancellation);
        var results = runs.Select(run => run.ToResult()).ToArray();
        long bestScore = results.Max(result => result.Score);
        var best = results.Where(result => result.Score == bestScore).ToArray();
        return new(pairs, results[0].TotalWords, Array.AsReadOnly(results), Array.AsReadOnly(best));
    }

    public static string SaveRegressionCsv(TypingRegressionReport report, string pairsPath)
    {
        ArgumentNullException.ThrowIfNull(report);
        string input = Path.GetFullPath(pairsPath);
        string directory = Path.GetDirectoryName(input)!;
        string timestamp = DateTime.Now.ToString("yyyyMMdd-HHmmssfff", CultureInfo.InvariantCulture);
        string fileName = $"{Path.GetFileNameWithoutExtension(input)}.regression-{timestamp}-{Guid.NewGuid():N}.csv";
        string destination = Path.Combine(directory, fileName);
        string temporary = Path.Combine(directory, $".gbutest-{Guid.NewGuid():N}.tmp");
        var bestThresholds = report.BestResults.Select(result => result.Threshold).ToHashSet();

        try
        {
            using (var file = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (var writer = new StreamWriter(file))
            {
                writer.WriteLine("Threshold,TotalScore,TotalWords,CompositeScore,Best,Good,Bad,Ugly,KeptCorrect,Missed");
                foreach (var row in report.Results)
                {
                    string best = bestThresholds.Contains(row.Threshold) ? "true" : "false";
                    writer.WriteLine(FormattableString.Invariant(
                        $"{row.Threshold:R},{row.Score},{row.TotalWords},{row.CompositeScore:R},{best},{row.Good},{row.Bad},{row.Ugly},{row.KeptCorrect},{row.Missed}"));
                }
            }
            File.Move(temporary, destination);
        }
        finally { File.Delete(temporary); }
        return destination;
    }

    private static int Evaluate(string pairsPath, string triePath, IReadOnlyList<ThresholdScore> runs,
        List<TypingWordResult>? wordResults, Action<TypingTestProgress>? progress, CancellationToken cancellation)
    {
        var trie = TrieModel.Load(triePath);
        var pairs = SentenceFuzzer.ReadPairs(pairsPath, cancellation);
        var keyboard = TestData.LoadNeighbors();
        progress?.Invoke(new(0, pairs.Length));
        for (int pairIndex = 0; pairIndex < pairs.Length; pairIndex++)
        {
            cancellation.ThrowIfCancellationRequested();
            int pairNumber = pairIndex + 1;
            var words = ReadWords(pairs[pairIndex], pairNumber, cancellation);

            // Each threshold commits its own corrections. Share searches only for identical typed words and contexts.
            // Limit the cache to one pair so regression memory does not grow with the corpus.
            Dictionary<CorrectionContext, (string Word, double Probability)?>? candidates = runs.Count > 1 ? new() : null;
            foreach (var run in runs)
                ScorePair(run, words, pairNumber, candidates);

            progress?.Invoke(new(pairNumber, pairs.Length));
        }
        return pairs.Length;

        void ScorePair(ThresholdScore run, InputWord[] words, int pairNumber,
            Dictionary<CorrectionContext, (string Word, double Probability)?>? candidates)
        {
            string previous = "";
            string older = "";
            for (int wordIndex = 0; wordIndex < words.Length; wordIndex++)
            {
                cancellation.ThrowIfCancellationRequested();
                var word = words[wordIndex];
                if (word.StartsSentence) previous = older = "";
                var context = new CorrectionContext(word.FoldedTyped, previous, older);
                if (candidates is null || !candidates.TryGetValue(context, out var candidate))
                {
                    candidate = TrieCorrections.BestAutocorrection(trie, keyboard, word.FoldedTyped, previous, older, cancellation);
                    candidates?.Add(context, candidate);
                }
                string? correction = candidate is { } choice && choice.Probability >= run.Threshold ? choice.Word : null;
                string committed = correction ?? word.FoldedTyped;
                TypingOutcome outcome = Classify(word.FoldedExpected, word.FoldedTyped, committed, correction is not null);
                int points = outcome switch
                {
                    TypingOutcome.Good => 10,
                    TypingOutcome.Bad => -25,
                    TypingOutcome.Ugly => UglyPoints,
                    TypingOutcome.KeptCorrect => 1,
                    _ => 0,
                };
                run.Add(outcome, points);
                if (wordResults is not null)
                {
                    string? proposed = candidate is { } suggestion ? TrieSuggestions.DisplayCorrection(suggestion.Word, word.Typed) : null;
                    string result = correction is not null && proposed is not null ? proposed : word.Typed;
                    wordResults.Add(new(pairNumber, wordIndex + 1, word.Expected, word.Typed, result, candidate?.Probability,
                        outcome, points) { Proposed = proposed });
                }

                older = previous;
                previous = committed;
            }
        }
    }

    private static InputWord[] ReadWords(SentencePair pair, int pairNumber, CancellationToken cancellation)
    {
        var expectedWords = WordSpans(pair.Original);
        var typedWords = WordSpans(pair.Fuzzed);
        if (expectedWords.Count != typedWords.Count)
            throw new InvalidDataException($"Pair {pairNumber:N0} has {expectedWords.Count:N0} original words and {typedWords.Count:N0} fuzzed words. The two lines must have matching word positions.");

        var words = new InputWord[typedWords.Count];
        int lastWordEnd = 0;
        for (int wordIndex = 0; wordIndex < words.Length; wordIndex++)
        {
            cancellation.ThrowIfCancellationRequested();
            var (typedStart, typedLength) = typedWords[wordIndex];
            var (expectedStart, expectedLength) = expectedWords[wordIndex];
            bool startsSentence = TrieSuggestions.LastSentenceSeparator(pair.Fuzzed.AsSpan(lastWordEnd, typedStart - lastWordEnd)) >= 0;
            lastWordEnd = typedStart + typedLength;
            string expected = pair.Original.Substring(expectedStart, expectedLength);
            string typed = pair.Fuzzed.Substring(typedStart, typedLength);
            words[wordIndex] = new(expected, typed, TrieSuggestions.Fold(expected), TrieSuggestions.Fold(typed), startsSentence);
        }
        return words;
    }

    private static List<(int Start, int Length)> WordSpans(ReadOnlySpan<char> text)
    {
        var words = new List<(int Start, int Length)>();
        for (int index = 0; index < text.Length;)
        {
            if (!ReadWordCharacter(text[index..], out int width))
            {
                index += Math.Max(1, width);
                continue;
            }

            int start = index;
            index += width;
            while (index < text.Length)
            {
                int apostropheWidth = text[index] is '\'' or '’' ? 1 : 0;
                if (!ReadWordCharacter(text[(index + apostropheWidth)..], out width)) break;
                index += apostropheWidth + width;
            }
            words.Add((start, index - start));
        }
        return words;

        static bool ReadWordCharacter(ReadOnlySpan<char> tail, out int width) =>
            Rune.DecodeFromUtf16(tail, out var rune, out width) == OperationStatus.Done &&
            (Rune.IsLetter(rune) || Rune.IsNumber(rune) || Rune.GetUnicodeCategory(rune) is
                UnicodeCategory.NonSpacingMark or UnicodeCategory.SpacingCombiningMark or UnicodeCategory.EnclosingMark);
    }

    private static TypingOutcome Classify(string expected, string typed, string committed, bool applied)
    {
        if (typed == expected) return applied ? TypingOutcome.Ugly : TypingOutcome.KeptCorrect;
        if (!applied) return TypingOutcome.Missed;
        return committed == expected ? TypingOutcome.Good : TypingOutcome.Bad;
    }

    private readonly record struct InputWord(string Expected, string Typed, string FoldedExpected, string FoldedTyped, bool StartsSentence);
    private readonly record struct CorrectionContext(string Typed, string Previous, string Older);

    private sealed class ThresholdScore(double threshold)
    {
        private readonly int[] counts = new int[Enum.GetValues<TypingOutcome>().Length];
        private long score;
        public double Threshold { get; } = threshold;

        public void Add(TypingOutcome outcome, int points)
        {
            counts[(int)outcome]++;
            score += points;
        }

        public TypingRegressionResult ToResult() => new(Threshold, score, counts.Sum(), counts[(int)TypingOutcome.Good],
            counts[(int)TypingOutcome.Bad], counts[(int)TypingOutcome.Ugly], counts[(int)TypingOutcome.KeptCorrect],
            counts[(int)TypingOutcome.Missed]);
    }
}
