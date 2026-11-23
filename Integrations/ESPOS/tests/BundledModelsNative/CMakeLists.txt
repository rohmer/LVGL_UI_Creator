cmake_minimum_required(VERSION 3.24)
project(ESPOSBundledModelsTest LANGUAGES C CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CREATOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(core "${CREATOR_ROOT}/LVSerial/Widgets/AdvancedKeyboard")
set(component "${CREATOR_ROOT}/Integrations/ESPOS/templates/Keyboard/AdvancedKeyboard")
if(NOT EXISTS "${LVGL_ARCHIVE}")
    message(FATAL_ERROR "Set LVGL_ARCHIVE to an existing native LVGL static library")
endif()
if(NOT LV_CONF_PATH)
    set(LV_CONF_PATH "${CREATOR_ROOT}/3rdParty/LVGL/lv_conf_v9.h")
endif()
set(models "${CMAKE_CURRENT_BINARY_DIR}/bundled_models.generated.cpp")
file(GLOB model_inputs CONFIGURE_DEPENDS "${CREATOR_ROOT}/UICreator/KeyboardModels/models/*.ngtr")
add_custom_command(OUTPUT "${models}"
    COMMAND "${Python3_EXECUTABLE}" "${component}/generate_bundled_models.py"
        --catalog "${CREATOR_ROOT}/UICreator/KeyboardModels/catalog.json"
        --index-builder "${core}/layout/build_ngtr_index.py"
        --models "en-US:16;zh-CN:16;zh-CN:256;ar-SA:16;ar-SA:256" --output "${models}"
    DEPENDS "${component}/generate_bundled_models.py"
        "${CREATOR_ROOT}/UICreator/KeyboardModels/catalog.json"
        "${core}/layout/build_ngtr_index.py" ${model_inputs}
    VERBATIM)
add_executable(bundled_models_test Main.cpp "${component}/BundledModels.cpp" "${models}")
foreach(source Key.cpp Keyboard.cpp TypingView.cpp SuggestionText.cpp AdvancedKeyboard.cpp
        Prediction.cpp PredictionModel.cpp PredictionPrefix.cpp PredictionStorage.cpp Utf8Fold.cpp
        KeyboardFont.c KeyboardHindiFont.c KeyboardBengaliFont.c KeyboardCyrillicFont.c
        KeyboardGreekFont.c KeyboardUrduFont.c)
    target_sources(bundled_models_test PRIVATE "${core}/${source}")
endforeach()
target_compile_features(bundled_models_test PRIVATE cxx_std_17)
target_include_directories(bundled_models_test PRIVATE "${component}/include" "${core}/include" "${core}"
    "${CREATOR_ROOT}/3rdParty/LVGL/lvgl" "${CREATOR_ROOT}/3rdParty/LVGL/lvgl/include"
    "${CREATOR_ROOT}/3rdParty/SDL3/include")
target_compile_definitions(bundled_models_test PRIVATE "LV_CONF_PATH=\"${LV_CONF_PATH}\""
    LV_KCONFIG_IGNORE LV_LVGL_H_INCLUDE_SIMPLE LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1
    LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=0 LV_ADVANCED_KEYBOARD_USE_HINDI_FONT=1
    LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT=1 LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT=1
    LV_ADVANCED_KEYBOARD_USE_GREEK_FONT=1 LV_ADVANCED_KEYBOARD_USE_URDU_FONT=1
    LV_ADVANCED_KEYBOARD_USE_PINYIN=1)
target_link_libraries(bundled_models_test PRIVATE "${LVGL_ARCHIVE}")
if(MSVC)
    target_compile_options(bundled_models_test PRIVATE /utf-8 /Zc:preprocessor)
endif()
enable_testing()
add_test(NAME ESPOSBundledModelsNative COMMAND bundled_models_test)
