#include "Screen.h"
#include "../../UICreator/ObjectUserData.h"


namespace Serialization
{
	json JsonDoc::ToJSON(std::vector<lv_obj_t*> objects)
	{
		json sj;
		for(int i=0; i<objects.size(); i++)
		{
			ObjectUserData* oud = (ObjectUserData*)lv_obj_get_user_data(objects[i]);
			json j = oud->objectJson;
			j["OID"] = oud->objectID;
			j["parentOID"] = oud->objectID;
			sj["objects"][i] = j;
		}
		//TODO: optimize styles, so we have a single one if it is used in more than one place
		return sj;
	}
	std::vector<lv_obj_t*> JsonDoc::FromJSON(json j)
	{
		std::vector<lv_obj_t*> objects;
		std::map<std::string, lv_style_t> styles;

		return objects;		
	}
}
