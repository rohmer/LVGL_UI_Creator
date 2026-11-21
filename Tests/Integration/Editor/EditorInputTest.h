#pragma once
#include <lvgl/lvgl.h>
#include <SDL3/SDL.h>
#include <string>
#include <vector>

class UI;
class SdlSimulator;

namespace EditorInputTest {
void Require(bool condition,const std::string& message);
void Drain(SdlSimulator& simulator);
void Click(lv_obj_t* object,SdlSimulator& simulator);
void Key(SDL_Keycode key,SDL_Scancode scan,SdlSimulator& simulator,SDL_Keymod modifiers=SDL_KMOD_NONE);
void Text(const char* text,SdlSimulator& simulator);
void AssertFocused(lv_obj_t* field,SdlSimulator& simulator);
// Clears a textarea using physical Home/Delete events, then enters real SDL text.
// Spinboxes retain their native digit semantics; exercise them with Key/Text.
void Replace(lv_obj_t* field,const std::string& value,SdlSimulator& simulator);
lv_obj_t* FindLabel(lv_obj_t* root,const char* caption);
lv_obj_t* FindPlaceholder(lv_obj_t* root,const char* placeholder);
lv_obj_t* FindClass(lv_obj_t* root,const lv_obj_class_t* type);
lv_obj_t* FindField(lv_obj_t* root,const char* caption,const lv_obj_class_t* type=&lv_textarea_class);
std::vector<lv_obj_t*> Fields(lv_obj_t* root,const lv_obj_class_t* type=&lv_textarea_class);
lv_obj_t* ParentButton(lv_obj_t* label);
lv_obj_t* WindowFromTitle(const char* title);
}

void CheckAssetEditorInput(UI& editor,SdlSimulator& simulator);
void CheckPropertyEditorInput(UI& editor,SdlSimulator& simulator);
