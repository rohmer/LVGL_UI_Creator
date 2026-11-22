namespace NgramTrie;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length == 0 || args is ["--help" or "-h"])
        {
            Console.WriteLine("""
                Generate a weighted ESP32 n-gram trie from Unicode text.

                NgramTrie <corpus files or directories> [options]
                NgramTrie --file <file> [options]

                  --trie-output FILE     Write the NGTR binary
                  --trie-size BYTES      Maximum binary size (minimum 40, or 20 with --compact)
                  --compact             Write compact NGTR v3 records
                  --language CODE       en|es|fr|de|el|ar|hi|zh|bn|id|ur|ru (default: en)
                  --wordlist FILE        Custom vocabulary for en or a new Unicode language
                  --no-wordlist          Disable filtering (the original es/fr/de/el require their bundled list)
                  --unigrams N           Retain N unigrams
                  --bigrams N            Retain N bigrams
                  --trigrams N           Retain N trigrams
                  --workers N            Counting workers (default: logical processors)
                  --no-subdirectories    Search only the listed directories for .txt files
                  -o, --output FILE      Write JSON (default: stdout if no binary output)
                  --cache-dir DIRECTORY  Location of per-file count caches
                  --no-cache             Count the source text again
                  --low-memory           Keep counts and rankings on disk

                Corpus directories are searched recursively for .txt files by default.
                A limit of zero disables that order. With a byte budget, positive limits are targets
                and unigrams get four attempts for every attempt at each higher order.
                ar/hi/zh/bn/id/ur/ru use NFC Unicode tokens without a bundled word filter.
                Mandarin uses Han character n-grams; custom Han vocabulary has one character per line.
                """);
            return 0;
        }

        bool interactive = !Console.IsErrorRedirected;
        try
        {
            Generator.Generate(args, interactive ? ShowProgress : null);
            return 0;
        }
        catch (Exception error) when (error is IOException or ArgumentException or AggregateException or
                                     UnauthorizedAccessException or OverflowException or Microsoft.Data.Sqlite.SqliteException)
        {
            var cause = error is AggregateException aggregate ? aggregate.Flatten().InnerExceptions[0] : error;
            Console.Error.WriteLine($"NgramTrie: {cause.Message}");
            return 1;
        }
        finally { if (interactive) Console.Error.WriteLine(); }
    }

    private static void ShowProgress(GenerationProgress progress)
    {
        int cells = progress.Percent * 24 / 100;
        Console.Error.Write($"\r[{new string('#', cells)}{new string('-', 24 - cells)}] {progress.Percent,3}%  {progress.Stage,-20}");
    }

    internal static StringComparer PathComparer => OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;

    internal static void AtomicWrite(string path, Action<Stream> write, CancellationToken cancellation = default)
    {
        string temporary = Path.Combine(Path.GetDirectoryName(path)!, $".ngram-{Guid.NewGuid():N}.tmp");
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None, 65536))
                write(stream);
            cancellation.ThrowIfCancellationRequested();
            File.Move(temporary, path, overwrite: true);
        }
        finally { File.Delete(temporary); }
    }
}

internal sealed class Options
{
    public List<string> Inputs { get; } = [];
    public string? SingleFile { get; private set; }
    public string? JsonOutput { get; private set; }
    public string? TrieOutput { get; private set; }
    public string? CacheDirectory { get; private set; }
    public long? TrieSize { get; private set; }
    public int?[] Limits { get; } = new int?[3];
    public int Workers { get; private set; } = Environment.ProcessorCount;
    public bool IncludeSubdirectories { get; private set; } = true;
    public bool LowMemory { get; private set; }
    public bool Compact { get; private set; }
    public string Language { get; private set; } = "en";
    public bool UseWordList { get; private set; } = true;
    public string? WordListPath { get; private set; }

    public static Options Parse(string[] args)
    {
        var options = new Options();
        bool noCache = false;
        for (int i = 0; i < args.Length; i++)
        {
            var flag = args[i];
            switch (flag)
            {
                case "--file" or "-f": options.SingleFile = Path.GetFullPath(Value()); break;
                case "--output" or "-o": options.JsonOutput = Path.GetFullPath(Value()); break;
                case "--trie-output": options.TrieOutput = Path.GetFullPath(Value()); break;
                case "--trie-size": options.TrieSize = Number(20, long.MaxValue); break;
                case "--compact": options.Compact = true; break;
                case "--language":
                    options.Language = Value().ToLowerInvariant();
                    if (!WordNormalization.SupportsLanguage(options.Language))
                        throw new ArgumentException("Use --language en, es, fr, de, el, ar, hi, zh, bn, id, ur, or ru.");
                    break;
                case "--wordlist": options.WordListPath = Path.GetFullPath(Value()); break;
                case "--no-wordlist": options.UseWordList = false; break;
                case "--workers": options.Workers = (int)Number(1, int.MaxValue); break;
                case "--unigrams": options.Limits[0] = (int)Number(0, int.MaxValue); break;
                case "--bigrams": options.Limits[1] = (int)Number(0, int.MaxValue); break;
                case "--trigrams": options.Limits[2] = (int)Number(0, int.MaxValue); break;
                case "--no-subdirectories": options.IncludeSubdirectories = false; break;
                case "--cache-dir": options.CacheDirectory = Path.GetFullPath(Value()); break;
                case "--no-cache": noCache = true; break;
                case "--low-memory": options.LowMemory = true; break;
                case "--": options.Inputs.AddRange(args[(i + 1)..].Select(Path.GetFullPath)); i = args.Length; break;
                default:
                    if (flag.StartsWith('-')) throw new ArgumentException($"Unknown option: {flag}");
                    options.Inputs.Add(Path.GetFullPath(flag));
                    break;
            }

            string Value() => ++i < args.Length ? args[i] : throw new ArgumentException($"{flag} needs a value.");
            long Number(long minimum, long maximum) => long.TryParse(Value(), out long value) &&
                value >= minimum && value <= maximum ? value : throw new ArgumentException($"Invalid value for {flag}.");
        }

        if (options.SingleFile is not null && options.Inputs.Count != 0)
            throw new ArgumentException("Use --file or positional corpus inputs.");
        if (options.SingleFile is null && options.Inputs.Count == 0)
            throw new ArgumentException("Provide a corpus input or use --file.");
        if (!options.Compact && options.TrieSize is < 40)
            throw new ArgumentException("An NGTR v2 trie needs at least 40 bytes.");
        if (WordList.HasBundledLanguage(options.Language) && options.Language != "en" && (!options.UseWordList || options.WordListPath is not null))
            throw new ArgumentException($"--language {options.Language} requires its bundled word list.");
        if (!options.UseWordList && options.WordListPath is not null)
            throw new ArgumentException("Use --no-wordlist or --wordlist.");
        if (noCache && options.CacheDirectory is not null)
            throw new ArgumentException("Use --no-cache or --cache-dir.");
        if (!noCache)
            options.CacheDirectory ??= Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "espos-ngram-dotnet");
        return options;
    }
}

