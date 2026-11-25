cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_LIST_DIR}/tools/advanced_keyboard_dependency.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(NOT DEFINED ESPOS_CONFIG)
    set(ESPOS_CONFIG "${CMAKE_CURRENT_LIST_DIR}/espos.yaml")
endif()
get_filename_component(ESPOS_CONFIG "${ESPOS_CONFIG}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(ESPOS_CONFIG "${ESPOS_CONFIG}" CACHE FILEPATH "ESPOS YAML configuration" FORCE)
set(ESPOS_NGRAM_MODEL "" CACHE FILEPATH "Compact NGTR model to flash into the ngram partition")
set(ESPOS_NGRAM_BENCH_CASES "" CACHE FILEPATH "GBUTest reference header for the device benchmark")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${ESPOS_CONFIG}" "${CMAKE_CURRENT_LIST_DIR}/tools/python/espos_config.py")
execute_process(
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/tools/python/espos_config.py"
            generate "${ESPOS_CONFIG}" --output-dir "${CMAKE_BINARY_DIR}/generated"
    RESULT_VARIABLE espos_config_result
    ERROR_VARIABLE espos_config_error)
if(NOT espos_config_result EQUAL 0)
    message(FATAL_ERROR "ESPOS configuration failed\n${espos_config_error}")
endif()

include("${CMAKE_BINARY_DIR}/generated/espos_components.cmake")
if(ESPOS_PROFILE STREQUAL "full" OR ESPOS_USE_LVGL)
    espos_require_advanced_keyboard()
endif()

# Ship the ESP-IDF-compatible kernel as part of ESPOS. Registering the component
# with the same name overrides IDF's copy; only one FreeRTOS kernel is linked.
set(espos_freertos "${CMAKE_CURRENT_LIST_DIR}/third_party/freertos")
if(NOT EXISTS "${espos_freertos}/CMakeLists.txt")
    message(FATAL_ERROR "The bundled ESP-OS kernel component is missing: ${espos_freertos}")
endif()
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
    message(FATAL_ERROR "The bundled ESP-OS kernel port requires ESP-IDF 6.1.x. See third_party/freertos/README.ESPOS.md.")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${espos_freertos}")

# Keep the networking baseline beside the kernel. These retain their upstream
# sibling layout because Wi-Fi and WPA share private headers with each other.
set(espos_wifi_root "${CMAKE_CURRENT_LIST_DIR}/third_party/wifi/components")
set(espos_wifi_components esp_wifi wpa_supplicant lwip esp_netif esp_netif_stack
    esp_event esp_http_server nvs_flash esp_phy esp_coex)
if(ESPOS_USE_WIFI)
    foreach(component IN LISTS espos_wifi_components)
        if(NOT EXISTS "${espos_wifi_root}/${component}/CMakeLists.txt")
            message(FATAL_ERROR "The bundled Wi-Fi component is missing: ${espos_wifi_root}/${component}")
        endif()
        list(APPEND EXTRA_COMPONENT_DIRS "${espos_wifi_root}/${component}")
    endforeach()
endif()

# Each optional library is registered only when selected by the YAML. The
# MQTT Agent's dependency on coreMQTT is resolved by the configuration generator.
set(espos_core_libraries_root "${CMAKE_CURRENT_LIST_DIR}/third_party/freertos_libraries/components")
set(espos_core_library_components)
foreach(component IN ITEMS core_mqtt core_mqtt_agent core_sntp core_json core_pkcs11)
    string(TOUPPER "${component}" option)
    if(ESPOS_USE_${option})
        if(NOT EXISTS "${espos_core_libraries_root}/${component}/CMakeLists.txt")
            message(FATAL_ERROR "The selected bundled library is missing: ${component}")
        endif()
        list(APPEND EXTRA_COMPONENT_DIRS "${espos_core_libraries_root}/${component}")
        list(APPEND espos_core_library_components "${component}")
    endif()
endforeach()

if(ESPOS_USE_TRACE)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/components/espos_trace/CMakeLists.txt")
        message(FATAL_ERROR "The selected ESPOS trace recorder component is missing")
    endif()
    list(APPEND espos_core_library_components espos_trace)
endif()

if(ESPOS_PROFILE STREQUAL "minimal" OR NOT ESPOS_USE_WIFI)
    # Full builds with Wi-Fi disabled must also follow main's explicit dependency
    # closure, instead of main implicitly depending on every SDK component.
    idf_build_set_property(MINIMAL_BUILD ON)
endif()

if(ESPOS_PROFILE STREQUAL "minimal")
    # main declares only FreeRTOS/log/system dependencies in this profile.
    # IDF then builds their dependency closure instead of every project component.
    foreach(component IN LISTS COMPONENTS)
        if(NOT component STREQUAL "main" AND NOT component IN_LIST espos_core_library_components
                AND NOT (ESPOS_PLATFORMIO AND component STREQUAL "__pio_env"))
            message(FATAL_ERROR "The minimal profile supports only main, selected optional libraries and their required IDF dependencies")
        endif()
    endforeach()
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.freertos-minimal.defaults")
    if(ESPOS_NGRAM_MODEL OR ESPOS_NGRAM_BENCH_CASES)
        message(FATAL_ERROR "NGTR model/benchmark options require espos.profile: full")
    endif()
endif()

# Trace settings follow the minimal defaults, which disable trace support.
# Project Creator regeneration backs up old SDK configuration before changing
# these settings. The normal disabled build retains the SDK's own defaults.
if(ESPOS_USE_TRACE)
    set(espos_trace_defaults "${CMAKE_BINARY_DIR}/generated/sdkconfig.trace.defaults")
    set(espos_trace_stream_value n)
    if(ESPOS_TRACE_STREAM)
        set(espos_trace_stream_value y)
    endif()
    file(WRITE "${espos_trace_defaults}"
        "CONFIG_ESPOS_TRACE_ENABLE=y\nCONFIG_ESPOS_TRACE_BUFFER_EVENTS=${ESPOS_TRACE_BUFFER_EVENTS}\nCONFIG_ESPOS_TRACE_STREAM=${espos_trace_stream_value}\nCONFIG_ESP_TRACE_ENABLE=y\nCONFIG_ESP_TRACE_LIB_EXTERNAL=y\nCONFIG_ESP_TRACE_TRANSPORT_NONE=y\nCONFIG_ESP_TRACE_TS_SOURCE_ESP_TIMER=y\nCONFIG_FREERTOS_USE_TRACE_FACILITY=y\n")
    list(APPEND SDKCONFIG_DEFAULTS "${espos_trace_defaults}")
endif()

# Explicit roots keep optional libraries in minimal dependency builds, including
# older generated projects whose customized main/CMakeLists.txt is preserved.
# ESPOS_PLATFORMIO_COMPONENT_ROOTS v1
# PlatformIO needs its C/C++/ASM environment component in the CMake codemodel.
# An explicit component list bypasses its patched default minimal roots.
if((espos_core_library_components OR ESPOS_PLATFORMIO)
        AND (ESPOS_PROFILE STREQUAL "minimal" OR NOT ESPOS_USE_WIFI OR COMPONENTS))
    list(APPEND COMPONENTS main ${espos_core_library_components})
    if(ESPOS_PLATFORMIO)
        list(APPEND COMPONENTS __pio_env)
    endif()
    list(REMOVE_DUPLICATES COMPONENTS)
endif()

if(ESPOS_USE_LVGL)
    # Apply script glyphs and logical Unicode bidi to fresh keyboard sdkconfigs.
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.keyboard-languages.defaults")
    # ESPOS_GENERATED_UI_REQUIREMENTS v1
    # The editor exports explicit LVGL feature requirements beside its code.
    # Apply them after board/language defaults when creating a fresh sdkconfig.
    set(espos_generated_ui "${CMAKE_CURRENT_LIST_DIR}/components/GeneratedUI")
    if(EXISTS "${espos_generated_ui}/espos-ui.json")
        if(NOT EXISTS "${espos_generated_ui}/espos_ui_sdkconfig.defaults")
            message(FATAL_ERROR "The sent UI has no LVGL SDK defaults. Send it again from LVGL UI Creator.")
        endif()
        list(APPEND SDKCONFIG_DEFAULTS "${espos_generated_ui}/espos_ui_sdkconfig.defaults")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${espos_generated_ui}/espos-ui.json" "${espos_generated_ui}/espos_ui_sdkconfig.defaults")
    endif()
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

# ESPOS_OPTIONAL_CORE_LIBRARIES v1
# Regeneration updates this managed root file without replacing user main code.
if(espos_core_library_components)
    idf_component_get_property(espos_main_component main COMPONENT_LIB)
    foreach(component IN LISTS espos_core_library_components)
        target_link_libraries(${espos_main_component} PRIVATE idf::${component})
    endforeach()
endif()

idf_component_get_property(espos_kernel_dir freertos COMPONENT_DIR)
if(NOT "${espos_kernel_dir}" STREQUAL "${espos_freertos}")
    message(FATAL_ERROR "ESP-IDF selected an unexpected ESP-OS kernel component: ${espos_kernel_dir}")
endif()
message(STATUS "ESP-OS profile: ${ESPOS_PROFILE}; bundled ESP-OS kernel: ${espos_kernel_dir}")
if(ESPOS_USE_WIFI)
    idf_build_get_property(espos_built_components BUILD_COMPONENTS)
    foreach(component IN LISTS espos_wifi_components)
        if(component IN_LIST espos_built_components)
            idf_component_get_property(espos_network_dir ${component} COMPONENT_DIR)
            if(NOT "${espos_network_dir}" STREQUAL "${espos_wifi_root}/${component}")
                message(FATAL_ERROR "ESP-IDF selected an unexpected ${component} component: ${espos_network_dir}")
            endif()
        endif()
    endforeach()
    message(STATUS "ESPOS bundled Wi-Fi: ${espos_wifi_root}")
endif()

if(ESPOS_NGRAM_MODEL)
    get_filename_component(ngram_model "${ESPOS_NGRAM_MODEL}" ABSOLUTE BASE_DIR "${PROJECT_DIR}")
    partition_table_get_partition_info(ngram_size "--partition-name ngram" "size")
    if(NOT ngram_size)
        message(FATAL_ERROR "ESPOS_NGRAM_MODEL requires the ngram partition in partitions.ngram.csv")
    endif()
    set(ngram_image "${CMAKE_BINARY_DIR}/ngram.bin")
    set(ngram_tools "${PROJECT_DIR}/tools/python")
    add_custom_command(OUTPUT "${ngram_image}"
        COMMAND "${PYTHON}" "${ngram_tools}/pack_ngram_partition.py" "${ngram_model}" "${ngram_image}" "${ngram_size}"
        DEPENDS "${ngram_model}" "${ngram_tools}/pack_ngram_partition.py" "${ngram_tools}/build_ngtr_index.py"
        VERBATIM)
    add_custom_target(espos_ngram_image ALL DEPENDS "${ngram_image}")
    esptool_py_flash_to_partition(flash ngram "${ngram_image}")
    add_dependencies(flash espos_ngram_image)
endif()
