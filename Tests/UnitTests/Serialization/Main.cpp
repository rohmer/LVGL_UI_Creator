#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#define _DEFAULT_SOURCE /* needed for usleep() */
#include <stdlib.h>
#include <cstdio>
#if defined(WIN32) || defined(_WINDOWS)
#include <windows.h>

void usleep(__int64 usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#else
#include <unistd.h>
#endif
#define SDL_MAIN_HANDLED        /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_drivers/indev/mousewheel.h"
#include "lv_drivers/indev/keyboard.h"

static void hal_init(void);
static int tick_thread(void* data);
static void memory_monitor(lv_task_t* param);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_indev_t* kb_indev;


int main(int argc, char* argv[])
{
	// global setup...
	/*Initialize LittlevGL*/
	lv_init();

	/*Initialize the HAL (display, input devices, tick) for LittlevGL*/
	hal_init();

	int result = Catch::Session().run(argc, argv);

	// global clean-up...

	return result;
}

/**
*Initialize the Hardware Abstraction Layer(HAL) for the Littlev graphics library
*/
static void hal_init(void)
{
	/* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
	monitor_init();

	/*Create a display buffer*/
	static lv_disp_buf_t disp_buf1;
	static lv_color_t buf1_1[3840 * 2160];
	lv_disp_buf_init(&disp_buf1, buf1_1, NULL, 3840 * 2160);

	/*Create a display*/
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.buffer = &disp_buf1;
	disp_drv.flush_cb = monitor_flush;    /*Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)*/
	lv_disp_drv_register(&disp_drv);

	/* Add the mouse as input device
	 * Use the 'mouse' driver which reads the PC's mouse*/
	mouse_init();
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);          /*Basic initialization*/
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = mouse_read;         /*This function will be called periodically (by the library) to get the mouse position and state*/
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
	lv_task_create(memory_monitor, 3000, LV_TASK_PRIO_MID, NULL);
}

/**
 * A task to measure the elapsed time for LittlevGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void* data)
{
	(void)data;

	while (1) {
		SDL_Delay(5);   /*Sleep for 5 millisecond*/
		lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
	}

	return 0;
}

/**
 * Print the memory usage periodically
 * @param param
 */
static void memory_monitor(lv_task_t * param)
{
	(void)param; /*Unused*/

	lv_mem_monitor_t mon;
	lv_mem_monitor(&mon);
	printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n", (int)mon.total_size - mon.free_size,
		mon.used_pct,
		mon.frag_pct,
		(int)mon.free_biggest_size);

}
