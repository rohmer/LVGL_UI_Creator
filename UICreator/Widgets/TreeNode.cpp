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
	if (object != nullptr)
	{
		lv_obj_del(object);
	}
	lv_obj_del(buttonObj);
	lv_obj_del(labelObj);
	lv_obj_del(clickObj);
	
	for (std::vector<TreeNode*>::iterator it = children.begin();
		it != children.end();
		it++)
	{
		if ((*it) != nullptr)
			delete(*it);
	}
}

void TreeNode::removeChild(TreeNode *node)
{
	int nodeIdx = -1;
	for(int i=0; i<children.size(); i++)
	{
		if (children[i] == node)
		{
			nodeIdx = i;
			break;
		}
	}
	if (nodeIdx != -1)
		children.erase(children.begin() + nodeIdx);
}

TreeNode *TreeNode::DeepCopy()
{
	lv_obj_t *objCopy;
	if (this->GetLVObject() == nullptr)
	{
		objCopy = nullptr;
	}
	else
	{
		objCopy = lv_obj_create(this->GetLVObject()->par, this->GetLVObject());
	}
	
	TreeNode *newNode = new TreeNode(name, parent, objCopy, protectedNode);
	
	for (std::vector<TreeNode*>::iterator it = children.begin();
		it != children.end();
		++it)
	{
		newNode->addChild((*it)->DeepCopy());
	}
	newNode->selected = false;
	newNode->level = newNode->GetLevel();
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
	if (buttonObj != nullptr)
		lv_obj_set_pos(buttonObj, x+ 4 + (level * 15), y);
	if (labelObj != nullptr)
		lv_obj_set_pos(labelObj, x + 30 + (level * 15), y);
}

lv_obj_t *TreeNode::GetLVObject()
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

void TreeNode::addChildFront(TreeNode* childNode)
{
	childNode->parent = this;
	children.insert(children.begin(), childNode);
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
	{
		i++;
		np = np->parent;
	}
	return i;
}
