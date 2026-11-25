#include "FontConverter.h"
#include "../Utils/CIdentifier.h"
#include "NativeFont.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace FontConversion {
namespace {
bool Identifier(const std::string& name) {
    return CIdentifier::Valid(name);
}
template<class T> bool OneOf(T value,std::initializer_list<T> values) {return std::find(values.begin(),values.end(),value)!=values.end();}
void ValidateRaster(const Options& o) {
    if(o.sources.empty())throw std::invalid_argument("Add at least one source font");
    if(o.size<1||o.size>4096)throw std::invalid_argument("Font size must be between 1 and 4096 pixels");
    if(!OneOf(o.bpp,{1,2,3,4,8}))throw std::invalid_argument("Choose 1, 2, 3, 4, or 8 bits per pixel");
    if(!OneOf(o.stride,{0,1,4,8,16,32,64})||!OneOf(o.align,{1,4,8,16,32,64,128,256,512,1024}))throw std::invalid_argument("Invalid bitmap alignment");
    if(o.stride&&(o.compress||o.bpp==3))throw std::invalid_argument("Stride alignment requires compression off and 1, 2, 4, or 8 bpp");
    if(!OneOf(o.smoothing,{0,1,2}))throw std::invalid_argument("Invalid subpixel rendering mode");
    for(const auto& source:o.sources) {
        if(!std::filesystem::is_regular_file(source.path))throw std::invalid_argument("Font file not found: "+source.path.u8string());
        if(source.ranges.empty()&&source.symbols.empty())throw std::invalid_argument("Each source needs Unicode ranges or symbols");
        if(source.hinting<0||source.hinting>2)throw std::invalid_argument("Invalid font hinting mode");
    }
}
void Write(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot write " + path.u8string());
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    out.close();
    if (!out) throw std::runtime_error("Cannot write " + path.u8string());
}
}
std::string SymbolName(const std::filesystem::path& source,int size) {
    std::string name="ui_"+source.stem().u8string()+"_"+std::to_string(size);for(char& c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))c='_';return name;
}
std::vector<std::string> Arguments(const Options& o,bool preview) {
    ValidateRaster(o);
    if(o.format!="lvgl"&&o.format!="bin"&&o.format!="dump")throw std::invalid_argument("Choose LVGL C, binary, or glyph dump output");
    if(o.bpp==3&&!o.compress&&o.format!="dump")throw std::invalid_argument("LVGL 3 bpp fonts require compression; enable compression or choose uncompressed 4 bpp");
    if(o.output.empty())throw std::invalid_argument("Choose an output file or folder");
    if(!Identifier(o.name)||(!o.fallback.empty()&&!Identifier(o.fallback)))throw std::invalid_argument("Font and fallback names must be valid C identifiers");
    if(o.include.empty()||o.include.find_first_of("\"\r\n")!=std::string::npos)throw std::invalid_argument("Invalid LVGL include path");
    auto output=std::filesystem::absolute(o.output).lexically_normal();
    if(o.format=="lvgl" && output.extension()!=".c")throw std::invalid_argument("LVGL C output must have a .c extension");
    std::vector<std::string> args;
    auto add=[&](const char* flag,const std::string& value){args.emplace_back(flag);args.push_back(value);};
    for(const auto& source:o.sources) {
        if(!std::filesystem::is_regular_file(source.path))throw std::invalid_argument("Font file not found: "+source.path.u8string());
        if(std::filesystem::exists(output)&&std::filesystem::equivalent(output,source.path))throw std::invalid_argument("The output would overwrite a source font");
        if(source.ranges.empty()&&source.symbols.empty())throw std::invalid_argument("Each source needs Unicode ranges or symbols");
        if(source.hinting<0||source.hinting>2)throw std::invalid_argument("Invalid font hinting mode");
        add("--font",std::filesystem::absolute(source.path).u8string());
        if(!source.ranges.empty())add("--range",source.ranges);
        if(!source.symbols.empty())add("--symbols",source.symbols);
        if(source.hinting==1)args.emplace_back("--autohint-off");if(source.hinting==2)args.emplace_back("--autohint-strong");
    }
    add("--size",std::to_string(o.size));add("--bpp",std::to_string(preview&&o.bpp==3?4:o.bpp));add("--format",preview?"bin":o.format);
    add("--output",preview?"preview.bin":output.u8string());
    if(preview||!o.compress)args.emplace_back("--no-compress");
    if(!o.prefilter)args.emplace_back("--no-prefilter");if(!o.kerning)args.emplace_back("--no-kerning");
    if(o.fastKerning)args.emplace_back("--force-fast-kern-format");if(o.colorInfo)args.emplace_back("--use-color-info");
    if(o.smoothing)args.emplace_back(o.smoothing==1?"--lcd":"--lcd-v");
    add("--stride",std::to_string(preview?0:o.stride));add("--align",std::to_string(preview?1:o.align));
    if(!preview&&o.format=="lvgl") {add("--lv-font-name",o.name);add("--lv-include",o.include);if(!o.fallback.empty())add("--lv-fallback",o.fallback);}
    if(!preview&&!o.relativeRoot.empty())add("--relative-project-path",o.relativeRoot.u8string());
    if(!preview&&o.fullInfo)args.emplace_back("--full-info");return args;
}
Result Prepare(const Options& options, const std::atomic<bool>* cancel) {
    Result result;
    try {
        ValidateRaster(options);
        if(options.bpp==3&&!options.compress)throw std::invalid_argument("LVGL 3 bpp fonts require compression; enable compression or choose 4 bpp");
        auto rasterOptions=options;
        rasterOptions.format="bin";rasterOptions.fullInfo=false;
        auto font=Native::Rasterize(rasterOptions,cancel);
        result.font=Native::ProjectFont(font,rasterOptions);
        auto previewOptions=rasterOptions;
        // Binary fonts have no row-stride field; pack preview rows tightly.
        // Keep selected quantization and compression for an accurate preview.
        previewOptions.stride=0;previewOptions.align=1;
        result.preview=Native::Binary(font,previewOptions);
        if(cancel&&cancel->load())throw std::runtime_error("Conversion cancelled");
        result.bytes=result.preview.size();
        result.success=true;
        result.message="Prepared "+std::to_string(font.glyphs.size())+" glyphs. Font data is stored in the project and included automatically when exporting screens.";
    } catch(const std::exception& error) {
        result.message=error.what();result.preview.clear();result.font=nullptr;
    }
    return result;
}
Result Convert(const Options& options, const std::atomic<bool>* cancel) {
    Result result;
    try {
        (void)Arguments(options);
        if (cancel && cancel->load()) throw std::runtime_error("Conversion cancelled");
        auto font = Native::Rasterize(options, cancel);
        auto previewOptions = options;
        previewOptions.compress = false;
        previewOptions.stride = 0;
        previewOptions.align = 1;
        if (previewOptions.bpp == 3) previewOptions.bpp = 4;
        result.preview = Native::Binary(font, previewOptions);
        std::vector<std::pair<std::filesystem::path, std::vector<uint8_t>>> outputs;
        const auto output = std::filesystem::absolute(options.output).lexically_normal();
        if (options.format == "bin") outputs.push_back({output, Native::Binary(font, options)});
        else if (options.format == "dump") outputs = Native::Dump(font, options, cancel);
        else {
            auto source = Native::CSource(font, options);
            outputs.push_back({output, std::vector<uint8_t>(source.begin(), source.end())});
            auto header = output; header.replace_extension(".h");
            const auto declaration = "#pragma once\n#include \"" + options.include + "\"\n#ifdef __cplusplus\nextern \"C\" {\n#endif\nLV_FONT_DECLARE(" + options.name + ");\n#ifdef __cplusplus\n}\n#endif\n";
            outputs.push_back({header, std::vector<uint8_t>(declaration.begin(), declaration.end())});
        }
        if (cancel && cancel->load()) throw std::runtime_error("Conversion cancelled");
        // Generate and validate every output before publishing any files.
        for (const auto& item : outputs) {
            if (!std::filesystem::exists(item.first)) continue;
            for (const auto& source : options.sources)
                if (std::filesystem::equivalent(item.first, source.path)) throw std::invalid_argument("The output would overwrite a source font");
        }
        for (const auto& item : outputs) {
            const auto destination = std::filesystem::absolute(item.first).lexically_normal();
            if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
            Write(destination, item.second);
            result.files.push_back(destination);
            result.bytes += item.second.size();
        }
        result.success = true;
        result.message = "Converted " + std::to_string(font.glyphs.size()) + " glyphs from " + std::to_string(options.sources.size()) + " font source(s), " + std::to_string(result.bytes) + " output bytes.";
        if (options.compress && options.bpp == 8) result.message += " 8 bpp glyphs are stored uncompressed for LVGL compatibility.";
    } catch (const std::exception& error) {
        result.message = error.what();
        result.preview.clear();
    }
    return result;
}
}
