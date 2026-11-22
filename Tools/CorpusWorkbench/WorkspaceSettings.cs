using System.Text.Json;

namespace CorpusWorkbench;

internal sealed record GeneratedFile(string Path, string Language);

internal sealed class WorkspaceSettings
{
    public string SelectedTool { get; set; } = "web";
    public GeneratedFile? Corpus { get; set; }
    public List<GeneratedFile> Corpora { get; set; } = [];
    public GeneratedFile? Trie { get; set; }
    public string? FuzzedFile { get; set; }
}

internal sealed class WorkspaceSettingsStore
{
    private readonly string path;

    internal WorkspaceSettingsStore(string? path = null)
    {
        this.path = Path.GetFullPath(path ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "espos-corpus-workbench", "workspace.json"));
    }

    internal WorkspaceSettings Load()
    {
        if (!File.Exists(path)) return new();
        using var stream = File.OpenRead(path);
        return JsonSerializer.Deserialize<WorkspaceSettings>(stream) ?? new();
    }

    internal void Save(WorkspaceSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temporary = path + $".{Guid.NewGuid():N}.tmp";
        try
        {
            using (var stream = File.Create(temporary)) JsonSerializer.Serialize(stream, settings);
            File.Move(temporary, path, overwrite: true);
        }
        finally { File.Delete(temporary); }
    }
}
