#include <ft2build.h>
#include FT_FREETYPE_H
#include <NativePng.h>
#include <libimagequant.h>
#include <resvg.h>
#include <array>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library)) return 2;
    FT_Face face = nullptr;
    const auto fontError = FT_New_Face(library, argv[1], 0, &face);
    if (fontError) { FT_Done_FreeType(library); return 3; }
    const bool rendered = !FT_Set_Pixel_Sizes(face, 0, 24) && !FT_Load_Char(face, 'A', FT_LOAD_RENDER) && face->glyph->bitmap.width > 0;
    FT_Done_Face(face); FT_Done_FreeType(library);
    if (!rendered) return 4;
    const std::array<uint8_t, 16> rgba{{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255}};
    auto png = NativePng::Encode(rgba.data(), 2, 2);
    if (png.size() < 8 || png[0] != 137 || png[1] != 'P') return 5;
    auto* quantizer = liq_attr_create();
    if (!quantizer || liq_version() <= 0) return 6;
    liq_attr_destroy(quantizer);
    auto* options = resvg_options_create();
    if (!options) return 7;
    const char* svg = "<svg xmlns='http://www.w3.org/2000/svg' width='2' height='2'><rect width='2' height='2' fill='red'/></svg>";
    resvg_render_tree* tree = nullptr;
    const auto svgError = resvg_parse_tree_from_data(svg, std::strlen(svg), options, &tree);
    resvg_options_destroy(options);
    if (svgError || !tree) return 8;
    std::array<uint8_t, 16> pixels{};
    resvg_transform transform{1, 0, 0, 1, 0, 0};
    resvg_render(tree, transform, 2, 2, reinterpret_cast<char*>(pixels.data()));
    resvg_tree_destroy(tree);
    if (pixels[0] != 255 || pixels[1] || pixels[2] || pixels[3] != 255) return 9;
    std::cout << "Native WOFF2, PNG, quantization and SVG dependencies linked and executed\n";
    return 0;
}
