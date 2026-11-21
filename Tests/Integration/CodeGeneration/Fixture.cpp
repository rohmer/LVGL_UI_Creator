#include "CCodeGenerator.h"
#include <Serialization/ObjectSerializer.h>
#include <Serialization/ExtendedWidgets.h>
#include <Serialization/CustomWidgets.h>
#include <Serialization/ImageAsset.h>
#include <Serialization/LVVariable.h>
#include <Serialization/LVFont.h>
#include <Serialization/Base64.h>
#include <fstream>
#include <filesystem>
#include <iostream>

using namespace Serialization;
int main(int argc,char** argv) {
    try {
        if(argc!=3)return 2;
        lv_init();auto* display=lv_display_create(800,600);
        json records=json::array(),ids=json::object();int id=1;
        for(const auto& type:ObjectSerializer::SupportedTypes()) {
            // The shared compatibility fixture must also compile on pre-9.6
            // targets. Dedicated CodeWidgets fixtures cover the new encoders.
            if(type=="barcode" || type=="qrcode" || type=="advancedKeyboard") continue;
            auto* object=ObjectSerializer::Create(type);if(!object)throw std::runtime_error("Cannot create "+type);
            lv_obj_set_size(object,180,100);lv_obj_set_pos(object,9,13);
            lv_obj_set_style_border_width(object,3,LV_PART_MAIN);
            if(type=="arc"){lv_arc_set_value(object,42);lv_obj_set_style_text_font(object,&lv_font_montserrat_14,LV_PART_MAIN);}
            if(type=="bar")lv_bar_set_value(object,37,LV_ANIM_OFF);
            if(type=="button")LVButton::SetText(object,"Push me");
            if(type=="label")lv_label_set_text(object,"Text \"quoted\" \\ newline\n UTF-8 \xc3\xa9");
            if(type=="checkbox"){lv_checkbox_set_text(object,"A check box");lv_obj_set_checked(object,true);}
            if(type=="dropdown"){lv_dropdown_set_options(object,"First\nSecond\nThird");lv_dropdown_set_selected(object,2);}
            if(type=="roller"){lv_roller_set_options(object,"One\nTwo\nThree",LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(object,1,LV_ANIM_OFF);}
            if(type=="slider")lv_slider_set_value(object,75,LV_ANIM_OFF);
            if(type=="spinbox")lv_spinbox_set_value(object,123);
            if(type=="switch")lv_obj_set_checked(object,true);
            if(type=="textarea")lv_textarea_set_text(object,"Type here");
            if(type=="led")lv_led_set_brightness(object,127);
            if(type=="line") { static const lv_point_precise_t points[]={{0,0},{30,20},{60,10}};lv_line_set_points(object,points,3);lv_line_set_y_invert(object,true); }
            if(type=="spinner")lv_spinner_set_anim_params(object,1200,80);
            if(type=="keyboard")lv_keyboard_set_mode(object,LV_KEYBOARD_MODE_NUMBER);
            if(type=="calendar")lv_calendar_set_today_date(object,2026,11,19);
            if(type=="image")ImageAsset::LoadFromFile(object,std::filesystem::path(argv[2])/"Tests/Fixtures/Images/sample.gif");
            if(type=="canvas")LVCanvas::Apply(object,{{"width",8},{"height",6},{"format",LV_COLOR_FORMAT_ARGB8888},{"fillColor",0x336699u}});
            if(type=="chart")ExtendedWidgets::Apply(object,{{"type",1},{"pointCount",4},{"series",{{{"color",0x4466aa},{"axis",0},{"y",{10,40,20,80}}}}},{"cursors",{{{"color",0xff0000},{"positionSet",false},{"series",0},{"point",2}}}}});
            if(type=="table")lv_table_set_cell_value(object,1,1,"Cell");
            if(type=="scale")ExtendedWidgets::Apply(object,{{"variant","gauge"},{"mode",LV_SCALE_MODE_ROUND_INNER},{"value",64}});
            if(type=="tabview"){lv_tabview_add_tab(object,"First");auto tab=lv_tabview_add_tab(object,"Second");lv_label_set_text(lv_label_create(tab),"Owned tab child");lv_tabview_set_active(object,1,LV_ANIM_OFF);}
            if(type=="tileview"){lv_tileview_add_tile(object,0,0,LV_DIR_RIGHT);lv_tileview_add_tile(object,1,0,LV_DIR_LEFT);}
            if(type=="window")ExtendedWidgets::Apply(object,{{"title","Settings"},{"headerHeight",32}});
            if(type=="messagebox"){lv_msgbox_add_title(object,"Question");lv_msgbox_add_text(object,"Continue?");lv_msgbox_add_close_button(object);lv_msgbox_add_footer_button(object,"OK");}
            if(type=="textBox")CustomWidgets::Apply(object,{{"title","Level"},{"value",65},{"step",5}});
            if(type=="layerView")CustomWidgets::Apply(object,{{"selected",2}});
            if(type=="object") {
                static const int32_t tracks[]={40,LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
                lv_obj_set_grid_dsc_array(object,tracks,tracks);
                static lv_grad_dsc_t gradient{};gradient.dir=LV_GRAD_DIR_VER;gradient.stops_count=2;
                gradient.stops[0]={lv_color_hex(0x112233),255,0};gradient.stops[1]={lv_color_hex(0xaabbcc),255,255};
                lv_obj_set_style_bg_grad(object,&gradient,LV_PART_MAIN);
            }
            auto record=ObjectSerializer::SerializeObject(object);record["oid"]=id;record["par"]=-1;ids[type]=id++;
            records.push_back(record);lv_obj_delete(object);
        }
        // Nested managed children, reverse ordering, all image input formats and all variable types.
        auto nested=json{{"base",{{"coords",{{"width",20},{"height",20}}}}},{"text","Nested managed child"},{"oid",100},{"par",ids["tabview"]},{"hostPath",{1,0}}};records.insert(records.begin(),nested);
        for(const char* ext:{"png","jpg"}) {
            auto* object=ObjectSerializer::Create("image");ImageAsset::LoadFromFile(object,std::filesystem::path(argv[2])/(std::string("Tests/Fixtures/Images/sample.")+ext));auto r=ObjectSerializer::SerializeObject(object);r["oid"]=id++;r["par"]=-1;records.push_back(r);lv_obj_delete(object);
        }
        for(auto& r:records)if(r.contains("imagebutton")) {
            r["imagebutton"]["states"][0][1]={{"kind","text"},{"source",(std::filesystem::path(argv[2])/"Tests/Fixtures/Images/sample.png").u8string()}};
        }
        json vars=json::array();
        auto add=[&](std::string name,VariableType type,json value,int oid,std::string property,int history){VariableData v;v.name=name;v.type=type;v.defaultValue=value;v.objectId=oid;v.property=property;v.history=history;vars.push_back(LVVariable::ToJSON(v));};
        add("level",VariableType::Integer,51,ids["bar"],"/bar/value",2);
        add("caption",VariableType::String,"Bound caption",ids["textarea"],"/textarea/text",2);
        add("enabled",VariableType::Boolean,true,ids["switch"],"/switch/checked",1);
        add("fraction",VariableType::Float,0.75,-1,"",0);
        add("timestamp",VariableType::DateTime,INT64_C(1770000000),-1,"",1);
        add("custom_level",VariableType::Integer,65,ids["textBox"],"/textBox/value",1);
        add("custom_selected",VariableType::Integer,2,ids["layerView"],"/layerView/selected",1);
        vars[0]["var"]["value"]={50,51};
        json project={{"screens",{{{"name","Main"},{"canvas",{{"width",800},{"height",600}}},{"obj",records}},{{"name","Other"},{"canvas",{{"width",800},{"height",600}}},{"obj",{{{"base",json::object()},{"cb",{{"text","Screen two"},{"checked",true}}},{"oid",1},{"par",-1}}}}}}},{"variables",vars}};
        project["screens"][0]["canvas"]["base"]["coords"]={{"x",5},{"y",9},{"width",800},{"height",600}};
        CCodeGenerator generator;if(!generator.LoadProject(project)||!generator.ExportProject(argv[1]))throw std::runtime_error(generator.GetLastError());
        std::ofstream(std::filesystem::path(argv[1])/"fixture.json")<<project.dump(2);
        auto proto=Protobuf::Encode(EntityKind::Project,"project",project);
        const auto full=Protobuf::Encode(EntityKind::Project,"project",project,false);
        if(proto.size()>=full.size())throw std::runtime_error("Default omission did not reduce project size");
        std::cout<<"Project Protobuf: "<<proto.size()<<" bytes (full: "<<full.size()<<" bytes)\n";
        std::ofstream pb(std::filesystem::path(argv[1])/"fixture.pb",std::ios::binary);pb.write(reinterpret_cast<const char*>(proto.data()),proto.size());pb.close();
        std::ofstream idsHeader(std::filesystem::path(argv[1])/"fixture_ids.h");for(auto it=ids.begin();it!=ids.end();++it)idsHeader<<"#define ID_"<<it.key()<<' '<<it.value()<<"\n";
        // Invalid documents must be rejected without creating their output directory.
        auto reject=[&](json document){CCodeGenerator invalid;invalid.LoadProject(document);auto dir=std::filesystem::path(argv[1])/"must-not-exist";if(invalid.ExportProject(dir.u8string())||std::filesystem::exists(dir))throw std::runtime_error("Invalid export accepted");};
        auto bad=project;bad["screens"][0]["obj"][0]["par"]=999999;reject(bad);
        bad=project;bad["screens"][0]["obj"].push_back(bad["screens"][0]["obj"][0]);reject(bad);
        bad=project;bad["variables"][0]["var"]["property"]="unsupported";reject(bad);
        bad=project;bad["screens"][0]["obj"][0]["par"]=100;reject(bad);
        bad=project;bad["activeScreen"]=100;reject(bad);
        lv_display_delete(display);lv_deinit();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
