# Minimal standalone runtime verification

This harness links a **label-only** generated component as a standalone native
library. Barcode, QR, chart, table, arc, bar, button, canvas and other unused
widget sources are absent. It exercises actual loading/rendering, repeated
lifetimes, malformed input, disabled-widget rejection and authored dimensions
after display resize. Linking the final executable detects omitted core source
dependencies that a static-library-only build cannot detect.

Configure `Tests/Integration/Runtime/Minimal` with
`-DLVGL_PB_COMPONENT_DIR=/absolute/path/to/a/generated/label-only/component`,
build, and run its CTest entry. `Verify.cmake` also automates generation from
`label.json` when passed `SOURCE_ROOT`, `BINARY_DIR`, `PYTHON` and optionally
`PARENT_GENERATOR`. Build in a C/C++ compiler environment.

`VerifyCodes.cmake` takes the same arguments plus `PROJECT_FILE` pointing to the
code-widget test's `fixture.pb`. It selects the sibling code-only consumer with
`LVGL_PB_CODES_TEST=ON`, verifies QR/barcode canvas/image dependencies and absent
unrelated widgets, checks embedded-NUL QR payloads, then scans actual RGB565
display flushes for both indexed bitmap colors. It repeats loading/rendering/
cleanup and confirms a chart project remains rejected by that minimal build.

`VerifyKeyboard.cmake` generates the `keyboard.json` profile and selects
`LVGL_PB_ADVANCED_KEYBOARD_TEST=ON`. It links the native ESPOS widget and trie
engine with only button/label/textarea enabled. The consumer checks real model
suggestions, independent textarea focus, Protobuf settings, rendering, two-way
text variable bindings, root events, feature rejection and complete lifetimes.
Pass `PROJECT_FILE` pointing to `keyboard-no-prediction.json` for the seven-source
widget profile with all three prediction source files and default model omitted.
The LVGL keyboard, button matrix, dropdown and flex sources remain omitted.
