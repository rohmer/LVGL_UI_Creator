using System.Text.Json;

namespace NgramTrie.UI;

internal sealed record UiSettings(
    string[] Directories, string Output, string MaximumBytes, string Unigrams, string Bigrams, string Trigrams,
    int Workers, bool Cache, bool LowMemory, bool Compact = false, string Language = "en",
    bool IncludeSubdirectories = true);

internal sealed class UiSettingsStore(string? path = null)
{
    private readonly string file = path ?? Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "espos-ngram-dotnet", "ui-settings.json");

    public UiSettings? Load()
    {
        if (!File.Exists(file)) return null;
        using var stream = File.OpenRead(file);
        return JsonSerializer.Deserialize<UiSettings>(stream);
    }

    public void Save(UiSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(file)!);
        string temporary = file + $".{Guid.NewGuid():N}.tmp";
        try
        {
            using (var stream = File.Create(temporary)) JsonSerializer.Serialize(stream, settings);
            File.Move(temporary, file, overwrite: true);
        }
        finally { File.Delete(temporary); }
    }
}
