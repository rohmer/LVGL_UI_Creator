The native editor integration executable accepts `--visual-baselines <output-directory>`. It renders 14 project themes at 320x240, 480x320 and 800x480 into isolated native LVGL framebuffers. The fixtures use the configured built-in LVGL font, static control state, and no installed system fonts or animated caret. The authored project is unchanged. The capture manifest records SDL/platform/LVGL/font configuration and every named case.

Run a capture and exact comparison on the same host configuration:

```powershell
python Tests/Visual/run.py --editor build/professional/bin/UICreatorEditorTests.exe --baseline Tests/Visual/baselines/windows --output build/visual-current
```

Use the actual executable path from the CMake build. The runner selects SDL's dummy video/audio drivers and software renderer, executes the checked keyboard replay, and records compiler/test output in `capture.log`. It then enforces real native render/input/prediction timing and input/framebuffer/object-count budgets from `desktop-budgets.json`. The default guards include 16.67 ms p95 rendering/input, 5 ms p95 prediction, 1000 ms load, and 1 MiB tracked LVGL allocation peak. That allocation counter covers LVGL allocations rather than whole-process or device RAM. These desktop smoke limits are explicit regression guards, not device performance estimates. Use `--budgets <machine-specific.json>` for tighter CI limits. Unsupported allocation/RAM/firmware measurements cannot pass a budget that requires them.

After reviewing a deliberate visual change, explicitly update that host's compact PNG baselines:

```powershell
python Tests/Visual/run.py --editor <editor-integration-executable> --baseline Tests/Visual/baselines/windows --output build/visual-current --update
```

`--update` is never implied by a missing baseline. Budget and replay failures prevent updates. Commit reviewed baseline PNGs and their manifest for the CI host. Use separate directories for Windows/Linux or changed LVGL/font configurations. No baseline is portable across an unreviewed configuration change.

The fixture runner reads `keyboard-replay.json` (override with `--replay`) and asserts text delivery to two different text boxes, Backspace, actual prefix prediction lookups, and 30 explicit native redraws. `performance.json` contains the measured report; `replay-trace.json` contains each successful action and observed target text; `budget-comparison.json` names every measured budget. A failed native assertion returns a failing process exit code.

To recheck saved artifacts without launching the editor:

```powershell
python Tests/Visual/run.py --existing --baseline Tests/Visual/baselines/windows --output build/visual-current
python Tests/Visual/compare.py --actual build/visual-current --baseline Tests/Visual/baselines/windows --output build/visual-diff
python -m unittest discover -s Tests/Visual -p test_regression.py
```

Comparisons default to zero changed pixels and zero channel tolerance. Changes write a red difference PNG and a JSON report. Optional `--masks <file.json>` maps case IDs to rectangles with `x`, `y`, `width`, `height`, and a required reviewable `reason`; the report counts excluded pixels. For standalone comparisons, a nonzero `--channel-threshold` or `--max-different-pixels` requires `--tolerance-reason`. The normal regression runner always uses exact comparisons. Avoid masking actual controls or text just to pass a changed render.

The integration check `--polish-core` covers multi-screen CRUD/reference remapping, clean screen selection, direct Save versus Save As, command/property search routing, undo patch storage for large assets, and idle document serialization. Run this alongside visual replay; screenshots alone cannot detect those behavioral regressions.
