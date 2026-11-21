#define CATCH_CONFIG_RUNNER

#include "../../../3rdParty/Catch2/single_include/catch2/catch.hpp"

int main(int argc, char* argv[])
{
    int result = Catch::Session().run(argc, argv);

    // global clean-up...

    return result;
}
