cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_SOURCE_DIR}/Packaging/WindowsArchitectures.cmake")
project(LVGL_UI_Creator VERSION 0.9.2 LANGUAGES C CXX)
uicreator_validate_windows_architecture()
if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

include(CTest)
option(UICREATOR_USE_LVGL9 "Build the editor with the vendored LVGL 9 release" ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Directory for editor executables and shared libraries")

# Bundled libraries written for older CMake releases still use these policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library")
set(SDL_STATIC OFF CACHE BOOL "Build SDL as a static library")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL test library")
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests")
add_subdirectory(3rdParty/SDL3 EXCLUDE_FROM_ALL)
if(NOT UICREATOR_USE_LVGL9)
    message(FATAL_ERROR "The editor requires LVGL 9.6 and native SDL3. Older LVGL targets are supported by the compiler/runtime backends.")
endif()

add_subdirectory(3rdParty/LVGL EXCLUDE_FROM_ALL)
add_subdirectory(3rdParty/freetype-2.10.1 EXCLUDE_FROM_ALL)
add_subdirectory(3rdParty/libimagequant EXCLUDE_FROM_ALL)
add_subdirectory(3rdParty/spdlog-1.x EXCLUDE_FROM_ALL)

add_subdirectory(ImgLib)
add_subdirectory(LVSerial)
add_subdirectory(CodeGeneration)
add_subdirectory(Runtime)
if(WIN32)
    add_subdirectory(Runtime/windows)
elseif(APPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/UICreator/Drivers/MacOSDesktop.cmake")
elseif(UNIX AND NOT APPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/UICreator/Drivers/LinuxDesktop.cmake")
endif()
add_subdirectory(UICreator)

if(BUILD_TESTING)
    add_subdirectory(Tests/Integration/ESP32Prediction)
    add_library(UICreatorCatch STATIC 3rdParty/Catch2/extras/catch_amalgamated.cpp)
    target_compile_definitions(UICreatorCatch PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN)
    target_include_directories(UICreatorCatch PUBLIC
        "${PROJECT_SOURCE_DIR}/3rdParty/Catch2/single_include/catch2")
    if(MSVC AND UICREATOR_TEST_LVGL_VERSIONS)
        # Backend fixtures import Release LVGL archives. Give their independent
        # Release projects a matching Catch ABI even when the editor is Debug.
        add_library(UICreatorCatchMatrix STATIC 3rdParty/Catch2/extras/catch_amalgamated.cpp)
        set_target_properties(UICreatorCatchMatrix PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        target_compile_definitions(UICreatorCatchMatrix PRIVATE
            CATCH_AMALGAMATED_CUSTOM_MAIN NDEBUG _ITERATOR_DEBUG_LEVEL=0)
        target_include_directories(UICreatorCatchMatrix PUBLIC
            "${PROJECT_SOURCE_DIR}/3rdParty/Catch2/single_include/catch2")
    endif()
    add_subdirectory(Tests/UnitTests/ImgLib)
    add_subdirectory(Tests/UnitTests/Serialization)
    add_subdirectory(Tests/Integration/LVGL9)
    add_executable(LVSerialDeviceTests Tests/Integration/DeviceCodec.c)
    target_link_libraries(LVSerialDeviceTests PRIVATE LVSerialDevice)
    add_test(NAME LVSerialDeviceCodec COMMAND LVSerialDeviceTests)
    find_program(UICREATOR_PROTOC protoc)
    # FindPython validates the interpreter; a PATH lookup alone can select the
    # non-executable Windows Store alias instead of the Python used to build.
    find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(UICREATOR_REGRESSION_PYTHON "${Python3_EXECUTABLE}")
    else()
        find_program(UICREATOR_REGRESSION_PYTHON NAMES python3 python)
    endif()
    if(WIN32)
        add_test(NAME UICreatorCMakeToolSelection COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/Tests/Integration/Packaging/CMakeTools.ps1" -CMakeExecutable "${CMAKE_COMMAND}")
        set_tests_properties(UICreatorCMakeToolSelection PROPERTIES LABELS "packaging;cmake" TIMEOUT 30)
        add_test(NAME UICreatorPeArchitecture COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${PROJECT_SOURCE_DIR}/Tests/Integration/Packaging/PeArchitecture.ps1")
        set_tests_properties(UICreatorPeArchitecture PROPERTIES LABELS "packaging;architecture" TIMEOUT 60)
    endif()
    if(UICREATOR_REGRESSION_PYTHON)
        add_test(NAME UICreatorMacOSCrossFrontend COMMAND "${UICREATOR_REGRESSION_PYTHON}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/Tests/Integration/Packaging" -p test_macos_cross.py)
        add_test(NAME UICreatorMacOSToolBootstrap COMMAND "${UICREATOR_REGRESSION_PYTHON}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/Tests/UnitTests/Packaging" -p test_macos_tools.py)
        add_test(NAME UICreatorMacOSPackaging COMMAND "${UICREATOR_REGRESSION_PYTHON}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/Tests/UnitTests/Packaging" -p test_macos_packaging.py)
        set_tests_properties(UICreatorMacOSCrossFrontend UICreatorMacOSToolBootstrap UICreatorMacOSPackaging PROPERTIES
            LABELS "packaging;macos;cross-compile" TIMEOUT 90)
        add_test(NAME UICreatorVisualRegressionTools COMMAND "${UICREATOR_REGRESSION_PYTHON}" -m unittest discover -s "${PROJECT_SOURCE_DIR}/Tests/Visual" -p test_regression.py)
        set_tests_properties(UICreatorVisualRegressionTools PROPERTIES LABELS "polish;visual" TIMEOUT 30)
        if(WIN32)
            add_test(NAME UICreatorVisualRegression COMMAND "${UICREATOR_REGRESSION_PYTHON}" "${PROJECT_SOURCE_DIR}/Tests/Visual/run.py" --editor $<TARGET_FILE:UICreatorEditorTests> --baseline "${PROJECT_SOURCE_DIR}/Tests/Visual/baselines/windows" --output "${CMAKE_BINARY_DIR}/visual-regression")
            set_tests_properties(UICreatorVisualRegression PROPERTIES LABELS "polish;visual;performance" TIMEOUT 180 RUN_SERIAL TRUE)
        endif()
    endif()
    if(UICREATOR_PROTOC)
        add_test(NAME LVSerialProtobufConformance COMMAND "${CMAKE_COMMAND}"
            "-DDEVICE_TEST=$<TARGET_FILE:LVSerialDeviceTests>" "-DPROTOC=${UICREATOR_PROTOC}"
            "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DFIXTURE=${CMAKE_BINARY_DIR}/device-codec.pb"
            -P "${PROJECT_SOURCE_DIR}/Tests/Integration/ProtobufConformance.cmake")
    endif()
endif()

include("${PROJECT_SOURCE_DIR}/Packaging/Release.cmake")
