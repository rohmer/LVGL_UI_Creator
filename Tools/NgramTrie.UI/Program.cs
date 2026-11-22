using System.Diagnostics;
using System.Globalization;
using System.Text.Json;

namespace NgramTrie.UI;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new GeneratorForm());
    }
}

internal sealed class GeneratorForm : Form
{
    private sealed record LanguageOption(string Code, string Label)
    {
        public override string ToString() => Label;
    }

    private static readonly LanguageOption[] Languages =
        [new("en", "English"), new("es", "Spanish"), new("fr", "French"), new("de", "German"), new("el", "Greek"),
         new("ar", "Arabic"), new("hi", "Hindi"), new("zh", "Mandarin Chinese"), new("bn", "Bengali"),
         new("id", "Indonesian"), new("ur", "Urdu"), new("ru", "Russian")];
    private readonly TextBox directories = new()
    {
        Multiline = true, AcceptsReturn = true, WordWrap = false, ScrollBars = ScrollBars.Vertical,
        MinimumSize = new Size(0, 140), Dock = DockStyle.Fill, PlaceholderText = @"D:\Corpus\foxnews",
    };
    private readonly CheckBox includeSubdirectories = new() { Checked = true, Text = "Include subdirectories", AutoSize = true };
    private readonly TextBox output = new() { Dock = DockStyle.Fill };
    private readonly ComboBox language = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 150 };
    private readonly TextBox bytes = new() { Text = "262144", PlaceholderText = "Unlimited" };
    private readonly TextBox unigrams = new() { PlaceholderText = "Automatic" };
    private readonly TextBox bigrams = new() { PlaceholderText = "Automatic" };
    private readonly TextBox trigrams = new() { PlaceholderText = "Automatic" };
    private readonly NumericUpDown workers = new() { Minimum = 1, Maximum = 1024, Value = Math.Min(Environment.ProcessorCount, 1024) };
    private readonly CheckBox useCache = new() { Checked = true, Text = "Reuse saved counts", AutoSize = true };
    private readonly CheckBox lowMemory = new() { Checked = true, Text = "Keep counts on disk", AutoSize = true };
    private readonly CheckBox compact = new() { Text = "Compact trie (NGTR v3)", AutoSize = true };
    private readonly Button add = new() { Text = "Add folder…", AutoSize = true };
    private readonly Button browse = new() { Text = "Browse…", AutoSize = true };
    private readonly Button generate = new() { Text = "Generate trie", AutoSize = true };
    private readonly Button stop = new() { Text = "Stop", AutoSize = true, Enabled = false };
    private readonly ProgressBar bar = new() { Dock = DockStyle.Fill };
    private readonly Label timing = new() { Dock = DockStyle.Fill, AutoSize = true, Text = "Ready" };
    private readonly Label backend = new() { Dock = DockStyle.Fill, AutoSize = true };
    private readonly TextBox status = new()
    {
        Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical,
        BorderStyle = BorderStyle.None, BackColor = SystemColors.Control, TabStop = false,
    };
    private readonly Stopwatch clock = new();
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 1000 };
    private GenerationProgress progress = new("Ready", 0);
    private CancellationTokenSource? current;
    private readonly UiSettingsStore settings;

    internal bool IsRunning => current is not null;
    internal event Action<string, string>? TrieCreated;

    internal void CancelWork() => current?.Cancel();

    internal bool UseCorpora(IEnumerable<string> files, string languageCode)
    {
        var selectedLanguage = Languages.FirstOrDefault(option => option.Code == languageCode);
        if (IsRunning || selectedLanguage is null) return false;
        directories.Lines = files.Select(Path.GetFullPath)
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        language.SelectedItem = selectedLanguage;
        return true;
    }

    public GeneratorForm() : this(new UiSettingsStore()) { }

    internal GeneratorForm(UiSettingsStore settings)
    {
        this.settings = settings;
        Text = "NGramTrie Generator";
        ClientSize = new Size(850, 570);
        MinimumSize = new Size(630, 520);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Font;
        output.Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "model.ngtr");
        language.Items.AddRange(Languages);
        language.SelectedItem = Languages[0];

        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new(20), ColumnCount = 3 };
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        layout.ColumnStyles.Add(new(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        Controls.Add(layout);

        AddRow("Corpus files or folders (one per line)", directories, 0, add);
        AddRow("Corpus search", includeSubdirectories, 1);
        AddRow("Trie output file", output, 2, browse);
        var languageNotice = new Label { AutoSize = true, MaximumSize = new Size(420, 0), Margin = new Padding(3, 6, 3, 0) };
        var languageControls = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
        languageControls.Controls.AddRange([language, languageNotice]);
        void DescribeLanguage() => languageNotice.Text = SelectedLanguage is "ar" or "hi" or "zh" or "bn" or "id" or "ur" or "ru"
            ? SelectedLanguage == "zh" ? "NFC Han character n-grams; no bundled vocabulary filter." : "NFC Unicode words and combining marks; no bundled vocabulary filter."
            : "Uses this language's bundled vocabulary filter.";
        language.SelectedIndexChanged += (_, _) => DescribeLanguage(); DescribeLanguage();
        AddRow("Corpus language", languageControls, 3);
        AddRow("Worker threads", workers, 4);
        AddRow("Maximum trie bytes", bytes, 5);

        var limits = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill };
        limits.Controls.AddRange([new Label { Text = "Unigrams", AutoSize = true }, unigrams,
            new Label { Text = "Bigrams", AutoSize = true }, bigrams, new Label { Text = "Trigrams", AutoSize = true }, trigrams]);
        AddRow("N-gram counts", limits, 6);
        AddRow("Cache", useCache, 7);
        AddRow("Low-memory mode", lowMemory, 8);
        AddRow("Binary format", compact, 9);

        backend.Text = GenerationBackend.Describe();
        AddRow("Generation engine", backend, 10);
        var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill };
        actions.Controls.AddRange([generate, stop]);
        AddWide(actions, 11, SizeType.AutoSize);
        AddWide(bar, 12, SizeType.AutoSize);
        AddWide(timing, 13, SizeType.AutoSize);
        AddWide(status, 14, SizeType.Percent);

        add.Click += (_, _) => AddFolder();
        browse.Click += (_, _) => ChooseOutput();
        generate.Click += Generate;
        stop.Click += (_, _) => { stop.Enabled = false; current?.Cancel(); };
        timer.Tick += (_, _) => UpdateProgress();
        FormClosing += (_, e) =>
        {
            if (current is null) { SaveSettings(); return; }
            e.Cancel = true;
            current.Cancel();
        };
        FormClosed += (_, _) => timer.Dispose();
        AcceptButton = generate;
        LoadSettings();

        void AddRow(string label, Control field, int row, Control? button = null)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row);
            layout.Controls.Add(field, 1, row);
            if (button is not null) layout.Controls.Add(button, 2, row);
        }

        void AddWide(Control control, int row, SizeType size)
        {
            layout.RowStyles.Add(new(size, size == SizeType.Percent ? 100 : 0));
            layout.Controls.Add(control, 0, row);
            layout.SetColumnSpan(control, 3);
        }
    }

    private void AddFolder()
    {
        using var dialog = new FolderBrowserDialog { Description = "Add a corpus folder", UseDescriptionForTitle = true };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        var lines = directories.Lines.Where(line => !string.IsNullOrWhiteSpace(line)).ToList();
        if (lines.Contains(dialog.SelectedPath, StringComparer.OrdinalIgnoreCase)) return;
        lines.Add(dialog.SelectedPath);
        directories.Lines = [.. lines];
    }

    private void ChooseOutput()
    {
        using var dialog = new SaveFileDialog { Filter = "N-gram trie (*.ngtr)|*.ngtr", FileName = output.Text, OverwritePrompt = false };
        if (dialog.ShowDialog(this) == DialogResult.OK) output.Text = dialog.FileName;
    }

    private async void Generate(object? sender, EventArgs e)
    {
        var folders = directories.Lines.Select(line => line.Trim().Trim('"')).Where(line => line.Length != 0)
            .Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (folders.Length == 0) { status.Text = "Add at least one corpus file or folder."; return; }
        var missing = folders.FirstOrDefault(path => !Directory.Exists(path) && !File.Exists(path));
        if (missing is not null) { status.Text = $"Corpus input not found: {missing}"; return; }

        string destination;
        try { destination = Path.GetFullPath(output.Text.Trim()); }
        catch (Exception error) when (error is ArgumentException or NotSupportedException)
        { status.Text = "Enter a valid trie output file."; return; }
        if (Directory.Exists(destination)) { status.Text = "Choose a file for the trie output."; return; }
        if (File.Exists(destination) && MessageBox.Show(this, "Replace the existing trie?", Text,
            MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;

        var args = new List<string>
        {
            "--trie-output", destination, "--workers", workers.Value.ToString(CultureInfo.InvariantCulture),
            "--language", SelectedLanguage,
        };
        AddOption("--trie-size", bytes);
        AddOption("--unigrams", unigrams);
        AddOption("--bigrams", bigrams);
        AddOption("--trigrams", trigrams);
        if (!includeSubdirectories.Checked) args.Add("--no-subdirectories");
        if (!useCache.Checked) args.Add("--no-cache");
        if (lowMemory.Checked) args.Add("--low-memory");
        if (compact.Checked) args.Add("--compact");
        args.Add("--");
        args.AddRange(folders);

        using var cancellation = new CancellationTokenSource();
        current = cancellation;
        SetRunning(true);
        status.Clear();
        backend.Text = GenerationBackend.Describe();
        SaveSettings();
        progress = new("Finding corpus files", 0);
        clock.Restart();
        timer.Start();
        UpdateProgress();
        var updates = new Progress<GenerationProgress>(update =>
        {
            if (current != cancellation) return;
            progress = update;
            UpdateProgress();
        });

        try
        {
            var result = await GenerationBackend.GenerateAsync([.. args], ((IProgress<GenerationProgress>)updates).Report, cancellation.Token);
            progress = new("Complete", 100);
            UpdateProgress();
            status.Text = $"Saved {result.TrieBytes:N0} bytes to {destination}{Environment.NewLine}" +
                          $"{result.Unigrams:N0} unigrams · {result.Bigrams:N0} bigrams · {result.Trigrams:N0} trigrams";
            TrieCreated?.Invoke(destination, SelectedLanguage);
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { status.Text = "Generation stopped."; }
        catch (Exception error)
        {
            status.Text = error is AggregateException aggregate ? aggregate.Flatten().InnerExceptions[0].Message : error.Message;
        }
        finally
        {
            timer.Stop();
            clock.Stop();
            current = null;
            SetRunning(false);
        }

        void AddOption(string flag, TextBox field)
        {
            if (string.IsNullOrWhiteSpace(field.Text)) return;
            args.Add(flag);
            args.Add(field.Text.Trim());
        }
    }

    private void UpdateProgress()
    {
        bar.Value = progress.Percent;
        string eta = progress.Percent == 0 ? "Calculating…" :
            FormatTime(TimeSpan.FromSeconds(clock.Elapsed.TotalSeconds * (100 - progress.Percent) / progress.Percent));
        timing.Text = $"{progress.Stage} · {progress.Percent}% · Estimated time remaining {eta}";
    }

    private static string FormatTime(TimeSpan time) => $"{(long)time.TotalHours:D2}:{time.Minutes:D2}:{time.Seconds:D2}";

    private string SelectedLanguage => ((LanguageOption)language.SelectedItem!).Code;

    private void LoadSettings()
    {
        try
        {
            if (settings.Load() is not { } saved) return;
            directories.Lines = saved.Directories ?? [];
            includeSubdirectories.Checked = saved.IncludeSubdirectories;
            output.Text = saved.Output;
            language.SelectedItem = Languages.FirstOrDefault(option => option.Code == saved.Language) ?? Languages[0];
            bytes.Text = saved.MaximumBytes;
            unigrams.Text = saved.Unigrams;
            bigrams.Text = saved.Bigrams;
            trigrams.Text = saved.Trigrams;
            workers.Value = Math.Clamp(saved.Workers, workers.Minimum, workers.Maximum);
            useCache.Checked = saved.Cache;
            lowMemory.Checked = saved.LowMemory;
            compact.Checked = saved.Compact;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        { status.Text = $"Could not load settings: {error.Message}"; }
    }

    private void SaveSettings()
    {
        try
        {
            settings.Save(new(directories.Lines, output.Text, bytes.Text, unigrams.Text, bigrams.Text, trigrams.Text,
                (int)workers.Value, useCache.Checked, lowMemory.Checked, compact.Checked, SelectedLanguage,
                includeSubdirectories.Checked));
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        { status.Text = $"Could not save settings: {error.Message}"; }
    }

    private void SetRunning(bool running)
    {
        foreach (var control in new Control[] { directories, includeSubdirectories, output, language, bytes, unigrams,
            bigrams, trigrams, workers, useCache, lowMemory, compact, add, browse, generate })
            control.Enabled = !running;
        stop.Enabled = running;
    }
}
