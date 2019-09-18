#include "Arc.h"

void Arc::Create(lv_obj_t* parent, TreeView* objectTree, TreeNode *parentTreeNode)
{
	// Creation should never be called with a null parent or parentTreeNode
	if (parent == nullptr)
		return;
	if(parentTreeNode==nullptr)
		return;

	lv_obj_t *newObj = lv_arc_create(parent,nullptr);
	lv_obj_set_size(newObj, 25, 25);
	lv_obj_set_parent(newObj, parent);
	lv_arc_set_angles(newObj, 90, 270);

	
	
}
