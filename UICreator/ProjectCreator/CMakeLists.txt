find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
set(UICREATOR_PREBUILT_PROJECT_TOOL_DIR "" CACHE PATH "Prepared target-native project-creator assets")
if(UICREATOR_PREBUILT_PROJECT_TOOL_DIR)
    set(creator_dist "${UICREATOR_PREBUILT_PROJECT_TOOL_DIR}")
    set(creator_runtime "${creator_dist}/UICreatorProjectTool${CMAKE_EXECUTABLE_SUFFIX}")
    if(NOT EXISTS "${creator_runtime}")
        message(FATAL_ERROR "Prepared project creator is missing: ${creator_runtime}")
    endif()
else()
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Cross builds require UICREATOR_PREBUILT_PROJECT_TOOL_DIR with target-native helpers")
endif()
set(creator_dist "${PROJECT_SOURCE_DIR}/build/project-tool-dist/UICreatorProjectTool")
set(creator_runtime "${creator_dist}/UICreatorProjectTool${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(OUTPUT "${creator_runtime}"
    COMMAND "${UICREATOR_IMAGE_PYTHON}" -m PyInstaller --noconfirm --clean --onedir --name UICreatorProjectTool
        --distpath "${PROJECT_SOURCE_DIR}/build/project-tool-dist" --workpath "${PROJECT_SOURCE_DIR}/build/project-tool-work"
        --specpath "${PROJECT_SOURCE_DIR}/build" "${CMAKE_CURRENT_SOURCE_DIR}/project_creator.py"
    DEPENDS project_creator.py raspberry_pi_linux.py raspberry_pi_pico.py VERBATIM)
endif()
set(creator_lvgl_archive "${CMAKE_CURRENT_BINARY_DIR}/lvgl-9.6.0.zip")
add_custom_command(OUTPUT "${creator_lvgl_archive}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_lvgl.py" "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl" "${creator_lvgl_archive}"
    DEPENDS package_lvgl.py "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl/lvgl.h" "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl/include/lvgl/lv_version.h"
    VERBATIM)
set(creator_linux_archive "${CMAKE_CURRENT_BINARY_DIR}/linux-toolkit.zip")
set(creator_linux_inputs
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lv_conf_v9.h"
    "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/CMakeLists.txt")
# Exact files must stay outside GLOB_RECURSE: a root LICENSE pattern also
# discovers generated SDK licenses beneath build/ and repeatedly reconfigures.
file(GLOB_RECURSE creator_linux_source_inputs CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/Runtime/*" "${PROJECT_SOURCE_DIR}/LVSerial/*"
    "${PROJECT_SOURCE_DIR}/UICreator/Drivers/SDL3/*" "${PROJECT_SOURCE_DIR}/CodeGeneration/Targets/*"
    "${PROJECT_SOURCE_DIR}/ImgLib/*.h" "${PROJECT_SOURCE_DIR}/3rdParty/JSON/*.hpp"
    "${PROJECT_SOURCE_DIR}/3rdParty/nanopb/*" "${PROJECT_SOURCE_DIR}/3rdParty/LVGL/lvgl/*"
    "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/cmake/*"
    "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/include/*" "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/src/*")
list(APPEND creator_linux_inputs ${creator_linux_source_inputs})
list(FILTER creator_linux_inputs EXCLUDE REGEX "/(__pycache__|build|node_modules|\\.git)/|\\.pyc$")
add_custom_command(OUTPUT "${creator_linux_archive}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Runtime/linux/package_toolkit.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/linux-toolkit" --replace
    DEPENDS ${creator_linux_inputs} VERBATIM)
set(creator_pico_archive "${CMAKE_CURRENT_BINARY_DIR}/pico-toolkit.zip")
add_custom_command(OUTPUT "${creator_pico_archive}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Runtime/pico/package_toolkit.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/pico-toolkit" --replace
    DEPENDS ${creator_linux_inputs} "${PROJECT_SOURCE_DIR}/Runtime/pico/package_toolkit.py" VERBATIM)
if(UICREATOR_WINDOWS_HELPERS_X86)
    set(creator_copy_command "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/StageWindowsHelper.py"
        "${creator_dist}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator"
        --keep catalog.json --keep lvgl-9.6.0.zip --keep linux-toolkit.zip --keep pico-toolkit.zip)
else()
    set(creator_copy_command "${CMAKE_COMMAND}" -E copy_directory "${creator_dist}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator")
endif()
add_custom_target(UICreatorProjectCreatorAssets
    COMMAND ${creator_copy_command}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/catalog.json" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator/catalog.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${creator_lvgl_archive}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator/lvgl-9.6.0.zip"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${creator_linux_archive}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator/linux-toolkit.zip"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${creator_pico_archive}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/project-creator/pico-toolkit.zip"
    DEPENDS "${creator_runtime}" "${creator_lvgl_archive}" "${creator_linux_archive}" "${creator_pico_archive}" catalog.json VERBATIM)
add_library(UICreatorProjectCreation STATIC ProjectCreator.cpp)
target_link_libraries(UICreatorProjectCreation PUBLIC UICreatorCodeGeneration UICreatorFontConversion)
target_include_directories(UICreatorProjectCreation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(UICreatorProjectCreation PRIVATE /utf-8)
endif()
if(WIN32)
    target_link_libraries(UICreatorProjectCreation PUBLIC shell32)
endif()
if(BUILD_TESTING)
    add_test(NAME UICreatorProjectCreatorUnit COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/UnitTests/ProjectCreator/test_creator.py")
    set_tests_properties(UICreatorProjectCreatorUnit PROPERTIES LABELS "unit;project-creator" TIMEOUT 90 RUN_SERIAL TRUE)
endif()
if(BUILD_TESTING)
    add_executable(UICreatorProjectCreationTests "${PROJECT_SOURCE_DIR}/Tests/Integration/ProjectCreator/Creator.cpp")
    target_link_libraries(UICreatorProjectCreationTests PRIVATE UICreatorProjectCreation LVGLRuntime::Runtime)
    add_dependencies(UICreatorProjectCreationTests UICreatorProjectCreatorAssets)
    add_test(NAME UICreatorProjectCreatorFunctional COMMAND "${CMAKE_COMMAND}"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DBINARY_DIR=${PROJECT_BINARY_DIR}/project-creator-tests" "-DCREATOR_TEST=$<TARGET_FILE:UICreatorProjectCreationTests>"
        "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/ProjectCreator/Verify.cmake")
    set_tests_properties(UICreatorProjectCreatorFunctional PROPERTIES DEPENDS LVGLGenerator96 TIMEOUT 360 LABELS "functional;project-creator" RUN_SERIAL TRUE)
endif()
