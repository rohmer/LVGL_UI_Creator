#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int32_t id, parent;
    const lv_obj_class_t *type;
} expected_object_t;
typedef struct { int32_t id; bool dismiss; } expected_action_t;
#include "fixture_checks.h"

#define CHECK(condition) do { \
    if(!(condition)) { \
        fprintf(stderr, "Preset %d, line %d: %s\n", FIXTURE_PRESET, __LINE__, #condition); \
        return 1; \
    } \
} while(0)

static void Flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels;
    lv_display_flush_ready(display);
}

int main(void)
{
    unsigned iteration, index;
    const size_t buffer_size = 1024u * 32u * 4u;
    void *pixels = malloc(buffer_size);
    CHECK(pixels != NULL);
    lv_init();
    lv_display_t *display = lv_display_create(1024, 768);
    CHECK(display != NULL);
    lv_display_set_buffers(display, pixels, NULL, (uint32_t)buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, Flush);
    lv_obj_t *previous = lv_screen_active();
    CHECK(previous != NULL);

    for(iteration = 0; iteration < 3; ++iteration) {
        CHECK(ui_init());
        CHECK(ui_init()); /* Initialization must be idempotent. */
        CHECK(ui_screen_count() == 1);
        CHECK(lv_screen_active() == ui_get_screen(0));
        CHECK(ui_get_object(0, -1) == NULL);
        CHECK(ui_get_screen(1) == NULL);
        CHECK(!ui_show_screen(1));
        lv_obj_update_layout(ui_get_screen(0));
        for(index = 0; index < FIXTURE_OBJECT_COUNT; ++index) {
            const expected_object_t *expected = &expected_objects[index];
            lv_obj_t *object = ui_get_object(0, expected->id);
            CHECK(object != NULL);
            CHECK(lv_obj_get_class(object) == expected->type);
            lv_obj_t *parent = expected->parent < 0 ? ui_get_screen(0) : ui_get_object(0, expected->parent);
            CHECK(parent != NULL);
            lv_obj_t *ancestor = lv_obj_get_parent(object);
            /* Tab and other native content hosts may sit between document nodes. */
            while(ancestor && ancestor != parent) ancestor = lv_obj_get_parent(ancestor);
            CHECK(ancestor == parent);
        }
        for(index = 0; index < FIXTURE_ACTION_COUNT; ++index) {
            const expected_action_t *action = &expected_actions[index];
            lv_obj_t *control = ui_get_object(0, action->id);
            CHECK(control != NULL);
            lv_obj_t *parent = lv_obj_get_parent(control);
            lv_obj_t *target = action->dismiss ? parent : lv_obj_get_child(parent, 1);
            CHECK(target != NULL);
            const bool was_hidden = lv_obj_is_hidden(target);
            lv_obj_send_event(control, LV_EVENT_CLICKED, NULL);
            CHECK(lv_obj_is_hidden(target) == (action->dismiss || !was_hidden));
            if(!action->dismiss) {
                lv_obj_send_event(control, LV_EVENT_CLICKED, NULL);
                CHECK(lv_obj_is_hidden(target) == was_hidden);
            }
            lv_obj_set_hidden(target, was_hidden);
        }
        /* Exercise layout, chart drawing, text wrapping, tab hosts and timers. */
        lv_tick_inc(40);
        lv_timer_handler();
        lv_obj_invalidate(ui_get_screen(0));
        lv_refr_now(display);
        ui_deinit();
        CHECK(lv_screen_active() == previous);
        CHECK(ui_get_screen(0) == NULL);
        for(index = 0; index < FIXTURE_OBJECT_COUNT; ++index)
            CHECK(ui_get_object(0, expected_objects[index].id) == NULL);
        ui_deinit(); /* Teardown must also be idempotent. */
    }
    lv_display_delete(display);
    lv_deinit();
    free(pixels);
    printf("Preset %d generated C passed (%u objects, %u actions)\n", FIXTURE_PRESET,
        (unsigned)FIXTURE_OBJECT_COUNT, (unsigned)FIXTURE_ACTION_COUNT);
    return 0;
}
