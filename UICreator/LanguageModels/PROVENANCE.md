# Language model generator provenance

The C++ generator and LVGL interface are ported from the ESPOS project:

- `tools/NgramTrie`: Generator.cs, Model.cs, Corpus.cs, DiskCounts.cs,
  WordList.cs, WordNormalization.cs and the command-line option definitions.
- `tools/NgramTrie.UI`: Program.cs and UiSettings.cs.

The port retains n-gram weighting, size-budget selection, corpus normalization,
language filtering and compact NGTR v3 compatibility. Disk-based counting uses
native sorted chunk/merge files instead of the C# SQLite dependency.

The bundled `wordlists` files and their original `LICENSE.md` are copied without
modification from `tools/NgramTrie/Wordlist`. That notice identifies English
public-domain data and the Spanish, French, German and Greek vocabulary sources
and licenses. It is distributed with the installed vocabulary files.

`CaseFold.txt` comes from the original generator. Native Unicode lookup tables
are generated during development and checked in; generation and normalization
do not invoke Python or C# at runtime.
`UnicodeTables.inc` contains modified lookup tables generated from Python's
Unicode 14.0.0 database and the original case-fold mappings. The Unicode data
notice is retained in `wordlists/Unicode-LICENSE.txt` (source:
https://www.unicode.org/license.txt). The generation script records its database
version in the checked-in output.
