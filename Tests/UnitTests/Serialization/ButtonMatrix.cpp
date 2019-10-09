#include "catch.hpp"
#include "../3rdParty/LVGL/lvgl/lvgl.h"
#include <Serialization/LVButtonMatrix.h>

TEST_CASE("Serialize Button Matrix")
{
	lv_obj_t* button = lv_btnm_create(lv_scr_act(), nullptr);
	lv_btnm_set_recolor(button, true);
	lv_btnm_set_one_toggle(button, true);
	json  j = Serialization::LVButtonMatrix::ToJSON(button);
	
	REQUIRE(j["btnm"]["oneToggle"] == true);
	REQUIRE(j["btnm"]["recolor"] == true);
	REQUIRE(j["btnm"]["btnMap"][0] == "Btn1");
	REQUIRE(j["btnm"]["btnMap"][4] == "Btn2");
	REQUIRE(j["btnm"]["btnMap"][8] == "Btn3");
	REQUIRE(j["btnm"]["btnMap"][13] == "Btn4");
	REQUIRE(j["btnm"]["btnMap"][17] == "Btn5");
	REQUIRE(j["btnm"]["btnStyles"][0]["name"] == "lv_style_btn_rel");
	REQUIRE(j["btnm"]["btnStyles"][1]["name"] == "lv_style_btn_pr");
	REQUIRE(j["btnm"]["btnStyles"][2]["name"] == "lv_style_btn_tgl_rel");
	REQUIRE(j["btnm"]["btnStyles"][3]["name"] == "lv_style_btn_tgl_pr");
	REQUIRE(j["btnm"]["btnStyles"][4]["name"] == "lv_style_btn_ina");
}

TEST_CASE("Deserialize Button Matrix")
{
	lv_obj_t* button = lv_btnm_create(lv_scr_act(), nullptr);
	json j = Serialization::LVButtonMatrix::ToJSON(button);
	lv_obj_t* nBtn = Serialization::LVButtonMatrix::FromJSON(j);

	std::vector<std::string> btn1, btn2;
	lv_btnm_ext_t* ext = (lv_btnm_ext_t*)lv_obj_get_ext_attr(button);
	int btnCnt = ext->btn_cnt;
	int buttonCt = ext->btn_cnt;
	const char** ma = ext->map_p;
	int ctr = 0;
	int row = 0;
	while (*ma)
	{
		const char* p = *ma;
		while (strlen(p) > 0)
		{
			btn1.push_back(p);
			p++;
			ctr++;
		}
		ma++;
	}

	lv_btnm_ext_t* ext2 = (lv_btnm_ext_t*)lv_obj_get_ext_attr(nBtn);
	int btnCnt2 = ext->btn_cnt;
	const char** ma2 = ext->map_p;
	ctr = 0;
	row = 0;
	while (*ma2)
	{
		const char* p = *ma2;
		while (strlen(p) > 0)
		{
			btn2.push_back(p);
			p++;
			ctr++;
		}
		ma2++;
	}
	REQUIRE(btnCnt == btnCnt2);
	for (int i = 0; i < btnCnt; i++)
		REQUIRE(btn1[i] == btn2[i]);
}