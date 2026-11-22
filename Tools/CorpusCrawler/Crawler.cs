using System.Diagnostics;
using System.Net;
using System.Text;
using AngleSharp.Html.Parser;
using Lingua;

namespace CorpusCrawler;

internal record CrawlOptions(
    Uri Start, int Workers, int PageLimit, Language Language, string Output,
    bool Restart = false, bool SameSiteOnly = true);
internal sealed class CrawlStoppedAfterExportException(CancellationToken cancellation) : OperationCanceledException(cancellation);

internal record CrawlProgress(
    int Visited, int Discovered, int Sentences, int Failed, string? LastError,
    TimeSpan TotalPageTime, int PageLimit, int Workers)
{
    public int PercentComplete => (int)(100L * Visited / PageLimit);
    public TimeSpan? AveragePageTime => Visited == 0 ? null : TotalPageTime / Visited;

    public TimeSpan? EstimatedTimeRemaining
    {
        get
        {
            int remaining = Discovered - Visited;
            int batches = (remaining + Workers - 1) / Workers;
            return AveragePageTime * batches;
        }
    }
}

internal static class Crawler
{
    private record Page(string[] Sentences, Uri[] Links, bool Parsed = false, string? Error = null);
    private record CompletedPage(
        Uri Url, Page Page, TimeSpan Elapsed, bool OpenNlpProcessed = false, bool UpdatedScope = false);

    public static async Task<CrawlProgress> RunAsync(
        CrawlOptions options, IProgress<CrawlProgress> progress, CancellationToken cancellation)
    {
        using var stop = CancellationTokenSource.CreateLinkedTokenSource(cancellation);
        using var openNlp = await OpenNlpSentences.StartAsync(options.Language, stop.Token);
        using var client = new HttpClient(new HttpClientHandler
        {
            AllowAutoRedirect = false,
            AutomaticDecompression = DecompressionMethods.All,
            UseCookies = false,
            MaxConnectionsPerServer = options.Workers,
        })
        {
            Timeout = TimeSpan.FromSeconds(30),
            MaxResponseContentBufferSize = 8 * 1024 * 1024,
        };
        client.DefaultRequestHeaders.UserAgent.ParseAdd("ESPOS-CorpusCrawler/1.0");
        using var detectors = new ThreadLocal<LanguageDetector>(() => LanguageDetectorBuilder.FromAllLanguages()
            .WithMinimumRelativeDistance(0.1).Build());

        var queue = new Queue<Uri>();
        var seen = new HashSet<Uri>();
        var writtenSentences = new HashSet<string>(StringComparer.Ordinal);
        var active = new List<Task<CompletedPage>>();
        Enqueue(options.Start);
        int visited = 0, failed = 0, parsed = 0;
        TimeSpan totalPageTime = TimeSpan.Zero;
        string? lastError = null;
        string temporary = options.Output + $".{Guid.NewGuid():N}.tmp";
        try
        {
            using var checkpoint = new CrawlCheckpoint(options);
            await using (var writer = new StreamWriter(temporary, append: false, new UTF8Encoding(false)))
            {
                var completed = new HashSet<Uri>();
                while (checkpoint.Read<CompletedPage>() is { } saved)
                {
                    stop.Token.ThrowIfCancellationRequested();
                    if (!seen.Contains(saved.Url) || !completed.Add(saved.Url))
                        throw new InvalidDataException("The crawl checkpoint contains an unexpected completed URL.");
                    if (!saved.OpenNlpProcessed)
                    {
                        var candidates = await openNlp.ExtractAsync(saved.Page.Sentences, stop.Token);
                        var filtered = candidates.Where(text => detectors.Value!.DetectLanguageOf(text) == options.Language);
                        saved = saved with { Page = saved.Page with { Sentences = [.. filtered] } };
                    }
                    await ApplyAsync(saved, writer);
                }
                if (visited > 0)
                {
                    queue = new Queue<Uri>(queue.Where(url => !completed.Contains(url)));
                    progress.Report(Snapshot());
                }

                while (active.Count > 0 || (queue.Count > 0 && !stop.IsCancellationRequested))
                {
                    while (queue.Count > 0 && active.Count < options.Workers && !stop.IsCancellationRequested)
                    {
                        var url = queue.Dequeue();
                        active.Add(FetchTimedAsync(client, url, options.Language, detectors, openNlp, stop.Token));
                    }
                    if (active.Count == 0) break;

                    var finished = await Task.WhenAny(active);
                    active.Remove(finished);
                    CompletedPage result;
                    try { result = await finished; }
                    catch (OperationCanceledException) when (stop.IsCancellationRequested)
                    {
                        continue;
                    }
                    checkpoint.Append(result);
                    await ApplyAsync(result, writer);
                    progress.Report(Snapshot());
                }
            }

            if (parsed == 0)
            {
                stop.Token.ThrowIfCancellationRequested();
                throw new InvalidOperationException(lastError ?? "The crawl found no HTML pages.");
            }
            File.Move(temporary, options.Output, overwrite: true);
            if (stop.IsCancellationRequested)
                throw new CrawlStoppedAfterExportException(stop.Token);
            return Snapshot();
        }
        finally
        {
            stop.Cancel();
            try { await Task.WhenAll(active); }
            catch (OperationCanceledException) when (stop.IsCancellationRequested) { }
            finally { File.Delete(temporary); }
        }

        CrawlProgress Snapshot() => new(
            visited, seen.Count, writtenSentences.Count, failed, lastError, totalPageTime, options.PageLimit, options.Workers);

        async Task ApplyAsync(CompletedPage result, StreamWriter writer)
        {
            var page = result.Page;
            visited++;
            totalPageTime += result.Elapsed;
            if (page.Parsed) parsed++;
            if (page.Error is not null)
            {
                failed++;
                lastError = page.Error;
            }
            foreach (var link in page.Links) Enqueue(link, legacyScope: !result.UpdatedScope);
            foreach (var sentence in page.Sentences)
            {
                // Apply the export rule to saved pages as well as newly extracted text.
                if (!WebText.HasSentenceEnding(sentence, options.Language) || !writtenSentences.Add(sentence)) continue;
                await writer.WriteLineAsync(sentence);
            }
        }

        void Enqueue(Uri url, bool legacyScope = false)
        {
            if (seen.Count >= options.PageLimit || !IsInScope(options.Start, url, options.SameSiteOnly, legacyScope)) return;
            var normalized = new UriBuilder(url) { Fragment = "" }.Uri;
            if (seen.Add(normalized)) queue.Enqueue(normalized);
        }
    }

    internal static bool IsInScope(Uri start, Uri url, bool sameSiteOnly, bool legacyScope = false)
    {
        if (url.Scheme is not ("http" or "https") || url.UserInfo.Length != 0) return false;
        if (!sameSiteOnly) return true;
        if (legacyScope)
        {
            // Older checkpoints recorded outbound links before deciding which ones to queue.
            return url.IdnHost == start.IdnHost &&
                   ((url.Scheme == start.Scheme && url.Port == start.Port) ||
                    (start.Scheme == "http" && start.IsDefaultPort && url.Scheme == "https" && url.IsDefaultPort));
        }

        return string.Equals(SiteHost(start), SiteHost(url), StringComparison.OrdinalIgnoreCase);

        static string SiteHost(Uri uri)
        {
            string host = uri.IdnHost.TrimEnd('.');
            return host.StartsWith("www.", StringComparison.OrdinalIgnoreCase) ? host[4..] : host;
        }
    }

    private static async Task<CompletedPage> FetchTimedAsync(
        HttpClient client, Uri url, Language language, ThreadLocal<LanguageDetector> detectors,
        OpenNlpSentences openNlp, CancellationToken cancellation)
    {
        long started = Stopwatch.GetTimestamp();
        var page = await FetchAsync(client, url, language, detectors, openNlp, cancellation);
        return new(url, page, Stopwatch.GetElapsedTime(started), OpenNlpProcessed: true, UpdatedScope: true);
    }

    private static async Task<Page> FetchAsync(
        HttpClient client, Uri url, Language language, ThreadLocal<LanguageDetector> detectors,
        OpenNlpSentences openNlp, CancellationToken cancellation)
    {
        try
        {
            using var response = await client.GetAsync(url, cancellation);
            if ((int)response.StatusCode is 301 or 302 or 303 or 307 or 308 &&
                response.Headers.Location is { } location && Uri.TryCreate(url, location, out var target))
                return new([], [target]);

            response.EnsureSuccessStatusCode();
            var mime = response.Content.Headers.ContentType;
            if (mime?.MediaType?.ToLowerInvariant() is not (null or "text/html" or "application/xhtml+xml"))
                return new([], []);

            var parser = new HtmlParser();
            using var document = mime?.CharSet is { Length: > 0 }
                ? await parser.ParseDocumentAsync(await response.Content.ReadAsStringAsync(cancellation), cancellation)
                : await parser.ParseDocumentAsync(await response.Content.ReadAsStreamAsync(cancellation), cancellation);

            Uri? linkBase = url;
            if (document.QuerySelector("base[href]")?.GetAttribute("href") is { } baseHref &&
                Uri.TryCreate(url, baseHref, out var resolved)) linkBase = resolved;
            var links = new List<Uri>();
            foreach (var anchor in document.QuerySelectorAll("a[href]"))
                if (Uri.TryCreate(linkBase, anchor.GetAttribute("href"), out var link)) links.Add(link);

            var sentences = new List<string>();
            foreach (var sentence in await openNlp.ExtractAsync(WebText.TextBlocks(document), cancellation))
            {
                cancellation.ThrowIfCancellationRequested();
                if (detectors.Value!.DetectLanguageOf(sentence) == language) sentences.Add(sentence);
            }
            return new([.. sentences], [.. links], Parsed: true);
        }
        catch (Exception error) when (error is HttpRequestException or IOException or InvalidOperationException ||
                                      error is OperationCanceledException && !cancellation.IsCancellationRequested)
        {
            return new([], [], Error: $"{url} — {error.Message}");
        }
    }
}
