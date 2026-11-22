using System.Collections.Frozen;
using System.Diagnostics.CodeAnalysis;
using System.Security.Cryptography;
using System.Text;

namespace NgramTrie;

internal sealed class WordList(FrozenSet<string> words, string contentHash, string? language)
{
    private static readonly Dictionary<string, Lazy<WordList>> Bundled = new(StringComparer.Ordinal)
    {
        ["en"] = new(() => LoadResource("NgramTrie.WordsAlpha.txt", "the built-in English word list", "en")),
        ["es"] = new(() => LoadResource("NgramTrie.WordsSpanish.txt", "the built-in Spanish word list", "es")),
        ["fr"] = new(() => LoadResource("NgramTrie.WordsFrench.txt", "the built-in French word list", "fr")),
        ["de"] = new(() => LoadResource("NgramTrie.WordsGerman.txt", "the built-in German word list", "de")),
        ["el"] = new(() => LoadResource("NgramTrie.WordsGreek.txt", "the built-in Greek word list", "el")),
    };

    public string CacheIdentity { get; } = language switch
    {
        null or "en" => $"word-list-v1:{contentHash}",
        "es" => $"word-list-nfc-v1:{contentHash}",
        _ => $"word-list-{language}-v1:{contentHash}",
    };

    public static bool HasBundledLanguage(string language) => Bundled.ContainsKey(language);

    public bool UseListedSpelling { get; } = WordNormalization.UsesListedSpelling(language);

    public static WordList? For(Options options)
    {
        if (!options.UseWordList) return null;
        if (options.WordListPath is string path) return Load(File.ReadAllBytes(path), path,
            WordNormalization.UsesUnicodeTokens(options.Language) ? options.Language : null);
        return Bundled.TryGetValue(options.Language, out var bundled) ? bundled.Value : null;
    }

    public string NormalizeLine(string text) => WordNormalization.PrepareText(text, language);

    public bool TryGetWord(string token, [NotNullWhen(true)] out string? word) =>
        words.TryGetValue(WordNormalization.NormalizePreparedWord(token, language), out word);

    private static WordList LoadResource(string resource, string description, string language)
    {
        using var source = typeof(WordList).Assembly.GetManifestResourceStream(resource)
            ?? throw new IOException($"This build is missing {description}.");
        var bytes = new byte[checked((int)source.Length)];
        source.ReadExactly(bytes);
        return Load(bytes, description, language);
    }

    private static WordList Load(byte[] bytes, string description, string? language = null)
    {
        using var snapshot = new MemoryStream(bytes, writable: false);
        using var reader = Corpus.OpenText(snapshot);
        var words = new HashSet<string>(StringComparer.Ordinal);
        bool useListedSpelling = WordNormalization.UsesListedSpelling(language);
        try
        {
            while (reader.ReadLine() is { } line)
            {
                string word = line.Trim();
                if (word.Length == 0 || word[0] == '#') continue;
                if (word.Any(char.IsWhiteSpace))
                    throw new IOException($"Use one word per line in {description}.");
                if (!useListedSpelling) word = WordNormalization.PrepareText(word, language);
                words.Add(word);
            }
        }
        catch (DecoderFallbackException error)
        {
            throw new IOException($"Cannot decode {description}: {error.Message}", error);
        }
        if (words.Count == 0) throw new IOException($"No words found in {description}.");
        return new(words.ToFrozenSet(StringComparer.Ordinal), Convert.ToHexString(SHA256.HashData(bytes)), language);
    }
}
