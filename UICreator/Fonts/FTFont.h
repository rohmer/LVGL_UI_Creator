#pragma once
#include <algorithm>
#include <string>
#include <map>

#include <lvgl.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>

class FTFont
{
public:
	FTFont();
	bool Init();

	lv_font_t *ConvertFont(std::string fontFile,
		int fontPt,
		int startChar,
		int endChar,
		int DPI = LV_DPI,
		int bitsPerPixel = 1,
		int fontFaceIdx=0, 
		bool compress=true);
	
private:
	FT_Library ftLib;
	bool initialized = false;
	std::vector<uint8_t> compressFont(std::vector<uint8_t> bitmaps);
	
	inline int maximum(int a, int b)
	{
		if (a > b)
			return a;
		return b;
	}
	
};
