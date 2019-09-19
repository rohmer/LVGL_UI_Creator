#pragma once
#include <any>
#include <string>
#include <vector>
#include "lvgl.h"

class TreeNodeV1
{
public:	
    ~TreeNodeV1();
	bool operator==(const TreeNodeV1 &other);
	TreeNodeV1 *DeepCopy();
	lv_obj_t *GetObject();
	unsigned int GetID();
	std::string GetName();
	unsigned int GetLevel();	
	std::vector<TreeNodeV1*> GetChildren();
	bool GetSelected();
	void SetNodeData(std::any nodeData);
	std::any GetNodeData();
    void RemoveChild(TreeNodeV1 *node);

protected:
	friend class TreeView;
	void setID(unsigned int tnid);
	void setPosition(int x, int y);
	TreeNodeV1(std::string name, lv_obj_t *object, bool protect=false);
	void addChild(TreeNodeV1 *childNode);
	std::vector<lv_obj_t*> lvObjects;
	
private:
	std::vector<TreeNodeV1 *> children;
	unsigned int id;
	unsigned int level;
	lv_obj_t *object;
	std::string name;
	int x, y;
	TreeNodeV1* parent;
	bool expanded = false;
	bool selected = false;
	bool protectedNode = false;
	std::any nodeData;
};