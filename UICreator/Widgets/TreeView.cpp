#include "TreeView.h"
#include <Serialization/LVObject.h>
#include "../Utils/EditorStyle.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include "../Utils/EditorControls.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorEvents.h"
#include "../Utils/ObjectIcons.h"

namespace {
#if LVGL_VERSION_MAJOR >= 9
constexpr int RowPitch = TreeNode::RowHeight + 2;
#else
constexpr int RowPitch = 28;
#endif
}

TreeView::TreeView(unsigned int x,
                   unsigned int y,
                   unsigned int width,
                   unsigned int height,
                   std::string title,
                   bool inWindow,
                   bool ro,
                   lv_obj_t* parent) :
    x(x),
    y(y),
    width(width),
    height(height),
    title(title)
{
    readOnly = ro;
    createTreeViewContainer(inWindow, parent);
}

TreeView::~TreeView()
{
    for (int i = 0; i < topLevelNodes.size(); i++)
        delete(topLevelNodes[i]);
}

lv_obj_t* TreeView::GetTreeViewUI()
{
    return window;
}

TreeNode* TreeView::GetSelectedNode()
{
    return selectedNode;
}

TreeNode* TreeView::getNode(TreeNode* node, unsigned int nodeID)
{
    if (node->id == nodeID)
        return node;
    for (std::vector<TreeNode*>::iterator it = node->children.begin();
         it != node->children.end();
         ++it)
    {
        if ((*it)->id == nodeID)
            return *it;
        TreeNode* n = getNode(*it, nodeID);
        if (n != nullptr)
            return n;
    }

    return nullptr;
}

TreeNode* TreeView::GetNodeByID(unsigned nodeID)
{
    std::vector<TreeNode *> nodes = getAllNodes();
    for (int i = 0; i < nodes.size(); i++)
        if (nodes[i]->GetID() == nodeID)
            return nodes[i];
    return nullptr;
}

TreeNode* TreeView::GetNodeByObject(lv_obj_t* object)
{
    for(auto* node : getAllNodes())
        if(node->object == object)return node;
    return nullptr;
}

unsigned int TreeView::AddNode(std::string name,
                               lv_obj_t* object,
                               unsigned int parentID,
                               bool protect)
{
    // First we need to get the node pointer for the parent
    std::vector<TreeNode*> nodes = getAllNodes();
    TreeNode* parent = nullptr;
    for (int i = 0; i < nodes.size(); i++)
    {
        if (nodes[i]->id == parentID)
        {
            parent = nodes[i];
            break;
        }
    }

    TreeNode* newNode = new TreeNode(name, parent, object, protect);
    newNode->expanded = true;
    newNode->id = curID;
    curID++;
    if (parent == nullptr)
    {
        topLevelNodes.push_back(newNode);
        newNode->level=0;
    }
    else
    {
        parent->addChild(newNode);
        newNode->level=newNode->GetLevel();
    }
    createUIObjects(newNode);
    placeUIObjects();
    return newNode->id;
}

std::vector<TreeNode*> TreeView::getAllNodes(TreeNode* node)
{
    std::vector<TreeNode*> retVal;
    for (std::vector<TreeNode*>::iterator it = node->children.begin();
         it != node->children.end();
         ++it)
    {
        retVal.push_back(*it);
        std::vector<TreeNode*> children = getAllNodes(*it);
        retVal.insert(retVal.end(), children.begin(), children.end());
    }
    return retVal;
}

std::vector<TreeNode*> TreeView::getAllNodes()
{
    std::vector<TreeNode*> retVal;
    for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
         it != topLevelNodes.end();
         ++it)
    {
        retVal.push_back(*it);
        std::vector<TreeNode*> children = getAllNodes(*it);
        retVal.insert(retVal.end(), children.begin(), children.end());
    }
    return retVal;
}

#pragma region Create TreeView
void TreeView::createTreeViewContainer(bool inWin, lv_obj_t* parent)
{
    if (inWin)
    {
        if (parent == nullptr)
            window = EditorWindow::Create(lv_scr_act());
        else
            window = EditorWindow::Create(parent);
        EditorWindow::SetTitle(window, title.c_str());
        if (!readOnly)
        {
            deleteButton = EditorWindow::AddButton(window, LV_SYMBOL_TRASH, "delete");
            moveDownButton = EditorWindow::AddButton(window, LV_SYMBOL_DOWN, "move-down");
            copyButton = EditorWindow::AddButton(window, LV_SYMBOL_COPY, "copy");
            pasteButton = EditorWindow::AddButton(window, LV_SYMBOL_PASTE, "paste");
            moveUpButton = EditorWindow::AddButton(window, LV_SYMBOL_UP, "move-up");
            lv_obj_set_user_data(deleteButton, static_cast<lv_obj_user_data_t>(this));
            SetEditorEventCallback<deleteButtonCB>(deleteButton);
            lv_obj_set_click(deleteButton, true);
            SetEditorHidden(deleteButton, true);
        }
        lv_obj_set_size(window, width, height);
        lv_obj_set_pos(window, x, y);
        yOffset = 0;
#if LVGL_VERSION_MAJOR >= 9
        if (!readOnly)
        {
            auto* actions = lv_cont_create(EditorWindow::Content(window), nullptr);
            lv_obj_set_size(actions, LV_PCT(100), 40);
            lv_obj_set_style_pad_all(actions, 6, LV_PART_MAIN);
            lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
            for (auto* button : {deleteButton, copyButton, pasteButton, moveUpButton, moveDownButton})
            {
                lv_obj_set_parent(button, actions);
                SetEditorHidden(button, true);
            }
            searchField = lv_textarea_create(actions);
            lv_textarea_set_one_line(searchField, true);
            lv_textarea_set_placeholder_text(searchField, "Search objects...");
            lv_obj_set_size(searchField, 0, 30);
            lv_obj_set_flex_grow(searchField, 1);
            lv_obj_set_style_min_width(searchField, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(searchField, 5, 0);
            lv_obj_add_event_cb(searchField, [](lv_event_t* e) {
                auto* tree = static_cast<TreeView*>(lv_event_get_user_data(e));
                if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) tree->placeUIObjects();
                else if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
                    FocusEditorControl(EditorKeyboardGroup(), tree->searchField);
                }
            }, LV_EVENT_ALL, this);
            yOffset = 40;
        }
#endif
    }
    else
    {
        if (parent == nullptr)
            window = lv_cont_create(lv_scr_act(), nullptr);
        else
            window = lv_cont_create(parent, nullptr);
        // We need to add a place for our buttons at top
        lv_obj_set_size(window, width, height);

        deleteButton = lv_btn_create(window, nullptr);
        lv_obj_set_pos(deleteButton, width - 40, 2);
        lv_obj_t* dbLabel = lv_label_create(deleteButton, nullptr);
        lv_obj_set_size(deleteButton, 35, 35);
        lv_label_set_text(dbLabel, LV_SYMBOL_BACKSPACE);
        yOffset = 40;
        SetEditorHidden(deleteButton, true);
        moveDownButton = lv_btn_create(window, nullptr);
        lv_obj_set_pos(moveDownButton, width - 80, 2);
        lv_obj_set_size(moveDownButton, 35, 35);
        SetEditorHidden(moveDownButton, true);
        lv_obj_t* mdLbl = lv_label_create(moveDownButton, nullptr);
        lv_label_set_text(mdLbl, LV_SYMBOL_DOWN);
        copyButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(copyButton, 35, 35);
        lv_obj_set_pos(copyButton, width - 120, 2);
        SetEditorHidden(copyButton, true);
        lv_obj_t* cpLbl = lv_label_create(copyButton, nullptr);
        lv_label_set_text(cpLbl, LV_SYMBOL_COPY);
        pasteButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(pasteButton, 35, 35);
        lv_obj_set_pos(pasteButton, width - 160, 2);
        SetEditorHidden(pasteButton, true);
        lv_obj_t* pLbl = lv_label_create(pasteButton, nullptr);
        lv_label_set_text(pLbl, LV_SYMBOL_PASTE);
        moveUpButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(moveUpButton, 35, 35);
        lv_obj_set_pos(moveUpButton, width - 200, 2);
        SetEditorHidden(moveUpButton, true);
        lv_obj_t* muLbl = lv_label_create(moveUpButton, nullptr);
        lv_label_set_text(muLbl, LV_SYMBOL_UP);
        lv_obj_t* titleLabel = lv_label_create(window, nullptr);
        lv_obj_set_pos(titleLabel, 5, 5);
        lv_label_set_text(titleLabel, title.c_str());
#if LVGL_VERSION_MAJOR >= 9
        EditorIcons::Set(deleteButton, "delete", true, 20);
        EditorIcons::Set(moveDownButton, "move-down", true, 20);
        EditorIcons::Set(copyButton, "copy", true, 20);
        EditorIcons::Set(pasteButton, "paste", true, 20);
        EditorIcons::Set(moveUpButton, "move-up", true, 20);
#endif
    }
    treeContainer = lv_cont_create(inWin ? EditorWindow::Content(window) : window, nullptr);
    lv_obj_set_y(treeContainer, yOffset);
    lv_obj_set_size(treeContainer, width - 5, height - yOffset);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_style_bg_opa(treeContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_scrollable(treeContainer, true);
    lv_obj_set_scroll_dir(treeContainer, LV_DIR_VER);
    if (inWin)
    {
        auto* content = EditorWindow::Content(window);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(content, 0, LV_PART_MAIN);
        lv_obj_set_scrollable(content, false);
        lv_obj_set_width(treeContainer, LV_PCT(100));
        lv_obj_set_height(treeContainer, 0);
        lv_obj_set_flex_grow(treeContainer, 1);
    }
#endif

    if (inWin && readOnly)
        return;

    lv_obj_set_user_data(deleteButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<deleteButtonCB>(deleteButton);
    lv_obj_set_user_data(moveUpButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<moveUpButtonCB>(moveUpButton);
    lv_obj_set_user_data(moveDownButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<moveDownButtonCB>(moveDownButton);
    lv_obj_set_user_data(copyButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<copyButtonCB>(copyButton);
    lv_obj_set_user_data(pasteButton, static_cast<lv_obj_user_data_t>(this));
    SetEditorEventCallback<pasteButtonCB>(pasteButton);
}
#pragma endregion

#pragma region Create Node Objects
void TreeView::createUIObjects(TreeNode* node)
{
    sButtonCB* bcb = new sButtonCB();
    bcb->node = node;
    bcb->tv = this;
    // All the objects are created with the click object as the parent
    if (node->clickObj == nullptr)
    {
        node->clickObj = lv_obj_create(treeContainer, nullptr);
        lv_obj_set_size(node->clickObj, width - 5, 15);
        lv_obj_set_style(node->clickObj, &lv_style_transp);
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_set_height(node->clickObj, TreeNode::RowHeight);
        lv_obj_set_width(node->clickObj, LV_PCT(100));
        lv_obj_set_style_pad_all(node->clickObj, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(node->clickObj, 0, LV_PART_MAIN);
        lv_obj_set_scrollable(node->clickObj, false);
#endif
        lv_obj_set_user_data(node->clickObj, static_cast<lv_obj_user_data_t>(bcb));
        SetEditorEventCallback<clickObjCB>(node->clickObj);
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_add_event_cb(node->clickObj, [](lv_event_t* event) {
            auto* binding=static_cast<sButtonCB*>(lv_event_get_user_data(event));
            binding->tv->BeginRename(binding->node->object);
        }, LV_EVENT_DOUBLE_CLICKED, bcb);
#endif
#if LVGL_VERSION_MAJOR >= 9
        lv_obj_add_event_cb(node->clickObj, [](lv_event_t* event) {
            delete static_cast<sButtonCB*>(lv_event_get_user_data(event));
        }, LV_EVENT_DELETE, bcb);
#endif
    }
    if (node->buttonObj == nullptr)
    {
        node->buttonObj = lv_btn_create(node->clickObj, nullptr);
        lv_obj_set_size(node->buttonObj, 25, 15);
        lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_REL, &lv_style_pretty_color);
        lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_INA, &lv_style_pretty_color);
        lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_PR, &lv_style_pretty_color);
        lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_PR, &lv_style_pretty_color);
        lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_REL, &lv_style_pretty_color);
    }
    if (node->buttonLabel == nullptr)
    {
        node->buttonLabel = lv_label_create(node->buttonObj, nullptr);
        lv_obj_set_style(node->buttonLabel, &lv_style_pretty_color);
        bcb->label = node->buttonLabel;
    }
    if (node->labelObj == nullptr)
    {
        node->labelObj = lv_label_create(node->clickObj, nullptr);
        lv_label_set_text(node->labelObj, node->name.c_str());
        lv_label_set_style(node->labelObj, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
    }

    lv_obj_set_user_data(node->buttonObj, static_cast<lv_obj_user_data_t>(bcb));
#if LVGL_VERSION_MAJOR >= 9
    if(!node->iconObj)
        node->iconObj=ObjectIcons::Create(node->clickObj,ObjectIcons::ForObject(node->object),TreeNode::IconSize);
    lv_obj_set_size(node->buttonObj, 22, 22);
    lv_obj_set_style_pad_all(node->buttonObj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(node->buttonObj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(node->buttonObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_center(node->buttonLabel);
    lv_obj_set_style_bg_opa(node->buttonLabel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(node->labelObj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(node->labelObj,LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(node->labelObj,lv_font_get_line_height(lv_obj_get_style_text_font(node->labelObj,LV_PART_MAIN)));
    lv_obj_add_event_cb(node->clickObj,[](lv_event_t* event) {
        auto* binding=static_cast<sButtonCB*>(lv_event_get_user_data(event));
        binding->node->setPosition(binding->node->x,binding->node->y);
        binding->tv->positionRenameField(binding->node);
    },LV_EVENT_SIZE_CHANGED,bcb);
#endif
    SetEditorEventCallback<expandButtonCB>(node->buttonObj);
}
#pragma endregion

#pragma region Place Node Objects
int TreeView::placeUIObjects(TreeNode* node, int yCtr, bool shown)
{
    if (node->labelObj == nullptr ||
        node->clickObj == nullptr ||
        node->buttonLabel == nullptr ||
        node->buttonObj == nullptr)
    {
        createUIObjects(node);
    }
    node->setPosition(5, yCtr * RowPitch + 3);
    if (shown)
    {
        displayedNodes.push_back(node);
        yCtr++;
        SetEditorHidden(node->labelObj, false);
        SetEditorHidden(node->clickObj, false);
        SetEditorHidden(node->buttonLabel, false);
        SetEditorHidden(node->buttonObj, false);
    }
    else
    {
        SetEditorHidden(node->labelObj, true);
        SetEditorHidden(node->clickObj, true);
        SetEditorHidden(node->buttonLabel, true);
        SetEditorHidden(node->buttonObj, true);
    }
    if (node->children.empty())
    {
        lv_label_set_text(node->buttonLabel, LV_SYMBOL_MINUS);
    }
    else
    {
        if (node->expanded)
        {
            lv_label_set_text(node->buttonLabel, LV_SYMBOL_DOWN);
        }
        else
        {
            lv_label_set_text(node->buttonLabel, LV_SYMBOL_RIGHT);
        }
        for (std::vector<TreeNode*>::iterator cit = node->children.begin();
             cit != node->children.end();
             ++cit)
        {
            bool showChild = false;
            if (shown && node->expanded)
                showChild = true;
            yCtr = placeUIObjects(*cit, yCtr, showChild);
        }
    }
    return yCtr;
}

void TreeView::placeUIObjects()
{
    int yCtr = -1;
    displayedNodes.clear();
    for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
         it != topLevelNodes.end();
         ++it)
    {
        for(auto* object : {(*it)->clickObj, (*it)->labelObj, (*it)->buttonObj, (*it)->buttonLabel})
            SetEditorHidden(object, false);
        displayedNodes.push_back(*it);
        yCtr++;
        (*it)->setPosition(5, yCtr * RowPitch + 3);
        if ((*it)->children.empty())
        {
            lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_MINUS);
        }
        else
        {
            if ((*it)->expanded)
            {
                lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_DOWN);
                yCtr++;
            }
            else
            {
                lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_RIGHT);
            }
            for (std::vector<TreeNode*>::iterator cit = (*it)->children.begin();
                 cit != (*it)->children.end();
                 ++cit)
            {
                yCtr = placeUIObjects(*cit, yCtr, (*it)->expanded);
            }
        }
    }
#if LVGL_VERSION_MAJOR >= 9
    if(searchField && *lv_textarea_get_text(searchField)) {
        auto lower = [](std::string text) { for(auto& c : text)c = (char)std::tolower((unsigned char)c); return text; };
        const auto query = lower(lv_textarea_get_text(searchField));
        std::function<bool(TreeNode*)> matches = [&](TreeNode* node) {
            if(lower(node->name).find(query) != std::string::npos)return true;
            for(auto* child : node->children)if(matches(child))return true;
            return false;
        };
        displayedNodes.clear();int y = 0;
        for(auto* node : getAllNodes()) {
            const bool show = matches(node);
            for(auto* object : {node->clickObj, node->labelObj, node->buttonObj, node->buttonLabel})
                SetEditorHidden(object, !show);
            if(show) {node->setPosition(5, y++ * RowPitch + 3);displayedNodes.push_back(node);}
        }
    }
#endif
}
#pragma endregion

#pragma region Callbacks
void TreeView::clickObjCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    sButtonCB* bcbData = static_cast<sButtonCB*>(lv_obj_get_user_data(obj));
    if(bcbData->tv->renameField && lv_obj_get_parent(bcbData->tv->renameField)==obj) return;
    if(bcbData->tv->selectionHandler) {
        bcbData->tv->selectionHandler(bcbData->node->object, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
        return;
    }
    if (bcbData->node->selected)
    {
        bcbData->node->selected = false;
        if (bcbData->tv->selectedNode != nullptr)
        {
            bcbData->tv->selectedNode->selected = false;
        }
        if (bcbData->tv->selectedNode && bcbData->tv->selectedNode->object)
        {
#if LVGL_VERSION_MAJOR >= 9
            HighlightEditorObject(bcbData->tv->selectedNode->object, false);
#else
            lv_obj_set_style(bcbData->tv->selectedNode->object,
                             bcbData->tv->selectedNode->objectStyle);
#endif
        }
        bcbData->tv->selectedNode = nullptr;
        lv_obj_set_style(obj, &lv_style_transp);
        SetEditorHidden(bcbData->tv->deleteButton, true);
        SetEditorHidden(bcbData->tv->copyButton, true);
        SetEditorHidden(bcbData->tv->moveUpButton, true);
        SetEditorHidden(bcbData->tv->moveDownButton, true);
    }
    else
    {
        bcbData->node->selected = true;
        if (bcbData->node->object != nullptr)
        {
#if LVGL_VERSION_MAJOR >= 9
            HighlightEditorObject(bcbData->node->object, true);
#else
            bcbData->node->objectStyle = (lv_style_t*)lv_obj_get_style(bcbData->node->object);
            lv_obj_set_style(bcbData->node->object, &lv_style_plain);
#endif
        }

        if (bcbData->tv->selectedNode != nullptr)
        {
#if LVGL_VERSION_MAJOR >= 9
            if (bcbData->tv->selectedNode->object)
                HighlightEditorObject(bcbData->tv->selectedNode->object, false);
#else
            if (bcbData->tv->selectedNode->object != nullptr &&
                bcbData->tv->selectedNode->objectStyle != nullptr)
            {
                lv_obj_set_style(bcbData->tv->selectedNode->object, bcbData->tv->selectedNode->objectStyle);
            }
#endif
            bcbData->tv->selectedNode->selected = false;
            lv_obj_set_style(bcbData->tv->selectedNode->clickObj, &lv_style_transp);
        }
        bcbData->tv->selectedNode = bcbData->node;
        lv_obj_set_style(obj, &lv_style_plain_color);
        if (bcbData->tv->displayedNodes[0] == bcbData->node)
        {
            SetEditorHidden(bcbData->tv->moveUpButton, true);
        }
        else
        {
            if (!bcbData->node->protectedNode)
            {
                SetEditorHidden(bcbData->tv->moveUpButton, false);
            }
        }
        if (bcbData->tv->displayedNodes[bcbData->tv->displayedNodes.size() - 1] == bcbData->node)
        {
            SetEditorHidden(bcbData->tv->moveDownButton, true);
        }
        else
        {
            if (!bcbData->node->protectedNode)
            {
                SetEditorHidden(bcbData->tv->moveDownButton, false);
            }
        }
        if (!bcbData->node->protectedNode)
        {
            SetEditorHidden(bcbData->tv->deleteButton, false);
        }
        SetEditorHidden(bcbData->tv->copyButton, false);
        if (bcbData->tv->copiedNode != nullptr)
        {
            SetEditorHidden(bcbData->tv->pasteButton, false);
        }
    }
    if (bcbData->tv->selectCallbackFunc)
        bcbData->tv->selectCallbackFunc(bcbData->node);
}

void TreeView::expandButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    sButtonCB* buttonCB = static_cast<sButtonCB*>(lv_obj_get_user_data(obj));
    if (buttonCB->node->expanded)
    {
        buttonCB->node->expanded = false;
        lv_label_set_text(buttonCB->label, LV_SYMBOL_RIGHT);
        buttonCB->tv->placeUIObjects();
    }
    else
    {
        buttonCB->node->expanded = true;
        lv_label_set_text(buttonCB->label, LV_SYMBOL_DOWN);
        buttonCB->tv->placeUIObjects();
    }
}

void TreeView::moveUpButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    int nodeIdx = -1;
    for (int i = 0; i < tv->displayedNodes.size(); i++)
    {
        if (tv->displayedNodes[i] == tv->selectedNode)
        {
            nodeIdx = i;
            break;
        }
    }
    if (nodeIdx == -1)
        return;
    TreeNode* thisNode = tv->displayedNodes[nodeIdx];
    TreeNode* tradeNode = tv->displayedNodes[nodeIdx - 1];
    SetEditorHidden(tv->moveDownButton, false);
    if (tradeNode->parent == nullptr)
    {
        tv->topLevelNodes.insert(tv->topLevelNodes.begin(), thisNode);
        thisNode->parent->removeChild(thisNode);
        thisNode->parent = nullptr;
        thisNode->level = thisNode->GetLevel();
        tv->placeUIObjects();
        return;
    }
    if (thisNode->parent == tradeNode->parent)
    {
        int mIdx = -1;
        int tIdx = -1;
        for (int i = 0; i < thisNode->parent->children.size(); i++)
        {
            if (thisNode->parent->children[i] == thisNode)
                mIdx = i;
            if (thisNode->parent->children[i] == tradeNode)
                tIdx = i;
        }
        if (mIdx == -1 || tIdx == -1)
            return;
        thisNode->parent->children[mIdx] = tradeNode;
        thisNode->parent->children[tIdx] = thisNode;
        tv->placeUIObjects();
        return;
    }

    // We are changing parents
    thisNode->parent->removeChild(thisNode);
    thisNode->parent = tradeNode->parent;
    tradeNode->parent->addChildFront(thisNode);
    thisNode->level = thisNode->GetLevel();
    tv->placeUIObjects();
}

void TreeView::moveDownButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    int nodeIdx = -1;
    for (int i = 0; i < tv->displayedNodes.size(); i++)
    {
        if (tv->displayedNodes[i] == tv->selectedNode)
        {
            nodeIdx = i;
            break;
        }
    }
    if (nodeIdx == -1)
        return;
    SetEditorHidden(tv->moveUpButton, false);
    TreeNode* thisNode = tv->displayedNodes[nodeIdx];
    TreeNode* tradeNode = tv->displayedNodes[nodeIdx + 1];
    if (thisNode->parent == nullptr)
    {
        // We are a top level node
        thisNode->parent = tradeNode;
        int idx = -1;
        for (int i = 0; i < tv->topLevelNodes.size(); i++)
        {
            if (tv->topLevelNodes[i] == thisNode)
            {
                idx = i;
                break;
            }
        }
        if (idx != -1)
        {
            tv->topLevelNodes.erase(tv->topLevelNodes.begin() + idx);
        }
        tradeNode->addChildFront(thisNode);
        thisNode->level = thisNode->GetLevel();
        tv->placeUIObjects();
        return;
    }


    thisNode->parent->removeChild(thisNode);
    tradeNode->addChildFront(thisNode);
    thisNode->level = thisNode->GetLevel();
    tv->placeUIObjects();
}

void TreeView::deleteButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if(tv->deleteSelectionHandler)tv->deleteSelectionHandler();
    else if (tv->selectedNode)
        tv->DeleteObject(tv->selectedNode->object);
}

std::vector<lv_obj_t*> TreeView::GetObjects()
{
    std::vector<lv_obj_t*> objects;
    for (auto* node : getAllNodes())
        if (!node->protectedNode && node->object)
            objects.push_back(node->object);
    return objects;
}

void TreeView::SelectObject(lv_obj_t* object)
{
    TreeNode* target = nullptr;
    for (auto* node : getAllNodes())
        if (node->object == object) target = node;
    if (!target || selectedNode == target) return;
    if (selectedNode)
    {
        selectedNode->selected = false;
        lv_obj_set_style(selectedNode->clickObj, &lv_style_transp);
#if LVGL_VERSION_MAJOR >= 9
        if (!selectedNode->protectedNode) HighlightEditorObject(selectedNode->object, false);
#endif
    }
    selectedNode = target;
    target->selected = true;
    lv_obj_set_style(target->clickObj, &lv_style_plain_color);
#if LVGL_VERSION_MAJOR >= 9
    if (!target->protectedNode) HighlightEditorObject(target->object, true);
#endif
    SetEditorHidden(deleteButton, target->protectedNode);
    SetEditorHidden(copyButton, target->protectedNode);
    SetEditorHidden(moveUpButton, target->protectedNode);
    SetEditorHidden(moveDownButton, target->protectedNode);
    SetEditorHidden(pasteButton, copiedNode == nullptr);
}

bool TreeView::DeleteObject(lv_obj_t* object)
{
    return RemoveObject(object, true);
}

bool TreeView::RemoveObject(lv_obj_t* object, bool destroyWidget)
{
    TreeNode* target = nullptr;
    for (auto* node : getAllNodes())
        if (node->object == object && !node->protectedNode)
            target = node;
    if (!target) return false;
    selectedNode = nullptr;
    copiedNode = nullptr;
    if (target->parent)
        target->parent->removeChild(target);
    else
    {
        auto position = std::find(topLevelNodes.begin(), topLevelNodes.end(), target);
        if (position != topLevelNodes.end()) topLevelNodes.erase(position);
    }
    if (!destroyWidget) target->object = nullptr;
    delete target;
    for (auto* button : {deleteButton, moveUpButton, moveDownButton, copyButton, pasteButton})
        SetEditorHidden(button, true);
    placeUIObjects();
    return true;
}


void TreeView::copyButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if (tv->selectedNode == nullptr)
        return;
    tv->copiedNode = tv->selectedNode;
    SetEditorHidden(tv->pasteButton, false);
}

void TreeView::pasteButtonCB(lv_obj_t* obj, EditorEventCode ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if (tv->selectedNode == nullptr)
        return;
    if (tv->copiedNode == nullptr)
    {
        SetEditorHidden(tv->pasteButton, true);
        return;
    }
    TreeNode* tn = tv->copiedNode->DeepCopy();
    tv->curID++;
    tn->id = tv->curID;
    tv->selectedNode->addChild(tn);
    // Create the new nodes object
    tv->createUIObjects(tn);
    tn->level = tn->GetLevel();
    tv->placeUIObjects();
}

void TreeView::AddSelectCallback(tv_callback cbMethod)
{
    this->selectCallbackFunc = cbMethod;
}

void TreeView::AddDeleteCallback(tv_callback cbMethod)
{
    this->deleteCallbackFunc = cbMethod;
}


#pragma endregion

void TreeView::ShowSelection(const std::vector<lv_obj_t*>& objects, lv_obj_t* primary)
{
    selectedNode = nullptr;
    for(auto* node : getAllNodes()) {
        node->selected = std::find(objects.begin(), objects.end(), node->object) != objects.end() || node->object == primary;
        lv_obj_set_style(node->clickObj, node->selected ? &lv_style_plain_color : &lv_style_transp);
        if(!node->protectedNode && node->object)HighlightEditorObject(node->object, node->selected);
        if(node->object == primary)selectedNode = node;
    }
    SetEditorHidden(deleteButton, !selectedNode || selectedNode->protectedNode);
    for(auto* button : {copyButton, moveUpButton, moveDownButton})
        SetEditorHidden(button, !selectedNode || selectedNode->protectedNode || objects.size()>1);
}



bool TreeView::RenameObject(lv_obj_t* object, const std::string& supplied)
{
    const auto first = supplied.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return false;
    const auto last = supplied.find_last_not_of(" \t\r\n");
    auto name = supplied.substr(first, last-first+1);
    if(name.size() > 128 || name.find_first_of("\r\n") != std::string::npos) return false;
    for(auto* node : getAllNodes()) if(node->object == object) {
        if(node->name == name) return true;
        EditorEdit::Scope edit;
        if(object) {
            auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
            if(!data) {
                auto record = Serialization::LVObject::ToJSON(object);
                Serialization::LVObject::FromJSON(record, object);
                data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
            }
            data->objectName = name;
            data->objectJson["base"]["name"] = name;
        }
        node->name = name;
        lv_label_set_text(node->labelObj, name.c_str());
        placeUIObjects();
        return true;
    }
    return false;
}

void TreeView::positionRenameField(TreeNode* node)
{
#if LVGL_VERSION_MAJOR >= 9
    if(!renameField || lv_obj_get_parent(renameField)!=node->clickObj)return;
    // Child coordinates may still be pending during a row resize. Use the same
    // current row bounds as the caption instead of its cached layout.
    const auto [left,captionWidth]=node->captionBounds();
    lv_obj_set_pos(renameField,left,2);
    lv_obj_set_size(renameField,captionWidth,TreeNode::RowHeight-4);
#endif
}

void TreeView::BeginRename(lv_obj_t* object)
{
#if LVGL_VERSION_MAJOR >= 9
    if(readOnly || renameField) return;
    TreeNode* target = object ? nullptr : selectedNode;
    if(object) for(auto* node : getAllNodes()) if(node->object == object) target = node;
    if(!target || !target->object) return;
    struct Rename { TreeView* tree; lv_obj_t* object; bool finished = false; };
    auto* binding = new Rename{this,target->object};
    renameField = lv_textarea_create(target->clickObj);
    lv_textarea_set_one_line(renameField,true);
    lv_textarea_set_max_length(renameField,128);
    lv_textarea_set_text(renameField,target->name.c_str());
    EditorStyle::Control(renameField);
    lv_obj_set_style_pad_all(renameField,3,0);
    lv_obj_update_layout(target->clickObj);
    positionRenameField(target);
    lv_obj_add_event_cb(renameField,[](lv_event_t* event) {
        auto* state=static_cast<Rename*>(lv_event_get_user_data(event));
        const auto code=lv_event_get_code(event);
        if(code==LV_EVENT_DELETE) {state->tree->renameField=nullptr;delete state;return;}
        if(state->finished) return;
        if(code==LV_EVENT_KEY && lv_event_get_key(event)==LV_KEY_ESC) {
            state->finished=true;lv_obj_delete_async(lv_event_get_target_obj(event));return;
        }
        if(code==LV_EVENT_READY || code==LV_EVENT_DEFOCUSED || code==LV_EVENT_CANCEL) {
            state->finished=true;
            if(code!=LV_EVENT_CANCEL && lv_obj_is_valid(state->object)) state->tree->RenameObject(state->object,lv_textarea_get_text(lv_event_get_target_obj(event)));
            lv_obj_delete_async(lv_event_get_target_obj(event));
        }
    },LV_EVENT_ALL,binding);
    BindEditorControl(EditorKeyboardGroup(),renameField);
    FocusEditorControl(EditorKeyboardGroup(),renameField);
    lv_textarea_set_cursor_pos(renameField,LV_TEXTAREA_CURSOR_LAST);
#endif
}
