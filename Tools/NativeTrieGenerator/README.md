# Native Corpus Workbench trie backend

Corpus Workbench and the standalone trie window prefer `UICreatorModelGen` when
it is available. The native C++ engine also runs inside LVGL UI Creator. It uses
bounded counting chunks, parallel UTF-8 ranges, local temporary storage and the
same NGTR formats as the managed generator. The managed backend remains
available when the native helper is absent. The selected backend is shown in
the trie window; a failed native build is reported rather than retried silently.

From a Visual Studio developer command prompt, or Linux with CMake and a C++17
compiler:

```sh
cmake -S Tools/NativeTrieGenerator -B build/native-trie -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-trie --config Release --parallel
```

The helper and its bundled word lists are placed in `Tools/NativeTrieGenerator/bin`.
Visual Studio builds use that same directory for every configuration. For a
32-bit Windows helper, configure a separate build directory with `-A Win32`;
use `-A x64` for 64-bit Windows. Match the helper to the Windows system running it.
Alternatively set `UICREATOR_MODELGEN` to an existing helper executable. Full
desktop builds install it beside the editor.

```sh
Tools/NativeTrieGenerator/bin/UICreatorModelGen --language fr --trie-output model.ngtr --trie-size 262144 --workers 8 --low-memory --compact -- corpus-folder
```

On Windows the executable has the `.exe` suffix. Existing parent folders for
output files must exist. Add `--no-cache` for a fresh count, `--json-output` for
the selected n-gram counts and weights, or `--scratch-directory` to choose a
different local working disk. Do not place the working directory on a synced
or network drive when throughput matters. Final output replacement is atomic.

The helper accepts the managed trie's `--unigrams`, `--bigrams`, `--trigrams`,
`--no-subdirectories`, `--word-list` and `--no-wordlist` controls. Without
`--trie-size` the size is unlimited; without `--compact` it writes legacy v2.
Progress uses `@progress` tab-separated records and completion uses `@result`;
writing `cancel` followed by a newline to standard input requests a graceful
stop without replacing existing outputs.

Languages: English, Spanish, French, German, Greek, Arabic, Hindi, Mandarin
Chinese, Bengali, Indonesian, Urdu and Russian. The first five have bundled
vocabulary filters. The remaining languages train directly from the corpus;
an explicit custom word list can restrict them. Arabic, Hindi, Bengali and
Urdu retain combining marks and in-word joiners. Mandarin counts Han characters
as tokens, so its n-grams represent one, two or three consecutive characters;
a custom Mandarin vocabulary should list individual Han characters.

The portable generator is in `UICreator/LanguageModels`. The ESPOS copy keeps
the same sources under this tool's `source` folder so it can build independently.
See its provenance and Unicode notices before redistributing derived tools.
