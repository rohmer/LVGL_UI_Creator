using NgramTrie.Reader;

namespace KeyboardSimulator;

internal sealed class TrieViewerForm : Form
{
    private static readonly Color Workspace = Color.FromArgb(23, 30, 42);
    private static readonly Color Heading = Color.FromArgb(214, 225, 240);
    private readonly DataGridView[] grids = new DataGridView[3];
    private readonly Label status = new() { Dock = DockStyle.Fill, ForeColor = Heading, AutoEllipsis = true };
    private TrieEntry[][] entries = [[], [], []];
    private CancellationTokenSource? reading;

    public TrieViewerForm()
    {
        Text = "Trie Viewer";
        ClientSize = new Size(1050, 620);
        MinimumSize = new Size(620, 320);
        StartPosition = FormStartPosition.CenterParent;
        if (Screen.PrimaryScreen is { } display)
            Size = new Size(Math.Min(Width, display.WorkingArea.Width), Math.Min(Height, display.WorkingArea.Height));
        AutoScaleMode = AutoScaleMode.Font;
        BackColor = Workspace;

        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new(16, 12, 16, 6), ColumnCount = 3, RowCount = 3 };
        layout.RowStyles.Add(new(SizeType.Absolute, 32));
        layout.RowStyles.Add(new(SizeType.Percent, 100));
        layout.RowStyles.Add(new(SizeType.Absolute, 32));
        Controls.Add(layout);

        string[] names = ["Unigrams", "Bigrams", "Trigrams"];
        for (int order = 0; order < names.Length; order++)
        {
            layout.ColumnStyles.Add(new(SizeType.Percent, 100f / names.Length));
            layout.Controls.Add(new Label
            {
                Text = names[order], ForeColor = Heading, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft,
            }, order, 0);
            var grid = CreateGrid(names[order]);
            int group = order;
            grid.CellValueNeeded += (_, e) =>
            {
                var rows = entries[group];
                if ((uint)e.RowIndex >= (uint)rows.Length) return;
                e.Value = e.ColumnIndex == 0 ? rows[e.RowIndex].Text : rows[e.RowIndex].Count;
            };
            grids[order] = grid;
            layout.Controls.Add(grid, order, 1);
        }

        status.TextAlign = ContentAlignment.MiddleLeft;
        layout.Controls.Add(status, 0, 2);
        layout.SetColumnSpan(status, 3);
    }

    public async Task SetTrieAsync(TrieModel trie, string filename)
    {
        reading?.Cancel();
        using var cancellation = new CancellationTokenSource();
        reading = cancellation;
        Text = $"Trie Viewer — {filename}";
        ShowEntries([[], [], []]);
        status.Text = "Reading n-grams…";
        try
        {
            var result = await Task.Run(() => TrieEntries.Read(trie, cancellation.Token), cancellation.Token);
            if (reading != cancellation || IsDisposed) return;
            ShowEntries(result);
            status.Text = $"{entries.Sum(group => (long)group.Length):N0} stored n-grams";
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { }
        catch (InvalidDataException error)
        {
            if (reading == cancellation && !IsDisposed) status.Text = error.Message;
        }
        finally { if (reading == cancellation) reading = null; }
    }

    private void ShowEntries(TrieEntry[][] source)
    {
        entries = source;
        for (int order = 0; order < grids.Length; order++)
        {
            grids[order].RowCount = entries[order].Length;
            grids[order].ClearSelection();
            grids[order].Invalidate();
        }
    }

    private static DataGridView CreateGrid(string name)
    {
        var grid = new DataGridView
        {
            AccessibleName = name, Dock = DockStyle.Fill, VirtualMode = true, ReadOnly = true,
            AllowUserToAddRows = false, AllowUserToDeleteRows = false, AllowUserToResizeRows = false,
            RowHeadersVisible = false, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
            MultiSelect = false, BackgroundColor = Color.FromArgb(247, 249, 252),
            BorderStyle = BorderStyle.None, CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal,
            GridColor = Color.FromArgb(224, 231, 241), EnableHeadersVisualStyles = false,
        };
        grid.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(215, 224, 237);
        grid.ColumnHeadersDefaultCellStyle.ForeColor = Color.FromArgb(30, 44, 65);
        grid.AlternatingRowsDefaultCellStyle.BackColor = Color.FromArgb(243, 247, 253);
        grid.RowTemplate.Height = 25;
        grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "Ngram", HeaderText = "N-gram", AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill,
            SortMode = DataGridViewColumnSortMode.NotSortable,
        });
        grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "Count", HeaderText = "Count", Width = 88, ValueType = typeof(uint),
            SortMode = DataGridViewColumnSortMode.NotSortable,
            DefaultCellStyle = new() { Alignment = DataGridViewContentAlignment.MiddleRight, Format = "N0" },
        });
        return grid;
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            reading?.Cancel();
            reading = null;
        }
        base.Dispose(disposing);
    }
}
