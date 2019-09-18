#include "UI.h"

UI::UI()
{
	lv_coord_t hres = lv_disp_get_hor_res(nullptr);
	lv_coord_t vres = lv_disp_get_ver_res(nullptr);

	lv_obj_t *screen = lv_obj_create(lv_disp_get_scr_act(nullptr), nullptr);
	lv_obj_set_size(screen, hres, vres);
	initializeThemes(0);
	activeTheme = themes[0];
	lv_theme_set_current(activeTheme);

	simWindow = new SimWindow(320, 240);
	toolTray = new ToolTray(lv_scr_act());
	propertyWindow = new PropertyWindow(simWindow,hres,vres);
}

#pragma region ThemeInit
void UI::initializeThemes(uint16_t hue)
{
	themes.clear();
#if LV_USE_THEME_NIGHT
	themes.push_back(lv_theme_night_init(hue, NULL));
#endif

#if LV_USE_THEME_MATERIAL
	themes.push_back(lv_theme_material_init(hue, NULL));
#endif

#if LV_USE_THEME_ALIEN
	themes.push_back(lv_theme_alien_init(hue, NULL));
#endif

#if LV_USE_THEME_ZEN
	themes.push_back(lv_theme_zen_init(hue, NULL));
#endif

#if LV_USE_THEME_NEMO
	themes.push_back(lv_theme_nemo_init(hue, NULL));
#endif

#if LV_USE_THEME_MONO
	themes.push_back(lv_theme_mono_init(hue, NULL));
#endif

#if LV_USE_THEME_DEFAULT
	themes.push_back(lv_theme_default_init(hue, NULL));
#endif
}
#pragma endregion
