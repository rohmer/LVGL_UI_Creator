# C# language-model generator

This is the ESPOS C# generator mirrored into LVGL UI Creator's optional Windows corpus toolchain. The original English, Spanish, French, German and Greek output formats, normalization and bundled vocabulary filters remain compatible with the checked-in C# reference models.

The additional language codes are `ar` (Arabic), `hi` (Hindi), `zh` (Mandarin Chinese), `bn` (Bengali), `id` (Indonesian), `ur` (Urdu) and `ru` (Russian). These default to unfiltered Unicode words rather than applying an English vocabulary. They use Unicode folding and NFC normalization, retain combining marks after letters, and retain internal ZWJ/ZWNJ for Arabic, Hindi, Bengali and Urdu. Mandarin emits a token for each Han character, including supplementary-plane Han, and groups non-Han fragments into ordinary words. This is character segmentation, not a dictionary-based Chinese word segmenter.

Pass `--wordlist FILE` to filter a new language using a custom UTF-8 vocabulary. Entries use the same folding/NFC normalization. Use one Han character per line for Mandarin, with ordinary Latin words as whole entries. A rejected token resets n-gram context. `--no-wordlist` is also accepted for the seven new languages. Existing Spanish/French/German/Greek CLI restrictions retain their original bundled dictionaries. Their license and attribution notices remain in [Wordlist/LICENSE.md](Wordlist/LICENSE.md).

```powershell
dotnet build Tools/NgramTrie/NgramTrie.csproj -c Release
dotnet run --project Tools/NgramTrie/NgramTrie.csproj -c Release -- `
  --language hi --file corpus.txt --trie-output hindi.ngtr --compact `
  --trie-size 262144 --low-memory --workers 8
```

The UI prefers the optimized native helper when available; see [NativeTrieGenerator](../NativeTrieGenerator/README.md). The managed SQLite counting fallback stores temporary databases in local application data under `espos-ngram-dotnet/work`, rather than beside a potentially synchronized output file. New-language cache identities include the language and tokenizer semantic version; existing language cache identities are preserved.
