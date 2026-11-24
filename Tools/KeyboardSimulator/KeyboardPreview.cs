using System.Globalization;
using System.Text;
using NgramTrie;
using NgramTrie.Reader;

namespace KeyboardSimulator;

internal sealed class KeyboardPreview : Control, IMessageFilter
{
    public const decimal DefaultAutocorrectThreshold = 0.3m;
    private const int VariantHoldMilliseconds = 1000;

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
    private readonly System.Windows.Forms.Timer holdTimer = new() { Interval = VariantHoldMilliseconds };
    private readonly System.Windows.Forms.Timer suggestionTimer = new() { Interval = 45 };
    private readonly FlowLayoutPanel variantPopup = new()
    {
        Visible = false, AutoScroll = true, WrapContents = true,
        FlowDirection = FlowDirection.LeftToRight, Padding = new(4),
        BackColor = KeyboardColor, BorderStyle = BorderStyle.FixedSingle,
        AccessibleName = "Character variants",
    };
    private readonly List<Button> buttons = [];
    private Button? heldButton;
    private KeyboardKey? heldKey;
    private string? heldLayout;
    private Button? suppressClickButton;
    private Button? variantDragSource;
    private Button? highlightedVariant;
    private readonly List<Control> variantAncestors = [];
    private Form? popupForm;
    private bool filteringPopupMessages;
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
        Controls.Add(variantPopup);
        previousCandidates.Click += (_, _) => MoveCandidatePage(-1);
        nextCandidates.Click += (_, _) => MoveCandidatePage(1);
        tapTimer.Tick += (_, _) => FlushPendingKey();
        holdTimer.Tick += (_, _) => OpenVariants();
        suggestionTimer.Tick += RefreshSuggestions;
        editor.TextChanged += (_, _) => { CancelVariantInteraction(); pinyin.Clear(); candidatePage = 0; QueueSuggestions(); };
        editor.KeyDown += (_, e) => { CancelVariantInteraction(); if (e.KeyCode == Keys.Escape && pinyin.Spelling.Length > 0) { pinyin.Clear(); candidatePage = 0; QueueSuggestions(); e.SuppressKeyPress = true; } };
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
            CancelVariantInteraction();
            displaySize = value;
            PerformLayout();
        }
    }

    public void SetKeyboard(KeyboardDefinition definition)
    {
        CancelVariantInteraction();
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
        CancelVariantInteraction();
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
        CancelVariantInteraction();
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
        var button = new VariantKeyButton
        {
            FlatStyle = FlatStyle.Flat, UseVisualStyleBackColor = false, UseMnemonic = false,
            Padding = new(0), Margin = new(0), Font = keyFont ?? Font, TextAlign = ContentAlignment.MiddleCenter,
        };
        button.FlatAppearance.BorderSize = 0;
        button.MouseDown += (_, e) => BeginKeyHold(button, e);
        button.MouseMove += (_, e) =>
        {
            if (variantDragSource == button) HighlightVariant(VariantAt(button.PointToScreen(e.Location)));
        };
        button.MouseUp += (_, e) =>
        {
            if (e.Button != MouseButtons.Left) return;
            CompleteVariantDrag(button.PointToScreen(e.Location));
            CancelKeyHold();
        };
        button.MouseLeave += (_, _) => { if (heldButton == button) CancelKeyHold(suppressClick: true); };
        button.MouseCaptureChanged += (_, _) =>
        {
            if (variantDragSource == button && !button.Capture)
            {
                if ((MouseButtons & MouseButtons.Left) != 0) CancelVariantInteraction();
                else if (IsHandleCreated && !IsDisposed)
                {
                    // Some Button implementations release capture before MouseUp.
                    // Give that release message a chance to finish the selection.
                    BeginInvoke((Action)(() =>
                    {
                        if (variantDragSource == button && !button.Capture) CancelVariantInteraction();
                    }));
                }
            }
            if (heldButton == button && !button.Capture && MouseButtons != MouseButtons.None)
                CancelKeyHold(suppressClick: true);
        };
        button.Click += (_, _) =>
        {
            if (suppressClickButton == button) { suppressClickButton = null; return; }
            ActivateKey((KeyboardKey)button.Tag!);
        };
        buttons.Add(button);
        Controls.Add(button);
        return button;
    }

    private sealed class VariantKeyButton : Button
    {
        protected override void WndProc(ref Message message)
        {
            // Touch holds belong to the keyboard, rather than Windows' right-click gesture.
            if (message.Msg == 0x02CC)
            {
                message.Result = (IntPtr)1; // TABLET_DISABLE_PRESSANDHOLD
                return;
            }
            base.WndProc(ref message);
        }
    }

    private void BeginKeyHold(Button button, MouseEventArgs e)
    {
        if (e.Button != MouseButtons.Left) return;
        CancelVariantInteraction();
        suppressClickButton = null;
        if (model is null || button.Tag is not KeyboardKey key || key.Action.Kind != KeyboardActionKind.Insert)
            return;

        // Resolve a pending Shift before choosing the active key's variant list.
        FlushPendingKey(preserveKeyFocus: true);
        var current = model.Rows.SelectMany(row => row.Keys).FirstOrDefault(candidate => candidate.Id == key.Id);
        if (current is not { Variants.Count: > 0 }) return;
        heldButton = button;
        heldKey = current;
        heldLayout = model.ActiveLayoutId;
        holdTimer.Start();
    }

    private void CancelKeyHold(bool suppressClick = false)
    {
        holdTimer.Stop();
        if (suppressClick && heldButton is not null) suppressClickButton = heldButton;
        heldButton = null;
        heldKey = null;
        heldLayout = null;
    }

    private void OpenVariants()
    {
        if (heldButton is not { } source || heldKey is not { } key || heldLayout is not { } layout ||
            model?.ActiveLayoutId != layout || !source.Visible || !Enabled)
        {
            CancelKeyHold();
            return;
        }
        CancelKeyHold();
        suppressClickButton = source;
        var available = Rectangle.Intersect(screen, ClientRectangle);
        if (available.Width <= 10 || available.Height <= 10) return;

        variantPopup.SuspendLayout();
        try
        {
            ClearVariantButtons();
            variantPopup.AutoScrollMinSize = Size.Empty;
            variantPopup.AutoScrollPosition = Point.Empty;
            const int gap = 4;
            const int choiceMargin = 3;
            int paddingWidth = variantPopup.Padding.Horizontal + 2;
            int paddingHeight = variantPopup.Padding.Vertical + 2;
            bool openRight = source.Left + source.Width / 2 <= available.Left + available.Width / 2;
            int sideWidth = openRight ? available.Right - source.Right - gap : source.Left - gap - available.Left;
            int minimumWidth = Math.Min(Math.Max(source.Width, 36) + choiceMargin * 2 + paddingWidth, available.Width);
            bool besideKey = sideWidth >= minimumWidth;
            int maximumWidth = besideKey ? sideWidth : available.Width;
            int buttonHeight = Math.Min(Math.Max(source.Height, 32), Math.Max(1, available.Height - 10));
            int labelWidth = key.Variants.Max(text => TextRenderer.MeasureText(text, keyFont ?? Font,
                Size.Empty, TextFormatFlags.NoPadding).Width);
            int buttonWidth = Math.Clamp(Math.Max(Math.Max(source.Width, 36), labelWidth + 20),
                1, Math.Max(1, maximumWidth - paddingWidth - choiceMargin * 2));
            int columns = Math.Max(1, Math.Min(key.Variants.Count, (maximumWidth - paddingWidth) / (buttonWidth + choiceMargin * 2)));
            int rows = (key.Variants.Count - 1) / columns + 1;
            long fullHeight = (long)rows * (buttonHeight + choiceMargin * 2) + paddingHeight;
            bool scroll = fullHeight > available.Height;
            int scrollbarWidth = scroll ? SystemInformation.VerticalScrollBarWidth : 0;
            if (scroll)
            {
                buttonWidth = Math.Min(buttonWidth, Math.Max(1, maximumWidth - paddingWidth - choiceMargin * 2 - scrollbarWidth));
                columns = Math.Max(1, Math.Min(key.Variants.Count,
                    (maximumWidth - paddingWidth - scrollbarWidth) / (buttonWidth + choiceMargin * 2)));
                rows = (key.Variants.Count - 1) / columns + 1;
                fullHeight = (long)rows * (buttonHeight + choiceMargin * 2) + paddingHeight;
            }
            int width = Math.Min(maximumWidth, columns * (buttonWidth + choiceMargin * 2) + paddingWidth + scrollbarWidth);
            int height = (int)Math.Min(fullHeight, available.Height);
            foreach (string text in key.Variants)
            {
                var choice = new Button
                {
                    Text = text, AccessibleName = $"Insert {text}", AutoEllipsis = true,
                    UseMnemonic = false, FlatStyle = FlatStyle.Flat, UseVisualStyleBackColor = false,
                    BackColor = Color.White, ForeColor = Ink, Font = keyFont ?? Font,
                    RightToLeft = editor.RightToLeft, Margin = new(choiceMargin),
                    Size = new(buttonWidth, buttonHeight), TabStop = true,
                };
                choice.FlatAppearance.BorderSize = 0;
                choice.FlatAppearance.MouseOverBackColor = Color.FromArgb(231, 240, 255);
                choice.FlatAppearance.MouseDownBackColor = Color.FromArgb(199, 218, 247);
                choice.Click += (_, _) => InsertVariant(key, layout, text);
                variantPopup.Controls.Add(choice);
            }
            int left = openRight ? source.Right + gap : source.Left - gap - width;
            left = Math.Clamp(left, available.Left, available.Right - width);
            int top = source.Top;
            if (!besideKey)
            {
                top = source.Top - height - gap;
                if (top < available.Top) top = source.Bottom + gap;
            }
            top = Math.Clamp(top, available.Top, available.Bottom - height);
            variantPopup.Bounds = new(left, top, width, height);
            variantPopup.AutoScrollMinSize = new(0, (int)Math.Min(fullHeight - 2, int.MaxValue));
        }
        finally { variantPopup.ResumeLayout(performLayout: true); }
        variantPopup.Visible = true;
        variantPopup.BringToFront();
        variantDragSource = source;
        Application.AddMessageFilter(this);
        filteringPopupMessages = true;
        popupForm = FindForm();
        if (popupForm is not null) popupForm.Deactivate += DismissVariants;
        for (Control? ancestor = Parent; ancestor is not null; ancestor = ancestor.Parent)
        {
            ancestor.VisibleChanged += DismissVariants;
            ancestor.EnabledChanged += DismissVariants;
            ancestor.ParentChanged += DismissVariants;
            variantAncestors.Add(ancestor);
        }
    }

    private Button? VariantAt(Point screenPoint)
    {
        if (!Visible || !Enabled || !variantPopup.Visible ||
            !variantPopup.ClientRectangle.Contains(variantPopup.PointToClient(screenPoint)))
            return null;
        return variantPopup.Controls.OfType<Button>().FirstOrDefault(choice =>
            choice.Visible && choice.ClientRectangle.Contains(choice.PointToClient(screenPoint)));
    }

    private void HighlightVariant(Button? choice)
    {
        if (highlightedVariant == choice) return;
        if (highlightedVariant is { } previous)
        {
            previous.BackColor = Color.White;
            previous.ForeColor = Ink;
            previous.FlatAppearance.MouseOverBackColor = Color.FromArgb(231, 240, 255);
            previous.FlatAppearance.MouseDownBackColor = Color.FromArgb(199, 218, 247);
        }
        highlightedVariant = choice;
        if (choice is null) return;
        choice.BackColor = Accent;
        choice.ForeColor = Color.White;
        choice.FlatAppearance.MouseOverBackColor = Accent;
        choice.FlatAppearance.MouseDownBackColor = Accent;
    }

    private void CompleteVariantDrag(Point screenPoint)
    {
        if (variantDragSource is null) return;
        var choice = VariantAt(screenPoint);
        variantDragSource = null;
        HighlightVariant(null);
        choice?.PerformClick();
    }

    private void InsertVariant(KeyboardKey source, string layout, string text)
    {
        bool current = Visible && Enabled && variantPopup.Visible && model?.ActiveLayoutId == layout &&
            model.Rows.SelectMany(row => row.Keys).Any(key => ReferenceEquals(key, source));
        CancelVariantInteraction();
        if (!current || model is null) return;

        editor.Focus();
        var shift = model.Shift;
        model.CommitText();
        InsertText(text);
        if (model.Shift != shift) RefreshKeys();
        editor.ScrollToCaret();
    }

    private void DismissVariants(object? sender, EventArgs args) => CancelVariantInteraction();

    private void ClearVariantButtons()
    {
        while (variantPopup.Controls.Count > 0)
        {
            var choice = variantPopup.Controls[0];
            variantPopup.Controls.RemoveAt(0);
            choice.Dispose();
        }
    }

    private void CancelVariantInteraction()
    {
        var source = variantDragSource;
        variantDragSource = null;
        HighlightVariant(null);
        CancelKeyHold(suppressClick: true);
        variantPopup.Visible = false;
        ClearVariantButtons();
        if (filteringPopupMessages)
        {
            Application.RemoveMessageFilter(this);
            filteringPopupMessages = false;
        }
        if (popupForm is not null)
        {
            popupForm.Deactivate -= DismissVariants;
            popupForm = null;
        }
        foreach (var ancestor in variantAncestors)
        {
            ancestor.VisibleChanged -= DismissVariants;
            ancestor.EnabledChanged -= DismissVariants;
            ancestor.ParentChanged -= DismissVariants;
        }
        variantAncestors.Clear();
        if (source?.Capture == true) source.Capture = false;
    }

    bool IMessageFilter.PreFilterMessage(ref Message message)
    {
        if (variantDragSource is not null && message.Msg is 0x0200 or 0x0202)
        {
            var target = FromHandle(message.HWnd);
            long position = message.LParam.ToInt64();
            var point = new Point((short)(position & 0xffff), (short)((position >> 16) & 0xffff));
            point = target is null ? MousePosition : target.PointToScreen(point);
            if (message.Msg == 0x0200) HighlightVariant(VariantAt(point));
            else CompleteVariantDrag(point);
        }
        if (message.Msg == 0x0100 && (Keys)message.WParam.ToInt32() == Keys.Escape)
        {
            CancelVariantInteraction();
            return true;
        }
        if (message.Msg is 0x0201 or 0x0204 or 0x0207 or 0x0246)
        {
            var target = FromHandle(message.HWnd);
            if (target is null || target != variantPopup && !variantPopup.Contains(target))
                CancelVariantInteraction();
        }
        return false;
    }

    private void ActivateKey(KeyboardKey key)
    {
        CancelVariantInteraction();
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

    private void FlushPendingKey(bool preserveKeyFocus = false)
    {
        if (pendingKey is not { } key) return;
        tapTimer.Stop();
        pendingKey = null;
        Dispatch(key, focusEditor: !preserveKeyFocus);
    }

    private void Dispatch(string key, bool doubleTap = false, bool focusEditor = true)
    {
        CancelVariantInteraction();
        if (model is null) return;
        int selectionStart = editor.SelectionStart;
        int selectionLength = editor.SelectionLength;
        if (focusEditor) editor.Focus();
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

    protected override void OnSizeChanged(EventArgs e)
    {
        CancelVariantInteraction();
        base.OnSizeChanged(e);
    }

    protected override void OnVisibleChanged(EventArgs e)
    {
        if (!Visible) CancelVariantInteraction();
        base.OnVisibleChanged(e);
    }

    protected override void OnEnabledChanged(EventArgs e)
    {
        if (!Enabled) CancelVariantInteraction();
        base.OnEnabledChanged(e);
    }

    protected override void OnParentChanged(EventArgs e)
    {
        CancelVariantInteraction();
        base.OnParentChanged(e);
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
        CancelVariantInteraction();
        tapTimer.Dispose();
        holdTimer.Dispose();
        suggestionTimer.Dispose();
        search?.Cancel();
        search = null;
        base.Dispose(true);
        editorFont?.Dispose();
        keyFont?.Dispose();
        suggestionFont?.Dispose();
    }
}
