cmake_minimum_required(VERSION 3.0.0)

project(imagequant
  LANGUAGES C
  VERSION 2.12.2)

include_directories(${CMAKE_CURRENT_LIST_DIR})

option(BUILD_WITH_SSE "Use SSE" ON)
option(BUILD_WITH_OPENMP "Use OpenMP" OFF)

if(NOT IOS AND NOT ANDROID)
  if(BUILD_WITH_SSE)
    if(MSVC)
      add_definitions(/arch:SSE2)
      add_definitions(-DUSE_SSE=1)
    else(MSVC)
      add_definitions(-msse2)
      add_definitions(-DUSE_SSE=1)
    endif(MSVC)
  endif(BUILD_WITH_SSE)
  if(BUILD_WITH_OPENMP)
    find_package(OpenMP)
    if(OPENMP_FOUND)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif()
  endif(BUILD_WITH_OPENMP)
endif(NOT IOS AND NOT ANDROID)

set(IMAGEQUANT_SRC
  blur.c
  kmeans.c
  libimagequant.c
  mediancut.c
  mempool.c
  nearest.c
  pam.c)

set(IMAGEQUANT_PUBLIC_HEADERS
  libimagequant.h)

add_library(imagequant ${IMAGEQUANT_SRC} ${IMAGEQUANT_PUBLIC_HEADERS})

### Install (https://github.com/forexample/package-example) ###
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(include_install_dir "include")

set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY AnyNewerVersion
)

# Note: use 'targets_export_name'
configure_package_config_file(
    "cmake/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

install(
    TARGETS imagequant
    EXPORT "${targets_export_name}"
    INCLUDES DESTINATION "${include_install_dir}"
    RUNTIME DESTINATION "bin"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
)

install(
    DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/
    DESTINATION include/imagequant
    FILES_MATCHING PATTERN "libimagequant.h"
)

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
