#ifndef LVSERIAL_DEVICE_PB_H
#define LVSERIAL_DEVICE_PB_H
#include "../Proto/lvserial.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVSERIAL_BINARY_VERSION_LEGACY 1
#define LVSERIAL_BINARY_VERSION 2

/* All callbacks are synchronous. Borrowed metadata/payload expire on return.
 * Read TEXT/BINARY payloads with pb_read(), in chunks or directly into flash.
 * No JSON library, C++ runtime, LVGL dependency, or heap allocation is needed. */
typedef struct {
    uint32_t format_version;
    lvserial_EntityKind kind;
    char type[64];
} lvserial_record_info;
typedef struct {
    lvserial_Property metadata;
    const pb_byte_t* bytes;
    size_t size;
} lvserial_property;
typedef bool (*lvserial_read_property)(void* context, size_t index, lvserial_property* property);
typedef bool (*lvserial_visit_property)(void* context, const lvserial_record_info* record,
    const lvserial_Property* property, pb_istream_t* payload);

bool lvserial_pb_encode(pb_ostream_t* output, const lvserial_record_info* record,
    size_t count, lvserial_read_property read_property, void* context);
bool lvserial_pb_decode(const pb_byte_t* input, size_t size, lvserial_record_info* record,
    lvserial_visit_property visit, void* context, const char** error);
#ifdef __cplusplus
}
#endif
#endif
