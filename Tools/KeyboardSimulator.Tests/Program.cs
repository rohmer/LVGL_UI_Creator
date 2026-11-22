using KeyboardSimulator;
using System.Text;
using NgramTrie.Reader;

namespace KeyboardLanguageChecks;

internal static class Program
{
    private static void Check(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
    [STAThread]
    private static int Main(string[] args)
    {
        try {
            Application.SetUnhandledExceptionMode(UnhandledExceptionMode.ThrowException);
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            string profiles = args.Length > 0 ? args[0] : Path.Combine(AppContext.BaseDirectory, "Keyboards");
            foreach (var id in new[] { "zh-CN-pinyin", "ar-SA-arabic", "hi-IN-inscript", "bn-BD-inscript", "id-ID-qwerty", "ur-PK-phonetic", "ru-RU-jcuken" }) {
                Console.WriteLine("Checking " + id);
                var definition = KeyboardDefinition.Load(Path.Combine(profiles, id + ".json"));
                using var preview = new KeyboardPreview { Size = new(1024, 720) };
                preview.SetKeyboard(definition);
                var model = new KeyboardModel(definition);
                foreach (var row in model.Rows) foreach (var key in row.Keys) if (key.Action.Kind == KeyboardActionKind.Insert && key.Action.Text is { } text) {
                    preview.TextValue = ""; preview.PressKey(key.Id);
                    if (!id.StartsWith("zh-")) Check(preview.TextValue == text, id + ": key inserted incorrect logical Unicode: " + key.Id);
                }
                bool rtl = id.StartsWith("ar-") || id.StartsWith("ur-");
                Check(preview.RightToLeftTyping == rtl, id + ": wrong default direction");
                const string mixed = "ABC العربية اُردُو हिन्दी বাংলা 中文 Русский 123";
                preview.TextValue = mixed; preview.RightToLeftTyping = !rtl;
                Check(preview.TextValue == mixed && preview.EditorControl.RightToLeft == (!rtl ? RightToLeft.Yes : RightToLeft.No), id + ": direction modified stored text");
                preview.RightToLeftTyping = rtl;
                Check(preview.TextValue == mixed, id + ": restoring direction modified stored text");
            }
            using (var preview = new KeyboardPreview { Size = new(1024, 720) }) {
                preview.SetKeyboard(KeyboardDefinition.Load(Path.Combine(profiles, "zh-CN-pinyin.json")));
                preview.PressKey("n"); preview.PressKey("i");
                Check(preview.TextValue == "" && preview.CompositionText == "ni", "Pinyin spelling leaked into committed text");
                Check(preview.CandidateWords.Any(word => word == "你"), "Pinyin ni did not offer 你");
                preview.ChooseCandidate(Array.IndexOf(preview.CandidateWords, "你"));
                Check(preview.TextValue == "你" && preview.CompositionText == "", "Chinese candidate was not committed");
                preview.PressKey("h"); preview.PressKey("a"); preview.PressKey("o"); preview.PressKey("space");
                Check(preview.TextValue == "你好", "Space did not commit the first Pinyin character without a Latin space");
                preview.PressKey("n"); preview.PressKey("backspace");
                Check(preview.TextValue == "你好" && preview.CompositionText == "", "Backspace during composition deleted committed text");
                preview.TextValue = ""; preview.PressKey("s"); preview.PressKey("h"); preview.PressKey("i");
                preview.MoveCandidatePage(1); string pageCandidate = preview.CandidateWords[0];
                Check(pageCandidate.Length > 0 && pageCandidate != "是", "Paging did not change Pinyin candidates");
                preview.PressKey("space"); Check(preview.TextValue == pageCandidate, "Space ignored the visible candidate page");
                preview.PressKey("n"); preview.PressKey("i"); preview.EditorControl.SelectionStart = 0;
                preview.PressKey("h"); Check(preview.CompositionText == "h" && preview.TextValue == pageCandidate, "Caret change retained stale Pinyin composition");
                preview.PressKey("backspace"); preview.TextValue = "你好";
                preview.SetKeyboard(KeyboardDefinition.Load(Path.Combine(profiles, "hi-IN-inscript.json")));
                Check(preview.TextValue == "你好" && preview.CompositionText == "", "Language switch changed committed content");
                preview.TextValue = "";
                foreach (var key in new[] { "y", "i", "r", "semicolon", "e" }) preview.PressKey(key);
                Check(preview.TextValue == "बगीचा", "Hindi InScript vowel signs were not stored in logical order");
            }
            var chinese = SuggestionText.Capture("你好", 2, 0, "zh");
            Check(chinese.Prefix == "" && chinese.Context == "你好", "Committed Han became a multi-character word prefix");
            Check(TrieSuggestions.ReadContext(chinese.Context.AsSpan(), "zh") == ("好", "你"), "Han n-gram context did not preserve separate characters");
            Check(TrieSuggestions.ReadContext("你好world".AsSpan(), "zh") == ("world", "好"), "Mixed Latin/Han token context changed");
            Check(TrieSuggestions.ReadContext("你好。世".AsSpan(), "zh") == ("世", ""), "Chinese punctuation did not reset context");
            Check(TrieSuggestions.ReadContext("你𠀀".AsSpan(), "zh") == ("𠀀", "你"), "Supplementary Han split a surrogate pair");
            foreach (var (language, word) in new[] { ("hi", "बगीचा"), ("bn", "বাংলা"), ("ar", "عَرَبِيّ"), ("ur", "اُردُو"), ("hi", "क्‍ष") }) {
                var captured = SuggestionText.Capture(word, word.Length, 0, language);
                Check(captured.Prefix == word, language + ": combining signs/joiners were split from a word");
                Check(TrieSuggestions.ReadContext((word + " ").AsSpan(), language).Previous.Length > 0, language + ": context lost a script word");
            }
            if (args.Length > 1) {
                using var preview = new KeyboardPreview { Size = new(1024, 720) };
                preview.SetKeyboard(KeyboardDefinition.Load(Path.Combine(profiles, "zh-CN-pinyin.json")));
                preview.CreateControl(); preview.EditorControl.CreateControl();
                preview.SetTrie(TrieModel.Load(args[1])); preview.TextValue = "你好";
                preview.EditorControl.SelectionStart = 2; preview.RefreshSuggestionsNow();
                var deadline = Environment.TickCount64 + 10000;
                while (!preview.CandidateWords.Contains("世") && Environment.TickCount64 < deadline) { Application.DoEvents(); Thread.Sleep(10); }
                Check(preview.CandidateWords.Contains("世"), "A loaded Han trie did not supply contextual next-character suggestions");
                preview.ChooseCandidate(Array.IndexOf(preview.CandidateWords, "世"));
                Check(preview.TextValue == "你好世", "Accepting a Han trie suggestion inserted a space or replaced committed Han");
                Console.WriteLine("Loaded Mandarin trie context and suggestion acceptance passed.");
            }
            var pinyin = new PinyinComposition();
            Check(!pinyin.Backspace() && !pinyin.Append('1'), "Pinyin accepted an invalid edit");
            foreach (var letter in "ZHONG") Check(pinyin.Append(letter), "Pinyin rejected uppercase Latin input");
            Check(pinyin.CandidateCount > 0 && pinyin.Candidate(0) == "中", "Shared Pinyin dictionary differs from native candidates");
            Check(pinyin.Candidate(-1) == "" && pinyin.Candidate(999) == "", "Out-of-range Pinyin candidate did not remain bounded");
            Console.WriteLine("Seven language layouts, canonical script input, RTL toggling, Pinyin commit/backspace and Hindi vowel ordering passed.");
            return 0;
        } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
