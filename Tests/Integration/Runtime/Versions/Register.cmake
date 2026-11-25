set(runtime_matrix_catch UICreatorCatch)
if(MSVC)
    # The generator/backend matrix deliberately builds Release firmware. A
    # Debug editor's checked STL/debug CRT cannot cross this library boundary.
    set(runtime_matrix_catch UICreatorCatchMatrix)
endif()
set(runtime_parent_emulator_settings "")
if(CMAKE_CROSSCOMPILING_EMULATOR)
    # An init-cache file preserves the emulator's argument-list semicolons
    # through CTest and the nested configure helper.
    set(runtime_parent_emulator_settings "${PROJECT_BINARY_DIR}/runtime-matrix/parent-emulator.cmake")
    string(REPLACE "\\" "\\\\" runtime_parent_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
    string(REPLACE "\"" "\\\"" runtime_parent_emulator "${runtime_parent_emulator}")
    string(REPLACE "$" "\\$" runtime_parent_emulator "${runtime_parent_emulator}")
    file(WRITE "${runtime_parent_emulator_settings}"
        "set(CMAKE_CROSSCOMPILING_EMULATOR \"${runtime_parent_emulator}\" CACHE STRING \"Parent runtime fixture emulator\" FORCE)\n")
endif()
foreach(pair "9.6|vendored" "9.5|v9_5_0" "9.3|v9_3_0" "9.2|v9_2_2" "9.1|v9_1_0" "8.3|v8_3_11")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 version)
    list(GET fields 1 directory)
    set(version_source "${UICREATOR_LVGL_TEST_SOURCES}/${directory}")
    if(directory STREQUAL "vendored")
        set(version_source "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl")
    endif()
    if(EXISTS "${version_source}/lvgl.h")
        string(REPLACE "." "" suffix "${version}")
        add_test(NAME "LVGLRuntimeBackend${suffix}" COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILER=$<TARGET_FILE:LVGLCompiler>" "-DTARGET_VERSION=${version}"
            "-DLVGL_SOURCE_DIR=${version_source}" "-DCATCH_ARCHIVE=$<TARGET_FILE:${runtime_matrix_catch}>"
            "-DBINARY_DIR=${PROJECT_BINARY_DIR}/runtime-matrix/${version}"
            "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/${version}"
            "-DMEDIA_CASES=${PROJECT_BINARY_DIR}/Runtime/media-output"
            "-DWIDGET_INPUT=${PROJECT_BINARY_DIR}/CodeGeneration/generated_test/fixture.pb"
            "-DASSET_INPUT=${PROJECT_BINARY_DIR}/UICreator/Images/generated-test/runtime.pb"
            "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
            "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DPARENT_EMULATOR_SETTINGS=${runtime_parent_emulator_settings}"
            -P "${PROJECT_SOURCE_DIR}/Tests/Integration/Runtime/Versions/Verify.cmake")
        set_tests_properties("LVGLRuntimeBackend${suffix}" PROPERTIES DEPENDS "LVGLGenerator${suffix};LVGLRuntimeMedia" TIMEOUT 300 LABELS "functional;runtime;versions")
    endif()
endforeach()
