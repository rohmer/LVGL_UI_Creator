#include "TreeNode.h"

TreeNode::TreeNode(std::string name, TreeNode *parent, lv_obj_t* object, bool protect) :
	name(name),
	object(object),
	protectedNode(protect),
	expanded(false),
	selected(false),
	nodeData(nullptr)
{
}

TreeNode::~TreeNode()
{
	for (int i = 0; i < lvObjects.size(); ++i)
	{
		if (lvObjects[i] != nullptr)
			lv_obj_del(lvObjects[i]);
	}
	lv_obj_del_async(buttonObj);
	lv_obj_del_async(labelObj);
	lv_obj_del_async(clickObj);
	
	lvObjects.clear();
	for (std::vector<TreeNode*>::iterator it = children.begin();
		it != children.end();
		it++)
	{
		if ((*it) != nullptr)
			delete(*it);
	}
}

TreeNode *TreeNode::DeepCopy()
{
	lv_obj_t *objCopy;
	if (this->GetObject() == nullptr)
	{
		objCopy = nullptr;
	}
	else
	{
		objCopy = lv_obj_create(this->GetObject()->par, this->GetObject());
	}
	
	TreeNode *newNode = new TreeNode(name, parent, objCopy, protectedNode);
	// Copy UI items
	if(buttonObj!=nullptr)
	{
		newNode->buttonObj = lv_btn_create(this->GetObject()->par, buttonObj);
	}
	if(labelObj!=nullptr)
	{
		newNode->labelObj = lv_label_create(this->GetObject()->par, labelObj);
	}
	if(clickObj!=nullptr)
	{
		newNode->clickObj = lv_obj_create(this->GetObject()->par, clickObj);
	}
	
	for (std::vector<TreeNode*>::iterator it = children.begin();
		it != children.end();
		++it)
	{
		newNode->addChild((*it)->DeepCopy());
	}
	return newNode;
}

bool TreeNode::operator==(const TreeNode &other)
{
	if (other.id == id && other.name == name)
		return true;
	return false;
}

void TreeNode::SetNodeData(std::any nodeData)
{
	this->nodeData = nodeData;
}

std::any TreeNode::GetNodeData()
{
	return this->nodeData;
}

bool TreeNode::IsSelected()
{
	return selected;
}

void TreeNode::setID(unsigned int tnid)
{
	id = tnid;
}

void TreeNode::setPosition(int x, int y)
{
	this->x = x;
	this->y = y;
	if(clickObj!=nullptr)
		lv_obj_set_pos(clickObj, x, y);
}

lv_obj_t *TreeNode::GetObject()
{
	return object;
}

unsigned int TreeNode::GetID()
{
	return id;
}

std::string TreeNode::GetName()
{
	return name;
}

void TreeNode::addChild(TreeNode *node)
{
	node->parent = this;
	this->children.push_back(node);
}

unsigned int TreeNode::GetLevel()
{
	unsigned int i = 0;
	TreeNode *np = this;
	while (np->parent != nullptr)
		i++;
	return i;
}
