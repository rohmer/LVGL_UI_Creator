#include "ObjectTools.h"

std::vector<lv_obj_t*> ObjectTools::GetChildren(lv_obj_t* object)
{
	lv_obj_t * i;
	std::vector<lv_obj_t*> children;
	lv_obj_t * result = nullptr;

	result = (lv_obj_t*)lv_ll_get_head(&object->child_ll);
	while(result!=nullptr)
	{
		children.push_back(result);
		std::vector<lv_obj_t*> cNodes = GetChildren(result);
		children.insert(children.end(), cNodes.begin(), cNodes.end());
		result = (lv_obj_t*)lv_ll_get_next(&object->child_ll, result);
	}

	return children;
}
