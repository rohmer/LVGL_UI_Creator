#include "TreeNode.h"

TreeNode::TreeNode(std::string name, lv_obj_t* object, bool protect) :
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
    lvObjects.clear();
    for (std::vector<TreeNode*>::iterator it = children.begin();
        it != children.end();
        it++)
    {
        if ((*it) != nullptr)
            delete(*it);
    }
}

void TreeNode::RemoveChild(TreeNode *node)
{
    int idx = -1;
    for (int i = 0; i < children.size(); i++)
        if (children[i] == node)
            idx = i;
    if (idx != -1)
        children.erase(children.begin() + idx);
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

bool TreeNode::GetSelected()
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
	this->children.push_back(node);
}

unsigned int TreeNode::GetLevel()
{
	return level;
}

std::vector<TreeNode*> TreeNode::GetChildren()
{
	return children;
}