#include "LVCanvas.h"

namespace Serialization
{
	json LVCanvas::ToJSON(lv_obj_t* canvas)
	{
		json j;
		j["base"] = LVObject::ToJSON(canvas);
		lv_canvas_ext_t* ext = (lv_canvas_ext_t*)lv_obj_get_ext_attr(canvas);
		j["canvas"]["cf"] = (uint32_t)ext->dsc.header.cf;
		j["canvas"]["header.w"] = (uint16_t)ext->dsc.header.w;
		j["canvas"]["header.h"] = (uint16_t)ext->dsc.header.h;
		j["canvas"]["dataSize"] = (uint32_t)ext->dsc.data_size;
		j["canvas"]["data"] = base64_encode(ext->dsc.data, ext->dsc.data_size);
		j["canvas"]["autoSize"] = (uint8_t)ext->img.auto_size;
		j["canvas"]["offset"]["x"] = ext->img.offset.x;
		j["canvas"]["offset"]["y"] = ext->img.offset.y;
		j["canvas"]["img.h"] = ext->img.h;
		j["canvas"]["img.w"] = ext->img.w;
		j["canvas"]["srcType"] = (uint8_t)ext->img.src_type;
		if(lv_img_src_get_type(ext->img.src)==LV_IMG_SRC_VARIABLE)
		{
			auto buf = new unsigned char(4);
			std::stringstream ss;
			for(int y=0; y<ext->img.h; y++)
			{
				for (int x = 0; x < ext->img.w; x++)
				{
					lv_color_t col = lv_img_buf_get_px_color(&ext->dsc, x, y, nullptr);
					buf[0] = col.ch.red;
					buf[1] = col.ch.green;
					buf[2] = col.ch.blue;
					buf[3] = col.ch.alpha;
					ss << base64_encode(const_cast<unsigned char*>(buf), 4);
				}
			}
			j["canvas"]["img"] = ss.str();
		} else
		{
			std::stringstream ss;
			char* p = (char*)ext->img.src;
			while(p!='\0')
			{
				ss << p;
			}
			j["canvas"]["img"] = ss.str();
		}
		static const lv_style_t *style = lv_canvas_get_style(canvas, LV_CANVAS_STYLE_MAIN);
		j["canvas"]["img.color"]["r"] = style->image.color.ch.red;
		j["canvas"]["img.color"]["g"] = style->image.color.ch.green;
		j["canvas"]["img.color"]["b"] = style->image.color.ch.blue;
		j["canvas"]["img.color"]["a"] = style->image.color.ch.alpha;
		return j;
	}

	lv_obj_t* LVCanvas::FromJSON(json j)
	{
		lv_obj_t* canvas = lv_canvas_create(lv_scr_act(), nullptr);
		if (j["base"].is_object())
		{
			canvas = LVObject::FromJSON(j["base"], canvas);
		}
		if (j["canvas"].is_object())
			return canvas;
		int cf = -1;
		if(j["canvas"]["cf"].is_number())
		{
			cf = j["canvas"]["cf"];
		}
		int w = -1, h = -1;
		if(j["canvas"]["img.h"].is_number())
		{
			h = j["canvas"]["img.h"];
		}
		if (j["canvas"]["img.w"].is_number())
		{
			w = j["canvas"]["img.w"];
		}
		int srcType = -1;
		if(j["canvas"]["srcType"].is_number())
		{
			srcType = j["canvas"]["srcType"];
		}

		if(srcType==LV_IMG_SRC_VARIABLE && cf!=-1 && w!=-1 && h!=-1 && j["canvas"]["img"].is_string())
		{
			std::string val = base64_decode(j["canvas"]["img"]);
			lv_canvas_draw_img(canvas, 0,0, (void*)val.c_str(),nullptr);
		} else
		{
			lv_canvas_draw_img(canvas, 0, 0, j["canvas"]["img"].get<std::string>().c_str(), nullptr);			
		}
		uint16_t offX = -1, offY = -1;
		if (j["canvas"]["offset"].is_object())
		{
			if (j["canvas"]["offset"]["x"].is_number())
				offX = j["canvas"]["offset"]["x"];
			if (j["canvas"]["offset"]["y"].is_number())
				offY = j["canvas"]["offset"]["y"];
		}
		lv_canvas_ext_t* ext = (lv_canvas_ext_t*)lv_obj_get_ext_attr(canvas);
		if(offX!=-1)
		{
			ext->img.offset.x = offX;
		}
		if(offY!=-1)
		{
			ext->img.offset.y = offY;
		}
		if(j["canvas"]["img.color"].is_object())
		{
			static lv_style_t style;
			lv_style_copy(&style, lv_canvas_get_style(canvas,LV_CANVAS_STYLE_MAIN));
			lv_color_t color;
			int r, g, b, a;
			r = g = b = 0;
			a = 255;
			if (j["canvas"]["img.color"]["r"].is_number())
				r = j["canvas"]["img.color"]["r"];
			if (j["canvas"]["img.color"]["r"].is_number())
				g = j["canvas"]["img.color"]["g"];
			if (j["canvas"]["img.color"]["r"].is_number())
				b = j["canvas"]["img.color"]["b"];
			if (j["canvas"]["img.color"]["r"].is_number())
				a = j["canvas"]["img.color"]["a"];
			color = lv_color_make(r, g, b);
			style.image.color = color;
			lv_canvas_set_style(canvas, LV_CANVAS_STYLE_MAIN, &style);
		}
		return(canvas);
	}

	bool LVCanvas::SetValue(lv_obj_t* obj, uint32_t x, uint32_t y, uint32_t cf, std::vector<unsigned char> img, lv_style_t style)
	{
		lv_canvas_draw_img(obj, x, y, img.data(), &style);
		return true;
	}

	bool LVCanvas::SetValue(lv_obj_t* obj, std::string key, int value)
	{

		return false;
	}

}