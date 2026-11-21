if(NOT WIN32)
    return()
endif()
add_library(LVGLProtobufDisplay SHARED Display.cpp)
target_compile_features(LVGLProtobufDisplay PRIVATE cxx_std_17)
target_compile_definitions(LVGLProtobufDisplay PRIVATE LVGL_PB_DISPLAY_BUILD)
target_include_directories(LVGLProtobufDisplay PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(LVGLProtobufDisplay PRIVATE LVGLRuntime::Runtime SDL3::SDL3)
set_target_properties(LVGLProtobufDisplay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_executable(UICreatorViewer Viewer.cpp)
target_link_libraries(UICreatorViewer PRIVATE LVGLProtobufDisplay)
if(MSVC)
    target_compile_options(LVGLProtobufDisplay PRIVATE /utf-8 /Z7)
    target_link_options(LVGLProtobufDisplay PRIVATE /DEBUG:FULL "/MAP:${PROJECT_BINARY_DIR}/windows-display.map")
    target_compile_options(UICreatorViewer PRIVATE /utf-8)
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_custom_target(LVGLProtobufWindowsSDK
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_sdk.py"
        --dll "$<TARGET_FILE:LVGLProtobufDisplay>" --import-library "$<TARGET_LINKER_FILE:LVGLProtobufDisplay>"
        --sdl "$<TARGET_FILE:SDL3::SDL3>" --compiler "${CMAKE_CXX_COMPILER}"
        --viewer "$<TARGET_FILE:UICreatorViewer>"
        --output "${PROJECT_BINARY_DIR}/packages/LVGLProtobufDisplay-windows" --replace
    DEPENDS LVGLProtobufDisplay UICreatorViewer VERBATIM)
if(BUILD_TESTING)
    add_executable(LVGLProtobufDisplayTests "${PROJECT_SOURCE_DIR}/Tests/Integration/WindowsDisplay/Display.c")
    target_link_libraries(LVGLProtobufDisplayTests PRIVATE LVGLProtobufDisplay)
    set_target_properties(LVGLProtobufDisplayTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    add_test(NAME LVGLProtobufWindows COMMAND LVGLProtobufDisplayTests "${PROJECT_BINARY_DIR}/CodeGeneration/generated_test/fixture.pb")
    set_tests_properties(LVGLProtobufWindows PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;windows;protobuf")
    add_test(NAME LVGLProtobufViewer COMMAND UICreatorViewer "${PROJECT_BINARY_DIR}/CodeGeneration/generated_test/fixture.pb" --frames 20)
    set_tests_properties(LVGLProtobufViewer PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;windows;protobuf")
    add_executable(LVGLProtobufActiveScreenTests "${PROJECT_SOURCE_DIR}/Tests/Integration/WindowsDisplay/ActiveScreen.cpp")
    target_link_libraries(LVGLProtobufActiveScreenTests PRIVATE LVGLProtobufDisplay LVSerial)
    add_test(NAME LVGLProtobufActiveScreen COMMAND "${CMAKE_COMMAND}"
        "-DTEST=$<TARGET_FILE:LVGLProtobufActiveScreenTests>" "-DVIEWER=$<TARGET_FILE:UICreatorViewer>"
        "-DOUTPUT_DIR=${PROJECT_BINARY_DIR}/windows-active-screen" "-DPYTHON=${Python3_EXECUTABLE}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/WindowsDisplay/VerifyActive.cmake")
    set_tests_properties(LVGLProtobufActiveScreen PROPERTIES TIMEOUT 30 LABELS "functional;windows;protobuf")
endif()
