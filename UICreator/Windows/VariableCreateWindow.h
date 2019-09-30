#pragma once

#include <string>
#include <map>

#include <lvgl/lvgl.h>
#include "../ObjectUserData.h"
#include "ToolBar.h"
#include "../Widgets/TreeView.h"
#include "../Serialization/Serializers.h"

class VariableCreateWindow
{
public:
	VariableCreateWindow(ToolBar *toolBar);

private:
	ToolBar *toolbar;
	TreeNode *baseObjProps;
	std::map<std::string, TreeNode*> *objTypeProps;
	TreeView *treeView;
	
	void createBaseProperties();
	TreeNode *createObjTypeProperties(lv_obj_t *object);

	unsigned int parseNode(std::string nodeName, const json &node, unsigned int parent);
};