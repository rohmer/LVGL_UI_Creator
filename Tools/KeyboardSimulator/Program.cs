using System.Text.Json;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new SimulatorForm(Path.Combine(AppContext.BaseDirectory, "Keyboards")));
    }
}

internal sealed class SimulatorForm : Form
{
    private static readonly Color Workspace = Color.FromArgb(23, 30, 42);
    private static readonly Color Toolbar = Color.FromArgb(34, 44, 59);
    private static readonly Color Caption = Color.FromArgb(214, 225, 240);

    private readonly ComboBox orientation = NewSelector();
    private readonly ComboBox keyboard = NewSelector();
    private readonly ComboBox resolution = NewSelector();
    private readonly CheckBox rtl = new() { Text = "RTL", ForeColor = Caption, AutoSize = true, AccessibleName = "Right-to-left display and typing" };
    private readonly NumericUpDown autocorrectThreshold = new()
    {
        Minimum = 0, Maximum = 1, Value = KeyboardPreview.DefaultAutocorrectThreshold,
        DecimalPlaces = 3, Increment = 0.01m, Dock = DockStyle.Fill, Margin = new(0, 0, 14, 0),
    };
    private readonly Button loadTrie = new() { Text = "Load trie…", AutoSize = true, Margin = new Padding(0, 0, 6, 0) };
    private readonly Button viewTrie = new() { Text = "View trie…", AutoSize = true, Margin = new(0), Enabled = false };
    private readonly Label status = new() { Dock = DockStyle.Fill, ForeColor = Caption, AutoEllipsis = true };
    private readonly KeyboardPreview preview = new() { Dock = DockStyle.Fill, BackColor = Workspace };
    private readonly List<KeyboardDefinition> definitions = [];
    private string? profileWarning;
    private string? loadedTrieName;
    private TrieModel? loadedTrie;
    private TrieViewerForm? trieViewer;
    private string trieStatus = "No trie loaded";
    private CancellationTokenSource? loading;

    internal bool IsRunning => loading is not null;

    internal Task UseTrieAsync(string path) => LoadTrieFileAsync(path);

    internal void UseLanguage(string languageCode)
    {
        if (IsRunning) return;
        int index = definitions.FindIndex(definition => definition.Id.StartsWith(languageCode + "-", StringComparison.OrdinalIgnoreCase));
        if (index >= 0) keyboard.SelectedIndex = index;
    }

    internal void CancelWork() => loading?.Cancel();

    public SimulatorForm(string definitionDirectory)
    {
        Text = "Keyboard Simulator";
        ClientSize = new Size(1140, 785);
        MinimumSize = new Size(700, 460);
        StartPosition = FormStartPosition.CenterScreen;
        if (Screen.PrimaryScreen is { } display)
            Size = new Size(Math.Min(Width, display.WorkingArea.Width), Math.Min(Height, display.WorkingArea.Height));
        AutoScaleMode = AutoScaleMode.Font;
        BackColor = Workspace;

        var window = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3 };
        window.RowStyles.Add(new(SizeType.Absolute, 86));
        window.RowStyles.Add(new(SizeType.Percent, 100));
        window.RowStyles.Add(new(SizeType.Absolute, 38));
        Controls.Add(window);

        var toolbar = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, BackColor = Toolbar, Padding = new(22, 12, 22, 10),
            Margin = new(0), ColumnCount = 5, RowCount = 2,
        };
        toolbar.ColumnStyles.Add(new(SizeType.Percent, 28));
        toolbar.ColumnStyles.Add(new(SizeType.Percent, 44));
        toolbar.ColumnStyles.Add(new(SizeType.Percent, 28));
        toolbar.ColumnStyles.Add(new(SizeType.Absolute, 134));
        toolbar.ColumnStyles.Add(new(SizeType.AutoSize));
        toolbar.RowStyles.Add(new(SizeType.Absolute, 24));
        toolbar.RowStyles.Add(new(SizeType.Percent, 100));
        AddSelector("Orientation", orientation, 0);
        AddSelector("Keyboard", keyboard, 1);
        AddSelector("Panel resolution", resolution, 2);
        AddSelector("Autocorrect threshold", autocorrectThreshold, 3);
        autocorrectThreshold.ValueChanged += (_, _) => preview.AutocorrectThreshold = autocorrectThreshold.Value;
        var trieActions = new FlowLayoutPanel
        {
            AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
            WrapContents = false, Anchor = AnchorStyles.Top, Margin = new(0),
        };
        trieActions.Controls.AddRange([rtl, loadTrie, viewTrie]);
        rtl.CheckedChanged += (_, _) => preview.RightToLeftTyping = rtl.Checked;
        toolbar.Controls.Add(trieActions, 4, 1);
        loadTrie.Click += async (_, _) =>
        {
            using var dialog = new OpenFileDialog { Filter = "N-gram trie (*.ngtr)|*.ngtr", Title = "Load an n-gram trie" };
            if (dialog.ShowDialog(this) == DialogResult.OK) await LoadTrieFileAsync(dialog.FileName);
        };
        viewTrie.Click += (_, _) => OpenTrieViewer();
        window.Controls.Add(toolbar, 0, 0);
        window.Controls.Add(preview, 0, 1);
        status.Padding = new Padding(24, 0, 16, 0);
        window.Controls.Add(status, 0, 2);

        orientation.Items.AddRange(["Landscape", "Portrait"]);
        resolution.Items.Add(new PanelResolution(1024, 600));
        LoadDefinitions(definitionDirectory);

        orientation.SelectedIndex = 0;
        resolution.SelectedIndex = 0;
        orientation.SelectedIndexChanged += (_, _) => UpdateResolution();
        resolution.SelectedIndexChanged += (_, _) => UpdateResolution();
        keyboard.SelectedIndexChanged += (_, _) =>
        {
            if (keyboard.SelectedIndex >= 0) {
                preview.SetKeyboard(definitions[keyboard.SelectedIndex]);
                rtl.Checked = preview.RightToLeftTyping;
            }
        };
        int initial = definitions.FindIndex(definition => definition.Id == "en-US-qwerty");
        if (definitions.Count > 0) keyboard.SelectedIndex = Math.Max(initial, 0);
        else keyboard.Enabled = false;
        UpdateResolution();
        FormClosed += (_, _) =>
        {
            CancelWork();
            trieViewer?.Close();
        };

        void AddSelector(string title, Control field, int column)
        {
            field.AccessibleName = title;
            toolbar.Controls.Add(new Label
            {
                Text = title, ForeColor = Caption, AutoSize = true, Anchor = AnchorStyles.Left, Margin = new(0, 0, 0, 2),
            }, column, 0);
            toolbar.Controls.Add(field, column, 1);
        }
    }

    private static ComboBox NewSelector() => new()
    {
        DropDownStyle = ComboBoxStyle.DropDownList, Dock = DockStyle.Fill, Margin = new(0, 0, 14, 0),
    };

    private void LoadDefinitions(string directory)
    {
        try
        {
            var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string path in Directory.EnumerateFiles(directory, "*.json").Order(StringComparer.OrdinalIgnoreCase))
            {
                if (path.EndsWith(".schema.json", StringComparison.OrdinalIgnoreCase)) continue;
                try
                {
                    var definition = KeyboardDefinition.Load(path);
                    if (!ids.Add(definition.Id)) throw new InvalidDataException($"Duplicate ID {definition.Id}.");
                    definitions.Add(definition);
                    keyboard.Items.Add(definition.Id);
                }
                catch (Exception error) when (error is IOException or InvalidDataException or JsonException or UnauthorizedAccessException)
                {
                    profileWarning ??= $"{Path.GetFileName(path)} — {error.Message}";
                }
            }
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            profileWarning = error.Message;
        }
        if (definitions.Count == 0) profileWarning ??= "No keyboard definitions were found.";
    }

    private void UpdateResolution()
    {
        if (resolution.SelectedItem is not PanelResolution panel) return;
        var size = orientation.SelectedIndex == 1 ? new Size(panel.Height, panel.Width) : new Size(panel.Width, panel.Height);
        preview.DisplaySize = size;
        UpdateStatus();
    }

    private void UpdateStatus()
    {
        var size = preview.DisplaySize;
        status.Text = $"{size.Width} × {size.Height} px   ·   {trieStatus}" +
                      (profileWarning is null ? "" : $"   ·   {profileWarning}");
    }

    private async Task LoadTrieFileAsync(string path)
    {
        if (IsRunning) return;
        using var cancellation = new CancellationTokenSource();
        loading = cancellation;
        loadTrie.Enabled = false;
        string filename = Path.GetFileName(path);
        string previousStatus = loadedTrieName is null ? "No trie loaded" : $"Trie: {loadedTrieName}";
        trieStatus = $"Loading {filename}…";
        UpdateStatus();
        try
        {
            var trie = await Task.Run(() => TrieModel.Load(path));
            if (IsDisposed || cancellation.IsCancellationRequested) return;
            preview.SetTrie(trie);
            loadedTrie = trie;
            loadedTrieName = filename;
            trieStatus = $"Trie: {filename}";
            viewTrie.Enabled = true;
            if (trieViewer is { IsDisposed: false } viewer) _ = viewer.SetTrieAsync(trie, filename);
        }
        catch (Exception error) when (error is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException)
        {
            if (!IsDisposed) trieStatus = $"Could not load {filename}: {error.Message}   ·   {previousStatus}";
        }
        finally
        {
            loading = null;
            if (!IsDisposed)
            {
                if (cancellation.IsCancellationRequested) trieStatus = previousStatus;
                loadTrie.Enabled = true;
                UpdateStatus();
            }
        }
    }

    private void OpenTrieViewer()
    {
        if (loadedTrie is not { } trie) return;
        if (trieViewer is { IsDisposed: false } existing)
        {
            if (existing.WindowState == FormWindowState.Minimized) existing.WindowState = FormWindowState.Normal;
            existing.Activate();
            return;
        }

        var viewer = new TrieViewerForm();
        trieViewer = viewer;
        viewer.FormClosed += (_, _) => trieViewer = null;
        viewer.Show(TopLevelControl as Form ?? this);
        _ = viewer.SetTrieAsync(trie, loadedTrieName!);
    }

    private sealed record PanelResolution(int Width, int Height)
    {
        public override string ToString() => $"{Width} × {Height}";
    }
}
