#include "TreeView.h"
TreeView::TreeView(unsigned int x,
	unsigned int y,
	unsigned int width,
	unsigned int height,
	std::string title,
	bool inWindow,
	lv_obj_t* parent) :
	x(x),
	y(y),
	width(width),
	height(height),
	title(title)
{
	createTreeViewContainer(inWindow, parent);
}

lv_obj_t *TreeView::GetTreeViewUI()
{
	return window;
}

TreeNode *TreeView::GetSelectedNode()
{
	return selectedNode;
}

TreeNode *TreeView::getNode(TreeNode* node, unsigned int nodeID)
{
	if (node->id == nodeID)
		return node;
	for (std::vector<TreeNode*>::iterator it = node->children.begin();
		it != node->children.end();
		it++)
	{
		if ((*it)->id == nodeID)
			return *it;
		TreeNode *n = getNode(*it, nodeID);
		if (n != nullptr)
			return n;
	}

	return nullptr;
}

unsigned int TreeView::AddNode(std::string name,
	lv_obj_t* object,
	unsigned int parentID,
	bool protect)
{
	// First we need to get the node pointer for the parent
	std::vector<TreeNode*> nodes = getAllNodes();
	TreeNode *parent = nullptr;
	for(int i=0; i<nodes.size(); i++)
	{
		if(nodes[i]->id==parentID)
		{
			parent = nodes[i];
			break;
		}
	}

	TreeNode *newNode = new TreeNode(name, parent, object, protect);
	newNode->expanded = true;
	newNode->id = curID;
	curID++;
	if (parent == nullptr)
	{
		topLevelNodes.push_back(newNode);
	} else
	{
		parent->addChild(newNode);
	}
	newNode->level = newNode->GetLevel();
	createUIObjects(newNode);
	placeUIObjects();
	return newNode->id;
}

std::vector<TreeNode*> TreeView::getAllNodes(TreeNode* node)
{
	std::vector<TreeNode*> retVal;
	for (std::vector<TreeNode*>::iterator it = node->children.begin();
		it != node->children.end();
		it++)
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
		it++)
	{
		retVal.push_back(*it);
		std::vector<TreeNode*> children = getAllNodes(*it);
		retVal.insert(retVal.end(), children.begin(), children.end());
	}
	return retVal;
}

void TreeView::createTreeViewContainer(bool inWin, lv_obj_t *parent)
{
	if(inWin)
	{
		if (parent == nullptr)
			window = lv_win_create(lv_scr_act(), nullptr);
		else
			window = lv_win_create(parent, nullptr);
		lv_win_set_title(window, title.c_str());
		deleteButton = lv_win_add_btn(window, LV_SYMBOL_TRASH);
		moveDownButton = lv_win_add_btn(window, LV_SYMBOL_UP);
		copyButton = lv_win_add_btn(window, LV_SYMBOL_COPY);
		pasteButton = lv_win_add_btn(window, LV_SYMBOL_FILE);
		moveUpButton = lv_win_add_btn(window, LV_SYMBOL_UP);
		lv_obj_set_user_data(deleteButton, (lv_obj_user_data_t)this);
		lv_obj_set_event_cb(deleteButton, deleteButtonCB);
		lv_obj_set_click(deleteButton, true);
		lv_obj_set_hidden(deleteButton, true);
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
		lv_obj_t *dbLabel = lv_label_create(deleteButton, nullptr);
		lv_obj_set_size(deleteButton, 35, 35);
		lv_label_set_text(dbLabel, LV_SYMBOL_TRASH);
		yOffset = 40;
		lv_obj_set_hidden(deleteButton, true);
		moveDownButton = lv_btn_create(window, nullptr);
		lv_obj_set_pos(moveDownButton, width - 80, 2);
		lv_obj_set_size(moveDownButton, 35, 35);
		lv_obj_set_hidden(moveDownButton, true);
		lv_obj_t *mdLbl = lv_label_create(moveDownButton, nullptr);
		lv_label_set_text(mdLbl, LV_SYMBOL_DOWN);
		copyButton = lv_btn_create(window, nullptr);
		lv_obj_set_size(copyButton, 35, 35);
		lv_obj_set_pos(copyButton, width - 120, 2);
		lv_obj_set_hidden(copyButton, true);
		lv_obj_t *cpLbl = lv_label_create(copyButton, nullptr);
		lv_label_set_text(cpLbl, LV_SYMBOL_COPY);
		pasteButton = lv_btn_create(window, nullptr);
		lv_obj_set_size(pasteButton, 35, 35);
		lv_obj_set_pos(pasteButton, width - 160, 2);
		lv_obj_set_hidden(pasteButton, true);
		lv_obj_t *pLbl = lv_label_create(pasteButton, nullptr);
		lv_label_set_text(pLbl, LV_SYMBOL_FILE);
		moveUpButton = lv_btn_create(window, nullptr);
		lv_obj_set_size(moveUpButton, 35, 35);
		lv_obj_set_pos(moveUpButton, width - 200, 2);
		lv_obj_set_hidden(moveUpButton, true);
		lv_obj_t *muLbl = lv_label_create(moveUpButton, nullptr);
		lv_label_set_text(muLbl, LV_SYMBOL_UP);
		lv_obj_t *titleLabel = lv_label_create(window, nullptr);
		lv_obj_set_pos(titleLabel, 5, 5);
		lv_label_set_text(titleLabel, title.c_str());
	}
	treeContainer = lv_cont_create(window, nullptr);
	lv_obj_set_y(treeContainer, yOffset);
	lv_obj_set_size(treeContainer, width - 5, height - yOffset);

	lv_obj_set_user_data(deleteButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(deleteButton, deleteButtonCB);
	lv_obj_set_user_data(moveUpButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(moveUpButton, moveUpButtonCB);
	lv_obj_set_user_data(moveDownButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(moveDownButton, moveDownButtonCB);
	lv_obj_set_user_data(copyButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(copyButton, copyButtonCB);
	lv_obj_set_user_data(pasteButton, (lv_obj_user_data_t)this);
	lv_obj_set_event_cb(pasteButton, pasteButtonCB);
}

void TreeView::createUIObjects(TreeNode *node)
{
	sButtonCB *bcb = new sButtonCB();
	bcb->node = node;
	// All the objects are created with the click object as the parent
	if (node->clickObj == nullptr)
	{
		node->clickObj = lv_obj_create(treeContainer, nullptr);
		lv_obj_set_size(node->clickObj, width - 5, 17);
		lv_obj_set_style(node->clickObj, &lv_style_transp);
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
	if(node->buttonLabel==nullptr)
	{
		node->buttonLabel = lv_label_create(node->buttonObj, nullptr);
		bcb->label = node->buttonLabel;
	}
	if(node->labelObj==nullptr)
	{
		node->labelObj = lv_label_create(treeContainer, nullptr);
		lv_label_set_text(node->labelObj, node->name.c_str());
		lv_label_set_style(node->labelObj, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
	}
	bcb->tv = this;
	lv_obj_set_user_data(node->buttonObj, (lv_obj_user_data_t)bcb);
	lv_obj_set_event_cb(node->buttonObj, expandButtonCB);
}

int TreeView::placeUIObjects(TreeNode* node, int yCtr, bool shown)
{
	node->setPosition(x + 5, yCtr * 17 - 1);
	if(shown)
	{
		yCtr++;
		lv_label_set_style(node->labelObj, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
		lv_obj_set_style(node->buttonLabel, &lv_style_pretty_color);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_REL, &lv_style_pretty_color);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_INA, &lv_style_pretty_color);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_PR, &lv_style_pretty_color);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_PR, &lv_style_pretty_color);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_REL, &lv_style_pretty_color);
	} else
	{
		lv_label_set_style(node->labelObj, LV_LABEL_STYLE_MAIN, &lv_style_transp);
		lv_obj_set_style(node->buttonLabel, &lv_style_transp);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_REL, &lv_style_transp);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_INA, &lv_style_transp);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_PR, &lv_style_transp);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_PR, &lv_style_transp);
		lv_btn_set_style(node->buttonObj, LV_BTN_STYLE_TGL_REL, &lv_style_transp);		
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
			if (shown&&node->expanded)
				showChild = true;
			yCtr = placeUIObjects(*cit, yCtr, showChild);
		}
	}
	return yCtr;
}

void TreeView::placeUIObjects()
{
	int yCtr = 0;
	for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
		it != topLevelNodes.end();
		it++)
	{
		yCtr++;
		(*it)->setPosition(x+5, yCtr * 17 - 1);
		if((*it)->children.empty())
		{
			lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_MINUS);
		} else
		{
			if((*it)->expanded)
			{
				lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_DOWN);
				yCtr++;
			} else
			{
				lv_label_set_text((*it)->buttonLabel, LV_SYMBOL_RIGHT);
			}
			for(std::vector<TreeNode*>::iterator cit=(*it)->children.begin();
				cit!=(*it)->children.end();
				++cit)
			{
				yCtr = placeUIObjects(*cit, yCtr, (*it)->expanded);
			}
		}
	}
}

#pragma region Callbacks
void TreeView::deleteButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
}

void TreeView::labelButtonCB(lv_obj_t *obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
}

void TreeView::expandButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	sButtonCB *buttonCB = (sButtonCB*)lv_obj_get_user_data(obj);
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

void TreeView::moveUpButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
}

void TreeView::moveDownButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
}

void TreeView::copyButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
}

void TreeView::pasteButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
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