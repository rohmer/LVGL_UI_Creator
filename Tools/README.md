# Corpus tools from ESPOS

The optional Windows Corpus Workbench combines web collection, Wikipedia `.xml.bz2` extraction, trie generation and keyboard simulation. The source was ported from the user-provided ESPOS `tools` directory. That directory has no independent Git repository, so no upstream commit identifier is claimed. The mirror preserves relative project references and the required case-fold, vocabulary, spelling and keyboard assets. Build products, corpora, saved settings, machine-specific Java paths and downloaded model caches are not source inputs.

Build on Windows with the .NET 8 SDK and Windows desktop targeting/runtime support:

```powershell
dotnet build Tools/CorpusWorkbench/CorpusWorkbench.csproj -c Release
```

NuGet restores the existing pinned AngleSharp, SearchPioneer.Lingua, SharpCompress and Microsoft.Data.Sqlite dependencies. In LVGL UI Creator, `Directory.Build.props` sends binaries and intermediates to `build/corpus-tools/bin/<project>` and `build/corpus-tools/obj/<project>`. Run `build/corpus-tools/bin/CorpusWorkbench/Release/net8.0-windows/CorpusWorkbench.exe`. In the original ESPOS tree, projects retain their normal `bin/Release` layout.

Trie generation accepts twelve languages: English, Spanish, French, German, Greek, Arabic, Hindi, Mandarin Chinese, Bengali, Indonesian, Urdu and Russian. The crawler and Wikipedia extractor also retain Portuguese. Collection handoffs preserve the selected language when moving into the generator, and completed models can be sent to the keyboard simulator.

The native `UICreatorModelGen` helper is preferred when detected. Build/install it using [NativeTrieGenerator](NativeTrieGenerator/README.md), or set `UICREATOR_MODELGEN` to its executable. If it is absent, generation uses the managed C# implementation. The UI identifies the active engine, forwards progress and cooperative cancellation, and keeps outputs protected on failure. For the seven new languages, no English word list is silently applied.

New collection languages use an explicitly labelled Unicode boundary fallback, with no POS grammar classification or Java requirement. Original English/Spanish/French/Portuguese/German/Greek OpenNLP collection needs JDK 17+ and the pinned dependencies installed by `CorpusCrawler/setup-opennlp.ps1`, followed by rebuilding. Java sources and the setup script are mirrored; downloaded JARs and machine-specific `java-home.txt` stay outside source control. Russian and Indonesian model metadata exists upstream, but this port does not claim to bundle or invoke those models. See [Corpus Crawler](CorpusCrawler/README.md) for extraction behavior.

Vocabulary attribution and licenses are preserved in [NgramTrie/Wordlist/LICENSE.md](NgramTrie/Wordlist/LICENSE.md). Existing English spelling/fuzzer data remains unchanged; the fuzzer and GBU scoring workflow still uses its English spelling and QWERTY assumptions. Keyboard fonts/data have their own accompanying notices.

Run the offline language workflow checks, including the eleven original C# golden models, all seven new Unicode languages, real local-HTTP crawling, compressed Wikipedia exports and native-helper parity/cancellation:

```powershell
dotnet run --project Tools/CorpusLanguageTests/CorpusLanguageTests.csproj -c Release -- `
  E:/source/LVGL_UI_Creator E:/source/LVGL_UI_Creator/Tools/NativeTrieGenerator/bin/UICreatorModelGen.exe
```

The first argument is the LVGL UI Creator source root containing the checked-in reference fixtures. Omit the optional second argument to test the managed backend without a native helper. Tests create isolated files under that checkout's ignored `build/corpus-language-tests` directory and do not crawl external websites or change user corpora/settings.
