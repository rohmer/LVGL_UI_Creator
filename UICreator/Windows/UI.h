#pragma once
#include <string>
#include <SDL3/SDL.h>
#include <vector>
#include <map>
#include "lvgl/lvgl.h"
#include "PropertyWindow.h"
#include <Serialization/Project.h>
#include "ToolTray.h"
#include "SimWindow.h"
#include "ToolBar.h"
#include "../Fonts/FTFont.h"
#include "Themes.h"
#include "../Utils/EditorEdit.h"
#include "../Utils/EditorGrid.h"
#include "../Utils/UndoHistory.h"
#include "../Utils/EditorSession.h"
#include "../Utils/DebugPreview.h"
#include "../Utils/RunPreview.h"
#include "../Widgets/ColorPicker.h"
#ifdef _WINDOWS
#include <spdlog/spdlog.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#else
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

class UI : public EditorEdit::Observer
{
public:
    UI(lv_indev_t* kb_indev);
    ~UI();
    size_t SelectionCount() const { return propertyWindow->SelectedObjects().size(); }
    bool Undo();
    bool Redo();
    bool CanUndo() const { return history.CanUndo(); }
    bool CanRedo() const { return history.CanRedo(); }
    json ProjectDocument() const;
    void ApplyDocument(json document);
    void ApplyMetadata(json patch);
    uint64_t DocumentRevision() const { return documentRevision; }
    uint64_t SerializationCount() const { return serializationCount; }
    size_t HistoryStorageBytes() const {return history.StorageBytes();}
    size_t HistoryLogicalBytes() const {return history.LogicalBytes();}
    size_t ScreenCount() const { return screenRecords.empty()?1:screenRecords.size(); }
    size_t ActiveScreenIndex() const { return editingScreen; }
    bool SelectScreen(size_t screen);
    void SelectObjectOnScreen(size_t screen,int oid);
    size_t AddScreen(const std::string& name="");
    size_t DuplicateScreen(size_t screen);
    void RenameScreen(size_t screen,const std::string& name);
    void MoveScreen(size_t screen,size_t target);
    void DeleteScreen(size_t screen);
    void SetStartScreen(size_t screen);
    std::vector<int> SelectedObjectIds() const;
    void SelectObjectIds(const std::vector<int>& ids);
    bool Running() const { return runPreview.Running(); }
    ToolBar* GetToolBar() const { return toolBar; }
    lvgl_runtime_t* RunRuntime() const { return runPreview.Runtime(); }
    lv_obj_t* RunImage() const { return runPreview.Image(); }
    bool HandleShortcut(int key, bool control, bool shift = false, bool alt = false);
    void MenuAction(const std::string& action);
    bool Dirty() const;
    void EnableSessionPersistence(const std::filesystem::path& directory = EditorSession::DefaultDirectory());
    void FlushSession();
    const std::vector<std::string>& RecentProjects() const { return recentProjects; }
    bool DebugRunning() const { return debugPreview.Running(); }
    std::string DebugSnapshotPath() const { return debugPreview.SnapshotPath(); }

private:
    UndoHistory history;
    bool restoringHistory = false;
    bool restoringScreenFlow = false;
    bool applyingDocument = false;
    bool switchingScreen = false;
    mutable bool serializingDocument=false;
    bool screenContentDirty=true;
    unsigned editDepth = 0;
    const void* editKey = nullptr;
    bool editCoalesces = false;
    UndoHistory::State CaptureHistory() const;
    void RestoreHistory(const UndoHistory::State& state);
    void InstallProject(Serialization::LoadedProject restored, int selection = -1);
    void BeginEdit(const void* key, bool coalesce) override;
    void EndEdit() noexcept override;
    void InvalidateDocument();
    json BuildProjectDocument() const;
    const json& CachedDocument() const;
    const std::string& SerializedDocument() const;
    void RefreshScreensDock();
    void RenameVariableReferences(const std::string& oldName, const std::string& newName);
    void SaveProjectTo(const std::filesystem::path& path);
    void OpenCommandPalette(bool properties=false);
    lv_theme_t* activeTheme;
    std::vector<lv_theme_t *> themes;
    void initializeThemes(uint16_t hue);

    PropertyWindow* propertyWindow;
    ToolTray* toolTray;
    SimWindow* simWindow;
    ToolBar* toolBar;
#if LVGL_VERSION_MAJOR >= 9
    EditorSession session;
    DebugPreview debugPreview;
    RunPreview runPreview;
    json projectMetadata = json::object();
    json screenRecords=json::array();
    size_t editingScreen=0,startScreen=0;
    uint64_t documentRevision=1;
    mutable uint64_t cachedRevision=0,serializedRevision=0,serializationCount=0;
    mutable json cachedDocument;
    mutable std::string cachedSerialized;
    bool hasProjectPath=false;
    lv_obj_t* problemsDock=nullptr;
    std::string savedDocument, lastAutosaveDocument, sessionNotice;
    std::vector<std::string> recentProjects;
    json recoveryDocument;
    uint64_t lastSessionTick = 0, lastAutosaveTick = 0;
    unsigned autosaveSeconds = 30;
    bool closeApproved = false, allowDestructive = false, recoveryPending = false, protectWork = false;
    std::string pendingAction;
    lv_obj_t* documentTitle = nullptr;
    std::map<std::string, lv_obj_t*> menuButtons;
    struct MenuItem {
        std::string label, action, shortcut, submenu, detail;
        bool enabled = true;
    };
    struct MenuFrame {
        lv_obj_t* panel = nullptr;
        lv_obj_t* anchor = nullptr;
        std::vector<lv_obj_t*> rows;
        std::vector<std::string> submenus, labels;
        int focused = -1;
    };
    std::vector<MenuFrame> menuFrames;
    std::string menuName;
    bool swallowMenuText = false;
    std::vector<MenuItem> MenuItems(const std::string& name) const;
    void ShowMenuPanel(const std::string& name, int x, int y, size_t depth, lv_obj_t* anchor = nullptr);
    void TrimMenus(size_t depth);
    void FocusMenuRow(size_t depth, int index);
    void OpenMenuChild(size_t depth, int index);
    void CloseMenu();
    bool HandleMenuKey(int key, bool control, bool shift);
    int dockLeft = 0, dockHierarchy = 0, dockInspector = 0, dockBottom = 0;
    std::vector<lv_obj_t*> gridControls;
    std::vector<lv_obj_t*> preferenceFields;
    lv_obj_t* exportTarget = nullptr;
    void SessionTick();
    void MarkSaved(bool remember = true);
    void RememberProject();
    void OpenProjectPath(const std::string& path);
    bool GuardDestructive(const std::string& action);
    void ShowSessionPrompt(bool recovery);
    void ShowPreferences();
    void NudgeSelection(int x, int y);
    void DistributeSelection(bool horizontal);
    void CreateMenuBar();
    void CreateWorkspace();
    void UpdateCanvas();
    void FitCanvas();
    void ZoomCanvas(double factor, double x, double y);
    bool CanvasEvent(const SDL_Event& event);
    void SelectWidget(lv_obj_t* object, bool toggle);
    void DuplicateSelection();
    void AlignSelection(const std::string& alignment);
    bool SelectionEvent(const SDL_Event& event);
    void FinishSelectionGesture();
    void DrawSelection(lv_event_t* event);
    std::vector<lv_obj_t*> marqueeBase;
    struct GroupItem { lv_obj_t* object; int x, y; double scaleX, scaleY; };
    std::vector<GroupItem> groupItems;
    bool marqueeActive = false, groupActive = false, groupMoved = false, selectionClick = false;
    bool groupAllowX = true, groupAllowY = true;
    float gestureX = 0, gestureY = 0, gestureLastX = 0, gestureLastY = 0;
    size_t groupAnchor = 0;
    EditorGrid::State canvasGrid;
    lv_obj_t* canvasViewport = nullptr;
    lv_obj_t* zoomLabel = nullptr;
    double canvasZoom = 1, canvasX = 0, canvasY = 0;
    bool canvasFitted = false, canvasPanning = false;
    int panButton = 0;
    float panLastX = 0, panLastY = 0;
    int canvasWidth = 0, canvasHeight = 0;
    void LayoutWorkspace();
    lv_obj_t *screensDock = nullptr, *canvasToolbar = nullptr, *workspaceStats = nullptr;
    lv_obj_t *undoButton = nullptr, *redoButton = nullptr;
    lv_obj_t* runButton = nullptr;
    lv_timer_t* workspaceTimer = nullptr;
    int workspaceWidth = 0, workspaceHeight = 0;
    void OpenMenu(const std::string& name, int x);
    void ShowFileDialog(bool save, bool binary = false, bool cCode = false);
    void CloseDocument();
    lv_obj_t* menuPopup = nullptr;
    lv_obj_t* fileDialog = nullptr;
    lv_obj_t* projectPathField = nullptr;
    lv_obj_t* fileStatus = nullptr;
    std::string projectPath = "project.json";
    bool saving = false;
    bool exportingBinary = false;
    bool exportingC = false;
#endif
};
