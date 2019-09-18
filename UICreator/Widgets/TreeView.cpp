#include "TreeView.h"

TreeView::TreeView(unsigned int x, unsigned int y, unsigned int width, unsigned int height, std::string title, lv_obj_t* parent) :
	x(x),
	y(y),
	width(width),
	height(height),
	title(title)
{
	if (parent == nullptr)
		window = lv_win_create(lv_scr_act(), nullptr);
	else
		window = lv_win_create(parent, nullptr);
	lv_win_set_title(window, title.c_str());
	lv_obj_set_size(window, width, height);
	lv_obj_set_pos(window, x, y);
	
	deleteButton=lv_win_add_btn(window, LV_SYMBOL_TRASH);
	lv_obj_set_event_cb(deleteButton, deleteButtonCB);
	lv_obj_set_click(deleteButton, true);
	lv_obj_set_hidden(deleteButton, true);
	treeContainer = lv_cont_create(window,nullptr);		
	lv_cont_set_fit(treeContainer, LV_FIT_FLOOD);
}

TreeView::~TreeView()
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

unsigned int TreeView::GetNodeID(std::string name, lv_obj_t* object)
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
		TreeNode *n=getNode(*it, nodeID);
		if (n != nullptr)
			return n;
	}

	return nullptr;
}

unsigned int TreeView::findNode(TreeNode* node, std::string name, lv_obj_t* object)
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
unsigned int TreeView::AddNode(std::string name, lv_obj_t* object, unsigned int parentID)
{
	TreeNode *tn = new TreeNode(name, object);
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

std::vector<TreeNode*> TreeView::GetAllNodes()
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

unsigned int TreeView::GetNodeLevel(TreeNode *node)
{
	unsigned int level = 0;
	while (node->parent != nullptr)
	{
		level++;
		node = node->parent;
	}
	return level;
}

void TreeView::drawNode(TreeNode *node)
{
	int level = node->GetLevel();

	if (node->selected && node->lvObjects.size() >= 3)
	{
		lv_obj_t *selectionBar = node->lvObjects[0];
		if (selectionBar != nullptr)
		{
			lv_obj_set_pos(selectionBar, 0, yCtr * 17 - 1);
			lv_obj_set_style(selectionBar, &lv_style_plain);
		}
		static lv_style_t label_style;
		lv_style_copy(&label_style, &lv_style_plain);
		lv_color_t black = lv_color_make(0, 0, 0);
		
		label_style.text.color = black;
		lv_obj_t* label = node->lvObjects[2];
		lv_label_set_style(label, LV_LABEL_STYLE_MAIN, &label_style);
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
				lv_obj_set_style(selectionBar, &lv_style_transp);				
			}
			static lv_style_t label_style;
			lv_style_copy(&label_style, &lv_style_plain);
			lv_color_t black = lv_color_make(0, 0, 0);
			label_style.text.color = black;
			lv_obj_t* label = node->lvObjects[2];
			lv_label_set_style(label, LV_LABEL_STYLE_MAIN, &lv_style_pretty_color);
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
	lv_obj_set_style(selectionBar, &lv_style_transp);
	lv_obj_set_click(selectionBar, true);

	lv_obj_t* label;	
	label = lv_label_create(treeContainer, nullptr);
	lv_obj_set_pos(label, 30+(level*15), yCtr*17);	
	lv_label_set_text(label, node->name.c_str());	
	
	lv_obj_t* expandButton;
	expandButton = lv_btn_create(treeContainer, nullptr);
	lv_btn_set_fit(expandButton, LV_FIT_NONE);
	lv_obj_set_size(expandButton, 25, 15);
	lv_obj_set_pos(expandButton, 4+(level*15), yCtr*17);

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
		lv_btn_set_toggle(btnLbl, false);
	}
	if (node->expanded)
	{
		lv_label_set_text(btnLbl, LV_SYMBOL_DOWN);
	}
	else
	{
		lv_label_set_text(btnLbl, LV_SYMBOL_RIGHT);
	}
	node->lvObjects.push_back(selectionBar);
	node->lvObjects.push_back(expandButton);
	node->lvObjects.push_back(label);	
}

void TreeView::createObjects(TreeNode *node)
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

void TreeView::createObjects()
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

void TreeView::deleteButtonCB(lv_obj_t * obj, lv_event_t ev)
{

}

void TreeView::labelButtonCB(lv_obj_t *obj, lv_event_t ev)
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
		expBtn->tv->selNode = expBtn->node;		
		lv_obj_set_hidden(expBtn->tv->deleteButton, true);
	}
	else
	{
		expBtn->node->selected = true;
		if (expBtn->tv->selNode != nullptr)
			expBtn->tv->selNode->selected = false;
		expBtn->node->selected = true;
		expBtn->tv->selectedNode = expBtn->node->id;
		expBtn->tv->selNode = expBtn->node;
		expBtn->tv->createObjects();
		lv_obj_set_hidden(expBtn->tv->deleteButton, false);
	}
	expBtn->tv->createObjects();
}

void TreeView::expandButtonCB(lv_obj_t * obj, lv_event_t ev)
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