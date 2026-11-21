file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Targets/Compat9.h" COMPAT9_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Targets/Compat8.h" COMPAT8_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Targets/Compat9.h Targets/Compat8.h)
configure_file(Targets/CompatSource.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/Targets/CompatSource.h" @ONLY)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE keyboard_resources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/LVSerial/Widgets/AdvancedKeyboard/*")
set(keyboard_source_header "${CMAKE_CURRENT_BINARY_DIR}/include/AdvancedKeyboardSource.h")
add_custom_command(OUTPUT "${keyboard_source_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/EmbedAdvancedKeyboard.py"
        "${PROJECT_SOURCE_DIR}/LVSerial/Widgets/AdvancedKeyboard" "${keyboard_source_header}"
    DEPENDS EmbedAdvancedKeyboard.py ${keyboard_resources} VERBATIM)
add_library(UICreatorCodeGeneration STATIC CodeGeneratorBase.cpp CCodeGenerator.cpp ProjectAssets.cpp Targets/Target.cpp Targets/Pixels.cpp "${keyboard_source_header}")
target_link_libraries(UICreatorCodeGeneration PUBLIC LVSerial)
target_include_directories(UICreatorCodeGeneration BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
target_include_directories(UICreatorCodeGeneration PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(UICreatorCodeGeneration PRIVATE /utf-8)
endif()
add_executable(UICreatorCodeGen Main.cpp)
target_link_libraries(UICreatorCodeGen PRIVATE UICreatorCodeGeneration)

add_executable(LVGLCompiler LVGLCompiler.cpp)
target_link_libraries(LVGLCompiler PRIVATE UICreatorCodeGeneration)
if(MSVC)
    target_compile_options(LVGLCompiler PRIVATE /utf-8)
endif()

if(BUILD_TESTING AND UICREATOR_USE_LVGL9)
    add_executable(UICreatorCodeGenFixture ../Tests/Integration/CodeGeneration/Fixture.cpp)
    target_link_libraries(UICreatorCodeGenFixture PRIVATE UICreatorCodeGeneration)
    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_test")
    add_custom_command(OUTPUT "${generated_dir}/ui.c" "${generated_dir}/ui_assets.c" "${generated_dir}/ui.h" "${generated_dir}/fixture_ids.h"
        COMMAND UICreatorCodeGenFixture "${generated_dir}" "${PROJECT_SOURCE_DIR}"
        DEPENDS UICreatorCodeGenFixture
        VERBATIM)
    add_executable(UICreatorGeneratedCTests ../Tests/Integration/CodeGeneration/Generated.c
        "${generated_dir}/ui.c" "${generated_dir}/ui_assets.c")
    target_include_directories(UICreatorGeneratedCTests PRIVATE "${generated_dir}")
    target_link_libraries(UICreatorGeneratedCTests PRIVATE lvgl::lvgl)
    set_target_properties(UICreatorGeneratedCTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Versions/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Interactions/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/ScreenFocus/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Variables/Register.cmake")
    add_test(NAME LVGLCompilerCLI COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=$<TARGET_FILE:LVGLCompiler>" "-DGENERATED_DIR=${generated_dir}" -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Compiler.cmake")
    add_test(NAME UICreatorGeneratedC COMMAND UICreatorGeneratedCTests)
    add_test(NAME UICreatorCodeGenJSON COMMAND UICreatorCodeGen "${generated_dir}/fixture.json" "${generated_dir}/from-json")
    add_test(NAME UICreatorCodeGenProtobuf COMMAND UICreatorCodeGen "${generated_dir}/fixture.pb" "${generated_dir}/from-protobuf")
    add_test(NAME UICreatorCodeGenDeterminism COMMAND "${CMAKE_COMMAND}" "-DGENERATED_DIR=${generated_dir}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Compare.cmake")
    set_tests_properties(UICreatorCodeGenDeterminism PROPERTIES DEPENDS "UICreatorCodeGenJSON;UICreatorCodeGenProtobuf")
endif()
