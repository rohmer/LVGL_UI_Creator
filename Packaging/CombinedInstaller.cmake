# Build the two native payloads with UICreatorWindowsBoth first. Packaging is
# separate so a release never silently substitutes a single-architecture build.
if(WIN32)
    set(UICREATOR_INSTALLER_STAGE_ROOT "${PROJECT_SOURCE_DIR}/build/windows-installer/payload" CACHE PATH "Verified x64/x86 application payloads")
    set(UICREATOR_WINDOWS_PACKAGE_BUILD_ROOT "${PROJECT_SOURCE_DIR}/build/windows-installer/cpack" CACHE PATH "Windows CPack configuration directories")
    set(UICREATOR_WINDOWS_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/build/windows-installer/packages" CACHE PATH "Windows installer output directory")
    set(UICREATOR_NSIS_EXECUTABLE "" CACHE FILEPATH "Optional portable NSIS makensis.exe compiler")
    set(uicreator_nsis_arguments)
    if(UICREATOR_NSIS_EXECUTABLE)
        list(APPEND uicreator_nsis_arguments -NsisExecutable "${UICREATOR_NSIS_EXECUTABLE}")
    endif()
    add_custom_target(UICreatorWindowsInstaller
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_LIST_DIR}/BuildInstaller.ps1"
            -StageRoot "${UICREATOR_INSTALLER_STAGE_ROOT}"
            -BuildDirectory "${UICREATOR_WINDOWS_PACKAGE_BUILD_ROOT}"
            -OutputDirectory "${UICREATOR_WINDOWS_PACKAGE_DIRECTORY}"
            -Version "${PROJECT_VERSION}"
            -CMakeExecutable "${CMAKE_COMMAND}"
            ${uicreator_nsis_arguments}
        USES_TERMINAL VERBATIM)
endif()
