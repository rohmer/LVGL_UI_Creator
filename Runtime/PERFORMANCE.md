# Screen lifecycle and performance measurements

The existing three-field `lvgl_runtime_options_t` and loading functions retain
their ABI and eager behavior. Existing callers can still retrieve every screen
immediately after loading.

For an application with many screens, opt into the extended loader:

```c
lvgl_runtime_options_t options = { display, 0, true };
lvgl_runtime_t *runtime = NULL;
char error[512];
bool loaded = lvgl_runtime_load_memory_ex(bytes, size, &options,
    LVGL_RUNTIME_LAZY_SCREENS | LVGL_RUNTIME_UNLOAD_INACTIVE,
    &runtime, error, sizeof error);
```

Lazy loading retains normalized screen definitions and shared fonts, images and
trie assets. Widgets, image-animation timers and private keyboard prediction
workspaces are instantiated when a screen is shown. After navigation, inactive
widgets are released once any transition has finished. Typed variables and their
history remain in the runtime; unbound widget state is saved before unloading.

`lvgl_runtime_ensure_screen` explicitly creates an inactive screen.
`lvgl_runtime_unload_screen` releases an inactive, nontransitioning screen.
`lvgl_runtime_screen` and `lvgl_runtime_object` return null for unloaded objects.
Pointers from an unloaded screen expire and must be reacquired after ensuring or
showing it again. Keep the default input group supplied during lazy loading alive
until runtime destruction. Shared resource storage lives with the runtime;
unloading widgets does not promise to release every shared image or font.

`lvgl_runtime_show_screen_animated` supports none, fade and four slide directions,
with durations from 0 to 2000 milliseconds. `lvgl_runtime_show_screen` uses the
destination's optional `transition: {type, duration}` metadata. The Back stack is
bounded to 128 entries. `lvgl_runtime_back` returns to the previous screen without
adding a new history entry. An authored `base.accessibility.tabIndex` sets keyboard
focus order; -1 excludes that widget and its internal focus controls.

`lvgl_runtime_get_stats` reports actual input bytes, loaded screen/widget counts,
load time and load/unload counters. Its LVGL heap values describe the entire LVGL
allocator, exclude C++ resources and external framebuffer memory, and can include
other runtimes. Unsupported allocator monitoring is reported as unavailable.

## Editor performance dashboard

The dashboard samples the isolated Run display. It measures actual redraw frames,
render time, flush callback time, input processing, redraw pixels and loaded object
counts. Distributions retain the latest 256 samples and report mean, median, P95
and maximum. Idle screens do not redraw; their observed redraw FPS can be zero.
Frame CPU time is not a display refresh-rate guarantee, and host flush time does
not include a physical SPI/DSI/HDMI transfer.

The prediction benchmark runs real lookups against the current screen's selected
Advanced Keyboard predictor, without altering textbox content. Profiling can be
disabled, and closing the dashboard disables collection. The dashboard distinguishes
host memory from measured device RAM and never treats the editor's heap as target
RAM. Named application budgets are editable starting points, not board-capacity
claims. A selected `.bin` file supplies an actual image-file-size check; an ELF may
also contain symbols and debug data.

Device measurements can be imported as a JSON file. Collect the values on the
device using the transport, allocator and monotonic clock appropriate to that
application. Unknown fields should be omitted rather than fabricated as zero.

```json
{
  "format": "lvgl-performance-sample",
  "version": 1,
  "device": "My ESP32 display",
  "frames": 3,
  "elapsedSeconds": 0.1,
  "frameMs": [10.2, 11.1, 10.8],
  "renderMs": [8.1, 8.7, 8.5],
  "flushMs": [2.1, 2.4, 2.3],
  "predictionMs": [0.12, 0.14, 0.11],
  "ramPeakBytes": 160000,
  "firmwareBytes": 800000
}
```

These numbers are an example schema, not measured product results. Imported device
samples and host samples remain separate. Copy report exports aggregated statistics
without project content; its `lvgl-performance-report` format is a report rather
than a raw device-sample file.
