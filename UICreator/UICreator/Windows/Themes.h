#pragma once


#pragma region Themes
static const char * th_options =
{
#if LV_USE_THEME_NIGHT
		"Night"
#endif

#if LV_USE_THEME_MATERIAL
		"\nMaterial"
#endif

#if LV_USE_THEME_ALIEN
		"\nAlien"
#endif

#if LV_USE_THEME_ZEN
		"\nZen"
#endif

#if LV_USE_THEME_NEMO
		"\nNemo"
#endif

#if LV_USE_THEME_MONO
		"\nMono"
#endif

#if LV_USE_THEME_DEFAULT
		"\nDefault"
#endif
		""
};
#pragma endregion
