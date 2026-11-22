# Wikipedia Corpus Extractor

Open `bin/Release/net8.0-windows/WikipediaCorpus.exe`. Choose a Wikipedia `.xml.bz2` dump, choose the corpus folder, and click **Extract**. The app writes a UTF-8 `.txt` file named after the dump directly in the selected folder. Each line contains one sentence.

**Auto (dump language)** uses Wikipedia site information first, then the dump filename and root XML language. It recognizes English, Spanish, French, Portuguese, German, Greek, Arabic, Hindi, Mandarin Chinese, Bengali, Indonesian, Urdu and Russian. Each can also be selected explicitly. The original six profiles use Apache OpenNLP sentence and POS models. The seven new profiles are labelled **Unicode** and use a boundary-based fallback without a POS grammar model or Java requirement. It supports Arabic punctuation, Indic danda and Chinese sentence punctuation, preserves combining marks and closing quotes, and discards unfinished trailing fragments. [OpenNLP models](https://opennlp.apache.org/models.html)

The extractor streams the compressed XML and reads the latest article revision per page. It skips redirects and other namespaces, keeps article link labels, and removes tables, references, and other wiki markup. Templates are omitted. [MediaWiki XML export](https://www.mediawiki.org/wiki/Help:Export/en)

The progress bar follows compressed bytes read. The app installs the output file after a successful extraction and confirms before replacing an existing corpus. Canceling discards the temporary output.

Build from the `ESPOS` project folder. Only the original six OpenNLP profiles need the same Java JDK 17 or later and model files as Corpus Crawler; skip setup for the seven Unicode profiles. If models are already set up, run the build command directly.

```powershell
& .\tools\CorpusCrawler\setup-opennlp.ps1
dotnet build .\tools\WikipediaCorpus\WikipediaCorpus.csproj -c Release
```
