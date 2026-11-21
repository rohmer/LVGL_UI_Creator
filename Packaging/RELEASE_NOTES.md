# LVGL UI Creator 0.9.0

Windows builds now support x64 (default) and x86, with x32 as an alias. The
CPack NSIS installers are provided separately for each architecture and include
the matching native display SDK. Linux desktop packages are built separately
for x86_64 and ARM64/Raspberry Pi. See docs/Packaging/WINDOWS.md for Windows
build and package commands.

This release adds measured performance dashboards, lazy runtime screen loading,
multi-screen editing and animated navigation, responsive device comparisons,
native Flex/Grid authoring, connected project build tools, actual-screen theme
previews, reviewed component updates, and a persistent Problems panel. Document
caching and patch-based undo reduce repeated work and large-asset copying.
Command/property search and native visual/input/performance regression fixtures
complete the editor workflow. See docs/Docs/Professional-workflows.md.

The project creator now includes **Raspberry Pi - Linux** under the Raspberry Pi
vendor filter. It creates an offline CMake build of the current screen using
LVGL 9.6, the portable Protobuf runtime, bundled SDL3 and a project-specific
LVGL source profile. JSON, C and Protobuf choices all include the runnable
screen; the built-in entry survives cached catalogs and refreshes.

This update adds an in-editor Design/Run toggle (F6), a real two-model prediction
playground, a visual widget-event editor, linked reusable components, screen
validation with selectable issues, and build/flash/serial tools for installed
Arduino, ESP-IDF and CMake toolchains. Run uses an isolated portable Protobuf
runtime so testing does not alter the authored screen.

The theme gallery includes 14 curated light and dark palettes, live miniature
previews, project/selection application, shared color/radius/spacing tokens,
new-widget defaults and undo. Local style and component overrides survive
shared updates. See docs/EDITOR_WORKFLOWS.md and docs/Docs/Themes-and-components.md.

Menus are organized by task: File handles projects, Assets contains font/image
managers and converters, and Export contains Protobuf, LVGL source and firmware
project output. Recent projects, alignment, distribution and panel controls
have their own submenus. View also provides Fit, Actual size and zoom commands.
All eight top-level menus have Alt shortcuts and support arrow-key, Enter and
Escape navigation. Save updates the known JSON path; Save As chooses a new path.

A top-level Language Models menu opens a native LVGL/C++ port of the ESPOS C#
NgramTrie generator. It trains compact NGTR v3 models from corpus folders, with
five bundled language vocabulary filters, model byte budgets, n-gram limits,
background progress, cancellation, reusable counts and disk-based counting.
Generated and existing models can be loaded directly into the current project
and attached to Advanced Keyboard. The generator runs without .NET.

This development release adds work recovery and session preferences, a consistent inspector, editing refinements, export preflight, a welcome screen, diagnostics, native Bootstrap-inspired presets and admin examples.

It also includes native Code 128/GS1 barcode and QR-code widgets, a standalone Windows Protobuf display DLL/viewer and the editor's F5 screen-debugging command. The simulator preview stays open without a close button. The ESP32 source toolkit builds standalone project-specific components with only the needed LVGL modules; see docs/Runtime/esp32 and docs/Runtime/windows for integration.

Advanced Keyboard ports the ESPOS keyboard and prediction/autocorrect engine. It targets external text fields, automatically scrolls the selected field into view above the keyboard, and stays open until Enter or its X button closes it. The inspector's Language dropdown selects English (US), Spanish, French, German, Italian or Portuguese key layouts. A separate Prediction trie dropdown selects built-in English or a project model; Load trie… embeds a specific NGTR model for suggestions and autocorrect. Language and trie choices survive JSON/Protobuf, undo/redo and C export. Temporary scrolling preserves the saved layout. C exports include the portable C++17 engine. ESP32, Raspberry Pi/Linux and Arduino toolkits provide project-specific runtimes with the same widgets and model behavior. See docs/UICreator/AdvancedKeyboard.md and docs/Runtime for integration.

The Bootstrap library simulates component patterns with LVGL controls. It is not an HTML/CSS renderer. See the component coverage document for supported interactions and export limitations. Example forms and admin data do not connect to authentication, payment, or server services.

Extract the entire release archive, then run UICreator.exe. Keep the converter, project-creator and language-models folders beside it. For a per-user installation and Start menu entry, run Install.ps1 from PowerShell. No administrator privileges or changes to PATH are needed. A conventional NSIS installer can be built using `cpack -G NSIS` when NSIS is installed on the build machine.

Save JSON projects for continued editing. Keep application code outside generated firmware folders. Generated sources require the selected LVGL version and the configuration listed in ui_requirements.h.

## Build a release

Build Release with CMake, run CTest, then run `cpack --config build/release/CPackConfig.cmake`. Artifacts are written to build/release/packages. ZIP packaging is available without additional installer tools. Build matching symbols separately and retain them for crash analysis.
