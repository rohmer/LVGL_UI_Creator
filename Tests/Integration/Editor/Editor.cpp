#define SDL_MAIN_HANDLED
#include "CodeWidgets.h"
#include "AdvancedKeyboard.h"
#include "ProjectKeyboard.h"
#include "LanguageModels.h"
#include "Menus.h"
#include "RunMode.h"
#include "DesignTools.h"
#include "Interactions.h"
#include "DevicePlayground.h"
#include "VisualAuthoring.h"
#include "WorkflowProblems.h"
#include "PolishCore.h"
#include "VisualRegression.h"
#include "Performance.h"
#include "TftSettings.h"
#include "ScreenMemory.h"
#include "TextInputModes.h"
#include "EditorInputMatrix.h"
#include "ObjectIcons.h"
#include "HierarchyIcons.h"
#include "MenuToolbarIcons.h"
#include "ThemeGallery.h"
#include "Variables.h"
#include "../Figma/FigmaEditor.h"
#include "../../../UICreator/Utils/SdlSimulator.h"
#include "../../../UICreator/Utils/EditorWindow.h"
#include "../../../UICreator/Utils/EditorEdit.h"
#include "../../../UICreator/Fonts/BuiltinFonts.h"
#include "../../../UICreator/ProjectCreator/ProjectCreator.h"
#include "../../../UICreator/Windows/UI.h"
#include "../../../UICreator/Windows/PropertyWindows/BtnmEditorWindow.h"
#include "../../../UICreator/Widgets/FileBrowser.h"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/ProjectScreens.h>
#include <Serialization/Xml.h>
#include "../../../UICreator/Windows/PropertyWindows/ImageProperties.h"
#include <algorithm>
#include <map>
#include <set>
#include <cmath>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    lv_obj_t* FindText(lv_obj_t* parent, const char* text)
    {
        if (lv_obj_get_class(parent) == &lv_label_class && std::string(lv_label_get_text(parent)) == text) return parent;
        for (uint32_t index = 0; index < lv_obj_get_child_count(parent); ++index)
            if (auto* found = FindText(lv_obj_get_child(parent, index), text)) return found;
        return nullptr;
    }

    lv_obj_t* FindCheckbox(lv_obj_t* parent, const char* text)
    {
        if (lv_obj_get_class(parent) == &lv_checkbox_class && std::string(lv_checkbox_get_text(parent)) == text) return parent;
        for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
            if (auto* found = FindCheckbox(lv_obj_get_child(parent, i), text)) return found;
        return nullptr;
    }

    void Click(const char* text)
    {
        auto* trayTitle = FindText(lv_screen_active(), "Tool Tray");
        auto* tray = trayTitle ? EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(trayTitle))) : nullptr;
        auto* label = tray && std::string(text) != LV_SYMBOL_PLUS ? FindText(tray, text) : nullptr;
        if(std::string(text) == "Screen")
        {
            auto* hierarchyTitle = FindText(lv_screen_active(), "Hierarchy");
            auto* hierarchy = hierarchyTitle ? lv_obj_get_parent(lv_obj_get_parent(hierarchyTitle)) : nullptr;
            label = hierarchy ? FindText(hierarchy, text) : nullptr;
        }
        if (!label && std::string(text) == LV_SYMBOL_PLUS)
        {
            auto* toolsTitle = FindText(lv_screen_active(), "Tools");
            label = FindText(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(toolsTitle))), text);
        }
        if (!label) label = FindText(lv_screen_active(), text);
        Require(label != nullptr, "Expected menu or widget button is missing");
        lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, nullptr);
        lv_obj_update_layout(lv_screen_active());

    }

    void ClickPaletteWidget(SdlSimulator& simulator, const char* name)
    {
        auto* title = FindText(lv_screen_active(), "Tool Tray");
        auto* palette = EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title)));
        auto* label = FindText(palette, name);
        Require(label != nullptr, "Palette entry is missing");
        auto* button = lv_obj_get_parent(label);
        lv_obj_scroll_to_view(button, LV_ANIM_OFF);
        lv_obj_update_layout(lv_screen_active());
        lv_area_t bounds;
        lv_obj_get_coords(button, &bounds);
        const int x = (bounds.x1 + bounds.x2) / 2, y = (bounds.y1 + bounds.y2) / 2;
        const auto window = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.windowID = window;
        motion.motion.x = x; motion.motion.y = y;
        SDL_PushEvent(&motion);
        SDL_Event buttonEvent{};
        buttonEvent.type = SDL_EVENT_MOUSE_BUTTON_DOWN; buttonEvent.button.windowID = window;
        buttonEvent.button.button = SDL_BUTTON_LEFT; buttonEvent.button.down = true;
        buttonEvent.button.x = x; buttonEvent.button.y = y;
        SDL_PushEvent(&buttonEvent);
        for (int i = 0; i < 12; ++i) simulator.ProcessEvents();
        buttonEvent.type = SDL_EVENT_MOUSE_BUTTON_UP; buttonEvent.button.down = false;
        SDL_PushEvent(&buttonEvent);
        for (int i = 0; i < 12; ++i) simulator.ProcessEvents();
        lv_obj_update_layout(lv_screen_active());
    }

}

int main(int argc, char* argv[])
{
    try
    {
        SdlSimulator simulator;
        Require(simulator.Initialize(1440, 960), "SDL initialization failed");
        UI editor(simulator.GetKeyboard());
        if (argc == 3 && std::string(argv[1]) == "--figma-import") {
            CheckFigmaEditor(editor, simulator, std::filesystem::u8path(argv[2])); return 0;
        }
#if LVSERIAL_USE_XML
        if(argc==3 && std::string(argv[1])=="--xml-export") {
            const auto path=std::filesystem::absolute(std::filesystem::u8path(argv[2]));
            Click("Label");Click("Bar");
            auto legacy=editor.ProjectDocument();
            legacy["obj"].push_back({{"oid",2},{"par",-1},{"base",json::object()},{"advancedKeyboard",json::object()}});
            editor.ApplyDocument(std::move(legacy));
            const auto original=editor.ProjectDocument();const bool dirty=editor.Dirty();
            editor.MenuAction("menu:Export");Require(FindText(lv_layer_top(),"UI Creator XML (.xml)..."),"XML export menu entry is missing");
            editor.MenuAction("export-xml");auto* title=FindText(lv_layer_top(),"Export UI Creator XML");Require(title,"XML export dialog is missing");
            auto* field=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
            Require(lv_obj_has_class(field,&lv_textarea_class),"XML export path is not editable");
            lv_textarea_set_text(field,path.u8string().c_str());editor.MenuAction("export-preflight");
            bool ready=false;std::function<void(lv_obj_t*)> status=[&](lv_obj_t* object){
                if(lv_obj_has_class(object,&lv_label_class)&&std::string(lv_label_get_text(object)).find("Ready for UI Creator XML / LVGL 9.6")==0)ready=true;
                for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)status(lv_obj_get_child(object,i));
            };status(lv_layer_top());Require(ready,"XML preflight rejected supported native widgets");
            editor.MenuAction("confirm-file");Require(std::filesystem::is_regular_file(path),"XML export did not create its file");
            Require(!FindText(lv_layer_top(),"Export UI Creator XML"),"XML export failed to close its dialog");
            std::ifstream input(path,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(input)),{});
            const auto record=Serialization::Xml::Decode(bytes);
            Require(record.kind==Serialization::EntityKind::Project&&record.type=="project","XML export used the wrong entity envelope");
            Require(editor.ProjectDocument()==original&&editor.Dirty()==dirty,"XML export changed the editor document or saved state");
            editor.MenuAction("open");title=FindText(lv_layer_top(),"Open project");Require(title,"Open dialog is missing");
            field=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);lv_textarea_set_text(field,path.u8string().c_str());editor.MenuAction("confirm-file");
            Require(!FindText(lv_layer_top(),"Open project")&&editor.ProjectDocument()["obj"].size()==original["obj"].size(),"XML import lost widgets");
            Require(!editor.Dirty(),"Opening an XML project marked it unsaved");
            editor.MenuAction("save");title=FindText(lv_layer_top(),"Save project");Require(title,"Saving an XML device project must offer an editor JSON path");
            field=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
            Require(std::filesystem::u8path(lv_textarea_get_text(field)).extension()==".json","Save would write JSON into the XML source file");
            editor.MenuAction("cancel-file");
            Require(std::ifstream(path,std::ios::binary).good(),"Opening or saving removed the XML source");
            std::cout<<"Owned XML editor menu, preflight, Advanced Keyboard export, import and safe editor Save path passed\n";return 0;
        }
#endif
        if(argc==3 && std::string(argv[1])=="--micropython-export") {
            const auto output=std::filesystem::absolute(std::filesystem::u8path(argv[2]));
            Click("Label");Click("Bar");
            const auto document=editor.ProjectDocument();const bool dirty=editor.Dirty();
            editor.MenuAction("menu:Export");
            Require(FindText(lv_layer_top(),"MicroPython project..."),"Export menu is missing MicroPython");
            editor.MenuAction("export-micropython");
            auto* title=FindText(lv_layer_top(),"Export MicroPython project");
            Require(title,"MicroPython export dialog did not open");
            auto* field=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
            Require(lv_obj_has_class(field,&lv_textarea_class),"MicroPython output path is not editable");
            Require(std::string(lv_textarea_get_text(field)).find("generated_micropython")!=std::string::npos,"MicroPython output folder default is missing");
            lv_textarea_set_text(field,output.u8string().c_str());
            editor.MenuAction("export-preflight");
            auto hasStatus=[](const std::string& prefix) {
                bool found=false;
                std::function<void(lv_obj_t*)> walk=[&](lv_obj_t* object){
                    if(lv_obj_has_class(object,&lv_label_class)&&std::string(lv_label_get_text(object)).rfind(prefix,0)==0)found=true;
                    for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)walk(lv_obj_get_child(object,i));
                };walk(lv_layer_top());return found;
            };
            Require(hasStatus("Ready for MicroPython / LVGL 9.3.0"),"MicroPython preflight did not validate the project");
            editor.MenuAction("confirm-file");
            Require(!FindText(lv_layer_top(),"Export MicroPython project"),"MicroPython export failed to close its dialog");
            for(const char* name:{"ui.py","ui.json","uicreator_lvgl.py","main.py","README.md","export-manifest.json"})
                Require(std::filesystem::is_regular_file(output/name),"MicroPython export is missing a required file");
            json manifest;std::ifstream(output/"export-manifest.json")>>manifest;
            Require(manifest.at("format")=="micropython"&&manifest.at("lvglVersion")=="9.3.0","MicroPython manifest uses the wrong binding target");
            Require(editor.ProjectDocument()==document&&editor.Dirty()==dirty,"Export changed the authored project or its saved state");
            editor.MenuAction("cancel-file");
            auto legacy=editor.ProjectDocument();
            legacy["obj"].push_back({{"oid",2},{"par",-1},{"base",json::object()},{"advancedKeyboard",json::object()}});
            editor.ApplyDocument(std::move(legacy));
            editor.MenuAction("export-micropython");title=FindText(lv_layer_top(),"Export MicroPython project");
            field=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
            const auto unsupported=output/"unsupported-keyboard";lv_textarea_set_text(field,unsupported.u8string().c_str());
            editor.MenuAction("export-preflight");Require(hasStatus("Export blocked:"),"Unavailable native extensions were silently exported");
            editor.MenuAction("confirm-file");Require(!std::filesystem::exists(unsupported),"Rejected MicroPython export created output files");
            Require(FindText(lv_layer_top(),"Export MicroPython project"),"Failed export did not keep its editable dialog open");
            editor.MenuAction("cancel-file");
            std::cout<<"MicroPython menu, preflight, output files, unchanged document and unsupported native-extension rejection passed\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--file-browser") {
            const auto folder=std::filesystem::temp_directory_path()/("uicreator-file-browser-"+std::to_string(SDL_GetTicksNS()));
            std::filesystem::create_directories(folder/"Browser folder");
            {std::ofstream output(folder/"Visible example.txt");output<<"example";}
            {std::ofstream output(folder/"Excluded example.bin");output<<"binary";}
            FileBrowser browser(folder.u8string(),std::vector<std::string>{".txt"},lv_layer_top());
            std::function<unsigned(lv_obj_t*,const char*)> count=[&](lv_obj_t* object,const char* text){
                unsigned matches=lv_obj_has_class(object,&lv_label_class)&&std::string(lv_label_get_text(object))==text?1:0;
                for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)matches+=count(lv_obj_get_child(object,i),text);
                return matches;
            };
            Require(count(lv_layer_top(),"Browser folder")==1,"File browser duplicated or omitted a directory");
            Require(count(lv_layer_top(),"Visible example.txt")==1,"File browser omitted the matching file");
            Require(count(lv_layer_top(),"Excluded example.bin")==0,"File browser ignored its extension filter");
            std::cout<<"File browser lists matching files and folders exactly once\n";return 0;
        }
        simulator.SetShortcutHandler([&](int key,bool control,bool shift,bool alt){return editor.HandleShortcut(key,control,shift,alt);});
        lv_obj_update_layout(lv_screen_active());
        if(argc==2&&std::string(argv[1])=="--tft-settings") {CheckTftSettings(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--screen-memory") {CheckScreenMemory(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--text-input-modes") {CheckTextInputModes(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--editor-input-matrix") {CheckEditorInputMatrix(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--editor-input-assets") {CheckEditorInputAssets(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--editor-input-tools") {CheckEditorInputTools(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--editor-input-properties") {CheckEditorInputProperties(editor,simulator);return 0;}
        if(argc==2&&std::string(argv[1])=="--builtin-fonts") {
            const auto& catalog=BuiltinFonts::Catalog();Require(!catalog.empty(),"Built-in font catalog is empty");
            for(const char* id:{"badh-regular","caliper-regular","microgramma-d-medium-extended","ocr-a-extended"})Require(BuiltinFonts::Find(id),"An original built-in font lost its stable ID");
            const auto* popular=BuiltinFonts::Find("lato-regular");Require(popular&&popular->name=="Lato Regular","The popular regular font source is unavailable");
            for(const auto& entry:catalog)Require(std::filesystem::is_regular_file(entry.path),"A bundled font source is missing from the application resources");
            ClickPaletteWidget(simulator,"Label");
            const auto before=editor.ProjectDocument();Require(before.value("fonts",json::array()).empty(),"Built-in catalog was eagerly added to the project");
            auto window=[&](const char* title){auto* text=FindText(lv_layer_top(),title);Require(text,"Font dialog is missing");return lv_obj_get_parent(lv_obj_get_parent(text));};
            auto field=[&](lv_obj_t* body,const char* caption){auto* text=FindText(body,caption);Require(text,"Font field is missing");return lv_obj_get_child(lv_obj_get_parent(text),1);};
            auto click=[&](lv_obj_t* body,const char* caption){auto* text=FindText(body,caption);Require(text,"Font action is missing");lv_obj_send_event(lv_obj_get_parent(text),LV_EVENT_CLICKED,nullptr);};
            auto close=[&](lv_obj_t* root){lv_obj_send_event(EditorWindow::CloseButton(root),LV_EVENT_CLICKED,nullptr);};
            auto wait=[&](lv_obj_t* body){auto* status=lv_obj_get_child(body,-1);const auto start=SDL_GetTicks();while(std::string(lv_label_get_text(status)).rfind("Preparing font",0)==0&&SDL_GetTicks()-start<30000)simulator.ProcessEvents();std::cerr<<lv_label_get_text(status)<<'\n';return std::string(lv_label_get_text(status));};
            auto* fontChoice=field(lv_screen_active(),"Font");
            for(const auto& entry:catalog)Require(std::string(lv_dropdown_get_options(fontChoice)).find("Built-in: "+entry.name+"...")!=std::string::npos,"A built-in font is missing from widget font choices");
            const auto initialChoice=lv_dropdown_get_selected(fontChoice);
            const auto selectBuiltin=[&]{const auto option=lv_dropdown_get_option_index(fontChoice,("Built-in: "+catalog.front().name+"...").c_str());Require(option>=0,"Built-in font option is missing");lv_dropdown_set_selected(fontChoice,option);lv_obj_send_event(fontChoice,LV_EVENT_VALUE_CHANGED,nullptr);};
            selectBuiltin();auto* root=window("Add project font");auto* body=EditorWindow::Content(root);
            Require(lv_dropdown_get_selected(fontChoice)==initialChoice,"Opening built-in font options changed the widget's actual font selection");
            Require(lv_dropdown_get_selected(field(body,"Source font"))==1,"Inspector did not preselect the chosen built-in source");
            Require(lv_obj_is_hidden(lv_obj_get_parent(field(body,"Font file"))),"A built-in font requires the user to manage its source path");
            lv_textarea_set_text(field(body,"Size (pixels)"),"28");lv_textarea_set_text(field(body,"Unicode ranges"),"0x41-0x5A");
            click(body,"Preview");Require(wait(body).find("Preview ready")!=std::string::npos,"Built-in preview failed");
            Require(editor.ProjectDocument()==before,"Previewing a built-in font added unused font data to the project");close(root);
            Require(editor.ProjectDocument()==before,"Cancelling built-in font selection changed the project");
            selectBuiltin();root=window("Add project font");body=EditorWindow::Content(root);
            lv_textarea_set_text(field(body,"Size (pixels)"),"28");lv_textarea_set_text(field(body,"Unicode ranges"),"0x41-0x5A");
            click(body,"Add and apply");Require(wait(body).find("saved and applied")!=std::string::npos,"Built-in font was not prepared and applied");
            auto document=editor.ProjectDocument();Require(document["fonts"].size()==1,"Selecting one built-in font stored the unused catalog fonts");
            const auto& font=document["fonts"][0];Require(font["name"]==catalog.front().name+" 28 px","Automatic font name did not use the catalog's friendly name");
            Require(font["conversion"]["sources"][0]["builtinId"]==catalog.front().id,"Built-in source identity was not remembered");
            Require(!font["conversion"]["sources"][0].contains("path"),"Built-in settings depend on a machine-specific source path");
            const auto id=font["id"];Require(document["obj"][0]["base"]["fontId"]==id,"Prepared built-in font was not bound to its widget");close(root);
            // Simulate a project copied from another installation/platform.
            document["fonts"][0]["conversion"]["sources"][0]["path"]="/missing/old-install/font.ttf";editor.ApplyDocument(document);
            editor.MenuAction("font-manager");auto* manager=window("Project fonts");click(EditorWindow::Content(manager),"Edit font...");
            root=window("Edit project font");body=EditorWindow::Content(root);
            Require(std::string(lv_textarea_get_text(field(body,"Font file")))==catalog.front().path.u8string(),"Editing did not resolve the built-in source in this installation");
            Require(lv_dropdown_get_selected(field(body,"Source font"))==1,"Editing lost the built-in source choice");
            lv_textarea_set_text(field(body,"Size (pixels)"),"30");click(body,"Save font");Require(wait(body).find("Font saved")!=std::string::npos,"Editing a moved built-in font failed");close(root);close(manager);
            Require(editor.ProjectDocument()["fonts"][0]["id"]==id,"Editing a built-in source replaced its stable font asset");
            editor.MenuAction("font-converter");root=window("Add project font");body=EditorWindow::Content(root);auto* sourceChoice=field(body,"Source font");
            for(const auto& entry:catalog)Require(std::string(lv_dropdown_get_options(sourceChoice)).find(entry.name)!=std::string::npos,"Add font dialog is missing a built-in source");
            lv_dropdown_set_selected(sourceChoice,static_cast<uint32_t>(catalog.size()));lv_obj_send_event(sourceChoice,LV_EVENT_VALUE_CHANGED,nullptr);
            Require(std::string(lv_textarea_get_text(field(body,"Font file")))==catalog.back().path.u8string(),"Choosing a bundled source did not resolve its font file");
            lv_dropdown_set_selected(sourceChoice,0);lv_obj_send_event(sourceChoice,LV_EVENT_VALUE_CHANGED,nullptr);
            Require(!lv_obj_is_hidden(lv_obj_get_parent(field(body,"Font file"))),"Choosing a custom source did not restore the file picker");close(root);
            Require(editor.ProjectDocument()["fonts"].size()==1,"Browsing unused built-in sources stored extra font assets");
            auto* simulatorTitle=FindText(lv_screen_active(),"Simulator [Size:320x240]");Require(simulatorTitle,"Simulator is missing");
            auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(simulatorTitle))),0);
            {EditorEdit::Scope edit;lv_label_set_text(lv_obj_get_child(surface,0),"ABC");}
            editor.MenuAction("font-converter");root=window("Add project font");body=EditorWindow::Content(root);sourceChoice=field(body,"Source font");
            const auto popularOption=lv_dropdown_get_option_index(sourceChoice,popular->name.c_str());Require(popularOption>0,"A popular font is missing from the source picker");
            lv_dropdown_set_selected(sourceChoice,static_cast<uint32_t>(popularOption));lv_obj_send_event(sourceChoice,LV_EVENT_VALUE_CHANGED,nullptr);
            lv_textarea_set_text(field(body,"Size (pixels)"),"22");lv_textarea_set_text(field(body,"Unicode ranges"),"0x41-0x43");lv_textarea_set_text(field(body,"Preview text"),"ABC");
            click(body,"Add and apply");Require(wait(body).find("saved and applied")!=std::string::npos,"A popular font could not be prepared and applied");close(root);
            document=editor.ProjectDocument();Require(document["fonts"].size()==2,"Choosing a popular font prepared unused catalog entries");
            const auto selectedFont=document["obj"][0]["base"]["fontId"].get<std::string>();
            const auto used=std::find_if(document["fonts"].begin(),document["fonts"].end(),[&](const json& item){return item.at("id")==selectedFont;});
            Require(used!=document["fonts"].end()&&(*used)["conversion"]["sources"][0]["builtinId"]==popular->id,"Popular font selection did not use the bundled source");
            Require((*used)["font"]["dsc"]["GlyphDsc"].size()==4,"Popular font preparation ignored the selected character subset");
            const auto device=Serialization::ProjectScreens::Device(document);Require(device["fonts"].size()==1&&device["fonts"][0]["id"]==selectedFont,"Device export retained an unused font source");
            const auto protobuf=Serialization::Protobuf::Encode(Serialization::EntityKind::Project,"project",device);
            Require(Serialization::Protobuf::Decode(protobuf).data["fonts"].size()==1,"Device Protobuf retained unused catalog fonts");
            std::cout<<"Built-in font choices cover lazy preview, cancellation, preparation, friendly names, stable source IDs, moved projects, popular font subsets and used-only device export\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--font-assets") {
            ClickPaletteWidget(simulator,"Label");
            auto* simulatorTitle=FindText(lv_screen_active(),"Simulator [Size:320x240]");Require(simulatorTitle,"Simulator is missing");
            auto* surface=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(simulatorTitle))),0);
            auto label=[&]{return lv_obj_get_child(surface,0);};
            {EditorEdit::Scope edit;lv_obj_set_size(label(),180,48);}
            const auto before=Serialization::ProjectScreens::Canonical(editor.ProjectDocument());
            const auto originalFontHeight=lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height;
            const auto sourceRoot=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
            const auto work=std::filesystem::temp_directory_path()/("uicreator-font-assets-"+std::to_string(SDL_GetTicksNS()));std::filesystem::create_directories(work);
            const auto source=work/"Body source.ttf";std::filesystem::copy_file(sourceRoot/"3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf",source);
            auto window=[&](const char* title){auto* text=FindText(lv_layer_top(),title);Require(text,"Font dialog is missing");return lv_obj_get_parent(lv_obj_get_parent(text));};
            auto field=[&](lv_obj_t* body,const char* caption){auto* text=FindText(body,caption);Require(text,"Font field is missing");return lv_obj_get_child(lv_obj_get_parent(text),1);};
            auto click=[&](lv_obj_t* body,const char* caption){auto* text=FindText(body,caption);Require(text,"Font action is missing");lv_obj_send_event(lv_obj_get_parent(text),LV_EVENT_CLICKED,nullptr);};
            auto wait=[&](lv_obj_t* body){auto* status=lv_obj_get_child(body,-1);const auto start=SDL_GetTicks();while(std::string(lv_label_get_text(status)).rfind("Preparing font",0)==0&&SDL_GetTicks()-start<30000)simulator.ProcessEvents();std::cerr<<lv_label_get_text(status)<<'\n';return std::string(lv_label_get_text(status));};
            auto close=[&](lv_obj_t* root){lv_obj_send_event(EditorWindow::CloseButton(root),LV_EVENT_CLICKED,nullptr);};
            editor.MenuAction("font-converter");auto* root=window("Add project font");auto* body=EditorWindow::Content(root);
            for(const char* caption:{"Output path","Output format","Font symbol","LVGL include path","Fallback font symbol"})Require(!FindText(body,caption),"Font asset creation still requires an output detail");
            lv_textarea_set_text(field(body,"Font file"),source.u8string().c_str());lv_textarea_set_text(field(body,"Font name"),"Body font");
            lv_textarea_set_text(field(body,"Unicode ranges"),"0x20-0x7E,0xE9");
            click(body,"Preview");Require(wait(body).find("Preview ready")!=std::string::npos,"Font preview failed");
            Require(Serialization::ProjectScreens::Canonical(editor.ProjectDocument())==before,"Preview changed the authored document");
            click(body,"Add and apply");Require(wait(body).find("saved and applied")!=std::string::npos,"Font was not added and applied in one action");
            auto document=editor.ProjectDocument();Require(document["fonts"].size()==1,"Font was not stored once in the project");const auto id=document["fonts"][0]["id"].get<std::string>();
            Require(document["fonts"][0]["font"]["dsc"].value("bitmapFmt",0)>0,"The stored font lost the selected compression");
            Require(document["fonts"][0]["conversion"]["sources"][0]["ranges"]=="0x20-0x7E,0xE9","Character selection was not remembered");
            Require(document["obj"][0]["base"]["fontId"]==id,"Added font was not bound to its widget");
            const auto initialHeight=lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height;close(root);
            Require(editor.Undo(),"Adding a font cannot be undone");auto afterUndo=Serialization::ProjectScreens::Canonical(editor.ProjectDocument());auto expectedUndo=before;
            const auto stripHistoryIndex=[](const auto& self,json& value)->void {if(value.is_array())for(auto& item:value)self(self,item);else if(value.is_object()){value.erase("hostIndex");for(auto& item:value.items())self(self,item.value());}};
            stripHistoryIndex(stripHistoryIndex,afterUndo);stripHistoryIndex(stripHistoryIndex,expectedUndo);
            if(afterUndo!=expectedUndo){std::ofstream(work/"before-font-add.json")<<expectedUndo.dump(2);std::ofstream(work/"after-font-undo.json")<<afterUndo.dump(2);std::cerr<<"Font undo diagnostics: "<<work.u8string()<<'\n'<<json::diff(expectedUndo,afterUndo).dump(2)<<'\n';}
            Require(afterUndo.value("fonts",json::array()).empty(),"Undo left the added font in the library");
            Require(!afterUndo["obj"][0]["base"].contains("fontId"),"Undo left the added font bound to its widget");
            Require(lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height==originalFontHeight,"Undo did not restore the original widget font");
            Require(afterUndo==expectedUndo,"Undo did not restore the authored document");
            Require(editor.Redo(),"Adding a font cannot be redone");Require(editor.ProjectDocument()["fonts"].size()==1,"Redo lost the project font");
            editor.DuplicateScreen(0);Require(editor.ScreenCount()==2,"Font screen duplication failed");Require(editor.SelectScreen(0),"Could not return to the first font screen");
            editor.MenuAction("font-manager");auto* manager=window("Project fonts");auto* managerBody=EditorWindow::Content(manager);click(managerBody,"Edit font...");
            root=window("Edit project font");body=EditorWindow::Content(root);
            Require(std::string(lv_textarea_get_text(field(body,"Font file")))==source.u8string(),"Editing a font lost its source setting");
            Require(std::string(lv_textarea_get_text(field(body,"Unicode ranges")))=="0x20-0x7E,0xE9","Editing a font lost its character ranges");
            lv_textarea_set_text(field(body,"Size (pixels)"),"32");click(body,"Save font");Require(wait(body).find("Font saved")!=std::string::npos,"Editing the font failed");close(root);close(manager);
            const auto updatedHeight=lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height;Require(updatedHeight>initialHeight,"Editing a font did not update its existing widget");
            Require(editor.ProjectDocument()["fonts"][0]["id"]==id,"Editing a font changed its stable identifier");
            Require(editor.SelectScreen(1),"Could not switch to the second font screen");Require(lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height==updatedHeight,"Another screen retained the old font bitmap");
            Require(editor.SelectScreen(0),"Could not return to the first screen");lv_obj_send_event(label(),LV_EVENT_CLICKED,nullptr);
            auto* fontChoice=field(lv_screen_active(),"Font");lv_dropdown_set_selected(fontChoice,0);lv_obj_send_event(fontChoice,LV_EVENT_VALUE_CHANGED,nullptr);
            editor.MenuAction("font-manager");manager=window("Project fonts");managerBody=EditorWindow::Content(manager);click(managerBody,"Remove unused font");
            Require(std::string(lv_label_get_text(lv_obj_get_child(managerBody,-1))).find("in use")!=std::string::npos,"A font used on another screen was removed");close(manager);
            const auto saved=work/"fonts.json";editor.MenuAction("save-as");auto* saveWindow=window("Save project");
            lv_textarea_set_text(lv_obj_get_child(EditorWindow::Content(saveWindow),0),saved.u8string().c_str());editor.MenuAction("confirm-file");Require(std::filesystem::is_regular_file(saved),"Saving a project font failed");
            std::filesystem::remove(source);editor.MenuAction("new");editor.MenuAction("open");auto* openWindow=window("Open project");
            lv_textarea_set_text(lv_obj_get_child(EditorWindow::Content(openWindow),0),saved.u8string().c_str());editor.MenuAction("confirm-file");Require(!FindText(lv_layer_top(),"Open project"),"Opening a saved font project failed");
            Require(editor.SelectScreen(1),"Saved font screen was not restored");Require(lv_obj_get_style_text_font(label(),LV_PART_MAIN)->line_height==updatedHeight,"A saved font required the original source file");
            Require(editor.ProjectDocument()["fonts"][0]["font"]["dsc"].value("bitmapFmt",0)>0,"Save/load lost compressed font data");
            // References outside the visible canvas must also protect assets.
            FontLibrary fonts;auto fontId=fonts.Add("Reference fixture",editor.ProjectDocument()["fonts"][0]["font"]);
            auto referenceStyle=json::array({{{"selector",0},{"values",{{"text_font",{{"projectFont",fontId}}}}}}});
            auto referenceWidget=json{{"base",{{"fontId",fontId},{"nativeStyles",referenceStyle}}}};
            auto references=json{{"fonts",fonts.ToJSON()}};
            references["screens"]=json::array({{{"obj",json::array({referenceWidget})}}});
            references["sharedStyles"]=referenceStyle;
            referenceWidget["base"].erase("fontId");referenceWidget["base"]["nativeStyles"][0]["values"]["text_font"]=fonts.Record(fontId)["font"];
            references["components"]=json::array({{{"objects",json::array({referenceWidget})}}});
            Require(fonts.References(references,fontId)==3,"Font reference counting lost a shared style/component or counted a widget twice");
            auto* styled=lv_label_create(lv_layer_top());lv_obj_set_style_text_font(styled,fonts.Get(fontId),LV_PART_MAIN|LV_STATE_CHECKED);lv_obj_set_checked(styled,true);
            fonts.Update(fontId,"Updated fixture",Serialization::LVFont::ToJSON(*LV_FONT_DEFAULT),json::object(),styled);
            Require(lv_obj_get_style_text_font(styled,LV_PART_MAIN)==fonts.Get(fontId),"Updating a project font left a part/state style using its old bitmap");lv_obj_delete(styled);
            std::cout<<"Project fonts cover output-free preview/add/apply, one-step undo/redo, editing, cross-screen rebinding/removal protection and source-independent reload\n";return 0;
        }
        if((argc==2||argc==3)&&std::string(argv[1])=="--object-icons") {CheckObjectIcons(editor,simulator,argc==3?std::filesystem::u8path(argv[2]):std::filesystem::path{});return 0;}
        if((argc==2||argc==3)&&std::string(argv[1])=="--hierarchy-icons") {CheckHierarchyIcons(editor,simulator,argc==3?std::filesystem::u8path(argv[2]):std::filesystem::path{});return 0;}
        if(argc==3&&std::string(argv[1])=="--menu-toolbar-icons") {CheckMenuToolbarIcons(editor,simulator,std::filesystem::u8path(argv[2]));return 0;}
        if(argc==3&&std::string(argv[1])=="--theme-gallery") {CheckThemeGallery(editor,simulator,std::filesystem::u8path(argv[2]));return 0;}
        if(argc==3&&std::string(argv[1])=="--variables") {CheckVariablesEditor(editor,simulator,std::filesystem::u8path(argv[2]));return 0;}
        if(argc==2&&std::string(argv[1])=="--polish-core") {CheckPolishCore(editor,simulator);return 0;}
        if(argc==3&&std::string(argv[1])=="--visual-baselines") {ExportVisualRegressionFixtures(editor,simulator,std::filesystem::u8path(argv[2]));return 0;}
        if((argc==2||argc==3)&&(std::string(argv[1])=="--visual-authoring"||std::string(argv[1])=="--workflow-problems"||std::string(argv[1])=="--performance")){
            if(std::string(argv[1])=="--visual-authoring")CheckVisualAuthoringEditor(editor,simulator);
            else if(std::string(argv[1])=="--workflow-problems")CheckWorkflowProblems(editor,simulator);
            else CheckPerformanceEditor(editor,simulator);
            if(argc==3){for(int i=0;i<20;++i){SDL_Delay(16);simulator.ProcessEvents();}auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture workflow");const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save workflow preview");}
            return 0;
        }
        if(argc==2 && std::string(argv[1])=="--device-playground") {CheckDevicePlayground(editor,simulator);return 0;}
        if((argc==2 || argc==3) && (std::string(argv[1])=="--design-tools" || std::string(argv[1])=="--interactions")) {
            if(std::string(argv[1])=="--design-tools")CheckDesignToolsEditor(editor,simulator);else CheckInteractionsEditor(editor,simulator);
            if(argc==3) {
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture design tools");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save design tools preview");
            }
            return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--run-mode") {
            CheckRunMode(editor,simulator);
            if(argc==3) {
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture Run mode");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save Run preview");
            }
            return 0;
        }
        if(argc==2 && std::string(argv[1])=="--code-widgets") {
            CheckCodeWidgetEditor(editor,simulator);return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--menus") {
            CheckMenusEditor(editor,simulator);
            if(argc==3) {
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture editor menus");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save menu preview");
            }
            editor.MenuAction("dismiss");return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--language-models") {
            CheckLanguageModelsEditor(editor,simulator);
            if(argc==3) {
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture language model generator");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save language model generator preview");
            }
            return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--project-keyboard") {
            CheckProjectKeyboardEditor(editor,simulator);
            if(argc==3) {
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture project keyboard properties");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save project keyboard properties");
            }
            return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--advanced-keyboard") {
            CheckAdvancedKeyboardEditor(editor,simulator);
            if(argc==3) {
                editor.MenuAction("fit-canvas");
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer=static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* capture=SDL_RenderReadPixels(renderer,nullptr);Require(capture,"Could not capture Advanced Keyboard preview");
                const bool saved=SDL_SaveBMP(capture,argv[2]);SDL_DestroySurface(capture);Require(saved,"Could not save Advanced Keyboard preview");
            }
            return 0;
        }
        if(argc==2 && std::string(argv[1])=="--debug-screen") {
            Click("Bar");auto debugProject=editor.ProjectDocument();
            const auto firstBar=debugProject["obj"][0];
            Serialization::ProjectScreens::Add(debugProject,"Debug second page");
            debugProject["obj"]=json::array({firstBar});debugProject["activeScreen"]=0;
            debugProject["previewProfiles"]=json::array();debugProject["accessibility"]={{"minimumContrast",7.0}};
            Serialization::ProjectScreens::SyncAlias(debugProject);editor.ApplyDocument(std::move(debugProject));
            Require(editor.ActiveScreenIndex()==1,"Debug fixture did not select its second screen");
            const auto before=editor.ProjectDocument();
            Require(editor.HandleShortcut(SDLK_F5,false),"F5 did not invoke the display library debugger");
            Require(editor.DebugRunning(),"Standalone runtime viewer did not start");
            const auto path=std::filesystem::u8path(editor.DebugSnapshotPath());
            auto read=[&] {std::ifstream input(path,std::ios::binary);Serialization::Binary bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());return Serialization::Protobuf::Decode(bytes).data;};
            auto document=read();Require(document["runtimeTarget"]=="9.6" && document["screens"].size()==2 && document["activeScreen"]==1 && document["screens"][1]["obj"][0].contains("bar"),"Debug snapshot did not select the current authored screen");
            for(const auto* key:{"obj","canvas","editorScreen","previewProfiles","deviceTargets","accessibility","editorSession"})
                Require(!document.contains(key),"Debug snapshot retained authoring-only root metadata");
            Require(editor.ProjectDocument()==before,"Debugging changed the editor document");
            auto* title=FindText(lv_screen_active(),"Simulator [Size:320x240]");auto* host=lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))),0);
            auto* bar=lv_obj_get_child(host,0);
            {EditorEdit::Scope edit(bar);lv_bar_set_value(bar,75,LV_ANIM_OFF);}
            editor.MenuAction("debug-screen");
            Require(editor.DebugSnapshotPath()==path.u8string() && read()["screens"][1]["obj"][0]["bar"]["value"]==75,"Debug refresh did not replace the watched snapshot");
            for(int frame=0;frame<150;++frame)simulator.ProcessEvents();
            Require(editor.DebugRunning(),"Runtime viewer failed while loading the authored screen");
            editor.MenuAction("debug-stop");Require(!editor.DebugRunning(),"Stop debug did not stop the owned viewer");
            std::cout<<"Editor F5, Protobuf snapshot, watched refresh and runtime-viewer lifecycle passed\n";return 0;
        }
        if((argc==2 || argc==3) && std::string(argv[1])=="--bootstrap-palette") {
            Require(editor.ProjectDocument()["obj"].empty() && !editor.Dirty(), "New editor is not clean");
            auto* title = FindText(lv_screen_active(), "Simulator [Size:320x240]");
            Require(title, "Simulator is missing");
            auto* simulatorWindow = lv_obj_get_parent(lv_obj_get_parent(title));
            auto* surface = lv_obj_get_child(EditorWindow::Content(simulatorWindow), 0);
            auto closeDialog = [&](const char* name) {
                auto* label = FindText(lv_layer_top(), name);
                Require(label, "Product dialog did not open");
                auto* window = lv_obj_get_parent(lv_obj_get_parent(label));
                auto* close = EditorWindow::CloseButton(window);
                Require(close, "Product dialog has no close control");
                lv_obj_send_event(close, LV_EVENT_CLICKED, nullptr);
                Require(!FindText(lv_layer_top(), name), "Product dialog close failed");
            };
            Click("Bootstrap");
            for(const char* name : {"Accordion", "Checks and radios", "Data table", "Navs and tabs", "Popover"}) {
                Click(name);
                auto document = editor.ProjectDocument();
                Require(!document["obj"].empty() && editor.Dirty(), "Bootstrap palette did not create dirty document objects");
                const auto count = document["obj"].size();
                Require(editor.Undo() && editor.ProjectDocument()["obj"].empty() && !editor.Dirty(), "Bootstrap insertion was not one undo step");
                Require(editor.Redo() && editor.ProjectDocument()["obj"].size() == count, "Bootstrap redo lost nested objects");
                Require(editor.Undo() && editor.ProjectDocument()["obj"].empty(), "Bootstrap cleanup undo failed");
            }
            Click("Templates"); Click("Admin dashboard");
            const auto dashboard = editor.ProjectDocument();
            Require(dashboard["obj"].size() > 20, "Admin dashboard did not include its native content");
            bool chart = false, table = false;
            for(const auto& record : dashboard["obj"]) {chart = chart || record.contains("chart"); table = table || record.contains("table");}
            Require(chart && table, "Dashboard serialization lost charts or records");
            Require(editor.Undo() && editor.ProjectDocument()["obj"].empty(), "Dashboard insertion was not one undo step");
            for(const char* action : {"template:dashboard", "template:signin", "template:gallery"}) {
                editor.MenuAction(action);
                Require(!editor.ProjectDocument()["obj"].empty(), "Template action did not create a project");
                Require(editor.Undo() && editor.ProjectDocument()["obj"].empty(), "Template action undo failed");
            }
            for(const auto& item : {std::make_pair("about", "About LVGL UI Creator"),
                std::make_pair("shortcuts", "Keyboard shortcuts"), std::make_pair("diagnostics", "Diagnostics report"),
                std::make_pair("welcome", "Welcome to LVGL UI Creator")}) {
                editor.MenuAction(item.first); closeDialog(item.second);
                Require(!editor.Dirty(), "Product information dialog changed the document");
            }
            for(const char* name : {"Admin dashboard", "Sign-in form", "Component gallery"}) {
                editor.MenuAction("welcome");
                auto* label = FindText(lv_layer_top(), name);
                Require(label, "Welcome example action missing");
                lv_obj_send_event(lv_obj_get_parent(label), LV_EVENT_CLICKED, nullptr);
                Require(!FindText(lv_layer_top(), "Welcome to LVGL UI Creator") && editor.Dirty(), "Welcome did not close and insert its example");
                Require(editor.Undo() && editor.ProjectDocument()["obj"].empty(), "Welcome example insertion was not undoable");
            }
            editor.MenuAction("template:dashboard");
            editor.MenuAction("export-c"); editor.MenuAction("export-preflight");
            bool ready = false;
            std::function<void(lv_obj_t*)> findReady = [&](lv_obj_t* object) {
                if(lv_obj_get_class(object) == &lv_label_class && std::string(lv_label_get_text(object)).find("Ready for LVGL 9.6") == 0) ready = true;
                for(uint32_t index = 0; index < lv_obj_get_child_count(object); ++index) findReady(lv_obj_get_child(object, index));
            };
            findReady(lv_layer_top()); Require(ready, "Dashboard export preflight did not report readiness");
            editor.MenuAction("cancel-file");
            Require(lv_obj_get_child_count(surface) == 1, "Template action created duplicate top-level widgets");
            if(argc==3) {
                editor.MenuAction("fit-canvas");
                for(int frame=0;frame<20;++frame){SDL_Delay(16);simulator.ProcessEvents();}
                auto* renderer = static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(lv_display_get_default()));
                auto* image = SDL_RenderReadPixels(renderer, nullptr);
                Require(image, "Could not capture Bootstrap editor preview");
                const bool saved = SDL_SaveBMP(image, argv[2]); SDL_DestroySurface(image);
                Require(saved, "Could not save Bootstrap editor preview");
            }
            std::cout << "Bootstrap palettes, native templates, dirty tracking, undo/redo, welcome routes and product dialogs passed\n";
            return 0;
        }
        if(argc==2 && std::string(argv[1])=="--window-close") {
            Click("Bar");const auto original=editor.ProjectDocument();
            auto windowFor=[&](lv_obj_t* layer,const char* title){auto* label=FindText(layer,title);Require(label,"Expected editor window is missing");return lv_obj_get_parent(lv_obj_get_parent(label));};
            auto close=[&](lv_obj_t* window){lv_obj_update_layout(window);auto* button=EditorWindow::CloseButton(window);Require(button,"Window has no X button");auto* header=EditorWindow::Header(window);Require(lv_obj_get_child(header,-1)==button,"X is not the rightmost header control");lv_area_t hb,bb;lv_obj_get_coords(header,&hb);lv_obj_get_coords(button,&bb);Require(bb.x2<=hb.x2 && bb.x2>=hb.x2-14 && bb.y1>=hb.y1,"Close button is not in the upper-right corner");lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);};
            std::vector<std::pair<lv_obj_t*,std::string>> panels;
            auto* previewWindow=windowFor(lv_screen_active(),"Simulator [Size:320x240]");
            Require(EditorWindow::CloseButton(previewWindow)==nullptr,"Preview must not offer a close button");
            Require(FindText(EditorWindow::Header(previewWindow),LV_SYMBOL_SETTINGS),"Preview settings control is missing");
            for(const auto& item:{std::make_pair("Screens","screens"),std::make_pair("Tool Tray","widgets"),std::make_pair("Hierarchy","hierarchy"),std::make_pair("Properties","properties"),std::make_pair("Tools","tools")}) {
                auto* window=windowFor(lv_screen_active(),item.first);panels.push_back({window,item.second});
                lv_obj_t* focusedField=nullptr;
                if(std::string(item.second)=="hierarchy") {
                    std::function<void(lv_obj_t*)> findField=[&](lv_obj_t* object){if(lv_obj_get_class(object)==&lv_textarea_class)focusedField=object;for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)findField(lv_obj_get_child(object,i));};findField(window);
                    Require(focusedField,"Hierarchy search field is missing");FocusEditorControl(lv_indev_get_group(simulator.GetKeyboard()),focusedField);
                }
                close(window);Require(lv_obj_is_valid(window)&&lv_obj_is_hidden(window),"Closing a dock destroyed it or left it visible");
                if(focusedField)Require(lv_group_get_focused(lv_indev_get_group(simulator.GetKeyboard()))!=focusedField,"Closed window retained keyboard focus");
                Require(editor.ProjectDocument()==original,"Closing a panel changed project data");
                editor.MenuAction(std::string("view:")+item.second);Require(!lv_obj_is_hidden(window),"View did not restore the closed panel");
            }
            for(const auto& item:panels)close(item.first);
            Require(editor.HandleShortcut('v',false,false,true),"Alt+V did not open View");
            Require(FindText(lv_layer_top(),"Restore all panels"),"View menu is missing restore-all");editor.MenuAction("view:all");
            for(const auto& item:panels)Require(!lv_obj_is_hidden(item.first),"Restore-all left a panel hidden");
            auto* tray=windowFor(lv_screen_active(),"Tool Tray");auto* minimize=FindText(EditorWindow::Header(tray),LV_SYMBOL_DOWN);Require(minimize,"Minimize control is missing");lv_obj_send_event(lv_obj_get_parent(minimize),LV_EVENT_CLICKED,nullptr);Require(lv_obj_is_hidden(tray),"Minimize did not hide the tray");editor.MenuAction("view:widgets");
            unsigned visibleTitles=0;std::function<void(lv_obj_t*)> countTitles=[&](lv_obj_t* object){if(lv_obj_get_class(object)==&lv_label_class && std::string(lv_label_get_text(object))=="Tool Tray" && lv_obj_is_visible(object))++visibleTitles;for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)countTitles(lv_obj_get_child(object,i));};lv_obj_update_layout(lv_screen_active());countTitles(lv_screen_active());Require(visibleTitles==1,"View restoration left a minimized icon behind");
            for(const auto& item:{std::make_pair("open","Open project"),std::make_pair("save","Save project"),std::make_pair("export-c","Export C project"),std::make_pair("export-protobuf","Export device project"),std::make_pair("export-micropython","Export MicroPython project"),std::make_pair("export-xml","Export UI Creator XML"),std::make_pair("about","About LVGL UI Creator"),std::make_pair("font-manager","Project fonts"),std::make_pair("image-manager","Project images"),std::make_pair("font-converter","Add project font"),std::make_pair("image-converter","Image converter"),std::make_pair("project-creator","Create LVGL firmware project")}) {
                for(int i=0;i<2;++i){editor.MenuAction(item.first);auto* window=windowFor(lv_layer_top(),item.second);close(window);Require(!FindText(lv_layer_top(),item.second),"Dialog X did not close its window");}
            }
            Require(editor.ProjectDocument()==original,"Closing dialogs changed project data");
            auto* simulatorWindow=windowFor(lv_screen_active(),"Simulator [Size:320x240]");auto* settings=FindText(EditorWindow::Header(simulatorWindow),LV_SYMBOL_SETTINGS);Require(settings,"Simulator settings control is missing");
            for(int i=0;i<2;++i){
                lv_obj_send_event(lv_obj_get_parent(settings),LV_EVENT_CLICKED,nullptr);auto* window=windowFor(lv_screen_active(),"TFT Settings");Require(!lv_obj_is_hidden(window),"TFT settings did not reopen");
                if(i==0) {
                    lv_obj_update_layout(window);lv_area_t area;lv_obj_get_coords(EditorWindow::CloseButton(window),&area);
                    for(auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
                        SDL_Event event{};event.type=type;event.button.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));event.button.button=SDL_BUTTON_LEFT;event.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.x=(area.x1+area.x2)/2.0f;event.button.y=(area.y1+area.y2)/2.0f;SDL_PushEvent(&event);for(int frame=0;frame<10;++frame)simulator.ProcessEvents();
                    }
                } else close(window);
                Require(lv_obj_is_hidden(window),"TFT settings X did not hide the dialog");
            }
            Click("Advanced\nWidgets");Click("Button matrix");
            auto* surface=lv_obj_get_child(EditorWindow::Content(simulatorWindow),0);lv_obj_t* matrix=nullptr;
            for(uint32_t i=0;i<lv_obj_get_child_count(surface);++i)if(lv_obj_get_class(lv_obj_get_child(surface,i))==&lv_buttonmatrix_class)matrix=lv_obj_get_child(surface,i);
            Require(matrix,"Button matrix is missing");auto* properties=static_cast<ObjectUserData*>(lv_obj_get_user_data(matrix))->toolTray->GetPropertyWindow();
            for(int i=0;i<2;++i){BtnmEditorWindow::CreateBtnmEditorWindow(properties);close(windowFor(lv_screen_active(),"Button Map Editor"));for(int frame=0;frame<10;++frame)simulator.ProcessEvents();Require(!FindText(lv_screen_active(),"Button Map Editor"),"Button map editor X did not close");}
            FileBrowser browser(std::filesystem::current_path().string());close(windowFor(lv_screen_active(),"File Browser"));for(int frame=0;frame<10;++frame)simulator.ProcessEvents();Require(!FindText(lv_screen_active(),"File Browser"),"File browser X did not close");
            std::cout<<"Upper-right X controls, panel restoration, dialog cleanup, settings and editor close/reopen passed\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--multi-select") {
            Click("Bar");Click("Bar");Click("Bar");
            auto* title=FindText(lv_screen_active(),"Simulator [Size:320x240]");
            auto* window=lv_obj_get_parent(lv_obj_get_parent(title));auto* surface=lv_obj_get_child(EditorWindow::Content(window),0);
            auto* a=lv_obj_get_child(surface,0);auto* b=lv_obj_get_child(surface,1);auto* c=lv_obj_get_child(surface,2);
            static_cast<ObjectUserData*>(lv_obj_get_user_data(a))->objectName="A";
            static_cast<ObjectUserData*>(lv_obj_get_user_data(b))->objectName="B";
            static_cast<ObjectUserData*>(lv_obj_get_user_data(c))->objectName="C";
            lv_obj_set_pos(a,20,20);lv_obj_set_pos(b,110,60);lv_obj_set_pos(c,240,180);lv_obj_set_size(b,80,30);lv_obj_update_layout(surface);
            auto find=[&](const char* name)->lv_obj_t*{for(uint32_t i=0;i<lv_obj_get_child_count(surface);++i){auto* object=lv_obj_get_child(surface,i);auto* data=static_cast<ObjectUserData*>(lv_obj_get_user_data(object));if(data && data->objectName==name)return object;}throw std::runtime_error("Expected widget missing");};
            auto refresh=[&](){a=find("A");b=find("B");c=find("C");};
            const auto id=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
            auto drain=[&](){for(int i=0;i<10;++i)simulator.ProcessEvents();lv_obj_update_layout(lv_screen_active());};
            auto mouse=[&](Uint32 type,float x,float y){SDL_Event event{};event.type=type;if(type==SDL_EVENT_MOUSE_MOTION){event.motion.windowID=id;event.motion.x=x;event.motion.y=y;}else{event.button.windowID=id;event.button.button=SDL_BUTTON_LEFT;event.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.x=x;event.button.y=y;}Require(SDL_PushEvent(&event),"Cannot send selection mouse event");drain();};
            auto bounds=[&](lv_obj_t* object){lv_area_t area;lv_obj_get_coords(object,&area);lv_obj_get_transformed_area(object,&area,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);return area;};
            auto click=[&](lv_obj_t* object,bool shift){auto area=bounds(object);float x=(area.x1+area.x2)/2.0f,y=(area.y1+area.y2)/2.0f;SDL_SetModState(shift?SDL_KMOD_SHIFT:SDL_KMOD_NONE);mouse(SDL_EVENT_MOUSE_MOTION,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_UP,x,y);SDL_SetModState(SDL_KMOD_NONE);};
            const auto beforeSelection=editor.ProjectDocument();
            click(a,false);Require(editor.SelectionCount()==1,"Plain click did not select one widget");
            click(b,true);click(c,true);Require(editor.SelectionCount()==3,"Shift-click did not add widgets");
            click(c,true);Require(editor.SelectionCount()==2,"Shift-click did not toggle a widget off");
            click(a,true);click(a,true); // Preserve a primary widget selected after a later sibling.
            Require(editor.ProjectDocument()==beforeSelection,"Selection changed the project document");
            editor.MenuAction("zoom-in");drain();
            auto area=bounds(a);float x=(area.x1+area.x2)/2.0f,y=(area.y1+area.y2)/2.0f;
            const float scale=lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)/256.0f;
            mouse(SDL_EVENT_MOUSE_MOTION,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);
            mouse(SDL_EVENT_MOUSE_MOTION,x+30*scale,y+20*scale);mouse(SDL_EVENT_MOUSE_BUTTON_UP,x+30*scale,y+20*scale);
            Require(editor.SelectionCount()==2 && lv_obj_get_x(a)==50 && lv_obj_get_y(a)==40 && lv_obj_get_x(b)==140 && lv_obj_get_y(b)==80,"Zoomed group movement lost relative positions");
            Require(lv_obj_get_x(c)==240 && lv_obj_get_y(c)==180,"Group drag moved an unselected widget");
            Require(editor.Undo(),"Cannot undo group drag");refresh();
            Require(editor.SelectionCount()==2 && lv_obj_get_x(a)==20 && lv_obj_get_x(b)==110,"Undo did not restore group geometry and selection");
            Require(lv_group_get_focused(lv_indev_get_group(simulator.GetKeyboard()))==a,"Undo did not restore the primary widget");
            Require(editor.Redo(),"Cannot redo group drag");refresh();Require(lv_obj_get_x(a)==50 && lv_obj_get_x(b)==140,"Redo did not restore group movement");
            auto* snap=FindCheckbox(lv_screen_active(),"Snap");auto* guides=FindCheckbox(lv_screen_active(),"Guides");
            lv_obj_set_state(snap,LV_STATE_CHECKED,true);lv_obj_send_event(snap,LV_EVENT_VALUE_CHANGED,nullptr);
            lv_obj_set_state(guides,LV_STATE_CHECKED,false);lv_obj_send_event(guides,LV_EVENT_VALUE_CHANGED,nullptr);
            area=bounds(a);x=(area.x1+area.x2)/2.0f;y=(area.y1+area.y2)/2.0f;
            const int spacing=EditorGrid::active->spacing;
            mouse(SDL_EVENT_MOUSE_MOTION,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);
            mouse(SDL_EVENT_MOUSE_MOTION,x+10*scale,y+10*scale);mouse(SDL_EVENT_MOUSE_BUTTON_UP,x+10*scale,y+10*scale);
            Require(lv_obj_get_x(a)%spacing==0 && lv_obj_get_y(a)%spacing==0 && lv_obj_get_x(b)-lv_obj_get_x(a)==90 && lv_obj_get_y(b)-lv_obj_get_y(a)==40,"Group snapping lost relative offsets");
            Require(editor.Undo() && editor.SelectionCount()==2,"Snapped group undo failed");refresh();
            lv_obj_set_state(snap,LV_STATE_CHECKED,false);lv_obj_send_event(snap,LV_EVENT_VALUE_CHANGED,nullptr);
            lv_obj_set_state(guides,LV_STATE_CHECKED,true);lv_obj_send_event(guides,LV_EVENT_VALUE_CHANGED,nullptr);
            for(const auto* action:{"align-left","align-right","align-hcenter","align-top","align-bottom","align-vcenter"}) {
                editor.MenuAction(action);drain();
                lv_area_t aa,bb;lv_obj_get_coords(a,&aa);lv_obj_get_coords(b,&bb);
                std::string operation=action;
                Require(operation=="align-left"?aa.x1==bb.x1:operation=="align-right"?aa.x2==bb.x2:operation=="align-hcenter"?std::abs(aa.x1+aa.x2-bb.x1-bb.x2)<=1:operation=="align-top"?aa.y1==bb.y1:operation=="align-bottom"?aa.y2==bb.y2:std::abs(aa.y1+aa.y2-bb.y1-bb.y2)<=1,"Group alignment failed");
                Require(editor.Undo() && editor.SelectionCount()==2,"Alignment undo lost selection");refresh();
                Require(editor.Redo() && editor.SelectionCount()==2,"Alignment redo lost selection");refresh();
            }
            Require(editor.HandleShortcut('d',true),"Duplicate shortcut was not handled");drain();
            Require(editor.ProjectDocument()["obj"].size()==5 && editor.SelectionCount()==2,"Group duplicate count or selection is wrong");
            Require(find("A copy") && find("B copy"),"Duplicated names are missing");
            Require(lv_group_get_focused(lv_indev_get_group(simulator.GetKeyboard()))==find("A copy"),"Duplicate did not preserve the primary selection");
            Require(editor.Undo() && editor.ProjectDocument()["obj"].size()==3 && editor.SelectionCount()==2,"Duplicate undo failed");
            Require(editor.Redo() && editor.ProjectDocument()["obj"].size()==5 && editor.SelectionCount()==2,"Duplicate redo failed");
            Require(editor.HandleShortcut(SDLK_DELETE,false),"Delete shortcut was not handled");drain();
            Require(editor.ProjectDocument()["obj"].size()==3 && editor.SelectionCount()==0,"Group delete did not remove exactly the selection");
            Require(editor.Undo() && editor.ProjectDocument()["obj"].size()==5 && editor.SelectionCount()==2,"Group delete undo failed");
            Require(editor.Redo() && editor.ProjectDocument()["obj"].size()==3,"Group delete redo failed");refresh();
            lv_obj_set_pos(a,20,20);lv_obj_set_pos(b,120,60);lv_obj_set_pos(c,240,180);editor.MenuAction("zoom-reset");drain();
            auto root=bounds(surface);
            auto marquee=[&](int x1,int y1,int x2,int y2,bool shift){SDL_SetModState(shift?SDL_KMOD_SHIFT:SDL_KMOD_NONE);mouse(SDL_EVENT_MOUSE_MOTION,(float)x1,(float)y1);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,(float)x1,(float)y1);mouse(SDL_EVENT_MOUSE_MOTION,(float)x2,(float)y2);mouse(SDL_EVENT_MOUSE_BUTTON_UP,(float)x2,(float)y2);SDL_SetModState(SDL_KMOD_NONE);};
            marquee(root.x1+10,root.y1+10,root.x1+210,root.y1+120,false);Require(editor.SelectionCount()==2,"Marquee did not select the enclosed widgets");
            marquee(root.x1+230,root.y1+170,root.x1+310,root.y1+225,true);Require(editor.SelectionCount()==3,"Shift-marquee did not add to selection");
            marquee(root.x1+210,root.y1+120,root.x1+10,root.y1+10,false);Require(editor.SelectionCount()==2,"Reverse marquee failed");
            editor.MenuAction("new");Click("Container");
            auto* hierarchyTitle=FindText(lv_screen_active(),"Hierarchy");auto* hierarchy=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(hierarchyTitle)));
            auto treeClick=[&](const char* name,bool shift){auto* label=FindText(hierarchy,name);Require(label,"Hierarchy widget is missing");SDL_SetModState(shift?SDL_KMOD_SHIFT:SDL_KMOD_NONE);lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);SDL_SetModState(SDL_KMOD_NONE);drain();};
            treeClick("Container",false);Click("Bar");treeClick("Container",false);treeClick("Bar",true);
            Require(editor.SelectionCount()==2,"Hierarchy Shift-click did not select parent and child");
            editor.MenuAction("duplicate");Require(editor.ProjectDocument()["obj"].size()==4,"Selected child was duplicated twice with its parent");
            Require(editor.Undo() && editor.ProjectDocument()["obj"].size()==2 && editor.SelectionCount()==2,"Nested duplicate undo failed");
            editor.MenuAction("delete");Require(editor.ProjectDocument()["obj"].empty(),"Parent-child selection delete left widgets");
            Require(editor.Undo() && editor.ProjectDocument()["obj"].size()==2 && editor.SelectionCount()==2,"Nested group delete undo failed");
            Require(editor.Redo() && editor.ProjectDocument()["obj"].empty(),"Nested group delete redo failed");
            std::cout<<"Shift-click, marquee, zoomed group movement, all alignments, duplicate/delete, nested selections and undo/redo passed\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--grid-snapping") {
            auto* state=EditorGrid::active;Require(state && state->visible && !state->snapping,"Grid defaults are incorrect");
            Click("Bar");Click("Bar");
            auto* title=FindText(lv_screen_active(),"Simulator [Size:320x240]");
            auto* window=lv_obj_get_parent(lv_obj_get_parent(title));
            auto* surface=lv_obj_get_child(EditorWindow::Content(window),0);
            auto* first=lv_obj_get_child(surface,0);auto* second=lv_obj_get_child(surface,1);
            lv_obj_set_pos(first,96,96);lv_obj_set_pos(second,17,19);lv_obj_update_layout(surface);
            const auto original=editor.ProjectDocument();
            auto toggle=[&](const char* text,bool value){auto* control=FindCheckbox(lv_screen_active(),text);Require(control,"Grid checkbox is missing");lv_obj_set_state(control,LV_STATE_CHECKED,value);lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);};
            toggle("Snap",true);toggle("Guides",false);toggle("Grid",false);
            auto* label=FindText(lv_screen_active(),"Spacing");Require(label,"Grid spacing control is missing");
            auto* spacing=lv_obj_get_child(lv_obj_get_parent(label),lv_obj_get_index(label)+1);
            Require(lv_obj_get_class(spacing)==&lv_spinbox_class,"Spacing control is not editable");
            lv_spinbox_set_value(spacing,16);lv_obj_send_event(spacing,LV_EVENT_VALUE_CHANGED,nullptr);
            Require(state->snapping && !state->guides && !state->visible && state->spacing==16,"Grid settings did not apply");
            Require(editor.ProjectDocument()==original,"Grid settings leaked into the saved project");
            Require(EditorGrid::Round(-7,16)==0 && EditorGrid::Round(-10,16)==-16,"Negative grid rounding failed");
            const auto id=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
            auto drain=[&](){for(int i=0;i<10;++i)simulator.ProcessEvents();lv_obj_update_layout(lv_screen_active());};
            auto mouse=[&](Uint32 type,float x,float y){SDL_Event e{};e.type=type;if(type==SDL_EVENT_MOUSE_MOTION){e.motion.windowID=id;e.motion.x=x;e.motion.y=y;}else{e.button.windowID=id;e.button.button=SDL_BUTTON_LEFT;e.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.x=x;e.button.y=y;}Require(SDL_PushEvent(&e),"Cannot send widget drag");drain();};
            auto drag=[&](lv_obj_t* object,int dx,int dy,bool release){
                lv_area_t wb,ob;lv_obj_get_coords(window,&wb);lv_obj_get_coords(object,&ob);
                const float scale=lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)/256.0f;
                float x=wb.x1+((ob.x1+ob.x2)/2.0f-wb.x1)*scale;
                float y=wb.y1+((ob.y1+ob.y2)/2.0f-wb.y1)*scale;
                mouse(SDL_EVENT_MOUSE_MOTION,x,y);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);
                mouse(SDL_EVENT_MOUSE_MOTION,x+dx*scale,y+dy*scale);
                if(release)mouse(SDL_EVENT_MOUSE_BUTTON_UP,x+dx*scale,y+dy*scale);
            };
            editor.MenuAction("zoom-in");drain();
            drag(second,18,14,true);
            Require(lv_obj_get_x(second)==32 && lv_obj_get_y(second)==32,"Zoomed widget did not snap to 16-pixel grid");
            Require(!state->moving && !state->vertical && !state->horizontal,"Guides remained after release");
            Require(editor.Undo(),"Snapped move cannot be undone");second=lv_obj_get_child(surface,1);
            Require(lv_obj_get_x(second)==17 && lv_obj_get_y(second)==19,"Undo did not restore unsnapped geometry");
            Require(editor.Redo(),"Snapped move cannot be redone");second=lv_obj_get_child(surface,1);
            Require(lv_obj_get_x(second)==32 && lv_obj_get_y(second)==32,"Redo did not restore snapped geometry");
            Require(editor.Undo(),"Cannot prepare guide test");second=lv_obj_get_child(surface,1);
            toggle("Guides",true);toggle("Grid",true);
            drag(second,76,75,false);
            Require(lv_obj_get_x(second)==96 && lv_obj_get_y(second)==96,"Sibling alignment did not snap edges");
            Require(state->moving==second && state->vertical && state->horizontal && state->guideX && state->guideY,"Alignment guides are missing during movement");
            SDL_Event release{};release.type=SDL_EVENT_MOUSE_BUTTON_UP;release.button.windowID=id;release.button.button=SDL_BUTTON_LEFT;release.button.down=false;SDL_PushEvent(&release);drain();
            Require(!state->moving && !state->vertical && !state->horizontal,"Alignment guides did not clear");
            toggle("Snap",false);toggle("Guides",false);
            int x=23,y=29;EditorGrid::Move(second,x,y,true,true);Require(x==23 && y==29,"Disabling snap did not restore free placement");EditorGrid::Clear();
            toggle("Snap",true);x=23;y=29;EditorGrid::Move(second,x,y,false,true);Require(x==23 && y==32,"Snapping changed a locked drag axis");EditorGrid::Clear();
            auto* nested=lv_obj_create(surface);lv_obj_set_pos(nested,13,11);lv_obj_set_size(nested,100,100);
            lv_obj_set_style_pad_all(nested,0,0);lv_obj_set_style_border_width(nested,0,0);
            lv_obj_set_style_pad_left(nested,5,0);lv_obj_set_style_pad_top(nested,7,0);
            auto* child=lv_obj_create(nested);lv_obj_set_size(child,10,10);lv_obj_update_layout(surface);
            lv_area_t rootBounds,parentBounds;lv_obj_get_content_coords(surface,&rootBounds);lv_obj_get_content_coords(nested,&parentBounds);
            x=3;y=3;EditorGrid::Move(child,x,y,true,true);
            Require((parentBounds.x1-rootBounds.x1+x)%16==0 && (parentBounds.y1-rootBounds.y1+y)%16==0,"Nested widgets did not use the screen grid origin");
            EditorGrid::Clear();lv_obj_delete(nested);
            editor.MenuAction("menu:Assets");Require(FindText(lv_layer_top(),"Project fonts...") && FindText(lv_layer_top(),"Project images..."),"Assets menu is missing resource managers");editor.MenuAction("dismiss");
            std::cout<<"Grid controls, zoomed snapping, alignment guides, locked axes, undo/redo and Assets menu passed\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--canvas-navigation") {
            Click("Bar");
            auto* title=FindText(lv_screen_active(),"Simulator [Size:320x240]");
            auto* window=lv_obj_get_parent(lv_obj_get_parent(title));
            auto* viewport=lv_obj_get_parent(window);
            auto* surface=lv_obj_get_child(EditorWindow::Content(window),0);
            auto* bar=lv_obj_get_child(surface,0);
            const auto document=editor.ProjectDocument();
            const auto id=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
            auto drain=[&](){for(int i=0;i<12;++i)simulator.ProcessEvents();lv_obj_update_layout(lv_screen_active());};
            auto wheel=[&](float x,float y,float delta){SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.windowID=id;e.wheel.mouse_x=x;e.wheel.mouse_y=y;e.wheel.y=delta;Require(SDL_PushEvent(&e),"Cannot send zoom wheel");drain();};
            lv_area_t bounds;lv_obj_get_coords(viewport,&bounds);
            const int x=(bounds.x1+bounds.x2)/2,y=(bounds.y1+bounds.y2)/2;
            const double localX=x-bounds.x1-lv_obj_get_x(window),localY=y-bounds.y1-lv_obj_get_y(window);
            wheel((float)x,(float)y,1);
            int scale=lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN);
            Require(scale>256,"Mouse wheel did not zoom the simulator");
            Require(std::abs(x-bounds.x1-lv_obj_get_x(window)-localX*scale/256.0)<=1 && std::abs(y-bounds.y1-lv_obj_get_y(window)-localY*scale/256.0)<=1,"Zoom did not preserve the pointer anchor");
            wheel(30,400,1);Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)==scale,"Wheel outside the canvas changed zoom");
            editor.MenuAction("menu:File");wheel((float)x,(float)y,1);Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)==scale,"Canvas zoomed through an open menu");editor.MenuAction("dismiss");
            auto mouse=[&](Uint32 type,int button,float px,float py){SDL_Event e{};e.type=type;if(type==SDL_EVENT_MOUSE_MOTION){e.motion.windowID=id;e.motion.x=px;e.motion.y=py;}else{e.button.windowID=id;e.button.button=(Uint8)button;e.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.x=px;e.button.y=py;}Require(SDL_PushEvent(&e),"Cannot send canvas mouse input");drain();};
            int oldX=lv_obj_get_x(window),oldY=lv_obj_get_y(window);
            mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_MIDDLE,(float)x,(float)y);
            mouse(SDL_EVENT_MOUSE_MOTION,0,(float)x+80,(float)y-40);
            mouse(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_MIDDLE,10,10);
            Require(lv_obj_get_x(window)==oldX+80 && lv_obj_get_y(window)==oldY-40,"Middle-button pan did not move the view");
            mouse(SDL_EVENT_MOUSE_MOTION,0,(float)x,(float)y);Require(lv_obj_get_x(window)==oldX+80,"Pan did not stop on release outside the canvas");
            oldX=lv_obj_get_x(window);oldY=lv_obj_get_y(window);
            mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,bounds.x1+3.0f,bounds.y1+3.0f);
            mouse(SDL_EVENT_MOUSE_MOTION,0,bounds.x1+23.0f,bounds.y1+13.0f);
            mouse(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,bounds.x1+23.0f,bounds.y1+13.0f);
            Require(lv_obj_get_x(window)==oldX && lv_obj_get_y(window)==oldY,"Marquee selection unexpectedly panned the view");
            for(int i=0;i<30;++i)editor.MenuAction("zoom-in");Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)==1024,"Maximum zoom limit failed");
            for(int i=0;i<60;++i)editor.MenuAction("zoom-out");Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)>=26,"Minimum zoom limit failed");
            for(int i=0;i<20;++i)wheel((float)x,(float)y,0.1f);
            Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)>26,"Fractional wheel zoom did not accumulate at minimum scale");
            Click("Fit");drain();scale=lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN);
            Require(lv_obj_get_x(window)>=15 && lv_obj_get_y(window)>=15 && lv_obj_get_x(window)+lv_obj_get_width(window)*scale/256.0<=lv_obj_get_width(viewport)-15 && lv_obj_get_y(window)+lv_obj_get_height(window)*scale/256.0<=lv_obj_get_height(viewport)-15,"Fit did not contain the simulator");
            editor.MenuAction("zoom-reset");drain();Require(lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN)==256,"100 percent reset failed");
            Require(editor.ProjectDocument()==document,"Canvas navigation changed serialized project geometry");
            editor.MenuAction("zoom-in");drain();scale=lv_obj_get_style_transform_scale_x(window,LV_PART_MAIN);
            lv_area_t wb,bb;lv_obj_get_coords(window,&wb);lv_obj_get_coords(bar,&bb);
            float bx=wb.x1+((bb.x1+bb.x2)/2.0f-wb.x1)*scale/256.0f;
            float by=wb.y1+((bb.y1+bb.y2)/2.0f-wb.y1)*scale/256.0f;
            const int originalX=lv_obj_get_x(bar);
            mouse(SDL_EVENT_MOUSE_MOTION,0,bx,by);mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,bx,by);
            mouse(SDL_EVENT_MOUSE_MOTION,0,bx+24,by);mouse(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,bx+24,by);
            Require(std::abs(lv_obj_get_x(bar)-originalX-(int)std::lround(24*256.0/scale))<=1,"Widget dragging did not compensate for canvas zoom");
            Require(editor.Undo(),"Zoomed widget drag did not create an undo step");
            const auto restored=editor.ProjectDocument();
            Require(restored.at("obj").size()==1 && restored["obj"][0]["base"]["coords"]==document["obj"][0]["base"]["coords"] && restored["obj"][0]["bar"]==document["obj"][0]["bar"],"Undo of zoomed drag did not restore widget geometry and values");
            Require(editor.Undo() && lv_obj_get_child_count(surface)==0,"Navigation added unwanted undo steps");
            std::cout<<"Canvas wheel anchoring, pan, fit, zoom limits, modal isolation and zoomed widget drag passed\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--workspace-layout") {
            Click("Arc");
            const auto document = editor.ProjectDocument();
            auto windowFor = [&](const char* name) {
                auto* label = FindText(lv_screen_active(), name);
                Require(label != nullptr, "Workspace dock is missing");
                return lv_obj_get_parent(lv_obj_get_parent(label));
            };
            auto* nativeWindow = lv_sdl_window_get_window(lv_display_get_default());
            for(const auto& size : {std::make_pair(1280,800),std::make_pair(1920,1080),std::make_pair(1440,960)}) {
                Require(SDL_SetWindowSize(nativeWindow,size.first,size.second), "Cannot resize workspace");
                for(int i=0;i<4;++i){SDL_Delay(80);simulator.ProcessEvents();}
                lv_obj_update_layout(lv_screen_active());
                lv_area_t screens, palette, hierarchy, inspector, tools, canvas;
                lv_obj_get_coords(windowFor("Screens"), &screens);
                lv_obj_get_coords(windowFor("Tool Tray"), &palette);
                lv_obj_get_coords(windowFor("Hierarchy"), &hierarchy);
                lv_obj_get_coords(windowFor("Properties"), &inspector);
                lv_obj_get_coords(windowFor("Tools"), &tools);
                lv_obj_get_coords(windowFor("Simulator [Size:320x240]"), &canvas);
                Require(screens.y2 < palette.y1 && palette.x2 < hierarchy.x1, "Left docks overlap");
                Require(hierarchy.x2 < tools.x1 && tools.x2 < inspector.x1, "Center docks overlap sidebars");
                Require(canvas.x1 > hierarchy.x2 && canvas.x2 < inspector.x1 && canvas.y2 < tools.y1, "Simulator overlaps workspace docks");
                Require(palette.y2 < size.second && inspector.x2 < size.first, "Workspace exceeds the window bounds");
            }
            Require(editor.ProjectDocument() == document, "Workspace resizing changed the project");
            auto* tree = windowFor("Hierarchy");
            lv_obj_t* search = nullptr;
            std::function<void(lv_obj_t*)> findSearch = [&](lv_obj_t* object) {
                if(lv_obj_get_class(object)==&lv_textarea_class)search=object;
                for(uint32_t i=0;i<lv_obj_get_child_count(object);++i)findSearch(lv_obj_get_child(object,i));
            };
            findSearch(tree);Require(search != nullptr,"Hierarchy search is missing");
            lv_textarea_set_text(search,"no-such-widget");lv_obj_send_event(search,LV_EVENT_VALUE_CHANGED,nullptr);
            auto* node=FindText(tree,"Screen");Require(node && lv_obj_is_hidden(lv_obj_get_parent(node)),"Hierarchy search did not filter rows");
            lv_textarea_set_text(search,"");lv_obj_send_event(search,LV_EVENT_VALUE_CHANGED,nullptr);
            Require(!lv_obj_is_hidden(node) && !lv_obj_is_hidden(lv_obj_get_parent(node)),"Clearing hierarchy search did not restore rows");
            lv_obj_move_foreground(windowFor("Simulator [Size:320x240]"));
            std::cout<<"Docked workspace resizes without overlap and hierarchy search filters objects\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--project-creator") {
            editor.MenuAction("project-creator");lv_obj_update_layout(lv_screen_active());
            auto* title=FindText(lv_layer_top(),"Create LVGL firmware project");Require(title,"Project creator window is missing");
            auto* searchLabel=FindText(lv_layer_top(),"Search templates");auto* search=lv_obj_get_child(lv_obj_get_parent(searchLabel),1);
            auto* listLabel=FindText(lv_layer_top(),"Project template");auto* list=lv_obj_get_child(lv_obj_get_parent(listLabel),1);
            Require(lv_dropdown_get_option_count(list)>=67,"Bundled catalog is incomplete");
            lv_textarea_set_text(search,"Eclipse");Require(lv_dropdown_get_option_count(list)==1,"Template search did not filter");
            Require(FindText(lv_layer_top(),"Template branch"),"Branch selector is missing");
            Require(FindText(lv_layer_top(),"LVGL source version"),"Version selector is missing");
            Require(FindText(lv_layer_top(),"Create project"),"Create action is missing");
            Require(FindText(lv_layer_top(),"Open in VS Code"),"VS Code action is missing");
            lv_textarea_set_text(search,"not a board name 12345");Require(std::string(lv_dropdown_get_options(list))=="No matches","Empty search result was not handled");
            lv_textarea_set_text(search,"");Require(lv_dropdown_get_option_count(list)>=67,"Clearing search did not restore catalog");
            auto field=[](const char* caption){auto* label=FindText(lv_layer_top(),caption);Require(label,"Project creator filter field is missing");return lv_obj_get_child(lv_obj_get_parent(label),1);};
            auto select=[](lv_obj_t* dropdown,const std::string& expected) {
                const std::string options=lv_dropdown_get_options(dropdown);size_t begin=0;unsigned index=0;
                for(;;){const auto end=options.find('\n',begin);const auto option=options.substr(begin,end==std::string::npos?end:end-begin);
                    if(option==expected){lv_dropdown_set_selected(dropdown,index);lv_obj_send_event(dropdown,LV_EVENT_VALUE_CHANGED,nullptr);return;}
                    if(end==std::string::npos)throw std::runtime_error("Expected dropdown option is missing: "+expected);begin=end+1;++index;
                }
            };
            auto* vendor=field("Chip / board vendor");auto* environment=field("Environment");auto* host=field("Host operating system");auto* version=field("LVGL source version");
            select(vendor,"Raspberry Pi");const std::string piOptions=lv_dropdown_get_options(list);
            Require(piOptions.find("NuttX Waveshare RP2040-LCD-1.28")!=std::string::npos,"Raspberry Pi NuttX template was removed");
            const auto catalog=ProjectCreation::BundledCatalog();std::string piName;
            for(const auto& item:catalog)if(item.value("uicreatorTemplate",std::string{})=="raspberry-pi-linux")piName=item.at("name");
            Require(!piName.empty() && piOptions.find(piName)!=std::string::npos,"Raspberry Pi filter has no Linux build");
            select(list,piName);Require(lv_dropdown_get_option_count(version)==2,"Built-in Linux template offered incompatible LVGL versions");
            const std::string piVersions=lv_dropdown_get_options(version);Require(piVersions.find("9.6.0")!=std::string::npos && piVersions.find("9.5") == std::string::npos,"Built-in Linux version selector is incorrect");
            for(const char* system:{"Windows","Linux","MacOS"}) {
                select(host,system);select(environment,"Linux");Require(std::string(lv_dropdown_get_options(list)).find(piName)!=std::string::npos,"Linux project cannot be exported from selected host");
                select(environment,"CMake");Require(std::string(lv_dropdown_get_options(list)).find(piName)!=std::string::npos,"Linux project disappeared from CMake filter");
                select(list,piName);
            }
            select(host,"All hosts");select(environment,"Pico SDK");
            for(const char* name:{"Raspberry Pi Pico 1","Raspberry Pi Pico 2"}) {
                Require(std::string(lv_dropdown_get_options(list)).find(name)!=std::string::npos,"Pico SDK template is missing from the Raspberry Pi filter");
                select(list,name);Require(lv_dropdown_get_option_count(version)==2,"Pico template offered incompatible LVGL versions");
                Require(lv_obj_has_state(field("Template branch"),LV_STATE_DISABLED),"Pico template exposed an arbitrary remote branch");
                Require(lv_obj_has_state(field("LVGL folder (auto)"),LV_STATE_DISABLED),"Pico template exposed an incompatible LVGL override");
            }
            select(environment,"All environments");select(list,"NuttX Waveshare RP2040-LCD-1.28");
            Require(lv_dropdown_get_option_count(version)>=7,"Selecting an official template did not restore its LVGL version choices");
            select(vendor,"All vendors");Require(lv_dropdown_get_option_count(list)>=70,"Clearing vendor filter did not restore the new full catalog");
            std::cout<<"Project creator catalog, Raspberry Pi Linux/CMake and host filtering, compatible versions, official template restoration and launch controls are available\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--undo-redo") {
            Require(!editor.CanUndo() && !editor.CanRedo(),"Initial history is not empty");
            ClickPaletteWidget(simulator,"Arc");Require(editor.ProjectDocument()["obj"].size()==1,"Arc was not created");
            Require(editor.CanUndo(),"Widget creation was not recorded");
            Require(editor.Undo(),"Undo creation failed");Require(editor.ProjectDocument()["obj"].empty(),"Undo left a widget behind");
            Require(editor.CanRedo(),"Redo unavailable after undo");Require(editor.Redo(),"Redo creation failed");
            Require(editor.ProjectDocument()["obj"].size()==1,"Redo did not restore widget");
            ClickPaletteWidget(simulator,"Bar");Require(editor.ProjectDocument()["obj"].size()==2,"Nested widget creation failed");
            auto* value=FindText(lv_screen_active(),"Value");Require(value,"Bar value editor is missing");
            lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(value),1),"73");
            auto project=editor.ProjectDocument();Require(project["obj"][1]["bar"]["value"]==73,"Value edit failed");
            Require(editor.HandleShortcut('z',true),"Ctrl+Z was not handled");Require(editor.ProjectDocument()["obj"][1]["bar"]["value"]!=73,"Undo did not revert property");
            Require(editor.HandleShortcut('z',true,true),"Ctrl+Shift+Z was not handled");Require(editor.ProjectDocument()["obj"][1]["bar"]["value"]==73,"Redo did not restore property");
            editor.MenuAction("delete");Require(editor.ProjectDocument()["obj"].size()==1,"Delete failed");
            Require(editor.Undo(),"Undo deletion failed");Require(editor.ProjectDocument()["obj"].size()==2,"Undo deletion lost child");
            editor.HandleShortcut('y',true);Require(editor.ProjectDocument()["obj"].size()==1,"Ctrl+Y did not redo deletion");
            Require(editor.Undo(),"Undo deletion failed a second time");
            ClickPaletteWidget(simulator,"Check box");Require(!editor.CanRedo(),"New edits retained the old redo branch");
            editor.MenuAction("new");Require(!editor.CanUndo() && !editor.CanRedo(),"New project did not reset history");
            std::cout<<"Editor undo/redo covers creation, nesting, property edits, deletion, branching and shortcuts\n";return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--keyboard-mode-repeat")
        {
            Click("Advanced\nWidgets");
            ClickPaletteWidget(simulator, "Keyboard");
            auto* title = FindText(lv_screen_active(), "Simulator [Size:320x240]");
            auto* surface = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))), 0);
            auto* keyboard = lv_obj_get_child(surface, -1);
            auto* label = FindText(lv_screen_active(), "Mode");
            Require(label != nullptr, "Keyboard mode editor is missing");
            auto* dropdown = lv_obj_get_child(lv_obj_get_parent(label), 1);
            Require(lv_obj_get_class(dropdown) == &lv_dropdown_class, "Mode control is not a dropdown");
            struct ModeEventTiming {
                lv_obj_t* object;
                uint64_t started = 0, elapsed = 0;
                unsigned depth = 0, completed = 0;
                static bool IsOwnEvent(lv_event_t* event) {
                    return lv_event_get_target_obj(event) == lv_event_get_current_target_obj(event);
                }
                static void Begin(lv_event_t* event) {
                    if(!IsOwnEvent(event)) return;
                    auto* timing = static_cast<ModeEventTiming*>(lv_event_get_user_data(event));
                    if(timing->depth++ == 0) timing->started = SDL_GetTicksNS();
                }
                static void End(lv_event_t* event) {
                    if(!IsOwnEvent(event)) return;
                    auto* timing = static_cast<ModeEventTiming*>(lv_event_get_user_data(event));
                    Require(timing->depth != 0, "Mode event ended without its preprocess observer");
                    if(--timing->depth == 0) {
                        timing->elapsed += SDL_GetTicksNS() - timing->started;
                        ++timing->completed;
                    }
                }
                explicit ModeEventTiming(lv_obj_t* dropdown) : object(dropdown) {
                    // Preprocessing precedes the class/default handlers; the
                    // normal observer is appended after the editor's handlers.
                    lv_obj_add_event_cb(object, Begin, static_cast<lv_event_code_t>(LV_EVENT_VALUE_CHANGED | LV_EVENT_PREPROCESS), this);
                    lv_obj_add_event_cb(object, End, LV_EVENT_VALUE_CHANGED, this);
                }
                ~ModeEventTiming() {
                    if(lv_obj_is_valid(object)) {
                        lv_obj_remove_event_cb_with_user_data(object, Begin, this);
                        lv_obj_remove_event_cb_with_user_data(object, End, this);
                    }
                }
                void Reset() {
                    Require(depth == 0, "Mode event did not finish synchronously");
                    elapsed = 0; completed = 0;
                }
            } modeTiming(dropdown);
            const auto clickPoint = [&](int x, int y) {
                const auto id = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
                SDL_Event event{};
                event.type = SDL_EVENT_MOUSE_MOTION; event.motion.windowID = id; event.motion.x = x; event.motion.y = y;
                SDL_PushEvent(&event);
                event = {}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.windowID = id;
                event.button.button = SDL_BUTTON_LEFT; event.button.down = true; event.button.x = x; event.button.y = y;
                SDL_PushEvent(&event);
                for (int i = 0; i < 8; ++i) simulator.ProcessEvents();
                event.type = SDL_EVENT_MOUSE_BUTTON_UP; event.button.down = false;
                SDL_PushEvent(&event);
                for (int i = 0; i < 8; ++i) simulator.ProcessEvents();
            };
            for (uint32_t mode : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 0u, 1u, 1u})
            {
                const auto previousMode = lv_keyboard_get_mode(keyboard);
                modeTiming.Reset();
                lv_obj_scroll_to_view(dropdown, LV_ANIM_OFF);
                lv_obj_update_layout(lv_screen_active());
                lv_area_t bounds; lv_obj_get_coords(dropdown, &bounds);
                const auto started = SDL_GetTicks();
                clickPoint((bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2);
                Require(lv_dropdown_is_open(dropdown), "Keyboard mode dropdown did not reopen immediately after selection");
                auto* list = lv_dropdown_get_list(dropdown);
                auto* options = lv_obj_get_child(list, 0);
                lv_obj_update_layout(list);
                lv_area_t textBounds; lv_obj_get_coords(options, &textBounds);
                const int lineHeight = lv_font_get_line_height(lv_obj_get_style_text_font(options, LV_PART_MAIN)) + lv_obj_get_style_text_line_space(options, LV_PART_MAIN);
                clickPoint((bounds.x1 + bounds.x2) / 2, textBounds.y1 + static_cast<int>(mode) * lineHeight + lineHeight / 2);
                Require(lv_keyboard_get_mode(keyboard) == mode, "Mouse selection did not update the simulator keyboard mode");
                Require(!lv_dropdown_is_open(dropdown), "Mode dropdown did not close after selection");
                Require(modeTiming.depth == 0 && (previousMode == mode || modeTiming.completed != 0), "Mouse selection did not complete its mode event");
                Require(modeTiming.elapsed < 1000000000ULL, "Mode-change event blocked the editor for a second or longer");
                std::cout << "Mode " << mode << ": event " << modeTiming.elapsed / 1000000.0
                          << " ms, pointer/render roundtrip " << SDL_GetTicks() - started << " ms" << std::endl;
            }
            std::cout << "Repeated keyboard mode selections reopen immediately\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--stress-arcs-bars")
        {
            auto* title = FindText(lv_screen_active(), "Simulator [Size:320x240]");
            auto* surface = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))), 0);
            for (int i = 0; i < 20; ++i)
            {
                for (const auto* name : {"Arc", "Bar"})
                {
                    Click("Screen");
                    const auto count = lv_obj_get_child_count(surface);
                    ClickPaletteWidget(simulator, name);
                    Require(lv_obj_get_child_count(surface) == count + 1, "Repeated Arc/Bar click failed to create a widget");
                    auto* object = lv_obj_get_child(surface, -1);
                    Require(lv_obj_get_class(object) == (std::string(name) == "Arc" ? &lv_arc_class : &lv_bar_class), "Wrong repeated widget class");
                    if (std::string(name) == "Arc")
                    {
                        auto* checkbox = FindCheckbox(lv_screen_active(), "Arc Line Rounded");
                        Require(checkbox != nullptr, "Arc checkbox is missing");
                        lv_obj_add_state(checkbox, LV_STATE_CHECKED);
                        lv_obj_send_event(checkbox, LV_EVENT_CLICKED, nullptr);
                    }
                }
            }
            Require(lv_obj_get_child_count(surface) == 40, "Repeated creation lost widgets");
            editor.MenuAction("new");
            Require(lv_obj_get_child_count(surface) == 0, "Repeated widgets did not cleanly delete");
            std::cout << "Created, edited, and removed 40 alternating Arc and Bar widgets\n";
            return 0;
        }
        if (argc == 3)
        {
            if(std::string(argv[1])=="--capture-multi-select") {
                Click("Arc");Click("Bar");Click("Button");
                auto* title=FindText(lv_screen_active(),"Simulator [Size:320x240]");auto* window=lv_obj_get_parent(lv_obj_get_parent(title));
                auto* surface=lv_obj_get_child(EditorWindow::Content(window),0);auto* arc=lv_obj_get_child(surface,0);auto* bar=lv_obj_get_child(surface,1);
                lv_obj_set_pos(arc,20,25);lv_obj_set_pos(bar,170,80);lv_obj_set_pos(lv_obj_get_child(surface,2),135,165);lv_obj_update_layout(surface);
                lv_obj_send_event(arc,LV_EVENT_CLICKED,nullptr);
                lv_area_t area;lv_obj_get_coords(bar,&area);lv_obj_get_transformed_area(bar,&area,LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
                SDL_SetModState(SDL_KMOD_SHIFT);
                for(auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
                    SDL_Event e{};e.type=type;e.button.windowID=SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));e.button.button=SDL_BUTTON_LEFT;e.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.x=(area.x1+area.x2)/2.0f;e.button.y=(area.y1+area.y2)/2.0f;SDL_PushEvent(&e);for(int i=0;i<8;++i)simulator.ProcessEvents();
                }
                SDL_SetModState(SDL_KMOD_NONE);Require(editor.SelectionCount()==2,"Multi-selection preview setup failed");
            }
            else if (std::string(argv[1]) == "--capture-variables") Click("Variables");
            else if (std::string(argv[1]) == "--capture-slider") Click("Slider");
            else if (std::string(argv[1]) == "--capture-advanced") { Click("Advanced\nWidgets"); Click("Chart"); }
            else if (std::string(argv[1]) == "--capture-dialogs") { Click("Dialogs"); Click("Message box"); }
            else if (std::string(argv[1]) == "--capture-custom-text") { Click("Custom"); Click("Text box"); }
            else if (std::string(argv[1]) == "--capture-custom-layers") { Click("Custom"); Click("Layer view"); }
            for (int frame = 0; frame < 20; ++frame) { SDL_Delay(16); simulator.ProcessEvents(); }
            auto* display = lv_display_get_default();
            auto* renderer = static_cast<SDL_Renderer*>(lv_sdl_window_get_renderer(display));
            auto* image = SDL_RenderReadPixels(renderer, nullptr);
            Require(image != nullptr, "Could not capture SDL3 renderer");
            const bool saved = SDL_SaveBMP(image, argv[2]);
            SDL_DestroySurface(image);
            Require(saved, "Could not save editor screenshot");
            return 0;
        }
        Require(FindText(lv_screen_active(), "Advanced\nWidgets") != nullptr, "Advanced Widgets tab is missing");
        auto* hierarchyTitle = FindText(lv_screen_active(), "Hierarchy");
        auto* hierarchy = hierarchyTitle ? lv_obj_get_parent(lv_obj_get_parent(hierarchyTitle)) : nullptr;
        auto* screenNode = hierarchy ? FindText(hierarchy, "Screen") : nullptr;
        Require(hierarchyTitle != nullptr && screenNode != nullptr, "Hierarchy panel is missing");
        lv_area_t hierarchyBounds, nodeBounds;
        lv_obj_get_coords(lv_obj_get_parent(lv_obj_get_parent(hierarchyTitle)), &hierarchyBounds);
        lv_obj_get_coords(screenNode, &nodeBounds);
        Require(nodeBounds.x1 >= hierarchyBounds.x1 && nodeBounds.x2 <= hierarchyBounds.x2, "Hierarchy rows are clipped outside their dock");
        auto* title = FindText(lv_screen_active(), "Simulator [Size:320x240]");
        Require(title != nullptr, "Simulator window is missing");
        auto* surface = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(title))), 0);
        for (const auto* widget : {"Arc", "Bar", "Button", "Check box", "Container", "Drop down list", "Image", "Label",
                                  "LED", "Line", "Preloader", "Roller", "Slider", "Spinbox", "Switch", "Text area"})
        {
            Click("Screen");
            const auto before = lv_obj_get_child_count(surface);
            ClickPaletteWidget(simulator, widget);
            Require(lv_obj_get_child_count(surface) == before + 1, "Widget button did not create its widget");
            auto* object = lv_obj_get_child(surface, -1);
            const std::map<std::string, const lv_obj_class_t*> nativeClasses = {
                {"Arc", &lv_arc_class}, {"Bar", &lv_bar_class}, {"Button", &lv_button_class}, {"Check box", &lv_checkbox_class},
                {"Container", &lv_obj_class}, {"Drop down list", &lv_dropdown_class}, {"Image", &lv_image_class}, {"Label", &lv_label_class},
                {"LED", &lv_led_class}, {"Line", &lv_line_class}, {"Preloader", &lv_spinner_class}, {"Roller", &lv_roller_class},
                {"Slider", &lv_slider_class}, {"Spinbox", &lv_spinbox_class}, {"Switch", &lv_switch_class}, {"Text area", &lv_textarea_class}};
            Require(lv_obj_get_class(object) == nativeClasses.at(widget), "Palette entry created the wrong native widget class");
            if (std::string(widget) == "Image")
            {
                Require(FindText(lv_screen_active(), "Load image from disk...") != nullptr, "Image file picker button is missing");
                auto* window = static_cast<ObjectUserData*>(lv_obj_get_user_data(object))->toolTray->GetPropertyWindow();
                const auto fixture = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Fixtures" / "Images" / "sample.png";
                ImageProperties::LoadFromPath(window, fixture);
                Require(Serialization::ObjectSerializer::SerializeObject(object)["img"]["asset"]["name"] == "sample.png", "Image picker import did not update its image widget");
            }
            const auto record = Serialization::ObjectSerializer::SerializeObject(object);
            auto* restored = Serialization::ObjectSerializer::DeserializeObject(record);
            Require(restored != nullptr, "Widget did not deserialize");
            Require(lv_obj_get_class(restored) == lv_obj_get_class(object), "Save/reload changed the widget class");
            lv_obj_delete(restored);
            auto* binaryCopy = Serialization::ObjectSerializer::FromProtobuf(Serialization::ObjectSerializer::ToProtobuf(object));
            Require(binaryCopy != nullptr && lv_obj_get_class(binaryCopy) == lv_obj_get_class(object), "Widget Protobuf round-trip changed its class");
            lv_obj_delete(binaryCopy);
        }

        Click("Advanced\nWidgets");
        for (const auto* widget : {"Button matrix", "Calendar", "Canvas", "Chart", "Gauge", "Image button",
                                  "Keyboard", "Line meter", "Page", "Table", "Tabview", "Tile view", "Message box", "Window"})
        {
            std::cerr << "Checking advanced palette entry " << widget << '\n';
            if (std::string(widget) == "Message box") Click("Dialogs");
            Click("Screen");
            const auto count = lv_obj_get_child_count(surface);
            ClickPaletteWidget(simulator, widget);
            Require(lv_obj_get_child_count(surface) == count + 1, "Advanced palette entry did not create a widget");
            auto* object = lv_obj_get_child(surface, -1);
            const std::map<std::string, const lv_obj_class_t*> nativeClasses = {
                {"Button matrix", &lv_buttonmatrix_class}, {"Calendar", &lv_calendar_class}, {"Canvas", &lv_canvas_class},
                {"Chart", &lv_chart_class}, {"Gauge", &lv_scale_class}, {"Image button", &lv_imagebutton_class},
                {"Keyboard", &lv_keyboard_class}, {"Line meter", &lv_scale_class}, {"Page", &lv_obj_class},
                {"Table", &lv_table_class}, {"Tabview", &lv_tabview_class}, {"Tile view", &lv_tileview_class},
                {"Message box", &lv_msgbox_class}, {"Window", &lv_obj_class}};
            Require(lv_obj_get_class(object) == nativeClasses.at(widget), "Advanced palette entry created the wrong widget class");
            const auto record = Serialization::ObjectSerializer::SerializeObject(object);
            auto* restored = Serialization::ObjectSerializer::DeserializeObject(record);
            Require(restored != nullptr && lv_obj_get_class(restored) == lv_obj_get_class(object), "Advanced widget did not round-trip");
            lv_obj_delete(restored);
            auto* binaryCopy = Serialization::ObjectSerializer::FromProtobuf(Serialization::ObjectSerializer::ToProtobuf(object));
            Require(binaryCopy != nullptr && lv_obj_get_class(binaryCopy) == lv_obj_get_class(object), "Widget Protobuf round-trip changed its class");
            lv_obj_delete(binaryCopy);
            auto* content = Serialization::ExtendedWidgets::Type(object) ? Serialization::ExtendedWidgets::ContentParent(object) : object;
            lv_obj_send_event(content, LV_EVENT_CLICKED, nullptr);
            if (std::string(widget) == "Chart")
            {
                auto* label = FindText(lv_screen_active(), "Point Count");
                Require(label != nullptr, "Chart point count editor is missing");
                auto* field = lv_obj_get_child(lv_obj_get_parent(label), 1);
                lv_textarea_set_text(field, "7");
                Require(lv_chart_get_point_count(object) == 7, "Chart property edit did not update the widget");
            }
            const auto setNumber = [&](const char* name, const char* value) {
                auto* label = FindText(lv_screen_active(), name);
                Require(label != nullptr, "Advanced numeric property editor is missing");
                lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(label), 1), value);
            };
            if (std::string(widget) == "Button matrix")
            {
                auto* checkbox = FindCheckbox(lv_screen_active(), "Recolor");
                Require(checkbox != nullptr, "Button matrix recolor editor is missing");
                lv_obj_add_state(checkbox, LV_STATE_CHECKED);
                lv_obj_send_event(checkbox, LV_EVENT_VALUE_CHANGED, nullptr);
                Require(lv_buttonmatrix_has_button_ctrl(object, 0, LV_BUTTONMATRIX_CTRL_RECOLOR), "Button matrix recolor editor did not update the control flags");
            }
            if (std::string(widget) == "Canvas")
            {
                setNumber("Width", "160");
                Require(lv_canvas_get_draw_buf(object)->header.w == 160, "Canvas width editor did not resize its pixel buffer");
            }
            if (std::string(widget) == "Gauge")
            {
                setNumber("Value", "73");
                Require(Serialization::ObjectSerializer::SerializeObject(object)["scale"]["value"] == 73, "Gauge value editor did not update its needle");
            }
            if (std::string(widget) == "Line meter")
            {
                setNumber("Ticks", "31");
                Require(lv_scale_get_total_tick_count(object) == 31, "Line meter tick editor did not update the scale");
            }
            if (std::string(widget) == "Table")
            {
                auto* rows = FindText(lv_screen_active(), "Rows");
                auto* text = FindText(lv_obj_get_parent(lv_obj_get_parent(rows)), "Text");
                Require(text != nullptr, "Table cell editor is missing");
                lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(text), 1), "Edited cell");
                Require(std::string(lv_table_get_cell_value(object, 0, 0)) == "Edited cell", "Table cell editor did not update the table");
            }
            if (std::string(widget) == "Tabview")
            {
                auto* tabs = FindText(lv_screen_active(), "Tabs");
                auto* collection = lv_obj_get_child(lv_obj_get_parent(lv_obj_get_parent(tabs)), 1);
                auto* add = lv_obj_get_child(collection, -1);
                Require(add != nullptr && lv_obj_get_class(add) == &lv_button_class, "Tab collection editor is missing");
                lv_obj_send_event(add, LV_EVENT_CLICKED, nullptr);
                for (int frame = 0; frame < 5; ++frame) simulator.ProcessEvents();
                Require(lv_tabview_get_tab_count(object) == 3, "Tab collection editor did not add a tab");
                lv_tabview_set_active(object, 1, LV_ANIM_OFF);
                auto* host = Serialization::ExtendedWidgets::ContentParent(object);
                lv_obj_send_event(host, LV_EVENT_CLICKED, nullptr);
                const auto childCount = lv_obj_get_child_count(host);
                auto* objectData = static_cast<ObjectUserData*>(lv_obj_get_user_data(object));
                Require(objectData->toolTray->GetPropertyWindow()->GetSelectedObject() == object, "Clicking tab content did not select its root widget");
                Click("Label");
                Require(lv_obj_get_child_count(host) == childCount + 1, "Palette widget did not use the selected tab content host");
            }
            if (std::string(widget) == "Window" || std::string(widget) == "Message box")
            {
                auto* label = FindText(lv_screen_active(), "Title");
                Require(label != nullptr, "Dialog title editor is missing");
                lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(label), 1), "Edited dialog");
                Require(Serialization::ObjectSerializer::SerializeObject(object).at(std::string(widget) == "Window" ? "window" : "messagebox").at("title") == "Edited dialog",
                    "Dialog title editor did not update the widget");
            }
        }
        Click("Custom");
        for (const auto* widget : {"Text box", "Layer view"})
        {
            Click("Screen");
            const auto count = lv_obj_get_child_count(surface);
            ClickPaletteWidget(simulator, widget);
            Require(lv_obj_get_child_count(surface) == count + 1, "Custom palette button did not create a widget");
            auto* object = lv_obj_get_child(surface, -1);
            const auto* type = Serialization::CustomWidgets::Type(object);
            Require(type != nullptr && std::string(type) == (std::string(widget) == "Text box" ? "textBox" : "layerView"), "Custom widget type was not retained");
            auto* title = FindText(lv_screen_active(), "Title");
            Require(title != nullptr, "Custom title editor is missing");
            lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(title), 1), "Custom title");
            Require(Serialization::CustomWidgets::ToJSON(object)[type]["title"] == "Custom title", "Custom property edit did not reach its widget");
            if (std::string(widget) == "Text box")
            {
                auto* plus = Serialization::CustomWidgets::Part(object, "Plus");
                Require(lv_obj_get_class(Serialization::CustomWidgets::Part(object, "Entry")) == &lv_textarea_class, "Text box has no real textarea");
                lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr);
                Require(Serialization::CustomWidgets::ToJSON(object)[type]["value"] == 1, "Text box increment button did not work");
            }
            else
            {
                auto* row = Serialization::CustomWidgets::RowObject(object, 2);
                Require(row != nullptr && lv_obj_get_class(row) == &lv_button_class, "Layer view has no real interactive rows");
                lv_obj_send_event(row, LV_EVENT_CLICKED, nullptr);
                Require(Serialization::CustomWidgets::ToJSON(object)[type]["selected"] == 2, "Layer view selection did not work");
            }
            const auto record = Serialization::ObjectSerializer::SerializeObject(object);
            auto* restored = Serialization::ObjectSerializer::DeserializeObject(record);
            Require(restored != nullptr && Serialization::CustomWidgets::Type(restored) != nullptr, "Custom widget did not deserialize");
            lv_obj_delete(restored);
            auto* binaryCopy = Serialization::ObjectSerializer::FromProtobuf(Serialization::ObjectSerializer::ToProtobuf(object));
            Require(binaryCopy != nullptr && lv_obj_get_class(binaryCopy) == lv_obj_get_class(object), "Widget Protobuf round-trip changed its class");
            lv_obj_delete(binaryCopy);
        }
        Click("Screen");
        auto* selected = lv_obj_get_child(surface, -1);
        bool selectedDeleted = false;
        lv_obj_add_event_cb(selected, [](lv_event_t* event) {
            *static_cast<bool*>(lv_event_get_user_data(event)) = true;
        }, LV_EVENT_DELETE, &selectedDeleted);
        std::cerr << "Created and round-tripped all basic widgets\n";
        const auto before = lv_obj_get_child_count(surface);
        lv_obj_send_event(selected, LV_EVENT_CLICKED, nullptr);
        auto* xLabel = FindText(lv_screen_active(), "X");
        Require(xLabel != nullptr, "Position property field is missing");
        auto* xField = lv_obj_get_child(lv_obj_get_parent(xLabel), 1);
        lv_obj_send_event(xField, LV_EVENT_CLICKED, nullptr);
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(lv_sdl_window_get_window(lv_display_get_default()));
        event.key.key = SDLK_DELETE;
        event.key.scancode = SDL_SCANCODE_DELETE;
        SDL_PushEvent(&event);
        for (int frame = 0; frame < 30; ++frame) simulator.ProcessEvents();
        Require(lv_obj_get_child_count(surface) == before, "Delete in a property field removed the simulator widget");
        std::cerr << "Property field Delete preserved the widget\n";
        lv_obj_send_event(selected, LV_EVENT_CLICKED, nullptr);
        SDL_PushEvent(&event);
        for (int frame = 0; frame < 30; ++frame) simulator.ProcessEvents();
        Require(lv_obj_get_child_count(surface) == before - 1, "Delete did not remove the selected simulator widget");
        Require(selectedDeleted, "Selected simulator widget did not receive its deletion event");
        std::cerr << "Simulator Delete removed its widget\n";
        editor.MenuAction("about");
        Require(FindText(lv_layer_top(), "About LVGL UI Creator") != nullptr, "Help dialog did not open");
        editor.MenuAction("cancel-file");

        lv_obj_t* fontTarget = nullptr;
        for (uint32_t i = 0; i < lv_obj_get_child_count(surface); ++i)
            if (lv_obj_get_class(lv_obj_get_child(surface, i)) == &lv_label_class) fontTarget = lv_obj_get_child(surface, i);
        Require(fontTarget != nullptr, "No label available for font application");
        lv_obj_send_event(fontTarget, LV_EVENT_CLICKED, nullptr);
        editor.MenuAction("font-converter");
        auto* fontTitle = FindText(lv_layer_top(), "Add project font");
        Require(fontTitle != nullptr, "Font converter menu did not open");
        auto* fontWindow = lv_obj_get_parent(lv_obj_get_parent(fontTitle));
        auto* fontBody = EditorWindow::Content(fontWindow);
        auto fontField = [&](const char* caption) { auto* label = FindText(fontBody, caption); Require(label != nullptr, "Font conversion field missing"); return lv_obj_get_child(lv_obj_get_parent(label), 1); };
        const auto sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
        const auto fontPath = sourceRoot / "3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf";
        lv_textarea_set_text(fontField("Font file"), fontPath.u8string().c_str());
        auto* convertLabel = FindText(fontBody, "Add and apply");
        Require(convertLabel != nullptr, "Convert action missing");
        lv_obj_send_event(lv_obj_get_parent(convertLabel), LV_EVENT_CLICKED, nullptr);
        auto* fontStatus = lv_obj_get_child(fontBody, -1);
        const auto conversionStart = SDL_GetTicks();
        while (std::string(lv_label_get_text(fontStatus)).find("Preparing font") == 0 && SDL_GetTicks() - conversionStart < 30000)
            simulator.ProcessEvents();
        std::cerr << lv_label_get_text(fontStatus) << '\n';
        Require(std::string(lv_label_get_text(fontStatus)).find("saved and applied") != std::string::npos, "Font was not saved and applied");
        const int convertedHeight = lv_obj_get_style_text_font(fontTarget, LV_PART_MAIN)->line_height;
        Require(convertedHeight > (LV_FONT_DEFAULT)->line_height, "Converted font was not applied");
        auto* closeFont = EditorWindow::CloseButton(fontWindow);
        lv_obj_send_event(closeFont, LV_EVENT_CLICKED, nullptr);
        Require(lv_obj_get_style_text_font(fontTarget, LV_PART_MAIN)->line_height == convertedHeight, "Closing the converter invalidated the applied font");
        std::cerr << "Font converter preview and application passed\n";
        editor.MenuAction("font-manager");
        auto* managerTitle=FindText(lv_layer_top(),"Project fonts");Require(managerTitle!=nullptr,"Project font manager did not open");
        auto* manager=lv_obj_get_parent(lv_obj_get_parent(managerTitle));auto* managerBody=EditorWindow::Content(manager);
        auto* managerName=lv_obj_get_child(managerBody,3);lv_textarea_set_text(managerName,"Body font");
        lv_obj_send_event(lv_obj_get_parent(FindText(managerBody,"Rename")),LV_EVENT_CLICKED,nullptr);
        lv_obj_send_event(lv_obj_get_parent(FindText(managerBody,"Remove unused font")),LV_EVENT_CLICKED,nullptr);
        auto* managerStatus=lv_obj_get_child(managerBody,-1);Require(std::string(lv_label_get_text(managerStatus)).find("in use")!=std::string::npos,"Font manager removed an in-use font");
        lv_obj_send_event(lv_obj_get_parent(FindText(managerBody,"Close")),LV_EVENT_CLICKED,nullptr);
        lv_obj_t* fontBar=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(surface);++i)if(lv_obj_get_class(lv_obj_get_child(surface,i))==&lv_bar_class)fontBar=lv_obj_get_child(surface,i);
        Require(fontBar!=nullptr,"No bar available for font selection");lv_obj_send_event(fontBar,LV_EVENT_CLICKED,nullptr);
        auto* fontLabel=FindText(lv_screen_active(),"Font");Require(fontLabel!=nullptr,"Widget font selector missing");auto* fontChoice=lv_obj_get_child(lv_obj_get_parent(fontLabel),1);
        Require(std::string(lv_dropdown_get_options(fontChoice)).find("Body font")!=std::string::npos,"Renamed font missing from widget dropdown");
        lv_dropdown_set_selected(fontChoice,1);lv_obj_send_event(fontChoice,LV_EVENT_VALUE_CHANGED,nullptr);
        Require(lv_obj_get_style_text_font(fontBar,LV_PART_MAIN)==lv_obj_get_style_text_font(fontTarget,LV_PART_MAIN),"Widgets did not reuse the same project font");
        lv_dropdown_set_selected(fontChoice,0);lv_obj_send_event(fontChoice,LV_EVENT_VALUE_CHANGED,nullptr);
        Require(lv_obj_get_style_text_font(fontBar,LV_PART_MAIN)!=(lv_obj_get_style_text_font(fontTarget,LV_PART_MAIN)),"Theme font selection did not clear project font");
        lv_dropdown_set_selected(fontChoice,1);lv_obj_send_event(fontChoice,LV_EVENT_VALUE_CHANGED,nullptr);
        std::cerr<<"Project font manager and reusable widget selection passed\n";

        lv_obj_t* convertedImage=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(surface);++i)if(lv_obj_get_class(lv_obj_get_child(surface,i))==&lv_image_class)convertedImage=lv_obj_get_child(surface,i);
        Require(convertedImage!=nullptr,"No image available for converter");lv_obj_send_event(convertedImage,LV_EVENT_CLICKED,nullptr);
        editor.MenuAction("image-converter");auto* imageTitle=FindText(lv_layer_top(),"Image converter");Require(imageTitle!=nullptr,"Image converter menu missing");
        auto* imageWindow=lv_obj_get_parent(lv_obj_get_parent(imageTitle));auto* imageBody=EditorWindow::Content(imageWindow);
        auto* imageInputs=FindText(imageBody,"Input files / folders");auto* imageInput=lv_obj_get_child(lv_obj_get_parent(imageInputs),1);
        lv_textarea_set_text(imageInput,(sourceRoot/"Tests/Fixtures/Images/sample.png").u8string().c_str());
        auto* useEmbedded=FindCheckbox(imageBody,"Use the selected image's embedded original");lv_obj_set_checked(useEmbedded,false);
        auto* outputLabel=FindText(imageBody,"Output folder (optional)");lv_textarea_set_text(lv_obj_get_child(lv_obj_get_parent(outputLabel),1),"");
        auto* colorLabel=FindText(imageBody,"Color format");lv_dropdown_set_selected(lv_obj_get_child(lv_obj_get_parent(colorLabel),1),3);
        auto* compressionLabel=FindText(imageBody,"Compression");lv_dropdown_set_selected(lv_obj_get_child(lv_obj_get_parent(compressionLabel),1),2);
        lv_obj_send_event(lv_obj_get_parent(FindText(imageBody,"Convert images")),LV_EVENT_CLICKED,nullptr);
        auto* imageStatus=lv_obj_get_child(imageBody,-1);const auto imageStart=SDL_GetTicks();while(std::string(lv_label_get_text(imageStatus)).find("Converting images") == 0 && SDL_GetTicks()-imageStart<30000)simulator.ProcessEvents();
        std::cerr<<lv_label_get_text(imageStatus)<<'\n';Require(std::string(lv_label_get_text(imageStatus)).find("Converted ")==0,"Image conversion failed");
        lv_obj_send_event(lv_obj_get_parent(FindText(imageBody,"Apply first image")),LV_EVENT_CLICKED,nullptr);
        Require(Serialization::ObjectSerializer::SerializeObject(convertedImage)["img"]["asset"]["conversion"]["compression"]=="LZ4","Image conversion options were not applied");
        lv_obj_send_event(EditorWindow::CloseButton(imageWindow),LV_EVENT_CLICKED,nullptr);

        auto imageId=ImageLibrary::Selection(convertedImage);Require(!imageId.empty(),"Converted image not registered in project library");
        editor.MenuAction("image-manager");auto* imageManagerTitle=FindText(lv_layer_top(),"Project images");Require(imageManagerTitle!=nullptr,"Image manager missing");
        auto* imageManager=lv_obj_get_parent(lv_obj_get_parent(imageManagerTitle));auto* imageManagerBody=EditorWindow::Content(imageManager);
        auto* imageList=lv_obj_get_child(imageManagerBody,1);uint32_t selectedImage=0;for(const auto& entry:static_cast<ObjectUserData*>(lv_obj_get_user_data(convertedImage))->toolTray->GetPropertyWindow()->Images().ToJSON()){if(entry.at("id")==imageId)break;++selectedImage;}
        lv_dropdown_set_selected(imageList,selectedImage);lv_obj_send_event(imageList,LV_EVENT_VALUE_CHANGED,nullptr);
        lv_textarea_set_text(lv_obj_get_child(imageManagerBody,3),"Reusable image");lv_obj_send_event(lv_obj_get_parent(FindText(imageManagerBody,"Rename")),LV_EVENT_CLICKED,nullptr);
        lv_obj_send_event(lv_obj_get_parent(FindText(imageManagerBody,"Remove unused image")),LV_EVENT_CLICKED,nullptr);
        Require(std::string(lv_label_get_text(lv_obj_get_child(imageManagerBody,-1))).find("in use")!=std::string::npos,"Image manager removed an in-use image");
        lv_obj_send_event(lv_obj_get_parent(FindText(imageManagerBody,"Close")),LV_EVENT_CLICKED,nullptr);
        lv_obj_send_event(convertedImage,LV_EVENT_CLICKED,nullptr);
        auto* imagePropertyButton=FindText(lv_screen_active(),"Project images...");Require(imagePropertyButton!=nullptr,"Image library properties missing");
        auto* imagePropertyContent=lv_obj_get_parent(lv_obj_get_parent(imagePropertyButton));lv_obj_t* imageChoice=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(imagePropertyContent);++i){auto* child=lv_obj_get_child(imagePropertyContent,i);if(lv_obj_get_class(child)==&lv_dropdown_class&&std::string(lv_dropdown_get_options(child)).find("Reusable image")!=std::string::npos)imageChoice=child;}
        Require(imageChoice&&lv_obj_get_class(imageChoice)==&lv_dropdown_class,"Image dropdown missing");
        Require(std::string(lv_dropdown_get_options(imageChoice)).find("Reusable image")!=std::string::npos,"Renamed image not in widget dropdown");
        lv_dropdown_set_selected(imageChoice,selectedImage+1);lv_obj_send_event(imageChoice,LV_EVENT_VALUE_CHANGED,nullptr);
        Require(ImageLibrary::Selection(convertedImage)==imageId,"Image dropdown did not apply reusable image");
        std::cerr<<"Project image manager and widget selection passed\n";

        lv_obj_t* imageButton=nullptr;for(uint32_t i=0;i<lv_obj_get_child_count(surface);++i)if(lv_obj_get_class(lv_obj_get_child(surface,i))==&lv_imagebutton_class)imageButton=lv_obj_get_child(surface,i);
        Require(imageButton!=nullptr,"No image button available");lv_obj_send_event(imageButton,LV_EVENT_CLICKED,nullptr);
        const char* stateNames[]={"Released","Pressed","Disabled","Checked","Checked pressed","Checked disabled"};const char* sideNames[]={"Left project image","Middle project image","Right project image"};
        for(size_t state=0;state<6;++state)for(size_t side=0;side<3;++side){
            auto* stateTitle=FindText(lv_screen_active(),stateNames[state]);Require(stateTitle!=nullptr,"Image button state editor missing");auto* stateGroup=lv_obj_get_parent(lv_obj_get_parent(stateTitle));auto* sideTitle=FindText(stateGroup,sideNames[side]);Require(sideTitle!=nullptr,"Image button slot dropdown missing");auto* choice=lv_obj_get_child(lv_obj_get_parent(sideTitle),1);
            Require(std::string(lv_dropdown_get_options(choice)).find("Reusable image")!=std::string::npos,"Image button dropdown missing library image");lv_dropdown_set_selected(choice,selectedImage+2);lv_obj_send_event(choice,LV_EVENT_VALUE_CHANGED,nullptr);simulator.ProcessEvents();
            Require(ImageLibrary::ButtonSelection(imageButton,state,side)==imageId,"Image button state selection was not applied");
        }
        std::cerr<<"Image button project selections passed for all six states and three slots\n";

        Click("Variables");
        Click(LV_SYMBOL_PLUS);
        auto* variablesToolsTitle=FindText(lv_screen_active(),"Tools");auto* variablesTools=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(variablesToolsTitle)));
        auto* nameLabel = FindText(variablesTools, "Name");
        Require(nameLabel != nullptr, "Variable name label is missing");
        auto* nameField = lv_obj_get_child(lv_obj_get_parent(nameLabel), 1);
        lv_textarea_set_text(nameField, "temperature");
        lv_obj_update_layout(lv_screen_active());
        Require(FindText(lv_screen_active(), "temperature") != nullptr, "Variable was not added or renamed");
        auto* form = lv_obj_get_parent(lv_obj_get_parent(nameLabel));
        auto* typeField = lv_obj_get_child(lv_obj_get_child(form, 0), 1);
        lv_dropdown_set_selected(typeField, VAR_DATE);
        lv_obj_send_event(typeField, LV_EVENT_VALUE_CHANGED, nullptr);
        auto* defaultField = lv_obj_get_child(lv_obj_get_child(form, 3), 1);
        lv_textarea_set_text(defaultField, "1700000000");
        lv_area_t labelBounds, fieldBounds;
        lv_obj_get_coords(nameLabel, &labelBounds); lv_obj_get_coords(nameField, &fieldBounds);
        Require(labelBounds.x2 < fieldBounds.x1, "Variable form labels overlap their inputs");
        std::cerr << "Variable form editing passed\n";

        const auto path = std::filesystem::temp_directory_path() / "uicreator-editor-workflow.json";
        const auto cDirectory = std::filesystem::temp_directory_path() / "uicreator-editor-generated-c";
        std::cerr << "Opening C export dialog after variable form edit\n";
        editor.MenuAction("export-c");
        std::cerr << "C export dialog action returned\n";
        auto* cTitle = FindText(lv_layer_top(), "Export C project");
        Require(cTitle != nullptr, "C export dialog did not open");
        auto* cDialog = lv_obj_get_parent(lv_obj_get_parent(cTitle));
        auto* cPath = lv_obj_get_child(EditorWindow::Content(cDialog), 0);
        lv_textarea_set_text(cPath, cDirectory.u8string().c_str());
        std::cerr << "Confirming C export to " << cDirectory.u8string() << '\n';
        editor.MenuAction("confirm-file");
        std::cerr << "C export confirm action returned\n";
        Require(FindText(lv_layer_top(), "Export C project") == nullptr, "C export reported an error");
        Require(std::filesystem::exists(cDirectory / "ui.c") && std::filesystem::exists(cDirectory / "ui_assets.c"), "C export did not write its source files");
        editor.MenuAction("save");
        auto* dialogTitle = FindText(lv_layer_top(), "Save project");
        auto* dialog = lv_obj_get_parent(lv_obj_get_parent(dialogTitle));
        auto* pathField = lv_obj_get_child(EditorWindow::Content(dialog), 0);
        lv_textarea_set_text(pathField, path.u8string().c_str());
        editor.MenuAction("confirm-file");
        Require(std::filesystem::exists(path), "File menu did not save the project");
        {
            std::ifstream savedFile(path);
            const auto document = json::parse(savedFile);
            Require(document["variables"].size() == 1 && document["variables"][0]["var"]["name"] == "temperature", "Variables were not saved");
            Require(document.contains("images")&&!document["images"].empty(),"Project image library not saved");
            Require(document.contains("fonts")&&document["fonts"].size()==1&&document["fonts"][0]["name"]=="Body font","Project font library was not saved");
            Require(document["variables"][0]["var"]["dataType"] == VAR_DATE && document["variables"][0]["var"]["default"] == 1700000000,
                "DateTime variable defaults lost their numeric representation");
        }
        editor.MenuAction("new");
        std::cerr << "Saved and cleared the project\n";
        Require(lv_obj_get_child_count(surface) == 0, "New project did not clear simulator widgets");
        editor.MenuAction("open");
        auto* reopenTitle = FindText(lv_layer_top(), "Open project");
        Require(reopenTitle != nullptr, "Open project dialog did not appear");
        auto* reopenField = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(reopenTitle))), 0);
        lv_textarea_set_text(reopenField, path.u8string().c_str());
        editor.MenuAction("confirm-file");
        std::cerr << "Reopened the project\n";
        Require(lv_obj_get_child_count(surface) == before - 1, "File menu did not restore all saved widgets");
        Require(FindText(lv_screen_active(), "temperature") != nullptr, "Variables were not restored");
        bool fontRestored = false;
        for (uint32_t i = 0; i < lv_obj_get_child_count(surface); ++i) {
            auto* object = lv_obj_get_child(surface, i);
            if (lv_obj_get_class(object) == &lv_label_class && lv_obj_get_style_text_font(object, LV_PART_MAIN)->line_height == convertedHeight) fontRestored = true;
        }
        Require(fontRestored, "Project reload lost the converted font");
        lv_obj_t* restoredTabs = nullptr;
        for (uint32_t i = 0; i < lv_obj_get_child_count(surface); ++i)
            if (lv_obj_get_class(lv_obj_get_child(surface, i)) == &lv_tabview_class) restoredTabs = lv_obj_get_child(surface, i);
        Require(restoredTabs != nullptr && lv_tabview_get_tab_count(restoredTabs) == 3, "Project reload lost edited tabs");
        Require(lv_obj_get_child_count(lv_obj_get_child(lv_tabview_get_content(restoredTabs), 1)) == 1,
            "Project reload lost or duplicated the widget inside the second tab");
        lv_obj_send_event(lv_obj_get_child(surface, 0), LV_EVENT_CLICKED, nullptr);
        editor.MenuAction("delete");
        Require(lv_obj_get_child_count(surface) == before - 2, "Edit menu did not delete the selected widget");
        const auto binaryPath = path.parent_path() / "uicreator-editor-workflow.pb";
        editor.MenuAction("export-protobuf");
        auto* exportTitle = FindText(lv_layer_top(), "Export device project");
        Require(exportTitle != nullptr, "Device export dialog did not open");
        auto* exportField = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(exportTitle))), 0);
        lv_textarea_set_text(exportField, binaryPath.u8string().c_str());
        editor.MenuAction("confirm-file");
        Require(std::filesystem::exists(binaryPath), "Device Protobuf project was not exported");
        editor.MenuAction("new");
        editor.MenuAction("open");
        auto* openTitle = FindText(lv_layer_top(), "Open project");
        auto* openField = lv_obj_get_child(EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(openTitle))), 0);
        lv_textarea_set_text(openField, binaryPath.u8string().c_str());
        editor.MenuAction("confirm-file");
        Require(lv_obj_get_child_count(surface) == before - 2, "Protobuf project did not restore all widgets");
        // Duplicating resource-backed widgets must reuse the catalogs and bindings.
        {
            auto baseline=editor.ProjectDocument();
            auto* hierarchyTitle=FindText(lv_screen_active(),"Hierarchy");auto* tree=EditorWindow::Content(lv_obj_get_parent(lv_obj_get_parent(hierarchyTitle)));
            for(const auto& item:{std::make_pair("Image",false),std::make_pair("Image button",true),std::make_pair("Bar",true)}) {
                auto* label=FindText(tree,item.first);Require(label,"Resource widget hierarchy entry is missing");
                SDL_SetModState(item.second?SDL_KMOD_SHIFT:SDL_KMOD_NONE);lv_obj_send_event(lv_obj_get_parent(label),LV_EVENT_CLICKED,nullptr);SDL_SetModState(SDL_KMOD_NONE);
            }
            Require(editor.SelectionCount()==3,"Resource widgets were not selected together");
            editor.MenuAction("duplicate");auto duplicated=editor.ProjectDocument();
            Require(duplicated["obj"].size()==baseline["obj"].size()+3,"Resource-backed widgets did not duplicate");
            Require(duplicated["images"]==baseline["images"] && duplicated["fonts"]==baseline["fonts"] && duplicated["variables"]==baseline["variables"],"Duplication changed shared resource catalogs or variables");
            auto references=[](const json& record) {
                std::multiset<std::string> result;
                std::function<void(const json&)> walk=[&](const json& value){
                    if(value.is_array()){for(const auto& child:value)walk(child);}
                    else if(value.is_object())for(const auto& item:value.items()) {
                        if((item.key()=="imageId" || item.key()=="projectImage" || item.key()=="fontId") && item.value().is_string())result.insert(item.key()+":"+item.value().get<std::string>());
                        else walk(item.value());
                    }
                };walk(record);return result;
            };
            for(const char* name:{"Image","Image button","Bar"}) {
                json original,copy;
                for(const auto& record:baseline["obj"])if(record["base"].value("name",std::string())==name)original=record;
                for(const auto& record:duplicated["obj"])if(record["base"].value("name",std::string())==std::string(name)+" copy")copy=record;
                Require(!original.is_null() && !copy.is_null() && references(original)==references(copy),"Duplicated widget lost resource references");
            }
            Require(editor.Undo() && editor.ProjectDocument()["obj"].size()==baseline["obj"].size(),"Resource duplicate undo failed");
            Require(editor.Redo() && editor.ProjectDocument()["obj"].size()==duplicated["obj"].size(),"Resource duplicate redo failed");
        }
        std::filesystem::remove(binaryPath);
        std::filesystem::remove(path);
        std::cout << "Editor creation, serialization, Delete key, and File/Edit/Help workflows passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}





