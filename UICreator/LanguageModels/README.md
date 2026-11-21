# Language model generator

Open **Language Models → Generate language model…** in LVGL UI Creator (Alt+L).
This native C++ tool ports the ESPOS NgramTrie generator and its C# interface.
It runs inside the editor and requires no .NET installation.

Add folders containing corpus `.txt` files, choose whether to search their
subdirectories, and select the corpus language. Bundled vocabulary filters are
available for English, Spanish, French, German and Greek. These filters remove
unknown words; they are not pretrained prediction models. Provide representative
corpus text in the selected language to train a model.

Choose the output `.ngtr` file and maximum model size in bytes. The default
budget is 262,144 bytes. Optional unigram, bigram and trigram limits control how
many entries to retain; zero disables an order and blank uses automatic limits.
With a byte budget, positive counts are priority targets and remaining space
can retain more entries. Without a byte budget, positive counts are upper limits.
Worker count, saved count reuse and disk-based counting match the original
generator's controls. Disk mode uses bounded sorted chunks and merge files.

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
corpora can be trained even though a Greek key layout is not currently bundled.

Keep the `language-models/wordlists` directory next to the editor executable.
The bundled `language-models/examples` folder contains a tiny English corpus
for a quick generation test; use larger representative corpora for useful models.
Settings and count caches are separate from the current screen project.
See [PROVENANCE.md](PROVENANCE.md) and [wordlists/LICENSE.md](wordlists/LICENSE.md)
for source and vocabulary notices.
