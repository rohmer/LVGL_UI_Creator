using System.Buffers.Binary;
using System.Text.Json;

namespace CorpusCrawler;

internal sealed class CrawlCheckpoint : IDisposable
{
    private record Header(int Version, Uri Start, int PageLimit, string Language, bool? SameSiteOnly = null);

    private readonly FileStream stream;

    public static string PathFor(string output) => output + ".crawl";

    public CrawlCheckpoint(CrawlOptions options)
    {
        stream = new FileStream(PathFor(options.Output), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
        try
        {
            if (options.Restart) stream.SetLength(0);
            var expected = new Header(2, options.Start, options.PageLimit, options.Language.ToString(), options.SameSiteOnly);
            var saved = Read<Header>();
            if (saved is null) Append(expected);
            else if (saved != expected && !(options.SameSiteOnly && saved == (expected with { Version = 1, SameSiteOnly = null })))
                throw new InvalidOperationException("The saved crawl needs the original base URL, language, page limit, and site scope. Choose Start over to use different settings.");
        }
        catch
        {
            stream.Dispose();
            throw;
        }
    }

    public T? Read<T>() where T : class
    {
        long start = stream.Position;
        long available = stream.Length - start;
        if (available == 0) return null;

        Span<byte> header = stackalloc byte[sizeof(int)];
        if (available >= header.Length)
        {
            stream.ReadExactly(header);
            int length = BinaryPrimitives.ReadInt32LittleEndian(header);
            if (length <= 0) throw new InvalidDataException("Invalid crawl checkpoint record length.");
            if (length <= available - header.Length)
            {
                var payload = new byte[length];
                stream.ReadExactly(payload);
                return JsonSerializer.Deserialize<T>(payload)
                    ?? throw new InvalidDataException("Empty crawl checkpoint record.");
            }
        }

        // A shutdown during a write can leave one incomplete record at the end.
        stream.SetLength(start);
        stream.Position = start;
        return null;
    }

    public void Append<T>(T value)
    {
        byte[] payload = JsonSerializer.SerializeToUtf8Bytes(value);
        Span<byte> header = stackalloc byte[sizeof(int)];
        BinaryPrimitives.WriteInt32LittleEndian(header, payload.Length);
        stream.Write(header);
        stream.Write(payload);
        stream.Flush(flushToDisk: true);
    }

    public void Dispose() => stream.Dispose();
}
