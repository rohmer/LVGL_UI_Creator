#pragma once
#include "Protobuf.h"
#include <cstddef>
#include <string>

namespace Serialization
{
    // Limits apply before parsing/allocating the document, as well as while
    // building individual values. Runtime entry points can impose lower limits.
    struct XmlLimits
    {
        size_t maxBytes = 64u * 1024u * 1024u;
        size_t maxDepth = 64;
        size_t maxNodes = 1000000;
        size_t maxTextBytes = 64u * 1024u * 1024u;
    };

    // UI Creator XML v1 is an independent typed representation of the same
    // entity records as Protobuf. It is not the LVGL Pro XML specification.
    class Xml
    {
    public:
        static std::string Encode(EntityKind kind, const std::string& type,
            const nlohmann::json& data, const XmlLimits& limits = {});
        // Input is read only during this call. All returned text/binary data is
        // owned, including trie payloads; borrowedAssets is always empty.
        static BinaryRecord Decode(const void* data, size_t size, const XmlLimits& limits = {});
        static BinaryRecord Decode(const std::string& data, const XmlLimits& limits = {});
    };
}
