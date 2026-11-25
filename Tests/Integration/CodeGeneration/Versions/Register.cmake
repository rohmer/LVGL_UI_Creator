set(UICREATOR_LVGL_TEST_SOURCES "${PROJECT_SOURCE_DIR}/build/compat-lvgl" CACHE PATH "Pinned LVGL release source directories for functional matrix")
option(UICREATOR_TEST_LVGL_VERSIONS "Register available cross-version compiler tests" ON)
if(UICREATOR_TEST_LVGL_VERSIONS)
    set(generator_parent_emulator_settings "")
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(generator_parent_emulator_settings "${PROJECT_BINARY_DIR}/version-matrix/parent-emulator.cmake")
        string(REPLACE "\\" "\\\\" generator_parent_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
        string(REPLACE "\"" "\\\"" generator_parent_emulator "${generator_parent_emulator}")
        string(REPLACE "$" "\\$" generator_parent_emulator "${generator_parent_emulator}")
        file(WRITE "${generator_parent_emulator_settings}"
            "set(CMAKE_CROSSCOMPILING_EMULATOR \"${generator_parent_emulator}\" CACHE STRING \"Parent generator fixture emulator\" FORCE)\n")
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
            add_test(NAME "LVGLGenerator${suffix}" COMMAND "${CMAKE_COMMAND}"
                "-DCOMPILER=$<TARGET_FILE:LVGLCompiler>" "-DTARGET_VERSION=${version}"
                "-DLVGL_SOURCE_DIR=${version_source}"
                "-DBINARY_DIR=${PROJECT_BINARY_DIR}/version-matrix/${version}"
                "-DFIXTURE_DIR=${PROJECT_BINARY_DIR}/CodeGeneration/generated_test"
                "-DASSET_PROJECT=${PROJECT_BINARY_DIR}/UICreator/Images/generated-test/runtime.pb"
                "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
                "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DPARENT_EMULATOR_SETTINGS=${generator_parent_emulator_settings}"
                -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Versions/Verify.cmake")
            set_tests_properties("LVGLGenerator${suffix}" PROPERTIES TIMEOUT 300 LABELS "functional;versions")
        endif()
    endforeach()
endif()
