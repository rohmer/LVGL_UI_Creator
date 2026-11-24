// Native APNG demux/composition following the W3C PNG Third Edition:
// https://www.w3.org/TR/png-3/#APNG
// The private stb PNG decoder retains its license in ImgLib/stb_image.h.
#include "NativeApng.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace NativeApng::Detail {
constexpr size_t BudgetBytes = 256u * 1024u * 1024u;
struct Budget { size_t used = 0; bool exhausted = false; const std::atomic<bool>* cancel = nullptr; };
thread_local Budget* activeBudget = nullptr;
struct alignas(std::max_align_t) Allocation { size_t bytes; Budget* budget; };
void* Allocate(size_t bytes) {
    auto* budget = activeBudget;
    if (bytes > BudgetBytes - sizeof(Allocation) || (budget && (bytes + sizeof(Allocation) > BudgetBytes - budget->used || (budget->cancel && budget->cancel->load())))) {
        if (budget) budget->exhausted = true;
        return nullptr;
    }
    auto* memory = static_cast<Allocation*>(std::malloc(bytes + sizeof(Allocation)));
    if (!memory) return nullptr;
    memory->bytes = bytes + sizeof(Allocation); memory->budget = budget;
    if (budget) budget->used += memory->bytes;
    return memory + 1;
}
void Free(void* pointer) {
    if (!pointer) return;
    auto* memory = static_cast<Allocation*>(pointer) - 1;
    if (memory->budget) memory->budget->used -= memory->bytes;
    std::free(memory);
}
void* Reallocate(void* pointer, size_t bytes) {
    if (!pointer) return Allocate(bytes);
    if (!bytes) { Free(pointer); return nullptr; }
    auto* old = static_cast<Allocation*>(pointer) - 1;
    auto* budget = old->budget;
    if (bytes > BudgetBytes - sizeof(Allocation) || (budget && (bytes + sizeof(Allocation) > BudgetBytes - (budget->used - old->bytes) || (budget->cancel && budget->cancel->load())))) {
        if (budget) budget->exhausted = true;
        return nullptr;
    }
    const auto previous = old->bytes;
    auto* memory = static_cast<Allocation*>(std::realloc(old, bytes + sizeof(Allocation)));
    if (!memory) return nullptr;
    memory->bytes = bytes + sizeof(Allocation);
    if (budget) budget->used = budget->used - previous + memory->bytes;
    return memory + 1;
}
}
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MALLOC(size) ::NativeApng::Detail::Allocate(size)
#define STBI_REALLOC(pointer,size) ::NativeApng::Detail::Reallocate(pointer,size)
#define STBI_FREE(pointer) ::NativeApng::Detail::Free(pointer)
#include "../../ImgLib/stb_image.h"

namespace NativeApng {
namespace {
using Bytes = std::vector<uint8_t>;
constexpr std::array<uint8_t, 8> Signature = {137,80,78,71,13,10,26,10};
constexpr uint32_t Tag(char a, char b, char c, char d) { return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) | (uint32_t(uint8_t(c)) << 8) | uint8_t(d); }
constexpr auto IHDR = Tag('I','H','D','R'), PLTE = Tag('P','L','T','E'), TRNS = Tag('t','R','N','S'), IDAT = Tag('I','D','A','T'), IEND = Tag('I','E','N','D');
constexpr auto ACTL = Tag('a','c','T','L'), FCTL = Tag('f','c','T','L'), FDAT = Tag('f','d','A','T');
void Cancel(const std::atomic<bool>* cancel) { if (cancel && cancel->load()) throw std::runtime_error("Conversion cancelled"); }
[[noreturn]] void Invalid(const char* message) { throw std::invalid_argument(std::string("Invalid APNG: ") + message); }
uint32_t U32(const uint8_t* data) { return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3]; }
uint16_t U16(const uint8_t* data) { return uint16_t((uint16_t(data[0]) << 8) | data[1]); }
void Put32(Bytes& data, uint32_t value) { for (int shift = 24; shift >= 0; shift -= 8) data.push_back(uint8_t(value >> shift)); }
void Set32(Bytes& data, size_t at, uint32_t value) { for (unsigned i = 0; i < 4; ++i) data[at + i] = uint8_t(value >> (24 - i * 8)); }
constexpr std::array<uint32_t, 256> CrcTable() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < table.size(); ++i) { auto value = i; for (unsigned bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0); table[i] = value; }
    return table;
}
constexpr auto crcTable = CrcTable();
uint32_t Crc(const uint8_t* data, size_t size, const std::atomic<bool>* cancel = nullptr) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) { if ((i & 65535) == 0) Cancel(cancel); crc = crcTable[(crc ^ data[i]) & 255] ^ (crc >> 8); }
    return crc ^ 0xffffffffu;
}
void Chunk(Bytes& output, uint32_t type, const uint8_t* data, size_t size, const std::atomic<bool>* cancel = nullptr) {
    if (size > 0x7fffffffu || size + 12 > Detail::BudgetBytes - output.size()) throw std::invalid_argument("APNG compressed frame exceeds the 256 MiB memory limit");
    Put32(output, uint32_t(size)); const auto start = output.size(); Put32(output, type);
    if (size) output.insert(output.end(), data, data + size);
    Put32(output, Crc(output.data() + start, size + 4, cancel));
}
struct Span { const uint8_t* data = nullptr; size_t size = 0; };
struct Control { uint32_t width = 0, height = 0, x = 0, y = 0, delay = 100; uint8_t dispose = 0, blend = 0; };
struct Record { Control control; bool idat = false; size_t compressed = 0; std::vector<Span> data; };
struct Png {
    Bytes header;
    std::vector<Bytes> shared;
    uint32_t width = 0, height = 0;
    size_t frameBytes = 0;
    bool poster = false;
    Record defaultImage;
    std::vector<Record> frames;
};
bool PngSignature(const Bytes& bytes) { return bytes.size() >= Signature.size() && std::equal(Signature.begin(), Signature.end(), bytes.begin()); }
void Dimensions(uint32_t width, uint32_t height) {
    if (!width || !height || width > 65535 || height > 65535 || uint64_t(width) * height > 16u * 1024u * 1024u) Invalid("image dimensions exceed the 16-megapixel limit");
}
void OutputBudget(size_t frameBytes, uint64_t count) {
    if (!count || count > Detail::BudgetBytes / (frameBytes + sizeof(Frame))) throw std::invalid_argument("Decoded APNG animation exceeds the 256 MiB memory limit");
}
Png Parse(const Bytes& bytes, const std::atomic<bool>* cancel) {
    if (!PngSignature(bytes)) Invalid("missing PNG signature");
    if (bytes.size() > 512u * 1024u * 1024u) Invalid("input exceeds the image file limit");
    Png png; bool actl = false, idat = false, idatEnded = false, end = false, palette = false, transparency = false;
    uint32_t expectedFrames = 0, paletteEntries = 0;
    uint64_t sequence = 0;
    auto checkSequence = [&](const uint8_t* data) { if (sequence > UINT32_MAX || U32(data) != sequence++) Invalid("animation sequence numbers are not consecutive"); };
    size_t metadata = 0;
    auto span = [&](Record& record, const uint8_t* data, size_t size) {
        if (size > Detail::BudgetBytes - record.compressed || metadata > 16u * 1024u * 1024u - sizeof(Span)) throw std::invalid_argument("APNG compressed-frame or chunk-metadata memory limit exceeded");
        metadata += sizeof(Span); record.data.push_back({data, size}); record.compressed += size;
    };
    size_t at = Signature.size();
    while (at < bytes.size()) {
        Cancel(cancel);
        if (bytes.size() - at < 12) Invalid("truncated chunk");
        const auto length = U32(bytes.data() + at), type = U32(bytes.data() + at + 4);
        if (length > 0x7fffffffu || length > bytes.size() - at - 12) Invalid("truncated or oversized chunk");
        const auto* payload = bytes.data() + at + 8;
        for (unsigned i = 0; i < 4; ++i) if (!((bytes[at + 4 + i] >= 'A' && bytes[at + 4 + i] <= 'Z') || (bytes[at + 4 + i] >= 'a' && bytes[at + 4 + i] <= 'z'))) Invalid("invalid chunk type");
        if (Crc(bytes.data() + at + 4, size_t(length) + 4, cancel) != U32(payload + length)) Invalid("chunk CRC mismatch");
        if (png.header.empty() && type != IHDR) Invalid("IHDR must be first");
        if (idat && type != IDAT) idatEnded = true;
        if (type == IHDR) {
            if (!png.header.empty() || length != 13) Invalid("duplicate or malformed IHDR");
            png.width = U32(payload); png.height = U32(payload + 4); Dimensions(png.width, png.height);
            const auto depth = payload[8], color = payload[9];
            const bool validDepth = (color == 0 && (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)) ||
                ((color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16)) || (color == 3 && (depth == 1 || depth == 2 || depth == 4 || depth == 8));
            if (!validDepth || payload[10] || payload[11] || payload[12] > 1) Invalid("unsupported PNG header");
            png.header.assign(payload, payload + length); png.frameBytes = size_t(png.width) * png.height * 4;
        } else if (type == ACTL) {
            if (actl || idat || !png.frames.empty() || length != 8) Invalid("animation control must precede frames and IDAT");
            expectedFrames = U32(payload); OutputBudget(png.frameBytes, expectedFrames); actl = true;
        } else if (type == FCTL) {
            if (!actl || length != 26) Invalid("frame control requires acTL and 26 bytes");
            checkSequence(payload);
            if (!png.frames.empty() && !png.frames.back().compressed) Invalid("frame has no compressed image data");
            if (!idat && !png.frames.empty()) Invalid("multiple frame controls precede IDAT");
            if (png.frames.size() >= expectedFrames) Invalid("too many animation frames");
            Control control; control.width = U32(payload + 4); control.height = U32(payload + 8); control.x = U32(payload + 12); control.y = U32(payload + 16);
            Dimensions(control.width, control.height);
            if (uint64_t(control.x) + control.width > png.width || uint64_t(control.y) + control.height > png.height || payload[24] > 2 || payload[25] > 1) Invalid("invalid frame rectangle, disposal or blend operation");
            control.delay = (std::max)(20u, uint32_t(uint64_t(U16(payload + 20)) * 1000 / (U16(payload + 22) ? U16(payload + 22) : 100)));
            control.dispose = payload[24]; control.blend = payload[25];
            if (!idat && (control.x || control.y || control.width != png.width || control.height != png.height)) Invalid("IDAT animation frame must cover the full canvas");
            if (idat && png.frames.empty()) { png.poster = true; OutputBudget(png.frameBytes, uint64_t(expectedFrames) + 1); }
            if (metadata > 16u * 1024u * 1024u - sizeof(Record)) throw std::invalid_argument("APNG frame-metadata memory limit exceeded");
            metadata += sizeof(Record); png.frames.push_back({control, !idat, 0, {}});
        } else if (type == PLTE || type == TRNS) {
            if (idat) Invalid("palette and transparency must precede IDAT");
            const auto color = png.header[9];
            if (type == PLTE) {
                if (palette || transparency || !length || length % 3 || length > 768 || color == 0 || color == 4) Invalid("invalid palette");
                paletteEntries = length / 3;
                if (color == 3 && paletteEntries > (1u << png.header[8])) Invalid("palette exceeds sample depth");
                palette = true;
            } else {
                if (transparency || (color == 0 && length != 2) || (color == 2 && length != 6) || (color == 3 && (!palette || length > paletteEntries)) || color == 4 || color == 6) Invalid("invalid transparency chunk");
                transparency = true;
            }
            png.shared.emplace_back(bytes.begin() + at, bytes.begin() + at + size_t(length) + 12);
        } else if (type == IDAT) {
            if (!actl || idatEnded || (png.header[9] == 3 && !palette)) Invalid("IDAT ordering or palette is invalid");
            idat = true;
            auto& record = png.frames.empty() ? png.defaultImage : png.frames.front();
            if (!png.frames.empty() && !record.idat) Invalid("IDAT cannot replace fdAT frame data");
            span(record, payload, length);
        } else if (type == FDAT) {
            if (!actl || !idat || png.frames.empty() || png.frames.back().idat || length < 4) Invalid("frame data requires a preceding frame control");
            checkSequence(payload);
            auto& record = png.frames.back(); span(record, payload + 4, length - 4);
        } else if (type == IEND) {
            if (length || !actl || !idat || png.frames.size() != expectedFrames || png.frames.empty() || !png.frames.back().compressed) Invalid("incomplete animation or frame count mismatch");
            if (png.poster && !png.defaultImage.compressed) Invalid("default image is empty");
            if (at + size_t(length) + 12 != bytes.size()) Invalid("data follows IEND");
            end = true; break;
        } else if (!(bytes[at + 4] & 32)) Invalid("unknown critical PNG chunk");
        at += size_t(length) + 12;
    }
    if (!end) Invalid("missing IEND");
    return png;
}
struct BudgetScope {
    Detail::Budget budget;
    Detail::Budget* previous;
    explicit BudgetScope(const std::atomic<bool>* cancel) : previous(Detail::activeBudget) { budget.cancel = cancel; Detail::activeBudget = &budget; }
    ~BudgetScope() { Detail::activeBudget = previous; }
};
void Paint(const Png& png, const Record& record, Bytes& canvas, const std::atomic<bool>* cancel) {
    Cancel(cancel);
    Bytes stream(Signature.begin(), Signature.end()); auto header = png.header;
    Set32(header, 0, record.control.width); Set32(header, 4, record.control.height); Chunk(stream, IHDR, header.data(), header.size(), cancel);
    for (const auto& shared : png.shared) stream.insert(stream.end(), shared.begin(), shared.end());
    for (const auto& part : record.data) { Cancel(cancel); Chunk(stream, IDAT, part.data, part.size, cancel); }
    Chunk(stream, IEND, nullptr, 0, cancel);
    if (stream.size() > INT_MAX) Invalid("compressed frame exceeds decoder limits");
    BudgetScope scope(cancel); int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(stbi_load_from_memory(stream.data(), int(stream.size()), &width, &height, &channels, 4), stbi_image_free);
    if (!pixels) {
        Cancel(cancel);
        if (scope.budget.exhausted) throw std::invalid_argument("APNG frame decoding exceeds the 256 MiB scratch limit");
        throw std::invalid_argument(std::string("Cannot decode APNG frame: ") + (stbi_failure_reason() ? stbi_failure_reason() : "invalid PNG data"));
    }
    if (width != int(record.control.width) || height != int(record.control.height)) Invalid("decoded frame dimensions differ");
    for (uint32_t y = 0; y < record.control.height; ++y) {
        Cancel(cancel);
        auto* dest = canvas.data() + (size_t(record.control.y + y) * png.width + record.control.x) * 4;
        const auto* source = pixels.get() + size_t(y) * record.control.width * 4;
        if (!record.control.blend) { std::memcpy(dest, source, size_t(record.control.width) * 4); continue; }
        for (uint32_t x = 0; x < record.control.width; ++x, source += 4, dest += 4) {
            const unsigned alpha = source[3];
            if (!alpha) continue;
            if (alpha == 255 || !dest[3]) { std::memcpy(dest, source, 4); continue; }
            const unsigned denominator = alpha * 255 + unsigned(dest[3]) * (255 - alpha);
            for (unsigned channel = 0; channel < 3; ++channel) dest[channel] = uint8_t((unsigned(source[channel]) * alpha * 255 + unsigned(dest[channel]) * dest[3] * (255 - alpha) + denominator / 2) / denominator);
            dest[3] = uint8_t((denominator + 127) / 255);
        }
    }
}
Bytes Save(const Png& png, const Control& control, const Bytes& canvas) {
    Bytes previous(size_t(control.width) * control.height * 4);
    for (uint32_t y = 0; y < control.height; ++y) std::memcpy(previous.data() + size_t(y) * control.width * 4, canvas.data() + (size_t(control.y + y) * png.width + control.x) * 4, size_t(control.width) * 4);
    return previous;
}
void Dispose(const Png& png, const Control& control, Bytes& canvas, const Bytes& previous) {
    for (uint32_t y = 0; y < control.height; ++y) {
        auto* row = canvas.data() + (size_t(control.y + y) * png.width + control.x) * 4;
        if (control.dispose == 1 || previous.empty()) std::memset(row, 0, size_t(control.width) * 4);
        else std::memcpy(row, previous.data() + size_t(y) * control.width * 4, size_t(control.width) * 4);
    }
}
}
bool IsAnimated(const std::vector<uint8_t>& bytes) {
    if (!PngSignature(bytes)) return false;
    size_t at = Signature.size();
    while (bytes.size() - at >= 12) {
        const auto length = U32(bytes.data() + at), type = U32(bytes.data() + at + 4);
        if (type == ACTL || type == FCTL || type == FDAT) return true;
        if (length > bytes.size() - at - 12) return false;
        at += size_t(length) + 12;
    }
    return false;
}
std::vector<Frame> Decode(const std::vector<uint8_t>& bytes, const std::atomic<bool>* cancel) {
    Cancel(cancel); const auto png = Parse(bytes, cancel);
    Bytes canvas(png.frameBytes, 0);
    std::vector<Frame> output; output.reserve(png.frames.size() + (png.poster ? 1 : 0));
    if (png.poster) {
        auto poster = png.defaultImage; poster.control = {png.width, png.height, 0, 0, 100, 0, 0};
        Paint(png, poster, canvas, cancel); output.push_back({png.width, png.height, 100, canvas});
        std::fill(canvas.begin(), canvas.end(), 0);
    }
    for (size_t index = 0; index < png.frames.size(); ++index) {
        Cancel(cancel); const auto& record = png.frames[index];
        Bytes previous;
        if (record.control.dispose == 2 && index != 0) previous = Save(png, record.control, canvas);
        Paint(png, record, canvas, cancel);
        output.push_back({png.width, png.height, record.control.delay, canvas});
        if (record.control.dispose) Dispose(png, record.control, canvas, previous);
    }
    return output;
}
}
