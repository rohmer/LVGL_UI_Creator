#include "FigmaImportWindow.h"
#include "FigmaOnline.h"
#include "../Windows/UI.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorControls.h"
#include "../ProjectCreator/ProjectCreator.h"
#include <FigmaImporter.h>
#include <Serialization/ProjectScreens.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include "../Utils/NativeFileDialogUI.h"
#endif

namespace FigmaImportWindow {
#if LVGL_VERSION_MAJOR >= 9
namespace {
using json = nlohmann::json;
constexpr size_t MaxFileBytes = 64u * 1024u * 1024u;
struct Job {
    std::atomic<bool> done{false}, cancel{false};
    bool loading = false;
    std::shared_ptr<const FigmaOnline::Location> location;
    json source;
    std::vector<FigmaImport::Frame> frames;
    FigmaImport::Result result;
    std::string error;
    std::mutex progressMutex;
    std::string progress;
};
struct Window {
    UI* editor;
    lv_group_t* group;
    lv_obj_t *root, *body, *file, *scale, *frames, *destination, *hidden;
    lv_obj_t *browse, *load, *review, *apply, *status;
    lv_obj_t *sourceMode, *localPanel, *onlinePanel, *link, *token, *onlineLoad, *cancel;
    lv_timer_t* timer = nullptr;
    std::shared_ptr<Job> job;
    std::shared_ptr<const json> source;
    std::shared_ptr<const FigmaOnline::Location> location;
    std::vector<FigmaImport::Frame> candidates;
    FigmaImport::Result prepared;
    bool ready = false;
    ~Window() { if(job) job->cancel = true; if(timer) lv_timer_delete(timer); }
};
lv_obj_t* active = nullptr;

lv_obj_t* Caption(lv_obj_t* parent, const std::string& text, int width = LV_PCT(100)) {
    auto* label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text.c_str());
    return label;
}
lv_obj_t* Row(lv_obj_t* parent) {
    auto* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
    return row;
}
lv_obj_t* Button(lv_obj_t* parent, const char* text, std::function<void()> action, int width = 150) {
    auto* button = lv_button_create(parent);
    lv_obj_set_size(button, width, 36);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, [](lv_event_t* event) {
        auto* callback = static_cast<std::function<void()>*>(lv_event_get_user_data(event));
        if(lv_event_get_code(event) == LV_EVENT_DELETE) delete callback;
        else if(lv_event_get_code(event) == LV_EVENT_CLICKED) { auto invoke = *callback; invoke(); }
    }, LV_EVENT_ALL, new std::function<void()>(std::move(action)));
    return button;
}
void Status(Window* window, const std::string& text) { lv_label_set_text(window->status, text.c_str()); }
void Controls(Window* window, bool busy) {
    for(auto* field : {window->file, window->scale, window->frames, window->hidden, window->browse, window->load,
                      window->sourceMode, window->link, window->token, window->onlineLoad})
        lv_obj_set_disabled(field, busy);
    lv_obj_set_disabled(window->review, busy || !window->source);
    lv_obj_set_disabled(window->apply, busy || !window->ready);
    lv_obj_set_disabled(window->cancel, !busy);
}
void Invalidate(Window* window, bool fileChanged) {
    window->ready = false;
    window->prepared = {};
    if(fileChanged) {
        window->source.reset(); window->location.reset(); window->candidates.clear();
        lv_dropdown_set_options(window->frames, "All frames as screens");
    }
    Controls(window, bool(window->job));
    Status(window, fileChanged ? "Load the design to list its frames." : "Choose Review conversion to update the import preview.");
}
FigmaOnline::Progress Progress(const std::shared_ptr<Job>& job) {
    return [job](const std::string& message) {
        std::lock_guard<std::mutex> guard(job->progressMutex);
        job->progress = message;
    };
}
std::filesystem::path Browse() {
#ifdef _WIN32
    std::vector<wchar_t> path(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrTitle = L"Import Figma design";
    dialog.lpstrFilter = L"Figma design JSON\0*.figma.json;*.json\0All files\0*.*\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if(GetOpenFileNameW(&dialog)) return std::filesystem::path(path.data());
    return {};
#else
    return NativeFileDialogUI::PickOne(false, false, L"Import Figma design", L"Figma design JSON\0*.figma.json;*.json\0All files\0*.*\0\0");
#endif
}
json ReadDesign(const std::filesystem::path& path) {
    if(path.empty()) throw std::invalid_argument("Choose a Figma design JSON file.");
    if(path.extension() == ".fig") throw std::invalid_argument("Export the design with the bundled Figma plugin first. This importer reads .figma.json or Figma REST JSON.");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if(!input) throw std::runtime_error("Cannot open the design file.");
    const auto length = input.tellg();
    if(length <= 0 || length > std::streamoff(MaxFileBytes)) throw std::invalid_argument("Design files must contain between 1 byte and 64 MiB.");
    std::string data(static_cast<size_t>(length), '\0');
    input.seekg(0);
    if(!input.read(data.data(), static_cast<std::streamsize>(data.size()))) throw std::runtime_error("Could not read the complete design file.");
    return json::parse(data, [](int depth, json::parse_event_t, json&) {
        if(depth > 128) throw std::invalid_argument("The design JSON nesting limit is 128.");
        return true;
    });
}
FigmaImport::Options Options(Window* window) {
    FigmaImport::Options options;
    const auto selected = lv_dropdown_get_selected(window->frames);
    if(selected) {
        if(selected > window->candidates.size()) throw std::invalid_argument("Load the design again to refresh the frame selection.");
        options.nodeIds.push_back(window->candidates[selected - 1].id);
    }
    const std::string text = lv_textarea_get_text(window->scale);
    size_t consumed = 0;
    const auto percent = std::stod(text, &consumed);
    if(consumed != text.size() || !std::isfinite(percent) || percent < 1 || percent > 800)
        throw std::invalid_argument("Scale must be a number from 1 to 800 percent.");
    options.scale = percent / 100.0;
    options.includeHidden = lv_obj_has_state(window->hidden, LV_STATE_CHECKED);
    return options;
}
void Prepare(Window* window) {
    if(window->job) return;
    try {
        if(!window->source) throw std::invalid_argument("Load a design first.");
        const auto options = Options(window);
        const auto source = window->source;
        const auto location = window->location;
        const std::string token = location ? lv_textarea_get_text(window->token) : "";
        auto job = std::make_shared<Job>();
        window->job = job;
        window->ready = false;
        Controls(window, true);
        Status(window, "Converting design layers for review...");
        std::thread([job, source, location, token, options] {
            try {
                if(location) {
                    job->location = location;
                    job->source = FigmaOnline::AddRenders({*location, *source}, token, options, &job->cancel, Progress(job));
                    if(!job->cancel) job->result = FigmaImport::Convert(job->source, options);
                } else if(!job->cancel) job->result = FigmaImport::Convert(*source, options);
            }
            catch(const std::exception& error) { job->error = error.what(); }
            job->done = true;
        }).detach();
    } catch(const std::exception& error) { window->job.reset(); Controls(window, false); Status(window, error.what()); }
}
void LoadOnline(Window* window) {
    if(window->job) return;
    try {
        Invalidate(window, true);
        const std::string link = lv_textarea_get_text(window->link);
        FigmaOnline::ParseUrl(link);
        const std::string token = lv_textarea_get_text(window->token);
        if(token.empty()) throw std::invalid_argument("Enter a Figma personal access token with file_content:read access.");
        auto job = std::make_shared<Job>();
        job->loading = true;
        window->job = job;
        Controls(window, true);
        Status(window, "Connecting to Figma...");
        std::thread([job, link, token] {
            try {
                auto design = FigmaOnline::Load(link, token, &job->cancel, Progress(job));
                job->location = std::make_shared<const FigmaOnline::Location>(std::move(design.location));
                job->source = std::move(design.document);
                if(!job->cancel) job->frames = FigmaImport::Frames(job->source);
                if(job->frames.empty()) throw std::invalid_argument("This Figma link contains no importable frames. Copy a design or frame link from Figma.");
            } catch(const std::exception& error) { job->error = error.what(); }
            job->done = true;
        }).detach();
    } catch(const std::exception& error) { window->job.reset(); Controls(window, false); Status(window, error.what()); }
}
void Load(Window* window) {
    if(window->job) return;
    try {
        const auto path = std::filesystem::u8path(lv_textarea_get_text(window->file));
        Invalidate(window, true);
        auto job = std::make_shared<Job>();
        job->loading = true;
        window->job = job;
        Controls(window, true);
        Status(window, "Reading design frames...");
        std::thread([job, path] {
            try {
                job->source = ReadDesign(path);
                if(!job->cancel) job->frames = FigmaImport::Frames(job->source);
                if(job->frames.empty()) throw std::invalid_argument("No importable frames were found. Select frames in Figma and export them with the plugin.");
            } catch(const std::exception& error) { job->error = error.what(); }
            job->done = true;
        }).detach();
    } catch(const std::exception& error) { window->job.reset(); Controls(window, false); Status(window, error.what()); }
}
// Keep authored resources and screen IDs intact when adding the imported design.
// Resource IDs can repeat across Figma exports, so remap imported references.
void Remap(json& value, const std::map<std::string, std::string>& ids, const char* reference, const char* alias) {
    if(value.is_object()) {
        for(auto& item : value.items()) {
            if((item.key() == reference || item.key() == alias) && item.value().is_string()) {
                const auto found = ids.find(item.value().get<std::string>());
                if(found != ids.end()) item.value() = found->second;
            } else Remap(item.value(), ids, reference, alias);
        }
    } else if(value.is_array()) for(auto& item : value) Remap(item, ids, reference, alias);
}
void MergeCatalog(json& current, json& imported, const char* catalog, const char* reference, const char* alias) {
    if(!current.contains(catalog)) current[catalog] = json::array();
    if(!imported.contains(catalog)) return;
    std::set<std::string> usedIds, usedNames;
    for(const auto& item : current[catalog]) {
        usedIds.insert(item.value("id", std::string{}));
        usedNames.insert(item.value("name", std::string{}));
    }
    std::map<std::string, std::string> ids;
    for(auto& item : imported[catalog]) {
        const auto original = item.at("id").get<std::string>();
        const auto* payload = std::string(catalog) == "images" ? "image" : "font";
        const auto existing = std::find_if(current[catalog].begin(), current[catalog].end(), [&](const json& candidate) {
            return candidate.contains(payload) && item.contains(payload) && candidate[payload] == item[payload];
        });
        if(existing != current[catalog].end()) {
            ids[original] = existing->at("id").get<std::string>();
            continue;
        }
        auto id = original;
        for(size_t suffix = 2; usedIds.count(id); ++suffix) id = original + "-" + std::to_string(suffix);
        usedIds.insert(id);
        ids[original] = id;
        item["id"] = id;
        const auto originalName = item.value("name", original);
        auto name = originalName;
        for(size_t suffix = 2; usedNames.count(name); ++suffix) name = originalName + " (" + std::to_string(suffix) + ")";
        usedNames.insert(name);
        item["name"] = name;
        current[catalog].push_back(std::move(item));
    }
    Remap(imported["screens"], ids, reference, alias);
}
json Append(json current, json imported) {
    using namespace Serialization;
    ProjectScreens::Normalize(current, true);
    ProjectScreens::EnsureMulti(current);
    ProjectScreens::Normalize(imported, true);
    ProjectScreens::EnsureMulti(imported);
    const auto first = current["screens"].size();
    if(first + imported["screens"].size() > 256) throw std::invalid_argument("The combined project would exceed 256 screens.");
    MergeCatalog(current, imported, "images", "projectImage", "imageId");
    MergeCatalog(current, imported, "fonts", "projectFont", "fontId");
    for(auto& screen : imported["screens"]) {
        screen["id"] = ProjectScreens::UniqueId(current);
        current["screens"].push_back(std::move(screen));
    }
    ProjectScreens::Select(current, first);
    return current;
}
void Apply(Window* window) {
    if(!window->ready || window->job) return;
    try {
        auto document = window->prepared.project;
        if(lv_dropdown_get_selected(window->destination) == 0) document = Append(window->editor->ProjectDocument(), std::move(document));
        const auto count = Serialization::ProjectScreens::Count(window->prepared.project);
        window->editor->ApplyDocument(std::move(document));
        window->ready = false;
        Controls(window, false);
        Status(window, "Imported " + std::to_string(count) + " screen(s) and " + std::to_string(window->prepared.objects) + " editable object(s). Use Undo to restore the previous project. Review conversion to import again.");
    } catch(const std::exception& error) { Status(window, std::string("Import failed: ") + error.what()); }
}
void Poll(lv_timer_t* timer) {
    auto* window = static_cast<Window*>(lv_timer_get_user_data(timer));
    if(!window->job) return;
    if(!window->job->done.load()) {
        std::string progress;
        { std::lock_guard<std::mutex> guard(window->job->progressMutex); progress = window->job->progress; }
        if(!progress.empty() && !window->job->cancel) Status(window, progress);
        return;
    }
    auto job = std::move(window->job);
    if(job->cancel) { window->ready = false; Controls(window, false); Status(window, "Figma request cancelled. Your project has not changed."); return; }
    if(!job->error.empty()) { Controls(window, false); Status(window, job->error); return; }
    if(job->loading) {
        window->source = std::make_shared<const json>(std::move(job->source));
        window->location = std::move(job->location);
        window->candidates = std::move(job->frames);
        std::string names = "All frames as screens";
        for(const auto& frame : window->candidates) {
            auto name = frame.name;
            std::replace(name.begin(), name.end(), '\n', ' ');
            std::replace(name.begin(), name.end(), '\r', ' ');
            names += "\n" + name + " (" + std::to_string(frame.width) + " x " + std::to_string(frame.height) + ")" + (frame.hidden ? " [hidden]" : "");
        }
        lv_dropdown_set_options(window->frames, names.c_str());
        lv_dropdown_set_selected(window->frames, 0);
        Controls(window, false);
        if(window->location) Status(window, "Loaded " + std::to_string(window->candidates.size()) + " frame(s) from Figma. Select frames and choose Review conversion to download their artwork. Your project has not changed.");
        else Prepare(window);
    } else {
        if(job->location) window->source = std::make_shared<const json>(std::move(job->source));
        window->prepared = std::move(job->result);
        window->ready = true;
        Controls(window, false);
        std::string summary = "Ready: " + std::to_string(Serialization::ProjectScreens::Count(window->prepared.project)) + " screen(s), " + std::to_string(window->prepared.objects) + " object(s).\n";
        if(window->prepared.warnings.empty()) summary += "The design uses supported editable layers.";
        else {
            summary += "Conversion notes (" + std::to_string(window->prepared.warnings.size()) + "):\n";
            for(size_t i = 0; i < std::min<size_t>(window->prepared.warnings.size(), 40); ++i)
                summary += "- " + window->prepared.warnings[i] + "\n";
            if(window->prepared.warnings.size() > 40) summary += "More layers have the same conversion limits.\n";
        }
        Status(window, summary);
    }
}
}
void Close() { if(active) lv_obj_delete(active); }
void Open(UI* editor, lv_group_t* keyboardGroup) {
    if(active) { lv_obj_move_foreground(active); return; }
    auto* window = new Window{};
    window->editor = editor;
    window->group = keyboardGroup;
    window->root = EditorWindow::Create(lv_layer_top());
    active = window->root;
    lv_obj_set_size(window->root, std::max(280, std::min(900, lv_display_get_horizontal_resolution(nullptr) - 40)),
                    std::max(300, std::min(820, lv_display_get_vertical_resolution(nullptr) - 60)));
    lv_obj_center(window->root);
    EditorWindow::SetTitle(window->root, "Import Figma design");
    EditorWindow::SetCloseHandler(window->root, [window] { lv_obj_delete(window->root); });
    window->body = EditorWindow::Content(window->root);
    lv_obj_set_flex_flow(window->body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(window->body, 16, 0);
    lv_obj_set_style_pad_row(window->body, 10, 0);
    Caption(window->body, "Import a Figma design link or a local JSON export. Text and basic shapes stay editable; complex artwork is imported as images. Review the conversion before adding screens.");
    auto* sourceRow = Row(window->body);
    Caption(sourceRow, "Source", 140);
    window->sourceMode = lv_dropdown_create(sourceRow);
    lv_dropdown_set_options(window->sourceMode, "Local Figma JSON\nOnline Figma link");
    lv_obj_set_flex_grow(window->sourceMode, 1);
    window->localPanel = Row(window->body);
    lv_obj_set_flex_flow(window->localPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(window->localPanel, 10, 0);
    Caption(window->localPanel, "Use the bundled Figma plugin to save selected frames as .figma.json, or load Figma REST JSON.");
    auto* help = Row(window->localPanel);
    Button(help, "Open plugin folder", [window] {
        try {
            const auto folder = ProjectCreation::RuntimeDirectory().parent_path() / "integrations" / "figma";
            if(!ProjectCreation::OpenFolder(folder)) Status(window, "The plugin folder could not be opened. Find integrations/figma beside the application.");
        } catch(const std::exception& error) { Status(window, error.what()); }
    }, 180);
    auto* fileRow = Row(window->localPanel);
    Caption(fileRow, "Design JSON file", 140);
    window->file = lv_textarea_create(fileRow);
    lv_textarea_set_one_line(window->file, true);
    lv_textarea_set_text(window->file, "");
    lv_textarea_set_placeholder_text(window->file, ".figma.json or .json");
    lv_obj_set_height(window->file, 38);
    lv_obj_set_flex_grow(window->file, 1);
    auto* fileButtons = Row(window->localPanel);
    window->browse = Button(fileButtons, "Browse...", [window] {
        try { const auto file = Browse(); if(!file.empty()) { lv_textarea_set_text(window->file, file.u8string().c_str()); Invalidate(window, true); } }
        catch(const std::exception& error) { Status(window, error.what()); }
    }, 110);
    window->load = Button(fileButtons, "Load design", [window] { Load(window); });
    window->onlinePanel = Row(window->body);
    lv_obj_set_flex_flow(window->onlinePanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(window->onlinePanel, 10, 0);
    lv_obj_set_hidden(window->onlinePanel, true);
    auto* linkRow = Row(window->onlinePanel);
    Caption(linkRow, "Figma design link", 140);
    window->link = lv_textarea_create(linkRow);
    lv_textarea_set_one_line(window->link, true);
    lv_textarea_set_text(window->link, "");
    lv_textarea_set_placeholder_text(window->link, "https://www.figma.com/design/...");
    lv_obj_set_height(window->link, 38);
    lv_obj_set_flex_grow(window->link, 1);
    auto* tokenRow = Row(window->onlinePanel);
    Caption(tokenRow, "Access token", 140);
    window->token = lv_textarea_create(tokenRow);
    lv_textarea_set_one_line(window->token, true);
    lv_textarea_set_text(window->token, "");
    lv_textarea_set_placeholder_text(window->token, "Figma personal access token");
    lv_textarea_set_password_mode(window->token, true);
    lv_textarea_set_password_show_time(window->token, 0);
    lv_obj_set_height(window->token, 38);
    lv_obj_set_flex_grow(window->token, 1);
    Caption(window->onlinePanel, "Create a personal access token in Figma Settings with file_content:read access. Your account must have access to the design. The token is kept in this dialog and is never saved in project files. Figma API limits apply.");
    auto* onlineButtons = Row(window->onlinePanel);
    window->onlineLoad = Button(onlineButtons, "Load from Figma", [window] { LoadOnline(window); }, 180);
    Button(onlineButtons, "Token instructions", [window] {
        if(!ProjectCreation::OpenUrl("https://developers.figma.com/docs/rest-api/personal-access-tokens/"))
            Status(window, "Open Figma Settings to create a personal access token with file_content:read access.");
    }, 180);
    auto* frameRow = Row(window->body);
    Caption(frameRow, "Frames", 140);
    window->frames = lv_dropdown_create(frameRow);
    lv_dropdown_set_options(window->frames, "All frames as screens");
    lv_obj_set_flex_grow(window->frames, 1);
    auto* scaleRow = Row(window->body);
    Caption(scaleRow, "Scale (%)", 140);
    window->scale = lv_textarea_create(scaleRow);
    lv_textarea_set_one_line(window->scale, true);
    lv_textarea_set_text(window->scale, "100");
    lv_textarea_set_accepted_chars(window->scale, "0123456789.");
    lv_obj_set_size(window->scale, 110, 38);
    window->hidden = lv_checkbox_create(window->body);
    lv_checkbox_set_text(window->hidden, "Include hidden layers");
    auto* destinationRow = Row(window->body);
    Caption(destinationRow, "Destination", 140);
    window->destination = lv_dropdown_create(destinationRow);
    lv_dropdown_set_options(window->destination, "Add screens to current project\nReplace project");
    lv_obj_set_flex_grow(window->destination, 1);
    auto* actions = Row(window->body);
    window->review = Button(actions, "Review conversion", [window] { Prepare(window); }, 180);
    window->apply = Button(actions, "Import design", [window] { Apply(window); }, 160);
    window->cancel = Button(actions, "Cancel request", [window] {
        if(window->job) { window->job->cancel = true; Status(window, "Cancelling the Figma request..."); }
    }, 150);
    window->status = Caption(window->body, "Choose a source to begin. Import is one undoable edit; your current project stays intact until you choose Import design.");
    for(auto* field : {window->file, window->scale, window->frames, window->destination, window->hidden, window->sourceMode, window->link, window->token}) BindEditorControl(keyboardGroup, field);
    lv_obj_add_event_cb(window->sourceMode, [](lv_event_t* event) {
        auto* window = static_cast<Window*>(lv_event_get_user_data(event));
        const bool online = lv_dropdown_get_selected(window->sourceMode) != 0;
        lv_obj_set_hidden(window->localPanel, online);
        lv_obj_set_hidden(window->onlinePanel, !online);
        if(!online) lv_textarea_set_text(window->token, "");
        Invalidate(window, true);
        FocusEditorControl(window->group, online ? window->link : window->file);
    }, LV_EVENT_VALUE_CHANGED, window);
    for(auto* field : {window->link, window->token}) lv_obj_add_event_cb(field, [](lv_event_t* event) { Invalidate(static_cast<Window*>(lv_event_get_user_data(event)), true); }, LV_EVENT_VALUE_CHANGED, window);
    lv_obj_add_event_cb(window->file, [](lv_event_t* event) { Invalidate(static_cast<Window*>(lv_event_get_user_data(event)), true); }, LV_EVENT_VALUE_CHANGED, window);
    for(auto* field : {window->scale, window->frames, window->hidden}) lv_obj_add_event_cb(field, [](lv_event_t* event) { Invalidate(static_cast<Window*>(lv_event_get_user_data(event)), false); }, LV_EVENT_VALUE_CHANGED, window);
    Controls(window, false);
    window->timer = lv_timer_create(Poll, 50, window);
    lv_obj_add_event_cb(window->root, [](lv_event_t* event) { active = nullptr; delete static_cast<Window*>(lv_event_get_user_data(event)); }, LV_EVENT_DELETE, window);
    FocusEditorControl(keyboardGroup, window->file);
}
#else
void Open(UI*, lv_group_t*) {}
void Close() {}
#endif
}
