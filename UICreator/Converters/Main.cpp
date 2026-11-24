#include "../Fonts/FontConverter.h"
#include "../Images/ImageConverter.h"
#include "../../3rdParty/JSON/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;
fs::path Path(const std::string& value) { return fs::u8path(value); }
void Write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) throw std::runtime_error("Cannot write " + path.u8string());
}
FontConversion::Options FontOptions(const json& request) {
    FontConversion::Options options;
    const auto args = request.at("args").get<std::vector<std::string>>();
    auto value = [&](size_t& index) -> const std::string& {
        if (++index >= args.size()) throw std::invalid_argument("Missing converter option value");
        return args[index];
    };
    auto number = [&](size_t& index) {
        const auto& text = value(index); size_t end = 0; int result = std::stoi(text, &end);
        if (end != text.size()) throw std::invalid_argument("Invalid numeric converter option");
        return result;
    };
    auto source = [&]() -> FontConversion::Source& {
        if (options.sources.empty()) throw std::invalid_argument("Specify --font before source options");
        return options.sources.back();
    };
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--font") { FontConversion::Source item; item.path = Path(value(i)); item.ranges.clear(); options.sources.push_back(item); }
        else if (arg == "--range") { auto& ranges = source().ranges; const auto& added = value(i); if (!ranges.empty()) ranges += ','; ranges += added; }
        else if (arg == "--symbols") source().symbols += value(i);
        else if (arg == "--autohint-off") source().hinting = 1;
        else if (arg == "--autohint-strong") source().hinting = 2;
        else if (arg == "--size") options.size = number(i);
        else if (arg == "--bpp") options.bpp = number(i);
        else if (arg == "--stride") options.stride = number(i);
        else if (arg == "--align") options.align = number(i);
        else if (arg == "--format") options.format = value(i);
        else if (arg == "--output") options.output = Path(value(i));
        else if (arg == "--lv-font-name") options.name = value(i);
        else if (arg == "--lv-include") options.include = value(i);
        else if (arg == "--lv-fallback") options.fallback = value(i);
        else if (arg == "--relative-project-path") options.relativeRoot = Path(value(i));
        else if (arg == "--no-compress") options.compress = false;
        else if (arg == "--no-prefilter") options.prefilter = false;
        else if (arg == "--no-kerning") options.kerning = false;
        else if (arg == "--force-fast-kern-format") options.fastKerning = true;
        else if (arg == "--use-color-info") options.colorInfo = true;
        else if (arg == "--full-info") options.fullInfo = true;
        else if (arg == "--lcd") options.smoothing = 1;
        else if (arg == "--lcd-v") options.smoothing = 2;
        else throw std::invalid_argument("Unknown converter option: " + arg);
    }
    return options;
}
json Files(const std::vector<fs::path>& files, const fs::path& job) {
    json result = json::array();
    size_t index = 0;
    for (const auto& file : files) {
        auto actual = fs::absolute(file).lexically_normal();
        auto relative = actual.lexically_relative(job);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
            relative = "output-" + std::to_string(index) + actual.extension().u8string();
            fs::copy_file(actual, job / relative, fs::copy_options::overwrite_existing);
        }
        result.push_back({{"staged", relative.generic_u8string()}, {"destination", actual.u8string()}, {"name", actual.filename().u8string()}});
        ++index;
    }
    return result;
}
int Run(const std::string& kind, const fs::path& input) {
    const auto requestPath = fs::absolute(input).lexically_normal();
    const auto job = requestPath.parent_path();
    std::ifstream stream(requestPath, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read converter request");
    const auto request = json::parse(stream);
    json manifest;
    if (kind == "font") {
        const auto result = FontConversion::Convert(FontOptions(request));
        if (!result.success) throw std::runtime_error(result.message);
        Write(job / "preview.bin", result.preview);
        manifest = {{"files", Files(result.files, job)}, {"bytes", result.bytes}, {"message", result.message}};
    } else if (kind == "image") {
        std::vector<fs::path> sources;
        for (const auto& source : request.value("sources", json::array())) sources.push_back(Path(source.get<std::string>()));
        const auto result = ImageConversion::Convert(sources, request.value("options", json::object()), job / "converted", nullptr, request.value("embedded", json::array()));
        if (!result.success) throw std::runtime_error(result.message);
        manifest = {{"assets", result.assets}, {"files", Files(result.files, job)}, {"message", result.message}};
    } else throw std::invalid_argument("Choose font or image conversion");
    auto pending = job / "result.json.tmp";
    { std::ofstream output(pending, std::ios::binary | std::ios::trunc); output << manifest.dump(2) << '\n'; output.close(); if (!output) throw std::runtime_error("Cannot write converter result"); }
    std::error_code ignored;
    fs::remove(job / "result.json", ignored);
    fs::rename(pending, job / "result.json");
    std::cout << manifest.at("message").get<std::string>() << '\n';
    return 0;
}
}
int Main(const std::vector<std::string>& args) {
    try {
        if (args.size() == 2 && args[1] == "--help") {
            std::cout << "UICreatorConverter font|image --request request.json\nNative LVGL font and image conversion.\n";
            return 0;
        }
        if (args.size() != 4 || args[2] != "--request") throw std::invalid_argument("Usage: UICreatorConverter font|image --request request.json");
        return Run(args[1], Path(args[3]));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(std::filesystem::path(argv[i]).u8string());
    return Main(args);
}
#else
int main(int argc, char** argv) { return Main({argv, argv + argc}); }
#endif
