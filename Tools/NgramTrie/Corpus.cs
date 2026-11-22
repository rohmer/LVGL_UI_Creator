using System.Collections.Concurrent;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace NgramTrie;

internal readonly record struct Gram(string First, string? Second = null, string? Third = null)
{
    public Gram Prefix => Second is null ? default : new(First, Third is null ? null : Second);
    public string Text => Third is not null ? string.Concat(First, " ", Second, " ", Third) :
        Second is not null ? string.Concat(First, " ", Second) : First;
}

internal sealed class Counts
{
    public Dictionary<Gram, long>[] Orders { get; } = [new(), new(), new()];

    public void Merge(Counts other)
    {
        for (int order = 0; order < Orders.Length; order++)
            foreach (var entry in other.Orders[order]) Increment(Orders[order], entry.Key, entry.Value);
    }

    public static void Increment(Dictionary<Gram, long> counts, Gram gram, long amount = 1) =>
        CollectionsMarshal.GetValueRefOrAddDefault(counts, gram, out _) += amount;
}

internal static class Corpus
{
    private const int CacheVersion = 1;
    private static readonly Encoding Utf8 = new UTF8Encoding(false, true);

    public static List<string> FindFiles(List<string> inputs, string? singleFile, bool includeSubdirectories)
    {
        var paths = new List<string>();
        foreach (string path in singleFile is null ? inputs : [singleFile])
        {
            if (singleFile is null && Directory.Exists(path))
            {
                var files = Directory.EnumerateFiles(path, "*", new EnumerationOptions
                    {
                        RecurseSubdirectories = includeSubdirectories,
                        AttributesToSkip = FileAttributes.ReparsePoint,
                        IgnoreInaccessible = false,
                    })
                    .Where(file => Path.GetExtension(file).Equals(".txt", StringComparison.OrdinalIgnoreCase))
                    .Order(StringComparer.Ordinal).ToArray();
                if (files.Length == 0)
                    throw new IOException($"No .txt files found {(includeSubdirectories ? "in" : "directly in")} {path}.");
                paths.AddRange(files);
            }
            else if (File.Exists(path)) paths.Add(path);
            else throw new FileNotFoundException($"Corpus file not found: {path}");
        }
        return paths;
    }

    public static Counts Count(List<string> paths, Options options, Progress progress)
    {
        var wordList = WordList.For(options);
        var enabled = options.Limits.Select(limit => limit != 0).ToArray();
        bool[] allOrders = [true, true, true];
        if (options.CacheDirectory is { } directory) Directory.CreateDirectory(directory);
        Counts? combined = null;
        long completed = 0;
        progress.Update(0);
        int fileWorkers = Math.Min(paths.Count, options.Workers);
        int lineWorkers = options.Workers / fileWorkers;
        var mergeLock = new object();
        Parallel.ForEach(paths, new ParallelOptions { MaxDegreeOfParallelism = fileWorkers, CancellationToken = progress.Cancellation }, path =>
        {
            var signature = Signature.For(path);
            long reported = 0;
            void Report(long position)
            {
                progress.Update(Interlocked.Add(ref completed, position - reported));
                reported = position;
            }

            Counts? cached = null;
            string? cachePath = null;
            if (options.CacheDirectory is not null)
            {
                cachePath = Path.Combine(options.CacheDirectory, CacheKey(path, wordList, options.Language) + ".grams.gz");
                cached = ReadCache(cachePath, signature, enabled, Report);
            }

            var counts = cached ?? CountFile(path, cachePath is null ? enabled : allOrders, wordList, lineWorkers, Report, progress.Cancellation, options.Language);
            if (Signature.For(path) != signature) throw new IOException($"Corpus file changed while processing: {path}");
            if (cached is null && cachePath is not null) WriteCache(cachePath, signature, counts, progress.Cancellation);
            for (int order = 0; order < enabled.Length; order++)
                if (!enabled[order]) counts.Orders[order].Clear();
            lock (mergeLock)
            {
                if (combined is null) combined = counts;
                else combined.Merge(counts);
            }
            Report(signature.Length);
        });
        return combined ?? new Counts();
    }

    internal static string CacheKey(string path, WordList? wordList, string language = "en")
    {
        string key = OperatingSystem.IsWindows() ? path.ToUpperInvariant() : path;
        if (wordList is not null) key = $"{wordList.CacheIdentity}\0{key}";
        if (WordNormalization.UsesUnicodeTokens(language)) key = $"unicode-nfc-tokens-v1:{language}\0{key}";
        return Convert.ToHexString(SHA256.HashData(Utf8.GetBytes(key)));
    }

    private static Counts CountFile(string path, bool[] enabled, WordList? wordList, int workers, Action<long> progress,
        CancellationToken cancellation, string language)
    {
        var result = new Counts();
        var mergeLock = new object();
        if (workers == 1)
        {
            foreach (var batch in ReadBatches(path, progress, cancellation)) CountLines(batch, result, enabled, wordList, language);
        }
        else
        {
            var batches = Partitioner.Create(ReadBatches(path, progress, cancellation), EnumerablePartitionerOptions.NoBuffering);
            Parallel.ForEach(batches, new ParallelOptions { MaxDegreeOfParallelism = workers, CancellationToken = cancellation },
                () => new Counts(), (batch, _, local) =>
                {
                    CountLines(batch, local, enabled, wordList, language);
                    return local;
                }, local => { lock (mergeLock) result.Merge(local); });
        }
        return result;
    }

    internal static IEnumerable<List<string>> ReadBatches(string path, Action<long> progress, CancellationToken cancellation)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 65536, FileOptions.SequentialScan);
        using var reader = OpenText(stream);
        while (true)
        {
            cancellation.ThrowIfCancellationRequested();
            var batch = new List<string>(512);
            int characters = 0;
            while (batch.Count < 512 && characters < 131072)
            {
                string? line;
                try { line = reader.ReadLine(); }
                catch (DecoderFallbackException error) { throw new IOException($"Cannot decode {path}: {error.Message}", error); }
                if (line is null) break;
                batch.Add(line);
                characters += line.Length;
            }
            if (batch.Count == 0) yield break;
            yield return batch;
            progress(stream.Position);
        }
    }

    internal static StreamReader OpenText(Stream stream)
    {
        Span<byte> bom = stackalloc byte[4];
        int bytes = stream.ReadAtLeast(bom, bom.Length, throwOnEndOfStream: false);
        var prefix = bom[..bytes];
        (Encoding encoding, int skip) = prefix switch
        {
            [0xFF, 0xFE, 0x00, 0x00, ..] => (new UTF32Encoding(false, false, true), 4),
            [0x00, 0x00, 0xFE, 0xFF, ..] => (new UTF32Encoding(true, false, true), 4),
            [0xFF, 0xFE, ..] => (new UnicodeEncoding(false, false, true), 2),
            [0xFE, 0xFF, ..] => (new UnicodeEncoding(true, false, true), 2),
            [0xEF, 0xBB, 0xBF, ..] => (Utf8, 3),
            _ => (Utf8, 0),
        };
        stream.Position = skip;
        return new StreamReader(stream, encoding, detectEncodingFromByteOrderMarks: false, bufferSize: 65536, leaveOpen: true);
    }

    internal static void CountLines(IEnumerable<string> batch, Counts counts, bool[] enabled, WordList? wordList, string language = "en")
    {
        if (WordNormalization.UsesUnicodeTokens(language))
        {
            foreach (string line in batch)
            {
                string? previous = null, older = null;
                foreach (string? candidate in WordNormalization.UnicodeTokens(line, language))
                {
                    if (candidate is null) { previous = older = null; continue; }
                    string token = candidate;
                    if (wordList is not null)
                    {
                        if (!wordList.TryGetWord(token, out string? accepted)) { previous = older = null; continue; }
                        token = accepted;
                    }
                    if (enabled[0]) Counts.Increment(counts.Orders[0], new Gram(token));
                    if (enabled[1] && previous is not null) Counts.Increment(counts.Orders[1], new Gram(previous, token));
                    if (enabled[2] && older is not null) Counts.Increment(counts.Orders[2], new Gram(older, previous, token));
                    older = previous; previous = token;
                }
            }
            return;
        }
        bool useListedSpelling = wordList?.UseListedSpelling == true;
        foreach (string line in batch)
        {
            bool ascii = Ascii.IsValid(line);
            string text = ascii ? line.ToLowerInvariant() : wordList?.NormalizeLine(line) ?? WordNormalization.Fold(line);
            string? previous = null, older = null;
            int index = 0;
            while (index < text.Length)
            {
                int width = WordWidth(text, index, ascii);
                if (width == 0)
                {
                    if (text[index] == '.') previous = older = null;
                    index += char.IsHighSurrogate(text[index]) ? 2 : 1;
                    continue;
                }
                int start = index;
                do
                {
                    index += width;
                    width = WordWidth(text, index, ascii, includeMarks: useListedSpelling);
                    if (width == 0 && index < text.Length && text[index] is '\'' or '’')
                    {
                        width = WordWidth(text, index + 1, ascii);
                        if (width > 0) index++;
                    }
                } while (width > 0);

                string token = text[start..index];
                if (wordList is not null)
                {
                    if (!wordList.TryGetWord(token, out string? accepted))
                    {
                        previous = older = null;
                        continue;
                    }
                    token = accepted;
                }
                if (enabled[0]) Counts.Increment(counts.Orders[0], new Gram(token));
                if (enabled[1] && previous is not null) Counts.Increment(counts.Orders[1], new Gram(previous, token));
                if (enabled[2] && older is not null) Counts.Increment(counts.Orders[2], new Gram(older, previous, token));
                older = previous;
                previous = token;
            }
        }
    }

    private static int WordWidth(string text, int index, bool ascii, bool includeMarks = false)
    {
        if (index >= text.Length) return 0;
        if (ascii || text[index] < 128) return char.IsAsciiLetterOrDigit(text[index]) ? 1 : 0;
        Rune rune = Rune.GetRuneAt(text, index);
        return WordNormalization.IsWordRune(rune, includeMarks) ? rune.Utf16SequenceLength : 0;
    }

    internal readonly record struct Signature(long Length, long Modified, long Created)
    {
        public static Signature For(string path)
        {
            var file = new FileInfo(path);
            return new(file.Length, file.LastWriteTimeUtc.Ticks, file.CreationTimeUtc.Ticks);
        }
    }

    private static Counts? ReadCache(string path, Signature signature, bool[] enabled, Action<long> progress)
    {
        try
        {
            using var source = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete);
            using var gzip = new GZipStream(source, CompressionMode.Decompress);
            using var reader = new BinaryReader(gzip, Utf8);
            if (reader.ReadInt32() != CacheVersion ||
                new Signature(reader.ReadInt64(), reader.ReadInt64(), reader.ReadInt64()) != signature) return null;
            var counts = new Counts();
            for (int order = 0; order < 3; order++)
            {
                int entries = reader.ReadInt32();
                if (entries < 0) return null;
                for (int index = 0; index < entries; index++)
                {
                    if ((index & 4095) == 0) progress((long)(signature.Length * (double)source.Position / source.Length));
                    var gram = new Gram(reader.ReadString(), order >= 1 ? reader.ReadString() : null,
                        order >= 2 ? reader.ReadString() : null);
                    long count = reader.ReadInt64();
                    if (count < 1) return null;
                    if (enabled[order] && !counts.Orders[order].TryAdd(gram, count)) return null;
                }
            }
            return gzip.ReadByte() == -1 ? counts : null;
        }
        catch (Exception error) when (error is IOException or InvalidDataException or FormatException or ArgumentException) { return null; }
    }

    private static void WriteCache(string path, Signature signature, Counts counts, CancellationToken cancellation) => Program.AtomicWrite(path, stream =>
    {
        using var gzip = new GZipStream(stream, CompressionLevel.Fastest, leaveOpen: true);
        using var writer = new BinaryWriter(gzip, Utf8);
        writer.Write(CacheVersion);
        writer.Write(signature.Length);
        writer.Write(signature.Modified);
        writer.Write(signature.Created);
        foreach (var order in counts.Orders)
        {
            writer.Write(order.Count);
            int index = 0;
            foreach (var (gram, count) in order)
            {
                if ((index++ & 4095) == 0) cancellation.ThrowIfCancellationRequested();
                writer.Write(gram.First);
                if (gram.Second is not null) writer.Write(gram.Second);
                if (gram.Third is not null) writer.Write(gram.Third);
                writer.Write(count);
            }
        }
    }, cancellation);
}
