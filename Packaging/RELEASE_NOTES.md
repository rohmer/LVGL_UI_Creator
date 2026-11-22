# LVGL UI Creator 0.9.1

This release adds Figma importing, MicroPython and UI Creator XML export,
Raspberry Pi Pico projects, and substantial generated-code and runtime memory
improvements. It also includes the fixes and SDK updates made after the 0.9.0
public release.

## Import Figma designs

- **File > Import Figma design...** imports a whole design or selected frames
  into editable screens. Choose a scale, review conversion notes, then add
  screens to the current project or replace it. Each import is one undoable
  edit; reviewing a design does not change the project.
- **Online Figma link** reads design and frame links through Figma's REST API.
  Enter a personal access token with `file_content:read` permission. The token
  is masked and is not saved in projects, generated code, logs or request URLs.
  Download progress, cancellation, permission errors and API rate limits are
  handled in the import window.
- A bundled local Figma export plugin creates self-contained `.figma.json`
  files without an API token or network connection. The importer also reads
  Figma REST JSON and `JSON_REST_V1` node exports.
- Frames, groups, text and supported shapes retain editable hierarchy,
  geometry and styles. Complex artwork uses embedded PNG images. Online
  requests batch renders, fetch artwork for the chosen frames and reuse cached
  images. Repeated imports reuse identical resources and resolve ID/name
  collisions without changing existing bindings.
- Font substitutions and unsupported behavior are reported. Auto layout uses
  the resolved geometry at the exported size; prototype interactions,
  responsive rules and application behavior need configuration in UI Creator.

See [Figma import instructions](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/Integrations/Figma/README.md).

## Export MicroPython projects

- **Export > MicroPython project...** and both command-line compilers now
  generate projects for the official LVGL MicroPython binding, pinned to its
  LVGL 9.3.0 revision. Use `--format micropython` in the CLI.
- Generated projects include `ui.py`, `ui.json`, the portable Python runtime,
  an example entry point, deduplicated pixel assets, firmware requirements and
  a manifest recording upstream source revisions.
- Supported widgets, layouts, multiple screens, interactions and variables
  bound to several widgets use the native binding API. Images are decoded
  during export. QR codes and barcodes become bitmap assets.
- Preflight reports unsupported widgets/styles/fonts before export. Advanced
  Keyboard, Text box and Layer view use C, Protobuf or UI Creator XML output;
  the pinned binding also lacks native Spinner support. Dynamic barcode/QR
  bindings are rejected instead of silently becoming static.
- Regeneration removes only obsolete assets owned by the previous export
  manifest and preserves user files. Native-binding tests cover garbage
  collection, retained keyboard layouts, assets and repeated lifecycle use.

See the [MicroPython guide](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/CodeGeneration/MicroPython/README.md).

## Read and export UI Creator XML

- **Export > UI Creator XML (.xml)...** writes one self-contained, readable
  project file. Both CLIs accept XML input and support `--format xml`; the
  editor opens XML projects and offers JSON when saving an editable project.
- UI Creator XML v1 represents the same typed project model as Protobuf,
  including widgets, screens, variables, histories, bindings, images, fonts,
  actions and Advanced Keyboard models. It uses UI Creator's own schema.
- Existing runtime memory/file loading APIs and desktop viewers automatically
  recognize Protobuf or XML and use the same validator and renderer. XML input
  can be released after loading; its resources remain owned by the runtime.
- Full builds include XML by default. Minimal ESP32, Arduino, Linux and Pico
  profiles omit the parser unless enabled, preserving the default small build.
  ESP32, Arduino and Pico generators accept `--enable-xml` for runtime
  projects; Linux runtime builds expose the corresponding CMake option.
- The codec bounds input size, nesting, elements and text, rejects malformed
  typed values and does not resolve external entities. Failed exports preserve
  existing output files.

See the [XML export and runtime guide](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/CodeGeneration/XML.md).

## Raspberry Pi Pico 1 and Pico 2

- The firmware project creator now includes Raspberry Pi Pico 1/RP2040 and
  Pico 2/RP2350 Arm targets using the Pico SDK.
- Generate direct C projects or projects using the portable Protobuf runtime,
  with the selected widgets, images, fonts, variables and keyboard models.
  Runtime projects can opt into the XML reader.
- Device tools recognize the Pico SDK, board choice and Arm toolchain and can
  run native Ninja builds. Generated projects produce UF2 firmware.
- Display flush and input callbacks remain application-specific; connect them
  to the panel and touch hardware used by the board.

See the [Pico SDK guide](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/Runtime/pico/README.md).

## Smaller and faster generated C

- Fixed labels, checkbox captions and dropdown options can reference immutable
  text directly. Read-only byte arrays and typed assets share storage; writable
  canvas buffers remain independent.
- Static images no longer need animation tables/timers, and automatic button
  captions avoid unnecessary persistent handles and deletion callbacks.
- Small object lookup tables use switches; larger tables use binary search
  over immutable ID/slot arrays without an extra writable table.
- Bounded variable histories use typed circular storage rather than shifting
  every element. Single-entry histories use inline storage. Numeric text is
  formatted once per update across a variable's text bindings.
- The measured Windows host fixture reduced requested LVGL heap after layout
  from **255,640 to 222,328 bytes** and a filled 1,024-entry `uint8_t` history
  from **8,192 to 1,024 bytes**. These are fixture measurements, not device RAM
  or frame-rate guarantees.

Re-export projects to obtain these changes. Applications that directly mutate
fixed label buffers can disable static text with `UI_USE_STATIC_TEXT=OFF` or
make a label mutable before editing it. See the
[generated C performance guide](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/CodeGeneration/PERFORMANCE.md).

## Runtime memory and ESP32 PSRAM

- The bundled LVGL now removes only its own temporary screen-deletion callbacks,
  preserving application trackers even when the linker merges identical callback
  functions. This fixes stale screen handles during lazy unloading in optimized
  Windows builds, including the 32-bit build, without disabling code folding.
- The loader moves decoded screen definitions into retained storage, validates
  parent chains without repeatedly allocating ancestor paths, and avoids
  unnecessary retained variable/history metadata.
- Scalar variable updates validate values directly, and bounded histories use
  ring buffers. The benchmark's 5,000 scalar updates with initial history
  dropped from 570,000 C++ allocations to zero; bound LVGL widget updates can
  still allocate through LVGL.
- Identical trie catalogs share immutable model/index bytes. On ESP-IDF and
  Arduino ESP32, available PSRAM is preferred for trie data and index storage,
  with normal-memory/flash fallback when unavailable or exhausted. Shared
  owned storage is released after its last owner is destroyed.
- Owned loading avoids temporary full trie copies before placement and
  deduplication. Its input may be released after loading; the explicit
  borrowed-assets API retains its existing input-lifetime requirement.
- In the measured four-screen fixture, peak C++ loading allocations fell from
  **411,992 to 239,968 bytes**. A fixture referencing identical trie data under
  20 IDs fell from **657,876 to 64,841 bytes**. These figures exclude C/LVGL
  allocations, framebuffers, allocator overhead and existing input buffers.

See [runtime memory and lifecycle details](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/Runtime/PERFORMANCE.md).

## Builds, packages and regression coverage

- Release installers are provided separately for Windows x64, Windows x86,
  Linux x86_64 and Linux ARM64/Raspberry Pi; Linux also has portable tar archives.
  Windows x64 remains the default. The x86 package includes genuine 32-bit
  conversion helpers and runtimes as well as the editor, compiler and viewer.
- Complete CMake and Visual Studio source registrations, restored build files,
  CMake 4.5.1 workflows, target architecture audits and regenerated SDK inputs
  make the source checkout and packaged application reproducible together.
- New exporters are available in the editor and CLI tools; Figma importing is
  exposed in the editor. Linux online Figma import declares its system libcurl
  and CA certificate requirements.
- Expanded tests cover generated C memory behavior, PSRAM sharing/fallback,
  MicroPython on the real upstream binding, XML codec/runtime parity and
  parser-free profiles, Pico generation, Figma conversions, online download
  errors/caching/limits, editor typing, resource collisions and undo/redo.
- Installer smoke tests now exercise MicroPython conversion helpers in
  addition to the existing native tools. Build instructions are consolidated
  in [BUILD.md](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.1/BUILD.md).

Save editor JSON projects for continued authoring. Generated firmware requires
the chosen LVGL configuration and board-specific display/input integration.
Protobuf remains the compact runtime format; XML favors readable interchange.
Online Figma importing requires file access and is subject to Figma's API quotas.

[Full source comparison from 0.9.0](https://github.com/rohmer/LVGL_UI_Creator/compare/0.9.0...0.9.1).
