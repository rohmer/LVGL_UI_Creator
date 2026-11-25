if(BUILD_TESTING AND TARGET LVGLRuntime::Runtime AND TARGET UICreatorCodeGeneration)
    add_executable(UICreatorRadixAssetTests "${PROJECT_SOURCE_DIR}/Tests/Integration/RadixAssets.cpp")
    target_link_libraries(UICreatorRadixAssetTests PRIVATE UICreatorCodeGeneration LVSerial LVGLRuntime::Runtime)
    if(MSVC)
        target_compile_options(UICreatorRadixAssetTests PRIVATE /utf-8)
    endif()
    set(radix_asset_fixture "${PROJECT_BINARY_DIR}/radix-assets")
    add_test(NAME UICreatorRadixAssets COMMAND UICreatorRadixAssetTests "${radix_asset_fixture}")
    set_tests_properties(UICreatorRadixAssets PROPERTIES TIMEOUT 60
        FIXTURES_SETUP RadixAssets LABELS "functional;advanced-keyboard;prediction;protobuf;xml;radix")
    add_test(NAME UICreatorRadixGenerated COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY_DIR=${PROJECT_BINARY_DIR}/radix-generated"
        "-DEXPORTED_DIR=${radix_asset_fixture}" "-DLVGL_BUILD_DIR=${PROJECT_BINARY_DIR}/version-matrix/9.6"
        "-DPARENT_GENERATOR=${CMAKE_GENERATOR}" -P "${PROJECT_SOURCE_DIR}/Tests/Integration/RadixPrediction/VerifyGenerated.cmake")
    set_tests_properties(UICreatorRadixGenerated PROPERTIES DEPENDS "UICreatorRadixAssets;LVGLGenerator96"
        FIXTURES_REQUIRED RadixAssets TIMEOUT 180 LABELS "functional;firmware;advanced-keyboard;radix")
endif()
