file(GLOB test_sources CONFIGURE_DEPENDS "*.cpp")

add_executable(ImgLibUnitTests ${test_sources})
target_link_libraries(ImgLibUnitTests PRIVATE ImgLib UICreatorCatch)
add_test(NAME ImgLibUnitTests COMMAND ImgLibUnitTests)
set_tests_properties(ImgLibUnitTests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
