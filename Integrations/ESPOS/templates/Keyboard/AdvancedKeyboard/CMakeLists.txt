# ESPOS IDF wrapper only. Runtime, fonts, layouts and registry code are owned by
# the pinned AdvancedKeyboard dependency, not this adapter directory.
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/advanced_keyboard_dependency.cmake")
espos_require_advanced_keyboard()
set(ADVANCED_KEYBOARD_MODELS "en-US:16" CACHE STRING
    "Compiled corpus models: language:KiB pairs separated by semicolons, all, or none")
set_property(CACHE ADVANCED_KEYBOARD_MODELS PROPERTY STRINGS "en-US:16" "zh-CN:16" "zh-CN:256" "all" "none")

set(ADVKBD_BUILD_PREDICTION ON)
set(ADVKBD_BUILTIN_MODEL OFF)
set(ADVKBD_ENABLE_PINYIN ON)
set(ADVKBD_SCRIPT_FONTS ON)
set(ADVKBD_BUNDLED_MODELS "${ADVANCED_KEYBOARD_MODELS}")
include("${ADVANCED_KEYBOARD_ROOT}/cmake/AdvancedKeyboardSources.cmake")
# The canonical helper has a GLOBAL include guard. Reset its scoped root for
# this component even when another component included it during discovery.
set(_ADVKBD_PACKAGE_ROOT "${ADVANCED_KEYBOARD_ROOT}")
advkbd_widget_sources(advanced_keyboard_ui_sources)
set(advanced_keyboard_generated "${CMAKE_CURRENT_BINARY_DIR}/bundled_models.generated.cpp")
set(advanced_keyboard_report "${CMAKE_CURRENT_BINARY_DIR}/bundled-models.json")
idf_component_register(SRCS ${advanced_keyboard_ui_sources}
        "${ADVANCED_KEYBOARD_ROOT}/src/BundledModels.cpp" "${advanced_keyboard_generated}"
    INCLUDE_DIRS "${ADVANCED_KEYBOARD_ROOT}/include"
    PRIV_INCLUDE_DIRS "${ADVANCED_KEYBOARD_ROOT}/src"
    REQUIRES lvgl ngram_trie PRIV_REQUIRES heap)

# Dependency discovery returns from idf_component_register. Build-only commands
# below run in the real configure pass. Generate an empty registry for `none`
# because the legacy ESPOS view still calls the registry API in that profile.
advkbd_add_registry_rule("${advanced_keyboard_generated}" "${advanced_keyboard_report}")
advkbd_feature_definitions(advanced_keyboard_definitions)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ${advanced_keyboard_definitions})
