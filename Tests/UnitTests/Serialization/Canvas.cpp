#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
#include <Serialization/LVCanvas.h>

TEST_CASE("Serialize Canvas")
{
	int CANVAS_WIDTH = 320;
	int CANVAS_HEIGHT = 200;
	static lv_style_t style;
	lv_style_copy(&style, &lv_style_plain);
	style.body.main_color = lv_color_make(255, 0, 0);
	style.body.grad_color = lv_color_make(255,0,255);
	style.body.radius = 4;
	style.body.border.width = 2;
	style.body.border.color = lv_color_make(255,255,255);
	style.body.shadow.color = lv_color_make(255, 255, 255);
	style.body.shadow.width = 4;
	style.line.width = 2;
	style.line.color = lv_color_make(0, 0, 0);
	style.text.color = lv_color_make(0, 0, 255);

	static lv_color_t cbuf[4*320*200];

	lv_obj_t* canvas = lv_canvas_create(lv_scr_act(), NULL);
	lv_canvas_set_buffer(canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
	lv_obj_align(canvas, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_canvas_fill_bg(canvas, lv_color_make(22,33,44));

	lv_canvas_draw_rect(canvas, 70, 60, 100, 70, &style);

	lv_canvas_draw_text(canvas, 40, 20, 100, &style, "Some text on text canvas", LV_LABEL_ALIGN_LEFT);

	/* Test the rotation. It requires an other buffer where the orignal image is stored.
	 * So copy the current image to buffer and rotate it to the canvas */
	lv_color_t cbuf_tmp[320 * 200];
	memcpy(cbuf_tmp, cbuf, sizeof(cbuf_tmp));
	lv_img_dsc_t img;
	img.data = (const uint8_t*)cbuf_tmp;
	img.header.cf = LV_IMG_CF_TRUE_COLOR;
	img.header.w = CANVAS_WIDTH;
	img.header.h = CANVAS_HEIGHT;

	lv_canvas_fill_bg(canvas, lv_color_make(123,14,89));
	lv_canvas_rotate(canvas, &img, 30, 0, 0, CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2);
	json j = Serialization::LVCanvas::ToJSON(canvas);
}