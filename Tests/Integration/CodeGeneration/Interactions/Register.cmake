if(UICREATOR_TEST_LVGL_VERSIONS)
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
            add_test(NAME "LVGLGeneratorActions${suffix}" COMMAND "${CMAKE_COMMAND}"
                "-DCOMPILER=$<TARGET_FILE:LVGLCompiler>" "-DTARGET_VERSION=${version}"
                "-DLVGL_SOURCE_DIR=${version_source}"
                "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/${version}"
                "-DBINARY_DIR=${PROJECT_BINARY_DIR}/interaction-matrix/${version}"
                "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
                -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGeneration/Interactions/Verify.cmake")
            set_tests_properties("LVGLGeneratorActions${suffix}" PROPERTIES
                DEPENDS "LVGLGenerator${suffix}" TIMEOUT 120 LABELS "functional;versions;interactions")
        endif()
    endforeach()
endif()
