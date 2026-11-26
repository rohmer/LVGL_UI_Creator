cmake_minimum_required(VERSION 3.24)
project(UICreatorVisualStudioConverters LANGUAGES C CXX)
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This adapter builds native MSVC dependencies for the checked-in Visual Studio solution")
endif()
get_filename_component(UICREATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# Existing native dependency definitions resolve vendored sources relative to
# the Creator project. Keep this standalone adapter free of editor/tool assets.
set(PROJECT_SOURCE_DIR "${UICREATOR_ROOT}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(BUILD_SHARED_LIBS OFF)
set(SKIP_INSTALL_ALL ON)
set(SKIP_INSTALL_LIBRARIES ON)
set(FT_DISABLE_ZLIB ON CACHE BOOL "Use FreeType's bundled zlib" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "No external bzip2 dependency" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "No external PNG dependency" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "No external HarfBuzz dependency" FORCE)
set(FT_DISABLE_BROTLI OFF CACHE BOOL "Preserve WOFF2 support" FORCE)
set(FT_REQUIRE_BROTLI ON CACHE BOOL "Require the vendored WOFF2 decoder" FORCE)
add_subdirectory("${UICREATOR_ROOT}/3rdParty/brotli" brotli EXCLUDE_FROM_ALL)
add_subdirectory("${UICREATOR_ROOT}/3rdParty/freetype-2.15.0" freetype EXCLUDE_FROM_ALL)
add_subdirectory("${UICREATOR_ROOT}/3rdParty/libimagequant" imagequant EXCLUDE_FROM_ALL)
add_subdirectory("${UICREATOR_ROOT}/3rdParty/resvg" resvg EXCLUDE_FROM_ALL)
add_subdirectory("${UICREATOR_ROOT}/ImgLib" image-codecs EXCLUDE_FROM_ALL)

set(stage "${CMAKE_BINARY_DIR}/$<CONFIG>/lib")
add_custom_target(UICreatorVSNativeConverterLibraries
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${stage}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:UICreatorNativePng>" "${stage}/UICreatorNativePng.lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:freetype>" "${stage}/UICreatorNativeFreeType.lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:UICreatorBrotliDecoder>" "${stage}/UICreatorBrotliDecoder.lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:UICreatorBrotliCommon>" "${stage}/UICreatorBrotliCommon.lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:imagequant>" "${stage}/UICreatorImageQuant.lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:UICreatorResvg>" "${stage}/UICreatorResvg.lib"
    DEPENDS UICreatorNativePng freetype UICreatorBrotliDecoder UICreatorBrotliCommon imagequant_rust UICreatorResvgBuild
    VERBATIM)

option(UICREATOR_VS_VERIFY_CONVERTER_LIBRARIES "Build and run the native dependency ABI checks" OFF)
if(UICREATOR_VS_VERIFY_CONVERTER_LIBRARIES)
    add_executable(UICreatorVSConverterDependencyCheck Verify.cpp)
    target_compile_features(UICreatorVSConverterDependencyCheck PRIVATE cxx_std_17)
    target_compile_options(UICreatorVSConverterDependencyCheck PRIVATE /utf-8)
    target_link_libraries(UICreatorVSConverterDependencyCheck PRIVATE freetype UICreatorNativePng imagequant UICreatorResvg)
    add_dependencies(UICreatorVSNativeConverterLibraries UICreatorVSConverterDependencyCheck)
    enable_testing()
    add_test(NAME NativeConverterDependencies COMMAND UICreatorVSConverterDependencyCheck
        "${UICREATOR_ROOT}/Tests/Integration/FontFixtures/UICreatorFontFixture.woff2")
endif()
