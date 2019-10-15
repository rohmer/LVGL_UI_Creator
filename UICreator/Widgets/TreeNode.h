#pragma once
#include <any>
#include <string>
#include <vector>
#include "lvgl.h"

class TreeNode
{
public:
    TreeNode(std::string name, TreeNode* parent, lv_obj_t* object, bool protect = false);
    ~TreeNode();
    bool operator==(const TreeNode& other);
    TreeNode* DeepCopy();
    lv_obj_t* GetLVObject();
    unsigned int GetID();
    std::string GetName();
    unsigned int GetLevel();
    bool IsSelected();
    void SetNodeData(std::any nodeData);
    std::any GetNodeData();
    void SetExpanded(bool exp) { expanded = exp; }

private:
    friend class TreeView;
    void setID(unsigned int tnid);
    void setPosition(int x, int y);
    void addChild(TreeNode* childNode);
    void addChildFront(TreeNode* childNode);
    void removeChild(TreeNode* node);
    lv_obj_t *object, *buttonObj, *buttonLabel, *labelObj, *clickObj;

private:
    TreeNode* parent;
    std::vector<TreeNode *> children;
    unsigned int id;
    unsigned int level;
    std::string name;
    int x, y;
    bool expanded = false;
    bool selected = false;
    bool protectedNode = false;
    lv_style_t* objectStyle;
    std::any nodeData;
};
