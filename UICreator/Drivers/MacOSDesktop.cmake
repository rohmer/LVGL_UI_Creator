# Share the platform-neutral display API and SDL renderer with Linux without
# configuring a second LVGL/runtime build. macOS gets native dylib identities.
add_library(LVGLProtobufMacOS SHARED
    "${PROJECT_SOURCE_DIR}/Runtime/linux/Board.cpp"
    "${PROJECT_SOURCE_DIR}/Runtime/linux/Display.cpp")
target_compile_features(LVGLProtobufMacOS PRIVATE cxx_std_17)
target_include_directories(LVGLProtobufMacOS PUBLIC
    "${PROJECT_SOURCE_DIR}/Runtime/linux/include"
    "${PROJECT_SOURCE_DIR}/Runtime/windows/include")
target_link_libraries(LVGLProtobufMacOS PRIVATE LVGLRuntime::Runtime SDL3::SDL3)
set_target_properties(LVGLProtobufMacOS PROPERTIES
    OUTPUT_NAME lvgl_pb_display VERSION "${PROJECT_VERSION}" SOVERSION 0
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    MACOSX_RPATH ON INSTALL_NAME_DIR "@rpath" INSTALL_RPATH "@loader_path")
add_executable(UICreatorViewer "${PROJECT_SOURCE_DIR}/Runtime/linux/Viewer.cpp")
target_link_libraries(UICreatorViewer PRIVATE LVGLProtobufMacOS)
set_target_properties(UICreatorViewer PROPERTIES INSTALL_RPATH "@loader_path/../Frameworks")
