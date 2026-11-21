# Native language model generator checks

`Generator.cpp` compares native generation against the checked-in outputs of
the original ESPOS C# generator. The authored test corpora cover ranking,
weights, n-gram limits, byte budgets, both binary formats, full Unicode folding
and the Spanish, French, German and Greek bundled word filters. Compact models
also load through the device predictor and produce real suggestions and typo
correction.

The same test checks memory/disk counting, small external-merge chunks, worker
counts, UTF BOM decoding, cache reuse/invalidation/corruption, output protection
and cancellation. CTest requires neither .NET nor the ESPOS source tree.

To refresh reference files, build the original ESPOS generator, then run:

```powershell
python Tests/Integration/LanguageModels/MakeReference.py <ESPOS/tools/NgramTrie>
```

The script copies the original build into this repository's ignored `build`
folder before running it. All generated outputs and .NET temporary files stay
in this workspace. The original project is read-only. `fixtures/reference.json`
records the C# source and reference assembly hashes.

The `--language-models` editor test drives the LVGL controls, generates and
loads a model into Advanced Keyboard, checks undo/redo and replacement
confirmation, cancels jobs using Stop and X, and isolates generator settings
from the user's settings.
