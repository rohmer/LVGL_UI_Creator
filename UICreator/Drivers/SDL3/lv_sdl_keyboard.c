/**
 * @file lv_sdl_keyboard.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_sdl_private.h"
#include "NativeSDL3.h"

#if LV_USE_SDL

#include <src/misc/lv_text_private.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    char buf[KEYBOARD_BUFFER_SIZE];
    bool dummy_read;
} lv_sdl_keyboard_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void sdl_keyboard_read(lv_indev_t * indev, lv_indev_data_t * data);
static void flush_keyboard(lv_indev_t * indev);
static uint32_t keycode_to_ctrl_key(SDL_Keycode sdl_key);
static void release_indev_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_indev_t * lv_sdl_keyboard_create(void)
{
    lv_sdl_keyboard_t * dsc = lv_malloc_zeroed(sizeof(lv_sdl_keyboard_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) return NULL;

    lv_indev_t * indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if(indev == NULL) {
        lv_free(dsc);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, sdl_keyboard_read);
    lv_indev_set_driver_data(indev, dsc);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_add_event_cb(indev, release_indev_cb, LV_EVENT_DELETE, indev);

    return indev;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void sdl_keyboard_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    lv_sdl_keyboard_t * dev = lv_indev_get_driver_data(indev);

    const size_t len = lv_strlen(dev->buf);

    /*Send a release manually*/
    if(dev->dummy_read) {
        dev->dummy_read = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    /*Send the pressed character*/
    else if(len > 0) {
        dev->dummy_read = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = 0;
        /*Copy the first UTF8 character from the buffer*/
        uint32_t utf8_len = lv_text_encoded_size(dev->buf);
        if(utf8_len == 0) utf8_len = 1; /*Make sure that at least 1 character is read*/
        lv_memcpy(&data->key, dev->buf, utf8_len);

        /*Drop the first character*/
        lv_memmove(dev->buf, dev->buf + utf8_len, len - utf8_len + 1);
    }
}

static void release_indev_cb(lv_event_t * e)
{
    lv_indev_t * indev = (lv_indev_t *) lv_event_get_user_data(e);
    lv_sdl_keyboard_t * dev = lv_indev_get_driver_data(indev);
    if(dev) {
        lv_indev_set_driver_data(indev, NULL);
        lv_indev_set_read_cb(indev, NULL);
        lv_free(dev);
        LV_LOG_INFO("done");
    }
}

static uicreator_sdl3_shortcut_t shortcut_handler;
static void* shortcut_data;
void uicreator_sdl3_set_shortcut_handler(uicreator_sdl3_shortcut_t handler, void* data) { shortcut_handler=handler;shortcut_data=data; }
bool uicreator_sdl3_text_input_allowed(void)
{
    const SDL_Keymod modifiers = SDL_GetModState();
    return (modifiers & (SDL_KMOD_RALT | SDL_KMOD_MODE)) || !(modifiers & (SDL_KMOD_CTRL | SDL_KMOD_ALT));
}
static void flush_keyboard(lv_indev_t * indev)
{
    lv_sdl_keyboard_t * dsc = lv_indev_get_driver_data(indev);
    while(dsc->buf[0]) {
        lv_indev_read(indev);
        lv_indev_read(indev); /*Release the character before reading the next one.*/
    }
}
void lv_sdl_keyboard_handler(SDL_Event * event)
{
    if(event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
       !(event->key.mod & (SDL_KMOD_RALT | SDL_KMOD_MODE)) && shortcut_handler &&
       shortcut_handler(event->key.key,event->key.mod,shortcut_data))return;
    if(event->type == SDL_EVENT_TEXT_INPUT && !uicreator_sdl3_text_input_allowed())return;
    uint32_t win_id = UINT32_MAX;
    switch(event->type) {
        case SDL_EVENT_KEY_DOWN:
            win_id = event->key.windowID;
            break;
        case SDL_EVENT_TEXT_INPUT:
            win_id = event->text.windowID;
            break;
        default:
            return;
    }

    lv_display_t * disp = lv_sdl_get_disp_from_win_id(win_id);


    /*Find a suitable indev*/
    lv_indev_t * indev = lv_indev_get_next(NULL);
    while(indev) {
        if(lv_indev_get_read_cb(indev) == sdl_keyboard_read) {
            /*If disp is NULL for any reason use the first indev with the correct type*/
            if(disp == NULL || lv_indev_get_display(indev) == disp) break;
        }
        indev = lv_indev_get_next(indev);
    }
    if(indev == NULL) return;
    lv_sdl_keyboard_t * dsc = lv_indev_get_driver_data(indev);

    /* We only care about SDL_EVENT_KEY_DOWN and SDL_EVENT_TEXT_INPUT events */
    switch(event->type) {
        case SDL_EVENT_KEY_DOWN: {                     /*Button press*/
                const uint32_t ctrl_key = event->key.key == SDLK_TAB && (event->key.mod & SDL_KMOD_SHIFT)
                    ? LV_KEY_PREV : keycode_to_ctrl_key(event->key.key);
                if(ctrl_key == '\0')
                    return;
                const size_t len = lv_strlen(dsc->buf);
                if(len < KEYBOARD_BUFFER_SIZE - 1) {
                    dsc->buf[len] = ctrl_key;
                    dsc->buf[len + 1] = '\0';
                }
                break;
            }
        case SDL_EVENT_TEXT_INPUT: {                   /*Text input*/
                /* SDL3 can supply long IME/composition strings in one event.
                 * Stream complete UTF-8 characters through the bounded queue. */
                const char * text = event->text.text;
                const size_t length = text ? lv_strlen(text) : 0;
                size_t offset = 0;
                while(offset < length) {
                    uint32_t bytes = lv_text_encoded_size(text + offset);
                    if(bytes == 0) bytes = 1;
                    if(bytes > length - offset) bytes = (uint32_t)(length - offset);
                    size_t used = lv_strlen(dsc->buf);
                    if(used + bytes >= KEYBOARD_BUFFER_SIZE) {
                        flush_keyboard(indev);
                        used = 0;
                    }
                    lv_memcpy(dsc->buf + used, text + offset, bytes);
                    dsc->buf[used + bytes] = '\0';
                    offset += bytes;
                }
            }
            break;
        default:
            break;

    }

    flush_keyboard(indev);
}

/**
 * Convert a SDL key code to it's LV_KEY_* counterpart or return '\0' if it's not a control character.
 * @param sdl_key the key code
 * @return LV_KEY_* control character or '\0'
 */
static uint32_t keycode_to_ctrl_key(SDL_Keycode sdl_key)
{
    /*Remap some key to LV_KEY_... to manage groups*/
    switch(sdl_key) {
        case SDLK_RIGHT:
        case SDLK_KP_PLUS:
            return LV_KEY_RIGHT;

        case SDLK_LEFT:
        case SDLK_KP_MINUS:
            return LV_KEY_LEFT;

        case SDLK_UP:
            return LV_KEY_UP;

        case SDLK_DOWN:
            return LV_KEY_DOWN;

        case SDLK_ESCAPE:
            return LV_KEY_ESC;

        case SDLK_BACKSPACE:
            return LV_KEY_BACKSPACE;

        case SDLK_DELETE:
            return LV_KEY_DEL;

        case SDLK_KP_ENTER:
        case '\r':
            return LV_KEY_ENTER;

        case SDLK_TAB:
        case SDLK_PAGEDOWN:
            return LV_KEY_NEXT;

        case SDLK_PAGEUP:
            return LV_KEY_PREV;

        case SDLK_HOME:
            return LV_KEY_HOME;

        case SDLK_END:
            return LV_KEY_END;

        default:
            return '\0';
    }
}

#endif /*LV_USE_SDL*/
