#define SDL_MAIN_HANDLED
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorSession.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/EditorControls.h"
#include "../../../UICreator/Drivers/SDL3/NativeSDL3.h"
#include "../../../UICreator/Windows/UI.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
lv_obj_t* Find(lv_obj_t* object, const char* text)
{
    if (lv_obj_check_type(object, &lv_label_class) && std::string(lv_label_get_text(object)) == text) return object;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        if (auto* found = Find(lv_obj_get_child(object, i), text)) return found;
    return nullptr;
}
lv_obj_t* Field(lv_obj_t* object)
{
    if (lv_obj_check_type(object, &lv_textarea_class)) return object;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        if (auto* found = Field(lv_obj_get_child(object, i))) return found;
    return nullptr;
}
void Click(const char* text)
{
    auto* trayTitle=Find(lv_screen_active(), "Tool Tray");Require(trayTitle,"Tool Tray missing");
    auto* palette=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle)));
    auto* label = Find(palette, text);Require(label, "Palette entry missing");
    lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(lv_screen_active());
}
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "Pass an isolated session test directory");
        const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        std::filesystem::create_directories(root);
        // A stale snapshot emulates an interrupted previous editor process.
        SdlSimulator simulator;Require(simulator.Initialize(1440, 960), "SDL initialization failed");
        UI editor(simulator.GetKeyboard());
        Click("Bar");
        const auto original = editor.ProjectDocument();
        EditorSession::WriteAtomic(root / "recovery.json", json({{"version", 1}, {"path", (root / "recovered.json").u8string()}, {"project", original}}).dump());
        editor.MenuAction("new");
        editor.EnableSessionPersistence(root);
        Require(Find(lv_layer_top(), "Recover previous work"), "Recovery prompt was not shown");
        editor.MenuAction("recovery-restore");
        Require(editor.ProjectDocument() == original && editor.Dirty(), "Recovery did not restore unsaved work");
        editor.FlushSession();
        Require(std::filesystem::exists(root / "recovery.json"), "Recovery snapshot was not saved");
        EditorSession competing;
        bool locked = false;try { competing.Open(root); } catch (const std::exception&) { locked = true; }
        Require(locked, "Another instance could overwrite this session");
        editor.MenuAction("new");
        Require(Find(lv_layer_top(), "Save your changes?"), "New project did not protect unsaved work");
        editor.MenuAction("cancel-file");
        Require(editor.ProjectDocument() == original, "Cancel changed the project");
        editor.MenuAction("new");editor.MenuAction("save-pending");
        auto* field = Field(lv_layer_top());Require(field, "Save path field missing");
        const auto saved = (root / "saved.json").u8string();lv_textarea_set_text(field, saved.c_str());
        editor.MenuAction("confirm-file");
        Require(editor.ProjectDocument()["obj"].empty() && !editor.Dirty(), "Save and continue did not create a clean project");
        Require(std::filesystem::exists(root / "saved.json") && !std::filesystem::exists(root / "recovery.json"), "Saving did not clear the recovery snapshot");
        Require(!editor.RecentProjects().empty() && editor.RecentProjects().front() == saved, "Recent project list did not update");
        editor.MenuAction("recent:" + saved);Require(!editor.Dirty(), "Opened project is dirty");
        Click("Bar");Require(editor.Dirty(), "Project edit was not tracked");
        editor.Undo();Require(!editor.Dirty(), "Undo back to the saved document is dirty");
        Click("Bar");
        SDL_Event quit{};quit.type=SDL_EVENT_QUIT;SDL_PushEvent(&quit);
        for (int i=0;i<3;++i) Require(simulator.ProcessEvents(), "Native quit bypassed unsaved prompt");
        Require(Find(lv_layer_top(), "Save your changes?"), "Native quit did not show unsaved prompt");
        editor.MenuAction("cancel-file");
        auto* title = Find(lv_screen_active(), "Tool Tray");Require(title, "Tool Tray is missing");
        lv_obj_set_hidden(lv_obj_get_parent(lv_obj_get_parent(title)), true);
        editor.MenuAction("zoom-in");editor.FlushSession();
        std::ifstream settingsFile(root / "settings.json");const auto settings=json::parse(settingsFile);settingsFile.close();
        Require(settings["hidden"]["widgets"] == true && settings["zoom"].get<double>() > 1, "Workspace settings were not persisted");
        editor.MenuAction("new");editor.MenuAction("discard-pending");
        Require(!editor.Dirty() && editor.ProjectDocument()["obj"].empty(), "Discard did not continue the requested action");
        // Work with three siblings; selecting Screen between creations avoids
        // accidentally constructing a parent/child chain in the palette.
        for (int i = 0; i < 3; ++i) { editor.MenuAction("select-screen");Click("Bar"); }
        auto* simulatorTitle = Find(lv_screen_active(), "Simulator [Size:320x240]");Require(simulatorTitle, "Simulator missing");
        auto* simulatorWindow = lv_obj_get_parent(lv_obj_get_parent(simulatorTitle));
        auto* surface = lv_obj_get_child(EditorWindow::Content(simulatorWindow), 0);
        auto* a = lv_obj_get_child(surface, 0);auto* b = lv_obj_get_child(surface, 1);auto* c = lv_obj_get_child(surface, 2);
        auto* properties = static_cast<ObjectUserData*>(lv_obj_get_user_data(a))->toolTray->GetPropertyWindow();
        auto name = [&](lv_obj_t* object, const char* value) { properties->SetSelectedObject(object);Require(properties->RenameSelected(value), "Cannot name test widget"); };
        name(a, "Nudge A");name(b, "Nudge B");name(c, "Nudge C");
        auto findWidget = [&](const char* value) {
            for (auto* object : properties->GetObjectTree()->GetObjects())
            {
                const auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
                if (data && data->objectName == value) return object;
            }
            throw std::runtime_error("Undo/redo lost a test widget");
        };
        auto refresh = [&] { a=findWidget("Nudge A");b=findWidget("Nudge B");c=findWidget("Nudge C");lv_obj_update_layout(surface); };
        lv_obj_set_pos(a,20,20);lv_obj_set_pos(b,80,80);lv_obj_set_pos(c,220,180);
        lv_obj_set_size(a,20,20);lv_obj_set_size(b,30,30);lv_obj_set_size(c,40,40);lv_obj_update_layout(surface);
        properties->SetSelectedObject(a);
        Require(editor.HandleShortcut(SDLK_RIGHT,false) && lv_obj_get_x(a)==21, "Arrow key did not nudge one project pixel");
        Require(editor.Undo(), "Nudge has no undo step");refresh();Require(lv_obj_get_x(a)==20, "Undo did not restore the nudge position");
        Require(editor.Redo(), "Nudge has no redo step");refresh();Require(lv_obj_get_x(a)==21, "Redo did not restore the nudge position");
        Require(editor.HandleShortcut(SDLK_DOWN,false,true) && lv_obj_get_y(a)==30, "Shift+Arrow did not nudge ten project pixels");
        Require(editor.Undo(), "Shift nudge has no undo step");refresh();Require(lv_obj_get_y(a)==20, "Shift nudge undo failed");
        Require(editor.Redo(), "Shift nudge has no redo step");refresh();Require(lv_obj_get_y(a)==30, "Shift nudge redo failed");

        auto* xLabel=Find(properties->GetWindow(),"X");Require(xLabel,"Coordinate field missing");
        auto* xField=lv_obj_get_child(lv_obj_get_parent(xLabel),1);
        FocusEditorControl(properties->GetKBGroup(),xField);
        const auto beforeTextArrow=editor.ProjectDocument();
        Require(!editor.HandleShortcut(SDLK_RIGHT,false) && editor.ProjectDocument()==beforeTextArrow, "Text-field arrow key changed widget geometry");
        properties->SetSelectedObject(a);
        Serialization::LVObject::SetValue(a,"/base/drag",0);
        const int lockedX=lv_obj_get_x(a);
        Require(editor.HandleShortcut(SDLK_RIGHT,false) && lv_obj_get_x(a)==lockedX, "Nudge moved a position-locked widget");
        Serialization::LVObject::SetValue(a,"/base/drag",1);

        lv_obj_set_pos(a,20,20);lv_obj_set_pos(b,80,80);lv_obj_set_pos(c,220,180);lv_obj_update_layout(surface);
        properties->SetSelection({a,b,c});
        editor.MenuAction("distribute-horizontal");lv_obj_update_layout(surface);
        const auto horizontalGap1=lv_obj_get_x(b)-lv_obj_get_x(a)-lv_obj_get_width(a);
        const auto horizontalGap2=lv_obj_get_x(c)-lv_obj_get_x(b)-lv_obj_get_width(b);
        Require(lv_obj_get_x(a)==20 && lv_obj_get_x(c)==220 && horizontalGap1==75 && horizontalGap2==75, "Horizontal distribution did not preserve endpoints and equalize gaps");
        Require(editor.Undo(), "Distribution has no undo step");refresh();Require(lv_obj_get_x(b)==80 && editor.SelectionCount()==3, "Distribution undo lost geometry or selection");
        Require(editor.Redo(), "Distribution has no redo step");refresh();Require(lv_obj_get_x(b)==115 && editor.SelectionCount()==3, "Distribution redo lost geometry or selection");
        editor.MenuAction("distribute-vertical");lv_obj_update_layout(surface);
        const auto verticalGap1=lv_obj_get_y(b)-lv_obj_get_y(a)-lv_obj_get_height(a);
        const auto verticalGap2=lv_obj_get_y(c)-lv_obj_get_y(b)-lv_obj_get_height(b);
        Require(lv_obj_get_y(a)==20 && lv_obj_get_y(c)==180 && verticalGap1==55 && verticalGap2==55, "Vertical distribution did not preserve endpoints and equalize gaps");
        Require(editor.Undo(), "Vertical distribution has no undo step");refresh();Require(lv_obj_get_y(b)==80, "Vertical distribution undo failed");
        Require(editor.Redo(), "Vertical distribution has no redo step");refresh();Require(lv_obj_get_y(b)==95, "Vertical distribution redo failed");
        Serialization::LVObject::SetValue(b,"/base/drag",0);
        const auto beforeLockedDistribution=editor.ProjectDocument();editor.MenuAction("distribute-horizontal");
        Require(editor.ProjectDocument()==beforeLockedDistribution, "Distribution moved a locked selection");
        Serialization::LVObject::SetValue(b,"/base/drag",1);

        lv_obj_set_flex_flow(surface,LV_FLEX_FLOW_ROW);lv_obj_update_layout(surface);
        Require(lv_obj_is_layout_positioned(a),"Flex layout did not manage its child positions");
        properties->SetSelectedObject(a);
        const auto beforeManagedNudge=editor.ProjectDocument();
        Require(editor.HandleShortcut(SDLK_RIGHT,false) && editor.ProjectDocument()==beforeManagedNudge, "Nudge edited a position controlled by parent layout");
        properties->SetSelection({a,b,c});
        const auto beforeManagedAlignment=editor.ProjectDocument();editor.MenuAction("align-left");
        Require(editor.ProjectDocument()==beforeManagedAlignment, "Alignment edited positions controlled by parent layout");
        editor.MenuAction("distribute-horizontal");
        Require(editor.ProjectDocument()==beforeManagedAlignment, "Distribution edited positions controlled by parent layout");

        std::cout << "Session recovery, save guards, recents, storage, preferences, nudging and distribution passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
