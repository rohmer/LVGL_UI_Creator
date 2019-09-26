#include "LVButtonMatrix.h"

namespace Serialization
{
	json LVButtonMatrix::ToJSON(lv_obj_t* btnm)
	{
		json j;
		j["base"] = LVObject::ToJSON(btnm);
        lv_btnm_ext_t * ext =(lv_btnm_ext_t*) lv_obj_get_ext_attr(btnm);
        int btnCnt = ext->btn_cnt;
        json bj = j["btnm"];
		int buttonCt = ext->btn_cnt;
		const char **ma = ext->map_p;
		int ctr=0;
		int row = 0;
		while(*ma)
		{
			const char *p = *ma;
			while(strlen(p)>0)
			{
				j["btnMap"][row][ctr] = p;
				p++;
				ctr++;
			}
			row++;
			ma++;
		}
		for(int i=0; i<buttonCt; i++)
		{
			j["ctrl"][i] = ext->ctrl_bits[i];
		}
		if (ext->recolor == 1)
			j["recolor"] = true;
		else
			j["recolor"] = false;
		if (ext->one_toggle == 1)
			j["oneToggle"] = true;
		else
			j["oneToggle"] = false;
		for(int i=0; i<_LV_BTN_STATE_NUM; i++)
		{
			lv_style_t *sty = (lv_style_t*)ext->styles_btn[i];
			j["btnStyles"][i] = Style::Serialize(*sty);
		}
		
		return j;
	}
}