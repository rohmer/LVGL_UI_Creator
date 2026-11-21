# Windows Protobuf display SDK

`LVGLProtobufDisplay.dll` loads and displays saved LVSerial Protobuf projects in
a native SDL3 window. The DLL contains its own LVGL 9.6 renderer and Protobuf
runtime; C and C++ callers need only `include/lvgl_pb_display.h` and the import
library. No caller-side LVGL, SDL, JSON or Protobuf headers or installation are
required. The SDK packages SDL3 and the matching Visual C++ runtime DLLs beside
the display DLL. Keep the `bin` files together.

The SDK's architecture is recorded in `sdk.json`. Use the matching Visual Studio
compiler platform; an x86 DLL cannot be loaded by an x64 executable. The CMake
SDK example checks pointer width and copies runtime DLLs beside the executable.

## Use from C or C++

```c
#include "lvgl_pb_display.h"

char error[512];
lvgl_pb_window_t *window = lvgl_pb_window_create(320, 240, "My display", error, sizeof error);
if (!window) { /* report error */ }
if (!lvgl_pb_window_load_file(window, "C:/designs/dashboard.pb")) {
    /* report lvgl_pb_window_error(window) */
}
while (lvgl_pb_window_poll(window)) {
    /* Optional application work and variable updates on this UI thread. */
}
lvgl_pb_window_destroy(window);
```

Create at most one owning window per process, and call every API from its
creating UI thread. `poll` pumps native input, timers and rendering until the
window closes. The DLL owns its SDL video lifetime and event pump; a host that already manages
SDL can use the standalone viewer in a separate process.
Filename, title, variable and string data use UTF-8. Loading a
file resizes the display to the saved screen dimensions. The input limit is
16 MiB; malformed or unsupported projects report errors. Failed loads preserve
the previously displayed project.

`lvgl_pb_window_load_memory` accepts a buffer that the caller may release after
the call returns. Use `screen_count`/`show_screen` to navigate multiple screens,
`active_screen` to query the displayed screen index,
and the typed variable functions for application data and bindings. Returned
names/error text and variable strings are borrowed: do not free them, and copy
them before mutation/reload/destruction if needed later. Native object handles
are borrowed and become invalid on reload; the public interface deliberately
keeps LVGL types opaque. Application hardware/network actions remain the
caller's responsibility.

To integrate into an existing CMake project:

```cmake
include(path/to/sdk/cmake/LVGLProtobufDisplayConfig.cmake)
target_link_libraries(my_application PRIVATE LVGLProtobuf::Display)
lvgl_pb_copy_runtime(my_application)
```

Or configure the SDK root as a project to build `LVGLProtobufExample` from the
included `examples/example.c`. The example accepts one Protobuf filename and
handles Unicode command-line paths on Windows.

## Viewer and editor Debug

When included, `bin/UICreatorViewer.exe` runs independently of the editor:

```powershell
.\bin\UICreatorViewer.exe dashboard.pb
.\bin\UICreatorViewer.exe dashboard.pb --watch
.\bin\UICreatorViewer.exe dashboard.pb --screen 1 --frames 30 --screenshot output.bmp
```

Screen indices start at zero. With no `--screen` option, the viewer preserves
the project's saved `activeScreen` on initial load and each reload. An explicit
`--screen N` overrides that choice on every load. The window title reports the
screen actually displayed. `--watch` checks for file updates every 250 ms and
reloads through the same Protobuf runtime. Failed reloads retain the last valid
scene and report the error. The editor's F5 Debug action opens a viewer from a
Protobuf snapshot, exercising the device loader rather than the editable canvas.
This verifies host runtime behavior; it does not simulate an ESP32's display
drivers, memory budget, CPU performance or physical barcode scanning.

## Build and package this SDK

Build `LVGLProtobufDisplay` and optionally `UICreatorViewer` in the parent CMake
project, then run:

```powershell
python Runtime/windows/package_sdk.py --dll build/professional/bin/LVGLProtobufDisplay.dll --import-library build/professional/Runtime/windows/LVGLProtobufDisplay.lib --sdl build/professional/bin/SDL3.dll --compiler "C:/path/to/VC/Tools/MSVC/version/bin/Hostx86/x86/cl.exe" --viewer build/professional/bin/UICreatorViewer.exe --output build/LVGL-Protobuf-Windows-SDK
```

The packager verifies PE architectures, locates the matching MSVC redistributable
files, preserves the relevant licenses, creates an integrity manifest, and emits
a ZIP plus SHA256 checksum. `--replace` refreshes a previously generated SDK;
keep application source outside its generated folder.
