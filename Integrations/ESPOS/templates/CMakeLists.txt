cmake_minimum_required(VERSION 3.22)

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/Drivers"
    "${CMAKE_CURRENT_LIST_DIR}/Configuration")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(ESPOS_CONFIG "${CMAKE_CURRENT_LIST_DIR}/espos.yaml" CACHE FILEPATH "ESPOS YAML configuration")
set(ESPOS_NGRAM_MODEL "" CACHE FILEPATH "Compact NGTR model to flash into the ngram partition")
set(ESPOS_NGRAM_BENCH_CASES "" CACHE FILEPATH "GBUTest reference header for the device benchmark")
get_filename_component(ESPOS_CONFIG "${ESPOS_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${ESPOS_CONFIG}" "${CMAKE_CURRENT_LIST_DIR}/tools/espos_config.py")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tools/espos_config.py"
            generate "${ESPOS_CONFIG}" --output-dir "${CMAKE_BINARY_DIR}/generated"
    RESULT_VARIABLE espos_config_result
    ERROR_VARIABLE espos_config_error)
if(NOT espos_config_result EQUAL 0)
    message(FATAL_ERROR "ESPOS configuration failed\n${espos_config_error}")
endif()

include("${CMAKE_BINARY_DIR}/generated/espos_components.cmake")
if(ESPOS_USE_LVGL)
    # Apply script glyphs and logical Unicode bidi to fresh keyboard sdkconfigs.
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.keyboard-languages.defaults")
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    # Populate the archive here and let ESP-IDF register it as a component.
    FetchContent_Declare(lvgl
        URL https://codeload.github.com/lvgl/lvgl/tar.gz/refs/tags/v9.6.0
        URL_HASH SHA256=b20ee3acc1bba13c62d854f9ebd62e4c51e0b443b1e0225892e86442defa84df
        SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/lvgl"
        SOURCE_SUBDIR src)
    FetchContent_MakeAvailable(lvgl)
    list(APPEND EXTRA_COMPONENT_DIRS "${lvgl_SOURCE_DIR}" "${CMAKE_CURRENT_LIST_DIR}/Keyboard/AdvancedKeyboard" "${CMAKE_CURRENT_LIST_DIR}/Keyboard")
endif()

project(ESPOS)

if(ESPOS_NGRAM_MODEL)
    get_filename_component(ngram_model "${ESPOS_NGRAM_MODEL}" ABSOLUTE BASE_DIR "${PROJECT_DIR}")
    partition_table_get_partition_info(ngram_size "--partition-name ngram" "size")
    if(NOT ngram_size)
        message(FATAL_ERROR "ESPOS_NGRAM_MODEL requires the ngram partition in partitions.ngram.csv")
    endif()
    set(ngram_image "${CMAKE_BINARY_DIR}/ngram.bin")
    set(ngram_tools "${PROJECT_DIR}/tools")
    add_custom_command(OUTPUT "${ngram_image}"
        COMMAND "${PYTHON}" "${ngram_tools}/pack_ngram_partition.py" "${ngram_model}" "${ngram_image}" "${ngram_size}"
        DEPENDS "${ngram_model}" "${ngram_tools}/pack_ngram_partition.py" "${ngram_tools}/build_ngtr_index.py"
        VERBATIM)
    add_custom_target(espos_ngram_image ALL DEPENDS "${ngram_image}")
    esptool_py_flash_to_partition(flash ngram "${ngram_image}")
    add_dependencies(flash espos_ngram_image)
endif()
