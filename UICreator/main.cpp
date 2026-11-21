/*! \mainpage LVGL UI Creator main
 * This is the main.c for the LVGL UI Creator
 */
/**
 * @file main
 *
 */

/*********************
 *      INCLUDES
 *********************/
#define _DEFAULT_SOURCE /* needed for usleep() */
#include <stdlib.h>
#include <string_view>
#include <stdexcept>
#include <filesystem>
//#include <cstdio>
#if defined(WIN32) || defined(_WINDOWS)
#include <windows.h>

void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
    SetWaitableTimer(timer, &ft, 0, nullptr, nullptr, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#else
#include <unistd.h>
#endif
#define SDL_MAIN_HANDLED        /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL3/SDL.h>
#include "lvgl/lvgl.h"
#if LVGL_VERSION_MAJOR >= 9
#include "Utils/SdlSimulator.h"
#else
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_drivers/indev/mousewheel.h"
#include "lv_drivers/indev/keyboard.h"
#endif
#include "Windows/UI.h"

/*********************
 *      DEFINES
 *********************/

#include "Utils/CrashReporter.h"
#include "Utils/LanguageModelSmoke.h"

int main(int argc, char** argv)
{

    CrashReporter crashReporter;
    if (argc >= 2 && std::string_view(argv[1]) == "--language-model-smoke")
    {
        if (argc != 3)
        {
            fprintf(stderr, "Usage: UICreator --language-model-smoke <new-or-empty-directory>\n");
            return EXIT_FAILURE;
        }
        return RunLanguageModelSmoke(std::filesystem::u8path(argv[2]));
    }
    if (argc == 2 && std::string_view(argv[1]) == "--write-crash-dump")
        return crashReporter.WriteDump() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 2 && std::string_view(argv[1]) == "--terminate-test")
        throw std::runtime_error("Crash reporter verification");
    if (argc == 2 && std::string_view(argv[1]) == "--crash-test")
    {
#ifdef _WIN32
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
#endif
        return EXIT_FAILURE;
    }
    (void)argc; /*Unused*/
    (void)argv; /*Unused*/

#if LVGL_VERSION_MAJOR >= 9
    SdlSimulator simulator;
    if (!simulator.Initialize(1440, 960))
    {
        fprintf(stderr, "SDL simulator initialization failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    UI uiWindow(simulator.GetKeyboard());
    simulator.SetShortcutHandler([&](int key,bool control,bool shift,bool alt){return uiWindow.HandleShortcut(key,control,shift,alt);});
    const bool interactive = SDL_GetCurrentVideoDriver() && std::string_view(SDL_GetCurrentVideoDriver()) != "dummy";
    bool smoke=false, languageModels=false, themeGallery=false, runScreen=false, piLinux=false;std::string initialProject;
    for(int index=1;index<argc;++index) {
        const std::string argument=argv[index];
        if(argument=="--smoke-test")smoke=true;
        else if(argument=="--language-models")languageModels=true;
        else if(argument=="--theme-gallery")themeGallery=true;
        else if(argument=="--run")runScreen=true;
        else if(argument=="--raspberry-pi-linux")piLinux=true;
        else if(argument.rfind("--",0)!=0)initialProject=argument;
    }
    if (interactive)
    {
        uiWindow.EnableSessionPersistence();
    }
    if(!initialProject.empty()) {
        uiWindow.MenuAction("recent:"+initialProject);
        uiWindow.MenuAction("fit-canvas");
    } else if(interactive && !lv_obj_get_child_count(lv_layer_top()))uiWindow.MenuAction("welcome");
    if(languageModels)uiWindow.MenuAction("language-model-generator");
    if(themeGallery)uiWindow.MenuAction("theme-gallery");
    if(runScreen)uiWindow.MenuAction("run-toggle");
    if(piLinux)uiWindow.MenuAction("project-creator-linux");
    if (smoke)
    {
        for (int frame = 0; frame < 10 && simulator.ProcessEvents(); ++frame)
        {
        }
        return EXIT_SUCCESS;
    }
    while (simulator.ProcessEvents())
    {

    }
#else
    lv_init();
    /*Initialize the HAL (display, input devices, tick) for LittlevGL*/
    hal_init();

    UI* uiWindow = new UI(kb_indev);


    while (true)
    {
        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.*/
        lv_task_handler();
        usleep(5 * 1000);
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            mouse_handler(&event);
            keyboard_handler(&event);
        }

#ifdef SDL_APPLE
        SDL_Event event;

        while(SDL_PollEvent(&event)) {
#if USE_MOUSE != 0
            mouse_handler(&event);
#endif

#if USE_KEYBOARD
            keyboard_handler(&event);
#endif

#if USE_MOUSEWHEEL != 0
            mousewheel_handler(&event);
#endif
        }
#endif
    }
#endif

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the Littlev graphics library
 */
#if LVGL_VERSION_MAJOR < 9
static void hal_init(void)
{
    /* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
    monitor_init();

    /*Create a display buffer*/
    static lv_disp_buf_t disp_buf1;
    static lv_color_t buf1_1[3840 * 2160];
    lv_disp_buf_init(&disp_buf1, buf1_1, nullptr, 3840 * 2160);

    /*Create a display*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv); /*Basic initialization*/
    disp_drv.buffer = &disp_buf1;
    disp_drv.flush_cb = monitor_flush; /*Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)*/
    lv_disp_drv_register(&disp_drv);

    /* Add the mouse as input device
     * Use the 'mouse' driver which reads the PC's mouse*/
    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    /*This function will be called periodically (by the library) to get the mouse position and state*/
    lv_indev_t* mouse_indev = lv_indev_drv_register(&indev_drv);

    // /*Set a cursor for the mouse*/
    // LV_IMG_DECLARE(mouse_cursor_icon);                          /*Declare the image file.*/
    // lv_obj_t * cursor_obj =  lv_img_create(lv_disp_get_scr_act(NULL), NULL); /*Create an image object for the cursor */
    // lv_img_set_src(cursor_obj, &mouse_cursor_icon);             /*Set the image source*/
    // lv_indev_set_cursor(mouse_indev, cursor_obj);               /*Connect the image  object to the driver*/


#if USE_KEYBOARD
    lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = keyboard_read;
    kb_indev = lv_indev_drv_register(&kb_drv);
#endif

    /* Tick init.
     * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about how much time were elapsed
     * Create an SDL thread to do this*/
    SDL_CreateThread(tick_thread, "tick", NULL);

    /* Optional:
     * Create a memory monitor task which prints the memory usage in periodically.*/
    lv_task_create(memory_monitor, 3000, LV_TASK_PRIO_MID, nullptr);
}

/**
 * A task to measure the elapsed time for LittlevGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void* data)
{
    (void)data;

    while (true)
    {
        SDL_Delay(5); /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}

/**
 * Print the memory usage periodically
 * @param param
 */
static void memory_monitor(lv_task_t* param)
{
    (void)param; /*Unused*/

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n", static_cast<int>(mon.total_size) - mon.free_size,
           mon.used_pct,
           mon.frag_pct,
           static_cast<int>(mon.free_biggest_size));
}
#endif
