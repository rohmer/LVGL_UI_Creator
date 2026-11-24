#include "ProductWindows.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/ProductInfo.h"
#include "../Utils/EditorStyle.h"
#include "../Utils/ObjectIcons.h"
#include "../../3rdParty/LVGL/lvgl/include/lvgl/widgets/lv_bootstrap.h"
#include "../../CodeGeneration/CCodeGenerator.h"
#include "../../CodeGeneration/MicroPythonCodeGenerator.h"
#include "../../CodeGeneration/XmlCodeGenerator.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
lv_obj_t* dialog = nullptr;
lv_obj_t* Text(lv_obj_t* parent, const std::string& text)
{
    auto* label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_text(label, text.c_str());
    return label;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, std::function<void()> action)
{
    auto* button = lv_button_create(parent);
    lv_obj_set_size(button, LV_PCT(100), 36);
    EditorStyle::Button(button);
    lv_obj_set_style_shadow_width(button, 0, 0);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_center(label);
    auto* binding = new std::function<void()>(std::move(action));
    lv_obj_add_event_cb(button, [](lv_event_t* event) {
        auto* binding = static_cast<std::function<void()>*>(lv_event_get_user_data(event));
        if (lv_event_get_code(event) == LV_EVENT_DELETE) delete binding;
        else if (lv_event_get_code(event) == LV_EVENT_CLICKED) { auto invoke = *binding; invoke(); }
    }, LV_EVENT_ALL, binding);
    return button;
}
lv_obj_t* TemplateButton(lv_obj_t* parent, const char* text, int preset, std::function<void()> action)
{
    auto* button = Button(parent, text, std::move(action));
    auto* caption = lv_obj_get_child(button, 0);
    lv_obj_set_height(button, 52);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x365f79), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button, 12, LV_PART_MAIN);
    lv_obj_set_width(caption, 0);
    lv_obj_set_flex_grow(caption, 1);
    if (auto* icon = ObjectIcons::Create(button, ObjectIcons::Preset(preset), 32))
    {
        lv_obj_move_to_index(icon, 0);
        lv_obj_set_clickable(icon, false);
        lv_obj_set_click_focusable(icon, false);
        lv_obj_set_scrollable(icon, false);
    }
    return button;
}
lv_obj_t* Create(const char* title, int height)
{
    ProductWindows::Close();
    dialog = EditorWindow::Create(lv_layer_top());
    lv_obj_set_size(dialog, std::min(620, lv_display_get_horizontal_resolution(nullptr)-24),
                    std::min(height, lv_display_get_vertical_resolution(nullptr)-24));
    lv_obj_center(dialog);
    EditorWindow::SetTitle(dialog, title);
    EditorWindow::SetCloseHandler(dialog, [] { ProductWindows::Close(); });
    lv_obj_add_event_cb(dialog, [](lv_event_t*) { dialog = nullptr; }, LV_EVENT_DELETE, nullptr);
    auto* content = EditorWindow::Content(dialog);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    return content;
}
void OpenFolder(const std::string& path)
{
    // SDL handles platform URL opening; paths never become shell commands.
    auto absolute = std::filesystem::absolute(std::filesystem::u8path(path)).generic_u8string();
    std::string url = "file:///";
    constexpr char digits[] = "0123456789ABCDEF";
    for (unsigned char c : absolute) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '/' || c == ':' || c == '-' || c == '_' || c == '.') url += char(c);
        else { url += '%'; url += digits[c >> 4]; url += digits[c & 15]; }
    }
    SDL_OpenURL(url.c_str());
}
}
void ProductWindows::Close()
{
    if (dialog && lv_obj_is_valid(dialog)) { EditorWindow::ReleaseFocus(dialog); lv_obj_delete(dialog); }
    dialog = nullptr;
}
void ProductWindows::About()
{
    auto* content = Create("About LVGL UI Creator", 350);
    Text(content, std::string(ProductInfo::Name) + " " + ProductInfo::Version + "\nNative LVGL visual editor and firmware tools");
    Text(content, "Built with LVGL " + std::to_string(LVGL_VERSION_MAJOR) + "." + std::to_string(LVGL_VERSION_MINOR) + "." + std::to_string(LVGL_VERSION_PATCH) + " and SDL " + std::to_string(SDL_MAJOR_VERSION) + "." + std::to_string(SDL_MINOR_VERSION));
    Text(content, "MIT licensed. Third-party components retain their own licenses; see licenses in the application folder.");
    Button(content, "Keyboard shortcuts", [] { Shortcuts(); });
    Button(content, "Close", [] { Close(); });
}
void ProductWindows::Message(const char* title,const std::string& text)
{
    auto* content=Create(title,300);Text(content,text);Button(content,"Close",[]{Close();});
}
void ProductWindows::Shortcuts()
{
    auto* content = Create("Keyboard shortcuts", 480);
    Text(content, "Projects\nCtrl+N  New     Ctrl+O  Open     Ctrl+S  Save\nF5  Standalone Protobuf viewer     F6  Run / Design\nEscape  Leave Run mode\n\nEditing\nCtrl+Z  Undo     Ctrl+Y / Ctrl+Shift+Z  Redo\nCtrl+D  Duplicate     Delete  Delete selection\nArrow keys  Nudge 1 px     Shift+Arrow  Nudge 10 px\nShift+click  Add/remove selection\n\nCanvas\nWheel  Zoom at pointer     Middle drag  Pan\nFit  Fit preview     Click zoom percentage  Reset to 100%\n\nWorkspace\nAlt+F / E / V / A  File / Edit / View / Assets\nAlt+L / X / D / H  Language Models / Export / Debug / Help\nMenus: arrows to navigate, Enter to choose, Escape to go back or close\nView > Restore all panels  Reopen hidden panels");
    Button(content, "Close", [] { Close(); });
}
void ProductWindows::Welcome(const std::vector<std::string>& recent, std::function<void(std::string)> action)
{
    auto* content = Create("Welcome to LVGL UI Creator", 650);
    Text(content, std::string("LVGL UI Creator ") + ProductInfo::Version);
    Text(content, "Design a screen, preview it, and export it to your LVGL firmware project.");
    auto route = [action](std::string command) { Close(); action(std::move(command)); };
    Button(content, "New blank project", [route] { route("new"); });
    Button(content, "Open project...", [route] { route("open"); });
    Text(content, "Start with an editable example");
    TemplateButton(content, "Admin dashboard", LV_BOOTSTRAP_DASHBOARD, [route] { route("template:dashboard"); });
    TemplateButton(content, "Sign-in form", LV_BOOTSTRAP_SIGN_IN, [route] { route("template:signin"); });
    TemplateButton(content, "Component gallery", LV_BOOTSTRAP_COMPONENT_GALLERY, [route] { route("template:gallery"); });
    if (!recent.empty()) {
        Text(content, "Recent projects");
        for (size_t i = 0; i < std::min(size_t(5), recent.size()); ++i) {
            auto path = recent[i];
            Button(content, path.c_str(), [route, path] { route("recent:" + path); });
        }
    }
    Text(content, "1. Add controls from the widget palette.\n2. Edit layout and appearance in Properties.\n3. Save your project, then use Export for C, MicroPython, Protobuf or a firmware project.");
    Button(content, "Continue editing", [] { Close(); });
}
void ProductWindows::Diagnostics(const json& document)
{
    auto* content = Create("Diagnostics report", 500);
    std::ostringstream report;
    report << ProductInfo::Name << ' ' << ProductInfo::Version << "\nLVGL " << LVGL_VERSION_MAJOR << '.' << LVGL_VERSION_MINOR << '.' << LVGL_VERSION_PATCH
           << "\nSDL " << SDL_MAJOR_VERSION << '.' << SDL_MINOR_VERSION << '.' << SDL_MICRO_VERSION
           << "\nVideo driver: " << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unavailable")
           << "\nDisplay: " << lv_display_get_horizontal_resolution(nullptr) << " x " << lv_display_get_vertical_resolution(nullptr)
           << "\nProject JSON: " << document.dump().size() << " bytes\nWidgets on active screen: " << document.value("obj", json::array()).size()
           << "\nImage library entries: " << document.value("images", json::array()).size()
           << "\nFont library entries: " << document.value("fonts", json::array()).size()
           << "\nCompiler targets: 9.6, 9.5, 9.3, 9.2, 9.1, 8.3\n";
    const auto text = report.str();
    Text(content, text);
    Text(content, "This report excludes project content and file paths. Copy it when reporting a problem; nothing is uploaded.");
    auto* status = Text(content, "");
    Button(content, "Copy report", [text, status] { lv_label_set_text(status, SDL_SetClipboardText(text.c_str()) ? "Report copied." : SDL_GetError()); });
    Button(content, "Close", [] { Close(); });
}
std::string ProductWindows::Preflight(const json& document, const std::string& target, const std::string& sourceDirectory)
{
    CCodeGenerator generator;
    generator.SetSourceDirectory(sourceDirectory); generator.SetTargetVersion(target);
    json summary;
    if (!generator.LoadProject(document) || !generator.Preflight(summary))
        return "Export blocked: " + generator.GetLastError();
    std::ostringstream report;
    report << "Ready for LVGL " << target << ": " << summary.at("files") << " files, "
           << summary.at("sourceBytes") << " source bytes (assets: " << summary.at("assetSourceBytes")
           << "). Source sizes are not firmware RAM/flash estimates. See ui_requirements.h for required LVGL options.";
    return report.str();
}
void ProductWindows::ExportSuccess(const std::string& path, bool cProject)
{
    auto* content = Create("Export complete", 330);
    Text(content, std::string(cProject ? "C project exported to\n" : "Device project exported to\n") + path);
    Text(content, cProject ? "Add generated_ui to your firmware build, enable the options in ui_requirements.h, then call ui_init(). Keep application code outside the generated folder." : "Load the Protobuf file using the matching LVGL runtime target in your firmware.");
    const auto folder = cProject ? path : std::filesystem::u8path(path).parent_path().u8string();
    Button(content, "Open output folder", [folder] { OpenFolder(folder); });
    Button(content, "Close", [] { Close(); });
}
std::string ProductWindows::MicroPythonPreflight(const json& document, const std::string& sourceDirectory)
{
    MicroPythonCodeGenerator generator;
    generator.SetSourceDirectory(sourceDirectory);
    json summary;
    if (!generator.LoadProject(document) || !generator.Preflight(summary))
        return "Export blocked: " + generator.GetLastError();
    return "Ready for MicroPython / LVGL 9.3.0: " + std::to_string(summary.value("files", size_t(0))) +
        " files. Initialize your display and input drivers, then call ui.create(). See README.md for setup and export-manifest.json for firmware requirements.";
}
void ProductWindows::MicroPythonExportSuccess(const std::string& path)
{
    auto* content = Create("Export complete", 360);
    Text(content, "MicroPython project exported to\n" + path);
    Text(content, "Use LVGL-enabled MicroPython from lvgl/lv_binding_micropython. Initialize your display and input drivers, then call ui.create(). See the exported README.md for setup.");
    Button(content, "Open output folder", [path] { OpenFolder(path); });
    Button(content, "Close", [] { Close(); });
}
std::string ProductWindows::XmlPreflight(const json& document, const std::string& target, const std::string& sourceDirectory)
{
    XmlCodeGenerator generator;generator.SetSourceDirectory(sourceDirectory);generator.SetTargetVersion(target);json summary;
    if(!generator.LoadProject(document)||!generator.Preflight(summary))return "Export blocked: "+generator.GetLastError();
    return "Ready for UI Creator XML / LVGL "+target+": "+std::to_string(summary.value("sourceBytes",size_t(0)))+
        " bytes. The file embeds widgets, variables and assets for the UI Creator runtime.";
}
void ProductWindows::XmlExportSuccess(const std::string& path)
{
    auto* content=Create("Export complete",330);
    Text(content,"UI Creator XML exported to\n"+path);
    Text(content,"Load this file with the UI Creator runtime's lvgl_runtime_load_file() or lvgl_runtime_load_memory(). Its typed XML uses the same project model as Protobuf.");
    const auto folder=std::filesystem::u8path(path).parent_path().u8string();
    Button(content,"Open output folder",[folder]{OpenFolder(folder);});Button(content,"Close",[]{Close();});
}
