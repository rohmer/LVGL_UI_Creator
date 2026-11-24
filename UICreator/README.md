# LVGL UI Creator

**Assets > Theme gallery…** provides 14 curated light/dark palettes with live
previews, shared tokens, project/selection application and undo. **Assets >
Reusable components…** stores linked groups with shared masters and local
overrides. See [themes and components](../Docs/Themes-and-components.md).

Use **Run (F6)** to interact with the screen through the portable Protobuf
library inside the editor; Escape returns to Design. **Edit > Edit widget
events…** adds actions and **Debug > Validate screen…** checks the design.
**Language Models > Model playground…** compares prediction tries before loading.
**Export > Build, flash and serial monitor…** runs installed Arduino, ESP-IDF
or CMake tools. See [the workflow guide](../EDITOR_WORKFLOWS.md).

**Language Models > Generate language model…** (Alt+L) opens the native C++ port
of the ESPOS NgramTrie generator. Train compact models from corpus text with
language vocabulary filters, byte budgets, n-gram limits, disk-based counting,
progress and cancellation. Load a generated model into the current project and
select it for Advanced Keyboard. See [language model tools](LanguageModels/README.md).

The menu bar groups commands by task:

| Menu | Commands |
| --- | --- |
| File | New, Open, Recent projects, Save project…, Exit |
| Edit | Undo/Redo, Duplicate/Delete, Align/Distribute and widget event actions |
| View | Fit canvas, Actual size (100%), zoom, Show panel, Restore all panels and workspace preferences |
| Assets | Theme gallery, reusable components, project fonts/images and converters |
| Language Models | Generate, load or compare models in the playground |
| Export | Protobuf, LVGL source, firmware projects and build/flash/serial tools |
| Debug | Run/Design, validation and standalone viewer |
| Help | Welcome and examples, keyboard shortcuts, diagnostics and About |

Use Alt+F/E/V/A/L/X/D/H to open File/Edit/View/Assets/Language Models/Export/Debug/Help.
Within a menu, Up/Down selects a command, Right opens a submenu, Left returns to
its parent and Enter activates the selected command. Escape returns from a
submenu first, then closes the top-level menu. **File > Save project…** opens
the save dialog; Ctrl+S uses the same command.

**Debug > Debug current screen** (F5) opens the current design in the standalone Windows Protobuf display library. It writes a separate temporary Protobuf snapshot; saving the editor project is unnecessary. F5 again refreshes the same viewer. **Debug > Stop debug viewer** closes it. The simulator preview has no close button.

**Advanced Widgets** now includes native **Barcode** (Code 128/GS1) and **QR code**, with editable content and encoding options. See [code widget details](Codes.md). Use the [ESP32 component builder](../Runtime/esp32/README.md) to bundle a project-specific runtime and the required LVGL sources, or the [Windows display SDK](../Runtime/windows/README.md) to display Protobuf from a desktop application.

**Advanced Keyboard** ports the ESPOS keyboard and prediction/autocorrect engine. It targets external text fields, switches with field selection, and stays open until Enter or its X button closes it. Its inspector offers English (US), Spanish, French, German, Italian and Portuguese layouts, a prediction-trie dropdown and a **Load trie…** button for a specific model. See [Advanced Keyboard](AdvancedKeyboard.md).

**Help > Welcome and examples** offers recent projects and editable admin dashboard, sign-in and component-gallery examples. The **Bootstrap** and **Templates** palettes provide 36 native component recipes and 13 starter layouts. See [component and template coverage](Bootstrap.md) for supported interactions and firmware behavior.

Interactive sessions protect unsaved changes, periodically save recovery snapshots and remember workspace preferences. Use View > Workspace preferences for autosave timing and panel sizes. See [work recovery and editing shortcuts](SESSION.md).

The inspector groups Layout, Appearance and Interaction settings, shows units, supports reset actions, and explains locked or layout-managed fields. Rename objects in Properties or double-click a hierarchy row. Position locking and visibility survive saving and reopening. Arrow keys nudge selected widgets; Shift increases the step to ten pixels. Edit provides equal horizontal/vertical distribution for sibling selections.

Export dialogs offer an LVGL target and **Check project before export**. C preflight runs the complete generator without writing files and reports generated source sizes. Firmware RAM/flash use depends on your target build. Successful exports offer an output-folder shortcut. Help provides shortcuts, version information and a copyable local diagnostics report.

Click an entry in Widgets, Advanced Widgets, Dialogs, or Custom to create that component in the simulator. The selected hierarchy node determines its parent. Select Screen to create a top-level widget.

The Custom tab provides Text box and Layer view using LVGL 9.6 controls. Text box combines a caption, editable text, and increment/decrement buttons, with optional numeric range and step. Layer view provides independent, selectable, expandable tree rows. Its property editor shows stable layer IDs, with parent 0 indicating a root row. Removing a layer in the editor also removes its descendants.

Custom widget properties, internal styles, contents, selection, and expansion are saved through LVSerial under `textBox` and `layerView` records. Document widgets placed inside a Layer view keep their content host when a project is reloaded.

On Windows, Breakpad is enabled before the editor initializes and writes local crash dumps to `%LOCALAPPDATA%\LVGL_UI_Creator\CrashDumps`. Set `UICREATOR_DUMP_DIR` to override this directory. No crash-generation server or upload service is required. Keep the matching executable and PDB files to inspect dumps in a debugger.

`UICreator.exe --write-crash-dump` writes a diagnostic dump without crashing. The test-only switches `--crash-test` and `--terminate-test` intentionally terminate the process to verify native exception and uncaught C++ exception reporting. CTest runs these in separate processes and checks the resulting minidump files. Crash reporting is currently implemented for Windows.

Select an Image widget and use **Load image from disk...** to import GIF, PNG, or JPG/JPEG. The image source is included in the project, so reopening does not depend on the original disk path. Animated GIFs retain their frames and preview animation. There is no per-image code/file storage setting.

Editor JSON and device Protobuf share the LVSerial models. **Export > Device Protobuf (.pb)…** exports a `.pb` project, which can also be opened in the editor. The separate firmware-facing C codec is described in [LVSerial](../LVSerial/README.md).

**Export > LVGL source project…** generates a folder containing `ui.c`, `ui.h`, `ui_assets.c`, `ui_assets.h`, configuration requirements, CMake integration, a name/ID manifest, and firmware instructions. It covers every palette widget, composite contents, screens, styles, bitmap fonts, typed variables, histories, and supported property bindings. GIF/PNG/JPEG images become compiled pixel arrays; animated GIFs use LVGL timers. No per-image storage option is needed.

The Release build also provides a command-line exporter:

```powershell
.\build\release\bin\UICreatorCodeGen.exe project.json generated_ui
# A saved .pb project is also accepted.
```

Firmware supplies LVGL 9.6.x, display/input drivers and periodic LVGL tick/timer processing. Add the generated folder after the LVGL CMake target, link `generated_ui`, include `ui.h`, and call `ui_init()` after driver initialization. Use `ui_get_object(screen_index, saved_object_id)` to attach device behavior, `ui_show_screen(index)` to switch screens, and the generated `ui_set_v_NAME()` setters to change variables and maintain bindings/history. `ui_deinit()` releases generated objects, timers and variable storage. The generated README explains lifetime and memory requirements. Keep application code outside the generated folder so re-exporting can safely replace generated sources.

CTest compiles a generated project as C11 and runs it against LVGL, covering all 31 serialized widget types, two screens, nested content, custom control events, all variable types, image formats, animation and repeated initialization/cleanup. The editor workflow also exercises C export from the menu.

## Font converter

Open **Assets > Convert font…**, browse to a TTF, OTF, WOFF or WOFF2 font, and choose an output path. The converter uses the pinned official [lv_font_conv](https://github.com/lvgl/lv_font_conv) engine. Add more sources to merge fonts or icon sets. Each source has its own Unicode ranges, literal symbols and light/off/strong hinting. Ranges accept decimal or hexadecimal codepoints, comma-separated ranges, and remapping such as `0xE000-0xE010=>0xF000`.

Options include pixel size, 1/2/3/4/8 bpp, grayscale or horizontal/vertical LCD subpixel rendering, color-glyph information, RLE compression, XOR prefiltering, kerning, forced fast kerning classes, row stride and glyph address alignment. Stride alignment requires compression off and 1/2/4/8 bpp. C output also accepts a font symbol, LVGL include path and fallback font symbol. Relative project paths stabilize generated comments; full-info retains pixel data in debug JSON. The removed upstream `--byte-align` option is represented by stride 1.

Export produces **LVGL C (.c + .h)**, a **binary font**, or a **glyph PNG/JSON dump folder**. C fonts can be compiled with LVGL 9.6; include the generated header and use the declared font in a text style. Enable `LV_USE_FONT_COMPRESSED` for compressed C output and `LV_FONT_FMT_TXT_LARGE` for fonts exceeding the small glyph descriptor limits. Link the named fallback separately. Binary fonts need LVGL's binary font loader and a device filesystem or memory-file driver.

Conversion runs in the background; closing the window cancels its current job. The preview uses packed, uncompressed glyphs independently of export compression/alignment. **Apply to selected widget** stores a stable bitmap font in its main text style. The font survives closing the converter and is included in JSON, Protobuf and generated project C. Missing glyphs should be added to the source ranges or symbols before converting.

CMake and Visual Studio compile the C++ converter directly into the editor. FreeType/Brotli rasterize the source; native code writes LVGL C, binary fonts and PNG glyph dumps. Conversion needs no Node runtime or network connection. See [converter provenance and licenses](../3rdParty/lv_font_conv/UICREATOR.md).

## Project fonts

**Assets > Project fonts…** lists reusable project fonts, their IDs and usage counts. Successful font conversions add their bitmap to this library. Rename fonts without changing their IDs, apply them to the selected widget, or remove unused entries. In-use fonts must first be changed to **Inherit / theme** in their widget properties. Every widget inspector includes a **Font** dropdown; widgets choosing the same entry share its font instance.

JSON and Protobuf store the font library once and use references in widget styles. Original projects with inline fonts remain supported. Converted font bitmaps are embedded, so reopening does not require the original font files. New project clears the font library. Generated C deduplicates font assets and exposes `ui_get_font("font_ID")`; the generated manifest maps IDs to font names and symbols.

## Image conversion

Open **Assets > Convert images…** or **Image conversion options...** in an image widget's properties. Pick multiple PNG/JPEG/BMP/GIF/SVG files, select a folder for recursive conversion, or reuse the selected widget's embedded original. Choose a color format, stride alignment, dithering, alpha preservation, chroma key/background colors, premultiplication, RGB565 byte order, NEMA I8 palette layout, and NONE/RLE/LZ4 compression. An optional symbol controls the name for a single input. Verbose mode includes converter details.

All formats exposed by LVGLImage.py are available: ARGB8888, XRGB8888, RGB888, RGB565, RGB565_SWAPPED, RGB565A8, ARGB8565, ARGB8888_PREMULTIPLIED, L8, AL88, A1/A2/A4/A8, I1/I2/I4/I8, AUTO, RAW and RAW_ALPHA. Tool exports can be C arrays, LVGL binary files, or converted PNGs. RAW is C-only, as in the official script. Leave the output folder empty to convert only for applying to a widget. This export setting does not add a per-image code/file storage mode; project generation remains a global C/Protobuf choice.

Conversion is asynchronous; closing the converter cancels the job. **Apply first image** previews the converted colors in the selected Image widget. Saved projects embed the original, all conversion options, quantized preview frames, delays, and exact device frame bytes. Reconvert an image after reopening even if its disk source is gone. GIF animations retain all frames. Generated project C honors the selected pixel format, compression, stride and flags and emits required LVGL configuration checks. Compressed device images require `LV_BIN_DECODER_RAM_LOAD` as well as their RLE/LZ4 codec. Device firmware needs the chosen decoders/codecs and NEMA hardware support when selecting an accelerator-specific palette layout.

Both [LVGL's web converter](https://lvgl.io/tools/imageconverter) and [the reference converter](../3rdParty/lvgl_image_converter/UICREATOR.md) informed this workflow. Image conversion now runs directly in C++ with linked native codecs; SVG uses the native resvg library. No Python conversion process or helper folder is required.

### Project images

Open **Assets > Project images…** to import or convert images, preview them, rename them, apply them to the selected image widget, or remove unused entries. **Import / convert images** uses the existing converter and accepts multiple files or folders; successful conversions are automatically added to the project, even without applying them to a widget. **Convert selected image** uses the library entry's embedded original and saves the converted result as another reusable entry.

Each image widget has an image dropdown in its properties. Selecting a project image reuses its stable ID; renaming it keeps widget references intact. **Custom / current image** keeps the current pixels and detaches the library reference. In-use library images cannot be removed. Loading images directly from a widget also registers them in the library.

JSON and Protobuf save the image catalog once and refer to it from widgets. Original files, GIF frames, conversion settings, and device data are retained. Older projects with inline images still load. Generated C shares image descriptors and bytes across widgets; `ui_get_image(id)` returns the first frame of a project image. Animated widgets retain their own playback timers.

Image buttons expose project-image dropdowns for the Left, Middle, and Right slots in all six states (Released, Pressed, Disabled, Checked, Checked pressed, Checked disabled). For a single image per state, set its Middle slot and leave Left/Right as None. Custom / current image detaches the project reference while keeping the image. Slot references participate in usage checks, JSON/Protobuf persistence, and shared C generation. Embedded image-button assets preview their first frame in the editor.

### LVGLCompiler command line

The CMake solution builds `bin/LVGLCompiler.exe`. It runs independently of the editor and requires no converter runtimes for already embedded project assets.

```powershell
.\build\release\bin\LVGLCompiler.exe project.json --format c --output generated_ui
.\build\release\bin\LVGLCompiler.exe project.json --format protobuf --output project.pb
```

C output contains the same compilable sources, assets, CMake files and requirements as **Export > LVGL source project…**. Protobuf output uses the existing LVSerial project envelope and version 2 default omission for the device codec. Font/image catalogs and widget references are preserved. Firmware consumes it through the existing LVSerial runtime visitor. The compiler also accepts `.pb` input, supports multiple screens, resolves C-export file references relative to the input project, and rejects overwriting its input. Use `--help` for options. Exit codes: 0 success, 1 compilation or I/O error, 2 command-line usage error.

### Compiler target versions and firmware runtime

Select `--target 9.6` (default), `9.5`, `9.3`, `9.2`, `9.1`, or `8.3` for C generation:

```powershell
.\build\release\bin\LVGLCompiler.exe project.json --target 8.3 --format c --output generated_ui
.\build\release\bin\LVGLCompiler.exe project.json --target 9.6 --format protobuf --output project.pb
```

The C backends emit compatibility helpers, version guards, screens, widgets, typed variables/bindings, and shared image/font assets. Existing local PNG/JPG/GIF source paths are embedded at compile time; missing paths remain device filesystem paths. Older targets normalize device image formats from the embedded preview/original. LVGL 8.3 image/canvas output requires `LV_COLOR_DEPTH=32`. Properties without an equivalent on the selected version produce an explicit export error. See [target details](../CodeGeneration/Targets/README.md).

Protobuf records its selected runtime target. The on-device loader supports **9.6, 9.5, 9.3, 9.2, 9.1, and 8.3**; configure `LVGL_RUNTIME_TARGET` to match the compiled Protobuf target and your native LVGL library. Its C API creates project screens and resources, resolves object IDs, and provides typed variable/history access. Firmware must enable C++17 for the library even when application code is C. See [runtime integration](../Runtime/README.md) and [test instructions](../Tests/README.md).

Project preview fonts requested at 3 bpp are normalized to 4 bpp because LVGL cannot render uncompressed 3 bpp glyphs. Standalone font conversion retains the requested bit depth and compression settings.

### Editing history and firmware projects

Undo and Redo are available in the Edit menu and with Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z. History captures widget creation, property changes, movement, deletion and project resources. Consecutive typing is grouped; history is bounded to 100 steps and 32 MiB. Opening or creating a project resets history; saving preserves it.

**Export > Create firmware project…** opens the manifest-based board project wizard. See [firmware project creation](ProjectCreator/README.md).

The editor uses native SDL3. Headless runs use `SDL_VIDEO_DRIVER=dummy` and `SDL_RENDER_DRIVER=software`.

### Native Visual Studio Release builds

With the Visual Studio 2026 C++ toolset installed, build either solution platform:

```powershell
msbuild UICreator.sln /m /p:Configuration=Release /p:Platform=x86
msbuild UICreator.sln /m /p:Configuration=Release /p:Platform=x64
```

The editor is written to `Release/UICreator.exe` (x86) or `x64/Release/UICreator.exe`. The build copies SDL3 and FreeType DLLs and packages the font converter, image converter, firmware-project creator and language-model wordlists beside it. Packaging uses the same native codec builds as CMake and a separate Python helper for firmware project creation.

The workspace docks Screens and the widget palette on the left, Hierarchy beside them, Properties on the right, and Assets/Variables below the canvas. Hierarchy search filters object names and retains matching ancestors. The canvas toolbar offers undo, redo and centering; the Export menu exposes C, Protobuf and firmware-project output. Docks adapt when the native window is resized.

### Simulator canvas navigation

Scroll the mouse wheel over the central canvas to zoom around the pointer (10–400%). Use **View > Zoom in / Zoom out** or the toolbar minus/plus buttons to zoom around the viewport center. **View > Actual size (100%)** and the toolbar percentage return to 100%. **View > Fit canvas** centers the complete simulator within the visible canvas; a fitted view adapts to window or simulator-size changes.

Pan by dragging with the middle mouse button or dragging the simulator title bar. Left-dragging empty canvas space draws a selection marquee. Regular left-button dragging on widgets still edits their position, with distances adjusted for zoom. Canvas navigation is view-only: it does not alter saved project coordinates or create undo steps. Wheel scrolling over palettes and property panels retains its normal behavior.

### Grid and alignment guides

The second canvas-toolbar row contains **Grid**, **Snap**, **Guides**, and **Spacing**. Grid visibility and snapping are independent. Spacing is measured in project pixels and can be edited from 2 to 128 pixels or adjusted with its minus/plus buttons. The default is an 8-pixel grid with snapping off.

With Snap enabled, dragged widgets snap to the grid and nearby sibling/parent edges and centers. Cyan alignment guides appear during movement and clear on release. Guide attraction uses a screen-pixel tolerance, so it remains consistent when zoomed. Nested widget positions use the screen's grid origin; locked drag axes stay locked. View settings and overlay drawings do not become project objects or serialization records. Snapped moves participate in normal undo/redo.

Use the top-level **Assets** menu for **Project fonts…**, **Project images…**, **Convert font…**, and **Convert images…**. The bottom Assets tab also provides shortcuts to the resource managers.


### Selecting and editing groups

Shift-click widgets in the simulator or Hierarchy to add/remove them from the selection. Left-drag empty canvas space to select intersecting visible widgets; Shift-drag adds to the current selection. A cyan bounding rectangle identifies a group. The inspector shows the primary widget; individual property fields edit that widget.

Drag any selected widget to move the group. Zoom is accounted for, snapping uses the grabbed widget as the group anchor, and relative offsets stay intact. Selected parents carry their children once, even if those children are also selected.

Use **Edit > Align** for left/right/top/bottom or horizontal/vertical-center alignment to the selection's combined bounds. **Edit > Distribute** spaces eligible sibling widgets horizontally or vertically. **Edit > Duplicate selection** (Ctrl+D) copies selected widget trees, offsets them, and reuses shared image/font catalogs and variables. **Edit > Delete selection** or Delete removes the selected widget trees without deleting the screen. Align, duplicate and delete are also available after selecting widgets in Hierarchy.

Each group move, alignment, duplication or deletion is one undo/redo step. Undo restores the previous multi-selection alongside project objects. Selection and marquee overlays are editor-only and are not saved into JSON, Protobuf or generated C.

### Closing and reopening editor windows

Editor panels have an X at the upper right. Docked panels hide without deleting project contents; reopen one through **View > Show panel**, or choose **View > Restore all panels**. Alt+V opens View. The simulator preview has settings controls and stays open without a close button.

Dialog X buttons use their normal Close/Cancel behavior, including converter and firmware-project worker cleanup. Closing a window releases keyboard focus from its controls. The top-right application X exits the editor, like File > Exit.
