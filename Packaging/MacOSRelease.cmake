# CMake installs a conventional bundle; PackageMacOS.py makes its dependencies
# relocatable, applies ad-hoc signatures and audits the final ZIP/TGZ payloads.
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
set(UICREATOR_MACOS_APP_NAME "LVGL UI Creator.app")
set(UICREATOR_MACOS_CONTENTS "${UICREATOR_MACOS_APP_NAME}/Contents")
set(UICREATOR_MACOS_SDK_DIRECTORY "${PROJECT_BINARY_DIR}/packages" CACHE PATH "Published SDK archives to include in the macOS application")
option(UICREATOR_MACOS_REQUIRE_SDKS "Require the source SDK archives when packaging macOS" OFF)
if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(UICREATOR_MACOS_ARCH arm64)
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(UICREATOR_MACOS_ARCH x86_64)
else()
    message(FATAL_ERROR "Configure a separate x86_64 or arm64 macOS build before packaging")
endif()
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.5")
endif()
foreach(target UICreator LVGLCompiler UICreatorCodeGen UICreatorViewer UICreatorModelGen)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "macOS app packaging requires ${target}")
    endif()
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/../Frameworks")
    install(TARGETS ${target} RUNTIME DESTINATION "${UICREATOR_MACOS_CONTENTS}/MacOS")
endforeach()
foreach(target LVGLProtobufMacOS SDL3-shared freetype)
    if(TARGET ${target})
        get_target_property(target_type ${target} TYPE)
        if(target_type STREQUAL "SHARED_LIBRARY")
            set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path" INSTALL_NAME_DIR "@rpath")
            install(TARGETS ${target} LIBRARY DESTINATION "${UICREATOR_MACOS_CONTENTS}/Frameworks")
        endif()
    endif()
endforeach()
foreach(helper font-converter image-converter project-creator language-models python-runtime integrations)
    install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${helper}"
        DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources" USE_SOURCE_PERMISSIONS)
endforeach()
foreach(pair "esp32|LVGLProtobuf-ESP32-Toolkit.zip" "arduino|LVGLProtobuf-Arduino-Toolkit.zip"
             "pico|LVGLProtobuf-Pico-Toolkit.zip" "linux|LVGLProtobuf-Linux-Toolkit.zip"
             "linux|LVGLProtobuf-Linux-Keyboard-Profile.zip" "linux|LVGLProtobuf-Linux-x86_64.tar.gz"
             "linux|LVGLProtobuf-Linux-aarch64-keyboard.tar.gz" "windows|LVGLProtobufDisplay-windows.zip")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 platform)
    list(GET fields 1 archive)
    if(UICREATOR_MACOS_REQUIRE_SDKS)
        if(NOT EXISTS "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}" OR
           NOT EXISTS "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}.sha256")
            message(FATAL_ERROR "Missing macOS package SDK input/checksum ${archive}")
        endif()
        file(READ "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}.sha256" sdk_checksum)
        string(REGEX MATCH "^[0-9a-fA-F]+" sdk_expected "${sdk_checksum}")
        string(LENGTH "${sdk_expected}" sdk_expected_length)
        string(TOLOWER "${sdk_expected}" sdk_expected)
        file(SHA256 "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}" sdk_actual)
        if(NOT sdk_expected_length EQUAL 64 OR NOT sdk_actual STREQUAL sdk_expected)
            message(FATAL_ERROR "macOS package SDK checksum differs: ${archive}")
        endif()
    endif()
    install(FILES "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}" "${UICREATOR_MACOS_SDK_DIRECTORY}/${archive}.sha256"
        DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/sdk/${platform}" OPTIONAL)
endforeach()
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/Packaging/THIRD_PARTY_NOTICES.md"
    DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses")
if(UICREATOR_FONT_NODE)
    get_filename_component(node_directory "${UICREATOR_FONT_NODE}" DIRECTORY)
    foreach(notice "${node_directory}/LICENSE" "${node_directory}/../LICENSE")
        if(EXISTS "${notice}")
            install(FILES "${notice}" DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/Node" RENAME LICENSE)
            break()
        endif()
    endforeach()
endif()
foreach(dependency SDL3 freetype-2.10.1 spdlog-1.x Breakpad Catch2 libimagequant nanopb MXML)
    foreach(notice LICENSE LICENSE.txt LICENSE.TXT COPYING COPYING.txt COPYRIGHT README.md docs/FTL.TXT docs/GPLv2.TXT)
        if(EXISTS "${PROJECT_SOURCE_DIR}/3rdParty/${dependency}/${notice}")
            install(FILES "${PROJECT_SOURCE_DIR}/3rdParty/${dependency}/${notice}"
                DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/${dependency}")
        endif()
    endforeach()
endforeach()
install(FILES "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl/LICENCE.txt"
    DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/LVGL" OPTIONAL)
install(FILES "${PROJECT_SOURCE_DIR}/3rdParty/JSON/json.hpp"
    DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/JSON")
install(FILES "${PROJECT_SOURCE_DIR}/ImgLib/stb_image.h" "${PROJECT_SOURCE_DIR}/ImgLib/stb.h"
    DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/stb")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/LVSerial/Widgets/AdvancedKeyboard/"
    DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/licenses/AdvancedKeyboard"
    FILES_MATCHING PATTERN "*LICENSE*" PATTERN "README.md" PATTERN "PROVENANCE.md")
foreach(document BUILD.md EDITOR_WORKFLOWS.md Packaging/RELEASE_NOTES.md Packaging/MACOS.md Docs/CI.md
        Runtime/PERFORMANCE.md Runtime/INTERACTIONS.md Runtime/pico/README.md CodeGeneration/PERFORMANCE.md
        CodeGeneration/MicroPython/README.md CodeGeneration/XML.md LVSerial/Serialization/Xml.md
        Integrations/Figma/README.md UICreator/README.md UICreator/SESSION.md UICreator/AdvancedKeyboard.md
        UICreator/Bootstrap.md UICreator/Codes.md Docs/Themes-and-components.md Docs/Responsive-previews.md
        Docs/Professional-workflows.md Runtime/README.md Runtime/linux/README.md Runtime/arduino/README.md
        Runtime/esp32/README.md Runtime/windows/README.md UICreator/ProjectCreator/README.md
        UICreator/Devices/README.md UICreator/Validation/README.md Tests/README.md)
    get_filename_component(directory "${document}" DIRECTORY)
    install(FILES "${PROJECT_SOURCE_DIR}/${document}" DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources/docs/${directory}" OPTIONAL)
endforeach()
install(FILES "${PROJECT_SOURCE_DIR}/Packaging/UICreator.png" DESTINATION "${UICREATOR_MACOS_CONTENTS}/Resources")
foreach(variable PROJECT_SOURCE_DIR PROJECT_BINARY_DIR CMAKE_RUNTIME_OUTPUT_DIRECTORY CMAKE_OSX_DEPLOYMENT_TARGET)
    string(REPLACE "\\" "\\\\" ${variable}_json "${${variable}}")
    string(REPLACE "\"" "\\\"" ${variable}_json "${${variable}_json}")
endforeach()
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/macos-package.json" CONTENT
    "{\n  \"schema\": \"lvgl-ui-creator-macos-package\",\n  \"version\": \"${PROJECT_VERSION}\",\n  \"architecture\": \"${UICREATOR_MACOS_ARCH}\",\n  \"minimumMacOS\": \"${CMAKE_OSX_DEPLOYMENT_TARGET_json}\",\n  \"source\": \"${PROJECT_SOURCE_DIR_json}\",\n  \"binary\": \"${PROJECT_BINARY_DIR_json}\",\n  \"runtime\": \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_json}\"\n}\n")
add_custom_target(UICreatorMacOSPackages
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/PackageMacOS.py"
        --build-directory "${PROJECT_BINARY_DIR}" --architecture "${UICREATOR_MACOS_ARCH}"
        --cmake "${CMAKE_COMMAND}"
    DEPENDS UICreator LVGLCompiler UICreatorCodeGen UICreatorViewer UICreatorModelGen
    USES_TERMINAL VERBATIM)
