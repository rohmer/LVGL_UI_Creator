#pragma once
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_ERR_NOT_FOUND = 1, ESP_ERR_INVALID_SIZE = 2,
    ESP_ERR_INVALID_RESPONSE = 3, ESP_ERR_NO_MEM = 4;
inline const char* esp_err_to_name(esp_err_t) { return "test error"; }
