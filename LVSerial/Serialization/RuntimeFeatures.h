#pragma once
#include <lvgl/lvgl.h>

// Device bundles opt in to these gates. Full editor and existing runtime builds
// keep their previous complete feature set, including older LVGL backends.
#ifndef LVSERIAL_MINIMAL_RUNTIME
#define LVSERIAL_MINIMAL_RUNTIME 0
#endif
#ifndef LVSERIAL_USE_TEXTBOX
#define LVSERIAL_USE_TEXTBOX (!LVSERIAL_MINIMAL_RUNTIME)
#endif
#ifndef LVSERIAL_USE_LAYERVIEW
#define LVSERIAL_USE_LAYERVIEW (!LVSERIAL_MINIMAL_RUNTIME)
#endif
#ifndef LVSERIAL_USE_WINDOW
#define LVSERIAL_USE_WINDOW (!LVSERIAL_MINIMAL_RUNTIME)
#endif
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD
#define LVSERIAL_USE_ADVANCED_KEYBOARD (!LVSERIAL_MINIMAL_RUNTIME && LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 6)
#endif
#ifndef LVSERIAL_ENABLE_LEGACY_IMAGE_DECODERS
#define LVSERIAL_ENABLE_LEGACY_IMAGE_DECODERS 1
#endif
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#define LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION (LVSERIAL_USE_ADVANCED_KEYBOARD && !LVSERIAL_MINIMAL_RUNTIME)
#endif
#ifndef LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
#define LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
#endif

#if LVSERIAL_MINIMAL_RUNTIME && (LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 6)
#error Minimal runtime feature selection requires the bundled LVGL 9.6 backend
#endif
#if LVSERIAL_MINIMAL_RUNTIME && LVSERIAL_USE_TEXTBOX && (!LV_USE_LABEL || !LV_USE_BUTTON || !LV_USE_TEXTAREA || !LV_USE_FLEX)
#error Text box requires label, button, textarea and flex support
#endif
#if LVSERIAL_MINIMAL_RUNTIME && LVSERIAL_USE_LAYERVIEW && (!LV_USE_LABEL || !LV_USE_BUTTON || !LV_USE_FLEX)
#error Layer view requires label, button and flex support
#endif
#if LVSERIAL_MINIMAL_RUNTIME && LVSERIAL_USE_WINDOW && (!LV_USE_LABEL || !LV_USE_FLEX)
#error Window composition requires label and flex support
#endif
#if LVSERIAL_USE_ADVANCED_KEYBOARD && (LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR < 6)
#error Advanced keyboard requires LVGL 9.6 or later in the 9.x series
#endif
#if LVSERIAL_USE_ADVANCED_KEYBOARD && (!LV_USE_LABEL || !LV_USE_BUTTON || !LV_USE_TEXTAREA)
#error Advanced keyboard requires label, button and textarea support
#endif
