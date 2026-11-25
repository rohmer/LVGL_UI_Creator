#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include "../Device/lvserial_pb.h"
#include <vector>

namespace Serialization {
// Wire-only adapter. Projects keep the established portable LVFont JSON model;
// the device codec stores its tables as typed protobuf fields and raw bytes.
class FontData {
public:
    static bool Matches(const nlohmann::json& font);
    static std::vector<uint8_t> Encode(const nlohmann::json& font);
    static nlohmann::json Decode(pb_istream_t& input);
    static nlohmann::json ToXML(const nlohmann::json& font);
    static nlohmann::json FromXML(nlohmann::json font);
};
}
