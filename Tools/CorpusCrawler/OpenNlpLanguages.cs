using Lingua;

namespace CorpusCrawler;

internal sealed partial class OpenNlpSentences
{
    private static readonly (Language Language, string Code)[] Models =
    [
        (Language.English, "en"), (Language.Spanish, "es"), (Language.French, "fr"),
        (Language.Portuguese, "pt"), (Language.German, "de"), (Language.Greek, "el"),
        (Language.Arabic, "ar"), (Language.Hindi, "hi"), (Language.Chinese, "zh"),
        (Language.Bengali, "bn"), (Language.Indonesian, "id"), (Language.Urdu, "ur"), (Language.Russian, "ru"),
    ];

    public static IEnumerable<Language> Languages => Models.Select(model => model.Language);

    public static Task<OpenNlpSentences> StartAsync(Language language, CancellationToken cancellation)
    {
        string code = Models.FirstOrDefault(model => model.Language == language).Code
            ?? throw new ArgumentException($"This crawler does not support {language}.", nameof(language));
        return StartAsync(code, cancellation);
    }
}
