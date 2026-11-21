file(GLOB test_sources CONFIGURE_DEPENDS "*.cpp")

add_executable(SerializationUnitTests ${test_sources})
target_link_libraries(SerializationUnitTests PRIVATE ImgLib LVSerial UICreatorCatch)
add_test(NAME SerializationUnitTests COMMAND SerializationUnitTests)
set_tests_properties(SerializationUnitTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    ENVIRONMENT "SDL_VIDEO_DRIVER=dummy"
    TIMEOUT 30)
