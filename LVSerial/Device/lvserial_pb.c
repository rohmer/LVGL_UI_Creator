#include "lvserial_pb.h"
#include "lvserial_defaults.h"
#include <string.h>

typedef struct { size_t count; lvserial_read_property read; void* context; } encoder_context;
static bool encode_bytes(pb_ostream_t* stream, const pb_field_t* field, void* const* argument)
{
    const lvserial_property* property = (const lvserial_property*)*argument;
    return pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, property->bytes, property->size);
}
static bool valid_property(const lvserial_Property* property)
{
    switch (property->kind) {
        case lvserial_ValueKind_NULL_VALUE: return true;
        case lvserial_ValueKind_OBJECT: case lvserial_ValueKind_ARRAY: return property->has_child_count;
        case lvserial_ValueKind_BOOLEAN: return property->has_boolean_value;
        case lvserial_ValueKind_SIGNED: return property->has_signed_value;
        case lvserial_ValueKind_UNSIGNED: return property->has_unsigned_value;
        case lvserial_ValueKind_REAL: return property->has_real_value;
        case lvserial_ValueKind_TEXT: case lvserial_ValueKind_BINARY: return true;
        case lvserial_ValueKind_FONT_DATA: return true;
        default: return false;
    }
}
static bool encode_properties(pb_ostream_t* stream, const pb_field_t* field, void* const* argument)
{
    encoder_context* context = (encoder_context*)*argument;
    size_t i;
    for (i = 0; i < context->count; ++i) {
        lvserial_property property;
        memset(&property, 0, sizeof property);
        if (!context->read(context->context, i, &property)) PB_RETURN_ERROR(stream, "property callback failed");
        if (!memchr(property.metadata.path, 0, sizeof property.metadata.path) || !valid_property(&property.metadata))
            PB_RETURN_ERROR(stream, "invalid property");
        if (property.metadata.kind == lvserial_ValueKind_TEXT || property.metadata.kind == lvserial_ValueKind_BINARY || property.metadata.kind == lvserial_ValueKind_FONT_DATA) {
            if (property.size && !property.bytes) PB_RETURN_ERROR(stream, "missing payload");
            pb_callback_t* payload = property.metadata.kind == lvserial_ValueKind_FONT_DATA ? &property.metadata.font_value : &property.metadata.text_value;
            payload->funcs.encode = encode_bytes;
            payload->arg = &property;
        }
        if (!pb_encode_tag_for_field(stream, field) || !pb_encode_submessage(stream, lvserial_Property_fields, &property.metadata)) return false;
    }
    return true;
}
bool lvserial_pb_encode(pb_ostream_t* output, const lvserial_record_info* record,
    size_t count, lvserial_read_property read_property, void* context)
{
    lvserial_Record message = lvserial_Record_init_zero;
    encoder_context encoder;
    if (!output || !record || (!read_property && count)) return false;
    if ((record->format_version != LVSERIAL_BINARY_VERSION && record->format_version != LVSERIAL_BINARY_VERSION_LEGACY) || record->kind < _lvserial_EntityKind_MIN || record->kind > _lvserial_EntityKind_MAX ||
        !record->type[0] || !memchr(record->type, 0, sizeof record->type)) PB_RETURN_ERROR(output, "invalid record header");
    message.format_version = record->format_version;
    message.kind = record->kind;
    memcpy(message.type, record->type, sizeof message.type);
    encoder.count = count; encoder.read = read_property; encoder.context = context;
    message.properties.funcs.encode = encode_properties;
    message.properties.arg = &encoder;
    return pb_encode(output, lvserial_Record_fields, &message);
}

typedef struct { pb_istream_t stream; bool present; } captured_bytes;
static bool capture_bytes(pb_istream_t* stream, const pb_field_t* field, void** argument)
{
    captured_bytes* captured = (captured_bytes*)*argument;
    (void)field;
    captured->stream = *stream;
    captured->present = true;
    return pb_read(stream, NULL, stream->bytes_left);
}
typedef struct { lvserial_record_info* record; lvserial_visit_property visit; void* context; } decoder_context;
static bool decode_property(pb_istream_t* stream, const pb_field_t* field, void** argument)
{
    decoder_context* context = (decoder_context*)*argument;
    lvserial_Property property = lvserial_Property_init_zero;
    captured_bytes captured, font;
    pb_istream_t empty = pb_istream_from_buffer(NULL, 0);
    (void)field;
    memset(&captured, 0, sizeof captured);
    memset(&font, 0, sizeof font);
    property.text_value.funcs.decode = capture_bytes;
    property.text_value.arg = &captured;
    property.font_value.funcs.decode = capture_bytes;
    property.font_value.arg = &font;
    if (!pb_decode(stream, lvserial_Property_fields, &property)) return false;
    if (!valid_property(&property)) PB_RETURN_ERROR(stream, "invalid property value");
    if (property.kind == lvserial_ValueKind_FONT_DATA) {
        if (!font.present || captured.present) PB_RETURN_ERROR(stream, "invalid font property payload");
        if (context->visit && !context->visit(context->context, context->record, &property, &font.stream))
            PB_RETURN_ERROR(stream, "property visitor rejected font data");
        return true;
    }
    if (font.present) PB_RETURN_ERROR(stream, "font payload on a non-font property");
    if (context->record->format_version >= 2 && property.kind == lvserial_ValueKind_OBJECT &&
        (property.has_signed_value || property.has_unsigned_value)) {
        size_t count = 0, i, added = 0;
        const lvserial_default* defaults;
        uint64_t mask = property.unsigned_value;
        if (!property.has_signed_value || !property.has_unsigned_value || property.signed_value < 1 || property.signed_value > UINT32_MAX || !mask)
            PB_RETURN_ERROR(stream, "invalid default profile metadata");
        defaults = lvserial_defaults_profile((uint32_t)property.signed_value, &count);
        if (!defaults || count > 64 || (count < 64 && (mask >> count))) PB_RETURN_ERROR(stream, "unsupported default profile or mask");
        for (i=0;i<count;++i) if (mask & (UINT64_C(1)<<i)) {
            size_t j; ++added;
            for (j=0;j<i;++j) if ((mask & (UINT64_C(1)<<j)) && !strcmp(defaults[i].key,defaults[j].key))
                PB_RETURN_ERROR(stream,"duplicate default key");
        }
        if (property.child_count > UINT32_MAX-added) PB_RETURN_ERROR(stream, "default child count overflow");
        property.child_count += (uint32_t)added;
        property.has_signed_value = false; property.has_unsigned_value = false;
        if (context->visit && !context->visit(context->context, context->record, &property, NULL))
            PB_RETURN_ERROR(stream, "property visitor rejected default object");
        for (i=0;i<count;++i) if (mask & (UINT64_C(1)<<i)) {
            lvserial_Property restored = lvserial_Property_init_zero;
            const lvserial_default* item = &defaults[i];
            size_t parent_size = strlen(property.path), key_size = strlen(item->key);
            pb_istream_t text;
            /* Dictionary keys contain only literal identifier characters. */
            if (parent_size+1+key_size >= sizeof restored.path) PB_RETURN_ERROR(stream, "default path too long");
            memcpy(restored.path, property.path, parent_size); restored.path[parent_size]='/';
            memcpy(restored.path+parent_size+1, item->key, key_size+1); restored.kind=item->kind;
            if (item->kind == lvserial_ValueKind_SIGNED) { restored.has_signed_value=true;restored.signed_value=item->number; }
            else if (item->kind == lvserial_ValueKind_REAL) { restored.has_real_value=true;restored.real_value=(double)item->number; }
            else if (item->kind == lvserial_ValueKind_BOOLEAN) { restored.has_boolean_value=true;restored.boolean_value=item->number != 0; }
            else if (item->kind == lvserial_ValueKind_OBJECT || item->kind == lvserial_ValueKind_ARRAY) { restored.has_child_count=true;restored.child_count=0; }
            text=pb_istream_from_buffer((const pb_byte_t*)item->text,item->text?strlen(item->text):0);
            if (context->visit && !context->visit(context->context, context->record, &restored, item->kind==lvserial_ValueKind_TEXT?&text:NULL))
                PB_RETURN_ERROR(stream, "property visitor rejected default value");
        }
        return true;
    }
    if (context->visit && !context->visit(context->context, context->record, &property,
        (property.kind == lvserial_ValueKind_TEXT || property.kind == lvserial_ValueKind_BINARY) ? (captured.present ? &captured.stream : &empty) : NULL))
        PB_RETURN_ERROR(stream, "property visitor rejected value");
    return true;
}
bool lvserial_pb_decode(const pb_byte_t* input, size_t size, lvserial_record_info* record,
    lvserial_visit_property visit, void* context, const char** error)
{
    lvserial_Record message = lvserial_Record_init_zero;
    decoder_context decoder;
    pb_istream_t stream;
    if (error) *error = NULL;
    if ((!input && size) || !record) { if (error) *error = "invalid input"; return false; }
    stream = pb_istream_from_buffer(input, size);
    /* Read the header before visiting properties, regardless of wire field order. */
    if (!pb_decode(&stream, lvserial_Record_fields, &message)) { if (error) *error = PB_GET_ERROR(&stream); return false; }
    if ((message.format_version != LVSERIAL_BINARY_VERSION && message.format_version != LVSERIAL_BINARY_VERSION_LEGACY) || message.kind < _lvserial_EntityKind_MIN || message.kind > _lvserial_EntityKind_MAX || !message.type[0]) {
        if (error) *error = "unsupported record header";
        return false;
    }
    record->format_version = message.format_version; record->kind = message.kind;
    memcpy(record->type, message.type, sizeof record->type);
    decoder.record = record; decoder.visit = visit; decoder.context = context;
    message.properties.funcs.decode = decode_property;
    message.properties.arg = &decoder;
    stream = pb_istream_from_buffer(input, size);
    if (!pb_decode(&stream, lvserial_Record_fields, &message)) { if (error) *error = PB_GET_ERROR(&stream); return false; }
    return true;
}
