using System.Text;
using System.Text.Json;
using Entry = System.Collections.Generic.KeyValuePair<NgramTrie.Gram, long>;

namespace NgramTrie;

internal sealed record Selection(IReadOnlyList<Entry>[] Entries, int[] Totals, Trie? Trie);

internal static class Model
{
    private static readonly string[] Names = ["unigrams", "bigrams", "trigrams"];
    private static readonly IComparer<Entry> Frequency = Comparer<Entry>.Create(Compare);
    private static readonly IComparer<Entry> WorstFirst = Comparer<Entry>.Create((left, right) => Compare(right, left));

    public static Selection Select(Counts counts, Options options, Progress progress)
    {
        var ranked = new Entry[3][];
        int ordersRanked = 0;
        progress.Begin("Ranking n-grams", 70, 80, ranked.Length);
        Parallel.For(0, ranked.Length, new ParallelOptions { MaxDegreeOfParallelism = options.Workers, CancellationToken = progress.Cancellation }, order =>
        {
            int? limit = options.TrieSize is null ? options.Limits[order] : null;
            ranked[order] = Rank(counts.Orders[order], limit);
            counts.Orders[order] = new();
            progress.Update(Interlocked.Increment(ref ordersRanked));
        });
        return SelectRanked(ranked, ranked.Select(entries => (long)entries.Length).ToArray(), options, progress);
    }

    public static Selection SelectRanked(IEnumerable<Entry>[] ranked, long[] sizes, Options options, Progress progress)
    {
        var trie = options.TrieSize is not null || options.TrieOutput is not null ? new Trie(options.Compact) : null;
        bool includeJson = options.TrieOutput is null || options.JsonOutput is not null;
        var selected = new List<Entry>[] { [], [], [] };
        var totals = new int[3];
        long considered = 0;
        if (trie is not null) progress.Begin("Building trie", 80, 97, sizes.Sum());
        if (options.TrieSize is not { } bytes)
        {
            for (int order = 0; order < ranked.Length; order++)
                foreach (var entry in ranked[order])
                {
                    if ((considered++ & 511) == 0) progress.Update(considered);
                    trie?.TryAdd(Encoding.UTF8.GetBytes(entry.Key.Text), entry.Value, order == 0);
                    Keep(order, entry);
                }
            return new(selected, totals, trie);
        }

        long[] targets = sizes.Select((size, order) => Math.Min(size, options.Limits[order] ?? 0)).ToArray();
        var positions = new long[3];
        var candidates = ranked.Select(entries => entries.GetEnumerator()).ToArray();
        try
        {
            SelectUntil(targets);
            SelectUntil(sizes);
            return new(selected, totals, trie);
        }
        finally { foreach (var candidate in candidates) candidate.Dispose(); }

        void Keep(int order, Entry entry)
        {
            totals[order] = checked(totals[order] + 1);
            if (includeJson) selected[order].Add(entry);
        }

        void SelectUntil(long[] end)
        {
            while (positions[0] < end[0] || positions[1] < end[1] || positions[2] < end[2])
            {
                for (int order = 0; order < positions.Length; order++)
                {
                    bool unigram = order == 0;
                    int attempts = unigram ? 4 : 1;
                    for (int attempt = 0; attempt < attempts && positions[order] < end[order]; attempt++)
                    {
                        if ((considered++ & 511) == 0) progress.Update(considered);
                        if (!candidates[order].MoveNext()) throw new InvalidDataException("The ranked n-gram stream ended early.");
                        var entry = candidates[order].Current;
                        positions[order]++;
                        var path = Encoding.UTF8.GetBytes(entry.Key.Text);
                        if (!trie!.TryAdd(path, entry.Value, unigram, bytes)) continue;
                        Keep(order, entry);
                    }
                }
            }
        }
    }

    private static Entry[] Rank(Dictionary<Gram, long> counts, int? limit)
    {
        if (limit == 0 || counts.Count == 0) return [];
        Entry[] ranked;
        if (limit is { } top && top < counts.Count / 4)
        {
            var heap = new PriorityQueue<Entry, Entry>(top, WorstFirst);
            foreach (var entry in counts) heap.EnqueueDequeueWhenFull(entry, top);
            ranked = heap.UnorderedItems.Select(item => item.Element).ToArray();
        }
        else ranked = counts.ToArray();
        Array.Sort(ranked, Frequency);
        return limit is { } size && size < ranked.Length ? ranked[..size] : ranked;
    }

    private static void EnqueueDequeueWhenFull(this PriorityQueue<Entry, Entry> heap, Entry entry, int capacity)
    {
        if (heap.Count < capacity) heap.Enqueue(entry, entry);
        else heap.EnqueueDequeue(entry, entry);
    }

    private static int Compare(Entry left, Entry right)
    {
        int order = right.Value.CompareTo(left.Value);
        if (order != 0) return order;
        order = CompareTokens(left.Key.First, right.Key.First);
        if (order != 0) return order;
        order = CompareTokens(left.Key.Second, right.Key.Second);
        return order != 0 ? order : CompareTokens(left.Key.Third, right.Key.Third);
    }

    private static int CompareTokens(string? left, string? right)
    {
        if (left is null || right is null) return string.CompareOrdinal(left, right);
        int common = left.AsSpan().CommonPrefixLength(right);
        if (common == Math.Min(left.Length, right.Length)) return left.Length.CompareTo(right.Length);
        if (common > 0 && char.IsLowSurrogate(left[common])) common--;
        return Rune.GetRuneAt(left, common).Value.CompareTo(Rune.GetRuneAt(right, common).Value);
    }

    public static void WriteJson(IReadOnlyList<Entry>[] selected, Stream stream, Progress progress)
    {
        progress.Begin("Writing JSON", 97, 99, selected.Sum(order => (long)order.Count));
        long written = 0;
        using var writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true });
        writer.WriteStartObject();
        for (int order = 0; order < Names.Length; order++)
        {
            var prefixCounts = new Dictionary<Gram, long>();
            foreach (var entry in selected[order]) Counts.Increment(prefixCounts, entry.Key.Prefix, entry.Value);
            writer.WriteStartArray(Names[order]);
            foreach (var (gram, count) in selected[order])
            {
                if ((written++ & 511) == 0) progress.Update(written);
                writer.WriteStartObject();
                writer.WriteStartArray("tokens");
                writer.WriteStringValue(gram.First);
                if (gram.Second is not null) writer.WriteStringValue(gram.Second);
                if (gram.Third is not null) writer.WriteStringValue(gram.Third);
                writer.WriteEndArray();
                writer.WriteNumber("count", count);
                writer.WriteNumber("weight", (double)count / prefixCounts[gram.Prefix]);
                writer.WriteEndObject();
            }
            writer.WriteEndArray();
        }
        writer.WriteEndObject();
        writer.Flush();
        stream.WriteByte((byte)'\n');
    }
}

internal sealed class Trie(bool compact)
{
    public const int HeaderSize = 16;
    public const int RecordSize = 12;
    // Zero is the sentinel for child and sibling links. Actual children start at index 2.
    private struct Node(byte label)
    {
        public byte Label = label;
        public uint Mass;
        public int FirstChild;
        public int NextSibling;
    }

    private readonly List<Node> nodes = [new(0), new(0)];
    private long compactSize = HeaderSize + 4;
    public long Size => compact ? compactSize : HeaderSize + (long)nodes.Count * RecordSize;

    private long Growth(int parent, ReadOnlySpan<byte> path, uint count)
    {
        for (int index = 0; index < path.Length; index++)
        {
            int child = FindChild(parent, path[index], out _);
            if (child == 0 || nodes[child].Label != path[index])
            {
                int missing = path.Length - index;
                if (!compact) return (long)missing * RecordSize;

                // A new path starts with two bytes per node. Only the existing parent
                // can cross the one-byte flags boundary when it gains a child.
                int degree = 0;
                for (int sibling = nodes[parent].FirstChild; sibling != 0 && degree <= 63; sibling = nodes[sibling].NextSibling)
                    degree++;
                return (long)missing * 2 + VarintSize(count) + (degree == 63 ? 1 : 0);
            }
            parent = child;
        }
        if (!compact) return 0;

        uint terminal = nodes[parent].Mass;
        for (int child = nodes[parent].FirstChild; child != 0; child = nodes[child].NextSibling)
            terminal -= nodes[child].Mass;
        return VarintSize(terminal + count) - (terminal == 0 ? 0 : VarintSize(terminal));
    }

    public bool TryAdd(ReadOnlySpan<byte> path, long count, bool unigram, long? budget = null)
    {
        if (count > uint.MaxValue - nodes[0].Mass)
        {
            if (budget is not null) return false;
            throw new OverflowException("Trie exceeds the format's 32-bit count limit.");
        }
        long growth = compact || budget is not null
            ? Growth(0, path, (uint)count) + (unigram ? Growth(1, path, (uint)count) : 0)
            : 0;
        if (budget is long bytes && growth > bytes - Size) return false;
        Insert(0, path, (uint)count);
        if (unigram) Insert(1, path, (uint)count);
        if (compact) compactSize += growth;
        return true;
    }

    private void Insert(int parent, ReadOnlySpan<byte> path, uint count)
    {
        AddMass(parent, count);
        foreach (byte label in path)
        {
            int child = FindChild(parent, label, out int before);
            if (child == 0 || nodes[child].Label != label)
            {
                int created = nodes.Count;
                nodes.Add(new Node(label) { NextSibling = child });
                if (before == 0)
                {
                    var node = nodes[parent];
                    node.FirstChild = created;
                    nodes[parent] = node;
                }
                else
                {
                    var node = nodes[before];
                    node.NextSibling = created;
                    nodes[before] = node;
                }
                child = created;
            }
            parent = child;
            AddMass(parent, count);
        }
    }

    private void AddMass(int index, uint count)
    {
        var node = nodes[index];
        node.Mass += count;
        nodes[index] = node;
    }

    private int FindChild(int parent, byte label, out int before)
    {
        before = 0;
        int child = nodes[parent].FirstChild;
        while (child != 0 && nodes[child].Label < label)
        {
            before = child;
            child = nodes[child].NextSibling;
        }
        return child;
    }

    public void Write(Stream stream, Progress progress)
    {
        progress.Begin("Writing trie", 97, 99, nodes.Count);
        using var output = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
        output.Write("NGTR"u8);
        output.Write(compact ? 3u : 2u);
        output.Write((uint)nodes.Count);
        output.Write(compact ? 0u : (uint)RecordSize);
        var ordered = new List<int>(nodes.Count) { 0, 1 };
        for (int index = 0; index < ordered.Count; index++)
        {
            if ((index & 4095) == 0) progress.Update(index);
            var node = nodes[ordered[index]];
            int first = ordered.Count;
            uint terminal = node.Mass;
            for (int child = node.FirstChild; child != 0; child = nodes[child].NextSibling)
            {
                ordered.Add(child);
                if (compact) terminal -= nodes[child].Mass;
            }
            ushort childCount = (ushort)(ordered.Count - first);
            if (compact)
            {
                output.Write(node.Label);
                WriteVarint(output, ((uint)childCount << 1) | (terminal == 0 ? 0u : 1u));
                if (terminal != 0) WriteVarint(output, terminal);
            }
            else
            {
                output.Write(childCount == 0 ? 0u : (uint)first);
                output.Write(node.Mass);
                output.Write(childCount);
                output.Write(node.Label);
                output.Write((byte)0);
            }
        }
    }

    private static int VarintSize(uint value)
    {
        int size = 1;
        while ((value >>= 7) != 0) size++;
        return size;
    }

    private static void WriteVarint(BinaryWriter output, uint value)
    {
        while (value >= 128)
        {
            output.Write((byte)((value & 127) | 128));
            value >>= 7;
        }
        output.Write((byte)value);
    }
}
