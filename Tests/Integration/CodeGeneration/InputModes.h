/* Shared functional checks for generated C against every supported LVGL. */
#include "advanced_keyboard/include/lv_textarea_input_mode.h"
static int VerifyInputModes(void)
{
    lv_obj_t *upper = OBJ(input_upper), *lower = OBJ(input_lower);
    lv_obj_t *numeric = OBJ(input_numeric), *limited = OBJ(input_limited);
    lv_obj_t *custom = lv_obj_get_child(OBJ(input_custom), 1);
    lv_obj_t *decimal_box = OBJ(input_custom_decimal), *negative_box = OBJ(input_custom_negative);
    lv_obj_t *negative_range = OBJ(input_custom_negative_range);
    if(strcmp(lv_textarea_get_text(upper), "CAFÉ ПРИВЕТ ΑΒΓ SS")) {
        const unsigned char *bytes = (const unsigned char*)lv_textarea_get_text(upper);
        fprintf(stderr, "Uppercase field mode %u; bytes:", (unsigned)lv_advanced_keyboard_get_textarea_input_mode(upper));
        while(*bytes) fprintf(stderr, " %02x", *bytes++);
        fprintf(stderr, "\n");
    }
    CHECK(strcmp(lv_textarea_get_text(upper), "CAFÉ ПРИВЕТ ΑΒΓ SS") == 0);
    CHECK(strcmp(lv_textarea_get_text(lower), "café привет αβγ") == 0);
    CHECK(strcmp(lv_textarea_get_text(numeric), "12.345") == 0);
    CHECK(strcmp(lv_textarea_get_text(OBJ(input_alpha)), "Anything ! مرحبا 中文") == 0);
    CHECK(strcmp(lv_textarea_get_text(limited), "A1B") == 0);
    CHECK(strcmp(lv_textarea_get_text(OBJ(input_expanded)), "SSS") == 0);
    CHECK(strcmp(lv_textarea_get_text(custom), "CAFÉ") == 0);
    lv_textarea_set_text(upper, "éжω");
    CHECK(strcmp(lv_textarea_get_text(upper), "ÉЖΩ") == 0);
    lv_textarea_add_text(upper, "ß!");
    CHECK(strcmp(lv_textarea_get_text(upper), "ÉЖΩSS!") == 0);
    lv_textarea_set_text(lower, "ÉЖΩ");
    CHECK(strcmp(lv_textarea_get_text(lower), "éжω") == 0);
    lv_textarea_set_text(numeric, "-1.2e3.4 hello");
    CHECK(strcmp(lv_textarea_get_text(numeric), "1.234") == 0);
    lv_textarea_add_text(numeric, ".5");
    CHECK(strcmp(lv_textarea_get_text(numeric), "1.2345") == 0);
    lv_textarea_set_cursor_pos(numeric, 0);
    lv_textarea_add_text(numeric, "-6x.");
    CHECK(strcmp(lv_textarea_get_text(numeric), "61.2345") == 0);
    lv_textarea_set_text(numeric, "");
    lv_textarea_add_text(numeric, ".7");
    CHECK(strcmp(lv_textarea_get_text(numeric), ".7") == 0);
#if LVGL_VERSION_MAJOR >= 9
    lv_textarea_delete_char(numeric);
#else
    lv_textarea_del_char(numeric);
#endif
    CHECK(strcmp(lv_textarea_get_text(numeric), ".") == 0);
    lv_textarea_set_text(limited, "a1b2c3");
    CHECK(strcmp(lv_textarea_get_text(limited), "A1B") == 0);
    lv_textarea_set_text(OBJ(input_expanded), "ß");
    CHECK(strcmp(lv_textarea_get_text(OBJ(input_expanded)), "SS") == 0);
    lv_textarea_add_text(OBJ(input_expanded), "ß");
    CHECK(strcmp(lv_textarea_get_text(OBJ(input_expanded)), "SSS") == 0);
    lv_textarea_set_text(custom, "éжω");
    CHECK(strcmp(lv_textarea_get_text(custom), "ÉЖΩ") == 0);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(decimal_box, 1)), "7.3") == 0);
    lv_obj_send_event(lv_obj_get_child(decimal_box, 3), LV_EVENT_CLICKED, NULL);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(decimal_box, 1)), "8") == 0);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(negative_box, 1)), "3") == 0);
    lv_obj_send_event(lv_obj_get_child(negative_box, 2), LV_EVENT_CLICKED, NULL);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(negative_box, 1)), "2") == 0);
    lv_obj_send_event(lv_obj_get_child(negative_box, 2), LV_EVENT_CLICKED, NULL);
    lv_obj_send_event(lv_obj_get_child(negative_box, 2), LV_EVENT_CLICKED, NULL);
    lv_obj_send_event(lv_obj_get_child(negative_box, 2), LV_EVENT_CLICKED, NULL);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(negative_box, 1)), "0") == 0);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(negative_range, 1)), "7.3") == 0);
    lv_obj_send_event(lv_obj_get_child(negative_range, 3), LV_EVENT_CLICKED, NULL);
    CHECK(strcmp(lv_textarea_get_text(lv_obj_get_child(negative_range, 1)), "0") == 0);
    return 0;
}
