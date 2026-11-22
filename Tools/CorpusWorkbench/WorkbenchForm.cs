using System.Text.Json;

namespace CorpusWorkbench;

internal sealed class WorkbenchForm : Form
{
    private sealed record ToolPage(Form Form, Func<bool> IsRunning, Action CancelWork);

    private readonly TreeView navigation = new()
    {
        Dock = DockStyle.Fill, BorderStyle = BorderStyle.None,
        BackColor = Color.FromArgb(30, 40, 56), ForeColor = Color.FromArgb(228, 234, 244),
        ShowLines = false, ShowRootLines = false, ShowPlusMinus = false, FullRowSelect = true,
        HideSelection = false, ItemHeight = 36, Indent = 16, AccessibleName = "Tools",
    };
    private readonly Panel content = new() { Dock = DockStyle.Fill, AutoScroll = true, Margin = new(0) };
    private readonly Label title = new() { AutoSize = true, UseMnemonic = false, Anchor = AnchorStyles.Left, Font = new("Segoe UI", 14, FontStyle.Bold) };
    private readonly FlowLayoutPanel actions = new() { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
    private readonly Button useCorpus = new() { Text = "Use latest corpus", AutoSize = true };
    private readonly Button useTrie = new() { Text = "Use latest trie", AutoSize = true };
    private readonly Button useFuzzed = new() { Text = "Use latest fuzzed file", AutoSize = true };
    private readonly ToolStripStatusLabel status = new() { Spring = true, TextAlign = ContentAlignment.MiddleLeft, Text = "Ready" };
    private readonly ToolTip hints = new();
    private readonly System.Windows.Forms.Timer monitor = new() { Interval = 250 };
    private readonly Dictionary<string, ToolPage> pages = new(StringComparer.Ordinal);
    private readonly Dictionary<string, TreeNode> nodes = new(StringComparer.Ordinal);
    private readonly WorkspaceSettingsStore settings;
    private readonly WorkspaceSettings workspace;
    private string selected = "";
    private string lastStatus = "Ready";
    private bool closing;

    public WorkbenchForm() : this(new WorkspaceSettingsStore()) { }

    internal WorkbenchForm(WorkspaceSettingsStore settings)
    {
        this.settings = settings;
        workspace = LoadWorkspace();
        Text = "ESPOS Corpus Workbench";
        ClientSize = new(1360, 900);
        MinimumSize = new(1100, 740);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Font;
        if (Screen.PrimaryScreen is { } display)
            Size = new(Math.Min(Width, display.WorkingArea.Width), Math.Min(Height, display.WorkingArea.Height));

        var window = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 2, Margin = new(0) };
        window.ColumnStyles.Add(new(SizeType.Absolute, 208));
        window.ColumnStyles.Add(new(SizeType.Percent, 100));
        window.RowStyles.Add(new(SizeType.Percent, 100));
        window.RowStyles.Add(new(SizeType.AutoSize));
        Controls.Add(window);

        var sidebar = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, RowCount = 2, ColumnCount = 1, Margin = new(0), Padding = new(12, 20, 8, 12),
            BackColor = navigation.BackColor,
        };
        sidebar.RowStyles.Add(new(SizeType.AutoSize));
        sidebar.RowStyles.Add(new(SizeType.Percent, 100));
        sidebar.Controls.Add(new Label
        {
            Text = "Corpus Workbench", AutoSize = true, ForeColor = Color.White,
            Font = new("Segoe UI", 12, FontStyle.Bold), Margin = new(5, 0, 0, 22),
        }, 0, 0);
        sidebar.Controls.Add(navigation, 0, 1);
        window.Controls.Add(sidebar, 0, 0);

        var body = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2, ColumnCount = 1, Margin = new(0) };
        body.RowStyles.Add(new(SizeType.AutoSize));
        body.RowStyles.Add(new(SizeType.Percent, 100));
        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 1, RowCount = 2, Padding = new(20, 14, 20, 8), Margin = new(0),
        };
        header.RowStyles.Add(new(SizeType.AutoSize));
        header.RowStyles.Add(new(SizeType.AutoSize));
        header.Controls.Add(title, 0, 0);
        actions.Margin = new(0, 8, 0, 0);
        actions.Controls.AddRange([useCorpus, useTrie, useFuzzed]);
        header.Controls.Add(actions, 0, 1);
        body.Controls.Add(header, 0, 0);
        body.Controls.Add(content, 0, 1);
        window.Controls.Add(body, 1, 0);

        var statusBar = new StatusStrip { SizingGrip = false, Dock = DockStyle.Fill };
        statusBar.Items.Add(status);
        window.Controls.Add(statusBar, 0, 1);
        window.SetColumnSpan(statusBar, 2);

        AddGroup("Collect corpora", ("web", "Web crawler"), ("wikipedia", "Wikipedia XML"));
        AddGroup("Build n-grams", ("generator", "Trie generator"));
        AddGroup("Test corrections", ("fuzzer", "Sentence fuzzer"), ("tester", "GBU test & regression"), ("keyboard", "Keyboard simulator"));
        navigation.ExpandAll();
        navigation.AfterSelect += (_, e) =>
        {
            if (e.Node?.Tag is string key) ShowTool(key);
        };
        navigation.BeforeCollapse += (_, e) => e.Cancel = true;
        useCorpus.Click += (_, _) => UseLatestCorpus();
        useTrie.Click += async (_, _) => await UseLatestTrieAsync();
        useFuzzed.Click += (_, _) => UseLatestFuzzedFile();
        monitor.Tick += (_, _) => MonitorJobs();
        FormClosing += CloseWorkbench;
        FormClosed += (_, _) => { monitor.Dispose(); hints.Dispose(); };
        navigation.SelectedNode = nodes.GetValueOrDefault(workspace.SelectedTool ?? "") ?? nodes["web"];
        monitor.Start();
    }

    private void AddGroup(string name, params (string Key, string Label)[] tools)
    {
        var group = navigation.Nodes.Add(name);
        group.NodeFont = new Font(navigation.Font, FontStyle.Bold);
        foreach (var tool in tools)
        {
            var node = group.Nodes.Add(tool.Label);
            node.Tag = tool.Key;
            nodes.Add(tool.Key, node);
        }
    }

    private void ShowTool(string key)
    {
        if (selected == key) return;
        if (!pages.TryGetValue(key, out var page))
        {
            page = CreateTool(key);
            page.Form.TopLevel = false;
            page.Form.FormBorderStyle = FormBorderStyle.None;
            page.Form.Dock = DockStyle.Fill;
            pages.Add(key, page);
            content.Controls.Add(page.Form);
        }
        content.SuspendLayout();
        if (pages.TryGetValue(selected, out var previous)) previous.Form.Hide();
        content.AutoScrollPosition = Point.Empty;
        content.AutoScrollMinSize = new(Math.Max(960, page.Form.MinimumSize.Width), Math.Max(580, page.Form.MinimumSize.Height));
        selected = key;
        title.Text = nodes[key].Text;
        page.Form.Show();
        page.Form.BringToFront();
        content.ResumeLayout(performLayout: true);
        workspace.SelectedTool = key;
        UpdateActions();
    }

    private ToolPage CreateTool(string key)
    {
        switch (key)
        {
            case "web":
                var crawler = new CorpusCrawler.CrawlerForm();
                crawler.CorpusCreated += CorpusCreated;
                return new(crawler, () => crawler.IsRunning, crawler.CancelWork);
            case "wikipedia":
                var wikipedia = new WikipediaCorpus.ExportForm();
                wikipedia.CorpusCreated += CorpusCreated;
                return new(wikipedia, () => wikipedia.IsRunning, wikipedia.CancelWork);
            case "generator":
                var generator = new NgramTrie.UI.GeneratorForm();
                generator.TrieCreated += (path, language) =>
                {
                    workspace.Trie = new(path, language);
                    ArtifactCreated($"Trie ready · {path}");
                };
                return new(generator, () => generator.IsRunning, generator.CancelWork);
            case "fuzzer":
                var fuzzer = new GBUTest.Fuzzer.FuzzerForm();
                if (File.Exists(workspace.Corpus?.Path)) fuzzer.UseCorpus(workspace.Corpus.Path);
                if (File.Exists(workspace.Trie?.Path)) fuzzer.UseTrie(workspace.Trie.Path);
                fuzzer.FuzzedFileCreated += path =>
                {
                    workspace.FuzzedFile = path;
                    ArtifactCreated($"Fuzzed file ready · {path}");
                };
                return new(fuzzer, () => fuzzer.IsRunning, fuzzer.CancelWork);
            case "tester":
                var tester = new GBUTest.UI.TypingTesterForm();
                if (File.Exists(workspace.FuzzedFile)) tester.UseFuzzedFile(workspace.FuzzedFile);
                if (File.Exists(workspace.Trie?.Path)) tester.UseTrie(workspace.Trie.Path);
                return new(tester, () => tester.IsRunning, tester.CancelWork);
            case "keyboard":
                var keyboard = new KeyboardSimulator.SimulatorForm(Path.Combine(AppContext.BaseDirectory, "Keyboards"));
                return new(keyboard, () => keyboard.IsRunning, keyboard.CancelWork);
            default:
                throw new ArgumentException($"Unknown tool {key}.");
        }
    }

    private void CorpusCreated(string path, string language)
    {
        workspace.Corpus = new(path, language);
        workspace.Corpora.RemoveAll(corpus => string.Equals(corpus.Path, path, StringComparison.OrdinalIgnoreCase));
        workspace.Corpora.Add(workspace.Corpus);
        ArtifactCreated($"Corpus ready · {path}");
    }

    private void ArtifactCreated(string message)
    {
        lastStatus = message;
        SaveWorkspace();
        MonitorJobs();
    }

    private void UseLatestCorpus()
    {
        if (workspace.Corpus is not { } corpus || !File.Exists(corpus.Path)) return;
        switch (pages[selected].Form)
        {
            case NgramTrie.UI.GeneratorForm generator:
                var files = workspace.Corpora.Where(file => file.Language == corpus.Language && File.Exists(file.Path))
                    .Select(file => file.Path).Append(corpus.Path).Distinct(StringComparer.OrdinalIgnoreCase);
                if (!generator.UseCorpora(files, corpus.Language))
                {
                    lastStatus = $"The trie generator does not support corpus language '{corpus.Language}'.";
                    MonitorJobs();
                    return;
                }
                break;
            case GBUTest.Fuzzer.FuzzerForm fuzzer:
                fuzzer.UseCorpus(corpus.Path);
                break;
        }
        lastStatus = $"Selected corpus · {corpus.Path}";
        MonitorJobs();
    }

    private async Task UseLatestTrieAsync()
    {
        if (workspace.Trie is not { } trie || !File.Exists(trie.Path)) return;
        switch (pages[selected].Form)
        {
            case GBUTest.Fuzzer.FuzzerForm fuzzer:
                fuzzer.UseTrie(trie.Path);
                break;
            case GBUTest.UI.TypingTesterForm tester:
                tester.UseTrie(trie.Path);
                break;
            case KeyboardSimulator.SimulatorForm keyboard:
                keyboard.UseLanguage(trie.Language);
                await keyboard.UseTrieAsync(trie.Path);
                break;
        }
        lastStatus = $"Selected trie · {trie.Path}";
        MonitorJobs();
    }

    private void UseLatestFuzzedFile()
    {
        if (File.Exists(workspace.FuzzedFile) && pages[selected].Form is GBUTest.UI.TypingTesterForm tester)
            tester.UseFuzzedFile(workspace.FuzzedFile!);
    }

    private void UpdateActions()
    {
        actions.Visible = selected is "generator" or "fuzzer" or "tester" or "keyboard";
        useCorpus.Visible = selected is "generator" or "fuzzer";
        useCorpus.Text = selected == "generator" ? "Use collected corpora" : "Use latest corpus";
        useTrie.Visible = selected is "fuzzer" or "tester" or "keyboard";
        useFuzzed.Visible = selected == "tester";
        var idle = !closing && pages.TryGetValue(selected, out var page) && !page.IsRunning();
        useCorpus.Enabled = idle && File.Exists(workspace.Corpus?.Path);
        useTrie.Enabled = idle && File.Exists(workspace.Trie?.Path);
        useFuzzed.Enabled = idle && File.Exists(workspace.FuzzedFile);
        hints.SetToolTip(useCorpus, selected == "generator" && workspace.Corpus is { } corpus
            ? $"Collected corpus files in language '{corpus.Language}'. Latest file: {corpus.Path}"
            : workspace.Corpus?.Path ?? "Collect a corpus with the crawler or Wikipedia exporter.");
        hints.SetToolTip(useTrie, workspace.Trie?.Path ?? "Generate a trie first.");
        hints.SetToolTip(useFuzzed, workspace.FuzzedFile ?? "Generate sentence pairs with the fuzzer first.");
    }

    private void MonitorJobs()
    {
        var running = pages.Where(entry => entry.Value.IsRunning()).Select(entry => nodes[entry.Key].Text).ToArray();
        UpdateActions();
        status.Text = closing ? "Stopping active jobs and saving progress…" : running.Length == 0
            ? lastStatus : $"Running · {string.Join(", ", running)}";
        if (closing && running.Length == 0) Close();
    }

    private void CloseWorkbench(object? sender, FormClosingEventArgs e)
    {
        var running = pages.Values.Where(page => page.IsRunning()).ToArray();
        if (running.Length != 0)
        {
            e.Cancel = true;
            closing = true;
            navigation.Enabled = false;
            foreach (var page in pages.Values) page.Form.Enabled = false;
            foreach (var page in running) page.CancelWork();
            status.Text = "Stopping active jobs and saving progress…";
            UpdateActions();
            return;
        }
        monitor.Stop();
        SaveWorkspace();
        foreach (var page in pages.Values) page.Form.Close();
    }

    private WorkspaceSettings LoadWorkspace()
    {
        try
        {
            var saved = settings.Load();
            saved.Corpora ??= [];
            return saved;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        {
            lastStatus = $"Could not restore workspace · {error.Message}";
            return new();
        }
    }

    private void SaveWorkspace()
    {
        try { settings.Save(workspace); }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            lastStatus = $"Could not save workspace · {error.Message}";
        }
    }
}
