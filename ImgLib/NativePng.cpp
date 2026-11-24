#include "NativePng.h"
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

extern "C" unsigned uicreator_png_encode32(unsigned char**, size_t*, const unsigned char*, unsigned, unsigned);
extern "C" const char* uicreator_png_error_text(unsigned);

std::vector<uint8_t> NativePng::Encode(const uint8_t* rgba, uint32_t width, uint32_t height)
{
    if (!rgba || !width || !height || uint64_t(width) * height > 16u * 1024u * 1024u)
        throw std::invalid_argument("Invalid PNG dimensions");
    unsigned char* encoded = nullptr;
    size_t length = 0;
    auto error = uicreator_png_encode32(&encoded, &length, rgba, width, height);
    std::unique_ptr<unsigned char, decltype(&std::free)> owned(encoded, std::free);
    if (error) throw std::runtime_error(std::string("PNG encoding failed: ") + uicreator_png_error_text(error));
    return {encoded, encoded + length};
}
