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
		const char **ma = lv_btnm_get_map_array(btnm);
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
		
		return j;
	}
}