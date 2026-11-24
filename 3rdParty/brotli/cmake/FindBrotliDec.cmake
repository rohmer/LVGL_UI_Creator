if(NOT TARGET UICreatorBrotliDecoder)
    message(FATAL_ERROR "Add the vendored Brotli decoder before configuring FreeType")
endif()
get_target_property(BROTLIDEC_INCLUDE_DIRS UICreatorBrotliDecoder INTERFACE_INCLUDE_DIRECTORIES)
set(BROTLIDEC_LIBRARIES UICreatorBrotliDecoder)
set(BROTLIDEC_DEFINITIONS BROTLI_STATIC)
set(BROTLIDEC_VERSION "1.2.1")
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BrotliDec
    REQUIRED_VARS BROTLIDEC_INCLUDE_DIRS BROTLIDEC_LIBRARIES
    FOUND_VAR BROTLIDEC_FOUND VERSION_VAR BROTLIDEC_VERSION)
