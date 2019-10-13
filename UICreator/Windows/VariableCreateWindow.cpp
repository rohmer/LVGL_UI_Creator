#include "VariableCreateWindow.h"
#include "Serialization/LVLabel.h"

VariableCreateWindow::VariableCreateWindow(ToolBar* toolBar) :
	toolbar(toolBar)
{
	treeView = new TreeView(
		200,
		200,
		lv_obj_get_width(lv_scr_act()) - 400,
		lv_obj_get_height(lv_scr_act()) - 400,
		"Properties",
		false,
        false);
	createBaseProperties();
}

unsigned int VariableCreateWindow::parseNode(
	std::string nodeName,
	const json &node, 
	unsigned int parent)
{
	int  id=treeView->AddNode(nodeName, nullptr, parent, true);
	for (auto it = node.begin(); it != node.end(); ++it)
	{
		if(it->is_structured())
		{
			id=parseNode(it.key(), *it, id);
		} else
		{
			treeView->AddNode(it.key(), nullptr, id, true);
		}
	}
	return id;
}

void VariableCreateWindow::createBaseProperties()
{
	// create an object and then parse it
	lv_obj_t *obj = lv_label_create(lv_scr_act(), nullptr);
	json j = Serialization::LVLabel::ToJSON(obj);
	json base = j["base"];

	int baseID=parseNode("Base", j["base"],0);
	baseObjProps = treeView->GetNodeByID(0);
	treeView->ClearNodes();
	baseObjProps->SetExpanded(false);
}