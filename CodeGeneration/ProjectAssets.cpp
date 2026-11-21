#include "ProjectAssets.h"
#include <Serialization/Base64.h>
#include <Serialization/ProjectImages.h>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
namespace CGeneration
{
void EmbedLocalImages(nlohmann::json &project, const std::filesystem::path &source)
{
    using json = nlohmann::json;
    auto catalog = Serialization::ProjectImages::Normalize(project.value("images", json::array()));
    std::set<std::string> ids, names;
    for (const auto &entry : catalog)
    {
        ids.insert(entry.at("id"));
        names.insert(entry.at("name"));
    }
    std::map<std::filesystem::path, std::string> imported;
    auto import = [&](const std::string &name) -> std::string {
        if (name.empty() || name.find('\0') != std::string::npos)
            return {};
        auto path = std::filesystem::u8path(name);
        if (path.is_relative())
            path = source / path;
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
            return {};
        path = std::filesystem::weakly_canonical(path);
        auto found = imported.find(path);
        if (found != imported.end())
            return found->second;
        auto extension = path.extension().u8string();
        for (char &c : extension)
            c = (char)std::tolower((unsigned char)c);
        if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".gif")
            throw std::invalid_argument(
                "Import BMP/SVG and other external formats through the image manager before compiling");
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot read image: " + path.u8string());
        std::string data((std::istreambuf_iterator<char>(file)), {});
        if (data.empty() || data.size() > UINT32_MAX)
            throw std::runtime_error("Image is empty or too large: " + path.u8string());
        std::string id;
        size_t index = 1;
        do
        {
            id = "compiler_image_" + std::to_string(index++);
        } while (ids.count(id));
        ids.insert(id);
        auto displayName = path.filename().u8string();
        for (char &c : displayName)
            if (c == '\r' || c == '\n')
                c = ' ';
        auto stem = displayName;
        index = 2;
        while (names.count(displayName))
            displayName = stem + " (" + std::to_string(index++) + ")";
        names.insert(displayName);
        catalog.push_back(
            {{"id", id},
             {"name", displayName},
             {"image",
              {{"name", path.filename().u8string()},
               {"sourceData", base64_encode((const unsigned char *)data.data(), data.size())}}}});
        imported[path] = id;
        return id;
    };
    std::function<void(json &)> walk = [&](json &value) {
        if (value.is_array())
        {
            for (auto &item : value)
                walk(item);
            return;
        }
        if (!value.is_object())
            return;
        if (value.contains("kind") && value["kind"].is_string() && value["kind"] == "text" &&
            value.contains("source") && value["source"].is_string())
        {
            auto id = import(value["source"]);
            if (!id.empty())
            {
                value = {{"asset", {{"projectImage", id}}}, {"imageId", id}};
                return;
            }
        }
        if (value.contains("fileName") && value["fileName"].is_string() && !value.contains("asset"))
        {
            auto id = import(value["fileName"]);
            if (!id.empty())
            {
                value["asset"] = {{"projectImage", id}};
                value["srcType"] = 0;
                value.erase("fileName");
            }
        }
        for (auto &child : value.items())
            if (child.key() != "images")
                walk(child.value());
    };
    walk(project);
    if (!catalog.empty())
        project["images"] = std::move(catalog);
}
} // namespace CGeneration
