cmake_minimum_required(VERSION 3.24)

find_program(IMAGEQUANT_CARGO cargo HINTS "${PROJECT_SOURCE_DIR}/build/cargo/bin" "$ENV{CARGO_HOME}/bin" REQUIRED)
set(cargo_environment)
if(EXISTS "${PROJECT_SOURCE_DIR}/build/rustup/settings.toml")
    list(APPEND cargo_environment
        "RUSTUP_HOME=${PROJECT_SOURCE_DIR}/build/rustup"
        "CARGO_HOME=${PROJECT_SOURCE_DIR}/build/cargo")
endif()
set(cargo_target_directory "${CMAKE_CURRENT_BINARY_DIR}/cargo")
set(cargo_target_arguments)
if(WIN32)
    if(CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
        set(cargo_target aarch64-pc-windows-msvc)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(cargo_target i686-pc-windows-msvc)
    else()
        set(cargo_target x86_64-pc-windows-msvc)
    endif()
    list(APPEND cargo_target_arguments --target "${cargo_target}")
    set(imagequant_archive "${cargo_target_directory}/${cargo_target}/release/imagequant_sys.lib")
elseif(UNIX AND CMAKE_CROSSCOMPILING)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(cargo_target aarch64-unknown-linux-gnu)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(cargo_target x86_64-unknown-linux-gnu)
    else()
        message(FATAL_ERROR "No libimagequant Rust target for ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    list(APPEND cargo_target_arguments --target "${cargo_target}")
    set(imagequant_archive "${cargo_target_directory}/${cargo_target}/release/libimagequant_sys.a")
else()
    set(imagequant_archive "${cargo_target_directory}/release/libimagequant_sys.a")
endif()
file(GLOB_RECURSE imagequant_sources CONFIGURE_DEPENDS "src/*.rs" "imagequant-sys/src/*.rs")
add_custom_command(OUTPUT "${imagequant_archive}"
    COMMAND "${CMAKE_COMMAND}" -E env ${cargo_environment}
        "${IMAGEQUANT_CARGO}" build --release --locked ${cargo_target_arguments}
        --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/imagequant-sys/Cargo.toml"
        --target-dir "${cargo_target_directory}"
    DEPENDS ${imagequant_sources} Cargo.toml Cargo.lock imagequant-sys/Cargo.toml imagequant-sys/build.rs
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)
add_custom_target(imagequant_rust DEPENDS "${imagequant_archive}")
add_library(imagequant STATIC IMPORTED GLOBAL)
set_target_properties(imagequant PROPERTIES
    IMPORTED_LOCATION "${imagequant_archive}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/imagequant-sys")
add_dependencies(imagequant imagequant_rust)
if(WIN32)
    set_property(TARGET imagequant PROPERTY INTERFACE_LINK_LIBRARIES "bcrypt;userenv;ws2_32;ntdll;advapi32;synchronization")
elseif(UNIX)
    find_package(Threads REQUIRED)
    set_property(TARGET imagequant PROPERTY INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS};m")
endif()
