file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Targets/Compat9.h" COMPAT9_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Targets/Compat8.h" COMPAT8_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Targets/Compat9.h Targets/Compat8.h)
configure_file(Targets/CompatSource.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/Targets/CompatSource.h" @ONLY)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(advanced_controls_source "${PROJECT_SOURCE_DIR}/LVSerial/Serialization/AdvancedControlsRuntime.h")
set(advanced_controls_header "${CMAKE_CURRENT_BINARY_DIR}/include/AdvancedControlsSource.h")
add_custom_command(OUTPUT "${advanced_controls_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/EmbedAdvancedControls.py"
        "${advanced_controls_source}" "${advanced_controls_header}"
    DEPENDS EmbedAdvancedControls.py "${advanced_controls_source}" VERBATIM)
set(UICREATOR_MICROPYTHON_EXECUTABLE "" CACHE FILEPATH "MicroPython executable built with the official LVGL binding, for native export smoke tests")
set(UICREATOR_MICROPYTHON_WSL_DISTRIBUTION "" CACHE STRING "Optional WSL distribution used to run the LVGL-enabled MicroPython interpreter from Windows")
file(GLOB_RECURSE keyboard_resources CONFIGURE_DEPENDS "${ADVKBD_SOURCE_ROOT}/src/*" "${ADVKBD_SOURCE_ROOT}/include/*")
set(keyboard_source_header "${CMAKE_CURRENT_BINARY_DIR}/include/AdvancedKeyboardSource.h")
add_custom_command(OUTPUT "${keyboard_source_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/EmbedAdvancedKeyboard.py"
        "${ADVKBD_SOURCE_ROOT}" "${keyboard_source_header}"
    DEPENDS EmbedAdvancedKeyboard.py ${keyboard_resources}
        "${ADVKBD_SOURCE_ROOT}/LICENSE" "${ADVKBD_SOURCE_ROOT}/NOTICE.md"
        "${PROJECT_SOURCE_DIR}/advanced-keyboard.lock.json" VERBATIM)
file(GLOB micropython_resources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/MicroPython/*.py")
set(micropython_source_header "${CMAKE_CURRENT_BINARY_DIR}/include/MicroPythonSource.h")
add_custom_command(OUTPUT "${micropython_source_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/EmbedMicroPython.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/MicroPython" "${micropython_source_header}"
    DEPENDS EmbedMicroPython.py ${micropython_resources} VERBATIM)
add_library(UICreatorCodeGeneration STATIC CodeGeneratorBase.cpp CCodeGenerator.cpp MicroPythonCodeGenerator.cpp XmlCodeGenerator.cpp FigmaImporter.cpp ProjectAssets.cpp Targets/Target.cpp Targets/Pixels.cpp "${keyboard_source_header}" "${micropython_source_header}" "${advanced_controls_header}")
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
    add_test(NAME UICreatorMicroPythonRuntime
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Integration/MicroPython/test_runtime.py")
    set_tests_properties(UICreatorMicroPythonRuntime PROPERTIES LABELS "functional;micropython" TIMEOUT 60)
    add_test(NAME UICreatorMicroPythonAdvanced
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Integration/MicroPython/test_advanced.py")
    set_tests_properties(UICreatorMicroPythonAdvanced PROPERTIES LABELS "functional;micropython;advanced" TIMEOUT 60)
    add_test(NAME UICreatorMicroPythonExport
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/MicroPythonExport/test_export.py"
            --compiler "$<TARGET_FILE:LVGLCompiler>" --codegen "$<TARGET_FILE:UICreatorCodeGen>"
            --source-root "${PROJECT_SOURCE_DIR}" --output "${CMAKE_CURRENT_BINARY_DIR}/micropython-test")
    set_tests_properties(UICreatorMicroPythonExport PROPERTIES LABELS "functional;export;micropython" TIMEOUT 120
        FIXTURES_SETUP MicroPythonExport)
    foreach(micropython_case RoundTrip Assets Paths Rejections)
        string(TOLOWER "${micropython_case}" micropython_case_argument)
        add_test(NAME UICreatorMicroPython${micropython_case}
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/MicroPythonExport/test_regressions.py"
                --case "${micropython_case_argument}" --compiler "$<TARGET_FILE:LVGLCompiler>" --codegen "$<TARGET_FILE:UICreatorCodeGen>"
                --source-root "${PROJECT_SOURCE_DIR}" --output "${CMAKE_CURRENT_BINARY_DIR}/micropython-regressions/${micropython_case_argument}")
        set_tests_properties(UICreatorMicroPython${micropython_case} PROPERTIES LABELS "functional;export;micropython" TIMEOUT 120)
    endforeach()
    if(WIN32)
        add_test(NAME UICreatorMicroPythonInstalledCLI COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${PROJECT_SOURCE_DIR}/Tests/Integration/Packaging/InstalledMicroPython.ps1"
            -BinaryDirectory "$<TARGET_FILE_DIR:LVGLCompiler>"
            -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/micropython-installed-cli")
        set_tests_properties(UICreatorMicroPythonInstalledCLI PROPERTIES LABELS "functional;packaging;micropython" TIMEOUT 60)
    endif()
    if(UICREATOR_MICROPYTHON_EXECUTABLE)
        set(micropython_native_arguments)
        if(UICREATOR_MICROPYTHON_WSL_DISTRIBUTION)
            list(APPEND micropython_native_arguments --wsl-distribution "${UICREATOR_MICROPYTHON_WSL_DISTRIBUTION}")
        endif()
        add_test(NAME UICreatorMicroPythonNative
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/Integration/MicroPython/run_native.py"
                --interpreter "${UICREATOR_MICROPYTHON_EXECUTABLE}"
                --export "${CMAKE_CURRENT_BINARY_DIR}/micropython-test/generated"
                --report "${CMAKE_CURRENT_BINARY_DIR}/micropython-test/native-verification.json" ${micropython_native_arguments})
        set_tests_properties(UICreatorMicroPythonNative PROPERTIES
            FIXTURES_REQUIRED MicroPythonExport LABELS "functional;micropython;native" TIMEOUT 120)
    endif()
    add_executable(UICreatorCodeGenFixture ../Tests/Integration/CodeGeneration/Fixture.cpp)
    target_link_libraries(UICreatorCodeGenFixture PRIVATE UICreatorCodeGeneration)
    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_test")
    add_custom_command(OUTPUT "${generated_dir}/ui.c" "${generated_dir}/ui_assets.c" "${generated_dir}/ui.h" "${generated_dir}/fixture_ids.h"
        COMMAND UICreatorCodeGenFixture "${generated_dir}" "${PROJECT_SOURCE_DIR}"
        DEPENDS UICreatorCodeGenFixture
        VERBATIM)
    add_executable(UICreatorGeneratedCTests ../Tests/Integration/CodeGeneration/Generated.c
        "${generated_dir}/ui.c" "${generated_dir}/ui_assets.c"
        "${ADVKBD_SOURCE_ROOT}/src/TextareaInputMode.c")
    target_include_directories(UICreatorGeneratedCTests PRIVATE "${generated_dir}" "${ADVKBD_SOURCE_ROOT}/include")
    target_link_libraries(UICreatorGeneratedCTests PRIVATE lvgl::lvgl)
    if(MSVC)
        target_compile_options(UICreatorCodeGenFixture PRIVATE /utf-8)
        target_compile_options(UICreatorGeneratedCTests PRIVATE /utf-8)
    endif()
    set_target_properties(UICreatorGeneratedCTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Versions/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Interactions/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/ScreenFocus/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Variables/Register.cmake")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Memory/Register.cmake")
    add_test(NAME LVGLCompilerCLI COMMAND "${CMAKE_COMMAND}" "-DCOMPILER=$<TARGET_FILE:LVGLCompiler>" "-DGENERATED_DIR=${generated_dir}" -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Compiler.cmake")
    add_test(NAME UICreatorGeneratedC COMMAND UICreatorGeneratedCTests)
    add_test(NAME UICreatorCodeGenJSON COMMAND UICreatorCodeGen "${generated_dir}/fixture.json" "${generated_dir}/from-json")
    add_test(NAME UICreatorCodeGenProtobuf COMMAND UICreatorCodeGen "${generated_dir}/fixture.pb" "${generated_dir}/from-protobuf")
    add_test(NAME UICreatorCodeGenDeterminism COMMAND "${CMAKE_COMMAND}" "-DGENERATED_DIR=${generated_dir}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Compare.cmake")
    set_tests_properties(UICreatorCodeGenDeterminism PROPERTIES DEPENDS "UICreatorCodeGenJSON;UICreatorCodeGenProtobuf")
endif()
