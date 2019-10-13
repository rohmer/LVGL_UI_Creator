#pragma once

#ifdef _WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif
#include <io.h>

#include <filesystem>
#include <stdio.h>
#include <string>
#include <vector>
#include "ListBox.h"

class FileBrowser
{
public:    
    FileBrowser(
        std::string startingDir, 
        lv_obj_t* parent=nullptr,        
        lv_style_t *style=&lv_style_pretty_color);
    FileBrowser(
        std::string startingDir, 
        lv_area_t coords, 
        lv_obj_t* parent = nullptr, 
        lv_style_t* style = &lv_style_pretty_color);
    FileBrowser(
        std::string startingDir, 
        std::vector<std::string> extensions, 
        lv_obj_t* parent=nullptr, 
        lv_style_t* style = &lv_style_pretty_color);
    FileBrowser(
        std::string startingDir, 
        lv_area_t coords, 
        std::vector<std::string> extensions, 
        lv_obj_t* parent = nullptr, 
        lv_style_t* style = &lv_style_pretty_color);

    bool IsActive();
    std::filesystem::path SelectedPath();

private:    
    std::vector<std::string> ext;
    // Objects for drawing
    lv_obj_t* win, * closeBtn, * upDir, * pathBox, * refreshBtn;
    ListBox* listBox;
    lv_style_t* style;

    bool active = false;
    std::filesystem::path path;

    void createObjects(lv_obj_t* parent, lv_area_t coords);
    void refreshObjects();

    static void closeWinCB(lv_obj_t* obj, lv_event_t ev);
    static void upDirCB(lv_obj_t* obj, lv_event_t ev);
    static void refreshCB(lv_obj_t* obj, lv_event_t ev);
    
};
