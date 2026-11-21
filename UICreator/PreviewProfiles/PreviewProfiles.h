#pragma once
#include "../ProjectThemes/DesignContext.h"
#include "ProfileModels.h"
#include <SDL3/SDL.h>
namespace PreviewProfiles {
void Open(ProjectAuthoring::DesignContext context);
void Close();
lv_obj_t* ActiveWindow();
bool HandleEvent(const SDL_Event& event);
void OpenLayout(ProjectAuthoring::DesignContext context);
void CloseLayout();
}
