#pragma once
#include <string>
#include <vector>
#include "lvgl.h"

class TreeNode 
{
public:	

	bool operator=(const TreeNode &other);
	lv_obj_t *GetObject();
	unsigned int GetID();
	std::string GetName();
	unsigned int GetLevel();	
	std::vector<TreeNode*> GetChildren();
	
protected:
	friend class TreeView;
	void setID(unsigned int tnid);
	void setPosition(int x, int y);
	TreeNode(std::string name, lv_obj_t *object);
	void addChild(TreeNode *childNode);
	std::vector<lv_obj_t*> lvObjects;

private:
	std::vector<TreeNode *> children;
	unsigned int id;
	unsigned int level;
	lv_obj_t *object;
	std::string name;
	int x, y;
	TreeNode* parent;
	bool expanded = false;
	bool selected = false;
};