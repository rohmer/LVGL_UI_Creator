using System.Text.Json;
using System.Text.Json.Serialization;

namespace KeyboardSimulator;

internal enum KeyboardActionKind { Insert, SwitchLayout, Shift, ShiftLock, Backspace, Enter }
internal enum KeyboardInputKind { None, Insert, Backspace, Enter }
internal enum ShiftMode { Off, OneShot, Locked }

internal readonly record struct KeyboardInput(KeyboardInputKind Kind, string? Text = null);

internal sealed record KeyboardAction
{
    [JsonPropertyName("type")]
    public required KeyboardActionKind Kind { get; init; }
    public string? Text { get; init; }
    public string? Layout { get; init; }
}

internal sealed record KeyboardKey
{
    public required string Id { get; init; }
    public required string Label { get; init; }
    public float Width { get; init; } = 1;
    public required KeyboardAction Action { get; init; }
    public KeyboardAction? DoubleTapAction { get; init; }
}

internal sealed record KeyboardRow
{
    public float Offset { get; init; }
    public required IReadOnlyList<KeyboardKey> Keys { get; init; }
}

internal sealed record KeyboardLayout
{
    public string? ShiftedLayout { get; init; }
    public required IReadOnlyList<KeyboardRow> Rows { get; init; }
}

internal sealed record KeyboardDefinition
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        Converters = { new JsonStringEnumConverter<KeyboardActionKind>(JsonNamingPolicy.SnakeCaseLower, allowIntegerValues: false) },
    };

    [JsonPropertyName("$schema")]
    public string? Schema { get; init; }
    public required int Version { get; init; }
    public required string Id { get; init; }
    public required string InitialLayout { get; init; }
    public required float Width { get; init; }
    public required IReadOnlyDictionary<string, KeyboardLayout> Layouts { get; init; }

    public static KeyboardDefinition Load(string path)
    {
        using var file = File.OpenRead(path);
        try
        {
            var definition = JsonSerializer.Deserialize<KeyboardDefinition>(file, JsonOptions)
                ?? throw new InvalidDataException("The keyboard definition is empty.");
            definition.Validate();
            return definition;
        }
        catch (JsonException error)
        {
            throw new InvalidDataException($"{path}: {error.Message}", error);
        }
        catch (InvalidDataException error)
        {
            throw new InvalidDataException($"{path}: {error.Message}", error);
        }
    }

    private void Validate()
    {
        if (Version != 1) throw new InvalidDataException($"Unsupported keyboard version {Version}.");
        if (!IsId(Id, allowUppercase: true) || !float.IsFinite(Width) || Width <= 0)
            throw new InvalidDataException("A keyboard needs a valid ID and a positive width.");
        if (Layouts is null || Layouts.Count == 0)
            throw new InvalidDataException("A keyboard needs at least one layout.");

        var shiftTargets = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (name, layout) in Layouts)
        {
            if (!IsId(name) || layout is null)
                throw new InvalidDataException($"Invalid layout {name}.");
            if (layout.ShiftedLayout is not { } target) continue;
            if (!Layouts.TryGetValue(target, out var shifted) || shifted is null || shifted.ShiftedLayout is not null)
                throw new InvalidDataException($"Layout {name} has an invalid shifted layout {target}.");
            shiftTargets.Add(target);
        }

        if (InitialLayout is null || !Layouts.ContainsKey(InitialLayout) || shiftTargets.Contains(InitialLayout))
            throw new InvalidDataException("The initial layout must refer to a base layout.");

        foreach (var (name, layout) in Layouts)
        {
            if (layout.Rows is null || layout.Rows.Count == 0)
                throw new InvalidDataException($"Layout {name} has no rows.");
            var keyIds = new HashSet<string>(StringComparer.Ordinal);
            foreach (var row in layout.Rows)
            {
                if (row is null || !float.IsFinite(row.Offset) || row.Offset < 0 || row.Keys is null || row.Keys.Count == 0)
                    throw new InvalidDataException($"Layout {name} has an invalid row.");
                double right = row.Offset;
                foreach (var key in row.Keys)
                {
                    if (key is null || !IsId(key.Id) || !keyIds.Add(key.Id) || string.IsNullOrEmpty(key.Label) ||
                        !float.IsFinite(key.Width) || key.Width <= 0)
                        throw new InvalidDataException($"Layout {name} has an invalid or repeated key.");
                    right += key.Width;
                    ValidateAction(key.Action, key.Id, name);
                    if (key.DoubleTapAction is { } doubleTap)
                        ValidateAction(doubleTap, key.Id, name);
                }
                if (right > Width * (1.0 + 1e-6))
                    throw new InvalidDataException($"A row in layout {name} exceeds the keyboard width.");
            }
        }

        void ValidateAction(KeyboardAction? action, string key, string layout)
        {
            bool valid = action?.Kind switch
            {
                KeyboardActionKind.Insert => !string.IsNullOrEmpty(action.Text) && action.Layout is null,
                KeyboardActionKind.SwitchLayout => action.Text is null && action.Layout is { } target &&
                    Layouts.ContainsKey(target) && !shiftTargets.Contains(target),
                KeyboardActionKind.Shift or KeyboardActionKind.ShiftLock or KeyboardActionKind.Backspace or KeyboardActionKind.Enter
                    => action.Text is null && action.Layout is null,
                _ => false,
            };
            if (!valid) throw new InvalidDataException($"Key {key} in layout {layout} has an invalid action.");
        }
    }

    private static bool IsId(string? id, bool allowUppercase = false) =>
        id is { Length: > 0 } && char.IsAsciiLetterLower(id[0]) &&
        id.All(character => char.IsAsciiLetterLower(character) || char.IsAsciiDigit(character) ||
            character is '-' or '_' || (allowUppercase && char.IsAsciiLetterUpper(character)));
}

internal sealed class KeyboardModel(KeyboardDefinition definition)
{
    private string baseLayout = definition.InitialLayout;

    public KeyboardDefinition Definition { get; } = definition;
    public ShiftMode Shift { get; private set; }
    public string ActiveLayoutId => Shift == ShiftMode.Off ? baseLayout : Definition.Layouts[baseLayout].ShiftedLayout!;
    public IReadOnlyList<KeyboardRow> Rows => Definition.Layouts[ActiveLayoutId].Rows;

    public void CommitText()
    {
        if (Shift == ShiftMode.OneShot) Shift = ShiftMode.Off;
    }

    public KeyboardInput Press(string keyId, bool doubleTap = false)
    {
        var key = Rows.SelectMany(row => row.Keys).FirstOrDefault(key => key.Id == keyId)
            ?? throw new ArgumentException($"Key {keyId} is not in layout {ActiveLayoutId}.", nameof(keyId));
        var action = doubleTap ? key.DoubleTapAction ?? key.Action : key.Action;
        switch (action.Kind)
        {
            case KeyboardActionKind.Insert:
                CommitText();
                return new(KeyboardInputKind.Insert, action.Text);
            case KeyboardActionKind.Backspace:
                return new(KeyboardInputKind.Backspace);
            case KeyboardActionKind.Enter:
                return new(KeyboardInputKind.Enter);
            case KeyboardActionKind.SwitchLayout:
                baseLayout = action.Layout!;
                Shift = ShiftMode.Off;
                break;
            case KeyboardActionKind.Shift when Definition.Layouts[baseLayout].ShiftedLayout is not null:
                Shift = Shift == ShiftMode.Off ? ShiftMode.OneShot : ShiftMode.Off;
                break;
            case KeyboardActionKind.ShiftLock when Definition.Layouts[baseLayout].ShiftedLayout is not null:
                Shift = Shift == ShiftMode.Locked ? ShiftMode.Off : ShiftMode.Locked;
                break;
        }
        return default;
    }
}
