# ESPOS Corpus Workbench

Corpus Workbench brings the corpus and n-gram tools into one Windows application. Use the navigation on the left to switch tools. Running work continues when you switch pages, and closing the application waits for active work to stop and save its progress.

- **Collect corpora** contains the web crawler and Wikipedia XML extractor. The crawler accepts multiple URLs, saves one corpus per site, filters complete sentences, and supports stop and resume. The Wikipedia extractor reads compressed XML dumps.
- **Build n-grams** contains the trie generator, including language word lists, directory recursion, saved counts, low-memory processing, and a binary size limit.
- **Test corrections** contains the sentence fuzzer, GBU scorer and threshold regression, and keyboard simulator with its trie viewer.

After collecting text, open **Trie generator** and click **Use collected corpora** to select the exact collected files in the latest corpus's language. You can add more files or folders in the generator. Other languages are not included merely because their files share an output folder. It accepts English, Spanish, French, German, Greek, Arabic, Hindi, Mandarin Chinese, Bengali, Indonesian, Urdu and Russian. The original five vocabulary filters are preserved; the seven new languages default to NFC Unicode text without a bundled filter. Mandarin uses Han character n-grams. A completed trie becomes available through **Use latest trie** in the fuzzer, tester, and keyboard simulator. Completed sentence pairs are available through **Use latest fuzzed file** in the tester. Hover over these buttons to see their file paths. The workbench remembers these completed files and the last selected tool between sessions. The generator also retains its own settings.

The generation-engine label reports the backend actually available. When `UICreatorModelGen.exe` is beside the application, under an ancestor's `tools/NativeTrieGenerator/bin`, or explicitly selected with the `UICREATOR_MODELGEN` environment variable, the workbench uses the optimized native generator. Its progress and Stop command are connected to the UI. If no native helper is found, the managed C# generator remains available. A failed native run reports its error instead of silently starting a second full generation. Both backends keep temporary counts on local storage and publish final outputs atomically.

The crawler and Wikipedia extractor offer all twelve generation languages plus Portuguese. The seven new profiles clearly report a Unicode sentence-boundary fallback without OpenNLP/POS grammar classification. They work without Java. The original six collection profiles retain their OpenNLP models. See [Corpus Crawler](../CorpusCrawler/README.md) and [Wikipedia Corpus](../WikipediaCorpus/README.md).

The simulator selects the keyboard for a trie generated in the workbench. The fuzzer and GBU tester use their existing English spelling data and QWERTY neighbor model.

Open `CorpusWorkbench.csproj` in Visual Studio with the **.NET desktop development** workload and the **.NET 8 SDK** installed. Its relative project references include all supporting tools. Run **CorpusWorkbench** as the startup project with **F5**. NuGet restores the project dependencies, and the build copies the required keyboard definitions, spelling data, and any configured OpenNLP assets into the application output.

Build the solution from the workspace root on the command line with the .NET 8 Windows desktop SDK.

```powershell
dotnet build tools/CorpusWorkbench/CorpusWorkbench.csproj -c Release
```

In ESPOS, run `bin/Release/net8.0-windows/CorpusWorkbench.exe` in this project directory. The LVGL UI Creator mirror redirects build output to `build/corpus-tools/bin/CorpusWorkbench/Release/net8.0-windows`. Keep the output folder together so keyboard definitions, spelling data, any configured OpenNLP assets and runtime dependencies are available. The crawler and Wikipedia exporter use the existing [OpenNLP setup](../CorpusCrawler/README.md) only for their original six model-based profiles.
