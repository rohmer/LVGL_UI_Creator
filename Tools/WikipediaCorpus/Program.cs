namespace WikipediaCorpus;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new ExportForm());
    }
}

internal sealed class ExportForm : Form
{
    private sealed record LanguageOption(string? Code, string Label)
    {
        public override string ToString() => Label;
    }

    private static readonly LanguageOption[] Languages =
    [
        new(null, "Auto (dump language)"), new("en", "English"), new("es", "Spanish"),
        new("fr", "French"), new("pt", "Portuguese"), new("de", "German"), new("el", "Greek"),
        new("ar", "Arabic (Unicode)"), new("hi", "Hindi (Unicode)"), new("zh", "Mandarin Chinese (Unicode)"),
        new("bn", "Bengali (Unicode)"), new("id", "Indonesian (Unicode)"), new("ur", "Urdu (Unicode)"), new("ru", "Russian (Unicode)"),
    ];

    private readonly TextBox input = new() { Dock = DockStyle.Fill, AccessibleName = "Wikipedia dump" };
    private readonly TextBox output = new() { Dock = DockStyle.Fill, AccessibleName = "Corpus folder" };
    private readonly ComboBox language = new()
    {
        DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, AccessibleName = "Sentence language",
    };
    private readonly Button browseInput = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for Wikipedia dump" };
    private readonly Button browseOutput = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for corpus folder" };
    private readonly Button extract = new() { Text = "Extract", AutoSize = true };
    private readonly Button cancel = new() { Text = "Cancel", AutoSize = true, Enabled = false };
    private readonly ProgressBar bar = new() { Dock = DockStyle.Fill, Maximum = 1000 };
    private readonly TextBox status = new()
    {
        Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical,
        BackColor = SystemColors.Control, BorderStyle = BorderStyle.None, AccessibleName = "Extraction status",
        Text = "Choose a Wikipedia dump and a corpus folder.",
    };
    private CancellationTokenSource? current;
    private bool closeWhenFinished;

    internal bool IsRunning => current is not null;
    internal event Action<string, string>? CorpusCreated;

    internal void CancelWork() => CancelExtraction();

    public ExportForm()
    {
        Text = "Wikipedia Corpus Extractor";
        ClientSize = new Size(780, 320);
        MinimumSize = new Size(560, 310);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Font;

        language.Items.AddRange(Languages);
        language.SelectedIndex = 0;

        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new(20), ColumnCount = 3, RowCount = 6 };
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        layout.ColumnStyles.Add(new(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        Controls.Add(layout);

        AddRow(0, "Wikipedia dump", input, browseInput);
        AddRow(1, "Corpus folder", output, browseOutput);
        AddRow(2, "Sentence language", language);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = false };
        actions.Controls.AddRange([extract, cancel]);
        layout.RowStyles.Add(new(SizeType.AutoSize));
        layout.Controls.Add(actions, 1, 3);
        AddWide(4, bar, SizeType.AutoSize);
        AddWide(5, status, SizeType.Percent);

        browseInput.Click += (_, _) => ChooseInput();
        browseOutput.Click += (_, _) => ChooseOutput();
        extract.Click += Extract;
        cancel.Click += (_, _) => CancelExtraction();
        FormClosing += (_, e) =>
        {
            if (current is null) return;
            e.Cancel = true;
            closeWhenFinished = true;
            CancelExtraction();
        };
        AcceptButton = extract;

        void AddRow(int row, string text, Control control, Button? button = null)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(new Label { Text = text, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row);
            control.Margin = new Padding(6, 5, 3, 5);
            layout.Controls.Add(control, 1, row);
            if (button is not null) layout.Controls.Add(button, 2, row);
        }

        void AddWide(int row, Control control, SizeType size)
        {
            layout.RowStyles.Add(new(size, size == SizeType.Percent ? 100 : 0));
            layout.Controls.Add(control, 0, row);
            layout.SetColumnSpan(control, 3);
        }
    }

    private void ChooseInput()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Choose a Wikipedia dump",
            Filter = "Wikipedia XML dumps (*.xml.bz2)|*.xml.bz2|Bzip2 files (*.bz2)|*.bz2",
            CheckFileExists = true,
        };
        if (File.Exists(input.Text)) dialog.FileName = input.Text;
        if (dialog.ShowDialog(this) == DialogResult.OK) input.Text = dialog.FileName;
    }

    private void ChooseOutput()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Choose a folder for the extracted corpus",
            UseDescriptionForTitle = true,
            SelectedPath = Directory.Exists(output.Text) ? output.Text : "",
        };
        if (dialog.ShowDialog(this) == DialogResult.OK) output.Text = dialog.SelectedPath;
    }

    private async void Extract(object? sender, EventArgs args)
    {
        if (current is not null) return;
        if (string.IsNullOrWhiteSpace(input.Text))
        {
            status.Text = "Choose a Wikipedia dump.";
            return;
        }
        if (string.IsNullOrWhiteSpace(output.Text))
        {
            status.Text = "Choose a corpus folder.";
            return;
        }

        string source;
        string destination;
        try
        {
            source = Path.GetFullPath(input.Text.Trim());
            if (!File.Exists(source)) throw new FileNotFoundException("Select an existing Wikipedia dump.");
            string folder = Path.GetFullPath(output.Text.Trim());
            if (File.Exists(folder)) throw new IOException("Select a folder for the corpus output.");
            destination = WikipediaExporter.GetOutputPath(source, folder);
            if (Directory.Exists(destination)) throw new IOException($"The output file path is already a folder. {destination}");
        }
        catch (Exception error) when (error is ArgumentException or IOException or NotSupportedException or UnauthorizedAccessException)
        {
            status.Text = error.Message;
            return;
        }

        bool overwrite = File.Exists(destination);
        if (overwrite && MessageBox.Show(this, $"Replace the existing file?{Environment.NewLine}{destination}",
                Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) != DialogResult.Yes)
            return;

        using var operation = new CancellationTokenSource();
        current = operation;
        SetRunning(true);
        bar.Value = 0;
        status.Text = "Opening the dump…";
        string? languageCode = ((LanguageOption)language.SelectedItem!).Code;
        string sentenceEngine = languageCode is null ? "Sentence engine follows the dump language. New languages use Unicode boundaries without a POS model." : CorpusCrawler.OpenNlpSentences.Describe(languageCode);
        var progress = new Progress<ExportProgress>(value =>
        {
            if (current != operation || operation.IsCancellationRequested) return;
            double fraction = value.CompressedBytesTotal == 0 ? 0 :
                Math.Clamp((double)value.CompressedBytesRead / value.CompressedBytesTotal, 0, 1);
            bar.Value = (int)(fraction * bar.Maximum);
            status.Text = $"{sentenceEngine}{Environment.NewLine}Compressed dump read {fraction:P1}{Environment.NewLine}" +
                FormatCounts(value.PagesRead, value.ArticlesRead, value.SentencesWritten);
        });

        try
        {
            var result = await Task.Run(() => WikipediaExporter.ExportAsync(source, destination, languageCode, overwrite,
                progress, operation.Token));
            bar.Value = bar.Maximum;
            string name = Languages.FirstOrDefault(option => option.Code == result.LanguageCode)?.Label ?? result.LanguageCode;
            status.Text = $"Extraction complete · {name}{Environment.NewLine}" +
                $"{CorpusCrawler.OpenNlpSentences.Describe(result.LanguageCode)}{Environment.NewLine}" +
                $"{FormatCounts(result.PagesRead, result.ArticlesRead, result.SentencesWritten)}{Environment.NewLine}" +
                result.OutputPath;
            CorpusCreated?.Invoke(result.OutputPath, result.LanguageCode);
        }
        catch (OperationCanceledException) when (operation.IsCancellationRequested)
        {
            status.Text = "Extraction canceled.";
        }
        catch (Exception error)
        {
            status.Text = error.Message;
        }
        finally
        {
            current = null;
            SetRunning(false);
            if (closeWhenFinished) Close();
        }
    }

    private void CancelExtraction()
    {
        if (current is null || current.IsCancellationRequested) return;
        cancel.Enabled = false;
        status.Text = "Canceling extraction…";
        current.Cancel();
    }

    private void SetRunning(bool running)
    {
        foreach (Control control in new Control[] { input, output, language, browseInput, browseOutput, extract })
            control.Enabled = !running;
        cancel.Enabled = running;
    }

    private static string FormatCounts(long pages, long articles, long sentences) =>
        $"{pages:N0} pages · {articles:N0} articles · {sentences:N0} sentences";
}
