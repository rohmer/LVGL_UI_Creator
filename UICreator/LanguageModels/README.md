# Language model generator

Open **Language Models → Generate language model…** in LVGL UI Creator (Alt+L).
This native C++ tool ports the ESPOS NgramTrie generator and its C# interface.
It runs inside the editor and requires no .NET installation.

Add folders containing corpus `.txt` files, choose whether to search their
subdirectories, and select the corpus language. Bundled vocabulary filters are
available for English, Spanish, French, German and Greek. These filters remove
unknown words; they are not pretrained prediction models. Provide representative
corpus text in the selected language to train a model.

Arabic, Hindi, Mandarin Chinese, Bengali, Indonesian, Urdu and Russian are also
supported. These languages train directly from the corpus without applying an
English vocabulary filter. Explicit custom word lists are available through
the native command-line helper and Corpus Workbench. Arabic, Hindi, Bengali and
Urdu keep combining marks and in-word joiners. Text is normalized to NFC and
case-folded; Mandarin counts individual Han characters and groups Latin words.
Its n-grams therefore represent one, two or three consecutive Han characters,
not dictionary-based Chinese word segmentation. A custom Mandarin vocabulary
should list individual Han characters.

Choose the output `.ngtr` file and maximum model size in bytes. The default
budget is 262,144 bytes. Optional unigram, bigram and trigram limits control how
many entries to retain; zero disables an order and blank uses automatic limits.
With a byte budget, positive counts are priority targets and remaining space
can retain more entries. Without a byte budget, positive counts are upper limits.
Worker count, saved count reuse and disk-based counting match the original
generator's controls. Disk mode uses bounded sorted chunks and merge files.

Large UTF-8 files are divided at existing sentence or line resets and shared
by the requested worker pool. UTF-16/32 keep their strict whole-file decoder.
Disk-mode count chunks are bounded per active worker, so increasing the worker
count also increases memory use. Merge-file concurrency is bounded separately.
Temporary counting and sorting files use local application storage, independently
of the final output folder; saving a model to OneDrive or a network share does
not put all intermediate files there. Consumed merge files are removed promptly.

On the development host, a 32 MiB French Wikipedia sample improved from 37.9 to
4.9 seconds with identical NGTR and JSON bytes. A complete 2.52 GB French corpus
completed in 13 minutes 26 seconds using 24 workers, a 256 KiB model budget and
approximately 237 MB peak working memory. These are measured desktop results;
corpus vocabulary, storage, worker count and model limits affect other runs.

`UICreatorModelGen` provides the same engine to Corpus Workbench and scripts.
See [the native backend instructions](../../Tools/NativeTrieGenerator/README.md)
and [Corpus Workbench source tools](../../Tools/README.md). Corpus Workbench shows
the chosen native or managed backend and requests cancellation cooperatively.

**Generate trie** runs in a background worker while progress, elapsed time and
estimated time remaining update in the window. **Stop** cancels generation.
Output is published only after a successful build. The editor produces compact
NGTR v3 files compatible with Advanced Keyboard.

Use **Load into project** after generation, or **Language Models → Load model into
project…** for an existing model. If Advanced Keyboard is selected, importing
also selects that model for its suggestions and autocorrect. Otherwise the model
is added to the project and can be chosen from the keyboard's **Prediction trie**
dropdown. The model is embedded in JSON/Protobuf projects and C exports.

The corpus-language filter is separate from the keyboard's key layout. A French
model, for example, should be paired with the French keyboard layout. Greek
models can use the Greek layout. Advanced Keyboard's **Trie size** selector
also offers the supplied 16 KiB and 256 KiB corpus models for seven languages;
see [the bundled catalog](../KeyboardModels/README.md). Selection embeds the
model and its index in the current project.

Keep the `language-models/wordlists` directory next to the editor executable.
The bundled `language-models/examples` folder contains a tiny English corpus
for a quick generation test; use larger representative corpora for useful models.
Settings and count caches are separate from the current screen project.
See [PROVENANCE.md](PROVENANCE.md) and [wordlists/LICENSE.md](wordlists/LICENSE.md)
for source and vocabulary notices.
