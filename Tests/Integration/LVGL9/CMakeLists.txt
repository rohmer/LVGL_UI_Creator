if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.24)
    project(Lvgl9Integration LANGUAGES C CXX)
    include(CTest)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
set(creator_root "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

if(NOT TARGET SDL3::SDL3)
    set(SDL_SHARED ON CACHE BOOL "")
    set(SDL_STATIC OFF CACHE BOOL "")
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
    set(SDL_TESTS OFF CACHE BOOL "")
    add_subdirectory("${creator_root}/3rdParty/SDL3" sdl3 EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET lvgl)
    set(UICREATOR_USE_LVGL9 ON)
    add_subdirectory("${creator_root}/3rdParty/LVGL" lvgl)
endif()
if(NOT TARGET LVSerial)
    add_subdirectory("${creator_root}/LVSerial" serial EXCLUDE_FROM_ALL)
endif()

add_executable(Lvgl9Integration
    Simulator.cpp
    "${creator_root}/UICreator/Utils/SdlSimulator.cpp"
    "${creator_root}/UICreator/Utils/ObjectTools.cpp"
    "${creator_root}/UICreator/Utils/EditorIcons.cpp"
    "${creator_root}/UICreator/Utils/ObjectIcons.cpp"
    "${creator_root}/UICreator/ObjectCreation/Arc.cpp"
    "${creator_root}/UICreator/ObjectCreation/Bar.cpp"
    "${creator_root}/UICreator/ObjectCreation/Button.cpp"
    "${creator_root}/UICreator/ObjectCreation/ButtonMatrix.cpp"
    "${creator_root}/UICreator/ObjectCreation/Calendar.cpp"
    "${creator_root}/UICreator/ObjectCreation/Canvas.cpp"
    "${creator_root}/UICreator/ObjectCreation/Chart.cpp"
    "${creator_root}/UICreator/ObjectCreation/CheckBox.cpp"
    "${creator_root}/UICreator/ObjectCreation/Container.cpp")
target_link_libraries(Lvgl9Integration PRIVATE LVSerial lvgl::lvgl SDL3::SDL3)
add_test(NAME Lvgl9Integration COMMAND Lvgl9Integration)
set_tests_properties(Lvgl9Integration PROPERTIES
    ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
    TIMEOUT 10)

