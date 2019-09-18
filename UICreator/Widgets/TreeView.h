#pragma once
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "TreeNode.h"

class TreeView 
{
	typedef void(*tv_select_callback)(TreeNode *nodeSelected);
	
public:
	TreeView(unsigned int x,
		unsigned int y, 
		unsigned int width, 
		unsigned int height, 
		std::string title,
		bool inWindow=false,
		lv_obj_t* parent=nullptr);	
	~TreeView();
	void SetDrawTree(bool drawTree);

	unsigned int AddNode(std::string name, lv_obj_t* object, 
		unsigned int parentID=0, 
		bool protect=false);
	unsigned int GetNodeID(std::string name, lv_obj_t* object);
	std::vector<TreeNode*> GetAllNodes();
	unsigned int GetNodeLevel(TreeNode* node);
	lv_obj_t* GetBaseObject();
	TreeNode* FindNodeByID(unsigned int id);
	void AddSelectCallback(tv_select_callback cbMethod);
	
private:
	unsigned int curID = 1;
	std::string title;
	unsigned int x, y, width, height;
	std::vector<TreeNode*> topLevelNodes;
	lv_obj_t *window, *deleteButton, *treeContainer;
	static void deleteButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void expandButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void labelButtonCB(lv_obj_t * obj, lv_event_t ev);
	unsigned int findNode(TreeNode* node, std::string name, lv_obj_t* object);	
	TreeNode *getNode(TreeNode* node, unsigned int nodeID);
	std::vector<TreeNode*> getAllNodes();
	std::vector<TreeNode*> getAllNodes(TreeNode *node);	
	int yCtr = 0;
	void createObjects();
	void createObjects(TreeNode *node);

	void drawNode(TreeNode *tn);

	std::map<std::string, lv_obj_t*> drawObjects;
	int selectedNode = -1;
	TreeNode *selNode = nullptr;
	struct sExpButton
	{
		TreeNode *node;
		lv_obj_t *label;
		TreeView *tv;
	};
	bool dTree = false;
	tv_select_callback selectCallbackFunc = nullptr;
};