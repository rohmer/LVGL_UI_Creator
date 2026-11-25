# ESPOS integration and canonical keyboard dependency

All portable keyboard code, layouts/fonts, language-model tools and bundled
tries are maintained in [AdvancedKeyboard](https://github.com/rohmer/AdvancedKeyboard).
Creator consumes its pinned submodule; ESPOS consumes the same locked commit
from `third_party/AdvancedKeyboard`. See
[dependency setup/maintenance](../AdvancedKeyboard/README.md).

This manifest deploys only project-specific build wrappers, board/startup
adapters, dependency lock/bootstrap helpers and compatibility launchers. The
`Keyboard/AdvancedKeyboard` folder is an IDF wrapper, not a source mirror.
`ngram_trie` compiles the canonical headless engine once; the UI component
compiles the widget facade and links that engine. Generic ESPOS view/partition
adapters are consumed directly from canonical `integrations/espos`.

## Reviewed adapter deployment

Always supply the explicit ESPOS project directory. This workflow never uses
an ancestor repository, initializes ESPOS Git, stages files or publishes commits.

```sh
python Integrations/ESPOS/sync_keyboard.py --espos /path/to/ESPOS --check
python Integrations/ESPOS/sync_keyboard.py --espos /path/to/ESPOS --push --dry-run --plan-out build/keyboard-sync/plan.json
python Integrations/ESPOS/sync_keyboard.py --espos /path/to/ESPOS --apply-plan build/keyboard-sync/plan.json
python /path/to/ESPOS/tools/setup_advanced_keyboard.py --project-root /path/to/ESPOS --kind espos --init
python Integrations/ESPOS/sync_keyboard.py --espos /path/to/ESPOS --check
python Integrations/ESPOS/sync_keyboard.py --check-source
```

Use a reviewed `--pull` for adapter changes authored in ESPOS. Plans snapshot
source/target hashes; stale or conflicting edits are preserved. Applying writes
creates a backup ZIP, uses atomic individual file replacement and rolls back
failed writes. `sync-baseline.json` records only a completed deployment.

To review a single adapter independently, add `--only CMakeLists.txt` (or
repeat `--only` for several manifest target paths) when creating a push/pull
plan. Apply the resulting plan without `--only`; its reviewed scope is fixed.
Other adapters and their baseline records remain untouched. Run the full
checks above after finishing the remaining adapter changes.

The migration retires old shared copies only through explicit, known-hash
tombstones. Unknown custom layouts/models, source changes, corpora, personal
settings, OpenNLP downloads, board configuration and build outputs are preserved.
The user-owned `Trie` data and custom `Keyboards` profiles remain available as
input files; the shared catalog/runtime no longer reads copied project data.

After changing shared source, publish AdvancedKeyboard and advance both pins;
do not add source or model entries to this adapter manifest. Run dependency,
adapter, Creator/export and ESP32 checks. If ESPOS is unavailable, report that
peer integration is unverified rather than advancing a fabricated baseline.

```sh
python -m unittest discover -s Tests/UnitTests/KeyboardSync -v
python -m unittest discover -s Integrations/AdvancedKeyboard/tests -p test_dependency.py -v
python -m unittest discover -s Integrations/ESPOS/tests -p test_dependency_helper.py -v
```

Native adapter fixtures accept an explicit `ESPOS_TEST_SOURCE_ROOT` to compile
the actual deployed project wrappers/canonical dependency, or the Creator
dependency by default. A matching LVGL9.6 archive/configuration is required.
Host tests and firmware compile/link do not flash hardware or test a board's
display/touch drivers.
