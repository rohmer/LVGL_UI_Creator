#include "TreeViewV1.h"

TreeViewV1::TreeViewV1(unsigned int x, 
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
	int treeContPush = 0;
	if (inWindow)
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
	} else
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
		treeContPush = 40;
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
		lv_obj_t *pLbl= lv_label_create(pasteButton, nullptr);
		lv_label_set_text(pLbl, LV_SYMBOL_FILE);
		moveUpButton = lv_btn_create(window, nullptr);
		lv_obj_set_size(moveUpButton, 35, 35);
		lv_obj_set_pos(moveUpButton, width - 200, 2);
		lv_obj_set_hidden(moveUpButton, true);
		lv_obj_t *muLbl  = lv_label_create(moveUpButton, nullptr);
		lv_label_set_text(muLbl, LV_SYMBOL_UP);
		lv_obj_t *titleLabel = lv_label_create(window, nullptr);
		lv_obj_set_pos(titleLabel, 5, 5);
		lv_label_set_text(titleLabel, title.c_str());
	}
	treeContainer = lv_cont_create(window,nullptr);		
	lv_obj_set_y(treeContainer, treeContPush);
	lv_obj_set_size(treeContainer, width-5, height - treeContPush);

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
	//	lv_cont_set_fit(treeContainer, LV_FIT_FLOOD);
}

TreeViewV1::~TreeViewV1()
{
	std::vector<TreeNode*> nodes = getAllNodes();
	for (std::vector<TreeNode*>::iterator it = nodes.begin();
		it != nodes.end();
		it++)
	{
		if (*it != nullptr)
			delete(*it);
	}
}

lv_obj_t* TreeViewV1::GetBaseObject()
{
	return window;
}

unsigned int TreeViewV1::GetNodeID(std::string name, lv_obj_t* object)
{	
	for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
		it != topLevelNodes.end();
		it++)
	{
		unsigned int r = findNode(*it, name, object);
		if (r != 0)
			return r;
	}
	return 0;
}

TreeNode *TreeViewV1::getNode(TreeNode* node, unsigned int nodeID)
{
	if (node->id == nodeID)
		return node;
	for (std::vector<TreeNode*>::iterator it = node->children.begin();
		it != node->children.end();
		it++)
	{
		if ((*it)->id == nodeID)
			return *it;
		TreeNode *n=getNode(*it, nodeID);
		if (n != nullptr)
			return n;
	}

	return nullptr;
}

unsigned int TreeViewV1::findNode(TreeNode* node, std::string name, lv_obj_t* object)
{
	for (std::vector<TreeNode*>::iterator it = node->children.begin();
		it != node->children.end();
		it++)
	{
		if (*it == node)
			return (*it)->GetID();
		return findNode(*it, name, object);
	}
	return 0;
}

/*
	parentID: the id of the parent in the tree, 0=top level node
*/
unsigned int TreeViewV1::AddNode(std::string name, 
	lv_obj_t* object, 
	unsigned int parentID,
	bool protect)
{
	TreeNode *tn = new TreeNode(name, object, protect);
	tn->setID(curID);
	curID++;
	if (parentID == 0)
	{
		topLevelNodes.push_back(tn);
		tn->parent = nullptr;
		tn->level = 0;
		createObjects();
		return tn->GetID();
	}
	
	for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
		it != topLevelNodes.end();
		it++)
	{
		TreeNode *parent = getNode(*it, parentID);
		if (parent != nullptr)
		{
			if (parent->GetID() == parentID)
			{
				parent->addChild(tn);
				tn->parent = parent;
				tn->level = GetNodeLevel(tn);
				createObjects();
				return tn->GetID();
			}
		}		
	}

	return 0;
}

std::vector<TreeNode*> TreeViewV1::getAllNodes(TreeNode* node)
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

std::vector<TreeNode*> TreeViewV1::getAllNodes()
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

std::vector<TreeNode*> TreeViewV1::GetChildren(TreeNode *treeNode)
{
    std::vector<TreeNode*> retVal;
    for (std::vector<TreeNode*>::iterator it = treeNode->children.begin();
        it != treeNode->children.end();
        ++it)
    {
        std::vector<TreeNode*> children = getAllNodes(*it);
        retVal.insert(retVal.end(), children.begin(), children.end());
    }
    return retVal;
}

std::vector<TreeNode*> TreeViewV1::GetAllNodes()
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

TreeNode* TreeViewV1::FindNodeByID(unsigned int id)
{
	std::vector<TreeNode*> nodes = GetAllNodes();
	for(std::vector<TreeNode*>::iterator it=nodes.begin();
		it!=nodes.end();
		++it)
	{
		if ((*it)->id == id)
			return (*it);
	}
	return nullptr;
}

unsigned int TreeViewV1::GetNodeLevel(TreeNode *node)
{
	unsigned int level = 0;
	while (node->parent != nullptr)
	{
		level++;
		node = node->parent;
	}
	return level;
}

void TreeViewV1::drawNode(TreeNode *node)
{
	int level = node->GetLevel();

	// Set the drop down button if it has children, or not
	if(node->lvObjects.size()>=3)
	{
		lv_obj_t *buttonLabel = lv_obj_get_child(node->lvObjects[2],nullptr);
		bool labelHasChild = true;
		char *bTxt = lv_label_get_text(buttonLabel);
		if (strcmp(lv_label_get_text(buttonLabel), LV_SYMBOL_MINUS) == 0)
			labelHasChild = false;
		if(node->children.empty() && labelHasChild)
		{
			lv_label_set_text(buttonLabel, LV_SYMBOL_MINUS);
		}
		if(!node->children.empty() && !labelHasChild)
		{
			lv_label_set_text(buttonLabel, LV_SYMBOL_RIGHT);
		}
	}
	if (node->selected && node->lvObjects.size() >= 3)
	{
		lv_obj_t *selectionBar = node->lvObjects[0];
		if (selectionBar != nullptr)
		{
			lv_obj_set_pos(selectionBar, 0, yCtr * 17 - 1);
			selectionBar->style_p = &lv_style_pretty_color;
			lv_obj_refresh_style(selectionBar);
		}
		
		for (int i=0; i<node->lvObjects.size()-1; i++)
			lv_obj_set_hidden(node->lvObjects[i], false);		
		return;
	}
	else
	{
		if (node->lvObjects.size() >= 3)
		{
			lv_obj_t *selectionBar = node->lvObjects[0];
			if (selectionBar != nullptr)
			{
				lv_obj_set_pos(selectionBar, 0, yCtr * 17 - 1);
				selectionBar->style_p = &lv_style_transp;
				lv_obj_refresh_style(selectionBar);
			}
			
			//lv_label_set_style(label, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
			for (std::vector<lv_obj_t*>::iterator it = node->lvObjects.begin();
				it != node->lvObjects.end();
				it++)
				lv_obj_set_hidden(*it, false);
			

			return;
		} 
	}
	lv_obj_t *selectionBar = lv_obj_create(treeContainer, nullptr);
	lv_obj_set_size(selectionBar, width - 5, 17);	
	lv_obj_set_pos(selectionBar, 0, yCtr * 17);	
	if (node->selected)
		lv_obj_set_style(selectionBar, &lv_style_pretty_color);
	else
		lv_obj_set_style(selectionBar, &lv_style_transp);
	lv_obj_set_click(selectionBar, true);

	lv_obj_t* label;	
	label = lv_label_create(treeContainer, nullptr);
	lv_obj_set_pos(label, 30+(level*15), yCtr*17);	
	lv_label_set_text(label, node->name.c_str());
	lv_label_set_style(label, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
	
	lv_obj_t* expandButton;
	expandButton = lv_btn_create(treeContainer, nullptr);
	lv_btn_set_fit(expandButton, LV_FIT_NONE);
	lv_obj_set_size(expandButton, 25, 15);
	lv_obj_set_pos(expandButton, 4+(level*15), yCtr*17);
	lv_btn_set_style(expandButton, LV_BTN_STYLE_REL, &lv_style_pretty_color);
	lv_btn_set_style(expandButton, LV_BTN_STYLE_INA, &lv_style_pretty_color);
	lv_btn_set_style(expandButton, LV_BTN_STYLE_PR, &lv_style_pretty_color);
	lv_btn_set_style(expandButton, LV_BTN_STYLE_TGL_PR, &lv_style_pretty_color);
	lv_btn_set_style(expandButton, LV_BTN_STYLE_TGL_REL, &lv_style_pretty_color);
	lv_obj_t* btnLbl;
	btnLbl = lv_label_create(expandButton, nullptr);
	sExpButton *bObj = new sExpButton();
	bObj->label = btnLbl;
	bObj->node = node;
	bObj->tv = this;
	lv_obj_set_user_data(expandButton, (lv_obj_user_data_t)bObj);
	lv_obj_set_event_cb(expandButton, expandButtonCB);
	
	lv_obj_set_user_data(selectionBar, (lv_obj_user_data_t)bObj);
	lv_obj_set_event_cb(selectionBar, labelButtonCB);

	if (node->children.size() == 0)
	{
		lv_label_set_text(btnLbl, LV_SYMBOL_MINUS);
	}
	else
	{
		if (node->expanded)
		{
			lv_label_set_text(btnLbl, LV_SYMBOL_DOWN);
		}
		else
		{
			lv_label_set_text(btnLbl, LV_SYMBOL_RIGHT);
		}
	}
	node->lvObjects.push_back(selectionBar);
	node->lvObjects.push_back(label);
	node->lvObjects.push_back(expandButton);
}

void TreeViewV1::createObjects(TreeNode *node)
{
	for(std::vector<TreeNode*>::iterator it=node->children.begin(); 
		it!=node->children.end();
		it++)
	{
		yCtr++;
		drawNode(*it);
		if ((*it)->expanded && (*it)->children.size()>0)
		{
			createObjects(*it);
		}
		else
		{
			std::vector<TreeNode*> cnodes = getAllNodes(*it);
			for (std::vector<TreeNode*>::iterator ni = cnodes.begin();
				ni != cnodes.end();
				ni++)
			{
				for (std::vector<lv_obj_t*>::iterator lvi = (*ni)->lvObjects.begin();
					lvi != (*ni)->lvObjects.end();
					lvi++)
				{
					lv_obj_set_hidden(*lvi, true);
				}
			}
		}

	}
}

void TreeViewV1::createObjects()
{			
	yCtr = 0;
	
	for (std::vector<TreeNode*>::iterator it = topLevelNodes.begin();
		it != topLevelNodes.end();
		it++)
	{
		yCtr++;
		drawNode(*it);
		if ((*it)->expanded && (*it)->children.size()>0)
		{
			createObjects(*it);
		}
		else
		{
			std::vector<TreeNode*> cnodes = getAllNodes(*it);
			for (std::vector<TreeNode*>::iterator ni = cnodes.begin();
				ni != cnodes.end();
				ni++)
			{
				for (std::vector<lv_obj_t*>::iterator lvi = (*ni)->lvObjects.begin();
					lvi != (*ni)->lvObjects.end();
					lvi++)
				{
					lv_obj_set_hidden(*lvi, true);
				}
			}
		}
	}

	
	lv_obj_invalidate(treeContainer);
}

void TreeViewV1::deleteButtonCB(lv_obj_t * obj, lv_event_t ev)
{
    if (ev != LV_EVENT_CLICKED)
        return;
	TreeViewV1 *tv = (TreeViewV1*)lv_obj_get_user_data(obj);
	// First we process any call back to clean up data
	if(tv->deleteCallbackFunc!=nullptr)
	{
		tv->deleteCallbackFunc(tv->selNode);
	}

	TreeNode *tNode = tv->GetSelectedObject();
	// Then delete the node
    TreeNode *parent = tNode->parent;    
    if (parent == nullptr)
        return;
    parent->RemoveChild(tNode);
    delete(tNode);
    tv->createObjects();
    lv_obj_invalidate(tv->treeContainer);
}

void TreeViewV1::AddSelectCallback(tv_select_callback cbMethod)
{
	this->selectCallbackFunc = cbMethod;
}

void TreeViewV1::AddDeleteCallback(tv_select_callback cbMethod)
{
	this->deleteCallbackFunc = cbMethod;
}

void TreeViewV1::labelButtonCB(lv_obj_t *obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	sExpButton *expBtn = (sExpButton*)lv_obj_get_user_data(obj);
	if (expBtn->node->selected)
	{	
		expBtn->node->selected = false;
		if (expBtn->tv->selNode != nullptr)
			expBtn->tv->selNode->selected = false;
		expBtn->node->selected = false;
		expBtn->tv->selectedNode = expBtn->node->id;
		if (expBtn->tv->selectCallbackFunc != nullptr)
		{
			// Call once for previous node
			if(expBtn->tv->selNode!=nullptr)
				expBtn->tv->selectCallbackFunc(expBtn->tv->selNode);
			// Once for this node
			expBtn->tv->selectCallbackFunc(expBtn->node);
		}
		
		expBtn->tv->selNode = expBtn->node;		
		lv_obj_set_hidden(expBtn->tv->deleteButton, true);
		lv_obj_set_hidden(expBtn->tv->moveUpButton, true);
		lv_obj_set_hidden(expBtn->tv->moveDownButton, true);
		lv_obj_set_hidden(expBtn->tv->copyButton, true);
		lv_obj_set_hidden(expBtn->tv->pasteButton, true);

	}
	else
	{
		expBtn->node->selected = true;
		if (expBtn->tv->selNode != nullptr)
			expBtn->tv->selNode->selected = false;
		expBtn->node->selected = true;
		expBtn->tv->selectedNode = expBtn->node->id;
		if (expBtn->tv->selectCallbackFunc != nullptr)
		{
			// Call once for previous node
			if (expBtn->tv->selNode != nullptr)
				expBtn->tv->selectCallbackFunc(expBtn->tv->selNode);
			// Once for this node
			expBtn->tv->selectCallbackFunc(expBtn->node);
		}
		expBtn->tv->selNode = expBtn->node;
		expBtn->tv->createObjects();
		if (!expBtn->node->protectedNode)
		{
			lv_obj_set_hidden(expBtn->tv->deleteButton, false);
			lv_obj_set_hidden(expBtn->tv->moveUpButton, false);
			lv_obj_set_hidden(expBtn->tv->moveDownButton, false);
			lv_obj_set_hidden(expBtn->tv->copyButton, false);
			if(expBtn->tv->copiedNode!=nullptr)
				lv_obj_set_hidden(expBtn->tv->pasteButton, false);
		} else
		{
			lv_obj_set_hidden(expBtn->tv->deleteButton, true);
			lv_obj_set_hidden(expBtn->tv->moveUpButton, true);
			lv_obj_set_hidden(expBtn->tv->moveDownButton, true);
			lv_obj_set_hidden(expBtn->tv->copyButton, true);
			//lv_obj_set_hidden(expBtn->tv->pasteButton, true);
		}

	}
	expBtn->tv->createObjects();
}

void TreeViewV1::expandButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED) 
		return;
	sExpButton *expBtn= (sExpButton*)lv_obj_get_user_data(obj);
	if (expBtn->node->expanded)
	{
		expBtn->node->expanded = false;
		lv_label_set_text(expBtn->label, LV_SYMBOL_RIGHT);
		expBtn->tv->createObjects();		
	}
	else
	{
		expBtn->node->expanded = true;
		lv_label_set_text(expBtn->label, LV_SYMBOL_DOWN);		
		expBtn->tv->createObjects();
	}
}

TreeNode* TreeViewV1::GetSelectedObject()
{
	return selNode;
}

void TreeViewV1::copyButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	TreeViewV1 *tv = (TreeViewV1*)lv_obj_get_user_data(obj);
	if(tv->selNode!=nullptr)
	{
		tv->copiedNode = tv->selNode;
		lv_obj_set_hidden(tv->pasteButton, false);
	}
}

void TreeViewV1::moveUpButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	TreeViewV1 *tv = (TreeViewV1*)lv_obj_get_user_data(obj);
}

void TreeViewV1::moveDownButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	TreeViewV1 *tv = (TreeViewV1*)lv_obj_get_user_data(obj);
	
}

void TreeViewV1::pasteButtonCB(lv_obj_t * obj, lv_event_t ev)
{
	if (ev != LV_EVENT_CLICKED)
		return;
	TreeViewV1 *tv = (TreeViewV1*)lv_obj_get_user_data(obj);

	TreeNode *newNode = tv->copiedNode->DeepCopy();
	newNode->parent = tv->selNode;
	newNode->level = newNode->parent->GetLevel() + 1;
	tv->selNode->addChild(newNode);
	// Lets delete all our objects created up til now, to recreate them
	for(std::vector<TreeNode*>::iterator it=tv->topLevelNodes.begin(); it
		!=tv->topLevelNodes.end(); 
		++it)
	{
		std::vector<TreeNode*> children = (*it)->GetChildren();
		for (std::vector<TreeNode*>::iterator it2 = children.begin();
			it2 != children.end();
			++it2)
		{
			for (int i = 0; i < (*it2)->lvObjects.size(); ++i)
				lv_obj_del_async((*it2)->lvObjects[i]);
		}
	}
	tv->createObjects();
}