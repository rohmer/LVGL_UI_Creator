using System.Diagnostics;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using KeyboardSimulator;
using NgramTrie.Reader;

namespace GBUTest;

internal static class Program
{
    private const string Usage = """
        Usage:
          GBUTest <model.ngtr> [--json]
          GBUTest --pairs FILE --trie FILE [--threshold 0.9] [--json]
        """;

    private static int Main(string[] args)
    {
        if (args is ["--help" or "-h"])
        {
            Console.WriteLine(Usage);
            return 0;
        }

        if (args.Any(arg => arg is "--pairs" or "--trie" or "--threshold"))
            return RunSentenceMode(args);

        if (args.Length is < 1 or > 2 || args.Length == 2 && args[1] != "--json")
        {
            Console.Error.WriteLine(Usage);
            return 2;
        }

        try
        {
            var modelPath = Path.GetFullPath(args[0]);
            using var stream = File.OpenRead(modelPath);
            string sha256 = Convert.ToHexString(SHA256.HashData(stream));
            stream.Position = 0;
            var trie = TrieModel.Load(stream);

            var cases = TestData.LoadSpellings();
            var neighbors = TestData.LoadNeighbors();
            var adjacentCases = AdjacentCases(trie, cases, neighbors);
            var report = new Report(modelPath, sha256, stream.Length, trie.NodeCount,
                Score(trie, neighbors, "GBUTest", cases),
                Score(trie, neighbors, "Derived adjacent-key substitutions", adjacentCases));

            if (args.Length == 2)
                Console.WriteLine(JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
            else
                Print(report);
            return 0;
        }
        catch (Exception error) when (error is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException or JsonException)
        {
            Console.Error.WriteLine($"GBUTest: {error.Message}");
            return 1;
        }
    }

    private static int RunSentenceMode(string[] args)
    {
        var supplied = new HashSet<string>(StringComparer.Ordinal);
        string? pairs = null;
        string? trie = null;
        double threshold = TypingTester.DefaultThreshold;
        for (int index = 0; index < args.Length; index++)
        {
            string option = args[index];
            if (option is not ("--pairs" or "--trie" or "--threshold" or "--json"))
                return UsageError($"Unknown sentence-test option '{option}'.");
            if (!supplied.Add(option)) return UsageError($"Specify {option} only once.");
            if (option == "--json") continue;
            if (++index == args.Length || args[index].StartsWith("--", StringComparison.Ordinal) || string.IsNullOrWhiteSpace(args[index]))
                return UsageError($"{option} requires a value.");

            string value = args[index];
            if (option == "--pairs") pairs = value;
            else if (option == "--trie") trie = value;
            else if (!double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out threshold) ||
                !double.IsFinite(threshold) || threshold < 0 || threshold > 1)
                return UsageError("--threshold must be a number from 0 to 1, using a decimal point, such as 0.9.");
        }

        if (pairs is null || trie is null) return UsageError("Sentence testing requires both --pairs and --trie.");

        try
        {
            var report = TypingTester.Run(pairs, trie, threshold);
            if (supplied.Contains("--json"))
            {
                var options = new JsonSerializerOptions { WriteIndented = true };
                options.Converters.Add(new JsonStringEnumConverter());
                Console.WriteLine(JsonSerializer.Serialize(report, options));
            }
            else
            {
                Console.WriteLine($"Sentence pairs                {report.Pairs:N0}");
                Console.WriteLine($"Total words                   {report.TotalWords:N0}");
                Console.WriteLine($"Confidence threshold          {report.Threshold:P1}");
                Console.WriteLine($"Good (+10)                    {report.Good:N0}");
                Console.WriteLine($"Bad (−25)                     {report.Bad:N0}");
                string uglyLabel = $"Ugly ({TypingTester.UglyPoints:N0})";
                Console.WriteLine($"{uglyLabel,-30}{report.Ugly:N0}");
                Console.WriteLine($"Correct and kept (+1)         {report.KeptCorrect:N0}");
                Console.WriteLine($"Missed                        {report.Missed:N0}");
                Console.WriteLine($"Score                         {report.Score:N0}");
                Console.WriteLine($"Composite score               {report.CompositeScore?.ToString("N4") ?? "n/a"}");
            }
            return 0;
        }
        catch (Exception error) when (error is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException or JsonException)
        {
            Console.Error.WriteLine($"GBUTest: {error.Message}");
            return 1;
        }

        static int UsageError(string message)
        {
            Console.Error.WriteLine($"GBUTest: {message}");
            Console.Error.WriteLine(Usage);
            return 2;
        }
    }

    private static SpellingCase[] AdjacentCases(TrieModel trie, SpellingCase[] original, KeyboardNeighborMap neighbors)
    {
        var random = new Random(3157);
        var used = original.Select(item => item.Input).ToHashSet(StringComparer.Ordinal);
        var targets = original.Select(item => item.Expected).ToHashSet(StringComparer.Ordinal);
        var cases = new List<SpellingCase>(targets.Count);

        foreach (string expected in targets.Order(StringComparer.Ordinal))
        {
            // The first usable position in a shuffled sequence samples the usable positions uniformly.
            int[] positions = Enumerable.Range(0, expected.Length).ToArray();
            random.Shuffle(positions.AsSpan());
            string? typo = null;
            foreach (int position in positions)
            {
                var candidates = neighbors.GetNeighbors(new Rune(expected[position]))
                    .Select(neighbor => expected[..position] + neighbor.ToString() + expected[(position + 1)..])
                    .Where(candidate => TestData.IsWord(candidate) && !targets.Contains(candidate) && !used.Contains(candidate) &&
                        !TestData.HasUnigram(trie, candidate)).ToArray();
                if (candidates.Length == 0) continue;
                typo = candidates[random.Next(candidates.Length)];
                break;
            }
            if (typo is null) continue;
            used.Add(typo);
            cases.Add(new(typo, expected, "adjacent_substitution"));
        }
        return [.. cases];
    }

    private static ScoreReport Score(TrieModel trie, KeyboardNeighborMap neighbors, string name, SpellingCase[] cases)
    {
        var clock = Stopwatch.StartNew();
        var outcomes = new List<Outcome>(cases.Length);
        foreach (var item in cases)
        {
            var prefix = TrieSuggestions.Predict(trie, "", item.Input, CancellationToken.None);
            var weighted = TrieSuggestions.Predict(trie, "", item.Input, CancellationToken.None, neighbors);
            outcomes.Add(new(item, TestData.HasUnigram(trie, item.Expected), Rank(prefix, item.Expected), Rank(weighted, item.Expected)));
        }
        clock.Stop();

        var groups = outcomes.GroupBy(outcome => outcome.Item.Category).OrderBy(group => group.Key, StringComparer.Ordinal)
            .Select(group => Summarize(group.Key, group)).ToArray();
        var missing = outcomes.Where(outcome => !outcome.InTrie).Select(outcome => outcome.Item.Expected)
            .Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToArray();
        return new(name, clock.Elapsed.TotalMilliseconds, Summarize("Overall", outcomes), groups, missing);
    }

    private static Result Rank(IReadOnlyList<string> suggestions, string expected)
    {
        for (int index = 0; index < Math.Min(3, suggestions.Count); index++)
            if (string.Equals(suggestions[index], expected, StringComparison.OrdinalIgnoreCase)) return new(true, index + 1);
        return new(suggestions.Count != 0, 0);
    }

    private static Metrics Summarize(string category, IEnumerable<Outcome> outcomes)
    {
        var items = outcomes.ToArray();
        int covered = items.Count(item => item.InTrie);
        return new(category, items.Length, covered, items.Length - covered,
            Hits(item => item.Prefix), Hits(item => item.Weighted));

        RankMetrics Hits(Func<Outcome, Result> select) => new(
            items.Count(item => select(item).HasSuggestions),
            items.Count(item => select(item).Rank == 1),
            items.Count(item => select(item).Rank != 0),
            items.Count(item => item.InTrie && select(item).Rank == 1),
            items.Count(item => item.InTrie && select(item).Rank != 0));
    }

    private static void Print(Report report)
    {
        Console.WriteLine($"Trie: {report.ModelPath} ({report.Bytes:N0} bytes, {report.Nodes:N0} nodes)");
        Console.WriteLine($"SHA-256: {report.Sha256}");
        foreach (var suite in new[] { report.SpellingCases, report.AdjacentCases })
        {
            Console.WriteLine($"\n{suite.Name} ({suite.ElapsedMilliseconds:F0} ms)");
            Console.WriteLine($"{"Category",-23} {"Cases",6} {"Coverage",12} {"Prefix top 1",13} {"Prefix top 3",13} {"Weighted top 1",15} {"Weighted top 3",15}");
            foreach (var metrics in new[] { suite.Overall }.Concat(suite.ByCategory))
                Console.WriteLine($"{metrics.Category,-23} {metrics.Cases,6:N0} {Ratio(metrics.Covered, metrics.Cases),12} " +
                    $"{Ratio(metrics.Prefix.Top1, metrics.Cases),13} {Ratio(metrics.Prefix.Top3, metrics.Cases),13} " +
                    $"{Ratio(metrics.Weighted.Top1, metrics.Cases),15} {Ratio(metrics.Weighted.Top3, metrics.Cases),15}");
            Console.WriteLine($"Missing from trie: {suite.MissingTargets.Length:N0} distinct targets in {suite.Overall.Missing:N0} cases");
            Console.WriteLine($"At least one suggestion: {suite.Overall.Prefix.HasSuggestions:N0} prefix, " +
                $"{suite.Overall.Weighted.HasSuggestions:N0} weighted, of {suite.Overall.Cases:N0} cases");
            Console.WriteLine($"On covered cases, prefix top 1 / top 3: {Ratio(suite.Overall.Prefix.CoveredTop1, suite.Overall.Covered)} / " +
                $"{Ratio(suite.Overall.Prefix.CoveredTop3, suite.Overall.Covered)}");
            Console.WriteLine($"On covered cases, weighted top 1 / top 3: {Ratio(suite.Overall.Weighted.CoveredTop1, suite.Overall.Covered)} / " +
                $"{Ratio(suite.Overall.Weighted.CoveredTop3, suite.Overall.Covered)}");
        }
    }

    private static string Ratio(int hits, int denominator) => denominator == 0 ? "n/a" : $"{(double)hits / denominator:P1}";

    private readonly record struct Result(bool HasSuggestions, int Rank);
    private sealed record Outcome(SpellingCase Item, bool InTrie, Result Prefix, Result Weighted);
    private sealed record Metrics(string Category, int Cases, int Covered, int Missing, RankMetrics Prefix, RankMetrics Weighted);
    private sealed record RankMetrics(int HasSuggestions, int Top1, int Top3, int CoveredTop1, int CoveredTop3);
    private sealed record ScoreReport(string Name, double ElapsedMilliseconds, Metrics Overall, Metrics[] ByCategory, string[] MissingTargets);
    private sealed record Report(string ModelPath, string Sha256, long Bytes, int Nodes, ScoreReport SpellingCases, ScoreReport AdjacentCases);
}
