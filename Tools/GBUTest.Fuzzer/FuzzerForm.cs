namespace GBUTest.Fuzzer;

internal sealed class FuzzerForm : Form
{
    private const string TextFilter = "Text files (*.txt)|*.txt|All files (*.*)|*.*";
    private const string TrieFilter = "N-gram trie (*.ngtr)|*.ngtr";
    private static readonly Color AdjacentChange = Color.FromArgb(218, 244, 222);
    private static readonly Color CommonChange = Color.FromArgb(252, 223, 223);

    private readonly TextBox input = new() { Dock = DockStyle.Fill, AccessibleName = "Sentence input" };
    private readonly TextBox eligibilityTrie = new() { Dock = DockStyle.Fill, AccessibleName = "Eligibility trie" };
    private readonly TextBox output = new() { Dock = DockStyle.Fill, AccessibleName = "Output file" };
    private readonly NumericUpDown adjacent = Percentage("Adjacent-key mistakes (%)");
    private readonly NumericUpDown common = Percentage("Common misspellings (%)");
    private readonly Button browseInput = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for sentence input" };
    private readonly Button browseTrie = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for eligibility trie" };
    private readonly Button browseOutput = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for output file" };
    private readonly Button generate = new() { Text = "Generate 1,000 pairs", AutoSize = true };
    private readonly Button open = new() { Text = "Open paired file…", AutoSize = true };
    private readonly Button cancel = new() { Text = "Cancel", AutoSize = true, Enabled = false };
    private readonly Label status = new() { Dock = DockStyle.Fill, AutoEllipsis = true, Height = 23 };
    private readonly DataGridView preview = new()
    {
        Dock = DockStyle.Fill, AccessibleName = "Sentence pairs", BackgroundColor = SystemColors.Window,
        AccessibleDescription = "Common misspelling changes appear red. Adjacent-key-only changes appear green.",
        AutoGenerateColumns = false, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
        AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.DisplayedCellsExceptHeaders, ReadOnly = true,
        RowHeadersVisible = false, AllowUserToAddRows = false, AllowUserToDeleteRows = false,
        AllowUserToResizeRows = false, DefaultCellStyle = new() { WrapMode = DataGridViewTriState.True },
    };
    private CancellationTokenSource? active;
    private bool closeWhenDone;

    internal bool IsRunning => active is not null;
    internal event Action<string>? FuzzedFileCreated;

    public FuzzerForm()
    {
        Text = "GBUTest Fuzzer";
        ClientSize = new(1060, 680);
        MinimumSize = new(740, 420);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Font;

        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, Padding = new(16) };
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        layout.ColumnStyles.Add(new(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        Controls.Add(layout);

        AddPathRow("Sentence input", input, browseInput, 0);
        AddPathRow("Eligibility trie", eligibilityTrie, browseTrie, 1);
        AddPathRow("Output file", output, browseOutput, 2);
        AddWide(new Label
        {
            AutoSize = true,
            Text = "Use one sentence per input line. Only whole words stored as unigrams in the trie can be changed.",
        }, 3);
        AddWide(Flow(LabelFor(adjacent), adjacent, LabelFor(common), common,
            new Label { AutoSize = true, Text = "Independent chances per eligible word", Anchor = AnchorStyles.Left }), 4);
        AddWide(Flow(generate, open, cancel,
            Legend("Adjacent-key only", AdjacentChange), Legend("Common misspelling", CommonChange)), 5);

        preview.Columns.Add(TextColumn("Original", nameof(SentencePair.Original)));
        preview.Columns.Add(TextColumn("Fuzzed", nameof(SentencePair.Fuzzed)));
        preview.CellFormatting += FormatPair;
        AddWide(preview, 6, fill: true);
        AddWide(status, 7);

        browseInput.Click += (_, _) => ChooseInput();
        browseTrie.Click += (_, _) => ChooseEligibilityTrie();
        browseOutput.Click += (_, _) => ChooseOutput();
        generate.Click += async (_, _) => await GenerateAsync();
        open.Click += async (_, _) => await OpenPairsAsync();
        cancel.Click += (_, _) => CancelWork();
        FormClosing += (_, e) =>
        {
            if (active is null) return;
            e.Cancel = true;
            closeWhenDone = true;
            CancelWork();
        };
        AcceptButton = generate;

        void AddPathRow(string label, TextBox field, Button browse, int row)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left }, 0, row);
            layout.Controls.Add(field, 1, row);
            layout.Controls.Add(browse, 2, row);
        }

        void AddWide(Control control, int row, bool fill = false)
        {
            layout.RowStyles.Add(fill ? new(SizeType.Percent, 100) : new(SizeType.AutoSize));
            layout.Controls.Add(control, 0, row);
            layout.SetColumnSpan(control, 3);
        }
    }

    private static NumericUpDown Percentage(string name) => new()
    {
        AccessibleName = name, Minimum = 0, Maximum = 100, Value = 5, DecimalPlaces = 1,
        Increment = 0.5m, Width = 70,
    };

    private static Label LabelFor(Control field) => new()
    {
        Text = field.AccessibleName, AutoSize = true, Anchor = AnchorStyles.Left,
    };

    private static FlowLayoutPanel Flow(params Control[] controls)
    {
        var row = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, WrapContents = false,
        };
        row.Controls.AddRange(controls);
        return row;
    }

    private static Label Legend(string text, Color background) => new()
    {
        Text = text, BackColor = background, ForeColor = Color.Black, AutoSize = true,
        Anchor = AnchorStyles.Left, Padding = new(6, 3, 6, 3),
    };

    private static DataGridViewTextBoxColumn TextColumn(string title, string property) => new()
    {
        HeaderText = title, DataPropertyName = property, SortMode = DataGridViewColumnSortMode.NotSortable,
    };

    private void FormatPair(object? sender, DataGridViewCellFormattingEventArgs e)
    {
        if (e.RowIndex < 0 || preview.Rows[e.RowIndex].DataBoundItem is not SentencePair { Changed: true } pair) return;
        e.CellStyle!.BackColor = pair.HasCommonMisspelling ? CommonChange : AdjacentChange;
        e.CellStyle.ForeColor = Color.Black;
        e.CellStyle.SelectionBackColor = pair.HasCommonMisspelling ? Color.DarkRed : Color.DarkGreen;
        e.CellStyle.SelectionForeColor = Color.White;
    }

    private void ChooseInput()
    {
        using var dialog = new OpenFileDialog { Title = "Choose sentence input file", Filter = TextFilter, RestoreDirectory = true };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        UseCorpus(dialog.FileName);
    }

    internal void UseCorpus(string path)
    {
        if (IsRunning) return;
        input.Text = path;
        if (string.IsNullOrWhiteSpace(output.Text))
            output.Text = Path.Combine(Path.GetDirectoryName(path)!, Path.GetFileNameWithoutExtension(path) + "-fuzzed.txt");
    }

    internal void UseTrie(string path)
    {
        if (!IsRunning) eligibilityTrie.Text = path;
    }

    private void ChooseOutput()
    {
        using var dialog = new SaveFileDialog
        {
            Title = "Choose output file", Filter = TextFilter, DefaultExt = "txt", FileName = output.Text,
            OverwritePrompt = false, RestoreDirectory = true,
        };
        if (dialog.ShowDialog(this) == DialogResult.OK) output.Text = dialog.FileName;
    }

    private void ChooseEligibilityTrie()
    {
        using var dialog = new OpenFileDialog { Title = "Choose eligibility trie", Filter = TrieFilter, RestoreDirectory = true };
        if (dialog.ShowDialog(this) == DialogResult.OK) eligibilityTrie.Text = dialog.FileName;
    }

    private async Task GenerateAsync()
    {
        string source;
        string triePath;
        string destination;
        try
        {
            source = Path.GetFullPath(input.Text.Trim().Trim('"'));
            string selectedTrie = eligibilityTrie.Text.Trim().Trim('"');
            if (selectedTrie.Length == 0)
            {
                status.Text = "Choose an eligibility trie (.ngtr).";
                return;
            }
            triePath = Path.GetFullPath(selectedTrie);
        }
        catch (Exception error) when (error is ArgumentException or NotSupportedException or PathTooLongException)
        {
            status.Text = "Enter valid sentence and trie paths.";
            return;
        }

        if (!File.Exists(source)) { status.Text = "Choose an existing sentence input file."; return; }
        if (!File.Exists(triePath)) { status.Text = "Choose an existing eligibility trie (.ngtr)."; return; }
        try { destination = Path.GetFullPath(output.Text.Trim().Trim('"')); }
        catch (Exception error) when (error is ArgumentException or NotSupportedException or PathTooLongException)
        {
            status.Text = "Enter a valid output path.";
            return;
        }
        string[] resultFiles = [destination, SentenceFuzzer.GetEditMetadataPath(destination)];
        if (resultFiles.Contains(source, StringComparer.OrdinalIgnoreCase) ||
            resultFiles.Contains(triePath, StringComparer.OrdinalIgnoreCase))
        {
            status.Text = "Choose an output path that keeps the result files separate from the sentence input and eligibility trie.";
            return;
        }
        if (resultFiles.Any(Directory.Exists)) { status.Text = "Choose an output path available for both result files."; return; }
        var existing = resultFiles.Where(File.Exists).ToArray();
        if (existing.Length != 0)
        {
            string question = existing.Length == 1 ? "Replace this file?" : "Replace these files?";
            string prompt = question + Environment.NewLine + Environment.NewLine + string.Join(Environment.NewLine, existing);
            if (MessageBox.Show(this, prompt, Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question,
                MessageBoxDefaultButton.Button2) != DialogResult.Yes) return;
        }

        double adjacentPercent = (double)adjacent.Value;
        double commonPercent = (double)common.Value;
        if (await RunAsync("Generating pairs…", "Saved", destination,
            token => SentenceFuzzer.Generate(source, triePath, destination, adjacentPercent, commonPercent, token)))
        {
            FuzzedFileCreated?.Invoke(destination);
        }
    }

    private async Task OpenPairsAsync()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Open paired file", Filter = TextFilter, RestoreDirectory = true,
            FileName = File.Exists(output.Text) ? output.Text : "",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        string path = dialog.FileName;
        await RunAsync("Reading pairs…", "Opened", path, token => SentenceFuzzer.ReadPairs(path, token));
    }

    private async Task<bool> RunAsync(string activity, string completed, string path, Func<CancellationToken, SentencePair[]> work)
    {
        using var cancellation = new CancellationTokenSource();
        active = cancellation;
        SetBusy(true);
        status.Text = activity;
        try
        {
            var pairs = await Task.Run(() => work(cancellation.Token), cancellation.Token);
            preview.DataSource = pairs;
            preview.ClearSelection();
            status.Text = $"{completed} {Path.GetFileName(path)} · {pairs.Length:N0} pairs · {pairs.Count(pair => pair.Changed):N0} changed";
            return true;
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            status.Text = "Canceled.";
            return false;
        }
        catch (Exception error) when (error is IOException or InvalidDataException or UnauthorizedAccessException or
                                      ArgumentException or NotSupportedException)
        {
            status.Text = error.Message;
            if (!closeWhenDone) MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
        finally
        {
            active = null;
            SetBusy(false);
            if (closeWhenDone) Close();
        }
    }

    internal void CancelWork()
    {
        cancel.Enabled = false;
        active?.Cancel();
    }

    private void SetBusy(bool busy)
    {
        foreach (Control control in new Control[] { input, eligibilityTrie, output, browseInput, browseTrie, browseOutput, adjacent, common, generate, open })
            control.Enabled = !busy;
        cancel.Enabled = busy;
    }
}
