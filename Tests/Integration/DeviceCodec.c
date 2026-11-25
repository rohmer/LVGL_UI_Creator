#include "lvserial_pb.h"
#include <stdio.h>
#include <string.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "Device codec check failed at line %d\n", __LINE__); return 1; } } while (0)
static bool read_property(void* context, size_t index, lvserial_property* result)
{
    (void)context;
    if (index == 0) { result->metadata.kind = lvserial_ValueKind_OBJECT; result->metadata.has_child_count = true; result->metadata.child_count = 2; }
    else if (index == 1) { strcpy(result->metadata.path, "/value"); result->metadata.kind = lvserial_ValueKind_SIGNED; result->metadata.has_signed_value = true; result->metadata.signed_value = -42; }
    else { strcpy(result->metadata.path, "/name"); result->metadata.kind = lvserial_ValueKind_TEXT; result->bytes = (const pb_byte_t*)"hello"; result->size = 5; }
    return true;
}
static bool visit_property(void* context, const lvserial_record_info* record, const lvserial_Property* property, pb_istream_t* payload)
{
    size_t* count = (size_t*)context;
    if (record->kind != lvserial_EntityKind_VARIABLE || strcmp(record->type, "variable")) return false;
    ++*count;
    if (!strcmp(property->path, "/value")) return property->signed_value == -42;
    if (!strcmp(property->path, "/name")) {
        pb_byte_t text[6] = {0};
        return payload && payload->bytes_left == 5 && pb_read(payload, text, 5) && !strcmp((const char*)text, "hello");
    }
    return property->kind == lvserial_ValueKind_OBJECT;
}
static bool read_sparse(void* context, size_t index, lvserial_property* result)
{
    uint64_t mask=*(const uint64_t*)context;
    if(index) return read_property(NULL,1,result);
    result->metadata.kind=lvserial_ValueKind_OBJECT;
    result->metadata.has_child_count=true;result->metadata.child_count=1;
    result->metadata.has_signed_value=true;result->metadata.signed_value=1;
    result->metadata.has_unsigned_value=true;result->metadata.unsigned_value=mask;
    return true;
}
static bool visit_sparse(void* context,const lvserial_record_info* record,const lvserial_Property* property,pb_istream_t* payload)
{
    size_t* count=(size_t*)context;++*count;
    if(record->format_version!=2)return false;
    if(!property->path[0])return property->kind==lvserial_ValueKind_OBJECT && property->child_count==4 && !property->has_signed_value && !property->has_unsigned_value;
    if(!strcmp(property->path,"/name"))return property->kind==lvserial_ValueKind_TEXT && payload && payload->bytes_left==0;
    if(!strcmp(property->path,"/click"))return property->kind==lvserial_ValueKind_BOOLEAN && property->has_boolean_value && property->boolean_value;
    if(!strcmp(property->path,"/hidden"))return property->kind==lvserial_ValueKind_BOOLEAN && property->has_boolean_value && !property->boolean_value;
    return !strcmp(property->path,"/value") && property->signed_value==-42;
}
/* A typed FontData message with an empty bitmap, encoded independently of the
 * desktop C++ adapter. The streaming C codec needs no heap or font engine. */
static const pb_byte_t font_data[] = {8,1,24,16,82,0,88,0,96,4};
static bool read_font(void* context,size_t index,lvserial_property* result)
{
    (void)context;(void)index;
    result->metadata.kind=lvserial_ValueKind_FONT_DATA;
    result->bytes=font_data;result->size=sizeof font_data;
    return true;
}
static bool visit_font(void* context,const lvserial_record_info* record,const lvserial_Property* property,pb_istream_t* payload)
{
    lvserial_FontData font=lvserial_FontData_init_zero;
    size_t* count=(size_t*)context;
    if(record->kind!=lvserial_EntityKind_FONT || strcmp(record->type,"font") ||
        property->kind!=lvserial_ValueKind_FONT_DATA || !payload)return false;
    if(!pb_decode(payload,lvserial_FontData_fields,&font))return false;
    ++*count;
    return font.format_version==1 && font.line_height==16 && font.bpp==4 && font.bitmap_format==0;
}
int main(int argc, char** argv)
{
    pb_byte_t buffer[1024]; lvserial_record_info info = {LVSERIAL_BINARY_VERSION, lvserial_EntityKind_VARIABLE, "variable"};
    lvserial_record_info decoded; pb_ostream_t output = pb_ostream_from_buffer(buffer, sizeof buffer);
    const char* error = NULL; size_t count = 0;
    CHECK(lvserial_pb_encode(&output, &info, 3, read_property, NULL));
    CHECK(lvserial_pb_decode(buffer, output.bytes_written, &decoded, visit_property, &count, &error)); CHECK(count == 3);
    if (argc == 2) {
        FILE* fixture = fopen(argv[1], "wb");
        CHECK(fixture != NULL); CHECK(fwrite(buffer, 1, output.bytes_written, fixture) == output.bytes_written); CHECK(fclose(fixture) == 0);
    }
    CHECK(!lvserial_pb_decode(buffer, output.bytes_written - 1, &decoded, visit_property, &count, &error)); CHECK(error != NULL);
    info.format_version=LVSERIAL_BINARY_VERSION_LEGACY;output=pb_ostream_from_buffer(buffer,sizeof buffer);count=0;
    CHECK(lvserial_pb_encode(&output,&info,3,read_property,NULL));
    CHECK(lvserial_pb_decode(buffer,output.bytes_written,&decoded,visit_property,&count,&error));CHECK(count==3);
    {
        uint64_t mask=7;info.format_version=LVSERIAL_BINARY_VERSION;count=0;output=pb_ostream_from_buffer(buffer,sizeof buffer);
        CHECK(lvserial_pb_encode(&output,&info,2,read_sparse,&mask));
        CHECK(lvserial_pb_decode(buffer,output.bytes_written,&decoded,visit_sparse,&count,&error));CHECK(count==5);
        mask=UINT64_C(1)<<63;output=pb_ostream_from_buffer(buffer,sizeof buffer);
        CHECK(lvserial_pb_encode(&output,&info,2,read_sparse,&mask));
        CHECK(!lvserial_pb_decode(buffer,output.bytes_written,&decoded,NULL,NULL,&error));CHECK(error!=NULL);
    }
    output = pb_ostream_from_buffer(buffer, 1); CHECK(!lvserial_pb_encode(&output, &info, 3, read_property, NULL));
    info.kind=lvserial_EntityKind_FONT;strcpy(info.type,"font");count=0;
    output=pb_ostream_from_buffer(buffer,sizeof buffer);
    CHECK(lvserial_pb_encode(&output,&info,1,read_font,NULL));
    CHECK(lvserial_pb_decode(buffer,output.bytes_written,&decoded,visit_font,&count,&error));CHECK(count==1);
    CHECK(!lvserial_pb_decode(buffer,output.bytes_written-1,&decoded,visit_font,&count,&error));
    if(argc==2) {
        char path[1024];FILE* fixture;
        CHECK(snprintf(path,sizeof path,"%s.font.pb",argv[1])>0);
        fixture=fopen(path,"wb");CHECK(fixture!=NULL);
        CHECK(fwrite(buffer,1,output.bytes_written,fixture)==output.bytes_written);CHECK(fclose(fixture)==0);
    }
    puts("Plain-C nanopb encoding, streaming decoding, and failure checks passed"); return 0;
}
