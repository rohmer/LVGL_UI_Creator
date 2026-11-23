#include "lv_advanced_keyboard_prediction.h"
#include "PredictionModel.h"
#include "PredictionPrefix.h"
#include "PredictionAutocorrect.h"
#include "PredictionStorage.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#include <mutex>
#include <vector>
#endif

#if LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION
namespace prediction = espos::keyboard::prediction;
namespace {
void* AllocateTrieBytes(size_t size)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    if (void* bytes = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) return bytes;
#endif
    return std::malloc(size);
}
void FreeTrieBytes(void* bytes)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(bytes);
#else
    std::free(bytes);
#endif
}
void* CompactTrieBytes(void* bytes, size_t size)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    if (void* compact = heap_caps_realloc(bytes, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) return compact;
    return heap_caps_realloc(bytes, size, MALLOC_CAP_8BIT);
#else
    return std::realloc(bytes, size);
#endif
}
}

namespace espos::keyboard::prediction {
ModelStorage::ModelStorage(void* bytes, size_t trie_size, size_t index_size) noexcept
    : bytes_(bytes), trie_size_(trie_size), index_size_(index_size) {}
ModelStorage::~ModelStorage() { FreeTrieBytes(bytes_); }

std::shared_ptr<const ModelStorage> CopyModelToPsram(
    const void* trie, size_t trie_size, const void* index, size_t index_size)
{
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    if (!trie || !index || trie_size < 16 || index_size < 16 ||
        trie_size > UINT32_MAX || index_size > UINT32_MAX || trie_size > SIZE_MAX - index_size) return {};
    static std::mutex mutex;
    static std::vector<std::weak_ptr<const ModelStorage>> models;
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto iterator = models.begin(); iterator != models.end();) {
        const auto model = iterator->lock();
        if (!model) { iterator = models.erase(iterator); continue; }
        if (model->TrieSize() == trie_size && model->IndexSize() == index_size &&
            (model->Trie() == trie || !std::memcmp(model->Trie(), trie, trie_size)) &&
            (model->Index() == index || !std::memcmp(model->Index(), index, index_size))) return model;
        ++iterator;
    }
    void* bytes = heap_caps_malloc(trie_size + index_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!bytes) return {};
    std::shared_ptr<const ModelStorage> model;
    try { model = std::make_shared<ModelStorage>(bytes, trie_size, index_size); }
    catch (const std::bad_alloc&) { FreeTrieBytes(bytes); return {}; }
    std::memcpy(bytes, trie, trie_size);
    std::memcpy(static_cast<unsigned char*>(bytes) + trie_size, index, index_size);
    try { models.emplace_back(model); }
    catch (const std::bad_alloc&) { /* The model remains usable without caching. */ }
    return model;
#else
    (void)trie; (void)trie_size; (void)index; (void)index_size;
    return {};
#endif
}
}

struct lv_advanced_keyboard_predictor {
    prediction::Model model;
    prediction::PrefixScratch prefix;
    prediction::AutocorrectScratch autocorrect;
    std::shared_ptr<const prediction::ModelStorage> storage;
    void* owned_index = nullptr;
};

namespace {
#if LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
#include "PredictionBuiltin.inc"
#endif
uint32_t Read32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) | (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24);
}
void Write32(uint8_t* bytes,uint32_t value)
{
    for (unsigned shift=0;shift<32;shift+=8) *bytes++=static_cast<uint8_t>(value>>shift);
}
bool ReadVar(const uint8_t* bytes,size_t size,size_t& offset,uint32_t& value)
{
    value=0;
    for (unsigned shift=0;shift<=28 && offset<size;shift+=7) {
        const uint8_t next=bytes[offset++];
        if (shift==28 && next>0x0f) return false;
        value|=uint32_t(next&0x7f)<<shift;
        if (!(next&0x80)) return shift==0 || next!=0;
    }
    return false;
}
void WriteVar(uint8_t* bytes,size_t& offset,uint32_t value)
{
    while (value>=0x80) { bytes[offset++]=static_cast<uint8_t>(value|0x80);value>>=7; }
    bytes[offset++]=static_cast<uint8_t>(value);
}
struct RawCursor { size_t offset=16;uint32_t next_child=2;uint32_t node=0; };
struct RawNode { uint32_t first_child=0;uint32_t children=0;uint32_t terminal=0; };
bool ReadNode(const uint8_t* bytes,size_t size,uint32_t nodes,RawCursor& cursor,RawNode& out)
{
    if (cursor.node>=nodes || cursor.node>=cursor.next_child || cursor.offset>=size) return false;
    const auto label=bytes[cursor.offset++];
    uint32_t flags=0;
    if (!ReadVar(bytes,size,cursor.offset,flags) || (label==0)!=(cursor.node<2)) return false;
    const uint32_t count=flags>>1;
    if (count>255 || cursor.next_child>nodes || count>nodes-cursor.next_child) return false;
    uint32_t terminal=0;
    if ((flags&1) && (cursor.node<2 || !ReadVar(bytes,size,cursor.offset,terminal) || terminal==0)) return false;
    if (cursor.node>=2 && count==0 && terminal==0) return false;
    out={cursor.next_child,count,terminal};
    cursor.next_child+=count;
    ++cursor.node;
    return true;
}
bool NodeAt(const uint8_t* bytes,size_t size,uint32_t nodes,const uint8_t* checkpoints,uint32_t node,RawNode& out)
{
    const auto* checkpoint=checkpoints+(node/32)*12;
    RawCursor cursor{Read32(checkpoint),Read32(checkpoint+4),(node/32)*32};
    while (cursor.node<=node) if (!ReadNode(bytes,size,nodes,cursor,out)) return false;
    return true;
}
bool Token(const char* data,size_t size,std::string_view& token)
{
    if (!data && size) return false;
    token={data?data:"",size};
    return token.find('\0')==std::string_view::npos;
}
void CopySuggestion(lv_advanced_keyboard_suggestion_t& out,const prediction::Suggestion& candidate)
{
    out={};out.length=candidate.length;out.mass=candidate.mass;
    std::memcpy(out.text,candidate.text.data(),candidate.length);
}
}

extern "C" {
bool lv_advanced_keyboard_validate_trie(const void* trie,size_t trie_size,const void* index,size_t index_size)
{
    prediction::Model check;
    return check.Open(trie,trie_size,index,index_size);
}
bool lv_advanced_keyboard_build_trie_index(const void* data,size_t size,void** index,size_t* index_size)
{
    if (index) *index=nullptr;
    if (index_size) *index_size=0;
    if (!index || !index_size || !data || size<16 || size>UINT32_MAX) return false;
    const auto* bytes=static_cast<const uint8_t*>(data);
    const auto nodes=Read32(bytes+8);
    if (std::memcmp(bytes,"NGTR",4) || Read32(bytes+4)!=3 || Read32(bytes+12)!=0 || nodes<2 ||
        nodes>(size-16)/2 || size-16>uint64_t(nodes)*8) return false;
    const uint64_t mass_start64=16+((uint64_t(nodes)+31)/32)*12;
    const uint64_t capacity64=mass_start64+uint64_t(nodes)*5;
    if (uint64_t(nodes)*sizeof(uint32_t)>SIZE_MAX || capacity64>SIZE_MAX || capacity64>UINT32_MAX) return false;
    const auto mass_start=static_cast<size_t>(mass_start64);
    auto* masses=static_cast<uint32_t*>(AllocateTrieBytes(size_t(nodes)*sizeof(uint32_t)));
    auto* output=static_cast<uint8_t*>(AllocateTrieBytes(static_cast<size_t>(capacity64)));
    if (!masses || !output) {FreeTrieBytes(masses);FreeTrieBytes(output);return false;}
    auto fail=[&]() {FreeTrieBytes(masses);FreeTrieBytes(output);return false;};
    std::memcpy(output,"NGIX",4);Write32(output+4,1);Write32(output+8,static_cast<uint32_t>(size));Write32(output+12,nodes);
    RawCursor cursor;
    RawNode node;
    for (uint32_t id=0;id<nodes;++id) {
        if (id%32==0) {
            auto* checkpoint=output+16+(id/32)*12;
            Write32(checkpoint,static_cast<uint32_t>(cursor.offset));Write32(checkpoint+4,cursor.next_child);Write32(checkpoint+8,0);
        }
        if (!ReadNode(bytes,size,nodes,cursor,node)) return fail();
        masses[id]=node.terminal;
    }
    if (cursor.offset!=size || cursor.next_child!=nodes) return fail();
    for (uint32_t id=nodes;id-->0;) {
        if (!NodeAt(bytes,size,nodes,output+16,id,node)) return fail();
        uint64_t mass=masses[id];
        for (uint32_t child=0;child<node.children;++child) mass+=masses[node.first_child+child];
        if (mass>UINT32_MAX) return fail();
        masses[id]=static_cast<uint32_t>(mass);
    }
    cursor={};size_t used=mass_start;
    for (uint32_t id=0;id<nodes;++id) {
        if (id%32==0) Write32(output+16+(id/32)*12+8,static_cast<uint32_t>(used-mass_start));
        if (!ReadNode(bytes,size,nodes,cursor,node)) return fail();
        if (node.children) WriteVar(output,used,masses[id]);
    }
    prediction::Model check;
    if (!check.Open(bytes,size,output,used)) return fail();
    FreeTrieBytes(masses);
    if (auto* compact=static_cast<uint8_t*>(CompactTrieBytes(output,used))) output=compact;
    *index=output;*index_size=used;
    return true;
}
void lv_advanced_keyboard_free_trie_index(void* index) {FreeTrieBytes(index);}
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create_borrowed(const void* trie,size_t trie_size,const void* index,size_t index_size)
{
    auto* predictor=new(std::nothrow) lv_advanced_keyboard_predictor;
    if (!predictor) return nullptr;
    predictor->storage = prediction::CopyModelToPsram(trie,trie_size,index,index_size);
    if (predictor->storage) {trie=predictor->storage->Trie();index=predictor->storage->Index();}
    if (!predictor->model.Open(trie,trie_size,index,index_size)) {delete predictor;return nullptr;}
    return predictor;
}
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create_builtin(void)
{
#if LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
    return lv_advanced_keyboard_predictor_create_borrowed(kBuiltinTrie,sizeof kBuiltinTrie,kBuiltinIndex,sizeof kBuiltinIndex);
#else
    return nullptr;
#endif
}
bool lv_advanced_keyboard_get_builtin_trie(const void** trie,size_t* trie_size,const void** index,size_t* index_size)
{
    if (trie) *trie=nullptr;
    if (trie_size) *trie_size=0;
    if (index) *index=nullptr;
    if (index_size) *index_size=0;
#if LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE
    if (!trie || !trie_size || !index || !index_size) return false;
    *trie=kBuiltinTrie;*trie_size=sizeof kBuiltinTrie;*index=kBuiltinIndex;*index_size=sizeof kBuiltinIndex;
    return true;
#else
    return false;
#endif
}
lv_advanced_keyboard_predictor_t* lv_advanced_keyboard_predictor_create(const void* trie,size_t trie_size)
{
    void* index=nullptr;size_t index_size=0;
    if (!lv_advanced_keyboard_build_trie_index(trie,trie_size,&index,&index_size)) return nullptr;
    auto* predictor=lv_advanced_keyboard_predictor_create_borrowed(trie,trie_size,index,index_size);
    if (!predictor) {FreeTrieBytes(index);return nullptr;}
    if (predictor->storage) FreeTrieBytes(index);
    else predictor->owned_index=index;
    return predictor;
}
void lv_advanced_keyboard_predictor_destroy(lv_advanced_keyboard_predictor_t* predictor)
{
    if (!predictor) return;
    predictor->model.Close();FreeTrieBytes(predictor->owned_index);delete predictor;
}
size_t lv_advanced_keyboard_predictor_workspace_bytes(void) {return sizeof(lv_advanced_keyboard_predictor);}
uint32_t lv_advanced_keyboard_predictor_node_count(const lv_advanced_keyboard_predictor_t* predictor) {return predictor?predictor->model.NodeCount():0;}
size_t lv_advanced_keyboard_predictor_suggest_folded(lv_advanced_keyboard_predictor_t* predictor,
    const char* prefix,size_t prefix_size,const char* previous,size_t previous_size,
    const char* older,size_t older_size,lv_advanced_keyboard_suggestion_t* output,size_t capacity)
{
    std::string_view p,v,o;
    if (!predictor || !output || !capacity || !Token(prefix,prefix_size,p) || !Token(previous,previous_size,v) || !Token(older,older_size,o)) return 0;
    std::array<prediction::Suggestion,prediction::kSuggestionCount> candidates{};
    const auto count=prediction::Suggest(predictor->model,predictor->prefix,p,v,o,candidates.data(),capacity);
    for (size_t i=0;i<count;++i) CopySuggestion(output[i],candidates[i]);
    return count;
}
bool lv_advanced_keyboard_predictor_autocorrect_utf8(lv_advanced_keyboard_predictor_t* predictor,
    const char* typed,size_t typed_size,const char* previous,size_t previous_size,
    const char* older,size_t older_size,double threshold,lv_advanced_keyboard_suggestion_t* output,double* probability)
{
    if (output) *output={};
    if (probability) *probability=0;
    std::string_view t,v,o;
    if (!predictor || !output || !(threshold>=0 && threshold<=1) || !Token(typed,typed_size,t) || !Token(previous,previous_size,v) || !Token(older,older_size,o)) return false;
    prediction::AutocorrectResult result;
    if (!prediction::BestAutocorrectionUtf8(predictor->model,predictor->autocorrect,t,result,v,o) || result.probability<threshold || result.word.size()>sizeof(output->text)) return false;
    output->length=static_cast<uint16_t>(result.word.size());
    std::memcpy(output->text,result.word.data(),result.word.size());
    if (probability) *probability=result.probability;
    return true;
}
bool lv_advanced_keyboard_predictor_autocorrect_folded(lv_advanced_keyboard_predictor_t* predictor,
    const char* typed,size_t typed_size,const char* previous,size_t previous_size,
    const char* older,size_t older_size,double threshold,lv_advanced_keyboard_suggestion_t* output,double* probability)
{
    if (output) *output={};
    if (probability) *probability=0;
    std::string_view t,v,o;
    if (!predictor || !output || !(threshold>=0 && threshold<=1) || !Token(typed,typed_size,t) || !Token(previous,previous_size,v) || !Token(older,older_size,o)) return false;
    prediction::AutocorrectResult result;
    if (!prediction::BestAutocorrection(predictor->model,predictor->autocorrect,t,result,v,o) || result.probability<threshold || result.word.size()>sizeof(output->text)) return false;
    output->length=static_cast<uint16_t>(result.word.size());
    std::memcpy(output->text,result.word.data(),result.word.size());
    if (probability) *probability=result.probability;
    return true;
}
}
#endif
