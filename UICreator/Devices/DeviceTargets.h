#pragma once
#include "DeviceTools.h"
#include "../../3rdParty/JSON/json.hpp"
#include <optional>
namespace DeviceTargets {
using json=nlohmann::json;
json Serialize(const DeviceTools::Options& options);
DeviceTools::Options Restore(const json& value);
DeviceTools::Options Infer(const std::filesystem::path& project,const json& manifest={},const DeviceTools::Options& defaults={});
std::string Save(json& document,const std::string& name,const DeviceTools::Options& options,const std::string& replaceId={});
bool Remove(json& document,const std::string& id);
struct Progress {
    std::string phase="Waiting",pending;
    std::optional<unsigned> percent;
    void Feed(const std::string& output);
};
}
