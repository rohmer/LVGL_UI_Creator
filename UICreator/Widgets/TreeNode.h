#pragma once
#include <any>
#include <string>
#include <utility>
#include <vector>
#include "lvgl.h"

class TreeNode
{
public:
    static constexpr int RowHeight = 32;
    static constexpr int IconSize = 24;
    TreeNode(std::string name, TreeNode* parent, lv_obj_t* object, bool protect = false);
    ~TreeNode();
    bool operator==(const TreeNode& other);
    TreeNode* DeepCopy();
    lv_obj_t* GetLVObject();
    lv_obj_t* GetRowObject() const { return clickObj; }
    lv_obj_t* GetIconObject() const { return iconObj; }
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
    std::pair<int, int> captionBounds() const;
    void addChild(TreeNode* childNode);
    void addChildFront(TreeNode* childNode);
    void removeChild(TreeNode* node);
    lv_obj_t *object, *buttonObj = nullptr, *buttonLabel = nullptr, *labelObj = nullptr, *clickObj = nullptr, *iconObj = nullptr;

private:
    TreeNode* parent = nullptr;
    std::vector<TreeNode *> children;
    unsigned int id = 0;
    unsigned int level = 0;
    std::string name;
    int x = 0, y = 0;
    bool expanded = false;
    bool selected = false;
    bool protectedNode = false;
    lv_style_t* objectStyle = nullptr;
    std::any nodeData;
};
