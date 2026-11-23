# Exact portable core mirrored from LVGL UI Creator. ESPOS board/UI adapters
# live in the parent Keyboard component and depend on this component.
set(ADVANCED_KEYBOARD_MODELS "en-US:16" CACHE STRING
    "Compiled corpus models: language:KiB pairs separated by semicolons, all, or none")
set_property(CACHE ADVANCED_KEYBOARD_MODELS PROPERTY STRINGS "en-US:16" "zh-CN:16" "zh-CN:256" "all" "none")
idf_build_get_property(advanced_keyboard_python PYTHON)
set(advanced_keyboard_generated "${CMAKE_CURRENT_BINARY_DIR}/bundled_models.generated.cpp")
set(advanced_keyboard_report "${CMAKE_CURRENT_BINARY_DIR}/bundled-models.json")

idf_component_register(SRCS
        "Key.cpp" "Keyboard.cpp" "TypingView.cpp" "SuggestionText.cpp"
        "AdvancedKeyboard.cpp" "Prediction.cpp"
        "KeyboardFont.c" "KeyboardHindiFont.c" "KeyboardBengaliFont.c"
        "KeyboardCyrillicFont.c" "KeyboardGreekFont.c" "KeyboardUrduFont.c"
        "BundledModels.cpp" "${advanced_keyboard_generated}"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lvgl ngram_trie
    PRIV_REQUIRES heap)
# ESP-IDF returns from idf_component_register during its dependency script pass.
# Build generation commands must only run in the real configure pass.
file(GLOB advanced_keyboard_model_inputs CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/bundled/models/*.ngtr")
add_custom_command(OUTPUT "${advanced_keyboard_generated}" "${advanced_keyboard_report}"
    COMMAND "${advanced_keyboard_python}" "${CMAKE_CURRENT_LIST_DIR}/generate_bundled_models.py"
        --catalog "${CMAKE_CURRENT_LIST_DIR}/bundled/catalog.json"
        --index-builder "${CMAKE_CURRENT_LIST_DIR}/layout/build_ngtr_index.py"
        --models "${ADVANCED_KEYBOARD_MODELS}"
        --output "${advanced_keyboard_generated}" --report "${advanced_keyboard_report}"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generate_bundled_models.py"
        "${CMAKE_CURRENT_LIST_DIR}/bundled/catalog.json"
        "${CMAKE_CURRENT_LIST_DIR}/layout/build_ngtr_index.py" ${advanced_keyboard_model_inputs}
    VERBATIM)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION=1 LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE=0
    LV_ADVANCED_KEYBOARD_USE_HINDI_FONT=1 LV_ADVANCED_KEYBOARD_USE_BENGALI_FONT=1
    LV_ADVANCED_KEYBOARD_USE_CYRILLIC_FONT=1 LV_ADVANCED_KEYBOARD_USE_GREEK_FONT=1
    LV_ADVANCED_KEYBOARD_USE_URDU_FONT=1 LV_ADVANCED_KEYBOARD_USE_PINYIN=1)
