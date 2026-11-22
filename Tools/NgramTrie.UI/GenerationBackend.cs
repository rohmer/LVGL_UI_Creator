using System.Diagnostics;
using System.Globalization;
using System.Text;

namespace NgramTrie.UI;

internal static class GenerationBackend
{
    public static string? FindNativeHelper()
    {
        string name = OperatingSystem.IsWindows() ? "UICreatorModelGen.exe" : "UICreatorModelGen";
        if (Environment.GetEnvironmentVariable("UICREATOR_MODELGEN") is { Length: > 0 } configured)
            return File.Exists(configured) ? Path.GetFullPath(configured) : null;
        string adjacent = Path.Combine(AppContext.BaseDirectory, name);
        if (File.Exists(adjacent)) return adjacent;
        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
        {
            string candidate = Path.Combine(directory.FullName, "tools", "NativeTrieGenerator", "bin", name);
            if (File.Exists(candidate)) return candidate;
        }
        return null;
    }
    public static string Describe() => FindNativeHelper() is null
        ? "Managed C# fallback — native helper is not available"
        : "Native generator — optimized C++ counting and local spill storage";

    public static async Task<GenerationResult> GenerateAsync(string[] arguments, Action<GenerationProgress>? report,
        CancellationToken cancellation)
    {
        string? helper = FindNativeHelper();
        if (helper is null) return await Task.Run(() => Generator.Generate(arguments, report, cancellation));
        cancellation.ThrowIfCancellationRequested();
        var start = new ProcessStartInfo(helper)
        {
            UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
            WorkingDirectory = Path.GetDirectoryName(helper)!,
        };
        foreach (string argument in arguments) start.ArgumentList.Add(argument);
        using var process = Process.Start(start) ?? throw new IOException("The native model generator could not start.");
        var errors = process.StandardError.ReadToEndAsync();
        using var registration = cancellation.Register(() =>
        {
            try { if (!process.HasExited) { process.StandardInput.WriteLine("cancel"); process.StandardInput.Flush(); } }
            catch (Exception error) when (error is IOException or InvalidOperationException) { }
        });
        GenerationResult? result = null;
        while (await process.StandardOutput.ReadLineAsync() is { } line)
        {
            var fields = line.Split('\t');
            if (fields is ["@progress", var percent, var stage] && int.TryParse(percent, NumberStyles.None, CultureInfo.InvariantCulture, out int number))
                report?.Invoke(new(stage, Math.Clamp(number, 0, 100)));
            else if (fields is ["@result", var unigrams, var bigrams, var trigrams, var bytes])
                result = new(int.Parse(unigrams, CultureInfo.InvariantCulture), int.Parse(bigrams, CultureInfo.InvariantCulture),
                    int.Parse(trigrams, CultureInfo.InvariantCulture), long.Parse(bytes, CultureInfo.InvariantCulture));
        }
        await process.WaitForExitAsync();
        string errorText = (await errors).Trim();
        // A successful publication wins a late Stop, just as the managed port.
        if (process.ExitCode == 0 && result is not null) return result;
        if (cancellation.IsCancellationRequested) throw new OperationCanceledException(cancellation);
        throw new IOException(errorText.Length == 0 ? $"Native generation failed (exit {process.ExitCode})." : errorText);
    }
}
