using System.Globalization;
using System.Text;
using NgramTrie;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal sealed class KeyboardPreview : Control
{
    public const decimal DefaultAutocorrectThreshold = 0.3m;

    private static readonly Color ScreenColor = Color.FromArgb(247, 249, 252);
    private static readonly Color KeyboardColor = Color.FromArgb(215, 224, 237);
    private static readonly Color Ink = Color.FromArgb(30, 44, 65);
    private static readonly Color Accent = Color.FromArgb(44, 94, 177);
    private readonly TextBox editor = new()
    {
        Multiline = true, AcceptsReturn = true, BorderStyle = BorderStyle.None,
        ScrollBars = ScrollBars.Vertical, HideSelection = false,
        BackColor = ScreenColor, ForeColor = Ink, AccessibleName = "Preview text",
    };
    private readonly TableLayoutPanel suggestionBar = new()
    {
        ColumnCount = 3, RowCount = 1, BackColor = KeyboardColor,
        AccessibleName = "Text suggestions", Visible = false,
    };
    private readonly Button[] suggestionSlots = new Button[3];
    private readonly System.Windows.Forms.Timer tapTimer = new() { Interval = 300 };
    private readonly System.Windows.Forms.Timer suggestionTimer = new() { Interval = 45 };
    private readonly List<Button> buttons = [];
    private KeyboardModel? model;
    private string? pendingKey;
    private Size displaySize = new(1024, 600);
    private Rectangle screen;
    private Rectangle keyArea;
    private Font? editorFont;
    private Font? keyFont;
    private Font? suggestionFont;
    private TrieModel? trie;
    private KeyboardNeighborMap? keyboard;
    private string? language;
    private CancellationTokenSource? search;
    private SuggestionText? visibleSuggestions;
    private readonly PinyinComposition pinyin = new();
    private readonly Label preedit = new() { TextAlign = ContentAlignment.MiddleCenter, BackColor = KeyboardColor, ForeColor = Ink, Visible = false, AccessibleName = "Pinyin spelling" };
    private readonly Button previousCandidates = new() { Text = "<", Visible = false, AccessibleName = "Previous Chinese candidates" };
    private readonly Button nextCandidates = new() { Text = ">", Visible = false, AccessibleName = "Next Chinese candidates" };
    private int candidatePage, compositionCaret, compositionSelectionLength;
    private string? fontLanguage;
    private bool rightToLeftTyping;
    private bool Chinese => language == "zh";
    public bool RightToLeftTyping
    {
        get => rightToLeftTyping;
        set
        {
            rightToLeftTyping = value;
            editor.RightToLeft = value ? RightToLeft.Yes : RightToLeft.No;
            editor.TextAlign = value ? HorizontalAlignment.Right : HorizontalAlignment.Left;
            foreach (var button in buttons) button.RightToLeft = editor.RightToLeft;
            foreach (var button in suggestionSlots) button.RightToLeft = editor.RightToLeft;
        }
    }
    internal string TextValue { get => editor.Text; set => editor.Text = value; }
    internal string CompositionText => pinyin.Spelling;
    internal string[] CandidateWords => suggestionSlots.Select(slot => slot.Text).ToArray();
    internal void PressKey(string key) => Dispatch(key);
    internal void TypeText(string value) => InsertText(value);
    internal void ChooseCandidate(int index) => ApplySuggestion(suggestionSlots[index].Text);
    internal void MoveCandidatePage(int delta)
    {
        if (pinyin.Spelling.Length > 0 && (editor.SelectionStart != compositionCaret || editor.SelectionLength != compositionSelectionLength)) { pinyin.Clear(); candidatePage = 0; QueueSuggestions(); return; }
        candidatePage = Math.Clamp(candidatePage + delta * 3, 0, Math.Max(0, (pinyin.CandidateCount - 1) / 3 * 3));
        ShowPinyin();
    }
    internal void RefreshSuggestionsNow() => RefreshSuggestions(this, EventArgs.Empty);
    internal TextBox EditorControl => editor;

    public KeyboardPreview()
    {
        SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        Controls.Add(editor);
        suggestionBar.RowStyles.Add(new(SizeType.Percent, 100));
        for (int index = 0; index < suggestionSlots.Length; index++)
        {
            suggestionBar.ColumnStyles.Add(new(SizeType.Percent, 100f / suggestionSlots.Length));
            var slot = new Button
            {
                Dock = DockStyle.Fill, Margin = new Padding(8, 0, 8, 0), AutoEllipsis = true,
                TextAlign = ContentAlignment.MiddleCenter, ForeColor = Ink, UseMnemonic = false,
                FlatStyle = FlatStyle.Flat, UseVisualStyleBackColor = false, BackColor = KeyboardColor,
                Padding = new(0), Enabled = false, AccessibleName = $"Suggestion {index + 1}",
            };
            slot.FlatAppearance.BorderSize = 0;
            slot.FlatAppearance.MouseOverBackColor = Color.FromArgb(231, 240, 255);
            slot.FlatAppearance.MouseDownBackColor = Color.FromArgb(199, 218, 247);
            slot.Click += (_, _) => ApplySuggestion(slot.Text);
            suggestionSlots[index] = slot;
            suggestionBar.Controls.Add(slot, index, 0);
        }
        suggestionBar.CellPaint += (_, e) =>
        {
            if (e.Column == suggestionSlots.Length - 1) return;
            int x = e.CellBounds.Right - 1;
            int inset = e.CellBounds.Height / 4;
            using var divider = new Pen(Color.FromArgb(180, 194, 214));
            e.Graphics.DrawLine(divider, x, e.CellBounds.Top + inset, x, e.CellBounds.Bottom - inset);
        };
        Controls.Add(suggestionBar);
        Controls.AddRange([preedit, previousCandidates, nextCandidates]);
        previousCandidates.Click += (_, _) => MoveCandidatePage(-1);
        nextCandidates.Click += (_, _) => MoveCandidatePage(1);
        tapTimer.Tick += (_, _) => FlushPendingKey();
        suggestionTimer.Tick += RefreshSuggestions;
        editor.TextChanged += (_, _) => { pinyin.Clear(); candidatePage = 0; QueueSuggestions(); };
        editor.KeyDown += (_, e) => { if (e.KeyCode == Keys.Escape && pinyin.Spelling.Length > 0) { pinyin.Clear(); candidatePage = 0; QueueSuggestions(); e.SuppressKeyPress = true; } };
        editor.KeyPress += CompleteTypedWord;
        editor.KeyUp += (_, _) => QueueSuggestions();
        editor.MouseUp += (_, _) => QueueSuggestions();
    }

    public decimal AutocorrectThreshold { get; set; } = DefaultAutocorrectThreshold;

    public Size DisplaySize
    {
        get => displaySize;
        set
        {
            if (displaySize == value) return;
            displaySize = value;
            PerformLayout();
        }
    }

    public void SetKeyboard(KeyboardDefinition definition)
    {
        SuspendLayout();
        try {
        tapTimer.Stop();
        pendingKey = null;
        pinyin.Clear(); candidatePage = 0;
        model = new KeyboardModel(definition);
        keyboard = new KeyboardNeighborMap(definition);
        language = definition.Id.Split('-', 2)[0].ToLowerInvariant();
        RightToLeftTyping = language is "ar" or "ur";
        preedit.Visible = previousCandidates.Visible = nextCandidates.Visible = Chinese;
        RefreshKeys();
        suggestionBar.Visible = true;
        QueueSuggestions();
        PerformLayout();
        } finally { ResumeLayout(performLayout: true); }
    }

    public void SetTrie(TrieModel definition)
    {
        trie = definition;
        QueueSuggestions();
    }

    private void SetSuggestions(IReadOnlyList<string> suggestions)
    {
        for (int index = 0; index < suggestionSlots.Length; index++)
        {
            var slot = suggestionSlots[index];
            slot.Text = index < suggestions.Count ? suggestions[index] : "";
            slot.Enabled = slot.Text.Length != 0;
            slot.AccessibleName = slot.Enabled ? $"Suggestion {index + 1}: {slot.Text}" : $"Suggestion {index + 1}";
        }
    }

    private void QueueSuggestions()
    {
        suggestionTimer.Stop();
        search?.Cancel();
        search = null;
        visibleSuggestions = null;
        SetSuggestions([]);
        if (pinyin.Spelling.Length > 0 && (editor.SelectionStart != compositionCaret || editor.SelectionLength != compositionSelectionLength)) { pinyin.Clear(); candidatePage = 0; }
        if (Chinese) { ShowPinyin(); if (pinyin.Spelling.Length > 0) return; }
        if (trie is not null && model is not null) suggestionTimer.Start();
    }

    private async void RefreshSuggestions(object? sender, EventArgs args)
    {
        suggestionTimer.Stop();
        if (trie is not { } definition) return;
        string? selectedLanguage = language;
        var input = SuggestionText.Capture(editor.Text, editor.SelectionStart, editor.SelectionLength, selectedLanguage);
        var neighborMap = keyboard;
        using var cancellation = new CancellationTokenSource();
        search = cancellation;
        try
        {
            var words = await Task.Run(() => TrieSuggestions.Predict(definition, input.Context, input.Prefix,
                cancellation.Token, neighborMap, selectedLanguage), cancellation.Token);
            if (search != cancellation || IsDisposed) return;
            if (!MatchesEditor(input)) { QueueSuggestions(); return; }
            SetSuggestions(words);
            visibleSuggestions = input;
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested) { }
        finally { if (search == cancellation) search = null; }
    }

    private bool MatchesEditor(SuggestionText input) =>
        input.Caret == editor.SelectionStart && input.SelectionLength == editor.SelectionLength && input.Text == editor.Text;

    private void ApplySuggestion(string word)
    {
        if (Chinese && pinyin.Spelling.Length > 0) { CommitPinyin(word); return; }
        if (word.Length == 0 || visibleSuggestions is not { } input) return;
        if (!MatchesEditor(input)) { QueueSuggestions(); return; }

        FlushPendingKey();
        editor.Focus();
        editor.Select(input.Start, input.End - input.Start);
        editor.SelectedText = input.End == input.Text.Length && !Chinese ? word + " " : word;
        if (input.End < input.Text.Length && input.Text[input.End] is ' ' or '\t')
            editor.SelectionStart++;
        if (model is { Shift: ShiftMode.OneShot })
        {
            model.CommitText();
            RefreshKeys();
        }
        editor.ScrollToCaret();
        QueueSuggestions();
    }

    private void RefreshKeys()
    {
        SuspendLayout();
        try
        {
            int index = 0;
            foreach (var row in model!.Rows)
            {
                foreach (var key in row.Keys)
                {
                    var button = index < buttons.Count ? buttons[index] : AddKeyButton();
                    bool shift = key.Action.Kind == KeyboardActionKind.Shift;
                    bool locked = shift && model.Shift == ShiftMode.Locked;
                    bool highlighted = shift && model.Shift != ShiftMode.Off || key.Action.Kind == KeyboardActionKind.Enter;
                    bool insert = key.Action.Kind == KeyboardActionKind.Insert;
                    button.Tag = key;
                    button.Text = locked ? "⇪" : key.Label;
                    button.RightToLeft = RightToLeftTyping ? RightToLeft.Yes : RightToLeft.No;
                    button.AccessibleName = key.Action.Kind switch
                    {
                        KeyboardActionKind.Shift => locked ? "Shift locked" : "Shift",
                        KeyboardActionKind.Backspace => "Backspace",
                        KeyboardActionKind.Enter => "Enter",
                        _ => key.Label,
                    };
                    button.BackColor = highlighted ? Accent : insert ? Color.White : Color.FromArgb(179, 195, 216);
                    button.ForeColor = highlighted ? Color.White : Ink;
                    button.FlatAppearance.MouseOverBackColor = highlighted ? Color.FromArgb(36, 77, 148) : Color.FromArgb(231, 240, 255);
                    button.FlatAppearance.MouseDownBackColor = highlighted ? Color.FromArgb(30, 62, 119) : Color.FromArgb(199, 218, 247);
                    button.Visible = true;
                    index++;
                }
            }
            for (; index < buttons.Count; index++) buttons[index].Visible = false;
        }
        finally { ResumeLayout(performLayout: true); }
    }

    private Button AddKeyButton()
    {
        var button = new Button
        {
            FlatStyle = FlatStyle.Flat, UseVisualStyleBackColor = false, UseMnemonic = false,
            Padding = new(0), Margin = new(0), Font = keyFont ?? Font, TextAlign = ContentAlignment.MiddleCenter,
        };
        button.FlatAppearance.BorderSize = 0;
        button.Click += (_, _) => ActivateKey((KeyboardKey)button.Tag!);
        buttons.Add(button);
        Controls.Add(button);
        return button;
    }

    private void ActivateKey(KeyboardKey key)
    {
        if (pendingKey == key.Id)
        {
            tapTimer.Stop();
            pendingKey = null;
            Dispatch(key.Id, doubleTap: true);
            return;
        }

        // Apply a pending Shift tap before resolving the next key against the new layout.
        FlushPendingKey();
        var current = model!.Rows.SelectMany(row => row.Keys).FirstOrDefault(candidate => candidate.Id == key.Id);
        if (current is null) return;
        if (current.DoubleTapAction is null)
        {
            Dispatch(current.Id);
            return;
        }

        pendingKey = current.Id;
        tapTimer.Start();
    }

    private void FlushPendingKey()
    {
        if (pendingKey is not { } key) return;
        tapTimer.Stop();
        pendingKey = null;
        Dispatch(key);
    }

    private void Dispatch(string key, bool doubleTap = false)
    {
        if (model is null) return;
        int selectionStart = editor.SelectionStart;
        int selectionLength = editor.SelectionLength;
        editor.Focus();
        editor.Select(selectionStart, selectionLength);

        string layout = model.ActiveLayoutId;
        var shift = model.Shift;
        var input = model.Press(key, doubleTap);
        switch (input.Kind)
        {
            case KeyboardInputKind.Insert:
                InsertText(input.Text!);
                break;
            case KeyboardInputKind.Enter:
                if (!HandlePinyin(Environment.NewLine, enter: true)) InsertText(Environment.NewLine);
                break;
            case KeyboardInputKind.Backspace:
                if (!HandlePinyin(null, backspace: true)) Backspace();
                break;
        }
        if (layout != model.ActiveLayoutId || shift != model.Shift) RefreshKeys();
    }

    private void CompleteTypedWord(object? sender, KeyPressEventArgs e)
    {
        if (e.Handled) return;
        if (HandlePinyin(e.KeyChar.ToString(), backspace: e.KeyChar == '\b', enter: e.KeyChar == '\r')) { e.Handled = true; return; }
        if (e.KeyChar == '\r') e.Handled = TryCompleteWord(Environment.NewLine);
        else if (!char.IsControl(e.KeyChar)) e.Handled = TryCompleteWord(e.KeyChar.ToString());
    }

    private void InsertText(string text)
    {
        if (HandlePinyin(text)) return;
        if (!TryCompleteWord(text)) editor.SelectedText = text;
    }

    private void ShowPinyin()
    {
        preedit.Text = pinyin.Spelling.Length == 0 ? "Type Pinyin, then choose a Chinese character" : "Pinyin: " + pinyin.Spelling;
        if (candidatePage >= pinyin.CandidateCount) candidatePage = 0;
        SetSuggestions(Enumerable.Range(candidatePage, Math.Min(3, Math.Max(0, pinyin.CandidateCount - candidatePage)))
            .Select(index => pinyin.Candidate(index)).ToArray());
        previousCandidates.Enabled = candidatePage > 0;
        nextCandidates.Enabled = candidatePage + 3 < pinyin.CandidateCount;
    }
    private void CommitPinyin(string? word = null)
    {
        if (pinyin.Spelling.Length == 0) return;
        if (editor.SelectionStart != compositionCaret || editor.SelectionLength != compositionSelectionLength) { pinyin.Clear(); candidatePage = 0; QueueSuggestions(); return; }
        string accepted = string.IsNullOrEmpty(word) ? pinyin.CandidateCount > 0 ? pinyin.Candidate(candidatePage) : pinyin.Spelling : word;
        int caret = compositionCaret, length = compositionSelectionLength;
        pinyin.Clear(); candidatePage = 0;
        editor.Select(caret, length); editor.SelectedText = accepted; editor.ScrollToCaret();
        QueueSuggestions();
    }
    private bool HandlePinyin(string? value, bool backspace = false, bool enter = false)
    {
        if (!Chinese) return false;
        if (pinyin.Spelling.Length > 0 && (editor.SelectionStart != compositionCaret || editor.SelectionLength != compositionSelectionLength)) { pinyin.Clear(); candidatePage = 0; }
        if (backspace) { bool consumed = pinyin.Backspace(); if (consumed) { candidatePage = 0; QueueSuggestions(); } return consumed; }
        if (value is { Length: 1 } && char.IsAsciiLetter(value[0])) {
            if (pinyin.Spelling.Length == 0) { compositionCaret = editor.SelectionStart; compositionSelectionLength = editor.SelectionLength; }
            if (!pinyin.Append(value[0])) { CommitPinyin(); compositionCaret = editor.SelectionStart; compositionSelectionLength = editor.SelectionLength; pinyin.Append(value[0]); }
            candidatePage = 0; QueueSuggestions(); return true;
        }
        if (pinyin.Spelling.Length > 0) { CommitPinyin(); return enter || value == " "; }
        return false;
    }

    private bool TryCompleteWord(string delimiter)
    {
        if (trie is not { } definition || keyboard is not { } neighbors || editor.SelectionLength != 0 ||
            delimiter.Length == 0 || !delimiter.EnumerateRunes().All(IsWordDelimiter)) return false;

        var input = SuggestionText.Capture(editor.Text, editor.SelectionStart, selectionLength: 0, language: language);
        if (input.End != input.Caret || input.Start == input.End) return false;

        // An apostrophe can join a contraction or follow a word as punctuation.
        int wordEnd = input.End;
        if (input.Text[wordEnd - 1] is '\'' or '’') wordEnd--;
        if (wordEnd == input.Start) return false;

        string typed = input.Text[input.Start..wordEnd];
        var (previous, older) = TrieSuggestions.ReadContext(input.Text.AsSpan(0, input.Start), language);
        var candidate = TrieCorrections.BestAutocorrection(definition, neighbors, WordNormalization.NormalizeWord(typed, language),
            previous, older, CancellationToken.None);
        if (candidate is not { } correction || correction.Probability < (double)AutocorrectThreshold) return false;

        editor.Select(input.Start, input.End - input.Start);
        editor.SelectedText = TrieSuggestions.DisplayCorrection(correction.Word, typed, language) + input.Text[wordEnd..input.End] + delimiter;
        editor.ScrollToCaret();
        return true;
    }

    private static bool IsWordDelimiter(Rune rune) =>
        Rune.IsWhiteSpace(rune) || (Rune.IsPunctuation(rune) && rune.Value is not ('\'' or '’'));

    private void Backspace()
    {
        if (editor.SelectionLength > 0)
        {
            editor.SelectedText = "";
            return;
        }
        int caret = editor.SelectionStart;
        if (caret == 0) return;
        int[] boundaries = StringInfo.ParseCombiningCharacters(editor.Text);
        int position = Array.BinarySearch(boundaries, caret);
        if (position < 0) position = ~position;
        int previous = boundaries[position - 1];
        editor.Select(previous, caret - previous);
        editor.SelectedText = "";
    }

    protected override void OnLayout(LayoutEventArgs e)
    {
        if (IsDisposed || Disposing) return;
        base.OnLayout(e);
        if (editor is null) return;
        float scale = Math.Min((ClientSize.Width - 36f) / DisplaySize.Width, (ClientSize.Height - 28f) / DisplaySize.Height);
        if (scale <= 0) return;
        int width = (int)Math.Round(DisplaySize.Width * scale);
        int height = (int)Math.Round(DisplaySize.Height * scale);
        screen = new((ClientSize.Width - width) / 2, (ClientSize.Height - height) / 2, width, height);

        float keyboardHeight = model is null ? 0 : Math.Min(DisplaySize.Height * .46f, DisplaySize.Width * .45f);
        float keyboardTop = DisplaySize.Height - keyboardHeight;
        float suggestionHeight = model is null ? 0 : 46;
        float suggestionTop = keyboardTop - suggestionHeight;
        keyArea = BoundsInScreen(0, keyboardTop, DisplaySize.Width, keyboardHeight);
        editor.Bounds = BoundsInScreen(20, 20, DisplaySize.Width - 40, suggestionTop - 40);
        float compositionHeight = Chinese ? 24 : 0;
        if (Chinese) editor.Bounds = BoundsInScreen(20, 20, DisplaySize.Width - 40, suggestionTop - 40 - compositionHeight);
        preedit.Bounds = BoundsInScreen(36, suggestionTop - compositionHeight, DisplaySize.Width - 72, compositionHeight);
        previousCandidates.Bounds = BoundsInScreen(0, suggestionTop, 36, suggestionHeight);
        nextCandidates.Bounds = BoundsInScreen(DisplaySize.Width - 36, suggestionTop, 36, suggestionHeight);
        suggestionBar.Bounds = BoundsInScreen(Chinese ? 36 : 0, suggestionTop, DisplaySize.Width - (Chinese ? 72 : 0), suggestionHeight);
        SetFonts(scale);

        if (model is not null)
        {
            float unit = (DisplaySize.Width - 24) / model.Definition.Width;
            float rowHeight = (keyboardHeight - 16) / model.Rows.Count;
            float y = keyboardTop + 8;
            int index = 0;
            foreach (var row in model.Rows)
            {
                float x = 12 + row.Offset * unit;
                foreach (var key in row.Keys)
                {
                    float keyWidth = key.Width * unit;
                    float gap = Math.Min(6, keyWidth * .12f);
                    buttons[index++].Bounds = BoundsInScreen(x + gap / 2, y + 3, keyWidth - gap, rowHeight - 6);
                    x += keyWidth;
                }
                y += rowHeight;
            }
        }
        Invalidate();

        Rectangle BoundsInScreen(float x, float y, float w, float h) => Rectangle.FromLTRB(
            screen.Left + (int)Math.Round(x * scale), screen.Top + (int)Math.Round(y * scale),
            screen.Left + (int)Math.Round((x + w) * scale), screen.Top + (int)Math.Round((y + h) * scale));
    }

    private void SetFonts(float scale)
    {
        float size = Math.Max(8, (float)Math.Round(25 * scale));
        if (editorFont?.Size == size && fontLanguage == language) return;
        fontLanguage = language;
        Font? oldEditor = editorFont;
        Font? oldKey = keyFont;
        Font? oldSuggestion = suggestionFont;
        string family = language switch { "hi" or "bn" => "Nirmala UI", "zh" => "Microsoft YaHei UI", "ur" => "Urdu Typesetting", _ => Font.FontFamily.Name };
        editorFont = ReuseEqual(oldEditor, new Font(family, size, GraphicsUnit.Pixel));
        keyFont = ReuseEqual(oldKey, new Font(family, size, FontStyle.Bold, GraphicsUnit.Pixel));
        suggestionFont = ReuseEqual(oldSuggestion, new Font(family, Math.Max(8, size * .76f), GraphicsUnit.Pixel));
        preedit.Font = suggestionFont;
        editor.Font = editorFont;
        suggestionBar.Font = suggestionFont;
        foreach (var button in buttons) button.Font = keyFont;
        if (!ReferenceEquals(oldEditor, editorFont)) oldEditor?.Dispose();
        if (!ReferenceEquals(oldKey, keyFont)) oldKey?.Dispose();
        if (!ReferenceEquals(oldSuggestion, suggestionFont)) oldSuggestion?.Dispose();
    }

    private static Font ReuseEqual(Font? current, Font next)
    {
        // WinForms can retain its old Font reference when a replacement compares
        // equal. Reuse that owned instance so a language change cannot dispose it.
        if (current is not null && current.Equals(next)) { next.Dispose(); return current; }
        return next;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        using var page = new SolidBrush(ScreenColor);
        using var deck = new SolidBrush(KeyboardColor);
        e.Graphics.FillRectangle(page, screen);
        e.Graphics.FillRectangle(deck, keyArea);
    }

    protected override void Dispose(bool disposing)
    {
        if (!disposing) { base.Dispose(false); return; }
        tapTimer.Dispose();
        suggestionTimer.Dispose();
        search?.Cancel();
        search = null;
        base.Dispose(true);
        editorFont?.Dispose();
        keyFont?.Dispose();
        suggestionFont?.Dispose();
    }
}
