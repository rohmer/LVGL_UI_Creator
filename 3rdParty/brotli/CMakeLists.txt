cmake_minimum_required(VERSION 3.24)

# Decoder-only, pinned native dependency for FreeType's WOFF2 reader. No
# encoder, executable, interpreter, download or host-library lookup is needed.
add_library(UICreatorBrotliCommon STATIC
    c/common/constants.c c/common/context.c c/common/dictionary.c
    c/common/platform.c c/common/shared_dictionary.c c/common/transform.c)
add_library(UICreatorBrotliDecoder STATIC
    c/dec/bit_reader.c c/dec/decode.c c/dec/huffman.c
    c/dec/prefix.c c/dec/state.c c/dec/static_init.c)
foreach(target UICreatorBrotliCommon UICreatorBrotliDecoder)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/c/include")
    target_compile_definitions(${target} PUBLIC BROTLI_STATIC)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE ON)
endforeach()
target_link_libraries(UICreatorBrotliDecoder PUBLIC UICreatorBrotliCommon)

# FreeType appends its own finder directory. Prepending our finder ensures
# native and cross builds select the same vendored decoder and capabilities.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
