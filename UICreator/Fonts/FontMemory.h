#pragma once
#include <lvgl/lvgl.h>
#include "../../3rdParty/JSON/json.hpp"
#include <Serialization/FontRanges.h>
#include <Serialization/ResourceArena.h>
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <vector>

// Native LVGL resource payloads, matching the widget-instance estimate. This
// excludes allocator/container overhead, source TTFs and temporary draw buffers.
// Fonts restored by LVFont are immutable; remember their measured tables rather
// than serializing thousands of glyphs on every workspace refresh.
class ScreenFontMemory {
    struct Span { uintptr_t begin, end; };
    struct Fingerprint {
        const void *descriptor=nullptr,*bitmap=nullptr,*glyphs=nullptr,*maps=nullptr,*kern=nullptr;
        uint32_t glyphCount=0,lastOffset=0,lastWidth=0,lastHeight=0,kernCount=0;
        unsigned cmapCount=0,bpp=0,format=0,stride=0,kernClasses=0,kernIdSize=0;
        bool formatted=false;
        auto Values() const {return std::tie(descriptor,bitmap,glyphs,maps,kern,glyphCount,lastOffset,lastWidth,lastHeight,
            kernCount,cmapCount,bpp,format,stride,kernClasses,kernIdSize,formatted);}
        bool operator==(const Fingerprint& other) const {return Values()==other.Values();}
    };
    struct Measurement {Fingerprint fingerprint;std::vector<Span> spans;uint64_t frame=0;};
    std::map<const lv_font_t*,Measurement> cache;
    std::set<const lv_font_t*> visited;
    std::vector<Span> spans;
    uint64_t frame=0;

    static void Add(std::vector<Span>& spans,const void* data,size_t bytes) {
        if(data&&bytes) {const auto start=reinterpret_cast<uintptr_t>(data);spans.push_back({start,start+bytes});}
    }
    static Fingerprint Describe(const lv_font_t* font) {
        Fingerprint result;result.descriptor=font->dsc;
        result.formatted=font->dsc&&font->get_glyph_dsc==lv_font_get_glyph_dsc_fmt_txt;
        if(!result.formatted)return result;
        const auto& d=*static_cast<const lv_font_fmt_txt_dsc_t*>(font->dsc);
#if LV_VERSION_CHECK(9, 6, 0)
        if(d.are_glyphs_dynamic_loaded){result.formatted=false;return result;}
#endif
        result.bitmap=d.glyph_bitmap;result.glyphs=d.glyph_dsc;result.maps=d.cmaps;result.kern=d.kern_dsc;
        result.cmapCount=d.cmap_num;result.bpp=d.bpp;result.format=d.bitmap_format;result.kernClasses=d.kern_classes;
#if LV_VERSION_CHECK(9, 3, 0)
        result.stride=d.stride;
#endif
        result.glyphCount=Serialization::GlyphCount(d);
        if(d.glyph_dsc&&result.glyphCount) {
            const auto& last=d.glyph_dsc[result.glyphCount-1];
            result.lastOffset=last.bitmap_index;result.lastWidth=last.box_w;result.lastHeight=last.box_h;
        }
        if(d.kern_dsc) {
            if(d.kern_classes) {
                const auto& kern=*static_cast<const lv_font_fmt_txt_kern_classes_t*>(d.kern_dsc);
                result.kernCount=unsigned(kern.left_class_cnt)*kern.right_class_cnt;
            } else {
                const auto& kern=*static_cast<const lv_font_fmt_txt_kern_pair_t*>(d.kern_dsc);
                result.kernCount=kern.pair_cnt;result.kernIdSize=kern.glyph_ids_size;
            }
        }
        return result;
    }
    static nlohmann::json OwnedRecord(const lv_font_t* font) {
        // Compressed fmt_txt descriptors expose offsets, but no final bitmap
        // length. The immutable resource record is the authoritative bound.
        for(const auto& entry:Serialization::Resources().fonts)
            if(entry.second==font)return nlohmann::json::parse(entry.first);
        return nullptr;
    }
    static std::vector<Span> Measure(const lv_font_t* font,const Fingerprint& fingerprint) {
        std::vector<Span> result;Add(result,font,sizeof(lv_font_t));
        if(!fingerprint.formatted)return result;
        const auto& d=*static_cast<const lv_font_fmt_txt_dsc_t*>(font->dsc);
        Add(result,&d,sizeof d);
        const auto owned=OwnedRecord(font);
        const auto* record=owned.is_object()&&owned.contains("dsc")?&owned.at("dsc"):nullptr;
        const size_t glyphCount=record&&record->contains("GlyphDsc")?record->at("GlyphDsc").size():fingerprint.glyphCount;
        Add(result,d.glyph_dsc,glyphCount*sizeof(lv_font_fmt_txt_glyph_dsc_t));
        Add(result,d.cmaps,size_t(d.cmap_num)*sizeof(lv_font_fmt_txt_cmap_t));
        for(unsigned i=0;d.cmaps&&i<d.cmap_num;++i) {
            const auto& map=d.cmaps[i];
            const auto* saved=record&&record->contains("cmaps")&&i<record->at("cmaps").size()?&record->at("cmaps")[i]:nullptr;
            const size_t unicodeCount=saved&&saved->contains("unicodeList")?saved->at("unicodeList").size():map.list_length;
            Add(result,map.unicode_list,unicodeCount*sizeof(uint16_t));
            if(map.type==LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
                const size_t count=saved&&saved->contains("glyphIDOffsetList")?saved->at("glyphIDOffsetList").size():map.range_length;
                Add(result,map.glyph_id_ofs_list,count*sizeof(uint8_t));
            } else if(map.type==LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
                const size_t count=saved&&saved->contains("glyphIDOffsetList")?saved->at("glyphIDOffsetList").size():map.list_length;
                Add(result,map.glyph_id_ofs_list,count*sizeof(uint16_t));
            }
        }
        size_t bitmapBytes=0;
        if(record&&record->contains("bitmap")&&record->at("bitmap").is_string()) {
            const auto& base64=record->at("bitmap").get_ref<const std::string&>();
            bitmapBytes=base64.size()/4*3;
            if(!base64.empty()&&base64.back()=='=')--bitmapBytes;
            if(base64.size()>1&&base64[base64.size()-2]=='=')--bitmapBytes;
            if(d.bitmap_format)++bitmapBytes; // Owned compressed-reader lookahead.
        } else if(!d.bitmap_format&&d.glyph_dsc) {
            for(size_t i=0;i<glyphCount;++i) {
                const auto& glyph=d.glyph_dsc[i];
                size_t bytes=(size_t(glyph.box_w)*glyph.box_h*d.bpp+7)/8;
                if(fingerprint.stride) {
                    const auto row=(size_t(glyph.box_w)*d.bpp+7)/8;
                    bytes=((row+fingerprint.stride-1)/fingerprint.stride)*fingerprint.stride*glyph.box_h;
                }
                bitmapBytes=(std::max)(bitmapBytes,size_t(glyph.bitmap_index)+bytes);
            }
        }
        // Unknown external compressed engines need an explicit byte bound;
        // never walk beyond the last glyph to guess compressed storage length.
        Add(result,d.glyph_bitmap,bitmapBytes);
        if(d.kern_dsc) {
            if(d.kern_classes) {
                const auto& kern=*static_cast<const lv_font_fmt_txt_kern_classes_t*>(d.kern_dsc);
                Add(result,&kern,sizeof kern);
                Add(result,kern.class_pair_values,size_t(kern.left_class_cnt)*kern.right_class_cnt*sizeof(int8_t));
                Add(result,kern.left_class_mapping,glyphCount*sizeof(uint8_t));
                Add(result,kern.right_class_mapping,glyphCount*sizeof(uint8_t));
            } else {
                const auto& kern=*static_cast<const lv_font_fmt_txt_kern_pair_t*>(d.kern_dsc);
                Add(result,&kern,sizeof kern);
                Add(result,kern.values,size_t(kern.pair_cnt)*sizeof(int8_t));
                Add(result,kern.glyph_ids,size_t(kern.pair_cnt)*2*(kern.glyph_ids_size?sizeof(uint16_t):sizeof(uint8_t)));
            }
        }
        return result;
    }
public:
    void Begin() {++frame;visited.clear();spans.clear();}
    void Include(const lv_font_t* font) {
        if(!font||!visited.insert(font).second)return;
        const auto fingerprint=Describe(font);
        auto& saved=cache[font];
        if(saved.spans.empty()||!(saved.fingerprint==fingerprint)) {
            saved.fingerprint=fingerprint;saved.spans=Measure(font,fingerprint);
        }
        saved.frame=frame;spans.insert(spans.end(),saved.spans.begin(),saved.spans.end());
        Include(font->fallback);
    }
    size_t End() {
        for(auto entry=cache.begin();entry!=cache.end();)if(entry->second.frame!=frame)entry=cache.erase(entry);else++entry;
        // Shared tables and even overlapping views of a bitmap are counted
        // once, rather than once for every widget/font wrapper that uses them.
        std::sort(spans.begin(),spans.end(),[](const Span& a,const Span& b){return a.begin<b.begin;});
        size_t bytes=0;uintptr_t end=0;
        for(const auto& span:spans) {if(span.end>end){bytes+=span.end-(std::max)(span.begin,end);end=span.end;}}
        return bytes;
    }
};
