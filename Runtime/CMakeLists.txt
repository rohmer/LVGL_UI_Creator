cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LVGLRuntime LANGUAGES C CXX)
endif()
if(NOT CMAKE_CXX_COMPILER_LOADED)
    enable_language(CXX)
endif()
set(LVGL_RUNTIME_TARGET "9.6" CACHE STRING "LVGL runtime backend")
set_property(CACHE LVGL_RUNTIME_TARGET PROPERTY STRINGS "9.6" "9.5" "9.3" "9.2" "9.1" "8.3")
set(runtime_targets "9.6" "9.5" "9.3" "9.2" "9.1" "8.3")
if(NOT LVGL_RUNTIME_TARGET IN_LIST runtime_targets)
    message(FATAL_ERROR "Unknown LVGL_RUNTIME_TARGET: ${LVGL_RUNTIME_TARGET}")
endif()
string(REPLACE "." "_" runtime_suffix "${LVGL_RUNTIME_TARGET}")
set(runtime_library "LVGLRuntime_${runtime_suffix}")
if(NOT TARGET lvgl::lvgl)
    if(TARGET lvgl)
        add_library(lvgl::lvgl ALIAS lvgl)
    else()
        message(FATAL_ERROR "Add your selected LVGL target before add_subdirectory(Runtime)")
    endif()
endif()
if(NOT TARGET LVSerial)
    set(UICREATOR_USE_LVGL9 ON)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../LVSerial" "${CMAKE_CURRENT_BINARY_DIR}/lvserial")
    target_compile_definitions(LVSerial PUBLIC LVSERIAL_EMBEDDED_RUNTIME=1)
    if(NOT LVGL_RUNTIME_TARGET STREQUAL "9.6")
        target_compile_definitions(LVSerial PUBLIC LV_RUNTIME_PORTABLE=1)
    endif()
    target_include_directories(LVSerial BEFORE PUBLIC "${CMAKE_CURRENT_LIST_DIR}/compat")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/lvgl")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include/lvgl/lvgl.h" "#include <RuntimeCompat.h>\n")
    target_include_directories(LVSerial PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
endif()
add_library(${runtime_library} STATIC common/Runtime.cpp common/Validation.cpp)
if(NOT LVGL_RUNTIME_TARGET STREQUAL "9.6")
    target_sources(${runtime_library} PRIVATE ../CodeGeneration/Targets/Target.cpp ../CodeGeneration/Targets/Pixels.cpp)
endif()
add_library(LVGLRuntime::Runtime ALIAS ${runtime_library})
string(REPLACE "." ";" runtime_parts "${LVGL_RUNTIME_TARGET}")
list(GET runtime_parts 0 runtime_major)
list(GET runtime_parts 1 runtime_minor)
target_compile_definitions(${runtime_library} PRIVATE LVGL_RUNTIME_VERSION="${LVGL_RUNTIME_TARGET}" LVGL_RUNTIME_MAJOR=${runtime_major} LVGL_RUNTIME_MINOR=${runtime_minor})
target_include_directories(${runtime_library} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/compat")
target_compile_features(${runtime_library} PUBLIC cxx_std_17)
target_include_directories(${runtime_library} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(${runtime_library} PRIVATE LVSerial PUBLIC lvgl::lvgl)
if(MSVC)
    target_compile_options(${runtime_library} PRIVATE /utf-8 /Zi)
endif()
if(BUILD_TESTING AND TARGET UICreatorCodeGenFixture)
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/Xml/Register.cmake")
    add_executable(LVGLProjectUnitTests ../Tests/UnitTests/Runtime/Main.cpp ../Tests/UnitTests/Runtime/API.cpp ../Tests/UnitTests/Runtime/Targets.cpp ../Tests/UnitTests/Runtime/Interactions.cpp ../Tests/UnitTests/Runtime/LazyScreens.cpp ../Tests/UnitTests/Runtime/PerformanceMetrics.cpp ../Tests/UnitTests/Runtime/GeneratedScreenMetadata.cpp)
    if(MSVC)
        target_compile_options(LVGLProjectUnitTests PRIVATE /Zi)
        target_link_options(LVGLProjectUnitTests PRIVATE /DEBUG:FULL)
    endif()
    target_link_libraries(LVGLProjectUnitTests PRIVATE LVGLRuntime::Runtime LVSerial UICreatorCodeGeneration UICreatorCatch)
    target_compile_definitions(LVGLProjectUnitTests PRIVATE UICREATOR_UNIT_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/unit-output" UICREATOR_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    add_test(NAME LVGLRuntimeUnit COMMAND LVGLProjectUnitTests "[runtime]")
    add_test(NAME LVGLGeneratorUnit COMMAND LVGLProjectUnitTests "[targets]")
    set_tests_properties(LVGLRuntimeUnit LVGLGeneratorUnit PROPERTIES TIMEOUT 60)
    add_executable(LVGLRuntimeMediaTests ../Tests/Integration/Runtime/Media.cpp)
    target_link_libraries(LVGLRuntimeMediaTests PRIVATE LVGLRuntime::Runtime LVSerial UICreatorImageConversion UICreatorFontConversion)
    add_test(NAME LVGLRuntimeMedia COMMAND LVGLRuntimeMediaTests "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/media-output")
    set_tests_properties(LVGLRuntimeMedia PROPERTIES TIMEOUT 180 LABELS "functional;media")
    if(UICREATOR_TEST_LVGL_VERSIONS)
        include("${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Versions/Register.cmake")
        add_test(NAME LVGLRuntimeStandalone COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
            "-DBINARY_DIR=${PROJECT_BINARY_DIR}/runtime-standalone"
            "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6"
            "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
            "-DPROJECT_FILE=${PROJECT_BINARY_DIR}/UICreator/Images/generated-test/runtime.pb"
            -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Standalone/Verify.cmake")
        set_tests_properties(LVGLRuntimeStandalone PROPERTIES DEPENDS LVGLGenerator96 TIMEOUT 300 LABELS "functional;firmware")
    endif()
    add_executable(LVGLRuntimeTests ../Tests/Integration/Runtime/Runtime.cpp)
    target_link_libraries(LVGLRuntimeTests PRIVATE LVGLRuntime::Runtime LVSerial)
    add_dependencies(LVGLRuntimeTests UICreatorGeneratedCTests UICreatorAssetGeneratedTests)
    add_test(NAME LVGLRuntime96 COMMAND LVGLRuntimeTests "${PROJECT_BINARY_DIR}/CodeGeneration/generated_test/fixture.pb" "${PROJECT_BINARY_DIR}/UICreator/Images/generated-test/runtime.pb")
    add_executable(LVGLRuntimeCAPI ../Tests/Integration/Runtime/PublicAPI.c)
    target_link_libraries(LVGLRuntimeCAPI PRIVATE LVGLRuntime::Runtime)
    add_test(NAME LVGLRuntimeCAPI COMMAND LVGLRuntimeCAPI)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME LVGLRuntimeMinimal COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-embedded"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/Verify.cmake")
    set_tests_properties(LVGLRuntimeMinimal PROPERTIES TIMEOUT 180 LABELS "functional;firmware;protobuf;minimal")
    add_test(NAME LVGLRuntimeXmlMinimal COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-xml"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/XmlProfile/Verify.cmake")
    set_tests_properties(LVGLRuntimeXmlMinimal PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE
        LABELS "functional;firmware;xml;protobuf;minimal")
    add_test(NAME LVGLRuntimeCodesMinimal COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-codes"
        "-DPROJECT_FILE=${PROJECT_BINARY_DIR}/UICreator/code-widgets/fixture.pb"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/VerifyCodes.cmake")
    set_tests_properties(LVGLRuntimeCodesMinimal PROPERTIES TIMEOUT 180 DEPENDS UICreatorCodeWidgets LABELS "functional;firmware;protobuf;minimal;code-widgets")
    set(controls_minimal_emulator_settings "")
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(controls_minimal_emulator_settings "${PROJECT_BINARY_DIR}/minimal-advanced-controls-parent-emulator.cmake")
        string(REPLACE "\\" "\\\\" controls_minimal_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
        string(REPLACE "\"" "\\\"" controls_minimal_emulator "${controls_minimal_emulator}")
        string(REPLACE "$" "\\$" controls_minimal_emulator "${controls_minimal_emulator}")
        file(WRITE "${controls_minimal_emulator_settings}"
            "set(CMAKE_CROSSCOMPILING_EMULATOR \"${controls_minimal_emulator}\" CACHE STRING \"Parent profile emulator\" FORCE)\n")
    endif()
    add_test(NAME LVGLRuntimeAdvancedControlsMinimal COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-advanced-controls"
        "-DPROJECT_FILE=${PROJECT_BINARY_DIR}/UICreator/advanced-controls/fixture.pb"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DPARENT_EMULATOR_SETTINGS=${controls_minimal_emulator_settings}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/VerifyAdvancedControls.cmake")
    set_tests_properties(LVGLRuntimeAdvancedControlsMinimal PROPERTIES TIMEOUT 240 DEPENDS UICreatorAdvancedControls
        LABELS "functional;firmware;protobuf;minimal;advanced-controls")
    add_test(NAME LVGLRuntimeAdvancedKeyboardMinimal COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-keyboard-runtime"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/VerifyKeyboard.cmake")
    set_tests_properties(LVGLRuntimeAdvancedKeyboardMinimal PROPERTIES TIMEOUT 180 LABELS "functional;firmware;protobuf;minimal;advanced-keyboard")
    add_test(NAME LVGLRuntimeKeyboardWithoutPrediction COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/minimal-keyboard-no-prediction"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        "-DPROJECT_FILE=${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/keyboard-no-prediction.json"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Minimal/VerifyKeyboard.cmake")
    set_tests_properties(LVGLRuntimeKeyboardWithoutPrediction PROPERTIES TIMEOUT 180 LABELS "functional;firmware;protobuf;minimal;advanced-keyboard;prediction")
    add_test(NAME UICreatorArduinoProfile COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Runtime/arduino/tests" -v)
    set_tests_properties(UICreatorArduinoProfile PROPERTIES TIMEOUT 300 LABELS "functional;firmware;arduino;xml")
    add_test(NAME UICreatorESP32Profile COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Runtime/esp32/tests" -p "test_*.py")
    set_tests_properties(UICreatorESP32Profile PROPERTIES TIMEOUT 300 LABELS "unit;esp32;protobuf;xml")
    add_test(NAME UICreatorPicoProfile COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Runtime/pico/tests" -v)
    set_tests_properties(UICreatorPicoProfile PROPERTIES TIMEOUT 300
        LABELS "unit;pico;protobuf;minimal" RUN_SERIAL TRUE)
endif()
