using System.Runtime.InteropServices;
using System.Text;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal readonly record struct TrieEntry(string Text, uint Count);

internal static class TrieEntries
{
    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);

    public static TrieEntry[][] Read(TrieModel trie, CancellationToken cancellation)
    {
        List<TrieEntry>[] groups = [[], [], []];
        var path = new List<byte>();
        var pending = new Stack<PendingNode>();
        pending.Push(new(trie.GetNode((int)TrieRoot.Combined), PrefixLength: 0, WordCount: 1));
        while (pending.TryPop(out var current))
        {
            cancellation.ThrowIfCancellationRequested();
            // Each sibling restores the path to its parent's length.
            path.RemoveRange(current.PrefixLength, path.Count - current.PrefixLength);
            var node = current.Node;
            if (node.Label != 0) path.Add(node.Label);

            uint count = node.Mass;
            for (int index = node.FirstChild + node.ChildCount - 1; index >= node.FirstChild; index--)
            {
                var child = trie.GetNode(index);
                count -= child.Mass;
                pending.Push(new(child, path.Count, current.WordCount + (child.Label == (byte)' ' ? 1 : 0)));
            }
            if (count == 0) continue;

            var bytes = CollectionsMarshal.AsSpan(path);
            if (current.WordCount > groups.Length || bytes.IsEmpty || bytes[0] == (byte)' ' ||
                bytes[^1] == (byte)' ' || bytes.IndexOf("  "u8) >= 0)
                throw new InvalidDataException("The combined trie contains an invalid n-gram path.");
            try { groups[current.WordCount - 1].Add(new(Utf8.GetString(bytes), count)); }
            catch (DecoderFallbackException error)
            {
                throw new InvalidDataException("The combined trie contains an n-gram with invalid UTF-8.", error);
            }
        }

        var entries = new TrieEntry[groups.Length][];
        for (int index = 0; index < groups.Length; index++)
        {
            cancellation.ThrowIfCancellationRequested();
            var group = groups[index];
            try
            {
                group.Sort((left, right) =>
                {
                    cancellation.ThrowIfCancellationRequested();
                    int byCount = right.Count.CompareTo(left.Count);
                    return byCount != 0 ? byCount : StringComparer.Ordinal.Compare(left.Text, right.Text);
                });
            }
            catch (InvalidOperationException error) when (error.InnerException is OperationCanceledException)
            {
                cancellation.ThrowIfCancellationRequested();
                throw;
            }
            entries[index] = group.ToArray();
        }
        cancellation.ThrowIfCancellationRequested();
        return entries;
    }

    private readonly record struct PendingNode(TrieNode Node, int PrefixLength, int WordCount);
}
