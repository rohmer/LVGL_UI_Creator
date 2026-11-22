using System.ComponentModel;
using System.Diagnostics;

namespace GBUTest.UI;

internal sealed class TypingTesterForm : Form
{
    private static readonly TypingOutcome[] Outcomes = Enum.GetValues<TypingOutcome>();
    private readonly TextBox pairedFile = new() { Dock = DockStyle.Fill, AccessibleName = "Paired sentence file" };
    private readonly TextBox trie = new() { Dock = DockStyle.Fill, AccessibleName = "Trie model" };
    private readonly Button browsePairs = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for paired sentence file" };
    private readonly Button browseTrie = new() { Text = "Browse…", AutoSize = true, AccessibleName = "Browse for trie model" };
    private readonly NumericUpDown threshold = new()
    {
        AccessibleName = "Correction threshold", Minimum = 0, Maximum = 1, Value = 0.9m,
        DecimalPlaces = 3, Increment = 0.01m, Width = 78,
    };
    private readonly NumericUpDown regressionStep = new()
    {
        AccessibleName = "Regression step", Minimum = 0.001m, Maximum = 1, Value = 0.05m,
        DecimalPlaces = 3, Increment = 0.01m, Width = 78,
    };
    private readonly Button run = new() { Text = "Run test", AutoSize = true };
    private readonly Button runRegression = new()
    {
        Text = FormattableString.Invariant($"Run regression ({TypingTester.RegressionStart:0.0}–1.0)"), AutoSize = true,
    };
    private readonly Button openCsv = new() { Text = "Open CSV", AutoSize = true, Enabled = false };
    private readonly TextBox csvLocation = new() { Dock = DockStyle.Fill, ReadOnly = true, AccessibleName = "Regression CSV export" };
    private readonly Button cancel = new() { Text = "Cancel", AutoSize = true, Enabled = false };
    private readonly ComboBox outcome = new()
    {
        AccessibleName = "Word outcome filter", DropDownStyle = ComboBoxStyle.DropDownList, Width = 140,
        Enabled = false,
    };
    private readonly Label summary = new() { AutoSize = true };
    private readonly ProgressBar progress = new() { Dock = DockStyle.Fill, AccessibleName = "Pairs scored" };
    private readonly Label status = new() { Dock = DockStyle.Fill, AutoEllipsis = true, Height = 23, Text = "Ready" };
    private readonly DataGridView results = ResultGrid("Word results");
    private readonly DataGridView regressionResults = ResultGrid("Threshold regression results");
    private readonly TabControl views = new() { Dock = DockStyle.Fill };
    private readonly TabPage wordView;
    private readonly TabPage regressionView;

    private CancellationTokenSource? active;
    private bool closeWhenDone;
    private TypingTestReport? report;
    private readonly HashSet<double> bestThresholds = new();
    private string wordSummary = "";
    private string regressionSummary = "";
    private string? savedCsv;

    internal bool IsRunning => active is not null;

    internal void UseTrie(string path)
    {
        if (!IsRunning) trie.Text = path;
    }

    internal void UseFuzzedFile(string path)
    {
        if (!IsRunning) pairedFile.Text = path;
    }

    public TypingTesterForm()
    {
        Text = "GBUTest Scoring Tester";
        ClientSize = new(1120, 720);
        MinimumSize = new(820, 460);
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Font;

        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, Padding = new(16) };
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        layout.ColumnStyles.Add(new(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new(SizeType.AutoSize));
        Controls.Add(layout);

        AddPathRow("Paired sentence file", pairedFile, browsePairs, 0);
        AddPathRow("Trie model", trie, browseTrie, 1);
        AddWide(Flow(LabelFor("Correction threshold"), threshold, run,
            LabelFor("Regression step"), regressionStep, runRegression, cancel), 2);
        AddWide(progress, 3);
        AddWide(status, 4);
        AddWide(summary, 5);
        var export = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 3 };
        export.ColumnStyles.Add(new(SizeType.AutoSize));
        export.ColumnStyles.Add(new(SizeType.Percent, 100));
        export.ColumnStyles.Add(new(SizeType.AutoSize));
        export.Controls.Add(LabelFor("CSV export"), 0, 0);
        export.Controls.Add(csvLocation, 1, 0);
        export.Controls.Add(openCsv, 2, 0);
        AddWide(export, 6);
        AddWide(views, 7, fill: true);

        wordView = ResultTab("Word results", results, Flow(LabelFor("Word outcomes"), outcome));
        regressionView = ResultTab("Threshold regression", regressionResults);
        views.TabPages.AddRange([wordView, regressionView]);
        views.SelectedIndexChanged += (_, _) => ShowSummary();

        results.Columns.AddRange(
            Column("Pair", nameof(TypingWordResult.PairNumber), 64),
            Column("Word", nameof(TypingWordResult.WordNumber), 64),
            TextColumn("Expected", nameof(TypingWordResult.Expected)),
            TextColumn("Typed", nameof(TypingWordResult.Typed)),
            TextColumn("Result", nameof(TypingWordResult.Result)),
            Column("Outcome", nameof(TypingWordResult.Outcome), 108),
            Column("Confidence", nameof(TypingWordResult.Probability), 96, "0.0000",
                "Confidence of the proposed correction, including proposals below the threshold."),
            Column("Points", nameof(TypingWordResult.Points), 66, "+0;-0;0",
                $"Replacing a correctly typed word with the wrong word scores {TypingTester.UglyPoints:N0} points."));
        results.CellFormatting += (_, e) =>
        {
            if (e.Value is TypingOutcome value) e.Value = OutcomeName(value);
        };
        outcome.Items.Add("All outcomes");
        foreach (var value in Outcomes) outcome.Items.Add(OutcomeName(value));
        outcome.SelectedIndex = 0;
        outcome.SelectedIndexChanged += (_, _) => ShowWords();

        regressionResults.AccessibleDescription = "All thresholds with the highest score are marked Best and highlighted.";
        regressionResults.Columns.AddRange(
            Column("Best", "", 56),
            Column("Threshold", nameof(TypingRegressionResult.Threshold), 84, "0.000"),
            Column("Score", nameof(TypingRegressionResult.Score), 80, "+#,##0;-#,##0;0"),
            Column("Composite score", nameof(TypingRegressionResult.CompositeScore), 126, "0.0000",
                "Score divided by the total number of words."),
            Column("Good", nameof(TypingRegressionResult.Good), 58),
            Column("Bad", nameof(TypingRegressionResult.Bad), 58),
            Column("Ugly", nameof(TypingRegressionResult.Ugly), 58,
                hint: $"Each Ugly outcome scores {TypingTester.UglyPoints:N0} points."),
            Column("Kept correct", nameof(TypingRegressionResult.KeptCorrect), 96),
            Column("Missed typo", nameof(TypingRegressionResult.Missed), 96));
        regressionResults.CellFormatting += FormatRegressionRow;

        browsePairs.Click += (_, _) => ChooseFile(pairedFile, "Choose paired sentence file", "Text files (*.txt)|*.txt");
        browseTrie.Click += (_, _) => ChooseFile(trie, "Choose trie model", "N-gram trie (*.ngtr)|*.ngtr");
        run.Click += async (_, _) => await RunAsync();
        runRegression.Click += async (_, _) => await RunRegressionAsync();
        openCsv.Click += (_, _) => OpenRegressionCsv();
        cancel.Click += (_, _) => CancelWork();
        FormClosing += (_, e) =>
        {
            if (active is null) return;
            e.Cancel = true;
            closeWhenDone = true;
            CancelWork();
        };
        AcceptButton = run;

        void AddPathRow(string label, TextBox field, Button browse, int row)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(LabelFor(label), 0, row);
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

    private static Label LabelFor(string label) => new() { Text = label, AutoSize = true, Anchor = AnchorStyles.Left };

    private static DataGridView ResultGrid(string name) => new()
    {
        Dock = DockStyle.Fill, AccessibleName = name, BackgroundColor = SystemColors.Window,
        AutoGenerateColumns = false, ReadOnly = true, RowHeadersVisible = false, AllowUserToAddRows = false,
        AllowUserToDeleteRows = false, AllowUserToResizeRows = false, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
    };

    private static TabPage ResultTab(string title, DataGridView grid, params Control[] header)
    {
        var page = new TabPage(title);
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new(8) };
        foreach (var control in header)
        {
            layout.RowStyles.Add(new(SizeType.AutoSize));
            layout.Controls.Add(control);
        }
        layout.RowStyles.Add(new(SizeType.Percent, 100));
        layout.Controls.Add(grid);
        page.Controls.Add(layout);
        return page;
    }

    private static string OutcomeName(TypingOutcome value) => value switch
    {
        TypingOutcome.KeptCorrect => "Kept correct",
        TypingOutcome.Missed => "Missed typo",
        _ => value.ToString(),
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

    private static DataGridViewTextBoxColumn Column(string title, string property, int width, string? format = null, string? hint = null) => new()
    {
        HeaderText = title, DataPropertyName = property, Width = width, SortMode = DataGridViewColumnSortMode.NotSortable,
        ToolTipText = hint ?? "", DefaultCellStyle = new() { Format = format, NullValue = "—" },
    };

    private static DataGridViewTextBoxColumn TextColumn(string title, string property) => new()
    {
        HeaderText = title, DataPropertyName = property, AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill,
        MinimumWidth = 90, SortMode = DataGridViewColumnSortMode.NotSortable,
    };

    private void ChooseFile(TextBox field, string title, string filter)
    {
        using var dialog = new OpenFileDialog
        {
            Title = title, Filter = filter, RestoreDirectory = true, FileName = File.Exists(field.Text) ? field.Text : "",
        };
        if (dialog.ShowDialog(this) == DialogResult.OK) field.Text = dialog.FileName;
    }

    private Task RunAsync()
    {
        double correctionThreshold = (double)threshold.Value;
        return RunOperation(
            (pairs, model, updates, token) => TypingTester.Run(pairs, model, correctionThreshold, updates, token),
            result =>
            {
                views.SelectedTab = wordView;
                report = result;
                wordSummary = $"Score {result.Score:+#,##0;-#,##0;0}  ·  Composite {FormatComposite(result.CompositeScore)}  ·  " +
                              $"Good {result.Good:N0}  ·  Bad {result.Bad:N0}  ·  Ugly {result.Ugly:N0} ({TypingTester.UglyPoints:N0} each)  ·  " +
                              $"Kept correct {result.KeptCorrect:N0}  ·  Missed typo {result.Missed:N0}";
                ShowSummary();
                if (outcome.SelectedIndex == 0) ShowWords();
                else outcome.SelectedIndex = 0;
                UpdateProgress(result.Pairs, result.Pairs);
                status.Text = $"Scored {result.TotalWords:N0} words across {result.Pairs:N0} pairs with threshold {correctionThreshold:0.000}";
            });
    }

    private Task RunRegressionAsync()
    {
        decimal step = decimal.Round(regressionStep.Value, regressionStep.DecimalPlaces);
        return RunOperation(
            (pairs, model, updates, token) =>
            {
                var result = TypingTester.RunRegression(pairs, model, step, updates, token);
                token.ThrowIfCancellationRequested();
                try
                {
                    return (Report: result, Path: (string?)TypingTester.SaveRegressionCsv(result, pairs), SaveError: (string?)null);
                }
                catch (Exception error) when (error is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException)
                {
                    return (Report: result, Path: (string?)null, SaveError: error.Message);
                }
            },
            result => ShowRegression(result.Report, result.Path, result.SaveError));
    }

    private async Task RunOperation<T>(
        Func<string, string, Action<TypingTestProgress>, CancellationToken, T> work, Action<T> show)
    {
        string pairsPath = pairedFile.Text.Trim().Trim('"');
        string triePath = trie.Text.Trim().Trim('"');
        if (!File.Exists(pairsPath)) { status.Text = "Choose an existing paired sentence file."; return; }
        if (!File.Exists(triePath)) { status.Text = "Choose an existing trie model (.ngtr)."; return; }
        using var cancellation = new CancellationTokenSource();
        active = cancellation;
        SetBusy(true);
        progress.Value = 0;
        status.Text = "Loading paired sentences and trie…";
        IProgress<TypingTestProgress> updates = new Progress<TypingTestProgress>(update =>
        {
            if (active != cancellation) return;
            UpdateProgress(update.CompletedPairs, update.TotalPairs);
            status.Text = $"Scored {update.CompletedPairs:N0} of {update.TotalPairs:N0} pairs";
        });
        try
        {
            show(await Task.Run(() => work(pairsPath, triePath, updates.Report, cancellation.Token), cancellation.Token));
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { status.Text = "Canceled."; }
        catch (Exception error) when (error is IOException or InvalidDataException or UnauthorizedAccessException or
                                      ArgumentException or NotSupportedException)
        {
            status.Text = error.Message;
            if (!closeWhenDone) MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            active = null;
            SetBusy(false);
            if (closeWhenDone) Close();
        }
    }

    private static string FormatComposite(double? value) => value?.ToString("0.0000") ?? "—";

    private void ShowRegression(TypingRegressionReport result, string? csvPath, string? saveError)
    {
        views.SelectedTab = regressionView;
        savedCsv = csvPath;
        csvLocation.Text = csvPath ?? $"Could not save CSV. {saveError}";
        var best = result.BestResults;
        bestThresholds.Clear();
        foreach (var row in best) bestThresholds.Add(row.Threshold);
        regressionResults.DataSource = result.Results as System.Collections.IList ?? result.Results.ToArray();
        regressionResults.ClearSelection();
        string winners = best.Count switch
        {
            0 => "No winning threshold",
            1 => $"Best threshold {best[0].Threshold:0.000}",
            <= 5 => $"Best thresholds {string.Join(", ", best.Select(row => row.Threshold.ToString("0.000")))}",
            _ => $"{best.Count:N0} thresholds tied for best",
        };
        regressionSummary = best.Count == 0 ? winners :
            $"{winners}  ·  Score {best[0].Score:+#,##0;-#,##0;0}  ·  Composite {FormatComposite(best[0].CompositeScore)}";
        ShowSummary();
        UpdateProgress(result.Pairs, result.Pairs);
        status.Text = $"Tested {result.Results.Count:N0} thresholds on {result.TotalWords:N0} words across {result.Pairs:N0} pairs" +
                      (csvPath is null ? "  ·  CSV save failed" : "  ·  CSV saved");
    }

    private void OpenRegressionCsv()
    {
        if (savedCsv is not { } path) return;
        try { using var process = Process.Start(new ProcessStartInfo(path) { UseShellExecute = true }); }
        catch (Exception error) when (error is Win32Exception or IOException or InvalidOperationException)
        {
            status.Text = $"Could not open CSV. {error.Message}";
        }
    }

    private void ShowSummary() => summary.Text = views.SelectedTab == regressionView ? regressionSummary : wordSummary;

    private void FormatRegressionRow(object? sender, DataGridViewCellFormattingEventArgs e)
    {
        if (e.RowIndex < 0 || regressionResults.Rows[e.RowIndex].DataBoundItem is not TypingRegressionResult row ||
            !bestThresholds.Contains(row.Threshold)) return;
        if (e.ColumnIndex == 0) e.Value = "Yes";
        e.CellStyle!.BackColor = Color.FromArgb(219, 237, 255);
        e.CellStyle.ForeColor = Color.Black;
        e.CellStyle.SelectionBackColor = Color.DarkBlue;
        e.CellStyle.SelectionForeColor = Color.White;
    }

    private void ShowWords()
    {
        if (report is null) return;
        if (outcome.SelectedIndex <= 0)
            results.DataSource = report.Words as System.Collections.IList ?? report.Words.ToArray();
        else
        {
            var selected = Outcomes[outcome.SelectedIndex - 1];
            results.DataSource = report.Words.Where(word => word.Outcome == selected).ToArray();
        }
        results.ClearSelection();
    }

    private void UpdateProgress(int completed, int total)
    {
        progress.Maximum = Math.Max(1, total);
        progress.Value = Math.Clamp(completed, 0, progress.Maximum);
    }

    internal void CancelWork()
    {
        cancel.Enabled = false;
        active?.Cancel();
    }

    private void SetBusy(bool busy)
    {
        foreach (Control control in new Control[] { pairedFile, trie, browsePairs, browseTrie, threshold, run, regressionStep, runRegression })
            control.Enabled = !busy;
        cancel.Enabled = busy;
        outcome.Enabled = !busy && report is not null;
        openCsv.Enabled = !busy && savedCsv is not null;
    }
}
