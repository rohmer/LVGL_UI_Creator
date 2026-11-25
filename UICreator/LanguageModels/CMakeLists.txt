if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.24)
    project(LVGLCreatorLanguageModels LANGUAGES CXX)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/AdvancedKeyboardDependency.cmake")

# Only LanguageModelWindow and ModelPlayground are Creator-specific sources.
# Build the maintained native trainer/model codec from the pinned dependency.
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(ADVKBD_NATIVE_HELPER_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
    set(ADVKBD_NATIVE_HELPER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(NOT TARGET UICreatorLanguageModels)
    add_subdirectory("${ADVKBD_SOURCE_ROOT}/tools/NativeTrieGenerator"
        "${CMAKE_CURRENT_BINARY_DIR}/advanced-keyboard-trainer")
endif()
