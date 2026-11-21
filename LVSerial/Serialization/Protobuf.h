#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include "../Device/lvserial_pb.h"
#include <vector>
#include <string>
#include <map>

namespace Serialization
{
    using Binary = std::vector<uint8_t>;
    enum class EntityKind { Widget, Screen, Variable, Project, Style, Font, Asset };
    struct BorrowedBytes { const uint8_t* data = nullptr; size_t size = 0; };
    using BorrowedAssets = std::map<std::string, BorrowedBytes>;
    struct BinaryRecord { EntityKind kind; std::string type; nlohmann::json data; BorrowedAssets borrowedAssets; };
    class Protobuf
    {
    public:
        using BorrowedAssets = Serialization::BorrowedAssets;
        static Binary Encode(EntityKind kind, const std::string& type, const nlohmann::json& data, bool omitDefaults = true);
        static BinaryRecord Decode(const Binary& data);
        // Input is borrowed only for this call; no complete input-buffer copy.
        static BinaryRecord Decode(const void* data, size_t size);
        // Only trie catalog BINARY payloads are borrowed. Their input bytes must
        // remain readable until every object using the resulting catalog dies.
        static BinaryRecord DecodeBorrowedAssets(const void* data, size_t size);
    };
}
