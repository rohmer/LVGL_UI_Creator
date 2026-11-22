# Corpus Crawler

Build the Windows desktop app with the .NET 8 SDK or later. Arabic, Hindi, Mandarin Chinese, Bengali, Indonesian, Urdu and Russian use an explicitly labelled Unicode sentence-boundary fallback and do not require Java or downloaded models. This mode keeps punctuation-terminated text and has no part-of-speech grammar classifier. English, Spanish, French, Portuguese, German and Greek retain the original Apache OpenNLP pipeline and require a Java JDK 17 or later on `PATH` or through `JAVA_HOME`, plus the setup script's models. [Apache OpenNLP models](https://opennlp.apache.org/models.html)

If you use a portable JDK, pass its directory to `setup-opennlp.ps1 -JavaHome "C:\path\to\jdk"` before building. This lets you launch the executable directly without setting a system-wide Java path.

```powershell
& .\ESPOS\tools\CorpusCrawler\setup-opennlp.ps1
dotnet run --project .\ESPOS\tools\CorpusCrawler\CorpusCrawler.csproj
```

Enter one base URL per line, choose an output folder and a language, and set **Workers** to the maximum number of concurrent page jobs. The language choices include the six original languages and all seven Unicode fallback profiles listed above. The status panel identifies the active extraction mode. A hostname without a scheme uses HTTPS. The batch processes one starting URL at a time. With **Stay on the starting site** checked, links and redirects may use any path on the starting hostname or its `www` equivalent. Other hostnames, including other subdomains, are excluded. Uncheck the box to follow links to other HTTP or HTTPS sites. **Page limit per URL** includes the starting URL and redirect hops.

Each input has a subfolder under the selected destination. For example, `www.foxnews.com` writes `foxnews/corpus.txt` and `foxnews/corpus.txt.crawl`. Folder names omit a leading `www.` and trailing `.com`. If different URLs produce the same folder name, later entries get a numeric suffix. Repeated URLs are skipped.

The progress percentage is completed URL jobs divided by the page limit. It can finish below 100% if the crawler runs out of links. Average page time includes fetching, parsing, and language detection for each finished URL job, including redirects and failed requests. Concurrent page jobs are timed individually. The estimated time remaining multiplies the average page time by the number of worker-sized batches needed for queued and in-flight URLs. The estimate updates as new links are discovered.

To pause the batch, click **Stop**. Once checkpoint restoration has finished and at least one HTML page is available, Stop saves the filtered corpus so far for the current site. To resume, enter the same URL list in the same order, select the same output folder, language, page limit, and link scope, then choose **Yes** when asked to resume. You can change the worker count. Keep each `corpus.txt.crawl` checkpoint beside its corpus. The checkpoints preserve completed URL jobs, their sentences, and discovered links. Interrupted requests are retried. Choosing **No** at the resume prompt starts the batch over.

The app extracts text from downloaded HTML, removes scripts, styles, navigation, footers, and other excluded elements, and identifies each sentence's language locally. Short or ambiguous sentences can be skipped. Apache OpenNLP uses the selected language's sentence model to split text blocks into candidates. Content loaded by client-side JavaScript requires a browser.

On completion, the crawler saves UTF-8 text with one sentence per line. For the original six languages, OpenNLP checks for a verb or auxiliary, with additional fragment checks for English. The seven new profiles use NFC Unicode boundaries instead, retain Arabic and Indic marks, recognize Arabic question/full-stop marks, danda and Chinese punctuation, preserve closing quotes, and discard unfinished trailing fragments and numeric-only text. They do not claim grammatical completeness. Each exact sentence appears once across the whole export, including sentences restored from a checkpoint. The crawl status counts unique exported sentences.

In Trie Generator, select the same corpus language. English, Spanish, French, German and Greek keep their bundled vocabulary filters. `ar`, `hi`, `zh`, `bn`, `id`, `ur` and `ru` default to unfiltered Unicode tokens and also accept a custom vocabulary or `--no-wordlist`. Mandarin uses Han character n-grams, with Latin fragments kept as ordinary words; a custom Han vocabulary uses one character per line. Portuguese remains a collection-only profile. See [NgramTrie](../NgramTrie/README.md).

```powershell
dotnet run --project .\tools\NgramTrie\NgramTrie.csproj -c Release -- --language zh --file .\corpus.txt --compact --trie-size 262144 --trie-output model.ngtr
```
