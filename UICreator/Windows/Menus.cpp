#include "UI.h"
#include "../Utils/EditorStyle.h"
#include "../Utils/EditorIcons.h"
#include "../Utils/ObjectIcons.h"
#include <algorithm>
#include <array>
#include <cctype>

#if LVGL_VERSION_MAJOR >= 9
namespace {
constexpr const char* menus[] = {"File", "Edit", "View", "Assets", "Language Models", "Export", "Debug", "Help"};
int TextWidth(lv_obj_t* label,const char* text)
{
    lv_point_t extent{};lv_text_get_size(&extent,text,lv_obj_get_style_text_font(label,LV_PART_MAIN),
        lv_obj_get_style_text_letter_space(label,LV_PART_MAIN),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);return extent.x;
}
void OneLine(lv_obj_t* label)
{
    lv_obj_set_height(label,lv_font_get_line_height(lv_obj_get_style_text_font(label,LV_PART_MAIN)));
    lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_DOTS);
}
void LayoutMenuHeadings(lv_obj_t* bar)
{
    if(lv_obj_get_child_count(bar)<10)return;
    std::array<int,8> textWidths{};int full=36;
    for(unsigned index=0;index<8;++index){auto* label=lv_obj_get_child(lv_obj_get_child(bar,index),0);textWidths[index]=TextWidth(label,menus[index]);full+=textWidths[index]+40;}
    const bool compact=full>lv_obj_get_content_width(bar);const int size=compact?16:20,padding=compact?4:6;
    for(unsigned index=0;index<8;++index){
        auto* button=lv_obj_get_child(bar,index);auto* label=lv_obj_get_child(button,0);
        lv_label_set_text(label,menus[index]);lv_obj_set_style_pad_left(button,padding,0);lv_obj_set_style_pad_right(button,padding,0);
        lv_obj_set_size(button,textWidths[index]+size+8+padding*2,36);
        const auto action=std::string("menu:")+menus[index];EditorIcons::Set(button,action.c_str(),false,size);
    }
}
bool Movable(lv_obj_t* object, int axis)
{
    const auto* data = static_cast<const ObjectUserData*>(lv_obj_get_user_data(object));
    const auto base = data ? data->objectJson.value("base", json::object()) : json::object();
    return !lv_obj_is_layout_positioned(object) && base.value("drag", true) && (base.value("dragDir", 3) & axis);
}
}

std::vector<UI::MenuItem> UI::MenuItems(const std::string& name) const
{
    std::vector<MenuItem> items;
    auto add = [&](const char* label, const char* action, const char* shortcut = "", bool enabled = true) {
        items.push_back({label, action, shortcut, {}, {}, enabled});
    };
    auto child = [&](const char* label, const char* submenu, bool enabled = true) {
        items.push_back({label, {}, {}, submenu, {}, enabled});
    };
    auto separator = [&] { items.push_back({}); };
    const auto roots = propertyWindow->SelectionRoots();
    const bool canEdit = !Running() && !fileDialog && !restoringHistory && !editDepth;
    auto align = [&](int axis) {
        return canEdit && roots.size() >= 2 && std::any_of(roots.begin(), roots.end(), [&](lv_obj_t* object) { return Movable(object, axis); });
    };
    auto distribute = [&](int axis) {
        if (!canEdit || roots.size() < 3) return false;
        auto* parent = lv_obj_get_parent(roots.front());
        return std::all_of(roots.begin(), roots.end(), [&](lv_obj_t* object) { return lv_obj_get_parent(object) == parent && Movable(object, axis); });
    };
    if (name == "File") {
        add("New project", "new", "Ctrl+N", !fileDialog);
        add("Open project...", "open", "Ctrl+O", !fileDialog);
        child("Recent projects", "Recent projects", !fileDialog && !recentProjects.empty());
        separator(); add("Save project", "save", "Ctrl+S", !fileDialog);
        add("Save project as...", "save-as", "Ctrl+Shift+S", !fileDialog);
        separator(); add("Exit", "exit");
    } else if (name == "Recent projects") {
        for (const auto& path : recentProjects) {
            const auto file = std::filesystem::u8path(path);
            items.push_back({file.filename().u8string(), "recent:" + path, {}, {}, file.parent_path().u8string()});
        }
    } else if (name == "Edit") {
        add("Undo", "undo", "Ctrl+Z", canEdit && CanUndo());
        add("Redo", "redo", "Ctrl+Y", canEdit && CanRedo());
        separator();
        add("Duplicate selection", "duplicate", "Ctrl+D", canEdit && SelectionCount());
        add("Delete selection", "delete", "Delete", canEdit && SelectionCount());
        separator();
        child("Align", "Align", align(1) || align(2));
        child("Distribute", "Distribute", distribute(1) || distribute(2));
        separator(); add("Edit widget events...", "events-editor", "", canEdit && SelectionCount());
        add("Layout designer...","layout-designer","",canEdit);add("Find property...","property-search","Ctrl+Shift+F",canEdit);
    } else if (name == "Align") {
        add("Left", "align-left", "", align(1));
        add("Horizontal centers", "align-hcenter", "", align(1));
        add("Right", "align-right", "", align(1));
        separator();
        add("Top", "align-top", "", align(2));
        add("Vertical centers", "align-vcenter", "", align(2));
        add("Bottom", "align-bottom", "", align(2));
    } else if (name == "Distribute") {
        add("Horizontally", "distribute-horizontal", "", distribute(1));
        add("Vertically", "distribute-vertical", "", distribute(2));
    } else if (name == "View") {
        add("Commands...","commands","Ctrl+Shift+P");add("Device previews...","device-previews");add("Screens and navigation...","screen-flow");add("Problems","problems");separator();
        add("Fit canvas", "fit-canvas"); add("Actual size (100%)", "zoom-reset");
        add("Zoom in", "zoom-in", "", canvasZoom < 4); add("Zoom out", "zoom-out", "", canvasZoom > .25);
        separator(); child("Show panel", "Show panel"); add("Restore all panels", "view:all");
        separator(); add("Workspace preferences...", "preferences", "", !fileDialog);
    } else if (name == "Show panel") {
        add("Screens", "view:screens"); add("Tool Tray", "view:widgets"); add("Hierarchy", "view:hierarchy");
        add("Properties", "view:properties"); add("Tools", "view:tools"); add("Simulator preview", "view:simulator");
    } else if (name == "Assets") {
        add("Theme gallery...", "theme-gallery"); add("Reusable components...", "component-library"); separator();
        add("Project fonts...", "font-manager"); add("Project images...", "image-manager");
        separator(); add("Convert font...", "font-converter"); add("Convert images...", "image-converter");
    } else if (name == "Language Models") {
        add("Model playground...", "model-playground"); separator();
        add("Generate language model...", "language-model-generator"); add("Load model into project...", "language-model-load");
    } else if (name == "Export") {
        add("Device Protobuf (.pb)...", "export-protobuf", "", !fileDialog); add("LVGL source project...", "export-c", "", !fileDialog);
        separator(); add("Create firmware project...", "project-creator", "", !fileDialog);
        add("Build, flash and serial monitor...", "device-tools");
    } else if (name == "Debug") {
        add(Running() ? "Return to Design" : "Run screen in editor", "run-toggle", "F6", !fileDialog);
        add("Run application from start","run-application","",!fileDialog);add("Performance dashboard...","performance");
        add("Validate screen...", "validate-screen"); separator();
        add("Debug current screen", "debug-screen", "F5", !fileDialog); add("Stop debug viewer", "debug-stop", "", DebugRunning());
    } else if (name == "Help") {
        add("Welcome and examples...", "welcome"); add("Keyboard shortcuts", "shortcuts");
        separator(); add("Diagnostics report", "diagnostics"); add("About LVGL UI Creator", "about");
    }
    return items;
}

void UI::CreateMenuBar()
{
    auto* bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bar, LV_PCT(100), 36); lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_scrollable(bar, false); lv_obj_set_style_bg_color(bar, lv_color_hex(0x181818), 0);
    lv_obj_set_style_border_width(bar, 0, 0); lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0); lv_obj_set_style_pad_column(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    auto button = [&](const char* text, const std::string& action, int width) {
        auto* object = lv_button_create(bar); lv_obj_set_size(object, width, 36);
        lv_obj_set_click_focusable(object, false);
        lv_obj_set_style_radius(object, 3, 0); lv_obj_set_style_shadow_width(object, 0, 0);
        lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(object, lv_color_hex(EditorStyle::Surface), LV_PART_MAIN | LV_STATE_CHECKED);
        auto* label = lv_label_create(object); lv_label_set_text(label, text); lv_obj_center(label);
        struct Binding { UI* ui; std::string action; };
        lv_obj_add_event_cb(object, [](lv_event_t* event) {
            auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
            if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete binding; return; }
            if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
                auto* ui = binding->ui; const auto action = binding->action; ui->MenuAction(action);
            }
        }, LV_EVENT_ALL, new Binding{this, action});
        return object;
    };
    for (const auto* name : menus) {
        auto* object = button(name, std::string("menu:") + name, std::string(name) == "Language Models" ? 150 : 70);
        menuButtons[name] = object;
    }
    documentTitle = lv_label_create(bar); lv_label_set_text(documentTitle, "LVGL UI Creator");
    lv_obj_set_style_pad_top(documentTitle, 10, 0); lv_obj_set_style_pad_left(documentTitle, 16, 0);
    lv_obj_set_style_text_color(documentTitle, lv_color_hex(EditorStyle::Muted), 0);
    lv_obj_set_width(documentTitle, 0); lv_obj_set_flex_grow(documentTitle, 1);
    lv_obj_set_height(documentTitle,36);
    lv_label_set_long_mode(documentTitle, LV_LABEL_LONG_MODE_DOTS);
    EditorIcons::Set(button(LV_SYMBOL_CLOSE, "exit", 36),"exit",true);
    lv_obj_add_event_cb(bar,[](lv_event_t* event){LayoutMenuHeadings(lv_event_get_current_target_obj(event));},LV_EVENT_SIZE_CHANGED,nullptr);
    lv_obj_update_layout(bar);LayoutMenuHeadings(bar);
}

void UI::TrimMenus(size_t depth)
{
    while (menuFrames.size() > depth) {
        auto* panel = menuFrames.back().panel; menuFrames.pop_back(); lv_obj_delete(panel);
    }
}
void UI::CloseMenu()
{
    auto* popup = menuPopup; menuPopup = nullptr;
    menuFrames.clear(); menuName.clear();
    for (const auto& item : menuButtons) lv_obj_remove_state(item.second, LV_STATE_CHECKED);
    if (popup) lv_obj_delete(popup);
}
void UI::FocusMenuRow(size_t depth, int index)
{
    if (depth >= menuFrames.size()) return;
    auto& frame = menuFrames[depth];
    if (index < 0 || size_t(index) >= frame.rows.size() || lv_obj_is_disabled(frame.rows[index])) return;
    if (frame.focused >= 0) lv_obj_remove_state(frame.rows[frame.focused], LV_STATE_FOCUSED);
    frame.focused = index; lv_obj_add_state(frame.rows[index], LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(frame.rows[index], LV_ANIM_OFF);
}
void UI::OpenMenuChild(size_t depth, int index)
{
    if (depth >= menuFrames.size() || index < 0 || size_t(index) >= menuFrames[depth].rows.size()) return;
    const auto name = menuFrames[depth].submenus[index];
    auto* anchor = menuFrames[depth].rows[index];
    if (name.empty() || lv_obj_is_disabled(anchor)) return;
    lv_area_t panel{}, row{}; lv_obj_get_coords(menuFrames[depth].panel, &panel); lv_obj_get_coords(anchor, &row);
    const int width = (std::min)(320, lv_display_get_horizontal_resolution(nullptr) - 8);
    int x = panel.x2 + 1;
    if (x + width > lv_display_get_horizontal_resolution(nullptr) - 4) x = panel.x1 - width;
    ShowMenuPanel(name, x, row.y1, depth + 1, anchor);
}
void UI::ShowMenuPanel(const std::string& name, int x, int y, size_t depth, lv_obj_t* anchor)
{
    TrimMenus(depth);
    const auto entries = MenuItems(name);
    if (entries.empty()) return;
    const int displayWidth = lv_display_get_horizontal_resolution(nullptr), displayHeight = lv_display_get_vertical_resolution(nullptr);
    const int width = (std::max)(1, (std::min)(320, displayWidth - 8));
    auto* panel = lv_obj_create(menuPopup);
    lv_obj_set_pos(panel, std::clamp(x, 0, (std::max)(0, displayWidth - width)), 0);
    lv_obj_set_size(panel, width, LV_SIZE_CONTENT); lv_obj_set_style_max_height(panel, (std::max)(1, displayHeight - 40), 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(EditorStyle::Surface), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(EditorStyle::Border), 0);
    lv_obj_set_style_border_width(panel, 1, 0); lv_obj_set_style_radius(panel, 5, 0);
    lv_obj_set_style_pad_all(panel, 5, 0); lv_obj_set_style_pad_row(panel, 0, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER); lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN); lv_obj_set_event_bubble(panel, false);
    MenuFrame frame; frame.panel = panel; frame.anchor = anchor;
    for (const auto& entry : entries) {
        if (entry.label.empty()) {
            auto* separator = lv_obj_create(panel); lv_obj_set_size(separator, LV_PCT(100), 9);
            lv_obj_set_style_bg_opa(separator, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(separator, 0, 0);
            lv_obj_set_style_pad_all(separator, 0, 0); lv_obj_set_clickable(separator, false); lv_obj_set_scrollable(separator, false);
            auto* line = lv_obj_create(separator); lv_obj_set_size(line, LV_PCT(100), 1); lv_obj_center(line);
            lv_obj_set_style_bg_color(line, lv_color_hex(EditorStyle::Border), 0); lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(line, 0, 0); lv_obj_set_clickable(line, false); lv_obj_set_scrollable(line, false);
            continue;
        }
        auto* row = lv_button_create(panel); lv_obj_set_size(row, LV_PCT(100), entry.detail.empty() ? 34 : 52);
        lv_obj_set_click_focusable(row, false); lv_obj_set_event_bubble(row, false);
        lv_obj_set_style_pad_all(row, 8, 0); lv_obj_set_style_radius(row, 3, 0); lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x395771), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_opa(row, LV_OPA_40, LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_disabled(row, !entry.enabled);
        auto* label = lv_label_create(row); lv_label_set_text(label, entry.label.c_str());
        OneLine(label);constexpr int leading=28;int auxiliary=0;
        if (!entry.submenu.empty() || !entry.shortcut.empty()) {
            auto* hint = lv_label_create(row); lv_label_set_text(hint, entry.submenu.empty() ? entry.shortcut.c_str() : LV_SYMBOL_RIGHT);
            OneLine(hint);const int hintWidth=TextWidth(hint,entry.submenu.empty()?entry.shortcut.c_str():LV_SYMBOL_RIGHT);
            lv_obj_set_width(hint,hintWidth);auxiliary=hintWidth+10;
            lv_obj_set_style_text_color(hint, lv_color_hex(EditorStyle::Muted), 0); lv_obj_align(hint, LV_ALIGN_RIGHT_MID, 0, 0);
        }
        lv_obj_update_layout(row);const int contentWidth=lv_obj_get_content_width(row);
        lv_obj_set_width(label,(std::max)(1,contentWidth-leading-auxiliary));
        if(entry.detail.empty())lv_obj_align(label,LV_ALIGN_LEFT_MID,leading,0);else lv_obj_set_pos(label,leading,-3);
        if (!entry.detail.empty()) {
            auto* detail = lv_label_create(row); lv_label_set_text(detail, entry.detail.c_str());
            lv_obj_set_style_text_color(detail, lv_color_hex(EditorStyle::Muted), 0);
            lv_obj_set_width(detail,(std::max)(1,contentWidth-leading));OneLine(detail);lv_obj_set_pos(detail,leading,19);
        }
        const auto iconAction=entry.action.empty()?std::string("submenu:")+entry.submenu:entry.action=="run-toggle"&&Running()?std::string("run-stop"):entry.action;
        if(auto* icon=ObjectIcons::Create(row,ObjectIcons::Action(iconAction.c_str()),20))lv_obj_align(icon,LV_ALIGN_LEFT_MID,0,0);
        struct Binding { UI* ui; size_t depth; int index; std::string action; };
        const int index = static_cast<int>(frame.rows.size());
        lv_obj_add_event_cb(row, [](lv_event_t* event) {
            auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
            const auto code = lv_event_get_code(event);
            if (code == LV_EVENT_DELETE) { delete binding; return; }
            auto* row = lv_event_get_current_target_obj(event);
            if (lv_obj_is_disabled(row)) return;
            if (code == LV_EVENT_HOVER_OVER || code == LV_EVENT_CLICKED) {
                auto* ui = binding->ui; const auto depth = binding->depth; const auto index = binding->index;
                if (ui->menuFrames.size() > depth + 1 && ui->menuFrames[depth + 1].anchor != row) ui->TrimMenus(depth + 1);
                ui->FocusMenuRow(depth, index);
                if (code == LV_EVENT_CLICKED) {
                    const auto action = binding->action;
                    if (action.empty()) ui->OpenMenuChild(depth, index); else ui->MenuAction(action);
                }
            }
        }, LV_EVENT_ALL, new Binding{this, depth, index, entry.action});
        frame.rows.push_back(row); frame.submenus.push_back(entry.submenu); frame.labels.push_back(entry.label);
    }
    menuFrames.push_back(std::move(frame));
    lv_obj_update_layout(panel);
    const int top = std::clamp(y, 36, (std::max)(36, displayHeight - int(lv_obj_get_height(panel)) - 1));
    lv_obj_set_y(panel, top - 36);
    for (size_t index = 0; index < menuFrames.back().rows.size(); ++index)
        if (!lv_obj_is_disabled(menuFrames.back().rows[index])) { FocusMenuRow(depth, static_cast<int>(index)); break; }
}
void UI::OpenMenu(const std::string& name, int x)
{
    CloseMenu();
    if (MenuItems(name).empty()) return;
    menuName = name;
    const auto top = menuButtons.find(name); if (top != menuButtons.end()) lv_obj_add_state(top->second, LV_STATE_CHECKED);
    menuPopup = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(menuPopup, 0, 36);
    lv_obj_set_size(menuPopup, LV_PCT(100), (std::max)(1, lv_display_get_vertical_resolution(nullptr) - 36));
    lv_obj_set_style_bg_opa(menuPopup, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(menuPopup, 0, 0);
    lv_obj_set_style_pad_all(menuPopup, 0, 0); lv_obj_set_scrollable(menuPopup, false);
    lv_obj_add_event_cb(menuPopup, [](lv_event_t* event) {
        if (lv_event_get_target_obj(event) == lv_event_get_current_target_obj(event)) static_cast<UI*>(lv_event_get_user_data(event))->CloseMenu();
    }, LV_EVENT_CLICKED, this);
    ShowMenuPanel(name, x, 36, 0);
}
bool UI::HandleMenuKey(int key, bool control, bool shift)
{
    if (menuFrames.empty()) { CloseMenu(); return true; }
    const auto depth = menuFrames.size() - 1;
    auto back = [&] {
        auto* anchor = menuFrames.back().anchor; TrimMenus(depth);
        auto& parent = menuFrames.back(); const auto found = std::find(parent.rows.begin(), parent.rows.end(), anchor);
        if (found != parent.rows.end()) FocusMenuRow(depth - 1, static_cast<int>(found - parent.rows.begin()));
    };
    if (key == SDLK_ESCAPE) { if (depth) back(); else CloseMenu(); return true; }
    if (control) {
        std::string action;
        if (key == 'n' && !fileDialog) action = "new"; else if (key == 'o' && !fileDialog) action = "open"; else if (key == 's' && !fileDialog) action = "save";
        else if (key == 'z' && !fileDialog && (shift ? CanRedo() : CanUndo())) action = shift ? "redo" : "undo";
        else if (key == 'y' && !fileDialog && CanRedo()) action = "redo";
        else if (key == 'd' && !fileDialog && SelectionCount()) action = "duplicate";
        if (!action.empty()) MenuAction(action);
        return true;
    }
    if (key == SDLK_F5 && !fileDialog) { MenuAction("debug-screen"); return true; }
    if (key == SDLK_DELETE && menuName == "Edit" && !fileDialog && SelectionCount()) { MenuAction("delete"); return true; }
    if (key == SDLK_LEFT && depth) { back(); return true; }
    auto& frame = menuFrames.back();
    if (key == SDLK_RIGHT && frame.focused >= 0 && !frame.submenus[frame.focused].empty()) {
        OpenMenuChild(depth, frame.focused); return true;
    }
    if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        size_t index = 0; while (index < 8 && menuName != menus[index]) ++index;
        MenuAction(std::string("menu:") + menus[(index + (key == SDLK_LEFT ? 7 : 1)) % 8]); return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
        if (key == SDLK_SPACE) swallowMenuText = true;
        if (frame.focused >= 0) lv_obj_send_event(frame.rows[frame.focused], LV_EVENT_CLICKED, nullptr);
        return true;
    }
    int start = frame.focused;
    const bool backwards = key == SDLK_UP || (key == SDLK_TAB && shift) || key == SDLK_END;
    if (key == SDLK_HOME) start = -1; else if (key == SDLK_END) start = static_cast<int>(frame.rows.size());
    if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_TAB || key == SDLK_HOME || key == SDLK_END || (key >= 'a' && key <= 'z')) {
        const int size = static_cast<int>(frame.rows.size());
        for (int step = 1; step <= size; ++step) {
            const int index = (start + (backwards ? -step : step) + size * 2) % size;
            if (lv_obj_is_disabled(frame.rows[index])) continue;
            if (key >= 'a' && key <= 'z' && std::tolower(static_cast<unsigned char>(frame.labels[index][0])) != key) continue;
            FocusMenuRow(depth, index); break;
        }
    }
    return true;
}
#endif
