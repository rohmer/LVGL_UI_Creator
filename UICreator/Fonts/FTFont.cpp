#include "FTFont.h"

FTFont::FTFont()
{	
}

bool FTFont::Init()
{
	FT_Error err=FT_Init_FreeType(&ftLib);
	if(err)
	{
		initialized = false;
		return false;
	}
	initialized = true;
	return true;
}

lv_font_t *FTFont::ConvertFont(
	std::string fontFile, 
	int fontPt, 
	int startChar, 
	int endChar, 
	int DPI,
	int bitsPerPixel,
	int fontFaceIdx, 
	bool compress)
{
	lv_font_t *lvFont = nullptr;
	if(!initialized)
	{
		if (!Init())
			return lvFont;
	}
	int bppMul = 0;
	switch (bitsPerPixel)
	{
	case 1:
		bppMul = 1;
		break;
	case 2:
		bppMul = 4;
		break;
	case 4:
		bppMul = 8;
		break;
	case 8:
		bppMul = 16;
		break;
	default:
		return lvFont;
	}
	FT_Face font;
	FT_Error rc = FT_New_Face(ftLib, fontFile.c_str(), fontFaceIdx, &font);
	if (rc != 0)
		return lvFont;

	rc = FT_Set_Char_Size(font,
		0,
		fontPt * 64,
		DPI,
		DPI);
	if (rc != 0)
		return lvFont;
		

	int i, j, i_idx, j_idx;
	int coverage;
	int ch;
	int maxWidth = 0;
	int maxHeight = 0;
	int maxAscent = 0;
	int maxDescent = 0;
	int bytesPerChar;
	int bytesPerRow;
	int baseLine = 0;
	/*
	 * First found out how big character bitmap is needed. Every character
	 * must fit into it so that we can obtain correct character positioning.
	 */
	for (ch = startChar; ch <= endChar; ch++) {

		int ascent;
		int descent;

		rc = FT_Load_Char(font, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
		if (rc) {

			fprintf(stderr, "load char err %d\n", rc);
			return lvFont;
		}

		descent = maximum(0, font->glyph->bitmap.rows - font->glyph->bitmap_top);
		ascent = maximum(0, maximum(font->glyph->bitmap_top, font->glyph->bitmap.rows) - descent);
		if(font->glyph->bitmap_top>baseLine)
			baseLine = font->glyph->bitmap_top;
		if (descent > maxDescent)
			maxDescent = descent;

		if (ascent > maxAscent)
			maxAscent = ascent;

		if (font->glyph->bitmap.width > maxWidth)
			maxWidth = font->glyph->bitmap.width;
	}

	maxWidth = maxWidth / bppMul;
	maxHeight = (maxAscent + maxDescent)/bppMul;
	switch(bitsPerPixel)
	{
		case 1:
			bytesPerRow = (maxWidth + 7) / 8;
			break;
		case 2:
			bytesPerRow = (maxWidth + 3) / 4;
			break;
		case 4:
			bytesPerRow = (maxWidth + 1) / 2;
			break;
		case 8:
			bytesPerRow = maxWidth;
			break;
		
	}
	bytesPerChar = bytesPerRow * maxHeight;

	std::vector<uint8_t> glyphBitmaps(bytesPerChar*(endChar - startChar + 1));
	std::vector<lv_font_fmt_txt_glyph_dsc_t> glyphDesc(endChar - startChar + 1);
	lv_font_fmt_txt_glyph_dsc_t id0;
	id0.adv_w = id0.bitmap_index = id0.box_h = id0.box_w = id0.ofs_x = id0.ofs_y = 0;
	glyphDesc[0] = id0;
	int idx = 0;
	static lv_font_fmt_txt_glyph_dsc_t* glyph_dsc = new lv_font_fmt_txt_glyph_dsc_t[endChar - startChar];

	for (ch = startChar; ch <= endChar; ch++)
	{
		int ascent;
		int descent;

		rc = FT_Load_Char(font, ch, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
		lv_font_fmt_txt_glyph_dsc_t gDesc;
		gDesc.bitmap_index = idx;
		gDesc.adv_w = font->glyph->advance.x;
		gDesc.box_h = font->glyph->metrics.height;
		gDesc.box_w = font->glyph->metrics.width;
		if (font->glyph->metrics.width == 0)
			gDesc.ofs_x = 0;
		else
			gDesc.ofs_x = font->glyph->metrics.width/bppMul-maxWidth;
		if (font->glyph->metrics.height == 0)
			gDesc.ofs_y = 0;
		else
			gDesc.ofs_y = font->glyph->metrics.height/bppMul-maxHeight;
		glyphDesc.push_back(gDesc);
		idx++;
		std::vector<uint8_t> bitmap;
		for (i = 0; i < font->glyph->bitmap.rows / bppMul; i++)
			for (j = 0; j < font->glyph->bitmap.width / bppMul; j++)
			{
				coverage = 0;

				for (i_idx = 0; i_idx < bppMul; i_idx++)
				{
					for (j_idx = 0; j_idx < bppMul; j_idx++)
					{
						uint8_t *bits = (uint8_t *)font->glyph->bitmap.buffer;
						uint8_t b = bits[(i*bppMul + i_idx) * font->glyph->bitmap.pitch + ((j*bppMul + j_idx) / 8)];

						if (b & (1 << (7 - ((j*bppMul + j_idx) % 8))))
						{
							coverage++;
						}
					}
				}
				int xpos, ypos, ind;

				xpos = j + (font->glyph->bitmap_left / bppMul);
				ypos = (maxAscent / bppMul) + i - (font->glyph->bitmap_top / bppMul);
				switch (bitsPerPixel)
				{
				case 1:
				{
					ind = ypos * bytesPerRow;
					ind += xpos / 8;

					if (coverage != 0)
						glyphBitmaps[((ch - startChar) * bytesPerChar) + ind] |= (1 << ((xpos % 8)));
				}
				break;
				case 2:
					{
					ind = ypos * bytesPerRow;
					ind += xpos / 4;

					if (coverage != 0)
						glyphBitmaps[((ch - startChar) * bytesPerChar) + ind] |= (1 << ((xpos % 4)));
					}
				case 4:
				{
					ind = ypos * bytesPerRow;
					ind += xpos / 2;

					if (coverage != 0)
						glyphBitmaps[((ch - startChar) * bytesPerChar) + ind] |= (1 << ((xpos % 2)));
				}
				case 8:
				{
					ind = ypos * bytesPerRow;
					ind += xpos;

					glyphBitmaps[((ch - startChar) * bytesPerChar) + ind] = (255 * coverage) / 256; // need to be 0..255 range

				}
				break;
				}
			}
	
	}

	lvFont = new lv_font_t();
	lvFont->base_line = baseLine;
	lvFont->line_height = maxHeight;
	static lv_font_fmt_txt_dsc_t font_dsc;
	const LV_ATTRIBUTE_LARGE_CONST uint8_t* glyphBitmap;
	if(compress)
	{
		glyphBitmaps=compressFont(glyphBitmaps);
	}
	glyphBitmap = glyphBitmaps.data();
	font_dsc.glyph_dsc = glyphDesc.data();
	font_dsc.bpp = bppMul;
	font_dsc.bpp = bitsPerPixel;
}

std::vector<uint8_t> FTFont::compressFont(std::vector<uint8_t> bitmaps)
{
	// Minimal repetitions count to enable RLE mode.
	const int RLE_SKIP_COUNT = 1;
	// Number of repeats, when `1` used to replace data
	// If more - write as number
	const int RLE_BIT_COLLAPSED_COUNT = 10;

	const int RLE_COUNTER_BITS = 6; // (2^bits - 1) - max value
	const int RLE_COUNTER_MAX = (1 << RLE_COUNTER_BITS) - 1;
	// Force flush if counter dencity exceeded.
	const int RLE_MAX_REPEATS = RLE_COUNTER_MAX + RLE_BIT_COLLAPSED_COUNT + 1;

	int offset = 0;
	while (offset < bitmaps.size())
	{
		int p = bitmaps[offset];
		
	}

	return bitmaps;
}
