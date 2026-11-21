#include "UI.h"
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../CodeGeneration/Targets/Target.h"
#include "../../CodeGeneration/ProjectAssets.h"
#include "../Fonts/FontConverterWindow.h"
#include "../Fonts/FontManagerWindow.h"
#include "../Images/ImageConverterWindow.h"
#include "../Images/ImageManagerWindow.h"
#include "../ProjectCreator/ProjectCreatorWindow.h"
#include "../LanguageModels/LanguageModelWindow.h"
#include "../LanguageModels/ModelPlayground.h"
#include "../Devices/DeviceWindow.h"
#include "../ProjectThemes/DesignContext.h"
#include "../Components/ComponentLibrary.h"
#include "../ProjectThemes/ThemeGallery.h"
#include "../Interactions/InteractionEditor.h"
#include "../Validation/ScreenValidation.h"
#include "ProductWindows.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorTextInput.h"
#include "../Utils/EditorIcons.h"
#include "../Utils/EditorSpinbox.h"
#include <iostream>
#include <fstream>
#include <Serialization/Page.h>
#include <Serialization/Project.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/ProjectDesign.h>
#include <Serialization/NativeStyles.h>
#include <Serialization/InteractionActions.h>
#include "../Utils/EditorWindow.h"
#include "../Utils/CommandPalette.h"
#include "../PreviewProfiles/PreviewProfiles.h"
#include "../Performance/PerformanceWindow.h"
#include "../Validation/ProblemsPanel.h"
#include "../Screens/ScreenFlowWindow.h"
#include <unordered_map>
#include <unordered_set>
#include <SDL3/SDL.h>
#include <chrono>
#include <cmath>
#include "../Drivers/SDL3/NativeSDL3.h"
#ifdef _WIN32
#include <windows.h>
#endif

UI::UI(lv_indev_t* kb_indev)
{
    // Setup logger
    auto console = spdlog::stdout_color_mt("console");
    auto err_logger = spdlog::stderr_color_mt("stderr");
    lv_coord_t hres = lv_disp_get_hor_res(nullptr);
    lv_coord_t vres = lv_disp_get_ver_res(nullptr);
    lv_obj_t* screen = lv_obj_create(lv_disp_get_scr_act(nullptr), nullptr);
    lv_obj_set_style(screen, &lv_style_scr);
    lv_obj_set_size(screen, hres, vres);
    initializeThemes(120);
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(lv_scr_act(), false);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202020), LV_PART_MAIN);
#endif
    activeTheme = themes[0];
#if LVGL_VERSION_MAJOR >= 9
    lv_display_set_theme(lv_display_get_default(), activeTheme);
#else
    lv_theme_set_current(activeTheme);
#endif

    simWindow = new SimWindow(320, 240);
    propertyWindow = new PropertyWindow(kb_indev, simWindow, hres, vres);
    toolBar = new ToolBar(propertyWindow, simWindow);
    toolTray = new ToolTray(
        lv_scr_act(),
        propertyWindow->GetObjectTree(),
        simWindow->GetDrawSurface(),
        propertyWindow,
        toolBar);
#if LVGL_VERSION_MAJOR >= 9
    CreateMenuBar();
    CreateWorkspace();
    toolBar->variableDocument = [this] { return ProjectDocument(); };
    toolBar->variableScreen = [this] { return editingScreen; };
    toolBar->revealVariableTarget = [this](size_t screen, int oid) { SelectObjectOnScreen(screen, oid); };
    toolBar->variableRenamed = [this](const std::string& from, const std::string& to) { RenameVariableReferences(from, to); };
    toolBar->variableSelected = [this] { if (!restoringHistory) history.BreakGroup(); };
    toolTray->onWidgetCreated = [this](lv_obj_t* object) { ProjectAuthoring::ApplyNewDefaults(object, ProjectDocument()); };
    toolTray->onWidgetChanged = [this]{if(!restoringHistory&&!applyingDocument&&!serializingDocument){screenContentDirty=true;InvalidateDocument();}};
    propertyWindow->SetSelectedObject(simWindow->GetDrawSurface());
    history.Reset(CaptureHistory());
    EditorEdit::observer = this;
    savedDocument = ProjectDocument().dump();
    toolBar->RefreshVariableContext(CachedDocument(), true);
#endif
}

#if LVGL_VERSION_MAJOR >= 9
namespace
{
    lv_obj_t* CanvasHit(lv_obj_t* viewport, lv_point_t point)
    {
        auto* hit=lv_indev_search_obj(lv_layer_sys(),&point);
        if(!hit)hit=lv_indev_search_obj(lv_layer_top(),&point);
        if(!hit)hit=lv_indev_search_obj(lv_screen_active(),&point);
        for(auto* parent=hit;parent;parent=lv_obj_get_parent(parent))if(parent==viewport)return hit;
        return nullptr;
    }

    lv_obj_t* MenuButton(lv_obj_t* parent, const char* label, UI* ui, const std::string& action)
    {
        struct Binding { UI* ui; std::string action; };
        auto* button = lv_button_create(parent);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(button, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_all(button, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
        auto* text = lv_label_create(button);
        lv_label_set_text(text, label);
        lv_obj_center(text);
        const bool iconOnly=action=="undo" || action=="redo" || action=="zoom-in" || action=="zoom-out" || action=="fit-canvas";
        EditorIcons::Set(button,action.c_str(),iconOnly,iconOnly?18:20);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) delete binding;
            else if (lv_event_get_code(event) == LV_EVENT_CLICKED)
            {
                // The action can destroy its menu and this binding.
                auto* ui = binding->ui;
                const auto action = binding->action;
                ui->MenuAction(action);
            }
        }, LV_EVENT_ALL, new Binding{ui, action});
        return button;
    }
}

UI::~UI()
{
    toolBar->CancelVariableConnection();
    CloseMenu();
    CommandPalette::Close();PerformanceWindow::Close();PreviewProfiles::Close();PreviewProfiles::CloseLayout();ScreenFlowWindow::Close();
    if(problemsDock){ProblemsPanel::Close(problemsDock);problemsDock=nullptr;}
    DeviceWindow::SetContext({});toolTray->onWidgetChanged={};toolTray->onWidgetCreated={};
    runPreview.Stop();
    DeviceWindow::Shutdown(); ModelPlayground::Shutdown();
    ProjectThemeWindow::Close(); ComponentLibraryWindow::Close();
    InteractionEditor::Close(); ScreenValidation::Close();
    LanguageModelWindow::Shutdown();
    if(groupMoved)EndEdit();
    try { FlushSession(); } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    propertyWindow->onSelectionChanged = {};
    propertyWindow->GetObjectTree()->selectionHandler = {};
    propertyWindow->GetObjectTree()->deleteSelectionHandler = {};
    if(canvasViewport)lv_obj_remove_event_cb_with_user_data(canvasViewport,nullptr,this);
    lv_obj_remove_event_cb_with_user_data(simWindow->GetDrawSurface(),nullptr,this);
    uicreator_sdl3_set_event_handler(nullptr, nullptr);
    if(EditorGrid::active == &canvasGrid)EditorGrid::active = nullptr;
    lv_obj_remove_event_cb_with_user_data(canvasGrid.root, EditorGrid::Draw, &canvasGrid);
    if(workspaceTimer)lv_timer_delete(workspaceTimer);
    ProjectCreatorWindow::Close();
    if(EditorEdit::observer==this)EditorEdit::observer=nullptr;
    // Document deletion callbacks still use the hierarchy rows during LVGL teardown.
    // A dragged simulator may have moved after those rows in screen child order.
    lv_obj_move_to_index(canvasViewport ? canvasViewport : simWindow->GetWindow(), 0);
}
json UI::ProjectDocument() const {
    return CachedDocument();
}
std::vector<int> UI::SelectedObjectIds() const {
    const auto objects=propertyWindow->GetObjectTree()->GetObjects(); std::vector<int> ids;
    for(auto* object:propertyWindow->SelectedObjects()) {
        const auto found=std::find(objects.begin(),objects.end(),object);
        if(found!=objects.end())ids.push_back(static_cast<int>(std::distance(objects.begin(),found)));
    }
    return ids;
}
void UI::SelectObjectIds(const std::vector<int>& ids) {
    const auto objects=propertyWindow->GetObjectTree()->GetObjects(); std::vector<lv_obj_t*> selected;
    for(int id:ids)if(id>=0 && size_t(id)<objects.size())selected.push_back(objects[id]);
    propertyWindow->SetSelection(selected);
}
void UI::ApplyDocument(json document) {
    runPreview.Stop();
    Serialization::ProjectScreens::Normalize(document,true);
    // Construct first: invalid documents leave the current project intact.
    auto loaded=Serialization::Project::FromJSON(document);
    const auto selection=SelectedObjectIds(); EditorEdit::Scope edit(this);
    const bool previous=restoringHistory; restoringHistory=true; applyingDocument=true;
    try { InstallProject(std::move(loaded)); SelectObjectIds(selection); restoringHistory=previous; applyingDocument=false; }
    catch(...) {restoringHistory=previous;applyingDocument=false;throw;}
}
bool UI::Dirty() const { return SerializedDocument() != savedDocument; }

void UI::RememberProject()
{
    if (projectPath.empty()) return;
    std::error_code error;
    const auto path = std::filesystem::absolute(std::filesystem::u8path(projectPath), error).lexically_normal().u8string();
    if (error) return;
    recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), path), recentProjects.end());
    recentProjects.insert(recentProjects.begin(), path);
    if (recentProjects.size() > 8) recentProjects.resize(8);
}
void UI::MarkSaved(bool remember)
{
    savedDocument = SerializedDocument();
    lastAutosaveDocument.clear();
    if (remember) RememberProject();
    if (session.Enabled() && !recoveryPending)
    {
        try { session.RemoveRecovery(); FlushSession(); }
        catch (const std::exception& error) { sessionNotice = error.what(); }
    }
}
void UI::EnableSessionPersistence(const std::filesystem::path& directory)
{
    protectWork = true;
    try
    {
        session.Open(directory);
        json settings;
        try { settings = session.Read("settings.json"); }
        catch (const std::exception&) { sessionNotice = "Workspace preferences could not be read; defaults are in use."; }
        if (settings.is_object())
        {
            if (settings.contains("recent") && settings["recent"].is_array())
                for (const auto& path : settings["recent"])
                    if (path.is_string() && recentProjects.size() < 8) recentProjects.push_back(path.get<std::string>());
            auto number = [&](const char* key, int fallback, int minimum, int maximum) {
                const auto found = settings.find(key);
                if (found == settings.end() || !found->is_number_integer()) return fallback;
                return std::clamp(found->get<int>(), minimum, maximum);
            };
            autosaveSeconds = number("autosaveSeconds", 30, 10, 300);
            canvasGrid.spacing = number("spacing", 8, 2, 128);
            for (const auto& entry : {std::make_pair("grid", &canvasGrid.visible), std::make_pair("snap", &canvasGrid.snapping), std::make_pair("guides", &canvasGrid.guides)})
                if (settings.contains(entry.first) && settings[entry.first].is_boolean()) *entry.second = settings[entry.first].get<bool>();
            dockLeft = number("leftWidth", 0, 0, 460); dockHierarchy = number("hierarchyWidth", 0, 0, 380);
            dockInspector = number("inspectorWidth", 0, 0, 500); dockBottom = number("bottomHeight", 0, 0, 380);
            if (settings.contains("zoom") && settings["zoom"].is_number()) canvasZoom = std::clamp(settings["zoom"].get<double>(), 26 / 256.0, 4.0);
            if (settings.contains("fit") && settings["fit"].is_boolean()) canvasFitted = settings["fit"].get<bool>();
            for (const auto& panel : {std::make_pair("screens", screensDock), std::make_pair("widgets", toolTray->GetWindow()), std::make_pair("hierarchy", propertyWindow->GetObjectTree()->GetTreeViewUI()), std::make_pair("properties", lv_obj_get_parent(propertyWindow->GetWindow())), std::make_pair("tools", toolBar->GetWindow()), std::make_pair("simulator", simWindow->GetWindow())})
                if (settings.contains("hidden") && settings["hidden"].is_object() && settings["hidden"].contains(panel.first) && settings["hidden"][panel.first].is_boolean())
                    lv_obj_set_hidden(panel.second, settings["hidden"][panel.first].get<bool>());
            EditorWindow::Show(simWindow->GetWindow());
            if (gridControls.size() == 4)
            {
                lv_obj_set_state(gridControls[0], LV_STATE_CHECKED, canvasGrid.visible);
                lv_obj_set_state(gridControls[1], LV_STATE_CHECKED, canvasGrid.snapping);
                lv_obj_set_state(gridControls[2], LV_STATE_CHECKED, canvasGrid.guides);
                lv_spinbox_set_value(gridControls[3], canvasGrid.spacing);
            }
            LayoutWorkspace(); UpdateCanvas();
        }
        try
        {
            recoveryDocument = session.Read("recovery.json");
            if (recoveryDocument.contains("project") && recoveryDocument["project"].is_object())
            {
                recoveryPending = true;
                ShowSessionPrompt(true);
            }
            else if (std::filesystem::exists(session.Directory() / "recovery.json"))
                throw std::runtime_error("The recovery file does not contain a project document.");
        }
        catch (const std::exception& error)
        {
            sessionNotice = std::string("Recovery snapshot could not be read: ") + error.what();
            recoveryPending = true;recoveryDocument = {{"path", "an unreadable snapshot"}};
            ShowSessionPrompt(true);
            if (fileStatus) lv_label_set_text(fileStatus, (sessionNotice + "\nDiscard snapshot to continue, or Exit to keep it for inspection.").c_str());
        }
    }
    catch (const std::exception& error) { sessionNotice = error.what(); }
}
void UI::FlushSession()
{
    if (!session.Enabled()) return;
    json hidden = json::object();
    for (const auto& panel : {std::make_pair("screens", screensDock), std::make_pair("widgets", toolTray->GetWindow()), std::make_pair("hierarchy", propertyWindow->GetObjectTree()->GetTreeViewUI()), std::make_pair("properties", lv_obj_get_parent(propertyWindow->GetWindow())), std::make_pair("tools", toolBar->GetWindow()), std::make_pair("simulator", simWindow->GetWindow())})
        hidden[panel.first] = lv_obj_is_hidden(panel.second);
    session.Write("settings.json", {{"version", 1}, {"recent", recentProjects}, {"autosaveSeconds", autosaveSeconds},
        {"grid", canvasGrid.visible}, {"snap", canvasGrid.snapping}, {"guides", canvasGrid.guides}, {"spacing", canvasGrid.spacing},
        {"zoom", canvasZoom}, {"fit", canvasFitted}, {"hidden", hidden}, {"leftWidth", dockLeft}, {"hierarchyWidth", dockHierarchy},
        {"inspectorWidth", dockInspector}, {"bottomHeight", dockBottom}});
    if (recoveryPending || closeApproved) return;
    auto document = ProjectDocument();
    const auto serialized = SerializedDocument();
    if (serialized == savedDocument) { session.RemoveRecovery(); lastAutosaveDocument.clear(); }
    else if (serialized != lastAutosaveDocument)
    {
        session.Write("recovery.json", {{"version", 1}, {"path", projectPath}, {"project", std::move(document)}});
        lastAutosaveDocument = serialized;
        sessionNotice = "Recovery snapshot saved";
    }
}
void UI::SessionTick()
{
    const auto now = SDL_GetTicks();
    if (now - lastSessionTick < 1000) return;
    lastSessionTick = now;
    try
    {
        const auto title = std::filesystem::u8path(projectPath).filename().u8string() + (Dirty() ? " *" : "") + "  |  LVGL UI Creator";
        lv_label_set_text(documentTitle, title.c_str());
        lv_sdl_window_set_title(lv_display_get_default(), title.c_str());
        if (session.Enabled() && now - lastAutosaveTick >= uint64_t(autosaveSeconds) * 1000)
        {
            if (!editDepth) { FlushSession(); lastAutosaveTick = now; }
        }
    }
    catch (const std::exception& error) { sessionNotice = error.what(); }
}
bool UI::GuardDestructive(const std::string& action)
{
    if (!protectWork || allowDestructive || !Dirty()) return false;
    if (fileDialog) return true;
    pendingAction = action;
    ShowSessionPrompt(false);
    return true;
}
void UI::ShowSessionPrompt(bool recovery)
{
    if (fileDialog) return;
    fileDialog = EditorWindow::Create(lv_layer_top());
    EditorWindow::SetCloseHandler(fileDialog, [this, recovery]{ MenuAction(recovery ? "recovery-later" : "cancel-file"); });
    lv_obj_set_size(fileDialog, 560, 245); lv_obj_center(fileDialog);
    EditorWindow::SetTitle(fileDialog, recovery ? "Recover previous work" : "Save your changes?");
    auto* content = EditorWindow::Content(fileDialog);
    lv_obj_set_style_pad_all(content, 16, 0); lv_obj_set_style_pad_row(content, 14, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    auto* text = lv_label_create(content); fileStatus = text;lv_obj_set_width(text, LV_PCT(100));
    const auto message = recovery ? "A recovery snapshot is available for " + recoveryDocument.value("path", std::string("an untitled project")) + ". Recover it to review and save your work." : "This project has unsaved changes. Save them before continuing, discard them, or cancel to keep editing.";
    lv_label_set_text(text, message.c_str());
    auto* row = lv_obj_create(content); lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_pad_all(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0); lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(MenuButton(row, recovery ? "Recover" : "Save changes", this, recovery ? "recovery-restore" : "save-pending"), 140, 36);
    lv_obj_set_size(MenuButton(row, recovery ? "Discard snapshot" : "Discard changes", this, recovery ? "recovery-discard" : "discard-pending"), 160, 36);
    lv_obj_set_size(MenuButton(row, recovery ? "Exit" : "Cancel", this, recovery ? "recovery-later" : "cancel-file"), 100, 36);
}
void UI::OpenProjectPath(const std::string& filename)
{
    const auto path = std::filesystem::u8path(filename);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open project file: " + filename);
    Serialization::LoadedProject restored;
    if (path.extension() == ".pb")
    {
        const Serialization::Binary bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        restored = Serialization::Project::FromProtobuf(bytes);
    }
    else restored = Serialization::Project::FromJSON(json::parse(input));
    restoringHistory = true;
    try { InstallProject(std::move(restored)); restoringHistory = false; }
    catch (...) { restoringHistory = false; throw; }
    projectPath = filename;
    hasProjectPath=true;
    history.Reset(CaptureHistory());
    MarkSaved();
}
void UI::ShowPreferences()
{
    if (fileDialog) return;
    fileDialog = EditorWindow::Create(lv_layer_top());
    EditorWindow::SetCloseHandler(fileDialog, [this]{ MenuAction("cancel-file"); });
    lv_obj_set_size(fileDialog, 540, 460); lv_obj_center(fileDialog);
    EditorWindow::SetTitle(fileDialog, "Workspace preferences");
    auto* content = EditorWindow::Content(fileDialog);
    lv_obj_set_style_pad_all(content, 16, 0); lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    preferenceFields.clear();
    auto field = [&](const char* name, int value, int minimum, int maximum) {
        auto* row = lv_obj_create(content); lv_obj_set_size(row, LV_PCT(100), 40);
        lv_obj_set_style_pad_all(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        auto* label = lv_label_create(row); lv_label_set_text(label, name); lv_obj_set_flex_grow(label, 1);
        auto* spinbox = lv_spinbox_create(row); lv_spinbox_set_range(spinbox, minimum, maximum);
        lv_spinbox_set_digit_format(spinbox, 3, 0); lv_spinbox_set_value(spinbox, value);
        lv_obj_set_size(spinbox, 95, 36); preferenceFields.push_back(spinbox);
        lv_obj_add_event_cb(spinbox, [](lv_event_t* event) {
            auto* ui = static_cast<UI*>(lv_event_get_user_data(event));
            FocusEditorControl(ui->propertyWindow->GetKBGroup(), lv_event_get_target_obj(event));
        }, LV_EVENT_CLICKED, this);
    };
    field("Autosave interval (seconds)", autosaveSeconds, 10, 300);
    field("Widgets panel width (px)", lv_obj_get_width(toolTray->GetWindow()), 180, 460);
    field("Hierarchy width (px)", lv_obj_get_width(propertyWindow->GetObjectTree()->GetTreeViewUI()), 160, 380);
    field("Properties width (px)", lv_obj_get_width(lv_obj_get_parent(propertyWindow->GetWindow())), 280, 500);
    field("Assets panel height (px)", lv_obj_get_height(toolBar->GetWindow()), 140, 380);
    auto* info = lv_label_create(content); lv_obj_set_width(info, LV_PCT(100));
    const auto notice = session.Enabled() ? "Grid, zoom, panel sizes and visibility are remembered automatically. " + sessionNotice : "Preferences apply to this session. Persistent session storage is unavailable. " + sessionNotice;
    lv_label_set_text(info, notice.c_str());
    lv_obj_set_size(MenuButton(content, "Apply", this, "apply-preferences"), 100, 36);
}
UndoHistory::State UI::CaptureHistory() const {
    auto objects=propertyWindow->GetObjectTree()->GetObjects();int selected=-1;
    for(size_t i=0;i<objects.size();++i)if(objects[i]==propertyWindow->GetSelectedObject())selected=(int)i;
    auto document=ProjectDocument();
    std::vector<int> selections;
    if(!Serialization::ProjectScreens::Multiple(document))for(size_t i=0;i<objects.size();++i)document["obj"][i]["hostIndex"]=lv_obj_get_index(objects[i]);
    for(auto* object:propertyWindow->SelectedObjects()) {
        const auto found=std::find(objects.begin(),objects.end(),object);
        if(found!=objects.end())selections.push_back((int)std::distance(objects.begin(),found));
    }
    document=Serialization::ProjectScreens::Canonical(std::move(document));
    return {document.dump(),selected,std::move(selections),editingScreen};
}
void UI::BeginEdit(const void* key,bool coalesce) {
    if(restoringHistory)return;
    if(editDepth++==0) {
        editKey=key;editCoalesces=coalesce;
        if(!coalesce)history.BreakGroup();
        try {history.Record(CaptureHistory());}catch(const std::exception& e){std::cerr<<"History capture: "<<e.what()<<'\n';}
    }
}
void UI::EndEdit() noexcept {
    if(restoringHistory || !editDepth)return;
    if(--editDepth==0)try {
        toolBar->CommitVariableTargets();
        toolTray->RefreshKeyboardBindings();screenContentDirty=true;InvalidateDocument();
        const bool changed=history.Record(CaptureHistory(),editCoalesces?editKey:nullptr,SDL_GetTicks());
        toolBar->RefreshVariableContext(CachedDocument());
        if(changed)ScreenFlowWindow::Refresh();
    }catch(const std::exception& e){std::cerr<<"History capture: "<<e.what()<<'\n';}
}
void UI::InstallProject(Serialization::LoadedProject restored,int selection) {
                auto objects = std::move(restored.objects);
                std::map<int,lv_obj_t*> importedObjects;
                for(auto* object:objects)if(auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object)))importedObjects.emplace(static_cast<int>(data->objectID),object);
                // The canvas is reused when restoring a document. A full style
                // snapshot must also remove theme locals absent from that
                // snapshot, while retaining the editor's attached styles.
                const auto canvasBase=restored.screen.find("base");
                if(canvasBase!=restored.screen.end() && canvasBase->is_object()) {
                    const auto styles=canvasBase->find("nativeStyles");
                    if(styles!=canvasBase->end() && styles->is_array())
                        Serialization::NativeStyles::RemoveMissingLocalProperties(simWindow->GetDrawSurface(),*styles);
                }
                CloseDocument();
                screenRecords=restored.metadata.value("screens",json::array());
                editingScreen=Serialization::ProjectScreens::Editing(restored.metadata);startScreen=restored.metadata.value("activeScreen",size_t(0));
                for (const auto* key : {"projectTheme", "components", "sharedStyles","previewProfiles","deviceTargets","accessibility","translations"})
                    if(restored.metadata.contains(key))projectMetadata[key]=restored.metadata[key];
                auto* tree=propertyWindow->GetObjectTree();
                simWindow->SetSize(restored.screen.at("width").get<int>(), restored.screen.at("height").get<int>());
                Serialization::Screen::FromJSON(restored.screen, simWindow->GetDrawSurface());
                propertyWindow->Fonts().Load(restored.fonts);propertyWindow->Images().Load(restored.images);propertyWindow->Tries().Load(restored.tries);
                std::unordered_map<lv_obj_t*, unsigned int> parents{{lv_screen_active(), 0}};
                std::vector<lv_obj_t*> pending = objects;
                const std::unordered_set<lv_obj_t*> documentObjects(objects.begin(), objects.end());
                while (!pending.empty())
                {
                    bool progress = false;
                    for (auto entry = pending.begin(); entry != pending.end();)
                    {
                        auto* object = *entry;
                        auto* ancestor = lv_obj_get_parent(object);
                        while (ancestor && ancestor != lv_screen_active() && !documentObjects.count(ancestor))
                            ancestor = lv_obj_get_parent(ancestor);
                        const auto parent = parents.find(ancestor);
                        if (parent == parents.end()) { ++entry; continue; }
                        if (lv_obj_get_parent(object) == lv_screen_active()) lv_obj_set_parent(object, simWindow->GetDrawSurface());
                        toolTray->AdoptWidget(object, parent->second);
                        parents[object] = static_cast<ObjectUserData*>(lv_obj_get_user_data(object))->objectID;
                        entry = pending.erase(entry);
                        progress = true;
                    }
                    if (!progress) throw std::runtime_error("Project hierarchy could not be restored");
                }
                propertyWindow->Fonts().Rebind(simWindow->GetDrawSurface());propertyWindow->Images().Rebind(simWindow->GetDrawSurface());propertyWindow->Tries().Rebind(simWindow->GetDrawSurface());
                const auto currentObjects=tree->GetObjects();
                auto variables=restored.variables;
                for(auto& item:variables) {
                    auto remap=[&](json& binding) {
                        const int id=binding.value("oid",-1);
                        if(id<0 || binding.value("screen",size_t(0))!=editingScreen)return;
                        const auto original=importedObjects.find(id);if(original==importedObjects.end())return;
                        const auto field=std::find(currentObjects.begin(),currentObjects.end(),original->second);
                        if(field!=currentObjects.end())binding["oid"]=static_cast<int>(std::distance(currentObjects.begin(),field));
                    };
                    auto& variable=item.at("var");
                    if(variable.contains("bindings"))for(auto& binding:variable["bindings"])remap(binding);
                    else remap(variable);
                }
                toolBar->LoadVariables(variables);
                for(auto* keyboard:currentObjects)if(Serialization::AdvancedKeyboard::Type(keyboard)) {
                    const auto settings=Serialization::AdvancedKeyboard::ToJSON(keyboard).at("advancedKeyboard");
                    const int target=settings.value("targetOid",-1);const auto original=importedObjects.find(target);
                    if(target>=0 && original!=importedObjects.end()) {
                        const auto targetObject=std::find(currentObjects.begin(),currentObjects.end(),original->second);
                        if(targetObject!=currentObjects.end())propertyWindow->Tries().ApplySettings(keyboard,{{"targetOid",static_cast<int>(std::distance(currentObjects.begin(),targetObject))}});
                    }
                }
                toolTray->RefreshKeyboardBindings();
                const auto adopted=tree->GetObjects();
                propertyWindow->SetSelectedObject(selection>=0 && size_t(selection)<adopted.size()?adopted[selection]:simWindow->GetDrawSurface());
                screenContentDirty=screenRecords.empty();InvalidateDocument();RefreshScreensDock();
                toolBar->RefreshVariableContext(CachedDocument(), true);
}

void UI::RenameVariableReferences(const std::string& oldName, const std::string& newName)
{
    auto rewrite = [&](json& events) {
        if (!events.is_array()) return false;
        bool changed = false;
        for (auto item = events.begin(); item != events.end();) {
            const bool direct = item->value("variable", std::string{}) == oldName;
            const bool condition = item->contains("condition") && (*item)["condition"].is_object() &&
                (*item)["condition"].value("variable", std::string{}) == oldName;
            if (!direct && !condition) { ++item; continue; }
            changed = true;
            if (newName.empty()) { item = events.erase(item); continue; }
            if (direct) (*item)["variable"] = newName;
            if (condition) (*item)["condition"]["variable"] = newName;
            ++item;
        }
        return changed;
    };
    const auto objects = propertyWindow->GetObjectTree()->GetObjects();
    std::map<int, lv_obj_t*> targets;
    std::unordered_map<lv_obj_t*, int> indices;
    for (size_t i = 0; i < objects.size(); ++i) { targets[int(i)] = objects[i]; indices[objects[i]] = int(i); }
    for (auto* object : objects) {
        auto events = Serialization::InteractionActions::EventsForSave(object, indices);
        if (rewrite(events)) Serialization::InteractionActions::SetEvents(object, events, targets);
    }
    for (auto& screen : screenRecords) for (auto& object : screen["obj"])
        if (object.contains("base") && object["base"].contains("events")) rewrite(object["base"]["events"]);
    InteractionEditor::Close();
    InvalidateDocument();
}
void UI::RestoreHistory(const UndoHistory::State& state) {
    auto document=json::parse(state.document);if(Serialization::ProjectScreens::Multiple(document))Serialization::ProjectScreens::Select(document,state.screen);
    auto restored=Serialization::Project::FromJSON(document);
    restoringHistory=true;restoringScreenFlow=true;
    try {InstallProject(std::move(restored),state.selection);
        if(!state.selections.empty()) {
            const auto objects=propertyWindow->GetObjectTree()->GetObjects();std::vector<lv_obj_t*> selected;
            for(int index:state.selections)if(index>=0 && (size_t)index<objects.size())selected.push_back(objects[index]);
            propertyWindow->SetSelection(selected);
        }
        restoringHistory=false;restoringScreenFlow=false;}
    catch (...) {restoringHistory=false;restoringScreenFlow=false;throw;}
}
bool UI::Undo() {
    if(fileDialog || restoringHistory || editDepth)return false;
    try {history.Record(CaptureHistory());if(!history.CanUndo())return false;auto target=history.UndoState();RestoreHistory(target);history.DidUndo();history.ReplaceCurrent(CaptureHistory());return true;}
    catch(const std::exception& e){std::cerr<<"Undo: "<<e.what()<<'\n';return false;}
}
bool UI::Redo() {
    if(fileDialog || restoringHistory || editDepth)return false;
    try {history.Record(CaptureHistory());if(!history.CanRedo())return false;auto target=history.RedoState();RestoreHistory(target);history.DidRedo();history.ReplaceCurrent(CaptureHistory());return true;}
    catch(const std::exception& e){std::cerr<<"Redo: "<<e.what()<<'\n';return false;}
}
bool UI::HandleShortcut(int key,bool control,bool shift,bool alt) {
    if (toolBar->VariableDialogOpen()) {
        if (key == SDLK_ESCAPE && !control && !alt) { toolBar->CancelVariableConnection(); return true; }
        return false;
    }
    auto* inputGroup = EditorKeyboardGroup();
    auto* focusedInput = inputGroup ? lv_group_get_focused(inputGroup) : nullptr;
    bool canvasInput = false;
    for (auto* parent = focusedInput; parent; parent = lv_obj_get_parent(parent))
        if (parent == simWindow->GetDrawSurface()) { canvasInput = true; break; }
    const bool editorInput = !Running() && !menuPopup && focusedInput && IsEditorInputControl(focusedInput) && !canvasInput;
    if (!Running() && !menuPopup && control && !alt && HandleEditorTextShortcut(inputGroup, key)) return true;
    if (editorInput && !alt && ((!control && (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP ||
        key == SDLK_DOWN || key == SDLK_DELETE || key == SDLK_BACKSPACE)) || (control && key == 'd'))) return false;
    if(control&&shift&&key=='p'){MenuAction("commands");return true;}
    if(control&&shift&&key=='f'){MenuAction("property-search");return true;}
    if (alt && !control) {
        const char* menu = key == 'f' ? "File" : key == 'e' ? "Edit" : key == 'v' ? "View" :
            key == 'a' ? "Assets" : key == 'l' ? "Language Models" : key == 'x' ? "Export" :
            key == 'd' ? "Debug" : key == 'h' ? "Help" : nullptr;
        if (menu) { MenuAction(std::string("menu:") + menu); return true; }
    }
    if (menuPopup) return HandleMenuKey(key, control, shift);
    if(!control && !alt && (key==SDLK_F6 || (Running() && key==SDLK_ESCAPE))) {MenuAction("run-toggle");return true;}
    if(Running() && !(control && (key=='s' || key=='o' || key=='n')))return false;
    if(key==SDLK_F5 && !control && !alt && !fileDialog && !lv_obj_get_child_count(lv_layer_top())) {MenuAction("debug-screen");return true;}
    if (!control && !alt && !fileDialog && !lv_obj_get_child_count(lv_layer_top()) && SelectionCount())
    {
        auto* focused = lv_group_get_focused(propertyWindow->GetKBGroup());
        const bool editingText = focused && (lv_obj_check_type(focused, &lv_textarea_class) || lv_obj_check_type(focused, &lv_spinbox_class) || lv_obj_check_type(focused, &lv_dropdown_class));
        const int distance = shift ? 10 : 1;
        if (!editingText && (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN))
        {
            NudgeSelection(key == SDLK_LEFT ? -distance : key == SDLK_RIGHT ? distance : 0,
                           key == SDLK_UP ? -distance : key == SDLK_DOWN ? distance : 0);
            return true;
        }
    }
    if(control && (key=='z' || key=='y')) {MenuAction((key=='y'||shift)?"redo":"undo");return true;}
    if(control && key=='d' && !fileDialog && !lv_obj_get_child_count(lv_layer_top())){MenuAction("duplicate");return true;}
    if(key == SDLK_DELETE && !control && !alt && !fileDialog && !lv_obj_get_child_count(lv_layer_top())) {
        auto* focused=lv_group_get_focused(propertyWindow->GetKBGroup());
        const auto objects=propertyWindow->GetObjectTree()->GetObjects();
        if(focused && std::find(objects.begin(),objects.end(),focused)==objects.end() && (lv_obj_check_type(focused,&lv_textarea_class) || lv_obj_check_type(focused,&lv_spinbox_class)))return false;
        if(SelectionCount()){MenuAction("delete");return true;}
    }
    if(control && key=='s' && !fileDialog){MenuAction(shift?"save-as":"save");return true;}
    if(control && key=='o' && !fileDialog){MenuAction("open");return true;}
    if(control && key=='n' && !fileDialog){MenuAction("new");return true;}
    return false;
}

void UI::CloseDocument()
{
    CommandPalette::Close();
    if(!applyingDocument&&!switchingScreen){PreviewProfiles::Close();PreviewProfiles::CloseLayout();}
    if(!applyingDocument&&!switchingScreen&&!restoringScreenFlow)ScreenFlowWindow::Close();
    runPreview.Stop(); projectMetadata=json::object();
    screenRecords=json::array();editingScreen=startScreen=0;screenContentDirty=true;InvalidateDocument();
    DeviceWindow::Close(); ModelPlayground::Close();
    if(!applyingDocument&&!switchingScreen){ProjectThemeWindow::Close(); ComponentLibraryWindow::Close(); InteractionEditor::Close(); ScreenValidation::Close();}
    LanguageModelWindow::Close();
    FontConverterWindow::Close();FontManagerWindow::Close();ImageConverterWindow::Close();ImageManagerWindow::Close();
    toolBar->LoadVariables(json::array());
    propertyWindow->DeleteSelectedObject();
    auto* tree = propertyWindow->GetObjectTree();
    for (auto* object : tree->GetObjects())
        tree->DeleteObject(object);
    propertyWindow->Fonts().Apply(simWindow->GetDrawSurface(),{});
    propertyWindow->Fonts().Load(json::array());propertyWindow->Images().Load(json::array());propertyWindow->Tries().Load(json::array());
    propertyWindow->AddFontSelector();
}

void UI::ShowFileDialog(bool save, bool binary, bool cCode)
{
    if (fileDialog) lv_obj_delete(fileDialog);
    saving = save;
    exportingBinary = binary;
    exportingC = cCode;
    fileDialog = EditorWindow::Create(lv_layer_top());
    EditorWindow::SetCloseHandler(fileDialog,[this]{MenuAction("cancel-file");});
    lv_obj_set_size(fileDialog, 560, (binary || cCode) ? 420 : 230);
    lv_obj_center(fileDialog);
    EditorWindow::SetTitle(fileDialog, cCode ? "Export C project" : binary ? "Export device project" : save ? "Save project" : "Open project");
    auto* content = EditorWindow::Content(fileDialog);
    lv_obj_set_style_pad_all(content, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(content, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    projectPathField = lv_textarea_create(content);
    lv_obj_set_width(projectPathField, LV_PCT(100));
    lv_textarea_set_one_line(projectPathField, true);
    const auto initialPath = cCode ? (std::filesystem::u8path(projectPath).parent_path()/"generated_ui").u8string() : binary ? std::filesystem::u8path(projectPath).replace_extension(".pb").u8string() : save && std::filesystem::u8path(projectPath).extension() == ".pb" ? std::filesystem::u8path(projectPath).replace_extension(".json").u8string() : projectPath;
    lv_textarea_set_text(projectPathField, initialPath.c_str());
    BindEditorControl(propertyWindow->GetKBGroup(), projectPathField);
    fileStatus = lv_label_create(content);
    lv_obj_set_width(fileStatus, LV_PCT(100));
    lv_label_set_text(fileStatus, cCode ? "Output folder for C sources, headers, and assets" : binary ? "Device Protobuf file path (.pb)" : save ? "Editor project file path (.json)" : "Project file path (.json or .pb)");
    exportTarget = nullptr;
    if (binary || cCode)
    {
        auto* targetLabel = lv_label_create(content); lv_label_set_text(targetLabel, "Target LVGL version");
        exportTarget = lv_dropdown_create(content); lv_dropdown_set_options(exportTarget, "9.6\n9.5\n9.3\n9.2\n9.1\n8.3");
        lv_obj_set_width(exportTarget, LV_PCT(100));
        BindEditorControl(propertyWindow->GetKBGroup(), exportTarget);
        lv_obj_add_event_cb(exportTarget, [](lv_event_t* event) {
            auto* ui = static_cast<UI*>(lv_event_get_user_data(event));
            ui->MenuAction("export-preflight");
        }, LV_EVENT_VALUE_CHANGED, this);
        auto* preflight = MenuButton(content, "Check project before export", this, "export-preflight");
        lv_obj_set_size(preflight, LV_PCT(100), 36);
    }
    auto* row = lv_cont_create(content, nullptr);
    lv_obj_set_size(row, LV_PCT(100), 36);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(MenuButton(row, save ? "Save" : "Open", this, "confirm-file"), 100, 36);
    lv_obj_set_size(MenuButton(row, "Cancel", this, "cancel-file"), 100, 36);
    FocusEditorControl(propertyWindow->GetKBGroup(), projectPathField);
}

void UI::MenuAction(const std::string& action)
{
    if(action!="commands"&&action!="property-search")CommandPalette::Close();
    ProductWindows::Close();
    if (action.rfind("menu:", 0) == 0)
    {
        const auto name = action.substr(5);
        if (menuPopup && menuName == name) { CloseMenu(); return; }
        int x = 0;
        const auto anchor = menuButtons.find(name);
        if (anchor != menuButtons.end()) {
            lv_obj_update_layout(anchor->second);
            lv_area_t bounds{}; lv_obj_get_coords(anchor->second, &bounds); x = bounds.x1;
        }
        OpenMenu(name, x);
        return;
    }
    CloseMenu();
    if(Running() && action!="run-toggle" && action!="run-stop" && action!="performance" && action!="commands" && action!="property-search")runPreview.Stop();
    if ((action == "new" || action == "open" || action == "exit" || action.rfind("recent:", 0) == 0 || action.rfind("template:", 0) == 0) && GuardDestructive(action)) return;
    if(action.rfind("view:",0)==0) {
        const auto name=action.substr(5);
        for(const auto& panel : {std::make_pair("screens",screensDock),std::make_pair("widgets",toolTray->GetWindow()),std::make_pair("hierarchy",propertyWindow->GetObjectTree()->GetTreeViewUI()),std::make_pair("properties",lv_obj_get_parent(propertyWindow->GetWindow())),std::make_pair("tools",toolBar->GetWindow()),std::make_pair("simulator",simWindow->GetWindow())})
            if(name=="all" || name==panel.first)EditorWindow::Show(panel.second);
        if(name=="all")LayoutWorkspace();
    }
    else if (action == "undo") Undo();
    else if (action == "redo") Redo();
    else if (action == "run-toggle" || action == "run-stop" || action=="run-application") {
        if(Running())runPreview.Stop();
        else if(action!="run-stop")try {
            FinishSelectionGesture(); LanguageModelWindow::Close(); ModelPlayground::Close();PreviewProfiles::Close();PreviewProfiles::CloseLayout();
            ProjectThemeWindow::Close(); ComponentLibraryWindow::Close(); InteractionEditor::Close(); ScreenValidation::Close();ScreenFlowWindow::Close();
            auto document=ProjectDocument();if(action!="run-application"&&Serialization::ProjectScreens::Multiple(document))document["activeScreen"]=editingScreen;
            runPreview.Start(std::move(document),std::filesystem::u8path(projectPath).parent_path().u8string(),[this]{MenuAction("run-stop");});
        } catch(const std::exception& error){ProductWindows::Message("Cannot run screen",error.what());}
    }
    else if (action == "theme-gallery" || action == "component-library" || action=="device-previews" || action=="layout-designer") {
        ProjectAuthoring::DesignContext context;
        context.document=[this]{return ProjectDocument();}; context.apply=[this](json value){ApplyDocument(std::move(value));};
        context.selection=[this]{return SelectedObjectIds();}; context.select=[this](std::vector<int> ids){SelectObjectIds(ids);};
        context.keyboardGroup=propertyWindow->GetKBGroup();
        context.sourceDirectory=std::filesystem::u8path(projectPath).parent_path().u8string();context.applyMetadata=[this](json patch){ApplyMetadata(std::move(patch));};context.revision=[this]{return DocumentRevision();};
        if(action=="theme-gallery")ProjectThemeWindow::Open(std::move(context));else if(action=="component-library")ComponentLibraryWindow::Open(std::move(context));else if(action=="device-previews")PreviewProfiles::Open(std::move(context));else PreviewProfiles::OpenLayout(std::move(context));
    }
    else if(action=="performance")PerformanceWindow::Open({[this]{return runPreview.Metrics();},[this](bool enabled){runPreview.SetProfilingEnabled(enabled);},[this](const std::string& text,unsigned iterations){return runPreview.BenchmarkPrediction(text,iterations);},propertyWindow->GetKBGroup()});
    else if(action=="commands"||action=="property-search")OpenCommandPalette(action=="property-search");
    else if(action=="screen-add")AddScreen();
    else if(action=="screen-flow")ScreenFlowWindow::Open({[this]{return ProjectDocument();},[this](json document){ApplyDocument(std::move(document));},[this](size_t index){SelectScreen(index);},[this]{MenuAction("run-application");},propertyWindow->GetKBGroup()});
    else if(action=="problems"){EditorWindow::Show(toolBar->GetWindow());lv_tabview_set_active(toolBar->GetTabView(),2,LV_ANIM_OFF);ProblemsPanel::Refresh(problemsDock);}
    else if (action == "events-editor") InteractionEditor::Open(propertyWindow,[this]{return ProjectDocument();});
    else if (action == "validate-screen") ScreenValidation::Open(propertyWindow,[this]{return ProjectDocument();});
    else if (action == "device-tools") DeviceWindow::Open(propertyWindow);
    else if (action == "model-playground") ModelPlayground::Open(propertyWindow);
    else if (action == "debug-screen") {
        try { debugPreview.Start(ProjectDocument(),std::filesystem::u8path(projectPath).parent_path().u8string()); }
        catch(const std::exception& error){ProductWindows::Message("Debug screen failed",error.what());}
    }
    else if (action == "debug-stop") debugPreview.Stop();
    else if (action == "new") {restoringHistory=true;CloseDocument();restoringHistory=false;projectPath="project.json";hasProjectPath=false;history.Reset(CaptureHistory());MarkSaved(false);RefreshScreensDock();}
    else if(action=="save"&&hasProjectPath&&std::filesystem::u8path(projectPath).extension()!=".pb"){
        try{SaveProjectTo(std::filesystem::u8path(projectPath));}catch(const std::exception& error){ProductWindows::Message("Save failed",error.what());}
    }
    else if (action == "open" || action == "save" || action=="save-as") ShowFileDialog(action != "open");
    else if (action.rfind("recent:", 0) == 0)
    {
        try { OpenProjectPath(action.substr(7)); }
        catch (const std::exception& error) { ShowFileDialog(false); lv_label_set_text(fileStatus, error.what()); }
    }
    else if (action == "welcome") ProductWindows::Welcome(recentProjects, [this](std::string next){ MenuAction(next); });
    else if (action.rfind("template:", 0) == 0) toolTray->CreateBootstrapTemplate(action.substr(9));
    else if (action == "shortcuts") ProductWindows::Shortcuts();
    else if (action == "diagnostics") ProductWindows::Diagnostics(ProjectDocument());
    else if (action == "preferences") ShowPreferences();
    else if (action == "apply-preferences" && preferenceFields.size() == 5)
    {
        autosaveSeconds = lv_spinbox_get_value(preferenceFields[0]); dockLeft = lv_spinbox_get_value(preferenceFields[1]);
        dockHierarchy = lv_spinbox_get_value(preferenceFields[2]); dockInspector = lv_spinbox_get_value(preferenceFields[3]); dockBottom = lv_spinbox_get_value(preferenceFields[4]);
        preferenceFields.clear(); MenuAction("cancel-file"); LayoutWorkspace();
        try { FlushSession(); } catch (const std::exception& error) { sessionNotice = error.what(); }
    }
    else if (action == "export-protobuf") ShowFileDialog(true, true);
    else if (action == "export-c") ShowFileDialog(true, false, true);
    else if (action == "export-preflight" && exportTarget)
    {
        char target[16];lv_dropdown_get_selected_str(exportTarget, target, sizeof(target));
        const auto report = ProductWindows::Preflight(ProjectDocument(), target, std::filesystem::u8path(projectPath).parent_path().u8string());
        lv_label_set_text(fileStatus, report.c_str());
    }
    else if (action == "project-creator" || action == "project-creator-linux") ProjectCreatorWindow::Open(propertyWindow,[this]{return ProjectDocument();},std::filesystem::u8path(projectPath).parent_path().u8string(),action=="project-creator-linux"?"raspberry-pi-linux":"");
    else if (action == "language-model-generator") LanguageModelWindow::Open(propertyWindow);
    else if (action == "language-model-load") LanguageModelWindow::LoadTrie(propertyWindow);
    else if (action == "font-converter") FontConverterWindow::Open(propertyWindow);
    else if (action == "select-screen") propertyWindow->SetSelectedObject(simWindow->GetDrawSurface());
    else if (action == "center-canvas" || action == "fit-canvas") FitCanvas();
    else if (action == "zoom-in") ZoomCanvas(1.2, canvasWidth / 2.0, canvasHeight / 2.0);
    else if (action == "zoom-out") ZoomCanvas(1 / 1.2, canvasWidth / 2.0, canvasHeight / 2.0);
    else if (action == "zoom-reset") { ZoomCanvas(1 / canvasZoom, canvasWidth / 2.0, canvasHeight / 2.0); }
    else if (action == "image-manager") ImageManagerWindow::Open(propertyWindow);
    else if (action == "font-manager") FontManagerWindow::Open(propertyWindow);
    else if (action == "image-converter") ImageConverterWindow::Open(propertyWindow);
    else if (action == "duplicate") {try{DuplicateSelection();}catch(const std::exception& error){std::cerr<<"Duplicate: "<<error.what()<<'\n';}}
    else if (action.rfind("align-",0)==0) AlignSelection(action.substr(6));
    else if (action == "distribute-horizontal" || action == "distribute-vertical") DistributeSelection(action == "distribute-horizontal");
    else if (action == "delete") {FinishSelectionGesture();propertyWindow->DeleteSelectedObject();}
    else if (action == "exit") { closeApproved = true; SDL_Event event{}; event.type = SDL_EVENT_QUIT; SDL_PushEvent(&event); }
    else if (action == "cancel-file") { ProductWindows::Close();if (fileDialog) {EditorWindow::ReleaseFocus(fileDialog);lv_obj_delete(fileDialog);} fileDialog = nullptr; projectPathField = nullptr; exportTarget = nullptr; pendingAction.clear(); }
    else if (action == "save-pending")
    {
        const auto next = pendingAction; MenuAction("cancel-file"); pendingAction = next; ShowFileDialog(true);
    }
    else if (action == "discard-pending")
    {
        const auto next = pendingAction; MenuAction("cancel-file");
        if (session.Enabled()) try { session.RemoveRecovery(); } catch (const std::exception& error) { sessionNotice = error.what(); }
        allowDestructive = true; MenuAction(next); allowDestructive = false;
    }
    else if (action == "recovery-restore")
    {
        try
        {
            auto restored = Serialization::Project::FromJSON(recoveryDocument.at("project"));
            const auto path = recoveryDocument.value("path", std::string("project.json"));
            restoringHistory = true;
            try { InstallProject(std::move(restored)); restoringHistory = false; }
            catch (...) { restoringHistory = false; throw; }
            projectPath = path; savedDocument.clear(); history.Reset(CaptureHistory());
            MenuAction("cancel-file"); recoveryPending = false; recoveryDocument = json::object();
        }
        catch (const std::exception& error) { sessionNotice = error.what();if(fileDialog && fileStatus)lv_label_set_text(fileStatus,error.what()); }
    }
    else if (action == "recovery-discard" || action == "recovery-later")
    {
        MenuAction("cancel-file");
        if (action == "recovery-discard")
        {
            try { session.RemoveRecovery(); recoveryPending = false; recoveryDocument = json::object(); }
            catch (const std::exception& error) { sessionNotice = error.what(); }
        }
        else MenuAction("exit"); // Leave the recovery snapshot for the next launch.
    }
    else if (action == "confirm-file")
    {
        try
        {
            const std::filesystem::path path = std::filesystem::u8path(lv_textarea_get_text(projectPathField));
            if (saving)
            {
                auto document = ProjectDocument();
                if(exportingC||exportingBinary){
                    auto errors=ScreenValidation::BlockingErrors(document);if(!errors.empty())throw std::runtime_error(errors);
                    document=Serialization::ProjectScreens::Device(std::move(document));
                }
                if (exportingC)
                {
                    CCodeGenerator generator;
                    if (exportTarget) { char target[16];lv_dropdown_get_selected_str(exportTarget, target, sizeof(target));generator.SetTargetVersion(target); }
                    generator.SetSourceDirectory(std::filesystem::u8path(projectPath).parent_path().u8string());
                    if (!generator.LoadProject(document) || !generator.ExportProject(path.u8string()))
                        throw std::runtime_error(generator.GetLastError());
                    EditorWindow::ReleaseFocus(fileDialog);lv_obj_delete(fileDialog); fileDialog = nullptr;projectPathField = nullptr;exportTarget = nullptr;
                    ProductWindows::ExportSuccess(path.u8string(), true);
                    return;
                }
                auto temporary = path;
                temporary += ".uicreator-tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                try
                {
                    std::ofstream output(temporary, std::ios::binary);
                    if (!output) throw std::runtime_error("Cannot open project file for writing");
                    if (exportingBinary)
                    {
                        Serialization::ProjectDesign::Materialize(document, true);
                        if (exportTarget)
                        {
                            char target[16];lv_dropdown_get_selected_str(exportTarget, target, sizeof(target));
                            CGeneration::EmbedLocalImages(document, std::filesystem::u8path(projectPath).parent_path());
                            document["runtimeTarget"] = target;
                        }
                        const auto bytes = Serialization::Protobuf::Encode(Serialization::EntityKind::Project, "project", document);
                        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                    }
                    else {if(Serialization::ProjectScreens::Multiple(document)){document.erase("obj");document.erase("canvas");}output << document.dump(2);}
                    output.close();
                    if (!output) throw std::runtime_error("Could not finish writing the project file");
#ifdef _WIN32
                    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                        throw std::runtime_error("Could not replace the project file");
#else
                    std::filesystem::rename(temporary, path);
#endif
                }
                catch (...)
                {
                    std::error_code ignored;
                    std::filesystem::remove(temporary, ignored);
                    throw;
                }
            }
            else
            {
                std::ifstream input(path, std::ios::binary);
                if (!input) throw std::runtime_error("Cannot open project file");
                Serialization::LoadedProject restored;
                if (path.extension() == ".pb")
                {
                    const Serialization::Binary bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
                    restored = Serialization::Project::FromProtobuf(bytes);
                }
                else restored = Serialization::Project::FromJSON(json::parse(input));
                restoringHistory = true;
                try { InstallProject(std::move(restored)); restoringHistory = false; }
                catch (...) { restoringHistory = false; throw; }
                history.Reset(CaptureHistory());
            }
            if (!exportingBinary) { projectPath = lv_textarea_get_text(projectPathField); hasProjectPath=true;MarkSaved(); }
            const auto next = pendingAction;
            pendingAction.clear();
            EditorWindow::ReleaseFocus(fileDialog);lv_obj_delete(fileDialog); fileDialog = nullptr;projectPathField = nullptr;exportTarget = nullptr;
            if (exportingBinary) ProductWindows::ExportSuccess(path.u8string(), false);
            if (!next.empty()) MenuAction(next);
        }
        catch (const std::exception& error) { lv_label_set_text(fileStatus, error.what()); }
    }
    else if (action == "about") ProductWindows::About();
}
#else
void UI::MenuAction(const std::string&) {}
#endif

#pragma region ThemeInit
void UI::initializeThemes(uint16_t hue)
{
    themes.clear();
#if LVGL_VERSION_MAJOR >= 9
    themes.push_back(lv_theme_default_init(lv_display_get_default(), lv_color_hex(0x3589bd),
                                           lv_palette_main(LV_PALETTE_BLUE_GREY), true, LV_FONT_DEFAULT));
#else
#if LV_USE_THEME_NIGHT
    themes.push_back(lv_theme_night_init(hue, nullptr));
#endif

#if LV_USE_THEME_MATERIAL
    themes.push_back(lv_theme_material_init(hue, nullptr));
#endif

#if LV_USE_THEME_ALIEN
    themes.push_back(lv_theme_alien_init(hue, nullptr));
#endif

#if LV_USE_THEME_ZEN
    themes.push_back(lv_theme_zen_init(hue, nullptr));
#endif

#if LV_USE_THEME_NEMO
    themes.push_back(lv_theme_nemo_init(hue, nullptr));
#endif

#if LV_USE_THEME_MONO
    themes.push_back(lv_theme_mono_init(hue, nullptr));
#endif

#if LV_USE_THEME_DEFAULT
    themes.push_back(lv_theme_default_init(hue, nullptr));
#endif
#endif
}
#pragma endregion

#if LVGL_VERSION_MAJOR >= 9
void UI::CreateWorkspace()
{
    propertyWindow->GetObjectTree()->selectionHandler = [this](lv_obj_t* object,bool toggle){SelectWidget(object,toggle);};
    propertyWindow->GetObjectTree()->deleteSelectionHandler = [this]{MenuAction("delete");};
    canvasGrid.root = simWindow->GetDrawSurface();
    EditorGrid::active = &canvasGrid;
    lv_obj_add_event_cb(canvasGrid.root, EditorGrid::Draw, LV_EVENT_DRAW_MAIN_END, &canvasGrid);
    lv_obj_add_event_cb(canvasGrid.root, EditorGrid::Draw, LV_EVENT_DRAW_POST, &canvasGrid);
    lv_obj_add_event_cb(canvasGrid.root,[](lv_event_t* event){auto* ui=static_cast<UI*>(lv_event_get_user_data(event));if(!ui->restoringHistory&&!ui->applyingDocument&&!ui->serializingDocument&&(lv_event_get_code(event)==LV_EVENT_STYLE_CHANGED||lv_event_get_code(event)==LV_EVENT_SIZE_CHANGED)){ui->screenContentDirty=true;ui->InvalidateDocument();}},LV_EVENT_ALL,this);
    canvasViewport = lv_obj_create(lv_screen_active());
    propertyWindow->onSelectionChanged = [this]{lv_obj_invalidate(canvasViewport);};
    lv_obj_set_style_pad_all(canvasViewport, 0, 0);
    lv_obj_set_style_border_width(canvasViewport, 0, 0);
    lv_obj_set_style_radius(canvasViewport, 0, 0);
    lv_obj_set_style_bg_color(canvasViewport, lv_color_hex(0x202020), 0);
    lv_obj_set_scrollable(canvasViewport, false);
    lv_obj_set_parent(simWindow->GetWindow(), canvasViewport);
    EditorWindow::SetCloseHandler(simWindow->GetWindow(),[this]{FinishSelectionGesture();lv_obj_set_hidden(simWindow->GetWindow(),true);lv_obj_invalidate(canvasViewport);});
    lv_obj_add_event_cb(canvasViewport, [](lv_event_t* event){static_cast<UI*>(lv_event_get_user_data(event))->DrawSelection(event);}, LV_EVENT_DRAW_POST, this);
    lv_obj_set_style_transform_pivot_x(simWindow->GetWindow(), 0, 0);
    lv_obj_set_style_transform_pivot_y(simWindow->GetWindow(), 0, 0);
    uicreator_sdl3_set_event_handler([](const SDL_Event* event, void* data) {
        return static_cast<UI*>(data)->CanvasEvent(*event);
    }, this);
    screensDock = EditorWindow::Create(lv_screen_active());
    EditorWindow::SetTitle(screensDock, "Screens");
    auto* addScreen=EditorWindow::AddButton(screensDock,LV_SYMBOL_PLUS,"screen-add");
    lv_obj_add_event_cb(addScreen,[](lv_event_t* event){static_cast<UI*>(lv_event_get_user_data(event))->MenuAction("screen-add");},LV_EVENT_CLICKED,this);
    auto* flow=EditorWindow::AddButton(screensDock,LV_SYMBOL_LIST,"screen-flow");
    lv_obj_add_event_cb(flow,[](lv_event_t* event){static_cast<UI*>(lv_event_get_user_data(event))->MenuAction("screen-flow");},LV_EVENT_CLICKED,this);
    auto *content = EditorWindow::Content(screensDock);
    lv_obj_set_style_pad_all(content, 8, 0);
    auto *screen = MenuButton(content, "Screen", this, "select-screen");
    lv_obj_set_size(screen, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x32683e), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_align(screen, LV_TEXT_ALIGN_LEFT, 0);
    DeviceWindow::SetContext({[this]{return ProjectDocument();},[this](json value){ApplyDocument(std::move(value));},[this](json patch){ApplyMetadata(std::move(patch));}});
    auto* problemsTab=lv_tabview_add_tab(toolBar->GetTabView(),"Problems");lv_obj_set_style_pad_all(problemsTab,0,0);lv_obj_set_scrollable(problemsTab,false);
    ProblemsPanel::Context problems;
    problems.document=[this]{return ProjectDocument();};problems.apply=[this](json value){ApplyDocument(std::move(value));};problems.applyMetadata=[this](json patch){ApplyMetadata(std::move(patch));};problems.revision=[this]{return DocumentRevision();};problems.activeScreen=[this]{return ActiveScreenIndex();};
    problems.selectScreen=[this](size_t index){SelectScreen(index);};problems.selectObject=[this](size_t index,int oid){SelectObjectOnScreen(index,oid);};
    problemsDock=ProblemsPanel::Create(problemsTab,propertyWindow,std::move(problems));lv_obj_set_size(problemsDock,LV_PCT(100),LV_PCT(100));

    canvasToolbar = lv_obj_create(lv_screen_active());
    lv_obj_set_style_pad_all(canvasToolbar, 4, 0);
    lv_obj_set_style_pad_column(canvasToolbar, 6, 0);
    lv_obj_set_style_pad_row(canvasToolbar, 4, 0);
    lv_obj_set_style_border_width(canvasToolbar, 0, 0);
    lv_obj_set_style_radius(canvasToolbar, 8, 0);
    lv_obj_set_style_bg_color(canvasToolbar, lv_color_hex(0x202020), 0);
    lv_obj_set_scrollable(canvasToolbar, false);
    lv_obj_set_flex_flow(canvasToolbar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(canvasToolbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    undoButton = MenuButton(canvasToolbar, LV_SYMBOL_LEFT, this, "undo");
    redoButton = MenuButton(canvasToolbar, LV_SYMBOL_RIGHT, this, "redo");
    for (auto *button : {undoButton, redoButton})
        lv_obj_set_size(button, 30, 28);
    runButton = MenuButton(canvasToolbar, "Run (F6)", this, "run-toggle");
    lv_obj_set_size(runButton, 110, 28);
    workspaceStats = lv_label_create(canvasToolbar);
    lv_obj_set_size(workspaceStats,0,2*lv_font_get_line_height(lv_obj_get_style_text_font(workspaceStats,LV_PART_MAIN)));
    lv_obj_set_style_min_width(workspaceStats,0,0);
    lv_label_set_long_mode(workspaceStats,LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_flex_grow(workspaceStats, 1);
    lv_obj_set_style_text_color(workspaceStats, lv_color_hex(0xa9a9a9), 0);
    lv_obj_set_size(MenuButton(canvasToolbar, "-", this, "zoom-out"), 28, 28);
    auto* reset = MenuButton(canvasToolbar, "100%", this, "zoom-reset");
    lv_obj_set_size(reset, 72, 28);lv_obj_set_style_pad_all(reset,4,0);
    EditorIcons::Set(reset,"zoom-reset",false,16);zoomLabel = lv_obj_get_child(reset, 0);
    lv_obj_set_size(MenuButton(canvasToolbar, "+", this, "zoom-in"), 28, 28);
    lv_obj_set_size(MenuButton(canvasToolbar, "Fit", this, "fit-canvas"), 28, 28);

    auto* gridBar = lv_obj_create(canvasToolbar);
    lv_obj_set_size(gridBar, LV_PCT(100), 28);
    lv_obj_set_style_pad_all(gridBar, 0, 0);lv_obj_set_style_border_width(gridBar, 0, 0);
    lv_obj_set_style_bg_opa(gridBar, LV_OPA_TRANSP, 0);lv_obj_set_scrollable(gridBar, false);
    lv_obj_set_flex_flow(gridBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gridBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for(const auto& item : {std::make_pair("Grid", &canvasGrid.visible), std::make_pair("Snap", &canvasGrid.snapping), std::make_pair("Guides", &canvasGrid.guides)}) {
        auto* control = lv_checkbox_create(gridBar);lv_checkbox_set_text(control, item.first);
        gridControls.push_back(control);
        lv_obj_set_state(control, LV_STATE_CHECKED, *item.second);
        lv_obj_add_event_cb(control, [](lv_event_t* event) {
            auto* value = static_cast<bool*>(lv_event_get_user_data(event));
            *value = lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED);
            EditorGrid::Clear();
        }, LV_EVENT_VALUE_CHANGED, item.second);
    }
    auto* spacingLabel = lv_label_create(gridBar);lv_label_set_text(spacingLabel, "Spacing");
    auto* spacing = lv_spinbox_create(gridBar);lv_spinbox_set_range(spacing, 2, 128);
    gridControls.push_back(spacing);
    lv_spinbox_set_digit_format(spacing, 3, 0);lv_spinbox_set_value(spacing, canvasGrid.spacing);
    lv_obj_set_size(spacing, 60, 28);lv_obj_set_style_pad_all(spacing, 4, 0);
    lv_obj_add_event_cb(spacing, [](lv_event_t* event) {
        auto* state = static_cast<EditorGrid::State*>(lv_event_get_user_data(event));
        state->spacing = lv_spinbox_get_value(lv_event_get_target_obj(event));EditorGrid::Clear();
    }, LV_EVENT_VALUE_CHANGED, &canvasGrid);
    lv_obj_add_event_cb(spacing, [](lv_event_t* event) {
        auto* ui = static_cast<UI*>(lv_event_get_user_data(event));
        FocusEditorControl(ui->propertyWindow->GetKBGroup(), lv_event_get_target_obj(event));
    }, LV_EVENT_CLICKED, this);
    for(const auto& item : {std::make_pair("-", -1), std::make_pair("+", 1)}) {
        auto* button = lv_button_create(gridBar);lv_obj_set_size(button, 24, 26);lv_obj_set_style_pad_all(button, 0, 0);
        auto* label = lv_label_create(button);lv_label_set_text(label, item.first);lv_obj_center(label);
        EditorIcons::Set(button,item.second<0?"grid-decrease":"grid-increase",true,18);
        lv_obj_set_user_data(button, spacing);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto* field = static_cast<lv_obj_t*>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
            lv_spinbox_set_value(field, lv_spinbox_get_value(field) + (int)(intptr_t)lv_event_get_user_data(event));
            lv_obj_send_event(field, LV_EVENT_VALUE_CHANGED, nullptr);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)item.second);
    }

    auto *assets = toolBar->GetAssetTab();
    lv_obj_set_flex_flow(assets, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(assets, 12, 0);
    lv_obj_set_style_pad_column(assets, 12, 0);
    for (const auto &item : {std::make_pair("Project images", "image-manager"),
                             std::make_pair("Project fonts", "font-manager")})
    {
        auto *button = MenuButton(assets, item.first, this, item.second);
        lv_obj_set_size(button, 156, 70);
        lv_obj_set_style_max_width(button,LV_PCT(100),0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x484848), 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    }
    lv_label_set_text(workspaceStats, "Screens: 1\nWidgets: 0");
    LayoutWorkspace();
    workspaceTimer = lv_timer_create(
        [](lv_timer_t *timer) {
            auto *ui = static_cast<UI *>(lv_timer_get_user_data(timer));
            const int w = lv_display_get_horizontal_resolution(nullptr);
            const int h = lv_display_get_vertical_resolution(nullptr);
            if (w != ui->workspaceWidth || h != ui->workspaceHeight)
                ui->LayoutWorkspace();
            if(ui->canvasFitted)ui->FitCanvas();
            ui->SessionTick();
            lv_label_set_text_fmt(ui->workspaceStats, "Screens: %u  Widgets: %u\nSelected: %u",
                                  (unsigned)ui->ScreenCount(),(unsigned)ui->propertyWindow->GetObjectTree()->GetObjects().size(), (unsigned)ui->SelectionCount());
            lv_obj_set_state(ui->undoButton, LV_STATE_DISABLED, ui->Running() || !ui->CanUndo());
            lv_obj_set_state(ui->redoButton, LV_STATE_DISABLED, ui->Running() || !ui->CanRedo());
            lv_label_set_text(lv_obj_get_child(ui->runButton,0),ui->Running()?"Design":"Run (F6)");
            EditorIcons::Set(ui->runButton,ui->Running()?"run-stop":"run-toggle");
        },
        200, this);
}

void UI::LayoutWorkspace()
{
    if (menuPopup) CloseMenu();
    workspaceWidth = lv_display_get_horizontal_resolution(nullptr);
    workspaceHeight = lv_display_get_vertical_resolution(nullptr);
    const int left = dockLeft ? std::clamp(dockLeft, 180, 460) : std::clamp(workspaceWidth / 5, 280, 380);
    const int hierarchy = dockHierarchy ? std::clamp(dockHierarchy, 160, 380) : std::clamp(workspaceWidth / 6, 190, 330);
    const int inspector = dockInspector ? std::clamp(dockInspector, 280, 500) : std::clamp(workspaceWidth / 6, 340, 420);
    const int centerX = left + hierarchy + 4;
    const int centerWidth = std::max(180, workspaceWidth - centerX - inspector - 4);
    const int bottomHeight = dockBottom ? std::clamp(dockBottom, 140, 380) : std::clamp(workspaceHeight / 4, 220, 300);
    const int screenHeight = std::clamp(workspaceHeight / 3, 180, 340);
    auto dock = [](lv_obj_t *object, int x, int y, int w, int h) {
        lv_obj_set_pos(object, x, y);
        lv_obj_set_size(object, w, h);
    };
    dock(screensDock, 0, 38, left, screenHeight);
    dock(toolTray->GetWindow(), 0, 40 + screenHeight, left, workspaceHeight - screenHeight - 40);
    dock(propertyWindow->GetObjectTree()->GetTreeViewUI(), left + 2, 38, hierarchy, workspaceHeight - 38);
    dock(lv_obj_get_parent(propertyWindow->GetWindow()), workspaceWidth - inspector, 38, inspector,
         workspaceHeight - 38);
    propertyWindow->GetCWM()->SetWidth(inspector - 20);
    dock(canvasToolbar, centerX, 42, centerWidth, 76);
    dock(toolBar->GetWindow(), centerX, workspaceHeight - bottomHeight, centerWidth, bottomHeight);
    dock(canvasViewport, centerX, 122, centerWidth, std::max(1, workspaceHeight - bottomHeight - 122));
    lv_obj_update_layout(canvasViewport);
    const int oldWidth = canvasWidth, oldHeight = canvasHeight;
    canvasWidth = lv_obj_get_content_width(canvasViewport);
    canvasHeight = lv_obj_get_content_height(canvasViewport);
    if(canvasFitted) FitCanvas();
    else {
        if(oldWidth == 0) {
            canvasX = (canvasWidth - lv_obj_get_width(simWindow->GetWindow())) / 2.0;
            canvasY = (canvasHeight - lv_obj_get_height(simWindow->GetWindow())) / 2.0;
        } else { canvasX += (canvasWidth - oldWidth) / 2.0; canvasY += (canvasHeight - oldHeight) / 2.0; }
        UpdateCanvas();
    }
}
#endif

#if LVGL_VERSION_MAJOR >= 9
void UI::UpdateCanvas()
{
    auto *window = simWindow->GetWindow();
    lv_obj_set_style_transform_scale(window, (int)std::lround(canvasZoom * 256), 0);
    lv_obj_set_pos(window, (int)std::lround(canvasX), (int)std::lround(canvasY));
    lv_label_set_text_fmt(zoomLabel, "%d%%", (int)std::lround(canvasZoom * 100));
}
void UI::ZoomCanvas(double factor, double x, double y)
{
    const double next = std::clamp(canvasZoom * factor, 26 / 256.0, 4.0);
    // Retain fractional wheel input, but anchor using LVGL's actual fixed-point scale.
    const double ratio = std::round(next * 256) / std::round(canvasZoom * 256);
    canvasX = x - (x - canvasX) * ratio;
    canvasY = y - (y - canvasY) * ratio;
    canvasZoom = next;
    canvasFitted = false;
    UpdateCanvas();
}
void UI::FitCanvas()
{
    auto *window = simWindow->GetWindow();
    lv_obj_update_layout(window);
    canvasZoom = std::clamp(std::floor(std::min((canvasWidth - 32.0) / lv_obj_get_width(window),
                                                (canvasHeight - 32.0) / lv_obj_get_height(window)) *
                                       256) /
                                256,
                            26 / 256.0, 4.0);
    canvasX = (canvasWidth - lv_obj_get_width(window) * canvasZoom) / 2;
    canvasY = (canvasHeight - lv_obj_get_height(window) * canvasZoom) / 2;
    canvasFitted = true;
    UpdateCanvas();
}
bool UI::CanvasEvent(const SDL_Event &event)
{
    if(CommandPalette::HandleEvent(event))return true;
    if(!menuPopup&&!fileDialog&&!Running()&&PreviewProfiles::HandleEvent(event))return true;
    if (event.type == SDL_EVENT_KEY_DOWN) swallowMenuText = false;
    if ((event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING) && (menuPopup || swallowMenuText)) {
        swallowMenuText = false; return true;
    }
    if (menuPopup && event.type == SDL_EVENT_KEY_DOWN) {
        int key = static_cast<int>(event.key.key);
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        HandleShortcut(key, (event.key.mod & SDL_KMOD_CTRL) != 0, (event.key.mod & SDL_KMOD_SHIFT) != 0, (event.key.mod & SDL_KMOD_ALT) != 0);
        return true;
    }
    if (menuPopup && event.type == SDL_EVENT_WINDOW_FOCUS_LOST) CloseMenu();
    if (!menuPopup && !Running()) {
        auto* group = EditorKeyboardGroup();
        auto* field = group ? lv_group_get_focused(group) : nullptr;
        if (field && IsEditorInputControl(field) && !IsEditorControlAvailable(field) &&
            (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_TEXT_INPUT)) {
            lv_group_remove_all_objs(group);
            if (event.type == SDL_EVENT_TEXT_INPUT || !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) ||
                event.key.key == SDLK_A || event.key.key == SDLK_C || event.key.key == SDLK_X || event.key.key == SDLK_V)
                return true;
        }
        if (EditorSpinbox::HandleEvent(event, group)) return true;
        // LVGL moves the caret for Home/End but keeps an old mouse selection.
        // Clear it first so the next Delete/Backspace edits at the new caret.
        if(event.type==SDL_EVENT_KEY_DOWN && !(event.key.mod & SDL_KMOD_SHIFT) &&
            (event.key.key==SDLK_HOME || event.key.key==SDLK_END))
            ClearEditorTextSelection(group);
        if (event.type == SDL_EVENT_TEXT_INPUT && uicreator_sdl3_text_input_allowed() &&
            ReplaceEditorTextSelection(group, event.text.text)) return true;
        if (event.type == SDL_EVENT_KEY_DOWN &&
            (event.key.key == SDLK_DELETE || event.key.key == SDLK_BACKSPACE) &&
            DeleteEditorTextSelection(group)) return true;
    }
    if(Running() && !menuPopup) {
        if(event.type==SDL_EVENT_KEY_DOWN && !(event.key.mod&(SDL_KMOD_RALT|SDL_KMOD_MODE)) && HandleShortcut(static_cast<int>(event.key.key),(event.key.mod&SDL_KMOD_CTRL)!=0,(event.key.mod&SDL_KMOD_SHIFT)!=0,(event.key.mod&SDL_KMOD_ALT)!=0))return true;
        if(runPreview.HandleEvent(event))return true;
    }
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        if (!closeApproved && GuardDestructive("exit")) return true;
        closeApproved = true;
        return false;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && session.Enabled() && !editDepth)
        try { FlushSession(); } catch (const std::exception& error) { sessionNotice = error.what(); }
    if(SelectionEvent(event))return true;
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        canvasPanning = false;
        SDL_CaptureMouse(false);
        return false;
    }
    const bool modal = fileDialog || menuPopup || lv_obj_get_child_count(lv_layer_top());
    if (modal && canvasPanning)
    {
        canvasPanning = false;
        SDL_CaptureMouse(false);
    }
    if (canvasPanning)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            canvasX += event.motion.x - panLastX;
            canvasY += event.motion.y - panLastY;
            panLastX = event.motion.x;
            panLastY = event.motion.y;
            canvasFitted = false;
            UpdateCanvas();
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == panButton)
        {
            canvasPanning = false;
            SDL_CaptureMouse(false);
            return true;
        }
    }
    if (modal)
        return false;
    if (event.type != SDL_EVENT_MOUSE_WHEEL && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
        return false;
    const float x = event.type == SDL_EVENT_MOUSE_WHEEL ? event.wheel.mouse_x : event.button.x;
    const float y = event.type == SDL_EVENT_MOUSE_WHEEL ? event.wheel.mouse_y : event.button.y;
    lv_area_t bounds;
    lv_obj_get_coords(canvasViewport, &bounds);
    if (x < bounds.x1 || x > bounds.x2 || y < bounds.y1 || y > bounds.y2)
        return false;
    lv_point_t point{(int)x, (int)y};
    auto* hit=CanvasHit(canvasViewport,point);
    if(!hit)return false;
    if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        const double delta = event.wheel.y * (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1);
        if (delta)
            ZoomCanvas(std::pow(1.2, std::clamp(delta, -20.0, 20.0)), x - bounds.x1, y - bounds.y1);
        return true;
    }
    if (event.button.button == SDL_BUTTON_MIDDLE ||
        (event.button.button == SDL_BUTTON_LEFT &&
         (hit == EditorWindow::Header(simWindow->GetWindow()))))
    {
        canvasPanning = true;
        panButton = event.button.button;
        panLastX = x;
        panLastY = y;
        SDL_CaptureMouse(true);
        return true;
    }
    return false;
}
#endif

#if LVGL_VERSION_MAJOR >= 9
namespace
{
lv_area_t SelectionBounds(lv_obj_t* object, bool transformed)
{
    lv_area_t area;lv_obj_get_coords(object,&area);
    if(transformed)lv_obj_get_transformed_area(object,&area,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
    return area;
}
bool InSelection(lv_obj_t* object,const std::vector<lv_obj_t*>& objects)
{
    return std::find(objects.begin(),objects.end(),object)!=objects.end();
}
} // namespace
void UI::SelectWidget(lv_obj_t *object, bool toggle)
{
    auto selected = propertyWindow->SelectedObjects();
    if (!object || object == simWindow->GetDrawSurface())
        selected.clear();
    else if (toggle)
    {
        auto found = std::find(selected.begin(), selected.end(), object);
        if (found == selected.end())
            selected.push_back(object);
        else
            selected.erase(found);
    }
    else
        selected = {object};
    propertyWindow->SetSelection(selected);
    lv_obj_invalidate(canvasViewport);
}
void UI::FinishSelectionGesture()
{
    const bool moved = groupMoved;
    groupActive = groupMoved = marqueeActive = selectionClick = false;
    groupItems.clear();
    EditorGrid::Clear();
    SDL_CaptureMouse(false);
    if (moved)
    {
        const auto selected = propertyWindow->SelectedObjects();
        propertyWindow->SetSelection(selected);
        EndEdit();
    }
    lv_obj_invalidate(canvasViewport);
}
void UI::AlignSelection(const std::string &alignment)
{
    FinishSelectionGesture();
    const auto roots = propertyWindow->SelectionRoots();
    if (roots.size() < 2)
        return;
    EditorEdit::Scope edit;
    lv_obj_update_layout(simWindow->GetDrawSurface());
    auto bounds = SelectionBounds(roots.front(), false);
    for (auto *object : roots)
    {
        auto area = SelectionBounds(object, false);
        bounds.x1 = (std::min)(bounds.x1, area.x1);
        bounds.x2 = (std::max)(bounds.x2, area.x2);
        bounds.y1 = (std::min)(bounds.y1, area.y1);
        bounds.y2 = (std::max)(bounds.y2, area.y2);
    }
    for (auto *object : roots)
    {
        const auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        if (lv_obj_is_layout_positioned(object) || (data && !data->objectJson.value("base", json::object()).value("drag", true))) continue;
        auto area = SelectionBounds(object, false);
        int dx = 0, dy = 0;
        if (alignment == "left")
            dx = bounds.x1 - area.x1;
        else if (alignment == "right")
            dx = bounds.x2 - area.x2;
        else if (alignment == "hcenter")
            dx = (bounds.x1 + bounds.x2 - area.x1 - area.x2) / 2;
        else if (alignment == "top")
            dy = bounds.y1 - area.y1;
        else if (alignment == "bottom")
            dy = bounds.y2 - area.y2;
        else if (alignment == "vcenter")
            dy = (bounds.y1 + bounds.y2 - area.y1 - area.y2) / 2;
        const int direction = data ? data->objectJson.value("base", json::object()).value("dragDir", 3) : 3;
        if (!(direction & 1)) dx = 0;
        if (!(direction & 2)) dy = 0;
        lv_obj_set_pos(object, lv_obj_get_x(object) + dx, lv_obj_get_y(object) + dy);
    }
    const auto selected = propertyWindow->SelectedObjects();
    propertyWindow->SetSelection(selected);
}
void UI::NudgeSelection(int x, int y)
{
    FinishSelectionGesture();
    EditorEdit::Scope edit;
    for (auto* object : propertyWindow->SelectionRoots())
    {
        auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        const auto base = data ? data->objectJson.value("base", json::object()) : json::object();
        if (lv_obj_is_layout_positioned(object) || !base.value("drag", true)) continue;
        const auto direction = base.value("dragDir", 3);
        lv_obj_set_pos(object, lv_obj_get_x(object) + ((direction & 1) ? x : 0), lv_obj_get_y(object) + ((direction & 2) ? y : 0));
    }
    const auto selected = propertyWindow->SelectedObjects(); propertyWindow->SetSelection(selected);
}
void UI::DistributeSelection(bool horizontal)
{
    FinishSelectionGesture();
    auto roots = propertyWindow->SelectionRoots();
    if (roots.size() < 3) return;
    // Distributing siblings gives predictable gaps independent of nested
    // layout transforms. Locked objects are anchors and are never moved.
    auto* parent = lv_obj_get_parent(roots.front());
    for (auto* object : roots)
    {
        if (lv_obj_get_parent(object) != parent || lv_obj_is_layout_positioned(object)) return;
        const auto* data = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
        const auto base = data ? data->objectJson.value("base", json::object()) : json::object();
        if (!base.value("drag", true) || !(base.value("dragDir", 3) & (horizontal ? 1 : 2))) return;
    }
    lv_obj_update_layout(parent);
    auto start = [horizontal](lv_obj_t* object) { return horizontal ? lv_obj_get_x(object) : lv_obj_get_y(object); };
    auto extent = [horizontal](lv_obj_t* object) { return horizontal ? lv_obj_get_width(object) : lv_obj_get_height(object); };
    std::stable_sort(roots.begin(), roots.end(), [&](lv_obj_t* a, lv_obj_t* b) { return start(a) < start(b); });
    int occupied = 0; for (auto* object : roots) occupied += extent(object);
    const double gap = double(start(roots.back()) + extent(roots.back()) - start(roots.front()) - occupied) / (roots.size() - 1);
    EditorEdit::Scope edit;
    double position = start(roots.front()) + extent(roots.front()) + gap;
    for (size_t i = 1; i + 1 < roots.size(); ++i)
    {
        if (horizontal) lv_obj_set_x(roots[i], (int)std::lround(position));
        else lv_obj_set_y(roots[i], (int)std::lround(position));
        position += extent(roots[i]) + gap;
    }
    const auto selected = propertyWindow->SelectedObjects(); propertyWindow->SetSelection(selected);
}
void UI::DuplicateSelection()
{
    FinishSelectionGesture();
    const auto roots = propertyWindow->SelectionRoots();
    if (roots.empty())
        return;
    EditorEdit::Scope edit;
    auto document = ProjectDocument();
    const auto objects = propertyWindow->GetObjectTree()->GetObjects();
    std::vector<int> originals, rootIndices;
    for (size_t i = 0; i < objects.size(); ++i)
    {
        if (InSelection(objects[i], roots))
            rootIndices.push_back((int)i);
        for (auto *object = objects[i]; object; object = lv_obj_get_parent(object))
            if (InSelection(object, roots))
            {
                originals.push_back((int)i);
                break;
            }
    }
    std::unordered_map<int, int> remap;
    const int start = (int)objects.size();
    for (size_t i = 0; i < originals.size(); ++i)
        remap[originals[i]] = start + (int)i;
    std::unordered_set<std::string> names;
    for (const auto &record : document["obj"])
        names.insert(record["base"].value("name", std::string()));
    const int offset = canvasGrid.snapping ? canvasGrid.spacing : 16;
    json cloneRecords = json::array();
    for (int index : originals)
    {
        auto copy = document["obj"][index];
        copy["oid"] = remap.at(index);
        const int parent = copy.value("par", -1);
        if (remap.count(parent))
            copy["par"] = remap.at(parent);
        if(copy.contains("advancedKeyboard")) {
            const int target=copy["advancedKeyboard"].value("targetOid",-1);
            if(remap.count(target))copy["advancedKeyboard"]["targetOid"]=remap.at(target);
        }
        if(copy.contains("base") && copy["base"].contains("events")) {
            for(auto& event : copy["base"]["events"]) {
                const int target = event.value("targetOid", -1);
                if(!event.contains("targetScreen") && remap.count(target)) event["targetOid"] = remap.at(target);
            }
        }
        if (std::find(rootIndices.begin(), rootIndices.end(), index) != rootIndices.end())
        {
            copy["base"]["coords"]["x"] = lv_obj_get_x(objects[index]) + offset;
            copy["base"]["coords"]["y"] = lv_obj_get_y(objects[index]) + offset;
            copy.erase("hostIndex");
        }
        const auto base = copy["base"].value("name", std::string("Widget")) + " copy";
        auto name = base;
        for (int suffix = 2; names.count(name); ++suffix)
            name = base + " " + std::to_string(suffix);
        names.insert(name);
        copy["base"]["name"] = name;
        cloneRecords.push_back(std::move(copy));
    }
    Serialization::ProjectDesign::CloneDesignLinks(cloneRecords, document, std::map<int,int>(remap.begin(), remap.end()));
    for(auto& copy : cloneRecords) document["obj"].push_back(std::move(copy));
    auto loaded = Serialization::Project::FromJSON(document);
    std::vector<lv_obj_t *> clones;
    for(auto* root:roots) {
        const int index=(int)std::distance(objects.begin(),std::find(objects.begin(),objects.end(),root));
        clones.push_back(loaded.objects.at(remap.at(index)));
    }
    InstallProject(std::move(loaded));
    propertyWindow->SetSelection(clones);
}
void UI::DrawSelection(lv_event_t *event)
{
    lv_draw_rect_dsc_t rectangle;
    lv_draw_rect_dsc_init(&rectangle);
    rectangle.bg_opa = LV_OPA_TRANSP;
    rectangle.border_color = lv_color_hex(0x36c9f1);
    rectangle.border_width = 1;
    rectangle.border_opa = LV_OPA_COVER;
    lv_area_t bounds{};
    bool have = false;
    for (auto *object : propertyWindow->SelectedObjects())
    {
        if(!lv_obj_is_visible(object))continue;
        auto area = SelectionBounds(object, true);
        if (!have)
        {
            bounds = area;
            have = true;
        }
        else
        {
            bounds.x1 = (std::min)(bounds.x1, area.x1);
            bounds.x2 = (std::max)(bounds.x2, area.x2);
            bounds.y1 = (std::min)(bounds.y1, area.y1);
            bounds.y2 = (std::max)(bounds.y2, area.y2);
        }
    }
    if (have && SelectionCount() > 1)
        lv_draw_rect(lv_event_get_layer(event), &rectangle, &bounds);
    if (marqueeActive)
    {
        bounds = {(int)(std::min)(gestureX, gestureLastX), (int)(std::min)(gestureY, gestureLastY),
                  (int)(std::max)(gestureX, gestureLastX), (int)(std::max)(gestureY, gestureLastY)};
        rectangle.bg_color = rectangle.border_color;
        rectangle.bg_opa = LV_OPA_10;
        lv_draw_rect(lv_event_get_layer(event), &rectangle, &bounds);
    }
}
bool UI::SelectionEvent(const SDL_Event &event)
{
    const bool modal = fileDialog || menuPopup || lv_obj_get_child_count(lv_layer_top());
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || modal)
    {
        if (groupActive || marqueeActive || selectionClick)
            FinishSelectionGesture();
        return false;
    }
    if (selectionClick)
    {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
        {
            selectionClick = false;
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
            return true;
    }
    if (groupActive || marqueeActive)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            gestureLastX = event.motion.x;
            gestureLastY = event.motion.y;
            if (groupActive)
            {
                float dx = gestureLastX - gestureX, dy = gestureLastY - gestureY;
                if (!groupMoved && std::abs(dx) < 4 && std::abs(dy) < 4)
                    return true;
                if (!groupMoved)
                {
                    BeginEdit(this, false);
                    groupMoved = true;
                }
                auto &anchor = groupItems[groupAnchor];
                if (!lv_obj_is_valid(anchor.object))
                {
                    FinishSelectionGesture();
                    return true;
                }
                int x = anchor.x + (int)std::lround(dx / anchor.scaleX),
                    y = anchor.y + (int)std::lround(dy / anchor.scaleY);
                if (!groupAllowX)
                    x = anchor.x;
                if (!groupAllowY)
                    y = anchor.y;
                canvasGrid.ignored.clear();
                for (const auto &item : groupItems)
                    canvasGrid.ignored.push_back(item.object);
                EditorGrid::Move(anchor.object, x, y, groupAllowX, groupAllowY);
                const double physicalX = (x - anchor.x) * anchor.scaleX,
                             physicalY = (y - anchor.y) * anchor.scaleY;
                for (const auto &item : groupItems)
                    if (lv_obj_is_valid(item.object))
                        lv_obj_set_pos(item.object, item.x + (int)std::lround(physicalX / item.scaleX),
                                       item.y + (int)std::lround(physicalY / item.scaleY));
            }
            lv_obj_invalidate(canvasViewport);
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
        {
            if (marqueeActive)
            {
                gestureLastX = event.button.x;
                gestureLastY = event.button.y;
                lv_area_t box{
                    (int)(std::min)(gestureX, gestureLastX), (int)(std::min)(gestureY, gestureLastY),
                    (int)(std::max)(gestureX, gestureLastX), (int)(std::max)(gestureY, gestureLastY)};
                auto selected = marqueeBase;
                if (std::abs(gestureLastX - gestureX) >= 4 || std::abs(gestureLastY - gestureY) >= 4)
                    for (auto *object : propertyWindow->GetObjectTree()->GetObjects())
                        if (lv_obj_is_visible(object))
                        {
                            auto area = SelectionBounds(object, true);
                            if (area.x1 <= box.x2 && area.x2 >= box.x1 && area.y1 <= box.y2 &&
                                area.y2 >= box.y1 && !InSelection(object, selected))
                                selected.push_back(object);
                        }
                propertyWindow->SetSelection(selected);
            }
            FinishSelectionGesture();
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
            return true;
    }
    if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || event.button.button != SDL_BUTTON_LEFT || canvasPanning)
        return false;
    lv_area_t bounds;
    lv_obj_get_coords(canvasViewport, &bounds);
    if (event.button.x < bounds.x1 || event.button.x > bounds.x2 || event.button.y < bounds.y1 ||
        event.button.y > bounds.y2)
        return false;
    lv_point_t point{(int)event.button.x, (int)event.button.y};
    auto* hit=CanvasHit(canvasViewport,point);
    if(!hit)return false;
    auto *object = hit;
    const auto available = propertyWindow->GetObjectTree()->GetObjects();
    while (object && !InSelection(object, available) && object != simWindow->GetDrawSurface())
        object = lv_obj_get_parent(object);
    const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    if (object && object != simWindow->GetDrawSurface() && InSelection(object, available))
    {
        if (shift)
        {
            SelectWidget(object, true);
            selectionClick = true;
            return true;
        }
        if (SelectionCount() < 2 || !InSelection(object, propertyWindow->SelectedObjects()))
            return false;
        groupItems.clear();
        groupAnchor = 0;
        groupAllowX = groupAllowY = true;
        for (auto *root : propertyWindow->SelectionRoots())
        {
            auto *data = static_cast<ObjectUserData *>(lv_obj_get_user_data(root));
            if (data && !data->objectJson.value("base", json::object()).value("drag", true))
                continue;
            double sx = 1, sy = 1;
            for (auto *parent = lv_obj_get_parent(root); parent; parent = lv_obj_get_parent(parent))
            {
                sx *= lv_obj_get_style_transform_scale_x(parent, LV_PART_MAIN) / 256.0;
                sy *= lv_obj_get_style_transform_scale_y(parent, LV_PART_MAIN) / 256.0;
            }
            if (sx <= 0 || sy <= 0)
                continue;
            for (auto *ancestor = object; ancestor; ancestor = lv_obj_get_parent(ancestor))
                if (ancestor == root)
                    groupAnchor = groupItems.size();
            groupItems.push_back({root, lv_obj_get_x(root), lv_obj_get_y(root), sx, sy});
            const int direction =
                data ? data->objectJson.value("base", json::object()).value("dragDir", 3) : 3;
            groupAllowX = groupAllowX && (direction & 1);
            groupAllowY = groupAllowY && (direction & 2);
        }
        if (groupItems.empty())
            return false;
        groupActive = true;
    }
    else
    {
        if (hit != canvasViewport && object != simWindow->GetDrawSurface())
            return false;
        marqueeBase = shift ? propertyWindow->SelectedObjects() : std::vector<lv_obj_t *>{};
        if (!shift)
            propertyWindow->SetSelection({});
        marqueeActive = true;
    }
    gestureX = gestureLastX = event.button.x;
    gestureY = gestureLastY = event.button.y;
    SDL_CaptureMouse(true);
    return true;
}
#endif
