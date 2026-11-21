# Design, test, and deploy

The editor includes these tools in its top-level menus:

| Menu | Tool | Use |
| --- | --- | --- |
| Assets | Theme gallery | Preview 14 light/dark palettes, apply to the project or selection, edit shared color and size tokens. |
| Assets | Reusable components | Capture a widget group, insert linked copies, update a master, preserve local overrides, detach, and share libraries. |
| Edit | Edit widget events | Add click, value-change, or ready actions to the selected widget. |
| Language Models | Model playground | Compare two real prediction tries, inspect suggestions and autocorrect confidence, then explicitly load a chosen model. |
| Debug | Run screen in editor | Test the exported Protobuf screen using the portable display library. |
| Debug | Validate screen | Find clipped widgets, overlaps, small touch targets, missing assets or glyphs, broken bindings and action references. |
| Export | Build, flash and serial monitor | Configure an Arduino, ESP-IDF or CMake project, build it, upload firmware, and read serial output. |

## Run mode

Click **Run (F6)** on the canvas toolbar or choose **Debug > Run screen in editor**. The screen runs on a separate LVGL display rendered inside the editor. Mouse/touch, native keyboard input, Advanced Keyboard, typed variables and authored event actions operate on that runtime copy. F6, Escape or the Run window's close button returns to Design. Restarting Run resets the runtime to the authored screen.

The authored project and its undo history remain unchanged by runtime interactions. Save/export commands return to Design. F5 still opens the standalone Windows Protobuf viewer for testing outside the editor.

## Variables

The **Variables** tab in the **Tools** panel connects one typed value to one or
more named widgets, with a direction for each connection. Choose the application
C type and use **Copy wiring code** to bind a C/C++ variable by address. See
[variables and widget connections](UICreator/Variables.md) for setup, generated
APIs, synchronization, and text-buffer examples.

## Validation

Validation reports errors and warnings without modifying the document. Click an issue for the current screen to select its widget. Overlaps between a parent and its own children are intentional; overlapping siblings are reported. Scrollable containers may intentionally contain offscreen content. Glyph checks inspect the live font and its fallbacks when available. Results are design guidance and should be reviewed against the intended layout.

## Device workflow

Create a firmware project with **Export > Create firmware project**, then open **Build, flash and serial monitor** and choose that folder. Select the toolchain, configure executable/tool paths, board and port, and use **Tool version** to check the setup. **Build** compiles; **Flash board** uploads existing build output. The Arduino default is the Nano ESP32 FQBN `arduino:esp32:nano_nora`; another installed board can be specified. CMake builds support Linux/Raspberry Pi projects and Windows builds with their installed toolchains.

Tool arguments are passed directly to the process API. Output is streamed with a bounded log, Stop cancels the process tree, and closing the window stops workers and releases the serial port. Serial monitoring opens the selected port only after Connect. Port discovery and tool detection do not flash hardware.

See [themes and components](Docs/Themes-and-components.md) and [portable interactions](Runtime/INTERACTIONS.md) for persisted formats and export behavior.
