#pragma once
#include <any>
#include <string>
#include <vector>
#include "lvgl.h"

class TreeNode
{
public:
	~TreeNode();
	bool operator==(const TreeNode &other);
	TreeNode *DeepCopy();
	lv_obj_t *GetObject();
	unsigned int GetID();
	std::string GetName();
	unsigned int GetLevel();
	bool IsSelected();
	void SetNodeData(std::any nodeData);
	std::any GetNodeData();

private:
	friend class TreeView;
	void setID(unsigned int tnid);
	void setPosition(int x, int y);
	TreeNode(std::string name, TreeNode *parent, lv_obj_t *object, bool protect = false);
	void addChild(TreeNode *childNode);
	void removeChild(TreeNode *node);
	lv_obj_t *object, *buttonObj, *buttonLabel, *labelObj, *clickObj;
	
private:
	TreeNode *parent;
	std::vector<TreeNode *> children;
	unsigned int id;
	unsigned int level;
	std::string name;
	int x, y;
	bool expanded = false;
	bool selected = false;
	bool protectedNode = false;
	std::any nodeData;
};