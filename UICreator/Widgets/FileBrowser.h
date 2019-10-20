#pragma once

#ifdef _WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#include <io.h>
#include <filesystem>
using pathType = std::filesystem::path;
using dirit = std::filesystem::directory_iterator;
using dirent = std::filesystem::directory_entry;
#else
#include <unistd.h>
#include <experimental/filesystem>
using pathType = std::experimental::filesystem::path;
using dirit = std::experimental::filesystem::directory_iterator;
using dirent = std::experimental::filesystem::directory_entry;
#define GetCurrentDir getcwd
#endif

#include <stdio.h>
#include <sstream>
#include <string>
#include <vector>
#include "ListBox.h"

class FileBrowser
{
public:
    typedef void (*fb_callback)(pathType selectedFile);

    FileBrowser(
        std::string startingDir,
        lv_obj_t* parent = nullptr,
        lv_style_t* style = &lv_style_pretty_color);
    FileBrowser(
        std::string startingDir,
        lv_area_t coords,
        lv_obj_t* parent = nullptr,
        lv_style_t* style = &lv_style_pretty_color);
    FileBrowser(
        std::string startingDir,
        std::vector<std::string> extensions,
        lv_obj_t* parent = nullptr,
        lv_style_t* style = &lv_style_pretty_color);
    FileBrowser(
        std::string startingDir,
        lv_area_t coords,
        std::vector<std::string> extensions,
        lv_obj_t* parent = nullptr,
        lv_style_t* style = &lv_style_pretty_color);

    pathType SelectedPath();
    void AddCloseCallback(fb_callback cb);

private:
    static std::vector<std::string> ext;
    // Objects for drawing
    lv_obj_t *win, *closeBtn, *upDir;
    static lv_obj_t *pathBox, *selectButton;
    static ListBox* listBox;
    lv_style_t* style;

    bool active = false;
    static pathType path;

    void createObjects(lv_obj_t* parent, lv_area_t coords);
    static void refreshObjects();

    static void closeWinCB(lv_obj_t* obj, lv_event_t ev);
    static void upDirCB(lv_obj_t* obj, lv_event_t ev);
    static void refreshCB(lv_obj_t* obj, lv_event_t ev);

    static void listBoxCB(std::string selected);
    static void selectCBLocal(lv_obj_t* obj, lv_event_t ev);

    fb_callback selectCB;
};
