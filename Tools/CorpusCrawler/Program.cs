using Lingua;

namespace CorpusCrawler;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new CrawlerForm());
    }
}

internal sealed class CrawlerForm : Form
{
    private readonly TextBox url = new()
    {
        PlaceholderText = "https://www.foxnews.com/",
        Dock = DockStyle.Fill,
        Multiline = true,
        AutoSize = false,
        MinimumSize = new Size(0, 110),
        AcceptsReturn = true,
        ScrollBars = ScrollBars.Vertical,
        WordWrap = false,
    };
    private readonly TextBox output = new() { Dock = DockStyle.Fill };
    private readonly NumericUpDown workers = new() { Minimum = 1, Maximum = 64, Value = 4 };
    private readonly NumericUpDown limit = new() { Minimum = 1, Maximum = 1000000, Value = 500 };
    private readonly ComboBox language = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly CheckBox sameSiteOnly = new() { Text = "Stay on the starting site", Checked = true, AutoSize = true };
    private readonly Button browse = new() { Text = "Browse…", AutoSize = true };
    private readonly Button start = new() { Text = "Start crawl", AutoSize = true };
    private readonly Button cancel = new() { Text = "Stop", Enabled = false, AutoSize = true };
    private readonly ProgressBar progressBar = new() { Dock = DockStyle.Fill, Maximum = 100 };
    private readonly RichTextBox status = new()
    {
        Dock = DockStyle.Fill,
        ReadOnly = true,
        BorderStyle = BorderStyle.None,
        BackColor = SystemColors.Control,
        DetectUrls = false,
        ScrollBars = RichTextBoxScrollBars.Vertical,
        TabStop = false,
        Text = "Ready",
    };
    private CancellationTokenSource? current;

    internal bool IsRunning => current is not null;
    internal event Action<string, string>? CorpusCreated;

    internal void CancelWork() => current?.Cancel();

    public CrawlerForm()
    {
        Text = "Corpus Crawler";
        ClientSize = new Size(1600, 960);
        MinimumSize = new Size(620, 420);
        StartPosition = FormStartPosition.CenterScreen;
        if (Screen.PrimaryScreen is { } screen)
            Size = new Size(Math.Min(Width, screen.WorkingArea.Width), Math.Min(Height, screen.WorkingArea.Height));
        AutoScaleMode = AutoScaleMode.Font;

        language.Items.AddRange(OpenNlpSentences.Languages.Cast<object>().ToArray());
        language.SelectedIndexChanged += (_, _) =>
        {
            if (current is null && language.SelectedItem is Language choice)
                status.Text = OpenNlpSentences.Describe(LanguageCode(choice));
        };
        language.SelectedIndex = 0;
        output.Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Corpus");
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new(20), ColumnCount = 3 };
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        layout.ColumnStyles.Add(new(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        Controls.Add(layout);

        AddRow("URLs (one per line)", url, 0);
        AddRow("Output folder", output, 1);
        layout.Controls.Add(browse, 2, 1);
        AddRow("Language", language, 2);
        AddRow("Workers", workers, 3);
        AddRow("Page limit per URL", limit, 4);
        AddRow("Link scope", sameSiteOnly, 5);

        var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill };
        actions.Controls.AddRange([start, cancel]);
        layout.RowStyles.Add(new(SizeType.AutoSize));
        layout.Controls.Add(actions, 1, 6);
        layout.RowStyles.Add(new(SizeType.AutoSize));
        layout.Controls.Add(progressBar, 0, 7);
        layout.SetColumnSpan(progressBar, 3);
        layout.RowStyles.Add(new(SizeType.Percent, 100));
        layout.Controls.Add(status, 0, 8);
        layout.SetColumnSpan(status, 3);

        AcceptButton = start;
        start.Click += StartCrawl;
        cancel.Click += (_, _) => current?.Cancel();
        browse.Click += (_, _) => ChooseOutput();
        FormClosing += (_, e) =>
        {
            if (current is null) return;
            e.Cancel = true;
            current.Cancel();
        };

        void AddRow(string caption, Control field, int row)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(new Label { Text = caption, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row);
            field.Margin = new Padding(8, 5, 3, 5);
            layout.Controls.Add(field, 1, row);
        }
    }

    private void ChooseOutput()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Choose a folder for the site corpora",
            SelectedPath = output.Text,
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK) output.Text = dialog.SelectedPath;
    }

    private async void StartCrawl(object? sender, EventArgs e)
    {
        (Uri Url, string Output)[] sites;
        try
        {
            string directory = Path.GetFullPath(output.Text.Trim());
            if (File.Exists(directory)) throw new ArgumentException("Select an output folder.");
            sites = CrawlBatch.Plan(url.Lines, directory);
        }
        catch (Exception error) when (error is ArgumentException or NotSupportedException)
        {
            status.Text = error.Message;
            return;
        }
        if (sites.Any(site => File.Exists(site.Output)) && MessageBox.Show(this, "Replace existing corpus files in this batch?",
            Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;

        bool restart = false;
        if (sites.Any(site => File.Exists(CrawlCheckpoint.PathFor(site.Output))))
        {
            var choice = MessageBox.Show(this,
                "Resume saved crawls in this batch? Choose No to start over.",
                Text, MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
            if (choice == DialogResult.Cancel) return;
            restart = choice == DialogResult.No;
        }

        using var cancellation = new CancellationTokenSource();
        current = cancellation;
        SetRunning(true);
        var outcomes = new List<string>();
        try
        {
            for (int index = 0; index < sites.Length && !cancellation.IsCancellationRequested; index++)
            {
                var (baseUrl, destination) = sites[index];
                var caption = $"Crawl {index + 1} of {sites.Length} · {baseUrl}";
                bool siteRunning = true;
                progressBar.Value = 0;
                string sentenceEngine = OpenNlpSentences.Describe(LanguageCode((Language)language.SelectedItem!));
                status.Text = $"{caption}{Environment.NewLine}{sentenceEngine}{Environment.NewLine}Starting crawl…";
                var options = new CrawlOptions(baseUrl, (int)workers.Value, (int)limit.Value,
                    (Language)language.SelectedItem!, destination, restart, sameSiteOnly.Checked);
                var progress = new Progress<CrawlProgress>(value =>
                {
                    if (!siteRunning) return;
                    progressBar.Value = value.PercentComplete;
                    status.Text = $"{caption}{Environment.NewLine}{sentenceEngine}{Environment.NewLine}{Describe(value)}";
                });
                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                    var result = await Task.Run(() => Crawler.RunAsync(options, progress, cancellation.Token));
                    progressBar.Value = result.PercentComplete;
                    outcomes.Add($"Saved {result.Sentences:N0} sentences to {destination}");
                    CorpusCreated?.Invoke(destination, LanguageCode(options.Language));
                }
                catch (CrawlStoppedAfterExportException)
                {
                    outcomes.Add($"Saved the partial corpus to {destination}");
                    CorpusCreated?.Invoke(destination, LanguageCode(options.Language));
                }
                catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { }
                catch (Exception error)
                {
                    outcomes.Add($"{baseUrl} — {error.Message}");
                }
                finally { siteRunning = false; }
            }
            if (cancellation.IsCancellationRequested)
                outcomes.Add("Batch stopped. Enter the same URL list and output folder to resume.");
            status.Lines = [.. outcomes];
        }
        finally
        {
            current = null;
            SetRunning(false);
        }
    }

    private void SetRunning(bool running)
    {
        foreach (var control in new Control[] { url, output, language, workers, limit, sameSiteOnly, browse, start })
            control.Enabled = !running;
        cancel.Enabled = running;
    }

    private static string LanguageCode(Language selected) => selected switch
    {
        Language.English => "en",
        Language.Spanish => "es",
        Language.French => "fr",
        Language.Portuguese => "pt",
        Language.German => "de",
        Language.Greek => "el",
        Language.Arabic => "ar", Language.Hindi => "hi", Language.Chinese => "zh",
        Language.Bengali => "bn", Language.Indonesian => "id", Language.Urdu => "ur", Language.Russian => "ru",
        _ => throw new ArgumentException($"Unsupported corpus language: {selected}", nameof(selected)),
    };

    private static string Describe(CrawlProgress value)
    {
        var summary = $"{value.Visited:N0} / {value.Discovered:N0} URLs visited · " +
                      $"{value.Sentences:N0} sentences · {value.Failed:N0} errors";
        var average = value.AveragePageTime is { } elapsed ? $"{elapsed.TotalSeconds:N2} s" : "—";
        var remaining = value.EstimatedTimeRemaining is { } estimate
            ? $"{(long)estimate.TotalHours:D2}:{estimate.Minutes:D2}:{estimate.Seconds:D2}"
            : "—";
        var timing = $"{value.PercentComplete}% of page limit · Average page time {average}" +
                     $"{Environment.NewLine}Estimated time remaining {remaining}";
        var description = $"{timing}{Environment.NewLine}{summary}";
        return value.LastError is null ? description : $"{description}{Environment.NewLine}{value.LastError}";
    }
}
