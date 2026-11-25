cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ESP32PredictionTests LANGUAGES CXX)
    enable_testing()
endif()

get_filename_component(prediction_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${prediction_root}/cmake/AdvancedKeyboardDependency.cmake")
set(prediction_sources "${ADVKBD_SOURCE_ROOT}/src")
foreach(platform ESP32 ArduinoESP32)
    set(test_target "UICreator${platform}PredictionTests")
    add_executable(${test_target}
        "${ADVKBD_SOURCE_ROOT}/tests/native/esp32/Main.cpp"
        "${prediction_sources}/Prediction.cpp"
        "${prediction_sources}/PredictionStorage.cpp"
        "${prediction_sources}/PredictionModel.cpp"
        "${prediction_sources}/PredictionPrefix.cpp"
        "${prediction_sources}/Utf8Fold.cpp")
    target_compile_features(${test_target} PRIVATE cxx_std_17)
    target_compile_definitions(${test_target} PRIVATE
        LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1
        LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=1)
    if(platform STREQUAL "ESP32")
        target_compile_definitions(${test_target} PRIVATE ESP_PLATFORM=1)
    else()
        target_compile_definitions(${test_target} PRIVATE ARDUINO_ARCH_ESP32=1)
    endif()
    target_include_directories(${test_target} PRIVATE
        "${ADVKBD_SOURCE_ROOT}/tests/native/stubs"
        "${ADVKBD_SOURCE_ROOT}/include" "${prediction_sources}")
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /utf-8)
    endif()
    add_test(NAME "UICreator${platform}Prediction" COMMAND ${test_target})
    set_tests_properties("UICreator${platform}Prediction" PROPERTIES TIMEOUT 30
        LABELS "unit;esp32;advanced-keyboard;prediction;psram")
endforeach()
