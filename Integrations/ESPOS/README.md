# Advanced Keyboard synchronization

The portable implementation in `LVSerial/Widgets/AdvancedKeyboard` is shared
with the ESPOS `Keyboard/AdvancedKeyboard` component. Board startup/display
code remains outside that component. ESPOS compatibility adapters are reviewed
sources under `templates`, with explicit destination paths in `manifest.json`.
The shared model catalog and its binary files live under the component's
`bundled` directory. The top-level ESPOS `Keyboards` directory also receives
the canonical layout JSON files; additional ESPOS profiles are preserved.

The tool uses Python 3.10+ and the standard library. It does not search for Git
repositories, initialize a repository, stage files or push commits. Always
provide the intended ESPOS project directory explicitly. In particular, an
ancestor repository in a home directory is not the ESPOS project.

From the Creator checkout, compare without changing either tree:

```powershell
python Integrations/ESPOS/sync_keyboard.py --espos <ESPOS-project-directory> --check
```

Checks print the status, managed file count, differences and conflicts. Add
`--plan-out <path>` to save the complete hashes and baseline details for review.

After a shared change in Creator, review and apply the push:

```powershell
python Integrations/ESPOS/sync_keyboard.py --espos <ESPOS-project-directory> --push --dry-run --plan-out build/keyboard-sync/plan.json
python Integrations/ESPOS/sync_keyboard.py --espos <ESPOS-project-directory> --apply-plan build/keyboard-sync/plan.json
```

Use `--pull` instead of `--push` when the shared files were edited in ESPOS.
The baseline is a three-way comparison: an edit on the receiving side, or
competing edits on both sides, is preserved and reported as a conflict. Resolve
the differences deliberately, then create a new plan. There is no implicit
force mode. When one layout copy changed and its other view still matches the
baseline, a pull updates Creator and the unchanged peer mirror together.
Additional top-level ESPOS keyboard profiles and unlisted bundled models are
preserved. Add new shared assets to Creator and the manifest before syncing.

The first migration may replace known old adapters that have no common
baseline. Only that reviewed initial push uses `--bootstrap`:

```powershell
python Integrations/ESPOS/sync_keyboard.py --espos <ESPOS-project-directory> --push --bootstrap --dry-run --plan-out build/keyboard-sync/migration.json
python Integrations/ESPOS/sync_keyboard.py --espos <ESPOS-project-directory> --apply-plan build/keyboard-sync/migration.json
```

The plan records every source/destination hash. Applying it refuses stale or
modified plans. Unknown peer-only files are preserved even during bootstrap.
Retired duplicate sources are removed only when explicitly named in the
manifest with their known prior hash. There are no recursive deletes.

Every applying operation saves a ZIP under `build/keyboard-sync/backups`, uses
temporary files beside each destination, verifies the resulting hashes and
updates the baseline last. Copy failures roll back already-written files.
User corpora, settings, build outputs and board files outside the manifest are
not sync inputs. Resolved paths must stay inside the declared roots; real
symlink/junction escapes are rejected. OneDrive cloud placeholders are allowed.

Validate the first migration against an isolated copy of ESPOS before changing
the real checkout. `--staging` requires both the copied peer and its separate
`--state` file to be under this Creator checkout's `build` directory, so this
validation cannot advance the committed baseline:

```powershell
python Integrations/ESPOS/sync_keyboard.py --espos build/espos-staging --staging --state build/keyboard-sync/staging-state.json --push --bootstrap --dry-run --plan-out build/keyboard-sync/staging-plan.json
python Integrations/ESPOS/sync_keyboard.py --espos build/espos-staging --staging --state build/keyboard-sync/staging-state.json --apply-plan build/keyboard-sync/staging-plan.json
```

Build and test the staged project, then create a fresh plan for the real peer.
Use the same explicit `--state` when checking the staged project again.

UTF-8 source text is compared with CRLF normalized to LF and new copies use LF.
This prevents Git checkout line endings from producing false differences on
Linux CI. NGTR, NGIX and other binary assets are copied and hashed byte-for-byte.

The committed `sync-baseline.json` contains only relative owned paths and common
hashes. It provides an offline CI guard without requiring another checkout:

```powershell
python Integrations/ESPOS/sync_keyboard.py --check-source
python -m unittest discover -s Tests/UnitTests/KeyboardSync -v
```

CMake registers these as `UICreatorKeyboardSyncSource` and
`UICreatorKeyboardSyncUnit`. A changed, added or removed shared source, layout,
model, adapter or manifest causes the source check to fail until a completed
sync records the new baseline. Run both projects' relevant native tests/builds
after syncing; a hash check does not replace behavior tests.

Run the model generator's source-only tests with:

```powershell
python -m unittest discover -s Integrations/ESPOS/tests -p test_bundled_models.py -v
```

This suite is also registered as `UICreatorESPOSBundledGenerator`. The native
behavior fixtures exercise language/model selection, independent predictors,
Pinyin, legacy board controls, bound external fields, deletion, and headless
partition-model lifetime/PSRAM fallback. From a compiler developer shell (for
example, the matching Visual Studio x64 Native Tools environment), configure
them with an existing LVGL static library and the exact configuration header
used to build that library. The library must match the compiler and architecture:

```powershell
cmake -S Integrations/ESPOS/tests/BundledModelsNative -B build/espos-bundled-tests -G Ninja -DLVGL_ARCHIVE=<absolute-LVGL-library> -DLV_CONF_PATH=<absolute-matching-lv_conf.h>
cmake --build build/espos-bundled-tests
ctest --test-dir build/espos-bundled-tests --output-on-failure
cmake -S Integrations/ESPOS/tests/NativeAdapter -B build/espos-adapter-tests -G Ninja -DLVGL_ARCHIVE=<absolute-LVGL-library> -DLV_CONF_PATH=<absolute-matching-lv_conf.h>
cmake --build build/espos-adapter-tests
ctest --test-dir build/espos-adapter-tests --output-on-failure
```

On Linux, use the normal C/C++ toolchain environment with a matching `.a` archive;
on Windows use the matching `.lib`. Quote paths containing spaces. The ngram
component uses the canonical prediction sources with its private
`ESPOS_KEYBOARD_HEADLESS_PREDICTION` definition. The UI component depends on that
engine and compiles the LVGL facade, so engine symbols occur only once.

These host tests and staged ESP-IDF builds do not flash a board or verify touch
interaction on hardware. Choose bundled model pairs for the available firmware
partition: the sum of each selected NGTR and generated NGIX index, plus the
application and fonts, must fit. The generated `bundled-models.json` reports
model/index byte totals; profile names such as 16 KiB are not total firmware
sizes. Keep an external NGPK partition image within its separately configured
ngram partition size.

To invoke the tool while working in ESPOS, supply the Creator checkout too:

```powershell
python <Creator-checkout>/Integrations/ESPOS/sync_keyboard.py --creator <Creator-checkout> --espos <ESPOS-project-directory> --check
```

Keep machine-specific absolute paths in the shell or an ignored
`sync.local.json`, not in the committed manifest/baseline. If the peer is not
available, report that parity is unverified and preserve the completed local
work; do not silently advance the baseline.
