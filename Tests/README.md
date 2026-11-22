# Unit and functional tests

The `editor` label now includes `UICreatorRunMode`, `UICreatorDesignTools`,
`UICreatorInteractions`, and `UICreatorDevicePlayground`. These exercise real
pointer/keyboard routing, runtime isolation, shared theme updates after native
serialization, linked component updates and overrides, actions, Undo/Redo,
prediction comparison, explicit trie assignment, tool output and lifecycle.
`DeviceToolsNativeProcesses` independently checks argument/environment fidelity,
streamed output, cancellation and hardware-free tool probes. These tests never
flash a board or open a serial monitor.

The design core checks all fourteen palettes for text contrast, component target
remapping, partial/full duplication, import/export and materialization. Portable
runtime action tests cover conditions, typed variables, validation and safe
deletion; generated action fixtures compile and execute across all six LVGL
backends. Run `ctest --test-dir build/professional -R LVGLGeneratorActions --output-on-failure` after preparing the regular version matrix. See `Tests/Integration/CodeGeneration/Interactions`.

The `menus` label checks real pointer/keyboard menu navigation, action routing,
selection-dependent commands, recent-file flyouts, small-display bounds and
isolation from text input. These checks are also included in the `editor` label.

The `language-models` CTest label runs native generator parity and LVGL editor
integration checks. Eleven tiny NGTR fixtures come from the original ESPOS C#
generator; regular tests require no .NET runtime. They cover compact/legacy
formats, byte budgets, language normalization, disk/count-cache modes,
cancellation, predictor behavior and generated-model import into Advanced Keyboard.
Additional native and managed checks cover Arabic, Hindi, Mandarin Chinese,
Bengali, Indonesian, Urdu and Russian normalization and token boundaries.
`UICreatorKeyboardSuggestionText` checks real trie queries for Indic Nukta
normalization, Han-character context, UTF-8 caret edits and protected fields.

Advanced Keyboard coverage includes actual pointer input, Shift/Shift Lock,
localized layouts, symbol pages, UTF-8 editing, suggestion callbacks,
JSON/Protobuf, runtime text bindings, language/trie inspector selection and
undo/redo, independent generated C consumers and a minimized
ESP32 source profile. Select these checks with `ctest -L advanced-keyboard`.

## MicroPython export

Select the complete MicroPython group with `ctest -L micropython`. The standard
suite includes these checks without downloading or building firmware:

| Test | Coverage |
| --- | --- |
| `UICreatorMicroPythonExport` | Both CLI frontends, JSON/Protobuf input, generated scripts, native bitmap assets, target requirements and unsupported-feature errors. Produces the shared native-test fixture. |
| `UICreatorMicroPythonRoundTrip` | Deterministic regeneration and preserved project behavior across frontends and input formats. |
| `UICreatorMicroPythonAssets` | Image payloads, deduplication, distinct image headers and exported runtime parity. |
| `UICreatorMicroPythonPaths` | Relative assets, spaces and Unicode in source/output paths. |
| `UICreatorMicroPythonRejections` | Invalid inputs and unsupported features fail without overwriting an existing export. |
| `UICreatorMicroPythonRuntime` | Variable directions/history/validation, keyboard ownership, transactional cleanup, resources and screen lifecycle using a bounded LVGL test double. |
| `UICreatorMicroPythonEditor` | Menu routing, editable output path, preflight, output files, unchanged document state and rejected exports. |
| `UICreatorMicroPythonInstalledCLI` (Windows) | Runs the installer smoke checks against both built CLI frontends with a Windows-system-only PATH, checking that export needs no host Python installation. |

After configuring a build with `BUILD_TESTING=ON`, build the executable fixtures
and run the group:

```sh
cmake --build build/release --target UICreatorCodeGen LVGLCompiler UICreatorEditorTests
ctest --test-dir build/release -L micropython --output-on-failure --parallel 4
```

For real binding execution, build LVGL-enabled MicroPython from the pinned
official sources described in [the export guide](../CodeGeneration/MicroPython/README.md).
Ordinary MicroPython without `import lvgl` cannot run this test. On Linux:

```sh
cmake -S . -B build/release -DUICREATOR_MICROPYTHON_EXECUTABLE=/absolute/path/to/micropython
ctest --test-dir build/release -R '^UICreatorMicroPythonNative$' --output-on-failure
```

On Windows, an existing Linux interpreter can run through WSL:

```powershell
cmake -S . -B build/release -DUICREATOR_MICROPYTHON_EXECUTABLE=E:/path/to/micropython -DUICREATOR_MICROPYTHON_WSL_DISTRIBUTION=Ubuntu
ctest --test-dir build/release -R '^UICreatorMicroPythonNative$' --output-on-failure
```

`UICreatorMicroPythonNative` uses a CTest fixture, so selecting only that test
also regenerates the export first. A failed producer prevents the native test
from using stale files. It executes the shipped Python runtime with an in-memory
display, checks native widgets, pixel output, variable bindings, garbage
collection and repeated keyboard lifetimes, and writes
`CodeGeneration/micropython-test/native-verification.json`. The host unit suite
does not substitute for this native test. No panel, SDL window or hardware flash
is required. The native interpreter is optional and is never downloaded by
CTest.

Windows installer and Linux package smoke tests also exercise MicroPython
export through both packaged CLI frontends and verify their bundled output.
Linux desktop release validation includes the MicroPython CTest group.

## Running the full suite

The `xml` group exercises UI Creator's own XML codec and the shared runtime:
typed/Unicode/binary round trips, malformed and bounded input, identical
Protobuf/XML rendering, embedded images/fonts/trie prediction, released-input
ownership, lazy screens, LVGL filesystem loading, CLI conversion and editor
export/open. It also compiles reduced profiles with XML enabled and disabled.
Run `ctest --test-dir build/release -L xml --output-on-failure`; no external
XML tooling or license is required. See [XML integration](../CodeGeneration/XML.md).

From an x86 Visual Studio Developer Command Prompt, configure a Release build with testing enabled, build, then run CTest:

```bat
cmake -S . -B build/release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

Use the generator already configured for an existing build directory. For a Visual Studio multi-configuration generator add `--config Release` to the build and `-C Release` to CTest. Converter tests require the same Node/Python dependencies as the font/image managers. Protobuf conformance additionally runs when protoc is available.

## Cross-version C and C++ firmware matrix

Download the pinned official source headers/implementations before configuring:

```bat
python Tests/Integration/CodeGeneration/Versions/DownloadSources.py
cmake -S . -B build/release -DUICREATOR_TEST_LVGL_VERSIONS=ON
cmake --build build/release
ctest --test-dir build/release -L versions --output-on-failure
```

Sources live under build/compat-lvgl by default; set UICREATOR_LVGL_TEST_SOURCES to change that location. The 9.6 test uses the vendored source. Each registered version compiles generated widgets and shared assets, then runs C and C++ consumers against that real LVGL version. The matrix exercises rendering, screens, bindings/events, resource reuse and repeated initialization/teardown. Older version tests register only when their sources are present; check `ctest --test-dir build/release -N` for all six LVGLGenerator entries. Standalone firmware validation builds the runtime separately without editor/SDL dependencies and checks unknown backend selection. Each available LVGL release also registers an LVGLRuntimeBackend test that independently builds the runtime and runs the same unit suite, full widget rendering, asset rendering, a C firmware consumer, and media replay. It depends on its matching generator test and the 9.6 media fixture producer.

## Coverage

* LVGLRuntimeUnit: malformed/truncated Protobuf, schema/target/hierarchy validation, API ownership, all variable types, bindings/history, resource references, font/image validation and repeated/concurrent runtime lifetimes.
* LVGLGeneratorUnit: target capabilities/default omission, portable coordinates, font repacking, pixel normalization, local image embedding and failure-safe export.
* LVGLRuntime96 / LVGLRuntimeCAPI: actual widget/screen/resource construction, events, shared resources, file/memory loading, rollback and C API compatibility.
* LVGLRuntimeMedia: 59 image format/compression combinations and ten font conversion combinations, including native image decoding and glyph bitmap retrieval. Each LVGLRuntimeBackend entry replays all 69 exported media projects against that native release, retrieves glyph bitmaps and renders the loaded objects.
* SerializationUnitTests / LVSerialDeviceCodec: widget, screen, variable, font/image and style JSON/Protobuf round trips, compact default omission, binary codec limits and malformed input.
* Editor, converter, generated C, image library, crash dump and existing integration checks remain in the full suite.

Run just the two new unit suites with `ctest --test-dir build/release -R "LVGL(Runtime|Generator)Unit" --output-on-failure`. Run executable unit cases directly using `build/release/bin/LVGLProjectUnitTests.exe` for individual assertions and Catch filters. CTest's functional, versions, media and firmware labels select the new functional groups. Nested matrix logs are under build/release/version-matrix/<version>; standalone logs are under build/release/runtime-standalone. Runtime matrix build and nested test logs are under build/release/runtime-matrix/<version>. Use `ctest --test-dir build/release -R LVGLRuntimeBackend --output-on-failure` to run all available runtime backends, after their generator and media prerequisites have passed.

This is host-based software coverage. Device drivers, GPU-only pixel formats, physical display/input behavior, constrained-device memory budgets and every possible malformed byte stream require platform-specific testing; passing this suite does not establish those properties.

Native SDL3 input coverage is registered as UICreatorNativeSDL3 (keyboard/Unicode, shortcuts, mouse, touch, wheel, resize, shutdown). UICreatorUndoRedo exercises editor history; UICreatorHistoryUnit tests grouping, branching and bounds. Project-creator unit tests validate all 67 manifests; functional tests invoke the packaged worker and compile/run its generated C output. Each runtime backend replays shared image/font media fixtures in addition to project loading and C API tests.

`UICreatorCanvasNavigation` covers pointer-anchored and fractional wheel zoom, limits, middle/left-button panning, release outside the canvas, modal isolation, fit and 100% reset, unchanged saved geometry, and undo of widget movement while zoomed.

`UICreatorGridSnapping` covers independent grid/snap/guide controls, spacing, negative-coordinate rounding, zoomed grid snapping, edge alignment guides and cleanup, locked axes, undo/redo, unchanged project data when toggling view settings, and the top-level Assets menu.


`UICreatorMultiSelect` exercises canvas and Hierarchy Shift-click, additive/reverse marquee selection, group movement at non-unit zoom, snapping with preserved offsets, all six alignments, duplication/deletion shortcuts, parent/child selection, and geometry/selection restoration through undo/redo. History unit tests cover selection-only state updates. The editor workflow additionally duplicates image, image-button and font-backed widgets and verifies that shared resource catalogs and references survive undo/redo.

`UICreatorWindowClose` checks X placement, close/reopen for the five closable docks, a preview without a close button, View/Alt+V restoration, minimized-icon cleanup, focus release, project-data preservation, all main dialogs, simulator settings, the button-map editor and the file browser.

`UICreatorInspector` covers validated/partial numeric entry, units/reset controls, object naming, position locking, visibility, serialization and inline-rename undo/redo. `UICreatorSession` uses an isolated build-directory session to exercise recovery, exclusive storage locking, save/discard/cancel guards, native quit, dirty undo, recent projects and workspace persistence.

`UICreatorBootstrap` creates all 49 component/layout recipes, checks native interactions, round-trips JSON/Protobuf, and compares compiler preflight sizes with actual generated files. `UICreatorBootstrapPalette` checks editor insertion, welcome routes, undo/redo and product dialogs. `UICreatorBootstrapGenerated` independently compiles and runs 49 C11 consumers against native LVGL, checking object classes/hierarchy, persistent actions, rendering, and repeated initialization/cleanup. Build the default target or `UICreatorBootstrapGenerated` before running that test. See [firmware harness](Integration/BootstrapGenerated/README.md).

`UICreatorCodeWidgets`, `UICreatorCodeGenerated` and `UICreatorCodeWidgetsEditor` cover native barcode/QR encoding, editable payload/options, rollback, round trips, runtime loading and generated C. `LVGLRuntimeMinimal` and `LVGLRuntimeCodesMinimal` independently bundle and build a reduced LVGL configuration, then render label-only and indexed-code RGB565 scenes, reject disabled widgets and exercise repeated lifetimes. `UICreatorESP32Profile` checks typed wire decoding, feature closure, asset optimization and stale-profile detection. `LVGLProtobufWindows` checks the standalone C ABI; `LVGLProtobufViewer` and `UICreatorDebugScreen` exercise the real viewer, F5 snapshots, refresh and stop.
