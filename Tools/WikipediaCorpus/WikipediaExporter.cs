using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Xml;
using System.Xml.Linq;
using CorpusCrawler;
using SharpCompress.Compressors;
using SharpCompress.Compressors.BZip2;

namespace WikipediaCorpus;

internal sealed record ExportProgress(long CompressedBytesRead, long CompressedBytesTotal,
    long PagesRead, long ArticlesRead, long SentencesWritten);

internal sealed record ExportResult(string OutputPath, long PagesRead, long ArticlesRead,
    long SentencesWritten, string LanguageCode);

internal static class WikipediaExporter
{
    private const int BufferSize = 128 * 1024;
    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false);

    public static string GetOutputPath(string inputPath, string destinationDirectory)
    {
        string name = DumpName(inputPath);
        if (!inputPath.EndsWith(".bz2", StringComparison.OrdinalIgnoreCase) || name.Length == 0)
            throw new ArgumentException("Select a bzip2-compressed Wikipedia XML dump.", nameof(inputPath));
        return Path.GetFullPath(Path.Combine(destinationDirectory, name + ".txt"));
    }

    public static async Task<ExportResult> ExportAsync(string inputPath, string outputPath, string? languageCode,
        bool overwrite, IProgress<ExportProgress>? progress, CancellationToken cancellation)
    {
        cancellation.ThrowIfCancellationRequested();
        inputPath = Path.GetFullPath(inputPath);
        outputPath = Path.GetFullPath(outputPath);
        if (string.Equals(inputPath, outputPath, StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Choose an output file different from the dump.", nameof(outputPath));
        if (!overwrite && File.Exists(outputPath)) throw new IOException($"The output file already exists. {outputPath}");

        using var input = new FileStream(inputPath, FileMode.Open, FileAccess.Read, FileShare.Read, BufferSize,
            FileOptions.SequentialScan);
        long total = input.Length;
        if (!BZip2Stream.IsBZip2(input)) throw new InvalidDataException("The input is not a bzip2 file.");
        input.Position = 0;
        using var decompressed = BZip2Stream.Create(input, CompressionMode.Decompress, decompressConcatenated: true, leaveOpen: true);
        using var xml = XmlReader.Create(decompressed, new XmlReaderSettings
        {
            DtdProcessing = DtdProcessing.Prohibit,
            XmlResolver = null,
            IgnoreComments = true,
            IgnoreProcessingInstructions = true,
        });
        if (xml.MoveToContent() != XmlNodeType.Element || xml.LocalName != "mediawiki")
            throw new InvalidDataException("The archive must contain a MediaWiki XML export.");

        string xmlNamespace = xml.NamespaceURI;
        long pages = 0, articles = 0, sentences = 0;
        long lastReport = Stopwatch.GetTimestamp();
        progress?.Report(new(input.Position, total, pages, articles, sentences));
        string? detectedLanguage = DatabaseLanguage(DumpName(inputPath).Split('-', 2)[0]) ?? xml.XmlLang;
        string? language = null;
        OpenNlpSentences? worker = null;
        string directory = Path.GetDirectoryName(outputPath)!;
        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(directory, $".wikipedia-{Guid.NewGuid():N}.partial");
        var paragraphs = new List<string>();
        int characters = 0;

        try
        {
            using (var writer = new StreamWriter(new FileStream(temporary, FileMode.CreateNew, FileAccess.Write,
                       FileShare.None, BufferSize), Utf8, BufferSize) { NewLine = "\n" })
            {
                while (xml.Read())
                {
                    cancellation.ThrowIfCancellationRequested();
                    if (!IsDirectChild(xml, xmlNamespace)) continue;
                    if (xml.LocalName == "siteinfo")
                    {
                        using var siteReader = xml.ReadSubtree();
                        detectedLanguage = ReadSiteLanguage(XElement.Load(siteReader)) ?? detectedLanguage;
                        continue;
                    }
                    if (xml.LocalName != "page") continue;

                    pages++;
                    string? article = ReadArticle(xml, xmlNamespace, cancellation);
                    if (!string.IsNullOrWhiteSpace(article))
                    {
                        articles++;
                        foreach (string paragraph in WikiText.Paragraphs(article, cancellation))
                        {
                            paragraphs.Add(paragraph);
                            characters += paragraph.Length;
                            if (characters >= BufferSize) await WriteSentencesAsync(writer).ConfigureAwait(false);
                        }
                    }
                    if (progress is not null && Stopwatch.GetElapsedTime(lastReport).TotalMilliseconds >= 200)
                    {
                        lastReport = Stopwatch.GetTimestamp();
                        progress.Report(new(input.Position, total, pages, articles, sentences));
                    }
                }
                await WriteSentencesAsync(writer).ConfigureAwait(false);
            }

            cancellation.ThrowIfCancellationRequested();
            if (sentences == 0) throw new InvalidDataException("No complete article sentences were found in this dump.");
            File.Move(temporary, outputPath, overwrite);
            progress?.Report(new(total, total, pages, articles, sentences));
            return new(outputPath, pages, articles, sentences, language!);
        }
        finally
        {
            worker?.Dispose();
            File.Delete(temporary);
        }

        async Task WriteSentencesAsync(StreamWriter writer)
        {
            if (paragraphs.Count == 0) return;
            language ??= ResolveLanguage(languageCode, detectedLanguage);
            worker ??= await OpenNlpSentences.StartAsync(language, cancellation).ConfigureAwait(false);
            foreach (string sentence in await worker.ExtractAsync(paragraphs, cancellation).ConfigureAwait(false))
            {
                cancellation.ThrowIfCancellationRequested();
                writer.WriteLine(sentence);
                sentences++;
            }
            paragraphs.Clear();
            characters = 0;
        }
    }

    private static bool IsDirectChild(XmlReader reader, string xmlNamespace) =>
        reader.NodeType == XmlNodeType.Element && reader.Depth == 1 && reader.NamespaceURI == xmlNamespace;

    private static string? ReadArticle(XmlReader source, string xmlNamespace, CancellationToken cancellation)
    {
        using var page = source.ReadSubtree();
        page.Read();
        page.Read();
        bool mainNamespace = false;
        Revision latest = default;
        while (!page.EOF)
        {
            cancellation.ThrowIfCancellationRequested();
            if (!IsDirectChild(page, xmlNamespace))
            {
                page.Read();
                continue;
            }
            switch (page.LocalName)
            {
                case "ns":
                    if (page.ReadElementContentAsString().Trim() != "0") return null;
                    mainNamespace = true;
                    break;
                case "redirect":
                    return null;
                case "revision" when mainNamespace:
                    Revision revision = ReadRevision(page, xmlNamespace, cancellation);
                    if (revision.Timestamp >= latest.Timestamp) latest = revision;
                    page.Read();
                    break;
                default:
                    page.Skip();
                    break;
            }
        }
        return latest.Text;
    }

    private static Revision ReadRevision(XmlReader source, string xmlNamespace, CancellationToken cancellation)
    {
        using var revision = source.ReadSubtree();
        revision.Read();
        revision.Read();
        string? model = null, format = null, text = null;
        DateTimeOffset timestamp = default;
        while (!revision.EOF)
        {
            cancellation.ThrowIfCancellationRequested();
            if (!IsDirectChild(revision, xmlNamespace))
            {
                revision.Read();
                continue;
            }
            switch (revision.LocalName)
            {
                case "timestamp":
                    DateTimeOffset.TryParse(revision.ReadElementContentAsString(), CultureInfo.InvariantCulture,
                        DateTimeStyles.AssumeUniversal, out timestamp);
                    break;
                case "model":
                    model = revision.ReadElementContentAsString();
                    break;
                case "format":
                    format = revision.ReadElementContentAsString();
                    break;
                case "text" when revision.GetAttribute("deleted") is null:
                    text = revision.ReadElementContentAsString();
                    break;
                default:
                    revision.Skip();
                    break;
            }
        }
        return new(timestamp, model is null or "wikitext" && format is null or "text/x-wiki" ? text : null);
    }

    private static string? ReadSiteLanguage(XElement site)
    {
        XNamespace ns = site.Name.Namespace;
        if (DatabaseLanguage(site.Element(ns + "dbname")?.Value.Trim()) is { } language) return language;
        const string suffix = ".wikipedia.org";
        return Uri.TryCreate(site.Element(ns + "base")?.Value, UriKind.Absolute, out var address) &&
               address.Host.EndsWith(suffix, StringComparison.OrdinalIgnoreCase)
            ? address.Host[..^suffix.Length].Split('.', 2)[0]
            : null;
    }

    private static string? DatabaseLanguage(string? name) =>
        name is { Length: > 4 } && name.EndsWith("wiki", StringComparison.OrdinalIgnoreCase) ? name[..^4] : null;

    private static string DumpName(string path)
    {
        string name = Path.GetFileNameWithoutExtension(path);
        return name.EndsWith(".xml", StringComparison.OrdinalIgnoreCase) ? name[..^4] : name;
    }

    private static string ResolveLanguage(string? requested, string? detected)
    {
        string? language = (requested ?? detected)?.Trim().Split('-', 2)[0].ToLowerInvariant();
        if (language == "simple") language = "en";
        if (string.IsNullOrEmpty(language))
            throw new InvalidDataException("The dump does not identify its language. Choose the sentence language in the app.");
        if (language is not ("en" or "es" or "fr" or "pt" or "de" or "el" or "ar" or "hi" or "zh" or "bn" or "id" or "ur" or "ru"))
            throw new InvalidDataException($"No sentence extraction profile is available for '{language}'.");
        return language;
    }

    private readonly record struct Revision(DateTimeOffset Timestamp, string? Text);
}
