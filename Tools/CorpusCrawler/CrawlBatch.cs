using System.Text.RegularExpressions;

namespace CorpusCrawler;

internal static class CrawlBatch
{
    public static (Uri Url, string Output)[] Plan(IEnumerable<string> lines, string directory)
    {
        var urls = new HashSet<Uri>();
        var folders = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var destinations = new List<(Uri Url, string Output)>();
        foreach (var (line, index) in lines.Select((line, index) => (line.Trim(), index)))
        {
            if (line.Length == 0) continue;
            string address = line.Contains("://", StringComparison.Ordinal) ? line : "https://" + line;
            if (!Uri.TryCreate(address, UriKind.Absolute, out var url) ||
                url.Scheme is not ("http" or "https") || url.UserInfo.Length > 0)
                throw new ArgumentException($"Line {index + 1} must contain an HTTP or HTTPS URL.");
            url = new UriBuilder(url) { Fragment = "" }.Uri;
            if (!urls.Add(url)) continue;

            string name = url.IdnHost.ToLowerInvariant().TrimEnd('.');
            if (name.StartsWith("www.", StringComparison.Ordinal)) name = name[4..];
            if (name.EndsWith(".com", StringComparison.Ordinal)) name = name[..^4];
            name = Regex.Replace(name, "[^a-z0-9._-]", "-");
            if (name.Length > 100) name = name[..100].TrimEnd('.');
            if (Regex.IsMatch(name, @"^(?:con|prn|aux|nul|com[0-9]|lpt[0-9])(?:\.|$)")) name = "site-" + name;
            string folder = name;
            for (int suffix = 2; !folders.Add(folder); suffix++) folder = $"{name}-{suffix}";
            destinations.Add((url, Path.Combine(directory, folder, "corpus.txt")));
        }
        if (destinations.Count == 0) throw new ArgumentException("Enter at least one URL.");
        return [.. destinations];
    }
}
