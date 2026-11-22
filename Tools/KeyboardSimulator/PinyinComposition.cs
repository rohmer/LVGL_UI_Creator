using System.Text;
namespace KeyboardSimulator;
internal sealed class PinyinComposition
{
    private static readonly Dictionary<string, string> dictionary = Load();
    public string Spelling { get; private set; } = "";
    private string Candidates => dictionary.GetValueOrDefault(Spelling, "");
    public int CandidateCount => Candidates.EnumerateRunes().Count();
    public bool Append(char value) {
        if (value is >= 'A' and <= 'Z') value = (char)(value + 32);
        if (value is < 'a' or > 'z' || Spelling.Length >= 7) return false;
        Spelling += value; return true;
    }
    public bool Backspace() { if (Spelling.Length == 0) return false; Spelling = Spelling[..^1]; return true; }
    public void Clear() => Spelling = "";
    public string Candidate(int index) => index < 0 ? "" : Candidates.EnumerateRunes().Skip(index).Select(value => value.ToString()).FirstOrDefault() ?? "";
    private static Dictionary<string, string> Load() {
        using var stream = typeof(PinyinComposition).Assembly.GetManifestResourceStream("KeyboardSimulator.PinyinData.tsv")
            ?? throw new InvalidDataException("The Pinyin character dictionary is missing.");
        using var reader = new StreamReader(stream, new UTF8Encoding(false, true));
        var result = new Dictionary<string, string>(StringComparer.Ordinal);
        while (reader.ReadLine() is { } line) { var columns = line.Split('\t'); if (columns.Length != 2 || columns[0].Length == 0 || columns[1].Length == 0 || !result.TryAdd(columns[0], columns[1])) throw new InvalidDataException("Invalid Pinyin character dictionary."); }
        return result;
    }
}
