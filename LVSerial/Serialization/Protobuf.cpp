#include "Protobuf.h"
#include "ProjectTries.h"
#include "FontData.h"
#include "../Device/lvserial_defaults.h"
#include <set>
#include <stdexcept>
#include <cstring>
#include <cmath>

namespace Serialization
{
    namespace
    {
        using json = nlohmann::json;
        struct Property { lvserial_property value{}; std::string payload; };
        std::string Escape(std::string key)
        {
            std::string escaped;
            for (char ch : key) { if (ch == '~') escaped += "~0"; else if (ch == '/') escaped += "~1"; else escaped += ch; }
            return escaped;
        }
        bool MatchesDefault(const json& value, const lvserial_default& item)
        {
            switch (item.kind) {
            case lvserial_ValueKind_BOOLEAN: return value.is_boolean() && value.get<bool>() == (item.number != 0);
            case lvserial_ValueKind_SIGNED:
                if (!value.is_number_integer()) return false;
                if (value.is_number_unsigned()) return item.number >= 0 && value.get<uint64_t>() == uint64_t(item.number);
                return value.get<int64_t>() == item.number;
            case lvserial_ValueKind_TEXT: return value.is_string() && value.get_ref<const std::string&>() == item.text;
            case lvserial_ValueKind_REAL: return value.is_number_float() && value.get<double>() == double(item.number);
            case lvserial_ValueKind_ARRAY: return value.is_array() && value.empty();
            case lvserial_ValueKind_OBJECT: return value.is_object() && value.empty();
            default: return false;
            }
        }
        void Flatten(const json& value, const std::string& path, std::vector<Property>& properties, unsigned depth, bool omitDefaults)
        {
            if (depth > 64 || path.size() >= sizeof(lvserial_Property::path)) throw std::invalid_argument("Property path exceeds the device schema limits");
            Property entry;
            auto& property = entry.value.metadata;
            std::set<std::string> omitted;
            std::memcpy(property.path, path.c_str(), path.size() + 1);
            if (omitDefaults && FontData::Matches(value))
            {
                property.kind = lvserial_ValueKind_FONT_DATA;
                const auto bytes = FontData::Encode(value);
                entry.payload.assign(bytes.begin(), bytes.end());
                properties.push_back(std::move(entry));
                return;
            }
            if (value.is_object() || value.is_array())
            {
                property.kind = value.is_object() ? lvserial_ValueKind_OBJECT : lvserial_ValueKind_ARRAY;
                if (value.size() > UINT32_MAX) throw std::length_error("Container is too large");
                property.has_child_count = true; property.child_count = static_cast<uint32_t>(value.size());
                if (omitDefaults && value.is_object())
                {
                    uint32_t bestProfile = 0; uint64_t bestMask = 0; size_t bestSaving = 0;
                    for (uint32_t profile = 1; profile <= lvserial_defaults_profile_count(); ++profile)
                    {
                        size_t count = 0, saving = 0; uint64_t mask = 0;
                        const auto* defaults = lvserial_defaults_profile(profile, &count);
                        if (count > 64) throw std::logic_error("Default profile exceeds its mask");
                        for (size_t i = 0; i < count; ++i)
                        {
                            const auto found = value.find(defaults[i].key);
                            if (found != value.end() && MatchesDefault(*found, defaults[i]))
                            { mask |= uint64_t(1) << i; saving += path.size() + std::strlen(defaults[i].key) + 8; }
                        }
                        // Account for the profile ID and variable-length uint64 mask.
                        size_t maskBytes = 1; for (uint64_t n = mask; n >= 128; n >>= 7) ++maskBytes;
                        const size_t overhead = 3 + maskBytes;
                        if (saving > overhead && saving - overhead > bestSaving)
                        { bestSaving = saving - overhead; bestMask = mask; bestProfile = profile; }
                    }
                    if (bestProfile)
                    {
                        property.has_signed_value = true; property.signed_value = bestProfile;
                        property.has_unsigned_value = true; property.unsigned_value = bestMask;
                        size_t count = 0; const auto* defaults = lvserial_defaults_profile(bestProfile, &count);
                        for (size_t i = 0; i < count; ++i) if (bestMask & (uint64_t(1) << i))
                        {
                            const std::string key = defaults[i].key;
                            if (depth >= 64 || path.size() + 1 + Escape(key).size() >= sizeof property.path)
                                throw std::invalid_argument("Default property exceeds the device schema limits");
                            omitted.insert(key); --property.child_count;
                        }
                    }
                }
            }
            else if (value.is_null()) property.kind = lvserial_ValueKind_NULL_VALUE;
            else if (value.is_boolean()) { property.kind = lvserial_ValueKind_BOOLEAN; property.has_boolean_value = true; property.boolean_value = value.get<bool>(); }
            else if (value.is_number_unsigned()) { property.kind = lvserial_ValueKind_UNSIGNED; property.has_unsigned_value = true; property.unsigned_value = value.get<uint64_t>(); }
            else if (value.is_number_integer()) { property.kind = lvserial_ValueKind_SIGNED; property.has_signed_value = true; property.signed_value = value.get<int64_t>(); }
            else if (value.is_number_float())
            {
                property.kind = lvserial_ValueKind_REAL; property.has_real_value = true; property.real_value = value.get<double>();
                if (!std::isfinite(property.real_value)) throw std::invalid_argument("Non-finite numbers cannot be serialized");
            }
            else if (value.is_string()) { property.kind = lvserial_ValueKind_TEXT; entry.payload = value.get<std::string>(); }
            else if (value.is_binary()) { property.kind = lvserial_ValueKind_BINARY; const auto& bytes = value.get_binary(); entry.payload.assign(bytes.begin(), bytes.end()); }
            else throw std::invalid_argument("Unsupported property value");
            properties.push_back(std::move(entry));
            if (value.is_object()) for (const auto& child : value.items())
            { if (!omitted.count(child.key())) Flatten(child.value(), path + "/" + Escape(child.key()), properties, depth + 1, omitDefaults); }
            else if (value.is_array()) for (size_t i = 0; i < value.size(); ++i) Flatten(value[i], path + "/" + std::to_string(i), properties, depth + 1, omitDefaults);
        }
        bool Read(void* argument, size_t index, lvserial_property* property)
        {
            const auto& entry = static_cast<const std::vector<Property>*>(argument)->at(index);
            *property = entry.value;
            property->bytes = reinterpret_cast<const pb_byte_t*>(entry.payload.data());
            property->size = entry.payload.size();
            return true;
        }
        struct Builder
        {
            json result;
            bool rootSeen = false;
            bool borrowTrieAssets = false;
            const uint8_t* input = nullptr;
            size_t inputSize = 0;
            BorrowedAssets borrowedAssets;
            struct Container {
                std::string path; bool array; uint32_t expected; uint32_t actual = 0;
                std::set<std::string> keys;
            };
            std::vector<Container> containers;
            std::string error;
        };
        bool TriePayload(const std::string& path)
        {
            if (path.compare(0, 7, "/tries/") != 0) return false;
            auto index = size_t(7);
            while (index < path.size() && path[index] >= '0' && path[index] <= '9') ++index;
            return index > 7 && (path.compare(index, std::string::npos, "/data") == 0 ||
                                 path.compare(index, std::string::npos, "/indexData") == 0);
        }
        bool Visit(void* argument, const lvserial_record_info* info, const lvserial_Property* property, pb_istream_t* payload)
        {
            auto& builder = *static_cast<Builder*>(argument);
            try
            {
                const std::string path = property->path;
                if (!builder.rootSeen && !path.empty()) throw std::invalid_argument("Record root is missing");
                if (builder.rootSeen && path.empty()) throw std::invalid_argument("Duplicate property path");
                builder.rootSeen = true;
                if (!path.empty())
                {
                    const auto slash = path.rfind('/');
                    if (slash == std::string::npos) throw std::invalid_argument("Invalid property path");
                    const auto parent = path.substr(0, slash);
                    while (!builder.containers.empty() && builder.containers.back().path != parent)
                    {
                        if (builder.containers.back().actual != builder.containers.back().expected) throw std::invalid_argument("Incomplete property container");
                        builder.containers.pop_back();
                    }
                    if (builder.containers.empty()) throw std::invalid_argument("Property parent is missing");
                    auto& container = builder.containers.back();
                    if (container.array && path.substr(slash + 1) != std::to_string(container.actual)) throw std::invalid_argument("Invalid array index");
                    if (!container.array && !container.keys.insert(path.substr(slash + 1)).second) throw std::invalid_argument("Duplicate property path");
                    if (++container.actual > container.expected) throw std::invalid_argument("Too many container children");
                }
                json value;
                switch (property->kind)
                {
                case lvserial_ValueKind_NULL_VALUE: value = nullptr; break;
                case lvserial_ValueKind_OBJECT: value = json::object(); break;
                case lvserial_ValueKind_ARRAY: value = json::array(); break;
                case lvserial_ValueKind_BOOLEAN: value = property->boolean_value; break;
                case lvserial_ValueKind_SIGNED: value = property->signed_value; break;
                case lvserial_ValueKind_UNSIGNED: value = property->unsigned_value; break;
                case lvserial_ValueKind_REAL:
                    if (!std::isfinite(property->real_value)) throw std::invalid_argument("Invalid real number");
                    value = property->real_value; break;
                case lvserial_ValueKind_FONT_DATA:
                    if (!payload) throw std::invalid_argument("Missing font payload");
                    value = FontData::Decode(*payload); break;
                case lvserial_ValueKind_TEXT: case lvserial_ValueKind_BINARY:
                {
                    if (property->kind == lvserial_ValueKind_BINARY) {
                        if (builder.borrowTrieAssets && info->kind == lvserial_EntityKind_PROJECT &&
                            std::strcmp(info->type, "project") == 0 && TriePayload(path)) {
                            // lvserial_pb_decode uses only nanopb buffer streams;
                            // their state is the current immutable input pointer.
                            const auto* bytes = static_cast<const uint8_t*>(payload->state);
                            const auto begin = reinterpret_cast<uintptr_t>(builder.input);
                            const auto address = reinterpret_cast<uintptr_t>(bytes);
                            const size_t count = payload->bytes_left;
                            if (address < begin || count > builder.inputSize || address - begin > builder.inputSize - count)
                                throw std::invalid_argument("Trie payload is outside the Protobuf input");
                            builder.borrowedAssets.emplace(path, BorrowedBytes{bytes, count});
                            value = {{"borrowedBytes", path}, {"size", count}};
                            if (!pb_read(payload, nullptr, count)) throw std::invalid_argument("Truncated trie payload");
                        } else {
                            std::vector<uint8_t> bytes(payload->bytes_left);
                            if (!pb_read(payload, bytes.data(), bytes.size())) throw std::invalid_argument("Truncated property payload");
                            value = json::binary(std::move(bytes));
                        }
                    } else {
                        std::string text(payload->bytes_left, '\0');
                        if (!pb_read(payload, reinterpret_cast<pb_byte_t*>(text.data()), text.size())) throw std::invalid_argument("Truncated property payload");
                        value = std::move(text);
                    }
                    break;
                }
                default: throw std::invalid_argument("Unsupported property type");
                }
                builder.result[json::json_pointer(path)] = std::move(value);
                if (property->kind == lvserial_ValueKind_OBJECT || property->kind == lvserial_ValueKind_ARRAY)
                {
                    if (builder.containers.size() >= 65) throw std::invalid_argument("Too many nested containers");
                    builder.containers.push_back({path, property->kind == lvserial_ValueKind_ARRAY, property->child_count, 0, {}});
                }
                return true;
            }
            catch (const std::exception& error) { builder.error = error.what(); return false; }
        }
    }
    Binary Protobuf::Encode(EntityKind kind, const std::string& type, const json& data, bool omitDefaults)
    {
        lvserial_record_info info{};
        if (type.empty() || type.size() >= sizeof info.type) throw std::invalid_argument("Invalid entity type");
        info.format_version = omitDefaults ? LVSERIAL_BINARY_VERSION : LVSERIAL_BINARY_VERSION_LEGACY;
        info.kind = static_cast<lvserial_EntityKind>(kind);
        std::memcpy(info.type, type.c_str(), type.size() + 1);
        std::vector<Property> properties;
        if (kind == EntityKind::Project && data.contains("tries")) {
            auto wire = data;
            wire["tries"] = ProjectTries::ToWire(data.at("tries"));
            Flatten(wire, "", properties, 0, omitDefaults);
        } else Flatten(data, "", properties, 0, omitDefaults);
        pb_ostream_t sizing = PB_OSTREAM_SIZING;
        if (!lvserial_pb_encode(&sizing, &info, properties.size(), Read, &properties)) throw std::runtime_error(PB_GET_ERROR(&sizing));
        Binary output(sizing.bytes_written);
        auto stream = pb_ostream_from_buffer(output.data(), output.size());
        if (!lvserial_pb_encode(&stream, &info, properties.size(), Read, &properties)) throw std::runtime_error(PB_GET_ERROR(&stream));
        return output;
    }
    BinaryRecord Protobuf::Decode(const Binary& data)
    { return Decode(data.data(), data.size()); }
    static BinaryRecord DecodeRecord(const void* data, size_t size, bool borrowTrieAssets)
    {
        if (!data || !size) throw std::invalid_argument("Protobuf input is empty");
        Builder builder;
        builder.borrowTrieAssets = borrowTrieAssets;
        builder.input = static_cast<const uint8_t*>(data);
        builder.inputSize = size;
        lvserial_record_info info{};
        const char* error = nullptr;
        if (!lvserial_pb_decode(static_cast<const uint8_t*>(data), size, &info, Visit, &builder, &error))
            throw std::invalid_argument(builder.error.empty() ? (error ? error : "Invalid Protobuf record") : builder.error);
        if (!builder.rootSeen) throw std::invalid_argument("Record has no root property");
        for (const auto& container : builder.containers)
            if (container.actual != container.expected) throw std::invalid_argument("Incomplete property container");
        return {static_cast<EntityKind>(info.kind), info.type, std::move(builder.result), std::move(builder.borrowedAssets)};
    }
    BinaryRecord Protobuf::Decode(const void* data, size_t size) { return DecodeRecord(data, size, false); }
    BinaryRecord Protobuf::DecodeBorrowedAssets(const void* data, size_t size) { return DecodeRecord(data, size, true); }
}
