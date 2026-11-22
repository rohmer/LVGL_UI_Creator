#ifndef LVGL_PROJECT_RUNTIME_H
#define LVGL_PROJECT_RUNTIME_H
#include <lvgl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct lvgl_runtime lvgl_runtime_t;
    typedef struct
    {
        void *display;
        size_t max_input_bytes;
        bool load_initial_screen;
    } lvgl_runtime_options_t;
    typedef enum
    {
        LVGL_RUNTIME_STRING = 0,
        LVGL_RUNTIME_INTEGER = 1,
        LVGL_RUNTIME_BOOLEAN = 2,
        LVGL_RUNTIME_FLOAT = 3,
        LVGL_RUNTIME_DATETIME = 4
    } lvgl_runtime_value_type_t;
    typedef struct
    {
        lvgl_runtime_value_type_t type;
        const char *string_value;
        int64_t integer_value;
        double real_value;
        bool boolean_value;
    } lvgl_runtime_value_t;
    /* Call on the LVGL thread after lv_init and display creation. NULL options use
     * the default display, a 16 MiB input limit, and load the initial screen.
     * Input accepts Protobuf or LVGL UI Creator XML (when LVSERIAL_USE_XML is
     * enabled). XML is detected after an optional UTF-8 BOM and whitespace.
     * Input bytes may be released after load returns. Error is optional. */
    bool lvgl_runtime_load_memory(const void *bytes, size_t size, const lvgl_runtime_options_t *options,
                                  lvgl_runtime_t **output, char *error, size_t error_size);
    typedef enum {
        LVGL_RUNTIME_LAZY_SCREENS = 1u,
        LVGL_RUNTIME_UNLOAD_INACTIVE = 2u,
        LVGL_RUNTIME_BORROW_ASSETS = 4u
    } lvgl_runtime_load_flags_t;
    /* Extended loader. Existing loaders remain eager and ABI-compatible.
     * Lazy runtimes retain compact screen definitions/shared assets and create
     * widgets on demand. UNLOAD_INACTIVE snapshots widget state after navigation
     * and releases inactive widgets once any transition has completed. The
     * default input group used for loading must outlive a lazy runtime.
     * XML follows the same flags and lifecycle. BORROW_ASSETS is accepted for
     * XML, but decoded XML assets are always owned by the runtime. */
    bool lvgl_runtime_load_memory_ex(const void *bytes, size_t size, const lvgl_runtime_options_t *options,
        uint32_t flags, lvgl_runtime_t **output, char *error, size_t error_size);
    /* Opt-in flash/memory-mapped input: trie/index BINARY payloads remain in the
     * caller's immutable buffer until runtime destruction. Other input data is
     * decoded normally. XML assets are always decoded into owned storage, so
     * XML input bytes may be freed after this function returns. Use load_memory
     * for temporary or mutable Protobuf buffers. */
    bool lvgl_runtime_load_memory_borrowed_assets(const void *bytes, size_t size,
        const lvgl_runtime_options_t *options, lvgl_runtime_t **output, char *error, size_t error_size);
    /* Uses LVGL's filesystem (e.g. "S:/project.pb" or "S:/project.xml"), not
     * host stdio. Both formats use the same memory-loader validation. */
    bool lvgl_runtime_load_file(const char *path, const lvgl_runtime_options_t *options,
                                lvgl_runtime_t **output, char *error, size_t error_size);
    void lvgl_runtime_destroy(lvgl_runtime_t *runtime);
    const char *lvgl_runtime_backend(void);
    const char *lvgl_runtime_error(const lvgl_runtime_t *runtime);
    size_t lvgl_runtime_screen_count(const lvgl_runtime_t *runtime);
    lv_obj_t *lvgl_runtime_screen(const lvgl_runtime_t *runtime, size_t screen);
    /* Authored dimensions remain stable when a display resizes its active root. */
    bool lvgl_runtime_screen_size(const lvgl_runtime_t *runtime, size_t screen, int32_t *width, int32_t *height);
    bool lvgl_runtime_show_screen(lvgl_runtime_t *runtime, size_t screen);
    typedef enum {
        LVGL_RUNTIME_TRANSITION_NONE = 0,
        LVGL_RUNTIME_TRANSITION_FADE = 1,
        LVGL_RUNTIME_TRANSITION_LEFT = 2,
        LVGL_RUNTIME_TRANSITION_RIGHT = 3,
        LVGL_RUNTIME_TRANSITION_UP = 4,
        LVGL_RUNTIME_TRANSITION_DOWN = 5
    } lvgl_runtime_transition_t;
    bool lvgl_runtime_show_screen_animated(lvgl_runtime_t *runtime, size_t screen,
        lvgl_runtime_transition_t transition, uint32_t duration_ms);
    bool lvgl_runtime_ensure_screen(lvgl_runtime_t *runtime, size_t screen);
    /* Active/transitioning screens cannot be explicitly unloaded. Object
     * pointers belonging to an unloaded screen expire; reacquire after ensure. */
    bool lvgl_runtime_unload_screen(lvgl_runtime_t *runtime, size_t screen);
    size_t lvgl_runtime_active_screen(const lvgl_runtime_t *runtime); /* SIZE_MAX when none. */
    size_t lvgl_runtime_loaded_screen_count(const lvgl_runtime_t *runtime);
    size_t lvgl_runtime_back_count(const lvgl_runtime_t *runtime);
    bool lvgl_runtime_back(lvgl_runtime_t *runtime);
    typedef struct {
        size_t input_bytes, screen_count, loaded_screens, loaded_objects;
        uint64_t load_us, last_screen_load_us, screen_loads, screen_unloads;
        bool lvgl_heap_available;
        size_t lvgl_heap_used, lvgl_heap_peak, lvgl_heap_largest_free;
    } lvgl_runtime_stats_t;
    /* Heap figures describe the whole LVGL allocator, not this runtime alone;
     * C++ resource storage and host framebuffer allocations are excluded.
     * Unsupported allocator monitoring is reported as unavailable. */
    bool lvgl_runtime_get_stats(const lvgl_runtime_t *runtime, lvgl_runtime_stats_t *stats);
    lv_obj_t *lvgl_runtime_object(const lvgl_runtime_t *runtime, size_t screen, int32_t saved_id);
    const lv_font_t *lvgl_runtime_font(const lvgl_runtime_t *runtime, const char *id);
    /* Returns the first native image descriptor: const lv_image_dsc_t* on 9.x,
     * const lv_img_dsc_t* on 8.3. */
    const void *lvgl_runtime_image(const lvgl_runtime_t *runtime, const char *id);
    size_t lvgl_runtime_variable_count(const lvgl_runtime_t *runtime);
    const char *lvgl_runtime_variable_name(const lvgl_runtime_t *runtime, size_t index);
    bool lvgl_runtime_get_variable(lvgl_runtime_t *runtime, const char *name, lvgl_runtime_value_t *value);
    bool lvgl_runtime_set_variable(lvgl_runtime_t *runtime, const char *name,
                                   const lvgl_runtime_value_t *value);
    size_t lvgl_runtime_history_count(const lvgl_runtime_t *runtime, const char *name);
    bool lvgl_runtime_history_get(lvgl_runtime_t *runtime, const char *name, size_t index,
                                  lvgl_runtime_value_t *value);
/* Borrowed object/resource/value pointers expire when their owner is destroyed;
 * variable string pointers may also expire on mutation. */
#ifdef __cplusplus
}
#endif
#endif
