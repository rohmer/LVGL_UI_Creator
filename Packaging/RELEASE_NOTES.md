# LVGL UI Creator 0.9.2

This release adds Screen Memory totals, Unicode key variants, more keyboard
languages and bundled prediction models, faster corpus processing, populated
theme previews, and shared Advanced Keyboard sources for ESPOS. It includes
all changes since 0.9.1.

## macOS packages added to 0.9.2

- Separate Intel x86_64 and Apple Silicon arm64 app archives require macOS
  13.5 or newer. Extract the matching ZIP and copy `LVGL UI Creator.app` to
  Applications.
- Both include native font/image/project conversion helpers, the protobuf
  viewer and display library, bundled models and device SDK archives.
- The Linux/WSL build downloads and prepares the Apple SDK, OSXCross, LLVM
  runtime and helper toolchains. See `Packaging/MACOS.md` and `BUILD.md`.
- Packages pass cross-build, architecture, library-path, code-signature seal
  and archive-content checks. They are ad-hoc signed and are not notarized.
  A separate GitHub workflow checks published packages with Apple's verifier
  and exercises their native tools on Intel and Apple Silicon Mac runners.
- These packages are built from the follow-up packaging commit recorded in
  their manifests. The original 0.9.2 tag and Windows/Linux assets are retained.

## Automatic builds and tests

- GitHub Actions builds Windows x86/x64 Debug and Release, Linux x64/ARM64,
  and macOS Intel/Apple Silicon on pushes and pull requests.
- Desktop jobs run all registered CTests, including generated-C and runtime
  checks against six LVGL versions. Linux x64 also runs the real MicroPython
  binding. Separate jobs test the managed corpus/keyboard tools and compile
  ESP32, Arduino, Pico 1 and Pico 2 firmware.
- Build logs, test inventories, JUnit results and firmware artifacts are kept
  for 14 days. See `Docs/CI.md` for coverage and reproduction commands.

## Screen Memory in the screen frame

- The screen's window frame displays **Screen Memory: N bytes** and refreshes
  as widgets are added, removed, loaded or switched between screens.
- The total sums LVGL widget instance sizes for the screen container and its
  complete child tree, including hidden widgets and controls created inside
  other widgets. Each instance is counted once using its actual LVGL class size.
- This is the running build's widget-structure footprint. Dynamic text/style
  allocations, image/font/trie assets, allocator overhead and framebuffers are
  separate; the value is not a measurement of the complete device RAM budget.

## Advanced Keyboard

- Fourteen layouts are available: English, Spanish, French, German, Italian,
  Portuguese, Greek, Mandarin Chinese, Arabic, Hindi, Bengali, Indonesian,
  Urdu and Russian.
- Hold a character key for one second to open accented or alternate Unicode
  characters beside it. Slide onto a highlighted option and release, or release
  the original key and tap an option. Short taps still enter the original key.
- Variants follow the language and Shift state, support combining sequences,
  and wrap or scroll on small displays. Clicking outside, switching a page or
  language, resizing, dismissing or deleting the keyboard closes the popup.
  One-shot Shift is consumed after insertion; Shift Lock remains active.
- The layout schema, immutable C++ tables, key glyphs and Windows keyboard
  simulator include the same variant definitions and interaction behavior.
- Mandarin supports Pinyin composition and Han-character prediction contexts.
  Arabic and Urdu default to logical right-to-left editing with an explicit
  direction override. Hindi and Bengali retain combining marks and canonical
  query normalization. French, German and Greek queries preserve the relevant
  language-specific case-folding behavior.

## Bundled corpus tries and project exports

- Sixteen corpus trie bundles cover English, Spanish, French, German, Greek,
  Arabic, Hindi and Mandarin, with 16 KiB and 256 KiB profiles for each language.
- Advanced Keyboard provides language and trie-size selection, alongside custom
  trie loading. Selected models and their indexes are embedded in projects and
  generated code; device exports omit unused model assets.
- Firmware builds can compile explicit language/size pairs instead of every
  bundled model. Language switching selects an available compiled size when the
  previously selected size is unavailable.
- ESP32 and Arduino generation retry temporary Windows sharing locks during
  output publication with bounded waits. Conflicting destinations are preserved,
  and persistent filesystem failures remain errors.
- Immutable model and index bytes share one PSRAM allocation on ESP32 when
  available, with flash-backed fallback and independent prediction workspaces.
  Both model and index bytes contribute to the actual storage footprint;
  profile names are not the total firmware size.

## Corpus Workbench and trie generation

- Web crawl, Wikipedia XML, trie generation and keyboard simulation accept
  Arabic, Hindi, Mandarin, Bengali, Indonesian, Urdu and Russian in addition
  to the existing language profiles.
- Corpus Workbench can use the shared native generation backend. Large UTF-8
  files are divided at existing sentence or line boundaries across workers;
  spill chunks and merge concurrency are bounded, and intermediate files stay
  on local storage independently of the chosen model output location.
- A measured 32 MiB French corpus sample improved from 37.9 seconds to
  4.9 seconds with identical generated bytes. On the development host, the
  complete 2.52 GB French corpus finished in 13 minutes 26 seconds using
  24 workers, a 256 KiB model budget and about 237 MB peak working memory.
  Other inputs and hardware will vary.
- Additional-language profiles use Unicode-aware fallback sentence extraction;
  Mandarin models represent individual Han-character n-grams rather than
  dictionary-based Chinese word segmentation.

## Shared ESPOS keyboard implementation

- ESPOS uses the same portable keyboard core, layouts, corpus catalog and
  prediction engine as Creator. Compatibility adapters retain board startup,
  orientation controls and existing owned-editor factory APIs.
- The shared keyboard supports externally owned text boxes, target switching,
  Enter/X dismissal, automatic target visibility, RTL, model selection and
  autocorrect. Headless ESPOS builds keep prediction independent of LVGL.
- Partition-backed model mappings stay alive while a borrowing keyboard uses
  them. The graphical and headless components compile one prediction engine
  and share its model-storage cache.
- A manifest-based sync tool provides reviewed push/pull plans, conflict checks,
  backups, rollback and a committed parity baseline. CTest detects shared source
  changes that have not completed synchronization. ESPOS board configuration,
  corpora and unrelated files are preserved.
- ESPOS adapter build templates track its current `tools/python` and
  `tools/components` layout and bundled FreeRTOS integration for ESP-IDF 6.1.x.
  These ESPOS requirements are separate from the portable generated SDKs.

## Themes and previews

- Eight palettes bring the gallery to 36 themes: Vault, Bloom, Studio and Estate,
  each in light and dark. The palettes draw inspiration from the Dribbble UI
  Template gallery.
- Theme tiles and the detailed preview contain real native controls, including
  buttons, text fields, switches, sliders, dropdowns and progress indicators.
  The current authored screen remains available as an alternate preview.
- Applying a theme is one undoable project change and does not insert the
  preview's sample controls into the design.

## Packages and build information

- Separate Windows x64 and x86 installers, Linux x86_64 and Linux ARM64/Raspberry
  Pi installers and portable Linux archives are supplied with matching SDKs.
- Application resources, installer metadata and generated SDK manifests identify
  this release as 0.9.2. See the attached checksums and package manifests for
  artifact sizes, architectures and payload verification.
- See [BUILD.md](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.2/BUILD.md),
  [Advanced Keyboard](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.2/LVSerial/Widgets/AdvancedKeyboard/README.md),
  [bundled trie catalog](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.2/UICreator/KeyboardModels/README.md)
  and [ESPOS synchronization](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.2/Integrations/ESPOS/README.md).

[Changes since 0.9.1](https://github.com/rohmer/LVGL_UI_Creator/compare/0.9.1...0.9.2).
