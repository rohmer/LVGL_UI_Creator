using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using KeyboardSimulator;
using NgramTrie.Reader;

namespace GBUTest;

public sealed record SentencePair(string Original, string Fuzzed)
{
    public bool Changed => !string.Equals(Original, Fuzzed, StringComparison.Ordinal);
    public bool HasCommonMisspelling { get; init; }
}

public static partial class SentenceFuzzer
{
    public const int SampleSize = 1000;

    private const string WordCharacters = @"[\p{L}\p{M}\p{N}]";
    private const string WordPattern = WordCharacters + @"+(?:['’]" + WordCharacters + "+)*";
    private const string Month = @"(?:Jan(?:uary)?|Feb(?:ruary)?|Mar(?:ch)?|Apr(?:il)?|May|Jun(?:e)?|Jul(?:y)?|Aug(?:ust)?|Sep(?:t(?:ember)?)?|Oct(?:ober)?|Nov(?:ember)?|Dec(?:ember)?)";
    private const string Weekday = @"(?:Mon(?:day)?|Tue(?:s(?:day)?)?|Wed(?:nesday)?|Thu(?:r(?:s(?:day)?)?)?|Fri(?:day)?|Sat(?:urday)?|Sun(?:day)?)";
    private const string OrdinalUnderTen = "first|second|third|fourth|fifth|sixth|seventh|eighth|ninth";
    private const string DateNumber = @"(?:\p{Nd}{4}|\p{Nd}{1,2}(?:st|nd|rd|th)?|" + OrdinalUnderTen +
        @"|tenth|eleventh|twelfth|thirteenth|fourteenth|fifteenth|sixteenth|seventeenth|eighteenth|nineteenth|twentieth|twenty[ -](?:" +
        OrdinalUnderTen + @")|thirtieth|thirty[ -]first)";
    private const string DateSeparator = @"(?:\s+|\s*[-/]\s*)";
    private const string DatePattern = "(?:" + Weekday + @"\.?,?\s+)?(?:" +
        Month + @"\.?" + DateSeparator + @"(?:of\s+)?" + DateNumber + "|" +
        DateNumber + DateSeparator + @"(?:of\s+)?" + Month + @"\.?|\p{Nd}{1,4}[-/]\p{Nd}{1,2}[-/]\p{Nd}{1,4})";
    private const string TimePattern = @"\p{Nd}{1,2}(?::\p{Nd}{2}){0,2}\s*[ap]\.?m\.?";
    private const string DateOrTimePattern = "(?<!" + WordCharacters + ")(?:" + DatePattern + "|" + TimePattern +
        @")(?:['’]s)?(?!" + WordCharacters + "|['’]" + WordCharacters + ")";

    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
    private static readonly StringComparer PathComparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private static readonly Lazy<Dictionary<string, string[]>> Misspellings = new(() => TestData.LoadSpellings()
        .GroupBy(entry => entry.Expected, StringComparer.Ordinal)
        .ToDictionary(group => group.Key, group => group.Select(entry => entry.Input).ToArray(), StringComparer.Ordinal));
    private static readonly Lazy<KeyboardNeighborMap> Neighbors = new(TestData.LoadNeighbors);

    public static SentencePair[] Generate(string inputPath, string triePath, string outputPath, double adjacentKeyPercent,
        double commonMisspellingPercent, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        double adjacentChance = ToChance(adjacentKeyPercent, nameof(adjacentKeyPercent));
        double misspellingChance = ToChance(commonMisspellingPercent, nameof(commonMisspellingPercent));
        string source = Path.GetFullPath(inputPath);
        string modelPath = Path.GetFullPath(triePath);
        string destination = Path.GetFullPath(outputPath);
        string editsPath = GetEditMetadataPath(destination);
        if (PathComparer.Equals(source, destination) || PathComparer.Equals(modelPath, destination) ||
            PathComparer.Equals(source, editsPath) || PathComparer.Equals(modelPath, editsPath))
            throw new ArgumentException("Choose an output path that keeps the result files separate from the sentence input and eligibility trie.", nameof(outputPath));

        var trie = TrieModel.Load(modelPath);
        var random = new Random();
        string[] sample = SampleSentences(source, random, cancellation);
        var misspellings = misspellingChance > 0 ? Misspellings.Value : null;
        var neighbors = adjacentChance > 0 ? Neighbors.Value : null;
        var pairs = new SentencePair[sample.Length];
        for (int index = 0; index < sample.Length; index++)
        {
            cancellation.ThrowIfCancellationRequested();
            string original = sample[index];
            bool hasCommonMisspelling = false;
            string fuzzed = FuzzTokens().Replace(original, match => match.Groups["word"].Success &&
                !NumericText.TouchesNumber(original.AsSpan(0, match.Index), original.AsSpan(match.Index + match.Length))
                ? FuzzWord(match.Value, ref hasCommonMisspelling) : match.Value);
            pairs[index] = new(original, fuzzed) { HasCommonMisspelling = hasCommonMisspelling };
        }

        WritePairs(destination, editsPath, pairs, cancellation);
        return pairs;

        string FuzzWord(string original, ref bool hasCommonMisspelling)
        {
            if (NumericText.IsProtectedWord(original)) return original;
            string folded = TrieSuggestions.Fold(original);
            if (!TestData.HasUnigram(trie, folded)) return original;
            var fuzzed = original;
            if (misspellings is not null && misspellings.TryGetValue(folded, out string[]? alternatives) &&
                random.NextDouble() < misspellingChance)
            {
                fuzzed = MatchCase(original, alternatives[random.Next(alternatives.Length)]);
                hasCommonMisspelling = true;
            }
            if (neighbors is not null && random.NextDouble() < adjacentChance)
                fuzzed = SubstituteAdjacentKey(fuzzed, original, neighbors, random);
            return fuzzed;
        }
    }

    public static SentencePair[] ReadPairs(string path, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        using var input = File.OpenRead(path);
        using var reader = new StreamReader(input, Utf8, leaveOpen: true);
        var pairs = new List<SentencePair>();
        while (reader.ReadLine() is { } original)
        {
            cancellation.ThrowIfCancellationRequested();
            string fuzzed = reader.ReadLine() ?? throw new InvalidDataException($"Pair {pairs.Count + 1} is missing its fuzzed line.");
            pairs.Add(new(original, fuzzed));
        }
        if (pairs.Count == 0) throw new InvalidDataException("This file contains no sentence pairs.");
        RestoreEdits(GetEditMetadataPath(path), input, pairs);
        return [.. pairs];
    }

    public static string GetEditMetadataPath(string pairedFilePath) => pairedFilePath + ".edits.json";

    private static void RestoreEdits(string path, FileStream text, List<SentencePair> pairs)
    {
        EditMetadata? metadata;
        try
        {
            using var edits = File.OpenRead(path);
            metadata = JsonSerializer.Deserialize<EditMetadata>(edits);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        {
            // Older paired files have no edit metadata. The text still opens normally.
            return;
        }
        if (metadata?.CommonMisspellingRows is not { Length: > 0 } rows) return;

        text.Position = 0;
        if (metadata.TextSha256 != Convert.ToHexString(SHA256.HashData(text))) return;
        foreach (int index in rows)
            if ((uint)index < (uint)pairs.Count && pairs[index].Changed)
                pairs[index] = pairs[index] with { HasCommonMisspelling = true };
    }

    private static string[] SampleSentences(string path, Random random, CancellationToken cancellation)
    {
        using var reader = new StreamReader(path, Utf8);
        var sample = new string[SampleSize];
        long seen = 0;
        while (reader.ReadLine() is { } line)
        {
            cancellation.ThrowIfCancellationRequested();
            if (string.IsNullOrWhiteSpace(line)) continue;

            // Reservoir sampling gives every nonblank input line the same chance of inclusion.
            long slot = seen < sample.Length ? seen : random.NextInt64(seen + 1);
            if (slot < sample.Length) sample[(int)slot] = line;
            seen++;
        }
        if (seen < sample.Length)
            throw new InvalidDataException($"The input contains {seen:N0} nonblank lines. At least {sample.Length:N0} are required.");
        random.Shuffle(sample.AsSpan());
        return sample;
    }

    private static void WritePairs(string path, string editsPath, SentencePair[] pairs, CancellationToken cancellation)
    {
        string temporary = Path.Combine(Path.GetDirectoryName(path)!, $".gbutest-{Guid.NewGuid():N}.tmp");
        string temporaryEdits = GetEditMetadataPath(temporary);
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (var writer = new StreamWriter(stream, Utf8))
            {
                foreach (var pair in pairs)
                {
                    cancellation.ThrowIfCancellationRequested();
                    writer.WriteLine(pair.Original);
                    writer.WriteLine(pair.Fuzzed);
                }
            }

            var commonMisspellingRows = new List<int>();
            for (int index = 0; index < pairs.Length; index++)
                if (pairs[index].HasCommonMisspelling) commonMisspellingRows.Add(index);

            string hash;
            using (var text = File.OpenRead(temporary)) hash = Convert.ToHexString(SHA256.HashData(text));
            using (var edits = new FileStream(temporaryEdits, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                JsonSerializer.Serialize(edits, new EditMetadata(hash, [.. commonMisspellingRows]));

            cancellation.ThrowIfCancellationRequested();
            PublishPairs(temporary, path, temporaryEdits, editsPath);
        }
        finally
        {
            File.Delete(temporary);
            File.Delete(temporaryEdits);
        }
    }

    private static void PublishPairs(string temporary, string path, string temporaryEdits, string editsPath)
    {
        using var outputLock = new FileStream(editsPath + ".lock", FileMode.CreateNew, FileAccess.Write, FileShare.None,
            bufferSize: 1, FileOptions.DeleteOnClose);
        string previousEdits = temporaryEdits + ".previous";
        bool hasPrevious = File.Exists(editsPath);
        if (hasPrevious) File.Replace(temporaryEdits, editsPath, previousEdits, ignoreMetadataErrors: true);
        else File.Move(temporaryEdits, editsPath);

        try { File.Move(temporary, path, overwrite: true); }
        catch
        {
            if (hasPrevious) File.Replace(previousEdits, editsPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
            else File.Delete(editsPath);
            throw;
        }
        if (hasPrevious) File.Delete(previousEdits);
    }

    private sealed record EditMetadata(string TextSha256, int[] CommonMisspellingRows);

    private static string SubstituteAdjacentKey(string word, string original, KeyboardNeighborMap neighbors, Random random)
    {
        int position = -1;
        int eligible = 0;
        for (int index = 0; index < word.Length; index++)
        {
            if (!char.IsAsciiLetter(word[index]) || neighbors.GetNeighbors(new Rune(word[index])).Count == 0) continue;
            eligible++;
            if (random.Next(eligible) == 0) position = index;
        }
        if (position < 0) return word;

        // A second edit must not undo a common misspelling.
        bool canRevert = word.Length == original.Length && word.AsSpan(0, position).SequenceEqual(original.AsSpan(0, position)) &&
            word.AsSpan(position + 1).SequenceEqual(original.AsSpan(position + 1));
        bool uppercase = char.IsAsciiLetterUpper(word[position]);
        var keys = neighbors.GetNeighbors(new Rune(word[position]));
        Span<char> choices = stackalloc char[keys.Count];
        int count = 0;
        foreach (var key in keys)
        {
            char letter = (char)key.Value;
            if (uppercase) letter = char.ToUpperInvariant(letter);
            if (!canRevert || letter != original[position]) choices[count++] = letter;
        }
        if (count == 0) return word;
        return string.Concat(word.AsSpan(0, position), choices.Slice(random.Next(count), 1), word.AsSpan(position + 1));
    }

    private static string MatchCase(string original, string misspelling)
    {
        if (original.All(char.IsAsciiLetterUpper)) return misspelling.ToUpperInvariant();
        if (char.IsAsciiLetterUpper(original[0])) return char.ToUpperInvariant(misspelling[0]) + misspelling[1..];
        return misspelling;
    }

    private static double ToChance(double percent, string parameter)
    {
        if (!double.IsFinite(percent) || percent is < 0 or > 100)
            throw new ArgumentOutOfRangeException(parameter, "Enter a percentage from 0 to 100.");
        return percent / 100;
    }

    [GeneratedRegex(DateOrTimePattern + "|(?<word>" + WordPattern + ")", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex FuzzTokens();
}
