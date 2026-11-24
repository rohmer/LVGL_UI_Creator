#include "NativeApng.h"
#ifndef NATIVE_APNG_STANDALONE
#include "ImageConverter.h"
#include <Serialization/Base64.h>
#endif
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using Bytes = std::vector<uint8_t>;
#define CHECK(value) do { if (!(value)) throw std::runtime_error("APNG check failed at " + std::to_string(__LINE__) + ": " #value); } while (0)
uint32_t Word(const Bytes& bytes, size_t at) { CHECK(at + 4 <= bytes.size()); return (uint32_t(bytes[at]) << 24) | (uint32_t(bytes[at + 1]) << 16) | (uint32_t(bytes[at + 2]) << 8) | bytes[at + 3]; }
void Set(Bytes& bytes, size_t at, uint32_t value) { CHECK(at + 4 <= bytes.size()); for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (24 - i * 8)); }
void Big(Bytes& bytes, uint32_t value) { for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(uint8_t(value >> shift)); }
void Short(Bytes& bytes, uint16_t value) { bytes.push_back(uint8_t(value >> 8)); bytes.push_back(uint8_t(value)); }
uint32_t Crc(const uint8_t* bytes, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) { crc ^= bytes[i]; for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0); }
    return crc ^ 0xffffffffu;
}
void Chunk(Bytes& bytes, const char* type, const Bytes& data) {
    Big(bytes, uint32_t(data.size())); const auto start = bytes.size(); bytes.insert(bytes.end(), type, type + 4); bytes.insert(bytes.end(), data.begin(), data.end()); Big(bytes, Crc(bytes.data() + start, data.size() + 4));
}
// Independent fixture encoder: filter-zero scanlines, zlib with DEFLATE stored
// blocks and an Adler checksum. It does not call either production PNG codec.
Bytes Deflate(const Bytes& raw) {
    Bytes encoded{0x78, 0x01}; size_t at = 0;
    do {
        const auto count = (std::min)(size_t(65535), raw.size() - at);
        encoded.push_back(at + count == raw.size() ? 1 : 0); encoded.push_back(uint8_t(count)); encoded.push_back(uint8_t(count >> 8));
        encoded.push_back(uint8_t(~count)); encoded.push_back(uint8_t((~count) >> 8));
        encoded.insert(encoded.end(), raw.begin() + at, raw.begin() + at + count); at += count;
    } while (at < raw.size());
    uint32_t first = 1, second = 0; for (auto byte : raw) { first = (first + byte) % 65521; second = (second + first) % 65521; }
    Big(encoded, (second << 16) | first); return encoded;
}
Bytes Scanlines(const Bytes& pixels, uint32_t width, uint32_t height, unsigned bitsPerPixel, bool interlaced) {
    Bytes raw;
    if (!interlaced) {
        const size_t rowBytes = (size_t(width) * bitsPerPixel + 7) / 8; CHECK(pixels.size() == rowBytes * height);
        for (uint32_t y = 0; y < height; ++y) { raw.push_back(0); raw.insert(raw.end(), pixels.begin() + size_t(y) * rowBytes, pixels.begin() + size_t(y + 1) * rowBytes); }
    } else {
        CHECK(bitsPerPixel == 32 && pixels.size() == size_t(width) * height * 4);
        constexpr unsigned x0[] = {0,4,0,2,0,1,0}, y0[] = {0,0,4,0,2,0,1}, dx[] = {8,8,4,4,2,2,1}, dy[] = {8,8,8,4,4,2,2};
        for (unsigned pass = 0; pass < 7; ++pass) if (x0[pass] < width && y0[pass] < height)
            for (uint32_t y = y0[pass]; y < height; y += dy[pass]) { raw.push_back(0); for (uint32_t x = x0[pass]; x < width; x += dx[pass]) raw.insert(raw.end(), pixels.begin() + (size_t(y) * width + x) * 4, pixels.begin() + (size_t(y) * width + x + 1) * 4); }
    }
    return Deflate(raw);
}
struct Spec { uint32_t width, height, x = 0, y = 0; uint16_t num = 1, den = 10; uint8_t dispose = 0, blend = 0; Bytes samples; };
Bytes Build(uint32_t width, uint32_t height, const std::vector<Spec>& frames, const Bytes* poster = nullptr,
            uint8_t color = 6, uint8_t depth = 8, const Bytes& palette = {}, const Bytes& transparent = {}, bool interlaced = false) {
    Bytes bytes{137,80,78,71,13,10,26,10}, header;
    Big(header, width); Big(header, height); header.insert(header.end(), {depth,color,0,0,uint8_t(interlaced)}); Chunk(bytes, "IHDR", header);
    Bytes animation; Big(animation, uint32_t(frames.size())); Big(animation, 0); Chunk(bytes, "acTL", animation);
    if (!palette.empty()) Chunk(bytes, "PLTE", palette);
    if (!transparent.empty()) Chunk(bytes, "tRNS", transparent);
    const unsigned channels = color == 0 || color == 3 ? 1 : color == 2 ? 3 : color == 4 ? 2 : 4;
    const auto bitsPerPixel = unsigned(depth) * channels;
    uint32_t sequence = 0;
    auto data = [&](const Bytes& compressed, bool idat) {
        const size_t split = (std::min)(size_t(3), compressed.size());
        for (unsigned index = 0; index < 2; ++index) {
            const auto first = index ? split : 0, last = index ? compressed.size() : split;
            Bytes payload;
            if (!idat) Big(payload, sequence++);
            payload.insert(payload.end(), compressed.begin() + first, compressed.begin() + last);
            Chunk(bytes, idat ? "IDAT" : "fdAT", payload);
        }
    };
    if (poster) data(Scanlines(*poster, width, height, bitsPerPixel, interlaced), true);
    for (size_t i = 0; i < frames.size(); ++i) {
        const auto& frame = frames[i]; Bytes control; Big(control, sequence++); Big(control, frame.width); Big(control, frame.height); Big(control, frame.x); Big(control, frame.y);
        Short(control, frame.num); Short(control, frame.den); control.push_back(frame.dispose); control.push_back(frame.blend); Chunk(bytes, "fcTL", control);
        data(Scanlines(frame.samples, frame.width, frame.height, bitsPerPixel, interlaced), !poster && i == 0);
    }
    Chunk(bytes, "IEND", {}); return bytes;
}
Bytes Fill(uint32_t count, std::array<uint8_t,4> color) { Bytes bytes; for (uint32_t i = 0; i < count; ++i) bytes.insert(bytes.end(), color.begin(), color.end()); return bytes; }
void Pixel(const NativeApng::Frame& frame, uint32_t x, uint32_t y, std::array<uint8_t,4> expected) {
    CHECK(x < frame.width && y < frame.height); const auto at = (size_t(y) * frame.width + x) * 4;
    for (size_t i = 0; i < 4; ++i) CHECK(frame.pixels.at(at + i) == expected[i]);
}
size_t Find(const Bytes& bytes, const char* type, unsigned index = 0) {
    for (size_t at = 8; at + 12 <= bytes.size(); at += size_t(Word(bytes, at)) + 12)
        if (std::equal(type, type + 4, bytes.begin() + at + 4) && index-- == 0) return at;
    throw std::runtime_error("Fixture chunk not found");
}
void Checksum(Bytes& bytes, size_t at) { const auto size = Word(bytes, at); Set(bytes, at + size + 8, Crc(bytes.data() + at + 4, size_t(size) + 4)); }
template<class Mutate> void Reject(const Bytes& valid, Mutate mutate) {
    auto bytes = valid; mutate(bytes); bool rejected = false;
    try { (void)NativeApng::Decode(bytes); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}
void Write(const std::filesystem::path& path, const Bytes& bytes) { std::ofstream file(path, std::ios::binary); CHECK(file); file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); CHECK(file); }
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) return 2;
        const std::filesystem::path output = argv[2]; std::filesystem::create_directories(output);
        // Source/over blends, partial rectangles and all three disposal modes.
        auto animation = Build(3, 2, {
            {3,2,0,0,1,10,0,0,Fill(6,{255,0,0,255})},
            {1,1,1,0,1,4,2,0,Fill(1,{0,0,255,255})},
            {1,1,0,1,0,0,1,1,Fill(1,{0,255,0,128})},
            {1,1,2,1,3,2,0,1,Fill(1,{0,0,255,128})},
            {1,1,0,0,1,100,0,0,Fill(1,{20,30,40,0})}
        });
        CHECK(NativeApng::IsAnimated(animation)); auto frames = NativeApng::Decode(animation); CHECK(frames.size() == 5);
        CHECK(frames[0].delayMs == 100 && frames[1].delayMs == 250 && frames[2].delayMs == 20 && frames[3].delayMs == 1500 && frames[4].delayMs == 20);
        Pixel(frames[1],1,0,{0,0,255,255}); Pixel(frames[2],1,0,{255,0,0,255}); Pixel(frames[2],0,1,{127,128,0,255});
        Pixel(frames[3],0,1,{0,0,0,0}); Pixel(frames[3],2,1,{127,0,128,255}); Pixel(frames[4],0,0,{20,30,40,0});
        // A standalone default image remains a visible poster, but is not used
        // as the background of the first actual animation frame.
        const auto poster = Fill(2,{255,255,0,255});
        auto withPoster = Build(2,1,{{1,1,1,0,2,0,2,0,Fill(1,{0,255,0,255})},{1,1,0,0,1,3,0,1,Fill(1,{255,0,0,128})}},&poster);
        auto posted = NativeApng::Decode(withPoster); CHECK(posted.size() == 3); CHECK(posted[0].delayMs == 100 && posted[1].delayMs == 20 && posted[2].delayMs == 333);
        Pixel(posted[0],0,0,{255,255,0,255}); Pixel(posted[1],0,0,{0,0,0,0}); Pixel(posted[1],1,0,{0,255,0,255}); Pixel(posted[2],0,0,{255,0,0,128}); Pixel(posted[2],1,0,{0,0,0,0});
        auto translucent=NativeApng::Decode(Build(1,1,{{1,1,0,0,1,10,0,0,Fill(1,{255,0,0,128})},{1,1,0,0,1,10,0,1,Fill(1,{0,0,255,128})}}));Pixel(translucent[1],0,0,{85,0,170,192});
        // 16-bit RGB/RGBA/gray, palette transparency and packed gray sample
        // types preserve their normal PNG decoding before composition.
        auto rgb16 = NativeApng::Decode(Build(1,1,{{1,1,0,0,1,10,0,0,{0x12,0x34,0x56,0x78,0x9a,0xbc}}},nullptr,2,16)); Pixel(rgb16[0],0,0,{0x12,0x56,0x9a,255});
        auto rgba16 = NativeApng::Decode(Build(1,1,{{1,1,0,0,1,10,0,0,{0xab,0xcd,0x23,0x45,0x67,0x89,0x80,0x00}}},nullptr,6,16)); Pixel(rgba16[0],0,0,{0xab,0x23,0x67,0x80});
        auto gray16 = NativeApng::Decode(Build(1,1,{{1,1,0,0,1,10,0,0,{0x12,0x34}}},nullptr,0,16,{}, {0x12,0x34})); Pixel(gray16[0],0,0,{0x12,0x12,0x12,0});
        auto grayAlpha = NativeApng::Decode(Build(1,1,{{1,1,0,0,1,10,0,0,{70,80}}},nullptr,4,8)); Pixel(grayAlpha[0],0,0,{70,70,70,80});
        auto palette = NativeApng::Decode(Build(2,1,{{2,1,0,0,1,10,0,0,{0x10}}},nullptr,3,2,{255,0,0,0,255,0},{255,128})); Pixel(palette[0],0,0,{255,0,0,255}); Pixel(palette[0],1,0,{0,255,0,128});
        auto gray1 = NativeApng::Decode(Build(2,1,{{2,1,0,0,1,10,0,0,{0x40}}},nullptr,0,1)); Pixel(gray1[0],0,0,{0,0,0,255}); Pixel(gray1[0],1,0,{255,255,255,255});
        Bytes interlace; for (unsigned i = 0; i < 12; ++i) { const auto color = Fill(1,{uint8_t(i*17),uint8_t(255-i*13),uint8_t(i*9),255}); interlace.insert(interlace.end(),color.begin(),color.end()); }
        auto adam7 = NativeApng::Decode(Build(4,3,{{4,3,0,0,1,10,0,0,interlace}},nullptr,6,8,{}, {},true)); CHECK(adam7[0].pixels == interlace);
        // Structural corruption must fail instead of flattening to IDAT.
        Reject(animation,[](Bytes& b){b[Find(b,"fdAT")+12]^=1;});
        Reject(animation,[](Bytes& b){auto at=Find(b,"fdAT");Set(b,at+8,99);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"fdAT");b[at+12]=0;Checksum(b,at);});
        Reject(animation,[](Bytes& b){b.pop_back();});
        Reject(animation,[](Bytes& b){Set(b,Find(b,"fcTL"),0x7fffffffu);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"acTL");Set(b,at+8,2);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"fcTL",1);Set(b,at+20,3);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"fcTL",1);b[at+32]=3;Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"fcTL",1);b[at+33]=2;Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"IHDR");Set(b,at+8,4097);Set(b,at+12,4097);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"IHDR");Set(b,at+8,4096);Set(b,at+12,4096);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"acTL");Set(b,at+8,0xffffffffu);Checksum(b,at);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"IDAT",1);Bytes middle;Chunk(middle,"tEXt",{'x',0,'y'});b.insert(b.begin()+at,middle.begin(),middle.end());});
        Reject(animation,[](Bytes& b){auto at=Find(b,"acTL");b.erase(b.begin()+at,b.begin()+at+20);});
        Reject(animation,[](Bytes& b){auto at=Find(b,"IEND");b.erase(b.begin()+at,b.end());});
        Reject(animation,[](Bytes& b){b.push_back(0);});
        std::atomic<bool> cancelled{true}; bool stopped=false;try{(void)NativeApng::Decode(animation,&cancelled);}catch(const std::exception&){stopped=true;}CHECK(stopped);
        Write(output/"native.png",animation);Write(output/"poster.png",withPoster);
#ifndef NATIVE_APNG_STANDALONE
        // Real converter integration: animation metadata and each composed
        // device frame must survive the normal native export pipeline.
        auto converted=ImageConversion::Convert({output/"native.png"},{{"colorFormat","ARGB8888"},{"dither",false}},output/"export");
        CHECK(converted.success && converted.assets.size()==1);const auto& asset=converted.assets[0];CHECK(asset["deviceFrames"].size()==frames.size());
        for(size_t i=0;i<frames.size();++i){CHECK(asset["delays"][i]==frames[i].delayMs);const auto text=base64_decode(asset["deviceFrames"][i]["data"]);CHECK(text.size()==frames[i].pixels.size());for(size_t pixel=0;pixel<frames[i].pixels.size();pixel+=4){CHECK(uint8_t(text[pixel])==frames[i].pixels[pixel+2]);CHECK(uint8_t(text[pixel+1])==frames[i].pixels[pixel+1]);CHECK(uint8_t(text[pixel+2])==frames[i].pixels[pixel]);CHECK(uint8_t(text[pixel+3])==frames[i].pixels[pixel+3]);}}
        auto posterConverted=ImageConversion::Convert({output/"poster.png"},{});CHECK(posterConverted.success&&posterConverted.assets[0]["deviceFrames"].size()==3);
#endif
        std::cout<<"Native APNG composition, default image, formats, CRC/order, bounds and cancellation passed without lv_init\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
