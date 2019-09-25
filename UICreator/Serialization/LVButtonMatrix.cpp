#include "LVButtonMatrix.h"

namespace Serialization
{
	json LVButtonMatrix::ToJSON(lv_obj_t* btnm)
	{
		json j;
		j["base"] = LVObject::ToJSON(btnm);
        lv_btnm_ext_t * ext =(lv_btnm_ext_t*) lv_obj_get_ext_attr(btnm);
        int btnCnt = ext->btn_cnt;
        json bj = j["button"];


	}
}