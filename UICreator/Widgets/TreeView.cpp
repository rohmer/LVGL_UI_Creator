#include "TreeView.h"

TreeView::TreeView(unsigned int x,
                   unsigned int y,
                   unsigned int width,
                   unsigned int height,
                   std::string title,
                   bool ro,
                   bool inWindow,
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
            window = lv_win_create(lv_scr_act(), nullptr);
        else
            window = lv_win_create(parent, nullptr);
        lv_win_set_title(window, title.c_str());
        if (!readOnly)
        {
            deleteButton = lv_win_add_btn(window, LV_SYMBOL_TRASH);
            moveDownButton = lv_win_add_btn(window, LV_SYMBOL_UP);
            copyButton = lv_win_add_btn(window, LV_SYMBOL_COPY);
            pasteButton = lv_win_add_btn(window, LV_SYMBOL_PASTE);
            moveUpButton = lv_win_add_btn(window, LV_SYMBOL_UP);
            lv_obj_set_user_data(deleteButton, static_cast<lv_obj_user_data_t>(this));
            lv_obj_set_event_cb(deleteButton, deleteButtonCB);
            lv_obj_set_click(deleteButton, true);
            lv_obj_set_hidden(deleteButton, true);
        }
        lv_obj_set_size(window, width, height);
        lv_obj_set_pos(window, x, y);
        yOffset = 0;
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
        lv_obj_set_hidden(deleteButton, true);
        moveDownButton = lv_btn_create(window, nullptr);
        lv_obj_set_pos(moveDownButton, width - 80, 2);
        lv_obj_set_size(moveDownButton, 35, 35);
        lv_obj_set_hidden(moveDownButton, true);
        lv_obj_t* mdLbl = lv_label_create(moveDownButton, nullptr);
        lv_label_set_text(mdLbl, LV_SYMBOL_DOWN);
        copyButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(copyButton, 35, 35);
        lv_obj_set_pos(copyButton, width - 120, 2);
        lv_obj_set_hidden(copyButton, true);
        lv_obj_t* cpLbl = lv_label_create(copyButton, nullptr);
        lv_label_set_text(cpLbl, LV_SYMBOL_COPY);
        pasteButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(pasteButton, 35, 35);
        lv_obj_set_pos(pasteButton, width - 160, 2);
        lv_obj_set_hidden(pasteButton, true);
        lv_obj_t* pLbl = lv_label_create(pasteButton, nullptr);
        lv_label_set_text(pLbl, LV_SYMBOL_PASTE);
        moveUpButton = lv_btn_create(window, nullptr);
        lv_obj_set_size(moveUpButton, 35, 35);
        lv_obj_set_pos(moveUpButton, width - 200, 2);
        lv_obj_set_hidden(moveUpButton, true);
        lv_obj_t* muLbl = lv_label_create(moveUpButton, nullptr);
        lv_label_set_text(muLbl, LV_SYMBOL_UP);
        lv_obj_t* titleLabel = lv_label_create(window, nullptr);
        lv_obj_set_pos(titleLabel, 5, 5);
        lv_label_set_text(titleLabel, title.c_str());
    }
    treeContainer = lv_cont_create(window, nullptr);
    lv_obj_set_y(treeContainer, yOffset);
    lv_obj_set_size(treeContainer, width - 5, height - yOffset);

    lv_obj_set_user_data(deleteButton, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(deleteButton, deleteButtonCB);
    lv_obj_set_user_data(moveUpButton, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(moveUpButton, moveUpButtonCB);
    lv_obj_set_user_data(moveDownButton, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(moveDownButton, moveDownButtonCB);
    lv_obj_set_user_data(copyButton, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(copyButton, copyButtonCB);
    lv_obj_set_user_data(pasteButton, static_cast<lv_obj_user_data_t>(this));
    lv_obj_set_event_cb(pasteButton, pasteButtonCB);
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
        lv_obj_set_user_data(node->clickObj, static_cast<lv_obj_user_data_t>(bcb));
        lv_obj_set_event_cb(node->clickObj, clickObjCB);
    }
    if (node->buttonObj == nullptr)
    {
        node->buttonObj = lv_btn_create(treeContainer, nullptr);
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
        node->labelObj = lv_label_create(treeContainer, nullptr);
        lv_label_set_text(node->labelObj, node->name.c_str());
        lv_label_set_style(node->labelObj, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
    }

    lv_obj_set_user_data(node->buttonObj, static_cast<lv_obj_user_data_t>(bcb));
    lv_obj_set_event_cb(node->buttonObj, expandButtonCB);
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
    node->setPosition(x + 5, yCtr * 17 + 3);
    if (shown)
    {
        displayedNodes.push_back(node);
        yCtr++;
        lv_obj_set_hidden(node->labelObj, false);
        lv_obj_set_hidden(node->clickObj, false);
        lv_obj_set_hidden(node->buttonLabel, false);
        lv_obj_set_hidden(node->buttonObj, false);
    }
    else
    {
        lv_obj_set_hidden(node->labelObj, true);
        lv_obj_set_hidden(node->clickObj, true);
        lv_obj_set_hidden(node->buttonLabel, true);
        lv_obj_set_hidden(node->buttonObj, true);
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
        displayedNodes.push_back(*it);
        yCtr++;
        (*it)->setPosition(x + 5, yCtr * 17 + 3);
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
}
#pragma endregion

#pragma region Callbacks
void TreeView::clickObjCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    sButtonCB* bcbData = static_cast<sButtonCB*>(lv_obj_get_user_data(obj));
    if (bcbData->node->selected)
    {
        bcbData->node->selected = false;
        if (bcbData->tv->selectedNode != nullptr)
        {
            bcbData->tv->selectedNode->selected = false;
        }
        if (bcbData->tv->selectedNode->object != nullptr)
        {
            lv_obj_set_style(bcbData->tv->selectedNode->object,
                             bcbData->tv->selectedNode->objectStyle);
        }
        bcbData->tv->selectedNode = nullptr;
        lv_obj_set_style(obj, &lv_style_transp);
        lv_obj_set_hidden(bcbData->tv->deleteButton, true);
        lv_obj_set_hidden(bcbData->tv->copyButton, true);
        lv_obj_set_hidden(bcbData->tv->moveUpButton, true);
        lv_obj_set_hidden(bcbData->tv->moveDownButton, true);
    }
    else
    {
        bcbData->node->selected = true;
        if (bcbData->node->object != nullptr)
        {
            bcbData->node->objectStyle = (lv_style_t*)lv_obj_get_style(bcbData->node->object);
            lv_obj_set_style(bcbData->node->object, &lv_style_plain);
        }

        if (bcbData->tv->selectedNode != nullptr)
        {
            if (bcbData->tv->selectedNode->object != nullptr &&
                bcbData->tv->selectedNode->objectStyle != nullptr)
            {
                lv_obj_set_style(bcbData->tv->selectedNode->object, bcbData->tv->selectedNode->objectStyle);
            }
            bcbData->tv->selectedNode->selected = false;
            lv_obj_set_style(bcbData->tv->selectedNode->clickObj, &lv_style_transp);
        }
        bcbData->tv->selectedNode = bcbData->node;
        lv_obj_set_style(obj, &lv_style_plain_color);
        if (bcbData->tv->displayedNodes[0] == bcbData->node)
        {
            lv_obj_set_hidden(bcbData->tv->moveUpButton, true);
        }
        else
        {
            if (!bcbData->node->protectedNode)
            {
                lv_obj_set_hidden(bcbData->tv->moveUpButton, false);
            }
        }
        if (bcbData->tv->displayedNodes[bcbData->tv->displayedNodes.size() - 1] == bcbData->node)
        {
            lv_obj_set_hidden(bcbData->tv->moveDownButton, true);
        }
        else
        {
            if (!bcbData->node->protectedNode)
            {
                lv_obj_set_hidden(bcbData->tv->moveDownButton, false);
            }
        }
        if (!bcbData->node->protectedNode)
        {
            lv_obj_set_hidden(bcbData->tv->deleteButton, false);
        }
        lv_obj_set_hidden(bcbData->tv->copyButton, false);
        if (bcbData->tv->copiedNode != nullptr)
        {
            lv_obj_set_hidden(bcbData->tv->pasteButton, false);
        }
    }
}

void TreeView::expandButtonCB(lv_obj_t* obj, lv_event_t ev)
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

void TreeView::moveUpButtonCB(lv_obj_t* obj, lv_event_t ev)
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
    lv_obj_set_hidden(tv->moveDownButton, false);
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

void TreeView::moveDownButtonCB(lv_obj_t* obj, lv_event_t ev)
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
    lv_obj_set_hidden(tv->moveUpButton, false);
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

void TreeView::deleteButtonCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if (tv->selectedNode == nullptr)
        return;
    if (tv->selectedNode->protectedNode)
        return;
    if (tv->selectedNode->parent == nullptr)
    {
        // It could be a top level node
        int nodeIdx = -1;
        for (int i = 0; i < tv->topLevelNodes.size(); i++)
        {
            if (tv->topLevelNodes[i] == tv->selectedNode)
            {
                tv->topLevelNodes[i]->removeChild(tv->selectedNode);
                break;
            }
        }
    }
    else
    {
        tv->selectedNode->parent->removeChild(tv->selectedNode);
    }
    // Now delete the node itself
    delete(tv->selectedNode);
    tv->selectedNode = nullptr;
    lv_obj_set_hidden(tv->deleteButton, true);
    lv_obj_set_hidden(tv->moveUpButton, true);
    lv_obj_set_hidden(tv->moveDownButton, true);
    lv_obj_set_hidden(tv->copyButton, true);
    lv_obj_set_hidden(tv->pasteButton, true);
}

void TreeView::copyButtonCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if (tv->selectedNode == nullptr)
        return;
    tv->copiedNode = tv->selectedNode;
    lv_obj_set_hidden(tv->pasteButton, false);
}

void TreeView::pasteButtonCB(lv_obj_t* obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
    TreeView* tv = static_cast<TreeView*>(lv_obj_get_user_data(obj));
    if (tv->selectedNode == nullptr)
        return;
    if (tv->copiedNode == nullptr)
    {
        lv_obj_set_hidden(tv->pasteButton, true);
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
