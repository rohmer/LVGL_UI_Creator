namespace NgramTrie;

public sealed record GenerationProgress(string Stage, int Percent);
public sealed record GenerationResult(int Unigrams, int Bigrams, int Trigrams, long? TrieBytes);

public static class Generator
{
    public static GenerationResult Generate(string[] args, Action<GenerationProgress>? report = null,
        CancellationToken cancellation = default)
    {
        var options = Options.Parse(args);
        var progress = new Progress(report, cancellation);
        progress.Begin("Finding corpus files", 0, 0, 1);
        var files = Corpus.FindFiles(options.Inputs, options.SingleFile, options.IncludeSubdirectories);
        if (options.TrieOutput is { } binary && files.Contains(binary, Program.PathComparer) ||
            options.JsonOutput is { } json && files.Contains(json, Program.PathComparer))
            throw new ArgumentException("Output files must be different from the corpus files.");
        if (options.TrieOutput is not null && Program.PathComparer.Equals(options.TrieOutput, options.JsonOutput))
            throw new ArgumentException("JSON and trie outputs must use different files.");
        if (options.WordListPath is { } wordListPath)
        {
            if (Program.PathComparer.Equals(options.TrieOutput, wordListPath) ||
                Program.PathComparer.Equals(options.JsonOutput, wordListPath))
                throw new ArgumentException("Output files must be different from the word list.");
            files.RemoveAll(path => Program.PathComparer.Equals(path, wordListPath));
            if (files.Count == 0) throw new ArgumentException("Provide corpus text in addition to the word list.");
        }

        progress.Begin("Counting n-grams", 0, 70, files.Sum(path => new FileInfo(path).Length));
        var (selected, totals, trie) = options.LowMemory
            ? DiskCounts.Select(files, options, progress)
            : Model.Select(Corpus.Count(files, options, progress), options, progress);
        if (options.TrieOutput is { } triePath)
            Program.AtomicWrite(triePath, stream => trie!.Write(stream, progress), cancellation);
        if (options.JsonOutput is { } jsonPath)
            Program.AtomicWrite(jsonPath, stream => Model.WriteJson(selected, stream, progress), cancellation);
        else if (options.TrieOutput is null)
            Model.WriteJson(selected, Console.OpenStandardOutput(), progress);

        report?.Invoke(new("Complete", 100));
        return new(totals[0], totals[1], totals[2],
            options.TrieOutput is null ? null : trie!.Size);
    }
}

internal sealed class Progress(Action<GenerationProgress>? report, CancellationToken cancellation)
{
    private readonly object gate = new();
    private string stage = "";
    private int start;
    private int end;
    private int previous;
    private long total;

    public CancellationToken Cancellation => cancellation;

    // Phases change only after the previous phase's workers have joined.
    public void Begin(string description, int begin, int finish, long work)
    {
        cancellation.ThrowIfCancellationRequested();
        stage = description;
        start = begin;
        previous = Math.Max(previous, begin);
        end = finish;
        total = work;
        report?.Invoke(new(stage, previous));
    }

    public void Update(long consumed)
    {
        cancellation.ThrowIfCancellationRequested();
        if (report is null || total <= 0) return;
        int percent = start + (int)Math.Clamp((end - start) * ((double)consumed / total), 0, end - start);
        lock (gate)
        {
            if (percent <= previous) return;
            previous = percent;
            report(new(stage, percent));
        }
    }
}
