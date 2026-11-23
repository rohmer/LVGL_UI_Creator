# Bundled corpus tries

These sixteen compact NGTR v3 files were supplied by the user in `D:/Corpus`.
They cover English (US), Spanish, French, German, Greek, Arabic, Hindi and Mandarin Chinese, each
with nominal 16 KiB and 256 KiB variants. No text corpora or Wikipedia dumps are
included. No additional upstream license or training-data provenance is claimed.

The original names, SHA-256 hashes, exact byte lengths, node counts and generated
index lengths are recorded in `catalog.json`. Source aliases are retained there:
`esMS` is the model from the `es-MX` corpus and uses the shared Spanish `es-ES`
keyboard; `arAR`, `hiHI` and `elEL` map to `ar-SA`, `hi-IN` and `el-GR` layouts.
`zhZH` maps to the `zh-CN` Mandarin Pinyin keyboard. The Mandarin models use
individual Han characters and supply next-character unigram/bigram predictions;
their current data contains no trigram entries.
Nominal 256 KiB models are 40–340 bytes larger than 262,144 bytes; the original
model bytes remain intact. Model selection uses the nominal size, while project
payloads and footprint reporting use the actual size plus the NGIX index.

The editor stages these files under `language-models/bundled` beside its
executable. Catalog enumeration reads only small compiled metadata; model bytes
are loaded and validated when selected. Development builds can fall back to this
source directory. There is no runtime dependency on `D:/Corpus`.

Choosing a bundled model imports its exact bytes through the normal project trie
catalog, generates and retains an NGIX index, and deduplicates equal models.
Multiple widgets share one embedded model. Device/project generation retains only
referenced models, so the full bundled library is not copied into firmware.

Regenerate the catalog from the original files:

```powershell
python UICreator/KeyboardModels/import_corpus.py D:/Corpus
```

The importer fully validates all sixteen NGTR structures before copying any
model. It writes inside this directory and never changes the input files.
