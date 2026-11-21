# Responsive device previews and layout design

Open **View > Device previews** to compare the current screen at two device sizes. The built-in list includes 320x240 and 480x320 ESP32 displays, 800x480 touch displays, 1024x600 and 1280x720 Raspberry Pi displays, and 480x800 portrait displays. **Swap orientation** exchanges a preview's width and height. It does not rotate or rescale the authored widgets.

Each preview runs its own portable Protobuf display library on an independent LVGL display. Click a preview to use its controls; native keyboard input follows the last preview you clicked. Runtime changes belong to that preview. Clicking an editor control returns input to the editor. Opening another window or menu does not send its text into a preview.

Changing preview resolutions never rewrites the project's widget coordinates. Widgets using native LVGL percentage sizes, Flex or Grid reflow at the new size. Widgets with fixed positions and sizes retain those values and may be clipped, which is useful when reviewing a design for a smaller display.

Use **Refresh previews** after editing the design. **Remember comparison** stores the two profiles and orientations in the project. To save a custom profile, enter a name, width and height and click **Save profile**. Saved profiles are available in both preview lists. Profile metadata supports Undo/Redo and is removed from device exports. It does not select a physical display driver, RAM budget, board or firmware toolchain.

Open **Edit > Layout designer** to configure selected containers, or select the screen to configure its root layout:

- **Flex row / Flex column** choose native LVGL flow, wrapping and main/cross-axis alignment.
- **Gap** and **Padding** use pixels. **Container fills available width** sets a selected container's width to 100 percent of its parent.
- **Grid** accepts pixel tracks, `content`, and positive fractional weights, for example `1fr,2fr,120`. Blank rows create enough content-sized rows for the current children. Child cells are assigned in authored order and stretch within their cells.
- **Manual positions** removes the automatic layout while retaining the authored coordinate values.

**Preview layout** renders a temporary candidate. **Apply layout** commits one undoable change; **Cancel** leaves the project unchanged. Device previews help check the reflow across resolutions. The tool writes standard native LVGL style properties, so generated code and the portable runtimes use the same layout rules.

Previews are limited to 4096 pixels per side and eight million pixels per buffer. The images fit their editor panels; their native render resolution remains the selected profile size. Static theme previews and component thumbnails release runtime resources after rendering while retaining their pixel buffer.

`Tests/Design/VisualModels.cpp` checks model behavior, layout validation, token inheritance, profile persistence and cross-screen master propagation. `UICreatorEditorTests --visual-authoring` checks real native reflow, independent displays, frozen pixels, confirmation/cancellation, overrides and persistence.
