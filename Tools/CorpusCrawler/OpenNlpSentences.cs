using System.ComponentModel;
using System.Diagnostics;
using System.Text;

namespace CorpusCrawler;

internal sealed partial class OpenNlpSentences : IDisposable
{
    private sealed class WorkerException(string message, Exception? cause = null) : Exception(message, cause);

    private readonly Process? process;
    private readonly Task<string> errors = Task.FromResult("");
    private readonly string? fallbackLanguage;
    private readonly SemaphoreSlim exchange = new(1);

    private OpenNlpSentences(Process process)
    {
        this.process = process;
        process.StandardInput.AutoFlush = true;
        errors = process.StandardError.ReadToEndAsync();
    }
    private OpenNlpSentences(string language) => fallbackLanguage = language;
    public static string Describe(string code) => UnicodeSentenceFallback.Supports(code)
        ? "Unicode sentence boundaries (no OpenNLP/POS model; no grammar classification)"
        : "Apache OpenNLP sentence, tokenizer and POS models";

    public static async Task<OpenNlpSentences> StartAsync(string code, CancellationToken cancellation)
    {
        cancellation.ThrowIfCancellationRequested();
        code = code.Trim().Split('-', 2)[0].ToLowerInvariant();
        if (UnicodeSentenceFallback.Supports(code)) return new OpenNlpSentences(code);
        string directory = Path.Combine(AppContext.BaseDirectory, "OpenNlp");
        string dependencies = Path.Combine(directory, "dependencies");
        if (!Directory.Exists(dependencies) || new[] { "sentdetect", "tokenizer", "pos" }.Any(kind =>
            !Directory.EnumerateFiles(dependencies, $"opennlp-models-{kind}-{code}-*.jar").Any()))
            throw new WorkerException($"OpenNLP models for '{code}' are missing. Run tools/CorpusCrawler/setup-opennlp.ps1, then rebuild the app.");

        string configuration = Path.Combine(directory, "java-home.txt");
        string? configuredHome = File.Exists(configuration) ? File.ReadAllText(configuration).Trim() : null;
        string java = new[] { Environment.GetEnvironmentVariable("JAVA_HOME"), configuredHome }
            .Where(home => !string.IsNullOrWhiteSpace(home))
            .Select(home => Path.Combine(home!, "bin", "java.exe"))
            .FirstOrDefault(File.Exists) ?? "java.exe";
        var info = new ProcessStartInfo(java)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardInputEncoding = new UTF8Encoding(false),
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        info.ArgumentList.Add("-Dfile.encoding=UTF-8");
        info.ArgumentList.Add("--class-path");
        info.ArgumentList.Add(Path.Combine(dependencies, "*"));
        info.ArgumentList.Add(Path.Combine(directory, "SentenceWorker.java"));
        info.ArgumentList.Add(code);

        Process process;
        try { process = Process.Start(info)!; }
        catch (Win32Exception error)
        {
            throw new WorkerException("OpenNLP needs JDK 17 or later. Set JAVA_HOME or add java.exe to PATH.", error);
        }

        var worker = new OpenNlpSentences(process);
        try
        {
            var greeting = await process.StandardOutput.ReadLineAsync(cancellation).AsTask()
                .WaitAsync(TimeSpan.FromSeconds(45), cancellation);
            if (greeting != "READY")
                throw new WorkerException($"OpenNLP could not start. {await worker.errors.WaitAsync(cancellation)}");
            return worker;
        }
        catch
        {
            worker.Dispose();
            throw;
        }
    }

    public async Task<string[]> ExtractAsync(IEnumerable<string> blocks, CancellationToken cancellation)
    {
        if (fallbackLanguage is not null) return UnicodeSentenceFallback.Extract(blocks, cancellation);
        var active = process ?? throw new ObjectDisposedException(nameof(OpenNlpSentences));
        var text = string.Join('\n', blocks);
        if (text.Length == 0) return [];
        string request = Convert.ToBase64String(Encoding.UTF8.GetBytes(text));

        await exchange.WaitAsync(cancellation);
        try
        {
            // Finish each exchange before letting another page use the same pipes.
            await active.StandardInput.WriteLineAsync(request);
            var response = await active.StandardOutput.ReadLineAsync();
            if (response is null) throw new WorkerException($"OpenNLP stopped unexpectedly. {await errors}");
            cancellation.ThrowIfCancellationRequested();
            return Encoding.UTF8.GetString(Convert.FromBase64String(response))
                .Split('\n', StringSplitOptions.RemoveEmptyEntries);
        }
        catch (Exception error) when (error is IOException or FormatException)
        {
            throw new WorkerException("Could not read sentences from the OpenNLP worker.", error);
        }
        finally { exchange.Release(); }
    }

    public void Dispose()
    {
        if (process is not null && !process.HasExited)
        {
            try { process.Kill(); }
            catch (InvalidOperationException) { }
        }
        process?.Dispose();
        exchange.Dispose();
    }
}
