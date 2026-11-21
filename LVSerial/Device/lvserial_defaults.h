#ifndef LVSERIAL_DEFAULTS_H
#define LVSERIAL_DEFAULTS_H
#include "../Proto/lvserial.pb.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Version 2 default dictionaries are part of the wire format. Never reorder or
 * change existing entries; append new profiles instead. A mask preserves which
 * keys were present, including explicit false/zero/empty overrides. */
typedef struct {
    const char* key;
    lvserial_ValueKind kind;
    int64_t number;
    const char* text;
} lvserial_default;
size_t lvserial_defaults_profile_count(void);
const lvserial_default* lvserial_defaults_profile(uint32_t profile, size_t* count);
#ifdef __cplusplus
}
#endif
#endif
