if(UICREATOR_USE_LVGL9)
    if(NOT LV_BUILD_CONF_PATH AND NOT LV_BUILD_CONF_DIR)
        set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf_v9.h" CACHE FILEPATH
            "LVGL 9 configuration header" FORCE)
    endif()
    set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build upstream LVGL demos")
    set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build upstream LVGL examples")
    set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "Build the LVGL ThorVG backend")
    add_subdirectory(lvgl)
    get_target_property(editor_lvgl_sources lvgl SOURCES)
    list(FILTER editor_lvgl_sources EXCLUDE REGEX "/drivers/sdl/")
    set_property(TARGET lvgl PROPERTY SOURCES "${editor_lvgl_sources}")
    target_sources(lvgl PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UICreator/Drivers/SDL3/lv_sdl_window.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UICreator/Drivers/SDL3/lv_sdl_keyboard.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UICreator/Drivers/SDL3/lv_sdl_mouse.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UICreator/Drivers/SDL3/lv_sdl_mousewheel.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UICreator/Drivers/SDL3/lv_sdl_sw.c")
    target_link_libraries(lvgl PUBLIC SDL3::SDL3)
    return()
endif()

message(FATAL_ERROR "Editor builds require UICREATOR_USE_LVGL9=ON.")

