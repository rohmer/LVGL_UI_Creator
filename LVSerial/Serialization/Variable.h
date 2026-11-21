#pragma once
#include "LVVariable.h"
#include <ctime>
#include <type_traits>
#include <utility>
#include <stdexcept>

namespace Serialization
{
    enum eVarDataType { VAR_STRING, VAR_INT, VAR_BOOL, VAR_FLOAT, VAR_DATE };
    template <typename T> class Variable
    {
    public:
        Variable(std::string name, T defaultValue, int objectID, std::string propertyName, uint16_t history = 1) :
            oid(objectID), property(std::move(propertyName)), varName(std::move(name)), defaultValue(std::move(defaultValue)), history(history) {}
        virtual ~Variable() = default;
        virtual T GetValue() { return values.empty() ? defaultValue : values.back(); }
        virtual void SetValue(T value)
        {
            if (history && values.size() >= history) values.erase(values.begin(), values.begin() + (values.size() - history + 1));
            values.push_back(std::move(value));
        }
        virtual std::vector<T> GetValues() { return values; }
        virtual void SetValues(std::vector<T> newValues)
        {
            if (history && newValues.size() > history) newValues.erase(newValues.begin(), newValues.end() - history);
            values = std::move(newValues);
        }
        void ClearValues() { values.clear(); }
        nlohmann::json ToJSON(bool storeValues = true) const
        {
            VariableData data;
            data.name = varName; data.defaultValue = defaultValue; data.objectId = oid; data.property = property;
            data.type = Type(); data.history = history; data.values = values; data.extra = extra;
            return LVVariable::ToJSON(data, storeValues);
        }
        static Variable FromJSON(const nlohmann::json& record)
        {
            const auto data = LVVariable::FromJSON(record);
            if (data.type != Type()) throw std::invalid_argument("Variable type does not match the requested C++ type");
            Variable result(data.name, data.defaultValue.template get<T>(), data.objectId, data.property, data.history);
            result.SetValues(data.values.template get<std::vector<T>>()); result.extra = data.extra;
            return result;
        }
        Binary ToProtobuf(bool storeValues = true) const
        { return Protobuf::Encode(EntityKind::Variable, "variable", ToJSON(storeValues)); }
        static Variable FromProtobuf(const Binary& input)
        { return FromJSON(LVVariable::ToJSON(LVVariable::FromProtobuf(input))); }
    private:
        static constexpr VariableType Type()
        {
            if constexpr (std::is_same_v<T, std::string>) return VariableType::String;
            else if constexpr (std::is_same_v<T, bool>) return VariableType::Boolean;
            else if constexpr (std::is_floating_point_v<T>) return VariableType::Float;
            else if constexpr (std::is_same_v<T, std::time_t>) return VariableType::DateTime;
            else return VariableType::Integer;
        }
        int oid;
        std::string property, varName;
        T defaultValue;
        uint16_t history;
        std::vector<T> values;
        nlohmann::json extra = nlohmann::json::object();
    };
}
