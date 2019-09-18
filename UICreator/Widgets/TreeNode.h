#pragma once
#include <any>
#include <string>
#include <vector>
#include "lvgl.h"

class TreeNode
{
public:	

	bool operator==(const TreeNode &other);
	lv_obj_t *GetObject();
	unsigned int GetID();
	std::string GetName();
	unsigned int GetLevel();	
	std::vector<TreeNode*> GetChildren();
	bool GetSelected();
	void SetNodeData(std::any nodeData);
	std::any GetNodeData();
	void DeleteNode(TreeNode *node);
	
protected:
	friend class TreeView;
	void setID(unsigned int tnid);
	void setPosition(int x, int y);
	TreeNode(std::string name, lv_obj_t *object, bool protect=false);
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
	bool protectedNode = false;
	std::any nodeData;
};