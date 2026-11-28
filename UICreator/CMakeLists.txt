file(GLOB_RECURSE creator_sources CONFIGURE_DEPENDS "*.cpp")
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/AdvancedKeyboardDependency.cmake")
set(advanced_keyboard_trainer "${ADVKBD_SOURCE_ROOT}/tools/NativeTrieGenerator/source")
list(FILTER creator_sources EXCLUDE REGEX "/Converters/|/Fonts/Native[^/]*\\.cpp$|/Images/Native[^/]*\\.cpp$|/Utils/ExternalTool\\.cpp$")
list(REMOVE_ITEM creator_sources "${CMAKE_CURRENT_SOURCE_DIR}/Fonts/FontConverter.cpp")
list(REMOVE_ITEM creator_sources "${CMAKE_CURRENT_SOURCE_DIR}/Images/ImageConverter.cpp")
list(REMOVE_ITEM creator_sources "${CMAKE_CURRENT_SOURCE_DIR}/ProjectCreator/ProjectCreator.cpp")
list(FILTER creator_sources EXCLUDE REGEX "/LanguageModels/(Generator|TrainingModel|ModelGen)\\.cpp$")
include("${PROJECT_SOURCE_DIR}/Packaging/WindowsHelpers.cmake")
add_subdirectory(Fonts)
add_subdirectory(Images)
add_library(UICreatorExternalTool STATIC Utils/ExternalTool.cpp)
target_compile_features(UICreatorExternalTool PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(UICreatorExternalTool PRIVATE /utf-8)
endif()
add_subdirectory(ProjectCreator)
add_executable(UICreatorConverter Converters/Main.cpp)
target_link_libraries(UICreatorConverter PRIVATE UICreatorFontConversion UICreatorImageConversion)
target_compile_features(UICreatorConverter PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(UICreatorConverter PRIVATE /utf-8)
endif()
if(BUILD_TESTING)
    set(keyboard_parent_emulator_settings "")
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(keyboard_parent_emulator_settings "${PROJECT_BINARY_DIR}/keyboard-fixtures/parent-emulator.cmake")
        string(REPLACE "\\" "\\\\" keyboard_parent_emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
        string(REPLACE "\"" "\\\"" keyboard_parent_emulator "${keyboard_parent_emulator}")
        string(REPLACE "$" "\\$" keyboard_parent_emulator "${keyboard_parent_emulator}")
        file(WRITE "${keyboard_parent_emulator_settings}"
            "set(CMAKE_CROSSCOMPILING_EMULATOR \"${keyboard_parent_emulator}\" CACHE STRING \"Parent keyboard fixture emulator\" FORCE)\n")
    endif()
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    add_test(NAME UICreatorNativeConverters COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Converters/Verify.py"
        --tool "$<TARGET_FILE:UICreatorConverter>" --repository "${PROJECT_SOURCE_DIR}")
    set_tests_properties(UICreatorNativeConverters PROPERTIES TIMEOUT 150 LABELS "native;converter;packaging")
endif()
add_subdirectory(LanguageModels)
add_custom_target(UICreatorLanguageModelAssets
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${advanced_keyboard_trainer}/wordlists"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/wordlists"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${advanced_keyboard_trainer}/examples"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/examples"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${advanced_keyboard_trainer}/README.md"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/README.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${advanced_keyboard_trainer}/PROVENANCE.md"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/PROVENANCE.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${advanced_keyboard_trainer}/TrainingModel.md"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/TrainingModel.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ADVKBD_SOURCE_ROOT}/src/RadixFormat.md"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/RadixFormat.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${ADVKBD_SOURCE_ROOT}/models/bundled/models"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/bundled"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ADVKBD_SOURCE_ROOT}/models/bundled/catalog.json"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/bundled/catalog.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ADVKBD_SOURCE_ROOT}/models/bundled/README.md"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/language-models/bundled/README.md"
    VERBATIM)
if(TARGET UICreatorProjectCreationTests)
    target_sources(UICreatorProjectCreationTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/KeyboardModels/BundledTries.cpp")
    target_link_libraries(UICreatorProjectCreationTests PRIVATE SDL3::SDL3)
    add_dependencies(UICreatorProjectCreationTests UICreatorLanguageModelAssets)
endif()

if(WIN32)
    add_library(UICreatorBreakpad STATIC
        "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src/client/windows/handler/exception_handler.cc"
        "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src/client/windows/crash_generation/crash_generation_client.cc"
        "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src/common/windows/guid_string.cc"
        "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src/common/windows/string_utils.cc")
    target_include_directories(UICreatorBreakpad PUBLIC "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src")
    target_compile_definitions(UICreatorBreakpad PRIVATE UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(UICreatorBreakpad PUBLIC dbghelp rpcrt4)
endif()

add_executable(UICreator ${creator_sources})
add_dependencies(UICreator UICreatorModelGen)
add_custom_target(UICreatorFigmaAssets
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/Integrations/Figma"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/integrations/figma"
    VERBATIM)
add_dependencies(UICreator UICreatorFigmaAssets)
if(TARGET UICreatorViewer)
    add_dependencies(UICreator UICreatorViewer)
endif()
target_compile_definitions(UICreator PRIVATE USE_TELEMETRY=0)
target_link_libraries(UICreator PRIVATE LVGLRuntime::Runtime UICreatorLanguageModels UICreatorProjectCreation UICreatorImageConversion UICreatorFontConversion UICreatorCodeGeneration LVSerial freetype SDL3::SDL3 spdlog::spdlog_header_only)
add_dependencies(UICreator UICreatorLanguageModelAssets)
add_dependencies(UICreator UICreatorProjectCreatorAssets)
add_dependencies(UICreator UICreatorConverter)

if(WIN32)
    target_compile_definitions(UICreator PRIVATE UNICODE _UNICODE)
    target_link_libraries(UICreator PRIVATE UICreatorBreakpad comdlg32 ole32 shell32 winhttp)
else()
    target_link_libraries(UICreator PRIVATE ${CMAKE_DL_LIBS})
    target_include_directories(UICreator PRIVATE "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src")
endif()
if(MSVC)
    target_compile_options(UICreator PRIVATE /utf-8)
endif()
if(BUILD_TESTING AND UICREATOR_USE_LVGL9)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME UICreatorKeyboardSyncUnit COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Tests/UnitTests/KeyboardSync" -v)
    add_test(NAME UICreatorKeyboardSyncSource COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/Integrations/ESPOS/sync_keyboard.py" --check-source)
    add_test(NAME UICreatorAdvancedKeyboardDependency COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/Integrations/AdvancedKeyboard/dependency.py" --project-root "${PROJECT_SOURCE_DIR}" --check)
    add_test(NAME UICreatorAdvancedKeyboardDependencyUnit COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Integrations/AdvancedKeyboard/tests" -p test_dependency.py -v)
    add_test(NAME UICreatorESPOSDependencyUnit COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${PROJECT_SOURCE_DIR}/Integrations/ESPOS/tests" -p test_dependency_helper.py -v)
    add_test(NAME UICreatorESPOSBundledGenerator COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${ADVKBD_SOURCE_ROOT}/tests/python" -p test_bundled_models.py -v)
    add_test(NAME UICreatorBundledModelRebuild COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${ADVKBD_SOURCE_ROOT}/tools/RadixModel" -p test_rebuild_bundled_models.py -v)
    add_test(NAME UICreatorLanguageBenchmarkTools COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${ADVKBD_SOURCE_ROOT}/tools/benchmarks" -v)
    add_test(NAME UICreatorKeyboardPortableTools COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${ADVKBD_SOURCE_ROOT}/tests/python" -p test_legacy_tools.py -v)
    set_tests_properties(UICreatorAdvancedKeyboardDependency PROPERTIES TIMEOUT 90 LABELS "unit;advanced-keyboard;dependency")
    set_tests_properties(UICreatorAdvancedKeyboardDependencyUnit UICreatorESPOSDependencyUnit PROPERTIES TIMEOUT 90 LABELS "unit;advanced-keyboard;dependency")
    set_tests_properties(UICreatorKeyboardSyncUnit UICreatorKeyboardSyncSource PROPERTIES
        TIMEOUT 90 LABELS "unit;advanced-keyboard;sync")
    set_tests_properties(UICreatorESPOSBundledGenerator PROPERTIES
        TIMEOUT 90 LABELS "unit;advanced-keyboard;sync;language-models")
    set_tests_properties(UICreatorBundledModelRebuild PROPERTIES
        TIMEOUT 90 LABELS "unit;advanced-keyboard;language-models")
    set_tests_properties(UICreatorLanguageBenchmarkTools PROPERTIES
        TIMEOUT 90 LABELS "unit;language-models;benchmark")
    set_tests_properties(UICreatorKeyboardPortableTools PROPERTIES
        TIMEOUT 90 LABELS "unit;advanced-keyboard;tools;legacy;partition")
    add_executable(UICreatorBundledTrieTests
        "${PROJECT_SOURCE_DIR}/Tests/Integration/BundledTries.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/KeyboardModels/BundledTries.cpp")
    target_link_libraries(UICreatorBundledTrieTests PRIVATE LVSerial SDL3::SDL3)
    add_dependencies(UICreatorBundledTrieTests UICreatorLanguageModelAssets)
    if(MSVC)
        target_compile_options(UICreatorBundledTrieTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorBundledTries COMMAND UICreatorBundledTrieTests
        "${PROJECT_BINARY_DIR}/bundled-trie-tests")
    set_tests_properties(UICreatorBundledTries PROPERTIES TIMEOUT 120 LABELS "functional;language-models;prediction;advanced-keyboard")
    add_executable(UICreatorLanguageModelTests "${ADVKBD_SOURCE_ROOT}/tests/native/language-models/Generator.cpp")
    target_include_directories(UICreatorLanguageModelTests PRIVATE "${ADVKBD_SOURCE_ROOT}/tests/third_party")
    target_link_libraries(UICreatorLanguageModelTests PRIVATE UICreatorLanguageModels LVSerial)
    if(MSVC)
        target_compile_options(UICreatorLanguageModelTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorLanguageModels COMMAND UICreatorLanguageModelTests
        "${ADVKBD_SOURCE_ROOT}/tests/fixtures/language-models"
        "${PROJECT_BINARY_DIR}/language-model-tests"
        "${advanced_keyboard_trainer}/wordlists")
    set_tests_properties(UICreatorLanguageModels PROPERTIES TIMEOUT 90 LABELS "functional;language-models;prediction")
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/Integration/LanguageModels/Training" "${PROJECT_BINARY_DIR}/language-model-training-tests")
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/Integration/RadixPrediction" "${PROJECT_BINARY_DIR}/radix-prediction-tests")
    add_executable(UICreatorOptimizedLanguageModelTests "${ADVKBD_SOURCE_ROOT}/tests/native/language-models/Optimized.cpp")
    target_include_directories(UICreatorOptimizedLanguageModelTests PRIVATE "${ADVKBD_SOURCE_ROOT}/tests/third_party")
    target_link_libraries(UICreatorOptimizedLanguageModelTests PRIVATE UICreatorLanguageModels LVSerial)
    target_include_directories(UICreatorOptimizedLanguageModelTests PRIVATE "${ADVKBD_SOURCE_ROOT}/src")
    add_test(NAME UICreatorOptimizedLanguageModels COMMAND UICreatorOptimizedLanguageModelTests "${PROJECT_BINARY_DIR}/optimized-language-model-tests")
    set_tests_properties(UICreatorOptimizedLanguageModels PROPERTIES TIMEOUT 120 LABELS "functional;language-models;prediction;radix")
    add_executable(UICreatorRadixAssetTests "${PROJECT_SOURCE_DIR}/Tests/Integration/RadixAssets.cpp")
    target_include_directories(UICreatorRadixAssetTests PRIVATE "${ADVKBD_SOURCE_ROOT}/tests/native/radix")
    target_link_libraries(UICreatorRadixAssetTests PRIVATE UICreatorCodeGeneration LVSerial LVGLRuntime::Runtime)
    add_test(NAME UICreatorRadixAssets COMMAND UICreatorRadixAssetTests "${PROJECT_BINARY_DIR}/radix-asset-tests")
    set_tests_properties(UICreatorRadixAssets PROPERTIES TIMEOUT 90 LABELS "functional;language-models;prediction;protobuf;xml;radix" FIXTURES_SETUP RadixAssets)
    add_test(NAME UICreatorRadixGenerated COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/radix-generated-tests"
        "-DEXPORTED_DIR=${PROJECT_BINARY_DIR}/radix-asset-tests"
        "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6" "-DPARENT_GENERATOR=${CMAKE_GENERATOR}"
        "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}" "-DPARENT_EMULATOR_SETTINGS=${keyboard_parent_emulator_settings}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/RadixPrediction/VerifyGenerated.cmake")
    set_tests_properties(UICreatorRadixGenerated PROPERTIES TIMEOUT 240 LABELS "functional;language-models;prediction;export;radix" FIXTURES_REQUIRED RadixAssets)
    if(MSVC)
        target_compile_options(UICreatorOptimizedLanguageModelTests PRIVATE /utf-8)
        target_compile_options(UICreatorRadixAssetTests PRIVATE /utf-8)
    endif()
    add_executable(UICreatorKeyboardSuggestionTextTests "${PROJECT_SOURCE_DIR}/Tests/Integration/KeyboardSuggestionText.cpp")
    target_link_libraries(UICreatorKeyboardSuggestionTextTests PRIVATE UICreatorLanguageModels LVSerial)
    if(MSVC)
        target_compile_options(UICreatorKeyboardSuggestionTextTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorKeyboardSuggestionText COMMAND UICreatorKeyboardSuggestionTextTests)
    set_tests_properties(UICreatorKeyboardSuggestionText PROPERTIES TIMEOUT 30 LABELS "functional;advanced-keyboard;prediction")
    add_test(NAME UICreatorSmoke COMMAND UICreator --smoke-test)
    set_tests_properties(UICreatorSmoke PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 15)
    set(editor_test_sources ${creator_sources})
    list(REMOVE_ITEM editor_test_sources "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(UICreatorEditorTests ${editor_test_sources}
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Editor.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/CodeWidgets.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/AdvancedKeyboard.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/ProjectKeyboard.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/LanguageModels.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/RunMode.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Performance.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/TftSettings.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/ScreenMemory.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/TextInputModes.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/EditorInputMatrix.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/EditorInputAssets.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/EditorInputProperties.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/ObjectIcons.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/HierarchyIcons.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/MenuToolbarIcons.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/ThemeGallery.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Variables.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/PolishCore.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/VisualRegression.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/WorkflowProblems.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/VisualAuthoring.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Design/VisualModels.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/DevicePlayground.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Interactions.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/DesignTools.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Design/ProjectDesign.cpp"
        "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Menus.cpp")
    target_link_libraries(UICreatorEditorTests PRIVATE LVGLRuntime::Runtime UICreatorLanguageModels UICreatorProjectCreation UICreatorImageConversion UICreatorFontConversion UICreatorCodeGeneration LVSerial freetype SDL3::SDL3 spdlog::spdlog_header_only)
    add_dependencies(UICreatorEditorTests UICreatorLanguageModelAssets)
    add_dependencies(UICreatorEditorTests UICreatorProjectCreatorAssets)
    add_dependencies(UICreatorEditorTests UICreatorConverter)
    target_compile_definitions(UICreatorEditorTests PRIVATE USE_TELEMETRY=0)
    if(WIN32)
        target_compile_definitions(UICreatorEditorTests PRIVATE UNICODE _UNICODE)
        target_link_libraries(UICreatorEditorTests PRIVATE UICreatorBreakpad comdlg32 ole32 shell32 winhttp)
    else()
        target_link_libraries(UICreatorEditorTests PRIVATE ${CMAKE_DL_LIBS})
        target_include_directories(UICreatorEditorTests PRIVATE "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src")
    endif()
    if(MSVC)
        target_compile_options(UICreatorEditorTests PRIVATE /utf-8)
        target_link_options(UICreatorEditorTests PRIVATE "/MAP:${PROJECT_BINARY_DIR}/editor-tests.map")
    endif()
    add_test(NAME UICreatorKeyboardModeRepeat COMMAND UICreatorEditorTests --keyboard-mode-repeat)
    add_test(NAME UICreatorTftSettings COMMAND UICreatorEditorTests --tft-settings)
    add_test(NAME UICreatorScreenMemory COMMAND UICreatorEditorTests --screen-memory)
    set_tests_properties(UICreatorScreenMemory PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;editor;memory")
    add_test(NAME UICreatorTextInputModes COMMAND UICreatorEditorTests --text-input-modes)
    set_tests_properties(UICreatorTextInputModes PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor;advanced-keyboard;input-modes")
    add_test(NAME UICreatorEditorInputMatrix COMMAND UICreatorEditorTests --editor-input-matrix)
    add_test(NAME UICreatorObjectIcons COMMAND UICreatorEditorTests --object-icons "${PROJECT_BINARY_DIR}/object-icon-tests")
    add_test(NAME UICreatorHierarchyIcons COMMAND UICreatorEditorTests --hierarchy-icons "${PROJECT_BINARY_DIR}/hierarchy-icon-tests")
    add_test(NAME UICreatorMenuToolbarIcons COMMAND UICreatorEditorTests --menu-toolbar-icons "${PROJECT_BINARY_DIR}/menu-toolbar-icon-tests")
    add_test(NAME UICreatorThemeGallery COMMAND UICreatorEditorTests --theme-gallery "${PROJECT_BINARY_DIR}/theme-gallery-tests")
    add_test(NAME UICreatorVariablesEditor COMMAND UICreatorEditorTests --variables "${PROJECT_BINARY_DIR}/variables-editor-tests")
    set_tests_properties(UICreatorVariablesEditor PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 120 RUN_SERIAL TRUE LABELS "functional;editor;variables;bindings")
    set_tests_properties(UICreatorThemeGallery PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 90 RUN_SERIAL TRUE LABELS "functional;editor;themes;gallery")
    set_tests_properties(UICreatorMenuToolbarIcons PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 60 RUN_SERIAL TRUE LABELS "functional;editor;icons;menus;toolbar")
    set_tests_properties(UICreatorHierarchyIcons PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 90 RUN_SERIAL TRUE LABELS "functional;editor;icons;hierarchy")
    set_tests_properties(UICreatorObjectIcons PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 60 RUN_SERIAL TRUE LABELS "functional;editor;icons;palette")
    set_tests_properties(UICreatorEditorInputMatrix PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 120 RUN_SERIAL TRUE LABELS "functional;editor;keyboard")
    if(APPLE)
        # Hosted Mac software rendering is slower; retain every field check
        # in the complete input pass with a bounded platform-specific budget.
        set_tests_properties(UICreatorEditorInputMatrix PROPERTIES TIMEOUT 300)
    endif()
    set_tests_properties(UICreatorTftSettings PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 45 LABELS "functional;editor;keyboard;tft")
    set_tests_properties(UICreatorKeyboardModeRepeat PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 15 RUN_SERIAL TRUE)
    if(APPLE)
        # Keep the one-second event-handler check separate from the software
        # renderer's ten complete pointer/refresh round trips.
        set_tests_properties(UICreatorKeyboardModeRepeat PROPERTIES TIMEOUT 45)
    endif()
    add_test(NAME UICreatorArcBarStress COMMAND UICreatorEditorTests --stress-arcs-bars)
    set_tests_properties(UICreatorArcBarStress PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60)
    add_test(NAME UICreatorProjectCreatorWizard COMMAND UICreatorEditorTests --project-creator)
    set_tests_properties(UICreatorProjectCreatorWizard PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;project-creator")
    add_test(NAME UICreatorUndoRedo COMMAND UICreatorEditorTests --undo-redo)
    set_tests_properties(UICreatorUndoRedo PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60)
    add_test(NAME UICreatorEditorWorkflow COMMAND UICreatorEditorTests)
    add_test(NAME UICreatorFontAssetsEditor COMMAND UICreatorEditorTests --font-assets)
    set_tests_properties(UICreatorFontAssetsEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;fonts;assets")
    add_test(NAME UICreatorBuiltinFontsEditor COMMAND UICreatorEditorTests --builtin-fonts)
    set_tests_properties(UICreatorBuiltinFontsEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;fonts;assets")
    add_test(NAME UICreatorMicroPythonEditor COMMAND UICreatorEditorTests --micropython-export "${PROJECT_BINARY_DIR}/micropython-editor-export")
    set_tests_properties(UICreatorMicroPythonEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;micropython;export")
    if(LVSERIAL_USE_XML)
        add_test(NAME UICreatorXmlEditor COMMAND UICreatorEditorTests --xml-export "${PROJECT_BINARY_DIR}/xml-editor-export/project.xml")
        set_tests_properties(UICreatorXmlEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "xml;functional;editor;export")
    endif()
    add_test(NAME UICreatorCodeWidgetsEditor COMMAND UICreatorEditorTests --code-widgets)
    set_tests_properties(UICreatorCodeWidgetsEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor;code-widgets")
    add_test(NAME UICreatorAdvancedKeyboardEditor COMMAND UICreatorEditorTests --advanced-keyboard)
    set_tests_properties(UICreatorAdvancedKeyboardEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT "$<IF:$<CONFIG:Debug>,120,45>" LABELS "functional;editor;advanced-keyboard")
    add_test(NAME UICreatorProjectKeyboardEditor COMMAND UICreatorEditorTests --project-keyboard "${CMAKE_CURRENT_BINARY_DIR}/project-keyboard-properties.bmp")
    set_tests_properties(UICreatorProjectKeyboardEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 120 LABELS "functional;editor;advanced-keyboard;runtime")
    add_test(NAME UICreatorLanguageModelEditor COMMAND UICreatorEditorTests --language-models)
    set_tests_properties(UICreatorLanguageModelEditor PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;language-models")
    add_test(NAME UICreatorMenus COMMAND UICreatorEditorTests --menus)
    add_test(NAME UICreatorRunMode COMMAND UICreatorEditorTests --run-mode)
    add_test(NAME UICreatorPerformance COMMAND UICreatorEditorTests --performance)
    add_test(NAME UICreatorPolishCore COMMAND UICreatorEditorTests --polish-core)
    set_tests_properties(UICreatorPolishCore PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 120 LABELS "functional;editor;polish")
    add_test(NAME UICreatorVisualAuthoring COMMAND UICreatorEditorTests --visual-authoring)
    add_test(NAME UICreatorWorkflowProblems COMMAND UICreatorEditorTests --workflow-problems)
    set_tests_properties(UICreatorPerformance UICreatorVisualAuthoring UICreatorWorkflowProblems PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 90 LABELS "functional;editor;polish")
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/Integration/Devices" "${CMAKE_BINARY_DIR}/device-tools-tests")
    add_test(NAME UICreatorDevicePlayground COMMAND UICreatorEditorTests --device-playground)
    set_tests_properties(UICreatorDevicePlayground PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;devices;prediction")
    add_test(NAME UICreatorDesignTools COMMAND UICreatorEditorTests --design-tools)
    add_test(NAME UICreatorInteractions COMMAND UICreatorEditorTests --interactions)
    set_tests_properties(UICreatorDesignTools UICreatorInteractions PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;design")
    set_tests_properties(UICreatorRunMode PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;run")
    set_tests_properties(UICreatorMenus PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 60 LABELS "functional;editor;menus")
    if(TARGET UICreatorViewer)
        add_dependencies(UICreatorEditorTests UICreatorViewer)
        add_test(NAME UICreatorDebugScreen COMMAND UICreatorEditorTests --debug-screen)
        set_tests_properties(UICreatorDebugScreen PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;editor;desktop;protobuf")
    endif()
    if(APPLE)
        set(editor_workflow_timeout 180)
    else()
        set(editor_workflow_timeout "$<IF:$<CONFIG:Debug>,180,60>")
    endif()
    set_tests_properties(UICreatorEditorWorkflow PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT "${editor_workflow_timeout}" RUN_SERIAL TRUE)
endif()

if(BUILD_TESTING AND WIN32)
    add_test(NAME UICreatorCrashDump COMMAND "${CMAKE_COMMAND}"
        "-DEDITOR=$<TARGET_FILE:UICreator>" "-DDUMP_ROOT=${CMAKE_BINARY_DIR}/crash-tests"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/CrashDump.cmake")
    set_tests_properties(UICreatorCrashDump PROPERTIES TIMEOUT 30)
endif()
if(BUILD_TESTING)
    add_executable(UICreatorHistoryUnitTests "${PROJECT_SOURCE_DIR}/Tests/UnitTests/Undo/History.cpp")
    target_link_libraries(UICreatorHistoryUnitTests PRIVATE UICreatorCatch)
    add_test(NAME UICreatorHistoryUnit COMMAND UICreatorHistoryUnitTests)
endif()
if(BUILD_TESTING AND UICREATOR_USE_LVGL9)
    add_executable(UICreatorNativeSDL3Tests "${PROJECT_SOURCE_DIR}/Tests/Integration/SDL3/Native.cpp" Utils/SdlSimulator.cpp)
    target_link_libraries(UICreatorNativeSDL3Tests PRIVATE lvgl::lvgl SDL3::SDL3)
    if(MSVC)
        target_compile_options(UICreatorNativeSDL3Tests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorNativeSDL3 COMMAND UICreatorNativeSDL3Tests)
    set_tests_properties(UICreatorNativeSDL3 PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;sdl3")
endif()

if(MSVC AND TARGET UICreatorEditorTests)
    target_link_options(UICreatorEditorTests PRIVATE "/MAP:${CMAKE_BINARY_DIR}/editor-tests.map")
endif()


if(TARGET UICreatorEditorTests)
    add_test(NAME UICreatorWorkspaceLayout COMMAND UICreatorEditorTests --workspace-layout)
    set_tests_properties(UICreatorWorkspaceLayout PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;editor")
endif()

if(BUILD_TESTING AND TARGET LVGLRuntime::Runtime)
    add_executable(UICreatorCodeWidgetTests "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeWidgets.cpp")
    target_link_libraries(UICreatorCodeWidgetTests PRIVATE UICreatorCodeGeneration LVSerial LVGLRuntime::Runtime)
    if(MSVC)
        target_compile_options(UICreatorCodeWidgetTests PRIVATE /utf-8)
    endif()
    set(code_widget_fixture "${CMAKE_CURRENT_BINARY_DIR}/code-widgets")
    add_custom_command(OUTPUT "${code_widget_fixture}/ui.c" "${code_widget_fixture}/ui.h" "${code_widget_fixture}/ui_assets.c"
        COMMAND UICreatorCodeWidgetTests "${code_widget_fixture}"
        DEPENDS UICreatorCodeWidgetTests VERBATIM)
    add_executable(UICreatorCodeGeneratedTests "${PROJECT_SOURCE_DIR}/Tests/Integration/CodeGenerated.c"
        "${code_widget_fixture}/ui.c" "${code_widget_fixture}/ui_assets.c")
    target_include_directories(UICreatorCodeGeneratedTests PRIVATE "${code_widget_fixture}")
    target_link_libraries(UICreatorCodeGeneratedTests PRIVATE lvgl::lvgl)
    set_target_properties(UICreatorCodeGeneratedTests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
    add_test(NAME UICreatorCodeWidgets COMMAND UICreatorCodeWidgetTests "${code_widget_fixture}")
    add_test(NAME UICreatorCodeGenerated COMMAND UICreatorCodeGeneratedTests)
    set_tests_properties(UICreatorCodeWidgets UICreatorCodeGenerated PROPERTIES TIMEOUT 60 LABELS "functional;code-widgets")
    add_executable(UICreatorAdvancedKeyboardTests "${PROJECT_SOURCE_DIR}/Tests/Integration/AdvancedKeyboard.cpp")
    target_link_libraries(UICreatorAdvancedKeyboardTests PRIVATE UICreatorCodeGeneration LVSerial LVGLRuntime::Runtime)
    if(MSVC)
        target_compile_options(UICreatorAdvancedKeyboardTests PRIVATE /utf-8)
    endif()
    set(advanced_keyboard_fixture "${CMAKE_CURRENT_BINARY_DIR}/advanced-keyboard")
    add_test(NAME UICreatorAdvancedKeyboard COMMAND UICreatorAdvancedKeyboardTests "${advanced_keyboard_fixture}")
    set_tests_properties(UICreatorAdvancedKeyboard PROPERTIES TIMEOUT 60 LABELS "functional;advanced-keyboard")
    add_executable(UICreatorKeyboardPredictionTests "${ADVKBD_SOURCE_ROOT}/tests/native/prediction/KeyboardPrediction.cpp")
    target_link_libraries(UICreatorKeyboardPredictionTests PRIVATE LVSerial)
    add_executable(UICreatorTrieAssetTests "${PROJECT_SOURCE_DIR}/Tests/Integration/TrieAssets.cpp")
    target_link_libraries(UICreatorTrieAssetTests PRIVATE LVSerial LVGLRuntime::Runtime)
    if(MSVC)
        target_compile_options(UICreatorKeyboardPredictionTests PRIVATE /utf-8)
        target_compile_options(UICreatorTrieAssetTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorKeyboardPrediction COMMAND UICreatorKeyboardPredictionTests)
    add_test(NAME UICreatorTrieAssets COMMAND UICreatorTrieAssetTests)
    set_tests_properties(UICreatorKeyboardPrediction UICreatorTrieAssets PROPERTIES TIMEOUT 60 LABELS "functional;advanced-keyboard;prediction")
    add_test(NAME UICreatorAdvancedKeyboardGenerated COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/advanced-keyboard-generated"
        "-DEXPORTED_DIR=${advanced_keyboard_fixture}" "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6"
        "-DPARENT_GENERATOR=${CMAKE_GENERATOR}" "-DPARENT_TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}"
        "-DPARENT_EMULATOR_SETTINGS=${keyboard_parent_emulator_settings}"
        -P "${PROJECT_SOURCE_DIR}/Tests/Integration/AdvancedKeyboardGenerated/Verify.cmake")
    set_tests_properties(UICreatorAdvancedKeyboardGenerated PROPERTIES DEPENDS "UICreatorAdvancedKeyboard;LVGLGenerator96" TIMEOUT 180 LABELS "functional;firmware;advanced-keyboard")
endif()

if(TARGET UICreatorEditorTests)
    add_test(NAME UICreatorCanvasNavigation COMMAND UICreatorEditorTests --canvas-navigation)
    set_tests_properties(UICreatorCanvasNavigation PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;editor")
endif()

if(TARGET UICreatorEditorTests)
    add_test(NAME UICreatorGridSnapping COMMAND UICreatorEditorTests --grid-snapping)
    set_tests_properties(UICreatorGridSnapping PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30 LABELS "functional;editor")
endif()

if(TARGET UICreatorEditorTests)
    add_test(NAME UICreatorMultiSelect COMMAND UICreatorEditorTests --multi-select)
    set_tests_properties(UICreatorMultiSelect PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor")
endif()

if(TARGET UICreatorEditorTests)
    add_test(NAME UICreatorWindowClose COMMAND UICreatorEditorTests --window-close)
    set_tests_properties(UICreatorWindowClose PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor")
endif()

if(TARGET UICreatorEditorTests)
    add_executable(UICreatorInspectorTests ${editor_test_sources} "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Inspector.cpp")
    target_link_libraries(UICreatorInspectorTests PRIVATE LVGLRuntime::Runtime UICreatorLanguageModels UICreatorProjectCreation UICreatorImageConversion UICreatorFontConversion UICreatorCodeGeneration LVSerial freetype SDL3::SDL3 spdlog::spdlog_header_only)
    target_compile_definitions(UICreatorInspectorTests PRIVATE USE_TELEMETRY=0)
    if(WIN32)
        target_compile_definitions(UICreatorInspectorTests PRIVATE UNICODE _UNICODE)
        target_link_libraries(UICreatorInspectorTests PRIVATE UICreatorBreakpad comdlg32 ole32 shell32 winhttp)
    else()
        target_link_libraries(UICreatorInspectorTests PRIVATE ${CMAKE_DL_LIBS})
        target_include_directories(UICreatorInspectorTests PRIVATE "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src")
    endif()
    if(MSVC)
        target_compile_options(UICreatorInspectorTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorInspector COMMAND UICreatorInspectorTests)
    set_tests_properties(UICreatorInspector PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor")
    add_executable(UICreatorBootstrapTests "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Bootstrap.cpp")
    target_link_libraries(UICreatorBootstrapTests PRIVATE UICreatorCodeGeneration LVSerial)
    if(MSVC)
        target_compile_options(UICreatorBootstrapTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorBootstrap COMMAND UICreatorBootstrapTests "${CMAKE_BINARY_DIR}/bootstrap-generated")
    set_tests_properties(UICreatorBootstrap PROPERTIES TIMEOUT 120 LABELS "functional;bootstrap")
    add_test(NAME UICreatorBootstrapPalette COMMAND UICreatorEditorTests --bootstrap-palette)
    set_tests_properties(UICreatorBootstrapPalette PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 90 LABELS "functional;bootstrap;editor")
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/BootstrapGenerated/Targets.cmake")
    add_executable(UICreatorSessionTests ${editor_test_sources} "${PROJECT_SOURCE_DIR}/Tests/Integration/Editor/Session.cpp")
    target_link_libraries(UICreatorSessionTests PRIVATE LVGLRuntime::Runtime UICreatorLanguageModels UICreatorProjectCreation UICreatorImageConversion UICreatorFontConversion UICreatorCodeGeneration LVSerial freetype SDL3::SDL3 spdlog::spdlog_header_only)
    target_compile_definitions(UICreatorSessionTests PRIVATE USE_TELEMETRY=0)
    if(WIN32)
        target_compile_definitions(UICreatorSessionTests PRIVATE UNICODE _UNICODE)
        target_link_libraries(UICreatorSessionTests PRIVATE UICreatorBreakpad comdlg32 ole32 shell32 winhttp)
    else()
        target_link_libraries(UICreatorSessionTests PRIVATE ${CMAKE_DL_LIBS})
        target_include_directories(UICreatorSessionTests PRIVATE "${PROJECT_SOURCE_DIR}/3rdParty/Breakpad/src")
    endif()
    if(MSVC)
        target_compile_options(UICreatorSessionTests PRIVATE /utf-8)
    endif()
    add_test(NAME UICreatorSession COMMAND UICreatorSessionTests "${CMAKE_BINARY_DIR}/session-tests")
    set_tests_properties(UICreatorSession PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 45 LABELS "functional;editor")
endif()

if(BUILD_TESTING AND UNIX AND NOT APPLE)
    add_executable(UICreatorNativeFileDialogTests Utils/NativeFileDialog.cpp
        "${PROJECT_SOURCE_DIR}/Tests/Integration/NativeFileDialog/Dialog.cpp")
    target_compile_features(UICreatorNativeFileDialogTests PRIVATE cxx_std_17)
    add_test(NAME UICreatorNativeFileDialog COMMAND UICreatorNativeFileDialogTests
        "${CMAKE_BINARY_DIR}/native-file-dialog-tests-work")
    set_tests_properties(UICreatorNativeFileDialog PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE LABELS "functional;editor;file-dialog")
    add_test(NAME UICreatorFileBrowser COMMAND UICreatorEditorTests --file-browser)
    set_tests_properties(UICreatorFileBrowser PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 30 LABELS "functional;editor;linux")
    if(CMAKE_CROSSCOMPILING AND CMAKE_CROSSCOMPILING_EMULATOR)
        # The complete authoring/conversion workflow also runs target Node and
        # Python children through the emulator, in addition to the editor.
        set_tests_properties(UICreatorEditorWorkflow PROPERTIES TIMEOUT 180)
    endif()
endif()

if(BUILD_TESTING)
    include("${PROJECT_SOURCE_DIR}/Tests/Integration/Figma/Register.cmake")
endif()

if(APPLE)
    add_library(UICreatorMacPlatform STATIC Utils/NativeFileDialogMac.mm)
    target_compile_features(UICreatorMacPlatform PRIVATE cxx_std_17)
    target_compile_options(UICreatorMacPlatform PRIVATE -fobjc-arc)
    target_link_libraries(UICreatorMacPlatform PUBLIC "-framework Cocoa" "-framework UniformTypeIdentifiers")
    foreach(editor_target UICreator UICreatorEditorTests UICreatorInspectorTests UICreatorSessionTests)
        if(TARGET ${editor_target})
            target_link_libraries(${editor_target} PRIVATE UICreatorMacPlatform)
        endif()
    endforeach()
endif()

foreach(font_consumer UICreator UICreatorEditorTests UICreatorInspectorTests UICreatorSessionTests)
    if(TARGET ${font_consumer})
        add_dependencies(${font_consumer} UICreatorBuiltinFontAssets)
    endif()
endforeach()
