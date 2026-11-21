#pragma once
#include <map>
#include <functional>
#include "../Utils/EditorEvents.h"
#include <sstream>
#include <string>
#include <vector>

#include "TreeNode.h"


class TreeView
{
    typedef void (*tv_callback)(TreeNode* nodeSelected);

public:
    TreeView(unsigned int x,
             unsigned int y,
             unsigned int width,
             unsigned int height,
             std::string title,
             bool inWindow,
             bool ro,
             lv_obj_t* parent = nullptr);
    ~TreeView();

    unsigned int AddNode(std::string name, lv_obj_t* object,
                         unsigned int parentID = 0,
                         bool protect = false);
    lv_obj_t* GetTreeViewUI();
    TreeNode* GetSelectedNode();
    TreeNode* GetNodeByID(unsigned int nodeID);
    TreeNode* GetNodeByObject(lv_obj_t* object);
    void AddSelectCallback(tv_callback cb);
    void AddDeleteCallback(tv_callback cb);
    bool DeleteObject(lv_obj_t* object);
    void ForgetObject(lv_obj_t* object) { RemoveObject(object, false); }
    void SelectObject(lv_obj_t* object);
    void ShowSelection(const std::vector<lv_obj_t*>& objects, lv_obj_t* primary);
    std::function<void(lv_obj_t*, bool)> selectionHandler;
    std::function<void()> deleteSelectionHandler;
    std::vector<lv_obj_t*> GetObjects();
    bool RenameObject(lv_obj_t* object, const std::string& name);
    void BeginRename(lv_obj_t* object = nullptr);

    void ClearNodes()
    {
        topLevelNodes.clear();
    }

private:
    bool RemoveObject(lv_obj_t* object, bool destroyWidget);
    lv_obj_t* searchField = nullptr;
    lv_obj_t* renameField = nullptr;
    bool readOnly;
    int x, y, width, height, yOffset, curID = 0;
    std::string title;
    lv_obj_t* tvContainer;
    TreeNode* selectedNode = nullptr;
    TreeNode* copiedNode = nullptr;

    lv_obj_t *window, *deleteButton, *treeContainer,
             *moveUpButton, *moveDownButton, *copyButton, *pasteButton, *selectionBar;

    std::vector<TreeNode*> topLevelNodes;

    void createTreeViewContainer(bool inWin, lv_obj_t* parent);
    void createUIObjects(TreeNode* node);
    void positionRenameField(TreeNode* node);
    void placeUIObjects();
    int placeUIObjects(TreeNode* node, int yCtr, bool shown);

    TreeNode* getNode(TreeNode* node, unsigned int nodeID);

    std::vector<TreeNode*> getAllNodes();
    std::vector<TreeNode*> getAllNodes(TreeNode* node);

    struct sButtonCB
    {
        TreeNode* node;
        lv_obj_t* label;
        TreeView* tv;
    };

    // Callbacks
    static void deleteButtonCB(lv_obj_t* obj, EditorEventCode ev);
    static void expandButtonCB(lv_obj_t* obj, EditorEventCode ev);
    static void clickObjCB(lv_obj_t* obj, EditorEventCode ev);
    static void moveUpButtonCB(lv_obj_t* obj, EditorEventCode ev);
    static void copyButtonCB(lv_obj_t* obj, EditorEventCode ev);
    static void pasteButtonCB(lv_obj_t* obj, EditorEventCode ev);
    static void moveDownButtonCB(lv_obj_t* obj, EditorEventCode ev);

    tv_callback selectCallbackFunc = nullptr;
    tv_callback deleteCallbackFunc = nullptr;
    std::vector<TreeNode *> displayedNodes;
};
