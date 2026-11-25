cmake_minimum_required(VERSION 3.24)
project(ESPOSNativeAdapterTests LANGUAGES C CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${CMAKE_CURRENT_LIST_DIR}/../Dependency.cmake")
if(NOT EXISTS "${LVGL_ARCHIVE}")
    message(FATAL_ERROR "Set LVGL_ARCHIVE to a native LVGL archive built with LV_CONF_PATH")
endif()
if(NOT LV_CONF_PATH)
    set(LV_CONF_PATH "${CREATOR_ROOT}/3rdParty/LVGL/lv_conf_v9.h")
endif()
set(models "${CMAKE_CURRENT_BINARY_DIR}/bundled_models.generated.cpp")
file(GLOB model_inputs CONFIGURE_DEPENDS "${catalog_directory}/models/*.ngtr")
add_custom_command(OUTPUT "${models}"
    COMMAND "${Python3_EXECUTABLE}" "${model_generator}" --catalog "${catalog}"
        --index-builder "${index_builder}" --models "en-US:16;zh-CN:16;zh-CN:256;ar-SA:256" --output "${models}"
    DEPENDS "${model_generator}" "${catalog}" "${index_builder}" ${model_inputs} VERBATIM)
add_executable(native_adapter_test Main.cpp "${espos_adapter}/TypingView.cpp"
    "${core}/BundledModels.cpp" "${models}")
foreach(source Key.cpp Keyboard.cpp TypingView.cpp SuggestionText.cpp AdvancedKeyboard.cpp TextareaInputMode.c
        Prediction.cpp PredictionModel.cpp PredictionPrefix.cpp PredictionStorage.cpp Utf8Fold.cpp
        KeyboardFont.c KeyboardHindiFont.c KeyboardBengaliFont.c KeyboardCyrillicFont.c KeyboardGreekFont.c
        KeyboardUrduFont.c AdvancedKeyboardBuildChecks.c)
    target_sources(native_adapter_test PRIVATE "${core}/${source}")
endforeach()
target_compile_features(native_adapter_test PRIVATE cxx_std_17)
target_include_directories(native_adapter_test PRIVATE "${public_headers}" "${core}"
    "${espos_adapter}/include" "${CREATOR_ROOT}/3rdParty/LVGL/lvgl" "${CREATOR_ROOT}/3rdParty/LVGL/lvgl/include")
target_compile_definitions(native_adapter_test PRIVATE "LV_CONF_PATH=\"${LV_CONF_PATH}\"" LV_KCONFIG_IGNORE
    LV_LVGL_H_INCLUDE_SIMPLE ESPOS_KEYBOARD_LEGACY_VIEW_ADAPTER=1 ADVKBD_HAS_BUNDLED_MODELS=1
    LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1 LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=0
    LV_ADVANCED_KEYBOARD_USE_HINDI_FONT=1 LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT=1
    LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT=1 LV_ADVANCED_KEYBOARD_USE_GREEK_FONT=1
    LV_ADVANCED_KEYBOARD_USE_URDU_FONT=1 LV_ADVANCED_KEYBOARD_USE_PINYIN=1)
target_link_libraries(native_adapter_test PRIVATE "${LVGL_ARCHIVE}")
add_executable(headless_runtime_test Headless.cpp "${espos_adapter}/esp_runtime.cpp")
foreach(source Prediction.cpp PredictionModel.cpp PredictionPrefix.cpp PredictionStorage.cpp Utf8Fold.cpp)
    target_sources(headless_runtime_test PRIVATE "${core}/${source}")
endforeach()
target_compile_features(headless_runtime_test PRIVATE cxx_std_17)
target_compile_definitions(headless_runtime_test PRIVATE ESP_PLATFORM=1 ESPOS_KEYBOARD_HEADLESS_PREDICTION=1
    LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1 LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=0)
target_include_directories(headless_runtime_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs"
    "${ADVANCED_KEYBOARD_ROOT}/tests/native/stubs" "${ADVANCED_KEYBOARD_ROOT}/tests/native/radix"
    "${public_headers}" "${core}" "${espos_adapter}/include")
if(MSVC)
    target_compile_options(native_adapter_test PRIVATE /utf-8 /Zc:preprocessor)
    target_compile_options(headless_runtime_test PRIVATE /utf-8 /Zc:preprocessor)
endif()
enable_testing()
add_test(NAME ESPOSNativeAdapter COMMAND native_adapter_test)
add_test(NAME ESPOSHeadlessRuntime COMMAND headless_runtime_test "${catalog_directory}/models/en-US-256.ngtr")
add_test(NAME ESPOSHeadlessRadixRuntime COMMAND headless_runtime_test --formats)
