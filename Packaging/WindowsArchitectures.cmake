# Select the compiler architecture before project() with Visual Studio. Ninja
# builds use the matching Developer Command Prompt (or BuildWindows.ps1).
if(CMAKE_HOST_WIN32)
    if(NOT DEFINED UICREATOR_WINDOWS_ARCH)
        if(EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt")
            set(UICREATOR_WINDOWS_ARCH auto CACHE STRING "Windows application architecture")
        else()
            set(UICREATOR_WINDOWS_ARCH x64 CACHE STRING "Windows application architecture")
        endif()
    endif()
    set_property(CACHE UICREATOR_WINDOWS_ARCH PROPERTY STRINGS x64 x86 auto)
    if(UICREATOR_WINDOWS_ARCH STREQUAL "x32")
        set(UICREATOR_WINDOWS_ARCH x86 CACHE STRING "Windows application architecture" FORCE)
    endif()
    if(NOT UICREATOR_WINDOWS_ARCH MATCHES "^(x64|x86|auto)$")
        message(FATAL_ERROR "UICREATOR_WINDOWS_ARCH must be x64 (default), x86 (32-bit), or auto")
    endif()
    if(CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT CMAKE_GENERATOR_PLATFORM AND NOT UICREATOR_WINDOWS_ARCH STREQUAL "auto")
        if(UICREATOR_WINDOWS_ARCH STREQUAL "x86")
            set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Visual Studio target platform" FORCE)
        else()
            set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "Visual Studio target platform" FORCE)
        endif()
    endif()
endif()

function(uicreator_validate_windows_architecture)
    if(NOT WIN32)
        return()
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(actual x64)
        set(CPACK_SYSTEM_NAME win64 PARENT_SCOPE)
    else()
        set(actual x86)
        set(CPACK_SYSTEM_NAME win32 PARENT_SCOPE)
    endif()
    if(NOT UICREATOR_WINDOWS_ARCH STREQUAL "auto" AND NOT UICREATOR_WINDOWS_ARCH STREQUAL actual)
        message(FATAL_ERROR "Requested ${UICREATOR_WINDOWS_ARCH}, but this compiler targets ${actual}. Use a separate build folder and the matching Developer Command Prompt, or Packaging/BuildWindows.ps1 -Architecture ${UICREATOR_WINDOWS_ARCH}.")
    endif()
    set(UICREATOR_NATIVE_ARCH "${actual}" CACHE INTERNAL "Verified native binary architecture" FORCE)
    message(STATUS "LVGL UI Creator application architecture: ${actual}")
    set(UICREATOR_WINDOWS_BUILD_ROOT "${PROJECT_SOURCE_DIR}/build/windows-architectures" CACHE PATH "Native x64/x86 build directories")
    set(UICREATOR_INSTALLER_STAGE_ROOT "${PROJECT_SOURCE_DIR}/build/windows-installer/payload" CACHE PATH "Verified x64/x86 application payloads")
    set(UICREATOR_RELEASE_SDK_DIRECTORY "${PROJECT_SOURCE_DIR}/build/professional/packages" CACHE PATH "Published SDK archives included in release packages")
    set(UICREATOR_RELEASE_CONFIGURATION Release CACHE STRING "Configuration built for release packages")
    set_property(CACHE UICREATOR_RELEASE_CONFIGURATION PROPERTY STRINGS Release RelWithDebInfo Debug)
    set(UICREATOR_RELEASE_JOBS 6 CACHE STRING "Parallel native compilation jobs for release packages")
    add_custom_target(UICreatorWindowsBoth
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BuildWindows.ps1"
            -Architecture both -CMakeExecutable "${CMAKE_COMMAND}"
            -BuildRoot "${UICREATOR_WINDOWS_BUILD_ROOT}"
            -StageRoot "${UICREATOR_INSTALLER_STAGE_ROOT}"
            -SdkDirectory "${UICREATOR_RELEASE_SDK_DIRECTORY}"
            -Configuration "${UICREATOR_RELEASE_CONFIGURATION}"
            -Jobs "${UICREATOR_RELEASE_JOBS}"
        USES_TERMINAL VERBATIM)
endfunction()
