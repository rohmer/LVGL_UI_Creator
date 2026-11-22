using System.Buffers;
using System.Collections.Concurrent;
using System.Text.Encodings.Web;
using System.Text.Json;
using Microsoft.Data.Sqlite;
using Entry = System.Collections.Generic.KeyValuePair<NgramTrie.Gram, long>;

namespace NgramTrie;

internal static class DiskCounts
{
    private const int CacheVersion = 1;
    private const string Table = """
        CREATE TABLE grams(ord INTEGER NOT NULL, gram TEXT NOT NULL, frequency INTEGER NOT NULL,
            PRIMARY KEY(ord, gram)) WITHOUT ROWID;
        """;
    private const string Upsert = "ON CONFLICT(ord, gram) DO UPDATE SET frequency=grams.frequency+excluded.frequency";

    public static Selection Select(List<string> paths, Options options, Progress progress)
    {
        var wordList = WordList.For(options);
        // Count databases are high-churn intermediates. Keep them off a cloud
        // synchronized output directory while final publication stays atomic.
        string scratch = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "espos-ngram-dotnet", "work");
        Directory.CreateDirectory(scratch);
        string temporary = Temporary(scratch);
        if (options.CacheDirectory is { } cache) Directory.CreateDirectory(cache);
        var enabled = options.Limits.Select(limit => limit != 0).ToArray();
        try
        {
            using var database = Create(temporary);
            long completed = 0;
            foreach (string path in paths)
            {
                progress.Cancellation.ThrowIfCancellationRequested();
                var signature = Corpus.Signature.For(path);
                void Report(long bytes) => progress.Update(completed + bytes);

                if (options.CacheDirectory is { } cacheDirectory)
                {
                    var cachePath = Path.Combine(cacheDirectory, Corpus.CacheKey(path, wordList, options.Language) + ".grams.sqlite");
                    if (!CacheMatches(cachePath, signature))
                        BuildCache(cachePath, path, signature, wordList, options.Workers, Report, progress.Cancellation, options.Language);
                    MergeCache(database, cachePath, enabled);
                }
                else CountFile(database, path, enabled, wordList, options.Workers, Report, progress.Cancellation, options.Language);

                VerifyUnchanged(path, signature);
                completed += signature.Length;
                progress.Update(completed);
            }

            progress.Begin("Ranking n-grams on disk", 70, 80, 1);
            var sizes = Sizes(database, options);
            Execute(database, "CREATE INDEX ranked ON grams(ord, frequency DESC, gram);");
            progress.Update(1);
            var ranked = sizes.Select((size, order) => Rank(database, order, size)).ToArray();
            return Model.SelectRanked(ranked, sizes, options, progress);
        }
        finally { File.Delete(temporary); }
    }

    private static void BuildCache(string destination, string source, Corpus.Signature signature, WordList? wordList, int workers,
        Action<long> report, CancellationToken cancellation, string language)
    {
        var temporary = Temporary(Path.GetDirectoryName(destination)!);
        try
        {
            using (var database = Create(temporary))
            {
                CountFile(database, source, [true, true, true], wordList, workers, report, cancellation, language);
                VerifyUnchanged(source, signature);
                using var command = database.CreateCommand();
                command.CommandText = """
                    CREATE TABLE signature(version INTEGER, length INTEGER, modified INTEGER, created INTEGER);
                    INSERT INTO signature VALUES($version, $length, $modified, $created);
                    """;
                command.Parameters.AddWithValue("$version", CacheVersion);
                command.Parameters.AddWithValue("$length", signature.Length);
                command.Parameters.AddWithValue("$modified", signature.Modified);
                command.Parameters.AddWithValue("$created", signature.Created);
                command.ExecuteNonQuery();
            }
            cancellation.ThrowIfCancellationRequested();
            File.Move(temporary, destination, overwrite: true);
        }
        finally { File.Delete(temporary); }
    }

    private static bool CacheMatches(string path, Corpus.Signature signature)
    {
        if (!File.Exists(path)) return false;
        try
        {
            using var database = Open(path, SqliteOpenMode.ReadOnly);
            using var command = database.CreateCommand();
            command.CommandText = "SELECT version, length, modified, created FROM signature";
            using var reader = command.ExecuteReader();
            return reader.Read() && reader.GetInt32(0) == CacheVersion &&
                new Corpus.Signature(reader.GetInt64(1), reader.GetInt64(2), reader.GetInt64(3)) == signature;
        }
        catch (SqliteException) { return false; }
    }

    private static void MergeCache(SqliteConnection database, string path, bool[] enabled)
    {
        using var command = database.CreateCommand();
        command.CommandText = "ATTACH DATABASE $path AS cached";
        command.Parameters.AddWithValue("$path", path);
        command.ExecuteNonQuery();
        try
        {
            Execute(database, "PRAGMA cached.cache_size = -4096;");
            command.CommandText = $"""
                INSERT INTO grams SELECT ord, gram, frequency FROM cached.grams
                WHERE (ord=0 AND $uni) OR (ord=1 AND $bi) OR (ord=2 AND $tri)
                {Upsert};
                """;
            command.Parameters.Clear();
            command.Parameters.AddWithValue("$uni", enabled[0]);
            command.Parameters.AddWithValue("$bi", enabled[1]);
            command.Parameters.AddWithValue("$tri", enabled[2]);
            command.ExecuteNonQuery();
        }
        finally { Execute(database, "DETACH DATABASE cached"); }
    }

    private static void CountFile(SqliteConnection database, string path, bool[] enabled, WordList? wordList, int workers,
        Action<long> report, CancellationToken cancellation, string language)
    {
        var gate = new object();
        using var transaction = database.BeginTransaction();
        using var command = database.CreateCommand();
        command.Transaction = transaction;
        command.CommandText = $"""
            INSERT INTO grams SELECT $order, key, value FROM json_each(CAST($batch AS TEXT))
            WHERE true ORDER BY key {Upsert};
            """;
        var orderParameter = command.Parameters.Add("$order", SqliteType.Integer);
        var batchParameter = command.Parameters.Add("$batch", SqliteType.Blob);
        var buffer = new ArrayBufferWriter<byte>();
        using var writer = new Utf8JsonWriter(buffer, new JsonWriterOptions { Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping });
        var batches = Partitioner.Create(Corpus.ReadBatches(path, report, cancellation), EnumerablePartitionerOptions.NoBuffering);
        Parallel.ForEach(batches, new ParallelOptions { MaxDegreeOfParallelism = workers, CancellationToken = cancellation }, batch =>
        {
            var counts = new Counts();
            Corpus.CountLines(batch, counts, enabled, wordList, language);
            lock (gate)
            {
                cancellation.ThrowIfCancellationRequested();
                for (int order = 0; order < 3; order++)
                {
                    if (counts.Orders[order].Count == 0) continue;
                    orderParameter.Value = order;
                    int rows = 0;
                    StartBatch();
                    foreach (var (gram, count) in counts.Orders[order])
                    {
                        if (rows == 2048 || writer.BytesCommitted + writer.BytesPending >= 96 * 1024)
                        {
                            WriteBatch();
                            StartBatch();
                            rows = 0;
                        }
                        writer.WriteNumber(gram.Text, count);
                        rows++;
                    }
                    WriteBatch();
                }
            }
        });
        transaction.Commit();

        void StartBatch()
        {
            buffer.Clear();
            writer.Reset(buffer);
            writer.WriteStartObject();
        }

        void WriteBatch()
        {
            cancellation.ThrowIfCancellationRequested();
            writer.WriteEndObject();
            writer.Flush();
            batchParameter.Value = buffer.WrittenSpan.ToArray();
            command.ExecuteNonQuery();
        }
    }

    private static long[] Sizes(SqliteConnection database, Options options)
    {
        var sizes = new long[3];
        using var command = database.CreateCommand();
        command.CommandText = "SELECT ord, count(*) FROM grams GROUP BY ord";
        using var reader = command.ExecuteReader();
        while (reader.Read())
        {
            int order = reader.GetInt32(0);
            long count = reader.GetInt64(1);
            sizes[order] = options.TrieSize is null && options.Limits[order] is int limit ? Math.Min(count, limit) : count;
        }
        return sizes;
    }

    private static IEnumerable<Entry> Rank(SqliteConnection database, int order, long size)
    {
        if (size == 0) yield break;
        using var command = database.CreateCommand();
        // Space sorts before every word character, so joined UTF-8 keys preserve token-tuple order.
        command.CommandText = "SELECT gram, frequency FROM grams WHERE ord=$order ORDER BY frequency DESC, gram LIMIT $limit";
        command.Parameters.AddWithValue("$order", order);
        command.Parameters.AddWithValue("$limit", size);
        using var reader = command.ExecuteReader();
        while (reader.Read())
        {
            string text = reader.GetString(0);
            int first = order == 0 ? -1 : text.IndexOf(' ');
            int second = order == 2 ? text.IndexOf(' ', first + 1) : -1;
            var gram = order switch
            {
                0 => new Gram(text),
                1 => new Gram(text[..first], text[(first + 1)..]),
                _ => new Gram(text[..first], text[(first + 1)..second], text[(second + 1)..]),
            };
            yield return new(gram, reader.GetInt64(1));
        }
    }

    private static SqliteConnection Create(string path)
    {
        var connection = Open(path, SqliteOpenMode.ReadWriteCreate);
        try
        {
            // Temporary databases are disposable and cache files are published only after a successful close.
            Execute(connection, "PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA temp_store=FILE;");
            Execute(connection, Table);
            return connection;
        }
        catch { connection.Dispose(); throw; }
    }

    private static SqliteConnection Open(string path, SqliteOpenMode mode)
    {
        var connection = new SqliteConnection(new SqliteConnectionStringBuilder
            { DataSource = path, Mode = mode, Pooling = false }.ConnectionString);
        try { connection.Open(); return connection; }
        catch { connection.Dispose(); throw; }
    }

    private static void Execute(SqliteConnection connection, string sql)
    {
        using var command = connection.CreateCommand();
        command.CommandText = sql;
        command.ExecuteNonQuery();
    }

    private static void VerifyUnchanged(string path, Corpus.Signature signature)
    {
        if (Corpus.Signature.For(path) != signature) throw new IOException($"Corpus file changed while processing: {path}");
    }

    private static string Temporary(string directory) => Path.Combine(directory, $".ngram-{Guid.NewGuid():N}.sqlite");
}
