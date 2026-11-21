/**
 * @file lv_sdl_mouse.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_sdl_private.h"

#if LV_USE_SDL

/*********************
 *      DEFINES
 *********************/

#ifndef KEYBOARD_BUFFER_SIZE
    #define KEYBOARD_BUFFER_SIZE 32
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data);
static void release_indev_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

typedef struct {
    int16_t last_x;
    int16_t last_y;
    bool left_button_down;
#if LV_SDL_MOUSEWHEEL_MODE == LV_SDL_MOUSEWHEEL_MODE_CROWN
    int32_t diff;
    float wheel_fraction;
#endif
} lv_sdl_mouse_t;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_indev_t * lv_sdl_mouse_create(void)
{
    lv_sdl_mouse_t * dsc = lv_malloc_zeroed(sizeof(lv_sdl_mouse_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) return NULL;

    lv_indev_t * indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if(indev == NULL) {
        lv_free(dsc);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, sdl_mouse_read);
    lv_indev_set_driver_data(indev, dsc);

    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_add_event_cb(indev, release_indev_cb, LV_EVENT_DELETE, indev);

    return indev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void sdl_mouse_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    lv_sdl_mouse_t * dsc = lv_indev_get_driver_data(indev);

    /*Store the collected data*/
    data->point.x = dsc->last_x;
    data->point.y = dsc->last_y;
    data->state = dsc->left_button_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
#if LV_SDL_MOUSEWHEEL_MODE == LV_SDL_MOUSEWHEEL_MODE_CROWN
    data->enc_diff = dsc->diff;
    dsc->diff = 0;
#endif
}

static void release_indev_cb(lv_event_t * e)
{
    lv_indev_t * indev = (lv_indev_t *) lv_event_get_user_data(e);
    lv_sdl_mouse_t * dsc = lv_indev_get_driver_data(indev);
    if(dsc) {
        lv_indev_set_driver_data(indev, NULL);
        lv_indev_set_read_cb(indev, NULL);
        lv_free(dsc);
        LV_LOG_INFO("done");
    }
}

void lv_sdl_mouse_handler(SDL_Event * event)
{
    uint32_t win_id = UINT32_MAX;
    switch(event->type) {
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            win_id = event->button.windowID;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            win_id = event->motion.windowID;
            break;
#if LV_SDL_MOUSEWHEEL_MODE == LV_SDL_MOUSEWHEEL_MODE_CROWN
        case SDL_EVENT_MOUSE_WHEEL:
            win_id = event->wheel.windowID;
            break;
#endif
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
#if SDL_VERSION_ATLEAST(2,0,12)
            win_id = event->tfinger.windowID;
#endif
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            win_id = event->window.windowID;
            break;
        default:
            return;
    }

    lv_display_t * disp = lv_sdl_get_disp_from_win_id(win_id);
    if(disp == NULL) return;

    /*Find a suitable indev*/
    lv_indev_t * indev = lv_indev_get_next(NULL);
    while(indev) {
        if(lv_indev_get_read_cb(indev) == sdl_mouse_read) {
            /*If disp is NULL for any reason use the first indev with the correct type*/
            if(disp == NULL || lv_indev_get_display(indev) == disp) break;
        }
        indev = lv_indev_get_next(indev);
    }

    if(indev == NULL) return;
    lv_sdl_mouse_t * indev_dev = lv_indev_get_driver_data(indev);
    if(indev_dev == NULL) return;

    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);
    float zoom = lv_sdl_window_get_zoom(disp);

    switch(event->type) {

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(event->button.button == SDL_BUTTON_LEFT)
                indev_dev->left_button_down = false;
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            indev_dev->left_button_down = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(event->button.button == SDL_BUTTON_LEFT) {
                indev_dev->left_button_down = true;
                indev_dev->last_x = (int16_t)((float)(event->button.x) / zoom);
                indev_dev->last_y = (int16_t)((float)(event->button.y) / zoom);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            indev_dev->last_x = (int16_t)((float)(event->motion.x) / zoom);
            indev_dev->last_y = (int16_t)((float)(event->motion.y) / zoom);
            break;

        case SDL_EVENT_FINGER_UP:
            indev_dev->left_button_down = false;
            indev_dev->last_x = (int16_t)((float)hor_res * event->tfinger.x);
            indev_dev->last_y = (int16_t)((float)ver_res * event->tfinger.y);
            break;
        case SDL_EVENT_FINGER_DOWN:
            indev_dev->left_button_down = true;
            indev_dev->last_x = (int16_t)((float)hor_res * event->tfinger.x);
            indev_dev->last_y = (int16_t)((float)ver_res * event->tfinger.y);
            break;
        case SDL_EVENT_FINGER_MOTION:
            indev_dev->last_x = (int16_t)((float)hor_res * event->tfinger.x);
            indev_dev->last_y = (int16_t)((float)ver_res * event->tfinger.y);
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            lv_point_t point = {(int32_t)(event->wheel.mouse_x / zoom), (int32_t)(event->wheel.mouse_y / zoom)};
            lv_obj_t* target = lv_indev_search_obj(lv_display_get_layer_sys(disp), &point);
            if(!target)target = lv_indev_search_obj(lv_display_get_layer_top(disp), &point);
            if(!target)target = lv_indev_search_obj(lv_display_get_screen_active(disp), &point);
            while(target) {
                if(lv_obj_is_scrollable(target) &&
                   (lv_obj_get_scroll_top(target) > 0 || lv_obj_get_scroll_bottom(target) > 0 ||
                    lv_obj_get_scroll_left(target) > 0 || lv_obj_get_scroll_right(target) > 0))break;
                target = lv_obj_get_parent(target);
            }
            if(target) {
                float direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                indev_dev->wheel_fraction += event->wheel.y * 32.0f * direction;
                int32_t delta = (int32_t)indev_dev->wheel_fraction;
                indev_dev->wheel_fraction -= delta;
                lv_obj_scroll_by_bounded(target, (int32_t)(event->wheel.x * 32.0f * direction), delta, LV_ANIM_OFF);
            }
            return;
        }
    }
    lv_indev_read(indev);
}

#endif /*LV_USE_SDL*/

