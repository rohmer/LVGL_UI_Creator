using System.Buffers.Binary;
using System.Text;

namespace NgramTrie.Reader;

public enum TrieRoot { Combined = 0, Unigrams = 1 }

public readonly record struct TrieNode(uint Mass, int FirstChild, int ChildCount, byte Label);
public readonly record struct TrieTransition(byte Label, int NodeIndex, uint Mass, double Probability);

/// <summary>A read-only NGTR version 2 or 3 model. Nodes are addressed by their on-disk indices.</summary>
public sealed class TrieModel
{
    private const int HeaderSize = 16;
    private const int RecordSize = 12;
    private static readonly Encoding Utf8 = new UTF8Encoding(false, true);
    private readonly byte[] records;

    private TrieModel(byte[] records, Header header)
    {
        this.records = records;
        Version = header.Version;
        NodeCount = header.NodeCount;
        Validate();
    }

    public uint Version { get; }
    public int NodeCount { get; }

    public static TrieModel Load(string path)
    {
        using var source = File.OpenRead(path);
        return Load(source);
    }

    /// <summary>Reads one complete model from the current stream position and leaves the stream open.</summary>
    public static TrieModel Load(Stream source)
    {
        ArgumentNullException.ThrowIfNull(source);
        try
        {
            Span<byte> header = stackalloc byte[HeaderSize];
            source.ReadExactly(header);
            var format = ParseHeader(header);

            if (format.Version == 3)
            {
                bool lengthVerified = source.CanSeek;
                if (lengthVerified) CheckV3Length(source.Length - source.Position, format.NodeCount);
                return LoadV3(new RecordSource(source), format, lengthVerified);
            }

            int size = format.NodeCount * RecordSize;
            if (source.CanSeek)
            {
                if (source.Length - source.Position != size) throw new InvalidDataException("NGTR file length does not match its header.");
                var records = new byte[size];
                source.ReadExactly(records);
                return new(records, format);
            }

            // Grow on demand when the declared length cannot be checked against the stream up front.
            using var buffer = new MemoryStream();
            var chunk = new byte[Math.Min(size, 65536)];
            while (buffer.Length < size)
            {
                int count = (int)Math.Min(chunk.Length, size - buffer.Length);
                source.ReadExactly(chunk.AsSpan(0, count));
                buffer.Write(chunk, 0, count);
            }
            if (source.ReadByte() != -1) throw new InvalidDataException("Unexpected data after the NGTR records.");
            return new(buffer.ToArray(), format);
        }
        catch (EndOfStreamException error)
        {
            throw new InvalidDataException("Truncated NGTR file.", error);
        }
    }

    /// <summary>Loads and validates a copy of a complete binary model.</summary>
    public static TrieModel Parse(ReadOnlySpan<byte> data)
    {
        var format = ParseHeader(data);
        var content = data[HeaderSize..];
        if (format.Version == 3)
        {
            CheckV3Length(content.Length, format.NodeCount);
            try { return LoadV3(new RecordSource(content), format, lengthVerified: true); }
            catch (EndOfStreamException error) { throw new InvalidDataException("Truncated NGTR file.", error); }
        }

        if (content.Length != format.NodeCount * RecordSize)
            throw new InvalidDataException("NGTR file length does not match its header.");
        return new(content.ToArray(), format);
    }

    public TrieNode GetNode(int index)
    {
        if ((uint)index >= (uint)NodeCount) throw new ArgumentOutOfRangeException(nameof(index));
        return ReadNode(index);
    }

    public int? FindChild(int nodeIndex, byte label) => FindChild(GetNode(nodeIndex), label);

    private int? FindChild(TrieNode node, byte label)
    {
        int low = node.FirstChild;
        int high = low + node.ChildCount - 1;
        while (low <= high)
        {
            int middle = low + (high - low) / 2;
            byte candidate = records[middle * RecordSize + 10];
            if (candidate == label) return middle;
            if (candidate < label) low = middle + 1;
            else high = middle - 1;
        }
        return null;
    }

    /// <summary>Finds an exact UTF-8 prefix. Pass tokens already normalized as in the corpus.</summary>
    public int? FindPrefix(string prefix, TrieRoot root = TrieRoot.Combined) => FindPrefix(Utf8.GetBytes(prefix), root);

    public int? FindPrefix(ReadOnlySpan<byte> prefix, TrieRoot root = TrieRoot.Combined)
    {
        if (root is not (TrieRoot.Combined or TrieRoot.Unigrams)) throw new ArgumentOutOfRangeException(nameof(root));
        return Follow((int)root, prefix);
    }

    /// <summary>Continues an exact UTF-8 lookup from a node already found in this model.</summary>
    public int? FollowFrom(int nodeIndex, ReadOnlySpan<byte> suffix)
    {
        if ((uint)nodeIndex >= (uint)NodeCount) throw new ArgumentOutOfRangeException(nameof(nodeIndex));
        return Follow(nodeIndex, suffix);
    }

    public IReadOnlyList<TrieTransition> GetTransitions(int nodeIndex)
    {
        var node = GetNode(nodeIndex);
        var transitions = new TrieTransition[node.ChildCount];
        for (int offset = 0; offset < transitions.Length; offset++)
        {
            int index = node.FirstChild + offset;
            var child = ReadNode(index);
            transitions[offset] = new(child.Label, index, child.Mass, (double)child.Mass / node.Mass);
        }
        return transitions;
    }

    public uint GetTerminalCount(int nodeIndex)
    {
        var node = GetNode(nodeIndex);
        uint count = node.Mass;
        for (int offset = 0; offset < node.ChildCount; offset++) count -= ReadNode(node.FirstChild + offset).Mass;
        return count;
    }

    /// <summary>Gets the probability of ending the n-gram here, or null when the node has no mass.</summary>
    public double? GetTerminalProbability(int nodeIndex)
    {
        uint mass = GetNode(nodeIndex).Mass;
        return mass == 0 ? null : (double)GetTerminalCount(nodeIndex) / mass;
    }

    /// <summary>Gets the probability of a whole word from a root or the space after a context.</summary>
    /// <returns>Zero for a missing word, or null when the starting node has no mass.</returns>
    public double? GetWordProbability(int wordStart, string word)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(word);
        if (word.Any(char.IsWhiteSpace)) throw new ArgumentException("Provide one word.", nameof(word));
        var start = GetNode(wordStart);
        if (wordStart > 1 && start.Label != (byte)' ')
            throw new ArgumentException("The word must start at a root or after a space.", nameof(wordStart));
        if (start.Mass == 0) return null;
        if (Follow(wordStart, Utf8.GetBytes(word)) is not int end) return 0;

        uint mass = GetTerminalCount(end);
        if (FindChild(end, (byte)' ') is int space) mass += ReadNode(space).Mass;
        return (double)mass / start.Mass;
    }

    private int? Follow(int start, ReadOnlySpan<byte> prefix)
    {
        foreach (byte label in prefix)
        {
            if (FindChild(ReadNode(start), label) is not int index) return null;
            start = index;
        }
        return start;
    }

    private static Header ParseHeader(ReadOnlySpan<byte> header)
    {
        if (header.Length < HeaderSize || !header[..4].SequenceEqual("NGTR"u8))
            throw new InvalidDataException("Invalid NGTR header.");
        uint version = BinaryPrimitives.ReadUInt32LittleEndian(header[4..]);
        if (version is not (2 or 3))
            throw new InvalidDataException("Only NGTR versions 2 and 3 are supported.");
        uint count = BinaryPrimitives.ReadUInt32LittleEndian(header[8..]);
        uint recordSize = version == 2 ? (uint)RecordSize : 0u;
        if (count < 2 || (long)count * RecordSize > Array.MaxLength ||
            BinaryPrimitives.ReadUInt32LittleEndian(header[12..]) != recordSize)
            throw new InvalidDataException("Invalid NGTR node count or record size.");
        return new(version, (int)count);
    }

    private static void CheckV3Length(long size, int nodeCount)
    {
        // A label and flags take at least two bytes. Flags use at most two bytes
        // (one child per possible nonzero label), and a terminal count uses at most five.
        if (size < 2L * nodeCount || size > 8L * nodeCount)
            throw new InvalidDataException("NGTR file length does not match its header.");
    }

    private static TrieModel LoadV3(RecordSource source, Header header, bool lengthVerified)
    {
        // A header can claim far more nodes than an unverified stream provides.
        int size = header.NodeCount * RecordSize;
        var records = new byte[lengthVerified ? size : Math.Min(size, 256)];
        int nextChild = 2;
        for (int index = 0; index < header.NodeCount; index++)
        {
            byte label = source.ReadRequiredByte();
            uint flags = source.ReadVarUInt32();
            uint childCount = flags >> 1;
            if (index >= nextChild || childCount > byte.MaxValue || childCount > header.NodeCount - nextChild)
                throw new InvalidDataException($"Invalid child range for NGTR node {index}.");
            if (index < 2 ? label != 0 : label == 0)
                throw new InvalidDataException($"Invalid NGTR node {index}.");

            uint terminal = 0;
            if ((flags & 1) != 0)
            {
                terminal = source.ReadVarUInt32();
                if (terminal == 0 || index < 2)
                    throw new InvalidDataException($"Invalid terminal count for NGTR node {index}.");
            }
            if (index >= 2 && childCount == 0 && terminal == 0)
                throw new InvalidDataException($"Invalid NGTR leaf {index}.");

            int recordOffset = index * RecordSize;
            if (recordOffset > records.Length - RecordSize)
                Array.Resize(ref records, records.Length + Math.Min(records.Length, size - records.Length));

            Span<byte> record = records.AsSpan(recordOffset, RecordSize);
            BinaryPrimitives.WriteInt32LittleEndian(record, childCount == 0 ? 0 : nextChild);
            BinaryPrimitives.WriteUInt32LittleEndian(record[4..], terminal);
            BinaryPrimitives.WriteUInt16LittleEndian(record[8..], (ushort)childCount);
            record[10] = label;
            nextChild += (int)childCount;
        }
        if (nextChild != header.NodeCount || source.ReadByte() != -1)
            throw new InvalidDataException("NGTR file length does not match its header.");

        // Child ranges always follow their parent. Reconstruct masses in reverse BFS order.
        for (int index = header.NodeCount - 1; index >= 0; index--)
        {
            var raw = records.AsSpan(index * RecordSize, RecordSize);
            int firstChild = BinaryPrimitives.ReadInt32LittleEndian(raw);
            int childCount = BinaryPrimitives.ReadUInt16LittleEndian(raw[8..]);
            ulong mass = BinaryPrimitives.ReadUInt32LittleEndian(raw[4..]);
            for (int offset = 0; offset < childCount; offset++)
                mass += BinaryPrimitives.ReadUInt32LittleEndian(records.AsSpan((firstChild + offset) * RecordSize + 4, 4));
            if (mass > uint.MaxValue) throw new InvalidDataException($"NGTR mass overflows at node {index}.");
            BinaryPrimitives.WriteUInt32LittleEndian(raw[4..], (uint)mass);
        }
        return new(records, header);
    }

    private TrieNode ReadNode(int index)
    {
        var raw = records.AsSpan(index * RecordSize, RecordSize);
        return new(BinaryPrimitives.ReadUInt32LittleEndian(raw[4..]),
            unchecked((int)BinaryPrimitives.ReadUInt32LittleEndian(raw)),
            BinaryPrimitives.ReadUInt16LittleEndian(raw[8..]), raw[10]);
    }

    private void Validate()
    {
        int nextChild = 2;
        for (int index = 0; index < NodeCount; index++)
        {
            var node = ReadNode(index);
            if (index >= nextChild || records[index * RecordSize + 11] != 0 ||
                (index < 2 ? node.Label != 0 : node.Label == 0 || node.Mass == 0))
                throw new InvalidDataException($"Invalid NGTR node {index}.");
            if (node.ChildCount == 0 ? node.FirstChild != 0 :
                node.FirstChild != nextChild || node.ChildCount > NodeCount - nextChild)
                throw new InvalidDataException($"Invalid child range for NGTR node {index}.");

            uint remainingMass = node.Mass;
            byte previousLabel = 0;
            for (int offset = 0; offset < node.ChildCount; offset++)
            {
                var child = ReadNode(nextChild + offset);
                if (child.Label <= previousLabel || child.Mass > remainingMass)
                    throw new InvalidDataException($"Invalid child order or mass for NGTR node {index}.");
                remainingMass -= child.Mass;
                previousLabel = child.Label;
            }
            if (index < 2 && remainingMass != 0)
                throw new InvalidDataException($"Unexpected terminal mass on NGTR root {index}.");
            nextChild += node.ChildCount;
        }
    }

    private readonly record struct Header(uint Version, int NodeCount);

    private ref struct RecordSource
    {
        private readonly Stream? stream;
        private ReadOnlySpan<byte> remaining;

        public RecordSource(Stream stream)
        {
            this.stream = stream;
            remaining = default;
        }

        public RecordSource(ReadOnlySpan<byte> bytes)
        {
            stream = null;
            remaining = bytes;
        }

        public int ReadByte()
        {
            if (stream is not null) return stream.ReadByte();
            if (remaining.IsEmpty) return -1;
            byte value = remaining[0];
            remaining = remaining[1..];
            return value;
        }

        public byte ReadRequiredByte()
        {
            int value = ReadByte();
            if (value < 0) throw new EndOfStreamException();
            return (byte)value;
        }

        public uint ReadVarUInt32()
        {
            uint value = 0;
            for (int shift = 0; shift <= 28; shift += 7)
            {
                byte next = ReadRequiredByte();
                if (shift == 28 && (next & 0xf0) != 0)
                    throw new InvalidDataException("NGTR variable-length integer exceeds 32 bits.");

                value |= (uint)(next & 0x7f) << shift;
                if ((next & 0x80) != 0) continue;
                if (shift != 0 && next == 0)
                    throw new InvalidDataException("NGTR variable-length integer is not canonical.");
                return value;
            }
            throw new InvalidDataException("Invalid NGTR variable-length integer.");
        }
    }
}
