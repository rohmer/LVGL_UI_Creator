#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyWindow.h"
#include "ToolTray.h"
#include "SimWindow.h"
#include "ToolBar.h"
#include "../Fonts/FTFont.h"
#include "Themes.h"
#include "../Widgets/ColorPicker.h"
#include "../../3rdParty/spdlog-1.x/include/spdlog/spdlog.h"
#ifdef _WINDOWS
#include <spdlog/spdlog.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#else
#include "../../3rdParty/spdlog-1.x/include/spdlog/sinks/ansicolor_sink.h"
#endif

class UI
{
public:
	UI(lv_indev_t* kb_indev);

private:
	lv_theme_t *activeTheme;
	std::vector<lv_theme_t *> themes;
	void initializeThemes(uint16_t hue);

	PropertyWindow *propertyWindow;
	ToolTray *toolTray;
	SimWindow *simWindow;
	ToolBar *toolBar;
};