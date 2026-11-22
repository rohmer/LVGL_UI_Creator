using System.Text;

namespace KeyboardSimulator;

internal sealed class KeyboardNeighborMap
{
    public const double CorrectKeyProbability = 0.90;
    public const double NeighborSubstitutionTotal = 0.05;
    public const double NonNeighborSubstitutionTotal = 0.03;
    public const double InsertionProbability = 0.01;
    public const double DeletionProbability = 0.01;
    public const int QwertyAlphabetSize = 26;

    private const double EdgeTolerance = 1e-5;
    private static readonly double CorrectCost = -Math.Log(CorrectKeyProbability);
    private static readonly KeyProfile UnknownKey = new([], QwertyAlphabetSize);
    private readonly Dictionary<Rune, KeyProfile> profiles;
    private readonly AsciiKeyCost[] asciiCosts = new AsciiKeyCost[QwertyAlphabetSize];

    public KeyboardNeighborMap(KeyboardDefinition definition, string? layoutId = null)
    {
        var positions = definition.Id.Equals("en-US-qwerty", StringComparison.OrdinalIgnoreCase)
            ? ThesisQwertyPositions()
            : LayoutPositions(definition.Layouts[layoutId ?? definition.InitialLayout]);

        var neighbors = new Dictionary<Rune, HashSet<Rune>>();
        foreach (var position in positions)
            neighbors.TryAdd(position.Letter, []);
        for (int index = 0; index < positions.Count; index++)
        {
            var left = positions[index];
            for (int other = index + 1; other < positions.Count; other++)
            {
                var right = positions[other];
                if (left.Letter == right.Letter || !Touches(left, right)) continue;
                neighbors[left.Letter].Add(right.Letter);
                neighbors[right.Letter].Add(left.Letter);
            }
        }

        profiles = neighbors.ToDictionary(entry => entry.Key,
            entry => new KeyProfile(entry.Value.OrderBy(letter => letter.Value).ToArray(), neighbors.Count));

        for (int letter = 0; letter < asciiCosts.Length; letter++)
        {
            var profile = profiles.GetValueOrDefault(new Rune('a' + letter), UnknownKey);
            uint neighborMask = 0;
            foreach (var neighbor in profile.Neighbors)
            {
                int offset = neighbor.Value - 'a';
                if ((uint)offset < QwertyAlphabetSize) neighborMask |= 1u << offset;
            }
            asciiCosts[letter] = new(neighborMask, profile.NeighborCost - CorrectCost, profile.NonNeighborCost - CorrectCost);
        }
    }

    /// <summary>Returns the lowercase neighboring letters of the intended key.</summary>
    public IReadOnlyList<Rune> GetNeighbors(Rune intended) =>
        profiles.GetValueOrDefault(Rune.ToLowerInvariant(intended), UnknownKey).Neighbors;

    public bool AreNeighbors(Rune intended, Rune observed) =>
        profiles.GetValueOrDefault(Rune.ToLowerInvariant(intended), UnknownKey).Contains(Rune.ToLowerInvariant(observed));

    /// <summary>Returns the probability of observing the second key when the first was intended.</summary>
    public double SubstitutionProbability(Rune intended, Rune observed)
    {
        intended = Rune.ToLowerInvariant(intended);
        observed = Rune.ToLowerInvariant(observed);
        if (intended == observed) return CorrectKeyProbability;
        var profile = profiles.GetValueOrDefault(intended, UnknownKey);
        return profile.Contains(observed) ? profile.NeighborProbability : profile.NonNeighborProbability;
    }

    /// <summary>Returns the negative natural logarithm of the directed substitution probability.</summary>
    public double SubstitutionCost(Rune intended, Rune observed)
    {
        intended = Rune.ToLowerInvariant(intended);
        observed = Rune.ToLowerInvariant(observed);
        if (intended == observed) return CorrectCost;
        var profile = profiles.GetValueOrDefault(intended, UnknownKey);
        return profile.Contains(observed) ? profile.NeighborCost : profile.NonNeighborCost;
    }

    public double RelativeSubstitutionCost(Rune intended, Rune observed)
    {
        int source = intended.Value - 'a';
        int target = observed.Value - 'a';
        if ((uint)source >= QwertyAlphabetSize || (uint)target >= QwertyAlphabetSize)
            return SubstitutionCost(intended, observed) - CorrectCost;
        if (source == target) return 0;

        var costs = asciiCosts[source];
        return (costs.NeighborMask & (1u << target)) != 0 ? costs.Neighbor : costs.Other;
    }

    private static List<KeyPosition> ThesisQwertyPositions()
    {
        // Kirk (2018), figure 3.2 and table 3.1 use a left-aligned letter grid.
        // Keep this grid separate from the stagger used to draw the mobile keyboard.
        string[] rows = ["qwertyuiop", "asdfghjkl", "zxcvbnm"];
        var positions = new List<KeyPosition>(QwertyAlphabetSize);
        for (int row = 0; row < rows.Length; row++)
            for (int column = 0; column < rows[row].Length; column++)
                positions.Add(new(new Rune(rows[row][column]), row, column, column + 1));
        return positions;
    }

    private static List<KeyPosition> LayoutPositions(KeyboardLayout layout)
    {
        var positions = new List<KeyPosition>();
        for (int row = 0; row < layout.Rows.Count; row++)
        {
            var layoutRow = layout.Rows[row];
            double left = layoutRow.Offset;
            foreach (var key in layoutRow.Keys)
            {
                double right = left + key.Width;
                if (key.Action.Kind == KeyboardActionKind.Insert && key.Action.Text is { } text &&
                    Rune.TryGetRuneAt(text, 0, out var letter) && text.Length == letter.Utf16SequenceLength &&
                    Rune.IsLetter(letter))
                    positions.Add(new(Rune.ToLowerInvariant(letter), row, left, right));
                left = right;
            }
        }
        return positions;
    }

    private static bool Touches(KeyPosition left, KeyPosition right) => (right.Row - left.Row) switch
    {
        0 => Math.Abs(left.Right - right.Left) <= EdgeTolerance,
        1 => left.Left <= right.Right + EdgeTolerance && right.Left <= left.Right + EdgeTolerance,
        _ => false,
    };

    private sealed class KeyProfile
    {
        private readonly Rune[] neighbors;

        public KeyProfile(Rune[] neighbors, int alphabetSize)
        {
            this.neighbors = neighbors;
            Neighbors = Array.AsReadOnly(neighbors);
            NeighborProbability = NeighborSubstitutionTotal / Math.Max(1, neighbors.Length);
            NonNeighborProbability = NonNeighborSubstitutionTotal / Math.Max(1, alphabetSize - 1 - neighbors.Length);
            NeighborCost = -Math.Log(NeighborProbability);
            NonNeighborCost = -Math.Log(NonNeighborProbability);
        }

        public IReadOnlyList<Rune> Neighbors { get; }
        public double NeighborProbability { get; }
        public double NonNeighborProbability { get; }
        public double NeighborCost { get; }
        public double NonNeighborCost { get; }
        public bool Contains(Rune rune) => Array.IndexOf(neighbors, rune) >= 0;
    }

    private readonly record struct KeyPosition(Rune Letter, int Row, double Left, double Right);
    private readonly record struct AsciiKeyCost(uint NeighborMask, double Neighbor, double Other);
}
