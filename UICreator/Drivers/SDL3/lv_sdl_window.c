/**
 * @file lv_sdl_window.c
 *
 */

/**
 * Modified by NXP in 2025
 */

/*********************
 *      INCLUDES
 *********************/
#include <src/lvgl_public.h>

#if LV_USE_SDL
#include LV_STDBOOL_INCLUDE
#include <src/core/lv_global.h>
#include <src/display/lv_display_private.h>

#define SDL_MAIN_HANDLED /*To fix SDL's "undefined reference to WinMain" issue*/
#include "lv_sdl_private.h"
#include "NativeSDL3.h"

#if LV_COLOR_DEPTH == 1 && LV_SDL_RENDER_MODE != LV_DISPLAY_RENDER_MODE_PARTIAL
    #error SDL LV_COLOR_DEPTH 1 requires LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#endif

/*********************
 *      DEFINES
 *********************/
#define lv_deinit_in_progress  LV_GLOBAL_DEFAULT()->deinit_in_progress

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static inline int sdl_render_mode(void);
static lv_result_t window_create(lv_display_t * disp);
static void sdl_event_handler(lv_timer_t * t);
static void release_disp_cb(lv_event_t * e);
static void res_chg_event_cb(lv_event_t * e);

/**********************
 *  STATIC VARIABLES
 **********************/
static uicreator_sdl3_event_t editor_event_handler;
static void* editor_event_data;
void uicreator_sdl3_set_event_handler(uicreator_sdl3_event_t handler, void* data) { editor_event_handler = handler; editor_event_data = data; }
static bool inited = false;
static bool quit_requested = false;
static unsigned window_count = 0;
static uint32_t ticks32(void) { return (uint32_t)SDL_GetTicks(); }
bool uicreator_sdl3_quit_requested(void) { return quit_requested; }

static lv_timer_t * event_handler_timer;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_display_t * lv_sdl_window_create(int32_t hor_res, int32_t ver_res)
{
    if(!inited) {
#if LV_SDL_USE_EGL && defined(SDL_VIDEO_DRIVER_X11)
        /* We only support x11 with EGL for now*/
        SDL_SetHintWithPriority("SDL_VIDEODRIVER", "x11", SDL_HINT_OVERRIDE);
        SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "1");
#endif
        if(!SDL_Init(SDL_INIT_VIDEO))return NULL;
        quit_requested = false;
        event_handler_timer = lv_timer_create(sdl_event_handler, 5, NULL);
        lv_tick_set_cb(ticks32);
        lv_delay_set_cb(SDL_Delay);

        inited = true;
    }

    lv_sdl_window_t * dsc = lv_malloc_zeroed(sizeof(lv_sdl_window_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) return NULL;

    lv_display_t * disp = lv_display_create(hor_res, ver_res);
    if(disp == NULL) {
        lv_free(dsc);
        return NULL;
    }
    lv_display_set_driver_data(disp, dsc);
    lv_result_t res = window_create(disp);
    if(res != LV_RESULT_OK) {
        LV_LOG_ERROR("Failed to initialize window");
        lv_free(dsc);
        lv_display_delete(disp);
        return NULL;
    }

    lv_display_add_event_cb(disp, release_disp_cb, LV_EVENT_DELETE, disp);
    lv_display_add_event_cb(disp, res_chg_event_cb, LV_EVENT_RESOLUTION_CHANGED, NULL);
    lv_display_add_event_cb(disp, res_chg_event_cb, LV_EVENT_COLOR_FORMAT_CHANGED, NULL);

    /*Process the initial events*/
    sdl_event_handler(NULL);

    return disp;
}

void lv_sdl_window_set_resizeable(lv_display_t * disp, bool value)
{
    LV_CHECK_ARG(disp != NULL, return);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return, "Invalid display");
    SDL_SetWindowResizable(dsc->window, value);
}

void lv_sdl_window_set_size(lv_display_t * disp, int32_t hor_res, int32_t ver_res)
{
    LV_CHECK_ARG(disp != NULL, return);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return, "Invalid display");
    SDL_SetWindowSize(dsc->window, hor_res, ver_res);
}

void lv_sdl_window_set_zoom(lv_display_t * disp, float zoom)
{
    LV_CHECK_ARG(disp != NULL, return);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return, "Invalid display");
    dsc->zoom = zoom;
    lv_display_send_event(disp, LV_EVENT_RESOLUTION_CHANGED, NULL);
    lv_refr_now(disp);
}

float lv_sdl_window_get_zoom(lv_display_t * disp)
{
    LV_CHECK_ARG(disp != NULL, return 0.f);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return 0.f, "Invalid display");
    return dsc->zoom;
}

lv_display_t * lv_sdl_get_disp_from_win_id(uint32_t win_id)
{
    lv_display_t * disp = lv_display_get_next(NULL);
    if(win_id == UINT32_MAX) return disp;

    while(disp) {
        lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
        if(dsc != NULL && SDL_GetWindowID(dsc->window) == win_id) {
            return disp;
        }
        disp = lv_display_get_next(disp);
    }
    return NULL;
}

void lv_sdl_window_set_title(lv_display_t * disp, const char * title)
{
    LV_CHECK_ARG(disp != NULL, return);
    LV_CHECK_ARG(title != NULL, return);

    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return, "Invalid display");
    SDL_SetWindowTitle(dsc->window, title);
}

void lv_sdl_window_set_icon(lv_display_t * disp, void * icon, int32_t width, int32_t height)
{
    LV_CHECK_ARG(disp != NULL, return);
    LV_CHECK_ARG(icon != NULL, return);

    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return, "Invalid display");
    SDL_Surface * iconSurface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ARGB8888, icon, width * 4);
    SDL_SetWindowIcon(dsc->window, iconSurface);
    SDL_DestroySurface(iconSurface);
}

void * lv_sdl_window_get_renderer(lv_display_t * display)
{
    LV_CHECK_ARG(display != NULL, return NULL);
    return lv_sdl_backend_ops.get_renderer(display);
}

struct SDL_Window * lv_sdl_window_get_window(lv_display_t * disp)
{
    LV_CHECK_ARG(disp != NULL, return NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_CHECK_ARG_MSG(dsc != NULL, return NULL, "Invalid display");
    return dsc->window;
}

void lv_sdl_quit(void)
{
    if(inited) {
        lv_timer_delete(event_handler_timer);
        event_handler_timer = NULL;
        inited = false;
    }
}

void lv_sdl_backend_set_display_data(lv_display_t * display, void * backend_display_data)
{
    LV_ASSERT(display != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(display);
    dsc->backend_data = backend_display_data;
}
void * lv_sdl_backend_get_display_data(lv_display_t * display)
{
    LV_ASSERT(display != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(display);
    return dsc->backend_data;
}

int32_t lv_sdl_window_get_horizontal_resolution(lv_display_t * display)
{
    LV_ASSERT(display != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(display);
    LV_ASSERT(dsc != NULL);
    return (int32_t)((float)(display->hor_res) * dsc->zoom);
}
int32_t lv_sdl_window_get_vertical_resolution(lv_display_t * display)
{
    LV_ASSERT(display != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(display);
    LV_ASSERT(dsc != NULL);
    return (int32_t)((float)(display->ver_res) * dsc->zoom);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static inline int sdl_render_mode(void)
{
    return LV_SDL_RENDER_MODE;
}

/**
 * SDL main thread. All SDL related task have to be handled here!
 * It initializes SDL, handles drawing and the mouse.
 */
static void sdl_event_handler(lv_timer_t * t)
{
    LV_UNUSED(t);

    /*Refresh handling*/
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(editor_event_handler && editor_event_handler(&event, editor_event_data)) continue;
        lv_sdl_mouse_handler(&event);
#if LV_SDL_MOUSEWHEEL_MODE == LV_SDL_MOUSEWHEEL_MODE_ENCODER
        lv_sdl_mousewheel_handler(&event);
#endif
        lv_sdl_keyboard_handler(&event);

        if(event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
            lv_display_t * disp = lv_sdl_get_disp_from_win_id(event.window.windowID);
            if(disp == NULL) continue;
            lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
            switch(event.type) {
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                case SDL_EVENT_WINDOW_EXPOSED:
                    lv_sdl_backend_ops.redraw(disp);
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    if(event.window.data1 <= 0 || event.window.data2 <= 0)break;
                    dsc->ignore_size_chg = 1;
                    lv_display_set_resolution(disp, (int32_t)(event.window.data1 / dsc->zoom), (int32_t)(event.window.data2 / dsc->zoom));
                    dsc->ignore_size_chg = 0;
                    lv_refr_now(disp);
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    quit_requested = true;
                    break;
                default:break;
            }
        }
        if(event.type == SDL_EVENT_QUIT)quit_requested = true;
    }
}

static lv_result_t window_create(lv_display_t * disp)
{
    LV_ASSERT(disp != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_ASSERT(dsc != NULL);
    dsc->zoom = 1.0;

    SDL_WindowFlags flag = SDL_WINDOW_RESIZABLE;
#if LV_SDL_USE_EGL
    flag |= SDL_WINDOW_OPENGL;
#endif

#if LV_SDL_FULLSCREEN
    flag |= SDL_WINDOW_FULLSCREEN;
#endif

    int32_t hor_res = (int32_t)((float)(disp->hor_res) * dsc->zoom);
    int32_t ver_res = (int32_t)((float)(disp->ver_res) * dsc->zoom);
    dsc->window = SDL_CreateWindow("LVGL Simulator", hor_res, ver_res, flag);       /*last param. SDL_WINDOW_BORDERLESS to hide borders*/
    if(!dsc->window) {
        LV_LOG_ERROR("Failed to create SDL window: '%s'", SDL_GetError());
        return LV_RESULT_INVALID;
    }
    if(lv_sdl_backend_ops.init_display(disp) != LV_RESULT_OK) {
        LV_LOG_ERROR("Failed to initialize SDL backend");
        SDL_DestroyWindow(dsc->window);
        return LV_RESULT_INVALID;
    }

    SDL_StartTextInput(dsc->window);
    ++window_count;
    /*Some platforms (e.g. Emscripten) seem to require setting the size again */
    SDL_SetWindowSize(dsc->window, hor_res, ver_res);
    return LV_RESULT_OK;
}

static void res_chg_event_cb(lv_event_t * e)
{
    LV_ASSERT(e != NULL);
    lv_display_t * disp = lv_event_get_current_target(e);
    LV_ASSERT(disp != NULL);
    if(lv_sdl_backend_ops.resize_display(disp) != LV_RESULT_OK) {
        LV_LOG_WARN("Failed to resize display");
        return;
    }

    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    if(!dsc->ignore_size_chg) {
        SDL_SetWindowSize(dsc->window,
                          lv_sdl_window_get_horizontal_resolution(disp),
                          lv_sdl_window_get_vertical_resolution(disp));
    }
}


static void release_disp_cb(lv_event_t * e)
{

    LV_ASSERT(e != NULL);
    lv_display_t * disp = (lv_display_t *) lv_event_get_user_data(e);
    LV_ASSERT(disp != NULL);
    lv_sdl_window_t * dsc = lv_display_get_driver_data(disp);
    LV_ASSERT(dsc != NULL);

    lv_sdl_backend_ops.deinit_display(disp);
    SDL_DestroyWindow(dsc->window);
    lv_free(dsc);
    lv_display_set_driver_data(disp, NULL);
    if(window_count) --window_count;
    if(window_count == 0 && lv_deinit_in_progress) { lv_sdl_quit(); SDL_Quit(); }
}

#endif /*LV_USE_SDL*/
