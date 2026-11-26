# The native C++ ports retain upstream attribution without vendoring the retired
# JavaScript/Python implementations. Keep installed notice locations stable.
include_guard(GLOBAL)
function(uicreator_install_converter_notices license_root document_root)
    foreach(component Fonts Images)
        if(component STREQUAL "Fonts")
            set(upstream lv_font_conv)
        else()
            set(upstream lvgl_image_converter)
        endif()
        set(notices "${PROJECT_SOURCE_DIR}/UICreator/${component}/upstream/${upstream}")
        install(FILES "${notices}/LICENSE" "${notices}/UPSTREAM_COMMIT"
            DESTINATION "${license_root}/${upstream}")
        install(FILES "${notices}/README.md"
            DESTINATION "${document_root}/UICreator/${component}/upstream/${upstream}")
    endforeach()
endfunction()
