find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(UICREATOR_PREBUILT_IMAGE_TOOL_DIR "" CACHE PATH "Prepared target-native image-converter assets")
set(UICREATOR_LINUX_PYTHON_DIR "" CACHE PATH "Target-native relocatable Python runtime for Linux helpers")
if(UICREATOR_PREBUILT_IMAGE_TOOL_DIR)
    set(image_dist "${UICREATOR_PREBUILT_IMAGE_TOOL_DIR}")
    set(image_runtime "${image_dist}/UICreatorImageTool${CMAKE_EXECUTABLE_SUFFIX}")
    if(NOT EXISTS "${image_runtime}")
        message(FATAL_ERROR "Prepared image converter is missing: ${image_runtime}")
    endif()
else()
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Cross builds require UICREATOR_PREBUILT_IMAGE_TOOL_DIR with target-native helpers")
endif()
set(UICREATOR_IMAGE_PYTHON "${PROJECT_SOURCE_DIR}/build/image-tools-env/Scripts/python.exe" CACHE FILEPATH "Private image-converter packaging Python")
if(NOT EXISTS "${UICREATOR_IMAGE_PYTHON}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -m venv "${PROJECT_SOURCE_DIR}/build/image-tools-env" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not create image converter environment")
    endif()
    if(NOT WIN32)
        set(UICREATOR_IMAGE_PYTHON "${PROJECT_SOURCE_DIR}/build/image-tools-env/bin/python" CACHE FILEPATH "" FORCE)
    endif()
endif()
endif()
if(BUILD_TESTING)
    add_executable(UICreatorAssetLibraryTests "${PROJECT_SOURCE_DIR}/Tests/Integration/AssetLibrary.cpp")
    target_link_libraries(UICreatorAssetLibraryTests PRIVATE UICreatorImageConversion UICreatorCodeGeneration)
    add_dependencies(UICreatorAssetLibraryTests UICreatorImageConverterAssets)
    add_test(NAME UICreatorAssetLibrary COMMAND UICreatorAssetLibraryTests "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/test-output")
    set_tests_properties(UICreatorAssetLibrary PROPERTIES TIMEOUT 120)
    set(asset_fixture "${CMAKE_CURRENT_BINARY_DIR}/generated-test")
    add_custom_command(OUTPUT "${asset_fixture}/ui.c" "${asset_fixture}/ui_assets.c" "${asset_fixture}/ui.h"
        COMMAND UICreatorAssetLibraryTests --fixture "${PROJECT_SOURCE_DIR}" "${asset_fixture}"
        DEPENDS UICreatorAssetLibraryTests VERBATIM)
    add_executable(UICreatorAssetGeneratedTests "${PROJECT_SOURCE_DIR}/Tests/Integration/AssetGenerated.c" "${asset_fixture}/ui.c" "${asset_fixture}/ui_assets.c")
    target_link_libraries(UICreatorAssetGeneratedTests PRIVATE lvgl::lvgl)
    target_include_directories(UICreatorAssetGeneratedTests PRIVATE "${asset_fixture}")
    set_target_properties(UICreatorAssetGeneratedTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    add_test(NAME UICreatorAssetGeneratedC COMMAND UICreatorAssetGeneratedTests)
endif()
if(NOT UICREATOR_PREBUILT_IMAGE_TOOL_DIR)
execute_process(COMMAND "${UICREATOR_IMAGE_PYTHON}" -c "import PIL,png,lz4,resvg_py,PyInstaller" RESULT_VARIABLE image_imports OUTPUT_QUIET ERROR_QUIET)
if(NOT image_imports EQUAL 0)
    execute_process(COMMAND "${UICREATOR_IMAGE_PYTHON}" -m pip install -r "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter/requirements.txt" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not install image converter dependencies")
    endif()
endif()
set(image_dist "${PROJECT_SOURCE_DIR}/build/image-tool-dist/UICreatorImageTool")
set(image_runtime "${image_dist}/UICreatorImageTool${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(OUTPUT "${image_runtime}"
    COMMAND "${UICREATOR_IMAGE_PYTHON}" -m PyInstaller --noconfirm --clean --onedir --name UICreatorImageTool
        --paths "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter" --collect-all resvg_py --collect-all lz4
        --distpath "${PROJECT_SOURCE_DIR}/build/image-tool-dist" --workpath "${PROJECT_SOURCE_DIR}/build/image-tool-work"
        --specpath "${PROJECT_SOURCE_DIR}/build" "${CMAKE_CURRENT_SOURCE_DIR}/image-converter-bridge.py"
    DEPENDS image-converter-bridge.py "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter/LVGLImage.py"
    VERBATIM)
endif()
if(UICREATOR_WINDOWS_HELPERS_X86)
add_custom_target(UICreatorImageConverterAssets
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/StageWindowsHelper.py"
        "${image_dist}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/image-converter"
        --upstream "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter"
    DEPENDS "${image_runtime}" VERBATIM)
else()
add_custom_target(UICreatorImageConverterAssets
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/copy-converter-assets.py"
        "${image_dist}" "${PROJECT_SOURCE_DIR}/3rdParty/lvgl_image_converter" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/image-converter"
    DEPENDS "${image_runtime}" VERBATIM)
endif()
if(UICREATOR_LINUX_PYTHON_DIR)
    if(NOT EXISTS "${UICREATOR_LINUX_PYTHON_DIR}/bin/python3")
        message(FATAL_ERROR "Prepared Linux Python runtime is missing")
    endif()
    add_custom_command(TARGET UICreatorImageConverterAssets POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${UICREATOR_LINUX_PYTHON_DIR}"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python-runtime" VERBATIM)
endif()
add_library(UICreatorImageConversion STATIC ImageConverter.cpp)
target_link_libraries(UICreatorImageConversion PUBLIC UICreatorFontConversion LVSerial)
target_include_directories(UICreatorImageConversion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(UICreatorImageConversion PRIVATE /utf-8)
endif()
