extern alias CrawlerTools;
extern alias WikiTools;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Xml.Linq;
using NgramTrie;
using NgramTrie.UI;
using Crawler = CrawlerTools::CorpusCrawler;
using Wiki = WikiTools::WikipediaCorpus;
using SharpCompress.Compressors.BZip2;

internal static class Program
{
    private sealed record Case(string Code, string Text, string[][] Sentences, string Article);
    private static readonly Case[] Cases =
    [
        new("ar", "أهلاً عَالَم؟ أهلاً۔ س‍لام. ", [["أهلاً", "عَالَم"], ["أهلاً"], ["س‍لام"]], "هذه جملة عربية كاملة تتحدث عن القراءة والكتابة والتعلم في المدرسة."),
        new("hi", "नमस्ते दुनिया। नमस्ते॥ क्‍ष क्‌ष।", [["नमस्ते", "दुनिया"], ["नमस्ते"], ["क्‍ष", "क्‌ष"]], "यह एक पूरा हिंदी वाक्य है जिसमें हम शिक्षा और पुस्तकें पढ़ने के बारे में बात करते हैं।"),
        new("zh", "你好世界。你好！ LVGL9中文。𠀀𠀁。", [["你", "好", "世", "界"], ["你", "好"], ["lvgl9", "中", "文"], ["𠀀", "𠀁"]], "这是一个完整的中文句子，我们每天在学校学习和阅读书籍。"),
        new("bn", "নমস্কার পৃথিবী। ক্‍ষ ক্‌ষ॥", [["নমস্কার", "পৃথিবী"], ["ক্‍ষ", "ক্‌ষ"]], "এটি একটি সম্পূর্ণ বাংলা বাক্য যেখানে আমরা বিদ্যালয়ে পড়াশোনা এবং বই পড়ার কথা বলছি।"),
        new("id", "SELAMAT dunia. café café！", [["selamat", "dunia"], ["café", "café"]], "Ini adalah kalimat lengkap dalam bahasa Indonesia tentang membaca buku dan belajar di sekolah setiap hari."),
        new("ur", "اُردُو زَبان؟ اُردُو۔", [["اُردُو", "زَبان"], ["اُردُو"]], "یہ اردو زبان کا ایک مکمل جملہ ہے جس میں ہم اسکول میں پڑھنے اور کتابوں کے بارے میں بات کرتے ہیں۔"),
        new("ru", "ПРИВЕТ мир. ёлка ёлка？", [["привет", "мир"], ["ёлка", "ёлка"]], "Это полное предложение на русском языке о чтении книг и ежедневной учёбе в школе."),
    ];
    private static string root = "", work = "";
    private static void Check(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
    private static void Write(string path, string text) => File.WriteAllText(path, text, new UTF8Encoding(false));
    private static JsonObject Json(string path) => JsonNode.Parse(File.ReadAllText(path))!.AsObject();
    private static string Key(IEnumerable<string> tokens) => string.Join('\u001f', tokens);
    private static void Counts(JsonObject actual, string[][] sentences, string scenario)
    {
        string[] orders = ["unigrams", "bigrams", "trigrams"];
        for (int order = 0; order < 3; order++)
        {
            var expected = new Dictionary<string, long>();
            var totals = new Dictionary<string, long>();
            foreach (var sentence in sentences) for (int end = order; end < sentence.Length; end++)
            {
                string key = Key(sentence[(end - order)..(end + 1)]);
                expected[key] = expected.GetValueOrDefault(key) + 1;
            }
            foreach (var entry in expected)
            {
                var tokens = entry.Key.Split('\u001f'); string prefix = Key(tokens[..^1]);
                totals[prefix] = totals.GetValueOrDefault(prefix) + entry.Value;
            }
            var rows = actual[orders[order]]!.AsArray();
            Check(rows.Count == expected.Count, $"{scenario}: wrong order {order} count");
            foreach (var row in rows)
            {
                string[] tokens = row!["tokens"]!.AsArray().Select(value => value!.GetValue<string>()).ToArray();
                Check(expected.Remove(Key(tokens), out long count) && row["count"]!.GetValue<long>() == count, $"{scenario}: wrong Unicode n-gram");
                Check(Math.Abs(row["weight"]!.GetValue<double>() - (double)count / totals[Key(tokens[..^1])]) < 1e-12, $"{scenario}: wrong conditional weight");
            }
        }
    }
    private static string[] Arguments(string input, string prefix, string language, params string[] extra) =>
        ["--file", input, "--trie-output", prefix + ".ngtr", "--output", prefix + ".json", "--language", language, "--compact", "--no-cache", .. extra];
    private static void GoldenCases()
    {
        string fixtures = Path.Combine(root, "Tests", "Integration", "LanguageModels", "fixtures");
        foreach (string name in new[] { "full", "legacy", "budget128", "budget20", "limits", "zero", "folded", "es", "fr", "de", "el" })
        {
            bool listed = name is "es" or "fr" or "de" or "el";
            string prefix = Path.Combine(work, "golden-" + name);
            var args = Arguments(Path.Combine(fixtures, (listed ? name : name == "folded" ? "folded" : "ascii") + ".txt"), prefix, listed ? name : "en").ToList();
            if (!listed) args.Add("--no-wordlist");
            if (name == "legacy") args.Remove("--compact");
            if (name == "budget128") args.AddRange(["--trie-size", "128"]);
            if (name == "budget20") args.AddRange(["--trie-size", "20"]);
            if (name == "limits") args.AddRange(["--unigrams", "3", "--bigrams", "2", "--trigrams", "1"]);
            if (name == "zero") args.AddRange(["--unigrams", "0", "--bigrams", "0", "--trigrams", "0"]);
            Generator.Generate([.. args]);
            Check(File.ReadAllBytes(prefix + ".ngtr").SequenceEqual(File.ReadAllBytes(Path.Combine(fixtures, name + ".ngtr"))), $"Existing C# byte parity failed: {name}");
        }
        Console.WriteLine("11 existing C# NGTR golden cases passed");
    }
    private static void Languages()
    {
        var identities = new HashSet<string>();
        string sharedCacheInput = Path.Combine(work, "shared-language-cache.txt");
        foreach (var item in Cases)
        {
            string input = Path.Combine(work, item.Code + ".txt"), memory = Path.Combine(work, item.Code + "-memory"), disk = Path.Combine(work, item.Code + "-disk");
            Write(input, item.Text); Generator.Generate(Arguments(input, memory, item.Code)); Counts(Json(memory + ".json"), item.Sentences, item.Code);
            Generator.Generate(Arguments(input, disk, item.Code, "--low-memory", "--workers", "4"));
            Check(File.ReadAllBytes(memory + ".ngtr").SequenceEqual(File.ReadAllBytes(disk + ".ngtr")), item.Code + ": RAM/SQLite binary mismatch");
            Check(identities.Add(Corpus.CacheKey(sharedCacheInput, null, item.Code)), "New languages share an unfiltered cache key");
            Check(Corpus.CacheKey(sharedCacheInput, null, item.Code) != Corpus.CacheKey(sharedCacheInput, null), "A new tokenizer reused the English cache identity");
            string first = item.Sentences[0][0], last = item.Sentences[0][^1];
            string filter = Path.Combine(work, item.Code + "-words.txt"), filteredInput = Path.Combine(work, item.Code + "-filtered.txt"), filtered = Path.Combine(work, item.Code + "-filtered");
            Write(filter, first + "\n" + last + "\n"); Write(filteredInput, first + " unknownword " + last + ".");
            Generator.Generate(Arguments(filteredInput, filtered, item.Code, "--wordlist", filter));
            Counts(Json(filtered + ".json"), [[first], [last]], item.Code + " custom filter reset");
        }
        Write(sharedCacheInput, "你好 WORLD.");
        string cache = Path.Combine(work, "shared-count-cache");
        foreach (string language in new[] { "en", "zh", "zh" })
        {
            string prefix = Path.Combine(work, "cached-" + language);
            var args = Arguments(sharedCacheInput, prefix, language).Where(argument => argument != "--no-cache").ToList();
            args.AddRange(["--cache-dir", cache]);
            if (language == "en") args.Add("--no-wordlist");
            Generator.Generate([.. args]);
            Counts(Json(prefix + ".json"), language == "en" ? [["你好", "world"]] : [["你", "好", "world"]], "Cross-language shared cache " + language);
        }
        Check(Directory.GetFiles(cache, "*.grams.gz").Length == 2, "Language cache identities were not separate or a warm run created another identity");
        Console.WriteLine("Seven new languages: independent counts, marks/joiners, Han characters, RAM/disk and custom filter passed");
    }
    private static void Forms()
    {
        using var form = new GeneratorForm(new UiSettingsStore(Path.Combine(work, "ui-settings.json")));
        foreach (var item in Cases)
        {
            string file = Path.Combine(work, item.Code + ".txt");
            Check(form.UseCorpora([file], item.Code), item.Code + ": workbench corpus handoff rejected language");
            var inputs = Controls(form).OfType<TextBox>().Single(value => value.AcceptsReturn);
            Check(inputs.Lines.SequenceEqual([file]), "Corpus handoff widened a language-specific file into its mixed-language parent folder");
        }
        Check(!form.UseCorpora([Path.Combine(work, "en.txt")], "unsupported"), "Unsupported UI language was accepted");
        using var crawler = new Crawler.CrawlerForm();
        using var wiki = new Wiki.ExportForm();
        var crawlerChoices = Controls(crawler).OfType<ComboBox>().Single().Items.Cast<object>().Select(value => value.ToString()).ToHashSet();
        foreach (string language in new[] { "Arabic", "Hindi", "Chinese", "Bengali", "Indonesian", "Urdu", "Russian" }) Check(crawlerChoices.Contains(language), "Crawler dropdown omits " + language);
        var wikiChoices = Controls(wiki).OfType<ComboBox>().Single().Items.Cast<object>().Select(value => value.ToString()).ToArray();
        foreach (string language in new[] { "Arabic", "Hindi", "Mandarin Chinese", "Bengali", "Indonesian", "Urdu", "Russian" })
            Check(wikiChoices.Any(value => value == language + " (Unicode)"), "Wikipedia dropdown omits its Unicode profile: " + language);
        Console.WriteLine("Crawler/Wikipedia forms and seven workbench-to-generator language handoffs passed");
    }
    private static IEnumerable<Control> Controls(Control parent)
    {
        foreach (Control child in parent.Controls) { yield return child; foreach (var descendant in Controls(child)) yield return descendant; }
    }
    private static async Task SentencesAndWiki()
    {
        foreach (var item in Cases)
        {
            using var worker = await Crawler.OpenNlpSentences.StartAsync(item.Code, CancellationToken.None);
            var extracted = await worker.ExtractAsync([item.Article + " Unfinished trailing fragment", "123."], CancellationToken.None);
            Check(extracted.Length == 1 && extracted[0] == item.Article.Normalize(), item.Code + ": Unicode fallback dropped a sentence or kept an unfinished/numeric fragment");
            Check(Crawler.OpenNlpSentences.Describe(item.Code).Contains("no OpenNLP/POS"), item.Code + ": fallback was presented as a model");
            string dump = Path.Combine(work, item.Code + "wiki-test.xml.bz2"), output = Path.Combine(work, item.Code + "-wiki.txt");
            string xml = $"<mediawiki xmlns=\"http://www.mediawiki.org/xml/export-0.11/\" xml:lang=\"{item.Code}\"><siteinfo><dbname>{item.Code}wiki</dbname></siteinfo><page><title>Example</title><ns>0</ns><revision><timestamp>2026-01-01T00:00:00Z</timestamp><model>wikitext</model><format>text/x-wiki</format><text>{System.Security.SecurityElement.Escape(item.Article)}</text></revision></page></mediawiki>";
            using (var stream = File.Create(dump)) using (var compressed = BZip2Stream.Create(stream, SharpCompress.Compressors.CompressionMode.Compress, decompressConcatenated: false, leaveOpen: false)) compressed.Write(Encoding.UTF8.GetBytes(xml));
            var result = await Wiki.WikipediaExporter.ExportAsync(dump, output, null, false, null, CancellationToken.None);
            Check(result.LanguageCode == item.Code && result.SentencesWritten == 1 && File.ReadAllText(output).Trim() == item.Article.Normalize(), item.Code + ": Wikipedia auto-language BZ2 workflow failed");
        }
        Console.WriteLine("Seven explicit Unicode fallbacks and actual auto-language Wikipedia BZ2 exports passed");
    }
    private static async Task Crawl()
    {
        var item = Cases.Single(value => value.Code == "zh");
        using var server = new TcpListener(IPAddress.Loopback, 0); server.Start();
        int port = ((IPEndPoint)server.LocalEndpoint).Port;
        var serve = Task.Run(async () =>
        {
            using var client = await server.AcceptTcpClientAsync(); using var stream = client.GetStream();
            using var reader = new StreamReader(stream, leaveOpen: true);
            while (await reader.ReadLineAsync() is { Length: > 0 }) { }
            byte[] body = Encoding.UTF8.GetBytes("<html><body><article><p>" + item.Article + "</p></article></body></html>");
            byte[] header = Encoding.ASCII.GetBytes($"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {body.Length}\r\nConnection: close\r\n\r\n");
            await stream.WriteAsync(header); await stream.WriteAsync(body);
        });
        string path = Path.Combine(work, "crawl-zh.txt");
        var options = new Crawler.CrawlOptions(new Uri($"http://127.0.0.1:{port}/"), 1, 1, Lingua.Language.Chinese, path);
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var result = await Crawler.Crawler.RunAsync(options, new System.Progress<Crawler.CrawlProgress>(), timeout.Token);
        await serve.WaitAsync(timeout.Token);
        Check(result.Sentences == 1 && File.ReadAllText(path).Trim() == item.Article, "Actual local HTTP crawler did not export Chinese fallback sentences");
        Console.WriteLine("Actual offline HTTP crawler + language filter + Unicode sentence extraction passed");
    }
    private static async Task Backends(string? native)
    {
        string? previous = Environment.GetEnvironmentVariable("UICREATOR_MODELGEN");
        try
        {
            Environment.SetEnvironmentVariable("UICREATOR_MODELGEN", Path.Combine(work, "missing-native.exe"));
            Check(GenerationBackend.FindNativeHelper() is null && GenerationBackend.Describe().Contains("fallback"), "Missing native helper was presented as available");
            string prefix = Path.Combine(work, "managed-backend");
            string input = Path.Combine(root, "Tests", "Integration", "LanguageModels", "fixtures", "ascii.txt");
            await GenerationBackend.GenerateAsync(Arguments(input, prefix, "en", "--no-wordlist"), null, CancellationToken.None);
            if (native is null) return;
            Environment.SetEnvironmentVariable("UICREATOR_MODELGEN", native);
            Check(GenerationBackend.FindNativeHelper() == Path.GetFullPath(native), "Configured native helper was not detected");
            foreach (var item in Cases)
            {
                prefix = Path.Combine(work, item.Code + "-native"); int last = 0;
                await GenerationBackend.GenerateAsync(Arguments(Path.Combine(work, item.Code + ".txt"), prefix, item.Code), progress =>
                { Check(progress.Percent >= last, "Native protocol progress moved backwards"); last = progress.Percent; }, CancellationToken.None);
                Check(last == 100, "Native helper did not report completion");
                Check(File.ReadAllBytes(prefix + ".ngtr").SequenceEqual(File.ReadAllBytes(Path.Combine(work, item.Code + "-memory.ngtr"))), item.Code + ": C#/native NGTR differs");
                Counts(Json(prefix + ".json"), item.Sentences, item.Code + " native protocol");
            }
            string cancelInput = Path.Combine(work, "native-cancel.txt"), cancelPrefix = Path.Combine(work, "native-cancel");
            Write(cancelInput, string.Join(' ', Enumerable.Range(0, 50000).Select(index => "cancelword" + index)));
            Write(cancelPrefix + ".ngtr", "preserve native trie"); Write(cancelPrefix + ".json", "preserve native JSON");
            string scratch = Path.Combine(work, "native-cancel-scratch");
            using var stop = new CancellationTokenSource();
            bool stopped = false;
            try
            {
                await GenerationBackend.GenerateAsync(Arguments(cancelInput, cancelPrefix, "en", "--no-wordlist", "--low-memory", "--chunk-entries", "32", "--scratch-directory", scratch), progress =>
                { if (progress.Stage == "Counting n-grams" && progress.Percent > 0 && progress.Percent < 70) stop.Cancel(); }, stop.Token);
            }
            catch (OperationCanceledException) when (stop.IsCancellationRequested) { stopped = true; }
            Check(stopped && File.ReadAllText(cancelPrefix + ".ngtr") == "preserve native trie" && File.ReadAllText(cancelPrefix + ".json") == "preserve native JSON", "Native cancellation published partial output or did not stop");
            Check(!Directory.Exists(scratch) || !Directory.EnumerateFileSystemEntries(scratch).Any(), "Native cancellation left external count files behind");
            Console.WriteLine("Native helper protocol and seven exact C#/C++ model comparisons passed");
        }
        finally { Environment.SetEnvironmentVariable("UICREATOR_MODELGEN", previous); }
    }
    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            Check(args.Length is 1 or 2, "Expected repository root and optional native generator path");
            root = Path.GetFullPath(args[0]); work = Path.Combine(root, "build", "corpus-language-tests", Guid.NewGuid().ToString("N")); Directory.CreateDirectory(work);
            WindowsFormsSynchronizationContext.AutoInstall = false;
            GoldenCases(); Languages(); Forms(); SentencesAndWiki().GetAwaiter().GetResult(); Crawl().GetAwaiter().GetResult(); Backends(args.Length == 2 ? args[1] : null).GetAwaiter().GetResult();
            Console.WriteLine("All corpus language workflow checks passed: " + work); return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
