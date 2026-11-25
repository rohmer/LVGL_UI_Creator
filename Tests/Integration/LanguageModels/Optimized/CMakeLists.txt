cmake_minimum_required(VERSION 3.24)
project(OptimizedLanguageModels LANGUAGES CXX)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
include("${root}/cmake/AdvancedKeyboardDependency.cmake")
set(core "${ADVKBD_SOURCE_ROOT}/src")
add_subdirectory("${root}/UICreator/LanguageModels" trainer)
add_library(prediction_facade STATIC "${core}/Prediction.cpp" "${core}/PredictionPrefix.cpp"
    "${core}/Utf8Fold.cpp" "${core}/PredictionStorage.cpp")
target_compile_features(prediction_facade PUBLIC cxx_std_17)
target_include_directories(prediction_facade PUBLIC "${core}" "${ADVKBD_SOURCE_ROOT}/include"
    "${root}/3rdParty/LVGL/lvgl" "${root}/3rdParty/LVGL/lvgl/include")
target_compile_definitions(prediction_facade PUBLIC ESPOS_KEYBOARD_HEADLESS_PREDICTION=1
    LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1 LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=1
    LV_CONF_SKIP=1 LV_KCONFIG_IGNORE=1)
target_link_libraries(prediction_facade PUBLIC UICreatorLanguageModels)
add_executable(legacy_generator "${ADVKBD_SOURCE_ROOT}/tests/native/language-models/Generator.cpp")
target_include_directories(legacy_generator PRIVATE "${ADVKBD_SOURCE_ROOT}/tests/third_party")
target_link_libraries(legacy_generator PRIVATE prediction_facade)
add_executable(optimized_generator "${ADVKBD_SOURCE_ROOT}/tests/native/language-models/Optimized.cpp")
target_include_directories(optimized_generator PRIVATE "${ADVKBD_SOURCE_ROOT}/tests/third_party")
target_link_libraries(optimized_generator PRIVATE prediction_facade)
if(MSVC)
    foreach(target prediction_facade legacy_generator optimized_generator)
        target_compile_options(${target} PRIVATE /utf-8 /EHsc)
    endforeach()
endif()
enable_testing()
add_test(NAME LegacyGeneratorParity COMMAND legacy_generator
    "${ADVKBD_SOURCE_ROOT}/tests/fixtures/language-models" "${CMAKE_CURRENT_BINARY_DIR}/legacy-output"
    "${ADVKBD_SOURCE_ROOT}/tools/NativeTrieGenerator/source/wordlists")
add_test(NAME OptimizedGenerator COMMAND optimized_generator "${CMAKE_CURRENT_BINARY_DIR}/optimized-output")
set_tests_properties(LegacyGeneratorParity OptimizedGenerator PROPERTIES TIMEOUT 150)
