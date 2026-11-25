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
set(UICREATOR_PROJECT_PYTHON "" CACHE FILEPATH "Private project-helper packaging Python")
if(NOT UICREATOR_PROJECT_PYTHON)
    set(project_environment "${PROJECT_SOURCE_DIR}/build/project-tools-env")
    if(WIN32)
        set(project_python "${project_environment}/Scripts/python.exe")
    else()
        set(project_python "${project_environment}/bin/python")
    endif()
    if(NOT EXISTS "${project_python}")
        execute_process(COMMAND "${Python3_EXECUTABLE}" -m venv "${project_environment}" RESULT_VARIABLE project_venv_result)
        if(NOT project_venv_result EQUAL 0)
            message(FATAL_ERROR "Could not create the project helper packaging environment")
        endif()
    endif()
    set(UICREATOR_PROJECT_PYTHON "${project_python}" CACHE FILEPATH "Private project-helper packaging Python" FORCE)
endif()
execute_process(COMMAND "${UICREATOR_PROJECT_PYTHON}" -c "import PyInstaller" RESULT_VARIABLE project_import_result OUTPUT_QUIET ERROR_QUIET)
if(NOT project_import_result EQUAL 0)
    execute_process(COMMAND "${UICREATOR_PROJECT_PYTHON}" -m pip install "pyinstaller==6.19.0" RESULT_VARIABLE project_install_result)
    if(NOT project_install_result EQUAL 0)
        message(FATAL_ERROR "Could not prepare the project helper packager")
    endif()
endif()
set(creator_dist "${PROJECT_SOURCE_DIR}/build/project-tool-dist/UICreatorProjectTool")
set(creator_runtime "${creator_dist}/UICreatorProjectTool${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(OUTPUT "${creator_runtime}"
    COMMAND "${UICREATOR_PROJECT_PYTHON}" -m PyInstaller --noconfirm --clean --onedir --name UICreatorProjectTool
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
    "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/include/*" "${PROJECT_SOURCE_DIR}/3rdParty/SDL3/src/*"
    "${ADVKBD_SOURCE_ROOT}/src/*" "${ADVKBD_SOURCE_ROOT}/include/*" "${ADVKBD_SOURCE_ROOT}/cmake/*")
list(APPEND creator_linux_inputs "${PROJECT_SOURCE_DIR}/advanced-keyboard.lock.json"
    "${PROJECT_SOURCE_DIR}/Integrations/AdvancedKeyboard/dependency.py"
    "${PROJECT_SOURCE_DIR}/cmake/AdvancedKeyboardDependency.cmake" "${ADVKBD_SOURCE_ROOT}/CMakeLists.txt")
list(APPEND creator_linux_inputs ${creator_linux_source_inputs})
list(FILTER creator_linux_inputs EXCLUDE REGEX "/(__pycache__|build|node_modules|\\.git)/|\\.pyc$")
add_custom_command(OUTPUT "${creator_linux_archive}"
    COMMAND "${CMAKE_COMMAND}" -E env "ADVANCED_KEYBOARD_ROOT=${ADVKBD_SOURCE_ROOT}"
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Runtime/linux/package_toolkit.py"
        --output "${CMAKE_CURRENT_BINARY_DIR}/linux-toolkit" --replace
    DEPENDS ${creator_linux_inputs} VERBATIM)
set(creator_pico_archive "${CMAKE_CURRENT_BINARY_DIR}/pico-toolkit.zip")
add_custom_command(OUTPUT "${creator_pico_archive}"
    COMMAND "${CMAKE_COMMAND}" -E env "ADVANCED_KEYBOARD_ROOT=${ADVKBD_SOURCE_ROOT}"
        "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Runtime/pico/package_toolkit.py"
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
set(UICREATOR_RUNTIME_PYTHON_DIR "" CACHE PATH "Target-native relocatable Python for project creation")
set(UICREATOR_LINUX_PYTHON_DIR "" CACHE PATH "Legacy target-native project-helper Python path")
set(project_runtime_python "${UICREATOR_RUNTIME_PYTHON_DIR}")
if(NOT project_runtime_python)
    set(project_runtime_python "${UICREATOR_LINUX_PYTHON_DIR}")
endif()
if(project_runtime_python)
    if(NOT EXISTS "${project_runtime_python}/bin/python3")
        message(FATAL_ERROR "Prepared project helper Python runtime is missing")
    endif()
    add_custom_command(TARGET UICreatorProjectCreatorAssets POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Packaging/StageProjectRuntime.py"
            "${project_runtime_python}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" VERBATIM)
endif()
add_library(UICreatorProjectCreation STATIC ProjectCreator.cpp)
target_link_libraries(UICreatorProjectCreation PUBLIC UICreatorCodeGeneration UICreatorExternalTool)
target_include_directories(UICreatorProjectCreation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(UICreatorProjectCreation PRIVATE /utf-8)
endif()
if(WIN32)
    target_link_libraries(UICreatorProjectCreation PUBLIC shell32)
endif()
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(UICreatorExternalToolTests "${PROJECT_SOURCE_DIR}/Tests/Integration/ProjectCreator/ExternalTool.cpp")
    target_link_libraries(UICreatorExternalToolTests PRIVATE UICreatorExternalTool Threads::Threads)
    target_compile_features(UICreatorExternalToolTests PRIVATE cxx_std_17)
    add_test(NAME UICreatorProjectToolDeadline COMMAND UICreatorExternalToolTests "${PROJECT_BINARY_DIR}/project-tool-deadline-tests")
    set_tests_properties(UICreatorProjectToolDeadline PROPERTIES TIMEOUT 30 LABELS "functional;project-creator")
    add_test(NAME UICreatorProjectCreatorUnit COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/UnitTests/ProjectCreator/test_creator.py")
    set_tests_properties(UICreatorProjectCreatorUnit PROPERTIES LABELS "unit;project-creator" TIMEOUT 90 RUN_SERIAL TRUE)
endif()
if(BUILD_TESTING)
    add_executable(UICreatorProjectCreationTests "${PROJECT_SOURCE_DIR}/Tests/Integration/ProjectCreator/Creator.cpp")
    target_link_libraries(UICreatorProjectCreationTests PRIVATE UICreatorProjectCreation LVGLRuntime::Runtime)
    add_dependencies(UICreatorProjectCreationTests UICreatorProjectCreatorAssets)
    set(project_creator_parent_emulator_settings "")
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(project_creator_parent_emulator_settings "${PROJECT_BINARY_DIR}/project-creator-tests/parent-emulator.cmake")
        string(REPLACE "\\" "\\\\" project_creator_parent_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
        string(REPLACE "\"" "\\\"" project_creator_parent_emulator "${project_creator_parent_emulator}")
        string(REPLACE "$" "\\$" project_creator_parent_emulator "${project_creator_parent_emulator}")
        file(WRITE "${project_creator_parent_emulator_settings}"
            "set(CMAKE_CROSSCOMPILING_EMULATOR \"${project_creator_parent_emulator}\" CACHE STRING \"Parent project fixture emulator\" FORCE)\n")
    endif()
    add_test(NAME UICreatorProjectCreatorFunctional COMMAND "${CMAKE_COMMAND}"
        "-DPYTHON=${Python3_EXECUTABLE}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DBINARY_DIR=${PROJECT_BINARY_DIR}/project-creator-tests" "-DCREATOR_TEST=$<TARGET_FILE:UICreatorProjectCreationTests>"
        "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DPARENT_EMULATOR_SETTINGS=${project_creator_parent_emulator_settings}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/ProjectCreator/Verify.cmake")
    set_tests_properties(UICreatorProjectCreatorFunctional PROPERTIES DEPENDS LVGLGenerator96 TIMEOUT 1800 LABELS "functional;project-creator" RUN_SERIAL TRUE)
endif()
