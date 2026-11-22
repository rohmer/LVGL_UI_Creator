using System.Text.Json;
using System.Text.Json.Serialization;
using KeyboardSimulator;
using NgramTrie.Reader;

namespace GBUTest;

internal sealed record SpellingCase(
    [property: JsonPropertyName("input")] string Input,
    [property: JsonPropertyName("expected")] string Expected,
    [property: JsonPropertyName("category")] string Category);

internal static class TestData
{
    public static SpellingCase[] LoadSpellings()
    {
        string path = Path.Combine(AppContext.BaseDirectory, "common-misspellings.jsonl");
        var cases = new List<SpellingCase>();
        var inputs = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (line, index) in File.ReadLines(path).Select((line, index) => (line, index)))
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            SpellingCase? item;
            try { item = JsonSerializer.Deserialize<SpellingCase>(line); }
            catch (JsonException error)
            {
                throw new InvalidDataException($"Invalid JSON at {path}:{index + 1}.", error);
            }
            if (item is null || !IsWord(item.Input) || !IsWord(item.Expected) || string.IsNullOrWhiteSpace(item.Category) ||
                item.Input == item.Expected || !inputs.Add(item.Input))
                throw new InvalidDataException($"Invalid or duplicate spelling case at {path}:{index + 1}.");
            cases.Add(item);
        }
        if (cases.Count == 0) throw new InvalidDataException($"{path} contains no spelling cases.");
        return [.. cases];
    }

    public static KeyboardNeighborMap LoadNeighbors() =>
        new(KeyboardDefinition.Load(Path.Combine(AppContext.BaseDirectory, "Keyboards", "en-US-qwerty.json")));

    public static bool HasUnigram(TrieModel trie, string normalizedWord) =>
        trie.FindPrefix(normalizedWord, TrieRoot.Unigrams) is int index && trie.GetTerminalCount(index) != 0;

    public static bool IsWord(string? word) => !string.IsNullOrEmpty(word) && word.All(char.IsAsciiLetterLower);
}
