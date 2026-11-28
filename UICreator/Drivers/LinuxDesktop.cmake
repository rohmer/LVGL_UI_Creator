# Desktop builds already own LVGL and the runtime. Reuse those targets rather
# than configuring the standalone SDK's second copy of the same libraries.
add_library(LVGLProtobufLinux SHARED
    "${PROJECT_SOURCE_DIR}/Runtime/linux/Board.cpp" "${PROJECT_SOURCE_DIR}/Runtime/linux/Display.cpp")
target_compile_features(LVGLProtobufLinux PRIVATE cxx_std_17)
target_include_directories(LVGLProtobufLinux PUBLIC
    "${PROJECT_SOURCE_DIR}/Runtime/linux/include" "${PROJECT_SOURCE_DIR}/Runtime/windows/include")
target_link_libraries(LVGLProtobufLinux PRIVATE LVGLRuntime::Runtime SDL3::SDL3)
set_target_properties(LVGLProtobufLinux PROPERTIES OUTPUT_NAME LVGLProtobufDisplay
    VERSION 0.9.4 SOVERSION 0 LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
add_executable(UICreatorViewer "${PROJECT_SOURCE_DIR}/Runtime/linux/Viewer.cpp")
target_link_libraries(UICreatorViewer PRIVATE LVGLProtobufLinux)
if(BUILD_TESTING)
    add_executable(UICreatorVideoBackends "${PROJECT_SOURCE_DIR}/Tests/Integration/SDL3/Backends.c")
    target_link_libraries(UICreatorVideoBackends PRIVATE SDL3::SDL3)
    add_test(NAME UICreatorVideoBackends COMMAND UICreatorVideoBackends --require-x11)
    set_tests_properties(UICreatorVideoBackends PROPERTIES LABELS "functional;sdl3;linux" TIMEOUT 15)
endif()
