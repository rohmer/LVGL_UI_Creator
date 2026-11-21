# Include after UICreatorBootstrapTests is defined. Reuse the editor's native
# LVGL library; each firmware consumer compiles the generated C independently.
if(TARGET UICreatorBootstrapTests AND NOT TARGET UICreatorBootstrapGenerated)
    set(bootstrap_generated_root "${CMAKE_BINARY_DIR}/bootstrap-generated")
    set(bootstrap_harness_root "${PROJECT_SOURCE_DIR}/Tests/Integration/BootstrapGenerated")
    set(bootstrap_outputs)
    foreach(preset RANGE 0 48)
        foreach(file ui.c ui.h ui_assets.c ui_assets.h ui_requirements.h fixture_checks.h fixture.json fixture.pb manifest.json)
            list(APPEND bootstrap_outputs "${bootstrap_generated_root}/${preset}/${file}")
        endforeach()
    endforeach()
    add_custom_command(OUTPUT "${bootstrap_generated_root}/generated.stamp"
        BYPRODUCTS ${bootstrap_outputs}
        COMMAND "$<TARGET_FILE:UICreatorBootstrapTests>" "${bootstrap_generated_root}"
        COMMAND "${CMAKE_COMMAND}" "-DGENERATED_ROOT=${bootstrap_generated_root}" -P "${bootstrap_harness_root}/Prepare.cmake"
        COMMAND "${CMAKE_COMMAND}" -E touch "${bootstrap_generated_root}/generated.stamp"
        DEPENDS UICreatorBootstrapTests "${bootstrap_harness_root}/Prepare.cmake"
        COMMENT "Generate and validate all 49 Bootstrap native firmware projects"
        VERBATIM)
    add_custom_target(UICreatorBootstrapGeneratedFixtures DEPENDS "${bootstrap_generated_root}/generated.stamp")
    set(bootstrap_consumer_targets)
    set(bootstrap_consumer_files)
    foreach(preset RANGE 0 48)
        set(directory "${bootstrap_generated_root}/${preset}")
        set(target "UICreatorBootstrapGenerated_${preset}")
        set_source_files_properties("${directory}/ui.c" "${directory}/ui_assets.c" PROPERTIES GENERATED TRUE)
        add_executable(${target} "${bootstrap_harness_root}/Consumer.c" "${directory}/ui.c" "${directory}/ui_assets.c")
        add_dependencies(${target} UICreatorBootstrapGeneratedFixtures)
        target_include_directories(${target} PRIVATE "${directory}")
        target_link_libraries(${target} PRIVATE lvgl::lvgl)
        set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
        if(MSVC)
            target_compile_options(${target} PRIVATE /utf-8 /we4013)
        endif()
        list(APPEND bootstrap_consumer_targets ${target})
        list(APPEND bootstrap_consumer_files "$<TARGET_FILE:${target}>")
    endforeach()
    add_custom_target(UICreatorBootstrapGenerated DEPENDS ${bootstrap_consumer_targets})
    list(JOIN bootstrap_consumer_files "|" bootstrap_consumer_files)
    add_test(NAME UICreatorBootstrapGenerated COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLES=${bootstrap_consumer_files}" -P "${bootstrap_harness_root}/Verify.cmake")
    set_tests_properties(UICreatorBootstrapGenerated PROPERTIES TIMEOUT 180 LABELS "functional;bootstrap;firmware")
endif()
