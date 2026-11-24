idf_build_get_property(espos_target IDF_TARGET)
if(espos_target STREQUAL "esp32p4")
    set(display_sources "StartupEsp.cpp" "CrowPanelDisplayEsp.cpp" "CrowPanelTouchEsp.cpp")
    set(display_requires esp_timer freertos log esp_lcd esp_driver_gpio esp_driver_i2c esp_hw_support task_scheduler)
endif()
idf_component_register(SRCS "TypingView.cpp" "TypingViewEsp.cpp" ${display_sources}
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl ngram_trie AdvancedKeyboard
                       PRIV_REQUIRES ${display_requires})
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ESPOS_KEYBOARD_LEGACY_VIEW_ADAPTER=1)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
