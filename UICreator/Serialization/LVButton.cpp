#include "LVButton.h"


namespace Serialization
{
	json LVButton::ToJSON(lv_obj_t* button)
	{
		json j;
		j["base"] = LVObject::ToJSON(button);

		auto relStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_REL);
		auto inStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_INA);
		auto prStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_PR);
		auto tglPrStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTN_STYLE_TGL_PR);
		auto tglRelStyle = (lv_style_t*)lv_btn_get_style(button, LV_BTNM_STYLE_BTN_TGL_REL);

		j["button"]["styles"]["rel"] = Style::Serialize(*relStyle);
		j["button"]["styles"]["ina"] = Style::Serialize(*inStyle);
		j["button"]["styles"]["pr"] = Style::Serialize(*prStyle);
		j["button"]["styles"]["tglPr"] = Style::Serialize(*tglPrStyle);
		j["button"]["styles"]["tglRel"] = Style::Serialize(*tglRelStyle);
		j["button"]["toggle"] = lv_btn_get_toggle(button);
		j["button"]["layout"] = lv_btn_get_layout(button);
		j["button"]["fit"]["0"] = lv_btn_get_fit_bottom(button);
		j["button"]["fit"]["1"] = lv_btn_get_fit_left(button);
		j["button"]["fit"]["2"] = lv_btn_get_fit_top(button);
		j["button"]["fit"]["3"] = lv_btn_get_fit_right(button);
		j["button"]["ink"]["in"] = lv_btn_get_ink_in_time(button);
		j["button"]["ink"]["wait"] = lv_btn_get_ink_wait_time(button);
		j["button"]["ink"]["out"] = lv_btn_get_ink_out_time(button);
		return j;
	}

	lv_obj_t* LVButton::FromJSON(json j)
	{
		if (!j["base"].is_object())
			return nullptr;

		lv_obj_t *button = lv_btn_create(lv_scr_act(), nullptr);
		json bj = j["button"];
		if(bj["styles"].is_object())
		{
			if(bj["styles"]["rel"].is_object())
			{
				const lv_style_t* style = &Style::Deserialize(bj["styles"]["rel"]);
				lv_btn_set_style(button, LV_BTN_STYLE_REL, style);
			}
			if (bj["styles"]["ina"].is_object())
			{
				const lv_style_t* style = &Style::Deserialize(bj["styles"]["ina"]);
				lv_btn_set_style(button, LV_BTN_STYLE_INA, style);
			}
			if (bj["styles"]["pr"].is_object())
			{
				const lv_style_t* style = &Style::Deserialize(bj["styles"]["pr"]);
				lv_btn_set_style(button, LV_BTN_STYLE_PR, style);
			}
			if (bj["styles"]["tglPr"].is_object())
			{
				const lv_style_t* style = &Style::Deserialize(bj["styles"]["tglPr"]);
				lv_btn_set_style(button, LV_BTN_STYLE_TGL_PR, style);
			}
			if (bj["styles"]["tglRel"].is_object())
			{
				const lv_style_t* style = &Style::Deserialize(bj["styles"]["tglRel"]);
				lv_btn_set_style(button, LV_BTN_STYLE_TGL_REL, style);
			}
		}
		if (bj["toggle"].is_boolean())
		{
			lv_btn_set_toggle(button, bj["toggle"]);
		}
		if (bj["layout"])
		{
			lv_btn_set_layout(button, bj["layout"]);
		}
		if (bj["fit"].is_object())
		{
			int b = 0, l = 0, t = 0, r = 0;
			if (bj["fit"]["0"].is_number())
			{
				b = bj["fit"]["0"];
			}
			if (bj["fit"]["1"].is_number())
			{
				l = bj["fit"]["1"];
			}
			if (bj["fit"]["2"].is_number())
			{
				t = bj["fit"]["2"];
			}
			if (bj["fit"]["3"].is_number())
			{
				r = bj["fit"]["3"];
			}
			lv_btn_set_fit4(button, l, r, t, b);
		}
		if(bj["ink"].is_object())
		{
			if (bj["ink"]["in"].is_number())
				lv_btn_set_ink_in_time(button, bj["ink"]["in"]);
			if (bj["ink"]["wait"].is_number())
				lv_btn_set_ink_wait_time(button, bj["ink"]["in"]);
			if (bj["ink"]["out"].is_number())
				lv_btn_set_ink_out_time(button, bj["ink"]["in"]);
		}
		return button;
	}
	
}