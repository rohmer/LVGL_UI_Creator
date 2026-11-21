#pragma once
#include "Protobuf.h"
#include <cstddef>
#include <vector>

namespace Serialization
{
    enum class VariableType { String, Integer, Boolean, Float, DateTime };
    enum class VariableBindingDirection { TwoWay, ToWidget, FromWidget };
    struct VariableBinding
    {
        size_t screen = 0;
        int objectId = -1;
        std::string property;
        VariableBindingDirection direction = VariableBindingDirection::TwoWay;
        nlohmann::json extra = nlohmann::json::object();
    };
    struct VariableBindingProperty
    {
        std::string path, label;
        bool readable = false, writable = true;
    };
    struct VariableData
    {
        std::string name;
        VariableType type = VariableType::Integer;
        nlohmann::json defaultValue = 0;
        uint16_t history = 1;
        int objectId = -1;
        std::string property;
        nlohmann::json values = nlohmann::json::array();
        nlohmann::json extra = nlohmann::json::object();
    };
    class LVVariable
    {
    public:
        static nlohmann::json ToJSON(const VariableData& variable, bool storeValues = true);
        static VariableData FromJSON(const nlohmann::json& record);
        static Binary ToProtobuf(const VariableData& variable, bool storeValues = true);
        static VariableData FromProtobuf(const Binary& record);
        static nlohmann::json NormalizeList(const nlohmann::json& records);
        // Explicit bindings supersede the legacy oid/property/screen fields.
        // An absent list retains the old single-binding representation.
        static std::vector<VariableBinding> Bindings(const VariableData& variable);
        static nlohmann::json BindingToJSON(const VariableBinding& binding);
        static std::vector<VariableBindingProperty> BindingProperties(const nlohmann::json& widget, VariableType type);
        static void ValidateBinding(const VariableData& variable, const VariableBinding& binding, const nlohmann::json& widget);
        static std::string CodeType(const VariableData& variable);
        static std::string CodeSymbol(const VariableData& variable);
    };
}
