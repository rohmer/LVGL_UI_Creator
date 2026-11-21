#include "TreeNode.h"
#include <Serialization/ObjectSerializer.h>
#include "../Utils/EditorControls.h"
#include <algorithm>

TreeNode::TreeNode(std::string name, TreeNode* parent, lv_obj_t* object, bool protect) :
    object(object),
    parent(parent),
    name(name),
    expanded(false),
    selected(false),
    protectedNode(protect),
    nodeData(nullptr)
{
}

TreeNode::~TreeNode()
{
    for (std::vector<TreeNode*>::iterator it = children.begin();
         it != children.end();
         ++it)
    {
        if ((*it) != nullptr)
            delete(*it);
    }
    if (object && !protectedNode)
        lv_obj_delete(object);
    if (clickObj)
        lv_obj_delete(clickObj);
}

void TreeNode::removeChild(TreeNode* node)
{
    int nodeIdx = -1;
    for (int i = 0; i < children.size(); i++)
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

TreeNode* TreeNode::DeepCopy()
{
    lv_obj_t* objCopy;
    if (this->GetLVObject() == nullptr)
    {
        objCopy = nullptr;
    }
    else
    {
#if LVGL_VERSION_MAJOR >= 9
        objCopy = Serialization::ObjectSerializer::DeserializeObject(
            Serialization::ObjectSerializer::SerializeObject(GetLVObject()));
        if (objCopy)
            lv_obj_set_parent(objCopy, lv_obj_get_parent(GetLVObject()));
#else
        objCopy = lv_obj_create(this->GetLVObject()->par, this->GetLVObject());
#endif
    }

    TreeNode* newNode = new TreeNode(name, parent, objCopy, protectedNode);

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

bool TreeNode::operator==(const TreeNode& other)
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

std::pair<int, int> TreeNode::captionBounds() const
{
#if LVGL_VERSION_MAJOR >= 9
    const int rowWidth=lv_obj_get_content_width(clickObj);
#else
    const int rowWidth=lv_obj_get_width(clickObj);
#endif
    const int indent=std::min(62+int(level)*15,std::max(62,rowWidth-48));
    return {indent,std::max(1,rowWidth-indent-8)};
}

void TreeNode::setPosition(int x, int y)
{
    this->x = x;
    this->y = y;
    if (clickObj != nullptr)
        lv_obj_set_pos(clickObj, x, y);
#if LVGL_VERSION_MAJOR >= 9
    if(clickObj && labelObj) {
        if(lv_obj_get_content_width(clickObj)>0) {
            // Keep disclosure, object icon and caption in separate slots. Clamp
            // deep indentation so narrow panels still have room for the name.
            const auto [indent,captionWidth]=captionBounds();
            lv_obj_set_width(labelObj,captionWidth);
            lv_obj_align(labelObj,LV_ALIGN_LEFT_MID,indent,0);
            if(iconObj)lv_obj_align(iconObj,LV_ALIGN_LEFT_MID,indent-32,0);
            if(buttonObj)lv_obj_align(buttonObj,LV_ALIGN_LEFT_MID,indent-58,0);
        }
    }
#else
    if(buttonObj)lv_obj_set_pos(buttonObj,4+(level*15),0);
    if(labelObj)lv_obj_set_pos(labelObj,30+(level*15),5);
#endif
}

lv_obj_t* TreeNode::GetLVObject()
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

void TreeNode::addChild(TreeNode* node)
{
    node->parent = this;
    this->children.push_back(node);
}

unsigned int TreeNode::GetLevel()
{
    unsigned int i = 0;
    auto np = this;
    while (np->parent != nullptr)
    {
        i++;
        np = np->parent;
    }
    return i;
}
