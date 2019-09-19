#include "TreeNodeV1.h"

TreeNodeV1::TreeNodeV1(std::string name, lv_obj_t* object, bool protect) :
	name(name),
	object(object),
	protectedNode(protect),
	expanded(false),
	selected(false),
	nodeData(nullptr)
{
}

TreeNodeV1::~TreeNodeV1()
{
    for (int i = 0; i < lvObjects.size(); ++i)
    {
        if (lvObjects[i] != nullptr)
            lv_obj_del(lvObjects[i]);
    }
    lvObjects.clear();
    for (std::vector<TreeNodeV1*>::iterator it = children.begin();
        it != children.end();
        it++)
    {
        if ((*it) != nullptr)
            delete(*it);
    }
}

TreeNodeV1 *TreeNodeV1::DeepCopy()
{	
	lv_obj_t *objCopy;
	if(this->GetObject()==nullptr)
	{
		objCopy = nullptr;
	} else
	{
		objCopy = lv_obj_create(this->GetObject()->par, this->GetObject());
	}
	TreeNodeV1 *newNode = new TreeNodeV1(name, objCopy, protectedNode);
	for(std::vector<TreeNodeV1*>::iterator it=children.begin();
		it!=children.end();
		++it)
	{
		newNode->addChild((*it)->DeepCopy());
	}
	return newNode;
}

void TreeNodeV1::RemoveChild(TreeNodeV1 *node)
{
    int idx = -1;
    for (int i = 0; i < children.size(); i++)
        if (children[i] == node)
            idx = i;
    if (idx != -1)
        children.erase(children.begin() + idx);
}

bool TreeNodeV1::operator==(const TreeNodeV1 &other)
{
	if (other.id == id && other.name == name)
		return true;
	return false;
}

void TreeNodeV1::SetNodeData(std::any nodeData)
{
	this->nodeData = nodeData;
}

std::any TreeNodeV1::GetNodeData()
{
	return this->nodeData;
}

bool TreeNodeV1::GetSelected()
{
	return selected;
}

void TreeNodeV1::setID(unsigned int tnid)
{
	id = tnid;
}

void TreeNodeV1::setPosition(int x, int y)
{
	this->x = x;
	this->y = y;
}

lv_obj_t *TreeNodeV1::GetObject()
{
	return object;
}

unsigned int TreeNodeV1::GetID()
{
	return id;
}

std::string TreeNodeV1::GetName()
{
	return name;
}

void TreeNodeV1::addChild(TreeNodeV1 *node)
{
	this->children.push_back(node);
}

unsigned int TreeNodeV1::GetLevel()
{
	return level;
}

std::vector<TreeNodeV1*> TreeNodeV1::GetChildren()
{
	return children;
}