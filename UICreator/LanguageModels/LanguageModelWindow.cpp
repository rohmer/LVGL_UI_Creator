#include "LanguageModelWindow.h"
#ifndef _WIN32
#include "../Utils/NativeFileDialogUI.h"
#endif
#include "Generator.h"
#include "../Windows/PropertyWindow.h"
#include "../Windows/ProductWindows.h"
#include "../Utils/EditorControls.h"
#include "../Utils/EditorWindow.h"
#include "../Utils/EditorSession.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shlobj.h>
#endif

namespace LanguageModelWindow {
namespace {
using Clock = std::chrono::steady_clock;
constexpr const char* languageIds[] = {"en", "es", "fr", "de", "el", "ar", "hi", "zh", "bn", "id", "ur", "ru"};
constexpr uint32_t languageCount = sizeof(languageIds) / sizeof(*languageIds);
constexpr const char* languageLabels = "English\nSpanish\nFrench\nGerman\nGreek\nArabic\nHindi\nMandarin Chinese\nBengali\nIndonesian\nUrdu\nRussian";
constexpr uint64_t importLimit = 16u * 1024u * 1024u;
struct Work {
    std::atomic_bool cancel{false}, done{false};
    std::mutex mutex;
    LanguageModels::Progress progress{"Finding corpus files", 0};
    LanguageModels::Result result;
    std::string error;
    bool cancelled = false;
};
struct Job {
    std::shared_ptr<Work> work = std::make_shared<Work>();
    std::thread thread;
    ~Job() { work->cancel = true; if (thread.joinable()) thread.join(); }
};
struct Window {
    PropertyWindow* properties = nullptr;
    lv_obj_t *root = nullptr, *generate = nullptr, *stop = nullptr, *load = nullptr;
    lv_obj_t *status = nullptr, *timing = nullptr, *bar = nullptr, *confirmation = nullptr;
    lv_timer_t* timer = nullptr;
    std::map<std::string, lv_obj_t*> fields;
    std::vector<lv_obj_t*> editable;
    std::unique_ptr<Job> job;
    Clock::time_point started{};
    std::filesystem::path generated;
    bool closeWhenDone = false;
    ~Window() {
        if (timer) lv_timer_delete(timer);
        job.reset();
        if (confirmation && lv_obj_is_in_widget_tree(confirmation)) {
            EditorWindow::ReleaseFocus(confirmation);
            lv_obj_delete(confirmation);
        }
    }
};
Window* active = nullptr;
bool shuttingDown = false;

std::filesystem::path SettingsFile() { return EditorSession::DefaultDirectory() / "language-model-settings.json"; }
std::filesystem::path WordLists() {
    const auto* base = SDL_GetBasePath();
    return (base ? std::filesystem::u8path(base) : std::filesystem::current_path()) / "language-models" / "wordlists";
}
std::filesystem::path DefaultOutput() {
#ifdef _WIN32
    PWSTR directory = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &directory))) {
        std::filesystem::path result(directory); CoTaskMemFree(directory); return result / "model.ngtr";
    }
#else
    if (const auto* directory = std::getenv("HOME")) return std::filesystem::u8path(directory) / "Documents" / "model.ngtr";
#endif
    return EditorSession::DefaultDirectory() / "model.ngtr";
}
std::string Trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    value = value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
    return value;
}
std::string Text(Window* window, const char* key) { return lv_textarea_get_text(window->fields.at(key)); }
bool Checked(Window* window, const char* key) { return lv_obj_is_checked(window->fields.at(key)); }
void Status(Window* window, const std::string& message, bool error = false) {
    lv_label_set_text(window->status, message.c_str());
    lv_obj_set_style_text_color(window->status, lv_color_hex(error ? EditorStyle::Error : EditorStyle::Text), LV_PART_MAIN);
}
lv_obj_t* Row(lv_obj_t* parent) {
    auto* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN); lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN); lv_obj_set_scrollable(row, false);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(row, 10, LV_PART_MAIN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}
void Label(lv_obj_t* parent, const char* caption, int width = 175) {
    auto* label = lv_label_create(parent); lv_label_set_text(label, caption);
    lv_obj_set_width(label, width); lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}
void Focus(Window* window, lv_obj_t* field) {
    lv_obj_add_event_cb(field, [](lv_event_t* event) {
        auto* window = static_cast<Window*>(lv_event_get_user_data(event));
        FocusEditorControl(window->properties->GetKBGroup(), lv_event_get_target_obj(event));
    }, LV_EVENT_CLICKED, window);
}
lv_obj_t* Button(Window* window, lv_obj_t* parent, const char* caption, std::function<void()> action, int width = 135) {
    auto* button = lv_button_create(parent); lv_obj_set_size(button, width, 36); EditorStyle::Button(button);
    auto* text = lv_label_create(button); lv_label_set_text(text, caption); lv_obj_center(text);
    auto* callback = new std::function<void()>(std::move(action));
    lv_obj_add_event_cb(button, [](lv_event_t* event) {
        auto* callback = static_cast<std::function<void()>*>(lv_event_get_user_data(event));
        if (lv_event_get_code(event) == LV_EVENT_DELETE) { delete callback; return; }
        if (lv_event_get_code(event) == LV_EVENT_CLICKED) { auto action = *callback; action(); }
    }, LV_EVENT_ALL, callback);
    Focus(window, button); return button;
}
lv_obj_t* Input(Window* window, lv_obj_t* parent, const char* caption, const std::string& initial, const char* key) {
    auto* row = Row(parent); Label(row, caption);
    auto* field = lv_textarea_create(row); lv_textarea_set_one_line(field, true);
    lv_textarea_set_text(field, initial.c_str()); lv_obj_set_height(field, 38); lv_obj_set_flex_grow(field, 1);
    EditorStyle::Control(field); Focus(window, field); window->fields[key] = field; window->editable.push_back(field);
    return field;
}
lv_obj_t* Check(Window* window, lv_obj_t* parent, const char* caption, bool selected, const char* key) {
    auto* field = lv_checkbox_create(parent); lv_checkbox_set_text(field, caption); lv_obj_set_checked(field, selected);
    window->fields[key] = field; window->editable.push_back(field); Focus(window, field); return field;
}
std::filesystem::path Pick(bool save, bool folder, const wchar_t* title, const std::filesystem::path& initial = {}) {
#ifdef _WIN32
    if (folder) {
        const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IFileOpenDialog* dialog = nullptr; std::filesystem::path selected;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            DWORD options = 0; dialog->GetOptions(&options); dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            dialog->SetTitle(title);
            if (SUCCEEDED(dialog->Show(GetActiveWindow()))) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) { selected = path; CoTaskMemFree(path); }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
        return selected;
    }
    std::vector<wchar_t> path(32768, L'\0'); auto text = initial.wstring();
    if (text.size() < path.size()) std::copy(text.begin(), text.end(), path.begin());
    auto directory = initial.parent_path().wstring();
    OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof dialog; dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = L"Language models (*.ngtr;*.trie;*.bin)\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0";
    dialog.lpstrTitle = title; dialog.lpstrFile = path.data(); dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"ngtr"; dialog.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_PATHMUSTEXIST | (save ? 0 : OFN_FILEMUSTEXIST);
    if (!directory.empty()) dialog.lpstrInitialDir = directory.c_str();
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) return std::filesystem::path(path.data());
#else
    return NativeFileDialogUI::PickOne(save,folder,title,L"Language models (*.ngtr;*.trie;*.bin)\0*.ngtr;*.trie;*.bin\0All files\0*.*\0\0",initial);
#endif
    return {};
}
uint64_t Number(const std::string& raw, const char* caption) {
    const auto text = Trim(raw);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument(std::string(caption) + " must be a whole number.");
    size_t end = 0; uint64_t value = 0;
    try { value = std::stoull(text, &end); }
    catch (const std::exception&) { throw std::invalid_argument(std::string(caption) + " is too large."); }
    if (end != text.size()) throw std::invalid_argument(std::string("Invalid ") + caption + '.');
    return value;
}
LanguageModels::Options ReadOptions(Window* window) {
    LanguageModels::Options options;
    std::istringstream lines(Text(window, "directories")); std::string line;
    while (std::getline(lines, line)) {
        line = Trim(line); if (line.empty()) continue;
        auto path = std::filesystem::absolute(std::filesystem::u8path(line)).lexically_normal();
        if (!std::filesystem::is_directory(path)) throw std::invalid_argument("Corpus folder not found: " + path.u8string());
        if (std::find(options.inputs.begin(), options.inputs.end(), path) == options.inputs.end()) options.inputs.push_back(std::move(path));
    }
    if (options.inputs.empty()) throw std::invalid_argument("Add at least one corpus folder containing .txt files.");
    auto output = Trim(Text(window, "output"));
    if (output.empty()) throw std::invalid_argument("Choose a trie output file.");
    options.trieOutput = std::filesystem::absolute(std::filesystem::u8path(output)).lexically_normal();
    if (options.trieOutput.filename().empty() || std::filesystem::is_directory(options.trieOutput)) throw std::invalid_argument("Choose a file for the trie output.");
    if (!std::filesystem::is_directory(options.trieOutput.parent_path())) throw std::invalid_argument("The output folder does not exist.");
    const auto language = lv_dropdown_get_selected(window->fields.at("language"));
    options.language = languageIds[(std::min)(language, languageCount - 1)];
    options.wordListDirectory = WordLists();
    const auto workers = Number(Text(window, "workers"), "Worker threads");
    if (!workers || workers > 1024) throw std::invalid_argument("Worker threads must be between 1 and 1024.");
    options.workers = static_cast<unsigned>(workers);
    const auto bytes = Trim(Text(window, "maximumBytes"));
    options.maximumBytes = bytes.empty() ? std::optional<uint64_t>{} : Number(bytes, "Maximum trie bytes");
    if (options.maximumBytes && *options.maximumBytes < 20) throw std::invalid_argument("Maximum trie bytes must be at least 20, or blank for unlimited.");
    const char* keys[] = {"unigrams", "bigrams", "trigrams"};
    for (size_t order = 0; order < 3; ++order) {
        const auto text = Trim(Text(window, keys[order]));
        options.limits[order] = text.empty() ? std::optional<uint64_t>{} : Number(text, keys[order]);
    }
    options.recursive = Checked(window, "recursive"); options.useCache = Checked(window, "cache");
    options.lowMemory = Checked(window, "lowMemory"); options.compact = true;
    options.cacheDirectory = EditorSession::DefaultDirectory() / "language-model-cache";
    options.scratchDirectory = EditorSession::DefaultDirectory() / "language-model-work";
    return options;
}
void SaveSettings(Window* window) {
    try {
        json settings = {{"version", 1}, {"recursive", Checked(window, "recursive")}, {"cache", Checked(window, "cache")},
            {"lowMemory", Checked(window, "lowMemory")}, {"language", languageIds[(std::min)(lv_dropdown_get_selected(window->fields.at("language")), languageCount - 1)]}};
        for (const auto* key : {"directories", "output", "maximumBytes", "unigrams", "bigrams", "trigrams", "workers"}) settings[key] = Text(window, key);
        const auto path = SettingsFile(); std::filesystem::create_directories(path.parent_path());
        EditorSession::WriteAtomic(path, settings.dump(2));
    } catch (const std::exception& error) { Status(window, std::string("Could not save generator settings: ") + error.what(), true); }
}
void LoadSettings(Window* window) {
    try {
        std::ifstream stream(SettingsFile(), std::ios::binary); if (!stream) return;
        const auto settings = json::parse(stream);
        if (!settings.is_object() || settings.value("version", 0) != 1) return;
        for (const auto* key : {"directories", "output", "maximumBytes", "unigrams", "bigrams", "trigrams", "workers"})
            if (settings.contains(key) && settings.at(key).is_string()) lv_textarea_set_text(window->fields.at(key), settings.at(key).get_ref<const std::string&>().c_str());
        for (const auto* key : {"recursive", "cache", "lowMemory"}) if (settings.contains(key) && settings.at(key).is_boolean()) lv_obj_set_checked(window->fields.at(key), settings.at(key).get<bool>());
        const auto language = settings.value("language", std::string("en"));
        for (uint32_t index = 0; index < languageCount; ++index) if (language == languageIds[index]) lv_dropdown_set_selected(window->fields.at("language"), index);
    } catch (const std::exception& error) { Status(window, std::string("Could not load generator settings: ") + error.what(), true); }
}
void SetRunning(Window* window, bool running) {
    for (auto* field : window->editable) lv_obj_set_disabled(field, running);
    lv_obj_set_disabled(window->generate, running); lv_obj_set_disabled(window->stop, !running);
    lv_obj_set_disabled(window->load, running || window->generated.empty());
}
std::string Duration(uint64_t seconds) {
    std::ostringstream text; text << std::setfill('0') << std::setw(2) << seconds / 3600 << ':' << std::setw(2) << seconds / 60 % 60 << ':' << std::setw(2) << seconds % 60;
    return text.str();
}
void Start(Window* window, LanguageModels::Options options) {
    if (window->job) return;
    SaveSettings(window); window->generated.clear(); window->closeWhenDone = false;
    window->job = std::make_unique<Job>(); auto work = window->job->work;
    window->started = Clock::now(); SetRunning(window, true);
    lv_bar_set_value(window->bar, 0, LV_ANIM_OFF); Status(window, "Generating a compact NGTR v3 model. The editor remains responsive.");
    try { window->job->thread = std::thread([work, options = std::move(options)] {
        try {
            auto result = LanguageModels::Generate(options, [work](const LanguageModels::Progress& update) {
                std::lock_guard<std::mutex> lock(work->mutex); work->progress = update;
            }, &work->cancel);
            work->result = std::move(result);
        } catch (const LanguageModels::Cancelled&) { work->cancelled = true; }
        catch (const std::exception& error) { work->error = error.what(); }
        catch (...) { work->error = "An unexpected error stopped language model generation."; }
        work->done.store(true, std::memory_order_release);
    }); } catch (...) { window->job.reset(); SetRunning(window, false); throw; }
}
void DeleteConfirmation(Window* window) {
    if (!window->confirmation) return;
    auto* overlay = window->confirmation; window->confirmation = nullptr;
    EditorWindow::ReleaseFocus(overlay); lv_obj_delete(overlay);
}
void RequestStart(Window* window) {
    if (window->job || window->confirmation) return;
    try {
        auto options = ReadOptions(window);
        if (!std::filesystem::exists(options.trieOutput)) { Start(window, std::move(options)); return; }
        window->confirmation = lv_obj_create(lv_layer_top());
        auto* overlay = window->confirmation; lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(overlay, lv_color_black(), LV_PART_MAIN); lv_obj_set_style_bg_opa(overlay, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN); lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN); lv_obj_set_scrollable(overlay, false);
        auto* confirm = EditorWindow::Create(overlay); lv_obj_set_size(confirm, (std::min)(560, lv_display_get_horizontal_resolution(nullptr) - 40), 220); lv_obj_center(confirm);
        EditorWindow::SetTitle(confirm, "Replace language model?");
        EditorWindow::SetCloseHandler(confirm, [window] { DeleteConfirmation(window); });
        auto* body = EditorWindow::Content(confirm); lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(body, 16, LV_PART_MAIN);
        EditorStyle::Help(body, ("Replace this existing file?\n" + options.trieOutput.u8string()).c_str());
        auto* actions = Row(body);
        Button(window, actions, "Replace", [window, options] {
            DeleteConfirmation(window);
            try { Start(window, options); }
            catch (const std::exception& error) { Status(window, error.what(), true); }
        });
        Button(window, actions, "Cancel", [window] { DeleteConfirmation(window); });
    } catch (const std::exception& error) { Status(window, error.what(), true); }
}
void Poll(lv_timer_t* timer) {
    auto* window = static_cast<Window*>(lv_timer_get_user_data(timer));
    if (!window->job) return;
    auto work = window->job->work;
    const bool done = work->done.load(std::memory_order_acquire);
    LanguageModels::Progress progress;
    { std::lock_guard<std::mutex> lock(work->mutex); progress = work->progress; }
    const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - window->started).count());
    const auto percent = (std::min)(progress.percent, 100u);
    lv_bar_set_value(window->bar, static_cast<int32_t>(percent), LV_ANIM_OFF);
    const auto eta = percent ? Duration(elapsed * (100 - percent) / percent) : std::string("Calculating...");
    const bool complete = done && !work->cancelled && work->error.empty();
    std::string timing = (complete ? "Complete" : work->cancel.load() ? "Stopping" : progress.stage) + "  " + std::to_string(percent) + "%  |  Elapsed " + Duration(elapsed) + "  |  Remaining " + eta;
    if (progress.filesTotal) timing += "  |  Files " + std::to_string(progress.filesProcessed) + '/' + std::to_string(progress.filesTotal);
    lv_label_set_text(window->timing, timing.c_str());
    if (!done) return;
    window->job.reset();
    if (window->closeWhenDone) { Close(); return; }
    if (work->cancelled) Status(window, "Generation stopped.");
    else if (!work->error.empty()) Status(window, work->error, true);
    else {
        window->generated = work->result.output;
        std::ostringstream message;
        message << "Saved " << work->result.trieBytes << " bytes to " << window->generated.u8string()
                << "\n" << work->result.counts[0] << " unigrams, " << work->result.counts[1] << " bigrams, " << work->result.counts[2] << " trigrams"
                << "; " << work->result.files << " corpus files, " << work->result.cacheHits << " reused count caches.";
        if (work->result.trieBytes <= importLimit) message << "\nLoad into project to use this model in Advanced Keyboard.";
        else { message << "\nThis model exceeds the project's 16 MiB model limit. Generate with a smaller byte budget to embed it."; window->generated.clear(); }
        Status(window, message.str()); lv_bar_set_value(window->bar, 100, LV_ANIM_OFF);
    }
    SetRunning(window, false);
}
void UserClose(Window* window) {
    DeleteConfirmation(window);
    if (!window->job) { Close(); return; }
    window->closeWhenDone = true; window->job->work->cancel = true;
    lv_obj_set_disabled(window->stop, true); Status(window, "Stopping generation before closing...");
}
}

std::string Import(PropertyWindow* properties, const std::filesystem::path& path) {
    if (!properties) throw std::invalid_argument("No open project is available for this language model.");
    EditorEdit::Scope edit;
    const auto id = properties->Tries().Import(path);
    auto* selected = properties->GetSelectedObject();
    if (selected && lv_obj_is_in_widget_tree(selected) && Serialization::AdvancedKeyboard::Type(selected)) {
        properties->Tries().Apply(selected, id); properties->SetSelectedObject(selected);
    }
    return id;
}
void LoadTrie(PropertyWindow* properties) {
    const auto path = Pick(false, false, L"Load a language model into the project");
    if (path.empty()) return;
    try {
        Import(properties, path);
        if (active && active->properties == properties) Status(active, "Loaded " + path.filename().u8string() + " into the project.");
        else ProductWindows::Message("Language model loaded", "The model is embedded in this project. Select it in Advanced Keyboard's Prediction trie dropdown.");
    } catch (const std::exception& error) {
        if (active && active->properties == properties) Status(active, error.what(), true);
        else ProductWindows::Message("Could not load language model", error.what());
    }
}
void Close() {
    if (!active) return;
    auto* window = active; SaveSettings(window);
    auto* properties = window->properties;
    auto* focused = lv_group_get_focused(properties->GetKBGroup());
    bool restoreFocus = false;
    for (auto* parent = focused; parent; parent = lv_obj_get_parent(parent))
        if (parent == window->root || parent == window->confirmation) { restoreFocus = true; break; }
    if (window->job) window->job->work->cancel = true;
    window->job.reset();
    EditorWindow::ReleaseFocus(window->root);
    lv_obj_delete(window->root);
    if (restoreFocus && !shuttingDown) {
        auto* selected = properties->GetSelectedObject();
        if (selected && lv_obj_is_in_widget_tree(selected)) properties->SetSelectedObject(selected);
    }
}
void Shutdown() { shuttingDown = true; Close(); shuttingDown = false; }
lv_obj_t* ActiveWindow() { return active ? active->root : nullptr; }
bool Running() { return active && active->job != nullptr; }
void Open(PropertyWindow* properties) {
    if (active) { lv_obj_move_foreground(active->root); return; }
    auto* window = new Window; window->properties = properties;
    window->root = EditorWindow::Create(lv_layer_top()); active = window;
    const auto width = (std::min)(980, lv_display_get_horizontal_resolution(nullptr) - 40);
    const auto height = (std::min)(780, lv_display_get_vertical_resolution(nullptr) - 60);
    lv_obj_set_size(window->root, (std::max)(width, 320), (std::max)(height, 300)); lv_obj_center(window->root);
    EditorWindow::SetTitle(window->root, "Language model generator");
    EditorWindow::SetCloseHandler(window->root, [window] { UserClose(window); });
    lv_obj_add_event_cb(window->root, [](lv_event_t* event) {
        auto* window = static_cast<Window*>(lv_event_get_user_data(event));
        if (active == window) active = nullptr; delete window;
    }, LV_EVENT_DELETE, window);
    auto* body = EditorWindow::Content(window->root); lv_obj_set_scrollable(body, false); lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(body, 16, LV_PART_MAIN); lv_obj_set_style_pad_row(body, 10, LV_PART_MAIN);
    auto* settings = lv_obj_create(body); lv_obj_set_size(settings, LV_PCT(100), 0); lv_obj_set_flex_grow(settings, 1);
    lv_obj_set_flex_flow(settings, LV_FLEX_FLOW_COLUMN); lv_obj_set_scroll_dir(settings, LV_DIR_VER);
    lv_obj_set_style_pad_all(settings, 0, LV_PART_MAIN); lv_obj_set_style_pad_right(settings, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(settings, 0, LV_PART_MAIN); lv_obj_set_style_bg_opa(settings, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(settings, 10, LV_PART_MAIN);
    EditorStyle::Help(settings, "Build a compact language model from UTF-8 .txt corpora. The resulting NGTR v3 file works with Advanced Keyboard on every supported platform.");
    auto* corpusRow = Row(settings); Label(corpusRow, "Corpus folders (one per line)");
    auto* directories = lv_textarea_create(corpusRow); lv_obj_set_height(directories, 100); lv_obj_set_flex_grow(directories, 1);
    lv_textarea_set_placeholder_text(directories, "Add folders containing corpus .txt files");
    EditorStyle::Control(directories); Focus(window, directories); window->fields["directories"] = directories; window->editable.push_back(directories);
    auto* add = Button(window, corpusRow, "Add folder...", [window] {
        auto path = Pick(false, true, L"Add a corpus folder"); if (path.empty()) return;
        auto current = Text(window, "directories"); if (!current.empty() && current.back() != '\n') current += '\n';
        current += path.u8string(); lv_textarea_set_text(window->fields.at("directories"), current.c_str());
    }, 115); window->editable.push_back(add);
    Check(window, settings, "Include subdirectories", true, "recursive");
    auto* output = Input(window, settings, "Trie output file", DefaultOutput().u8string(), "output");
    auto* browse = Button(window, lv_obj_get_parent(output), "Browse...", [window] {
        auto path = Pick(true, false, L"Choose a trie output file", std::filesystem::u8path(Text(window, "output")));
        if (!path.empty()) lv_textarea_set_text(window->fields.at("output"), path.u8string().c_str());
    }, 115); window->editable.push_back(browse);
    auto* languageRow = Row(settings); Label(languageRow, "Corpus language");
    auto* language = lv_dropdown_create(languageRow); lv_dropdown_set_options(language, languageLabels); lv_obj_set_flex_grow(language, 1);
    EditorStyle::Control(language); Focus(window, language); window->fields["language"] = language; window->editable.push_back(language);
    Input(window, settings, "Worker threads", std::to_string((std::min)(1024u, (std::max)(1u, std::thread::hardware_concurrency()))), "workers");
    auto* bytes = Input(window, settings, "Maximum trie bytes", "262144", "maximumBytes"); lv_textarea_set_placeholder_text(bytes, "Unlimited");
    for (const auto* key : {"unigrams", "bigrams", "trigrams"}) {
        const auto* caption = key == std::string("unigrams") ? "Unigrams" : key == std::string("bigrams") ? "Bigrams" : "Trigrams";
        auto* field = Input(window, settings, caption, "", key); lv_textarea_set_placeholder_text(field, "Automatic (0 disables this order)");
    }
    Check(window, settings, "Reuse saved counts", true, "cache");
    Check(window, settings, "Keep counts on disk", true, "lowMemory");
    EditorStyle::Help(settings, "Temporary counts use local application storage, including when the final trie is saved to a synced or network folder.");
    EditorStyle::Help(settings, "Output format: compact NGTR v3. English, Spanish, French, German and Greek use bundled vocabulary filters. Other languages train directly from the corpus, preserving combining marks. Mandarin counts Han characters. The keyboard layout is selected separately. Blank count limits are automatic.");
    auto* footer = lv_obj_create(body); lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT); lv_obj_set_scrollable(footer, false);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(footer, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN); lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(footer, 8, LV_PART_MAIN);
    auto* actions = Row(footer); lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW_WRAP);
    window->generate = Button(window, actions, "Generate trie", [window] { RequestStart(window); });
    window->stop = Button(window, actions, "Stop", [window] {
        if (window->job) { window->job->work->cancel = true; lv_obj_set_disabled(window->stop, true); Status(window, "Stopping generation..."); }
    }, 85);
    window->load = Button(window, actions, "Load into project", [window] {
        try { Import(window->properties, window->generated); Status(window, "Loaded the generated model into this project. It is available in Advanced Keyboard's Prediction trie dropdown."); }
        catch (const std::exception& error) { Status(window, error.what(), true); }
    }, 165);
    auto* loadExisting = Button(window, actions, "Load existing trie...", [window] { LoadTrie(window->properties); }, 170);
    window->editable.push_back(loadExisting);
    window->bar = lv_bar_create(footer); lv_obj_set_size(window->bar, LV_PCT(100), 10); lv_bar_set_range(window->bar, 0, 100);
    window->timing = lv_label_create(footer); lv_obj_set_width(window->timing, LV_PCT(100)); lv_label_set_long_mode(window->timing, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(window->timing, "Ready");
    window->status = lv_label_create(footer); lv_obj_set_width(window->status, LV_PCT(100)); lv_label_set_long_mode(window->status, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_min_height(window->status, 45, LV_PART_MAIN); Status(window, "Choose corpus folders and an output file, then generate the model.");
    LoadSettings(window); SetRunning(window, false);
    window->timer = lv_timer_create(Poll, 100, window);
    FocusEditorControl(properties->GetKBGroup(), directories);
}
}
