#include <Serialization/PNGToArray.h>
#include <catch.hpp>

TEST_CASE("PNG To Array")
{
    std::vector<unsigned char> arr = PNGToArray("Image.png");
}