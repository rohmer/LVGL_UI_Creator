#include "LVButtonMatrix.h"

namespace Serialization
{
	json LVButtonMatrix::ToJSON(lv_obj_t* btnm)
	{
		json j;
		j["base"] = LVObject::ToJSON(btnm);
		const char **ma=lv_btnm_get_map_array(btnm);
		int arraySize = (sizeof(ma) / sizeof((ma)[0]));
		for (int i = 0; i < arraySize; i++)
		{
			j["buttonMap"]["button"][i]["txt"] = lv_btnm_get_btn_text(btnm, i);
		}
		fff
	}
}