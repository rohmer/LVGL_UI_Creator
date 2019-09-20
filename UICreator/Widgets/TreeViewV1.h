#pragma once
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "TreeNodeV1.h"

class TreeViewV1
{
	typedef void(*tv_select_callback)(TreeNodeV1 *nodeSelected);
	
public:
	TreeViewV1(unsigned int x,
		unsigned int y, 
		unsigned int width, 
		unsigned int height, 
		std::string title,
		bool inWindow=false,
		lv_obj_t* parent=nullptr);	
	~TreeViewV1();
	void SetDrawTree(bool drawTree);

	unsigned int AddNode(std::string name, lv_obj_t* object, 
		unsigned int parentID=0, 
		bool protect=false);
	unsigned int GetNodeID(std::string name, lv_obj_t* object);
	std::vector<TreeNodeV1*> GetAllNodes();
	unsigned int GetNodeLevel(TreeNodeV1* node);
	lv_obj_t *GetBaseObject();
	TreeNodeV1 *FindNodeByID(unsigned int id);
	void AddSelectCallback(tv_select_callback cbMethod);
	TreeNodeV1 *GetSelectedObject();
	void AddDeleteCallback(tv_select_callback cbMethod);
    std::vector<TreeNodeV1*> GetChildren(TreeNodeV1 *TreeNodeV1);

private:
	unsigned int curID = 1;
	std::string title;
	unsigned int x, y, width, height;
	std::vector<TreeNodeV1*> topLevelNodes;
	lv_obj_t *window, *deleteButton, *treeContainer,
		*moveUpButton, *moveDownButton, *copyButton, *pasteButton;
	TreeNodeV1 *copiedNode=nullptr;
	static void deleteButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void expandButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void labelButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void moveUpButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void copyButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void pasteButtonCB(lv_obj_t * obj, lv_event_t ev);
	static void moveDownButtonCB(lv_obj_t * obj, lv_event_t ev);
	unsigned int findNode(TreeNodeV1* node, std::string name, lv_obj_t* object);	
	TreeNodeV1 *getNode(TreeNodeV1* node, unsigned int nodeID);
	std::vector<TreeNodeV1*> getAllNodes();
	std::vector<TreeNodeV1*> getAllNodes(TreeNodeV1 *node);	
	int yCtr = 0;
	void createObjects();
	void createObjects(TreeNodeV1 *node);

	void drawNode(TreeNodeV1 *tn);

	std::map<std::string, lv_obj_t*> drawObjects;
	int selectedNode = -1;
	TreeNodeV1 *selNode = nullptr;
	struct sExpButton
	{
		TreeNodeV1 *node;
		lv_obj_t *label;
		TreeViewV1 *tv;
	};
	bool dTree = false;
	tv_select_callback selectCallbackFunc = nullptr;
	tv_select_callback deleteCallbackFunc = nullptr;
};