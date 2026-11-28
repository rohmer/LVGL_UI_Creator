#pragma once
// Shared, LVGL-independent validation for widget and screen keyboard settings.
#include "../../3rdParty/JSON/json.hpp"
#include <cmath>
#include <cstdint>
#include <climits>
#include <stdexcept>
#include <string>
namespace Serialization {
inline nlohmann::json NormalizeAdvancedKeyboardSettings(const nlohmann::json& input)
{
    if (!input.is_object()) throw std::invalid_argument("Advanced Keyboard properties must be an object");
    auto result = input;
    auto text = [&](const char* key, const char* fallback) {
        const auto found = input.find(key);
        if (found == input.end()) return std::string(fallback);
        if (!found->is_string()) throw std::invalid_argument(std::string(key) + " must be a string");
        const auto value = found->get<std::string>();
        if (value.find('\0') != std::string::npos) throw std::invalid_argument(std::string(key) + " cannot contain NUL bytes");
        return value;
    };
    auto number = [&](const char* key, uint32_t fallback, uint32_t maximum) {
        const auto found = input.find(key);
        if (found == input.end()) return fallback;
        if (!found->is_number_integer() || (!found->is_number_unsigned() && found->get<int64_t>() < 0) || found->get<uint64_t>() > maximum)
            throw std::invalid_argument(std::string(key) + " must be an integer in its supported range");
        return found->get<uint32_t>();
    };
    for (const auto* old : {"text", "placeholder", "password", "maxLength"}) result.erase(old);
    const auto target = input.find("targetOid");
    if (target != input.end() && (!target->is_number_integer() ||
        (target->is_number_unsigned() && target->get<uint64_t>() > INT32_MAX) ||
        (!target->is_number_unsigned() && (target->get<int64_t>() < -1 || target->get<int64_t>() > INT32_MAX))))
        throw std::invalid_argument("targetOid must be -1 or a nonnegative object ID");
    result["targetOid"] = input.value("targetOid", -1);
    for (const auto* key : {"autoFocus", "hideOnDefocus", "autocorrect"}) {
        if (input.contains(key) && !input.at(key).is_boolean()) throw std::invalid_argument(std::string(key) + " must be a boolean");
        result[key] = input.value(key, true);
    }
    result["hideOnDefocus"] = false; // Upgrade earlier Creator records to explicit Enter/X dismissal.
    const auto language = text("language", "en-US");
    if (language != "en-US" && language != "es-ES" && language != "fr-FR" &&
        language != "de-DE" && language != "it-IT" && language != "pt-PT" &&
        language != "zh-CN" && language != "ar-SA" && language != "hi-IN" &&
        language != "bn-BD" && language != "id-ID" && language != "ur-PK" && language != "ru-RU" && language != "el-GR")
        throw std::invalid_argument("Unsupported Advanced Keyboard language");
    result["language"] = language;
    if (input.contains("rightToLeft") && !input.at("rightToLeft").is_boolean()) throw std::invalid_argument("rightToLeft must be a boolean");
    result["rightToLeft"] = input.value("rightToLeft", language == "ar-SA" || language == "ur-PK");
    result["trieId"] = text("trieId", language == "en-US" ? "builtin-en-us" : "");
    const auto trieSize = number("trieSizeKiB", 0, 256);
    if (trieSize != 0 && trieSize != 16 && trieSize != 256)
        throw std::invalid_argument("trieSizeKiB must be 0, 16 or 256");
    result["trieSizeKiB"] = trieSize;
    const auto threshold = input.find("autocorrectThreshold");
    if (threshold != input.end() && (!threshold->is_number() || !std::isfinite(threshold->get<double>()) || threshold->get<double>() < 0 || threshold->get<double>() > 1))
        throw std::invalid_argument("autocorrectThreshold must be between zero and one");
    result["autocorrectThreshold"] = input.value("autocorrectThreshold", 0.3);
    result["keyRadius"] = number("keyRadius", 6, 32767);
    result["keyBackground"] = 0xff000000u | (number("keyBackground", 0xffffffffu, UINT32_MAX) & 0xffffffu);
    result["keyForeground"] = 0xff000000u | (number("keyForeground", 0xff000000u, UINT32_MAX) & 0xffffffu);
    return result;
}
}
