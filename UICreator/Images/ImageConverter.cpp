// LVGL pixel formats and compression envelope follow LVGLImage.py,
// Copyright (c) 2025 LVGL Kft, MIT license:
// upstream/lvgl_image_converter/LICENSE.
// Raster decoding, quantization and SVG rendering use linked native libraries.
#include "ImageConverter.h"
#include "NativeApng.h"
#include "../Utils/CIdentifier.h"
#include <NativePng.h>
#include <Serialization/Base64.h>
#include <libimagequant.h>
#include <resvg.h>
#include <src/libs/lz4/lz4.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include "../../ImgLib/stb_image.h"

namespace ImageConversion {
json Defaults() {return {{"colorFormat","ARGB8888"},{"format","C"},{"compression","NONE"},{"align",1},{"premultiply",false},{"dither",true},{"background",0},{"includeAlpha",true},{"chromaKey",false},{"chromaColor",0x00ff00},{"bigEndian",false},{"nemaGfx",false},{"name",""},{"verbose",false}};}
namespace {
using Bytes=std::vector<uint8_t>;
struct Format {const char* name;uint8_t code,bits,palette;};
constexpr Format formats[]={
    {"RAW",1,0,0},{"RAW_ALPHA",2,0,0},{"L8",6,8,0},{"I1",7,1,1},{"I2",8,2,1},{"I4",9,4,1},{"I8",10,8,1},
    {"A1",11,1,0},{"A2",12,2,0},{"A4",13,4,0},{"A8",14,8,0},{"RGB888",15,24,0},{"ARGB8888",16,32,0},
    {"XRGB8888",17,32,0},{"RGB565",18,16,0},{"ARGB8565",19,24,0},{"RGB565A8",20,16,0},{"AL88",21,16,0},
    {"ARGB8888_PREMULTIPLIED",26,32,0},{"RGB565_SWAPPED",27,16,0}
};
struct Frame {uint32_t width=0,height=0,delay=100;Bytes pixels;};
struct Input {std::filesystem::path path;std::string name;Bytes bytes;};
struct Encoded {Format format{};uint32_t width=0,height=0,stride=0;uint16_t flags=0;Bytes data,preview;};
struct Output {std::string name;Bytes bytes;};
void Cancel(const std::atomic<bool>* cancel) {if(cancel&&cancel->load())throw std::runtime_error("Conversion cancelled");}
std::string Lower(std::string text) {for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;}
const Format& Find(const std::string& name) {for(const auto& format:formats)if(name==format.name)return format;throw std::invalid_argument("Unknown LVGL color format: "+name);}
Bytes Read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("Cannot read image: "+path.u8string());
    const auto size=file.tellg();if(size<=0||uint64_t(size)>512u*1024u*1024u)throw std::invalid_argument("Image file is empty or too large");
    Bytes result(static_cast<size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(result.data()),result.size());
    if(!file)throw std::runtime_error("Cannot read complete image: "+path.u8string());return result;
}
std::string Base64(const Bytes& bytes) {return base64_encode(bytes.data(),bytes.size());}
void Dimensions(uint32_t width,uint32_t height) {
    if(!width||!height||width>65535||height>65535||uint64_t(width)*height>16u*1024u*1024u)throw std::invalid_argument("Image dimensions are too large or invalid");
}
bool GifSignature(const Bytes& bytes) {
    return bytes.size()>=6&&(!std::memcmp(bytes.data(),"GIF87a",6)||!std::memcmp(bytes.data(),"GIF89a",6));
}
size_t PreflightGif(const Bytes& bytes,const std::atomic<bool>* cancel) {
    constexpr uint64_t outputBudget=256u*1024u*1024u;
    auto invalid=[](){throw std::invalid_argument("Invalid or truncated GIF structure");};
    if(bytes.size()<13||!GifSignature(bytes))invalid();
    auto word=[&](size_t offset){return uint32_t(bytes[offset])|(uint32_t(bytes[offset+1])<<8);};
    const auto width=word(6),height=word(8);Dimensions(width,height);
    const uint64_t frameBytes=uint64_t(width)*height*4;
    const uint64_t maxFrames=outputBudget/frameBytes;
    size_t position=13,frames=0;
    auto skip=[&](size_t count){if(count>bytes.size()-position)invalid();position+=count;};
    auto subblocks=[&](){
        for(;;){
            Cancel(cancel);
            if(position==bytes.size())invalid();
            const auto count=bytes[position++];
            if(!count)break;
            skip(count);
        }
    };
    if(bytes[10]&0x80)skip(size_t(3)<<(1+(bytes[10]&7)));
    // Inspect only block structure; do not decode LZW or allocate canvas/frame
    // pixels. STB otherwise grows its full composed-frame buffer before its
    // caller gets an opportunity to check the decoded animation's size.
    while(position<bytes.size()){
        Cancel(cancel);const auto block=bytes[position++];
        if(block==0x3b){if(!frames)invalid();return frames;}
        if(block==0x21){if(position==bytes.size())invalid();++position;subblocks();continue;}
        if(block!=0x2c||bytes.size()-position<9)invalid();
        const auto left=word(position),top=word(position+2),frameWidth=word(position+4),frameHeight=word(position+6);
        const auto packed=bytes[position+8];position+=9;
        if(!frameWidth||!frameHeight||uint64_t(left)+frameWidth>width||uint64_t(top)+frameHeight>height)invalid();
        if(uint64_t(frames)>=maxFrames)throw std::invalid_argument("GIF animation exceeds the 256 MiB decoded-frame limit");
        ++frames;
        if(packed&0x80)skip(size_t(3)<<(1+(packed&7)));
        if(position==bytes.size())invalid();
        const auto codeSize=bytes[position++];if(codeSize<2||codeSize>8)invalid();
        subblocks();
    }
    invalid();return 0;
}
bool SvgNeedsFonts(const Bytes& bytes) {
    if(bytes.size()>1&&bytes[0]==0x1f&&bytes[1]==0x8b)return true;
    const std::string_view xml(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    size_t position=0;
    while((position=xml.find('<',position))!=std::string_view::npos) {
        if(xml.substr(position,4)=="<!--"){auto end=xml.find("-->",position+4);if(end==std::string_view::npos)return true;position=end+3;continue;}
        if(xml.substr(position,9)=="<![CDATA["){auto end=xml.find("]]>",position+9);if(end==std::string_view::npos)return true;position=end+3;continue;}
        if(xml.substr(position,2)=="<?"){auto end=xml.find("?>",position+2);if(end==std::string_view::npos)return true;position=end+2;continue;}
        // A DTD can supply entity-expanded text markup. Let resvg validate it,
        // and conservatively retain font support for this uncommon case.
        if(xml.substr(position,2)=="<!")return true;
        ++position;if(position<xml.size()&&xml[position]=='/')++position;
        auto start=position;
        while(position<xml.size()){
            const auto c=static_cast<unsigned char>(xml[position]);
            if(!(std::isalnum(c)||c=='_'||c==':'||c=='-'||c=='.'))break;++position;
        }
        auto name=xml.substr(start,position-start);auto colon=name.rfind(':');if(colon!=std::string_view::npos)name.remove_prefix(colon+1);
        if(name=="text"||name=="tspan"||name=="textPath"||name=="image"||name=="use"||name=="foreignObject")return true;
        // Attribute values may contain literal '<text' in quoted strings.
        char quote=0;
        while(position<xml.size()){
            char c=xml[position++];if(quote){if(c==quote)quote=0;}
            else if(c=='\''||c=='"')quote=c;else if(c=='>')break;
        }
    }
    return false;
}
std::vector<Frame> Decode(const Input& input,resvg_options*& svgOptions,bool& svgFontsLoaded,const std::atomic<bool>* cancel) {
    Cancel(cancel);
    if(Lower(input.path.extension().u8string())==".svg") {
        if(!svgOptions){svgOptions=resvg_options_create();if(!svgOptions)throw std::bad_alloc();}
        if(!svgFontsLoaded&&SvgNeedsFonts(input.bytes)){resvg_options_load_system_fonts(svgOptions);svgFontsLoaded=true;}
        auto resources=input.path.parent_path().u8string();resvg_options_set_resources_dir(svgOptions,resources.empty()?nullptr:resources.c_str());
        resvg_render_tree* tree=nullptr;
        auto error=resvg_parse_tree_from_data(reinterpret_cast<const char*>(input.bytes.data()),input.bytes.size(),svgOptions,&tree);
        std::unique_ptr<resvg_render_tree,decltype(&resvg_tree_destroy)> owned(tree,resvg_tree_destroy);
        if(error!=RESVG_OK||!tree)throw std::invalid_argument("Cannot parse SVG image (error "+std::to_string(error)+"): "+input.name);
        auto size=resvg_get_image_size(tree);
        if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<=0||size.height<=0||size.width>65535||size.height>65535)throw std::invalid_argument("SVG dimensions are too large or invalid");
        Frame frame;frame.width=uint32_t(std::ceil(size.width));frame.height=uint32_t(std::ceil(size.height));Dimensions(frame.width,frame.height);
        frame.pixels.resize(size_t(frame.width)*frame.height*4);
        resvg_render(tree,resvg_transform_identity(),frame.width,frame.height,reinterpret_cast<char*>(frame.pixels.data()));
        // The native C API returns premultiplied RGBA; conversion starts with straight RGBA.
        for(size_t i=0;i<frame.pixels.size();i+=4){auto alpha=frame.pixels[i+3];for(unsigned c=0;c<3;++c)frame.pixels[i+c]=alpha?uint8_t(std::min(255u,(unsigned(frame.pixels[i+c])*255u+alpha/2u)/alpha)):0;}
        Cancel(cancel);return {std::move(frame)};
    }
    if(NativeApng::IsAnimated(input.bytes)){
        auto decoded=NativeApng::Decode(input.bytes,cancel);std::vector<Frame> frames;frames.reserve(decoded.size());
        for(auto& frame:decoded)frames.push_back({frame.width,frame.height,std::max(20u,frame.delayMs),std::move(frame.pixels)});
        return frames;
    }
    if(input.bytes.size()>INT_MAX)throw std::invalid_argument("Image input is too large");
    const bool gif=GifSignature(input.bytes);
    const auto gifFrames=gif?PreflightGif(input.bytes,cancel):0;
    int width=0,height=0,channels=0,count=1;
    if(!stbi_info_from_memory(input.bytes.data(),int(input.bytes.size()),&width,&height,&channels))throw std::invalid_argument("Cannot decode image: "+input.name);
    Dimensions(uint32_t(width),uint32_t(height));
    int* frameDelays=nullptr;stbi_uc* decoded=nullptr;
    if(gif)
        decoded=stbi_load_gif_from_memory(input.bytes.data(),int(input.bytes.size()),&frameDelays,&width,&height,&count,&channels,4);
    else decoded=stbi_load_from_memory(input.bytes.data(),int(input.bytes.size()),&width,&height,&channels,4);
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(decoded,stbi_image_free);
    std::unique_ptr<int,decltype(&stbi_image_free)> delays(frameDelays,stbi_image_free);
    if(!decoded||count<1)throw std::invalid_argument(std::string("Cannot decode image: ")+(stbi_failure_reason()?stbi_failure_reason():input.name));
    Dimensions(uint32_t(width),uint32_t(height));const auto frameSize=size_t(width)*height*4;
    if(gif&&size_t(count)!=gifFrames)throw std::invalid_argument("GIF frame count changed during decoding");
    if(uint64_t(frameSize)*uint32_t(count)>(gif?256u:512u)*1024u*1024u)throw std::invalid_argument("Decoded animation is too large");
    std::vector<Frame> frames;frames.reserve(count);
    for(int index=0;index<count;++index){Cancel(cancel);Frame frame;frame.width=width;frame.height=height;frame.delay=delays?std::max(20,delays.get()[index]):100;frame.pixels.assign(decoded+size_t(index)*frameSize,decoded+size_t(index+1)*frameSize);frames.push_back(std::move(frame));}
    return frames;
}
constexpr uint8_t redThreshold[64]={1,7,3,5,0,8,2,6,7,1,5,3,8,0,6,2,3,5,0,8,2,6,1,7,5,3,8,0,6,2,7,1,0,8,2,6,1,7,3,5,8,0,6,2,7,1,5,3,2,6,1,7,3,5,0,8,6,2,7,1,5,3,8,0};
constexpr uint8_t greenThreshold[64]={1,3,2,2,3,1,2,2,2,2,0,4,2,2,4,0,3,1,2,2,1,3,2,2,2,2,4,0,2,2,0,4,1,3,2,2,3,1,2,2,2,2,0,4,2,2,4,0,3,1,2,2,1,3,2,2,2,2,4,0,2,2,0,4};
constexpr uint8_t blueThreshold[64]={5,3,8,0,6,2,7,1,3,5,0,8,2,6,1,7,8,0,6,2,7,1,5,3,0,8,2,6,1,7,3,5,6,2,7,1,5,3,8,0,2,6,1,7,3,5,0,8,7,1,5,3,8,0,6,2,1,7,3,5,0,8,2,6};
uint8_t Composite(uint8_t value,uint8_t alpha,uint8_t background){return uint8_t((unsigned(value)*alpha+unsigned(background)*(255-alpha))/255);}
uint8_t Luminance(uint8_t red,uint8_t green,uint8_t blue){
    auto linear=[](double x){x/=255;return x<0.04045?x/12.92:std::pow((x+0.055)/1.055,2.4);};
    double light=0.2126*linear(red)+0.7152*linear(green)+0.0722*linear(blue);
    return uint8_t(std::clamp((light<=0.0031308?12.92*light:1.055*std::pow(light,1.0/2.4)-0.055)*255,0.0,255.0));
}
uint8_t Expand(unsigned value,unsigned bits){return uint8_t(value*255/((1u<<bits)-1));}
void SetBits(uint8_t* row,uint32_t x,unsigned bits,uint8_t value){row[(size_t(x)*bits)/8]|=uint8_t(value<<(8-bits-(size_t(x)*bits)%8));}
int QuantizeProgress(float,void* data){auto* cancel=static_cast<const std::atomic<bool>*>(data);return !cancel||!cancel->load();}
struct Indexed {std::vector<liq_color> palette;Bytes indices;};
struct QuantizerAttributesDeleter {void operator()(liq_attr* value)const{liq_attr_destroy(value);}};
struct QuantizerImageDeleter {void operator()(liq_image* value)const{liq_image_destroy(value);}};
struct QuantizerResultDeleter {void operator()(liq_result* value)const{liq_result_destroy(value);}};
Indexed Quantize(const Frame& frame,unsigned maxColors,bool dither,const std::atomic<bool>* cancel){
    std::unique_ptr<liq_attr,QuantizerAttributesDeleter> attributes(liq_attr_create());if(!attributes)throw std::bad_alloc();
    if(liq_set_max_colors(attributes.get(),int(maxColors))!=LIQ_OK)throw std::runtime_error("Invalid palette size");
    liq_attr_set_progress_callback(attributes.get(),QuantizeProgress,const_cast<std::atomic<bool>*>(cancel));
    std::unique_ptr<liq_image,QuantizerImageDeleter> image(liq_image_create_rgba(attributes.get(),frame.pixels.data(),int(frame.width),int(frame.height),0));if(!image)throw std::bad_alloc();
    liq_result* raw=nullptr;auto error=liq_image_quantize(image.get(),attributes.get(),&raw);
    std::unique_ptr<liq_result,QuantizerResultDeleter> result(raw);Cancel(cancel);
    if(error!=LIQ_OK||!result)throw std::runtime_error("Image palette quantization failed");
    liq_result_set_progress_callback(result.get(),QuantizeProgress,const_cast<std::atomic<bool>*>(cancel));
    if(liq_set_dithering_level(result.get(),dither?1.0f:0.0f)!=LIQ_OK)throw std::runtime_error("Cannot configure indexed dithering");
    Indexed indexed;indexed.indices.resize(size_t(frame.width)*frame.height);
    error=liq_write_remapped_image(result.get(),image.get(),indexed.indices.data(),indexed.indices.size());Cancel(cancel);
    if(error!=LIQ_OK)throw std::runtime_error("Image palette remapping failed");
    const auto* palette=liq_get_palette(result.get());if(!palette||!palette->count)throw std::runtime_error("Image has an empty palette");
    indexed.palette.assign(palette->entries,palette->entries+palette->count);return indexed;
}
void Prepare(Frame& frame,const json& options){
    const bool includeAlpha=options.at("includeAlpha"),keyEnabled=options.at("chromaKey");
    const uint32_t background=options.at("background"),key=options.at("chromaColor");
    for(size_t i=0;i<frame.pixels.size();i+=4){auto* p=frame.pixels.data()+i;
        if(!includeAlpha){for(unsigned c=0;c<3;++c)p[c]=Composite(p[c],p[3],uint8_t(background>>(16-c*8)));p[3]=255;}
        if(keyEnabled&&((uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|p[2])==(key&0xffffff))p[3]=0;
    }
}
Encoded Pack(const Frame& frame,const Format& requested,bool automatic,const json& options,const std::atomic<bool>* cancel){
    Encoded image;image.format=requested;image.width=frame.width;image.height=frame.height;Indexed indexed;
    if(automatic||requested.palette){indexed=Quantize(frame,automatic?256u:(1u<<requested.bits),options.at("dither"),cancel);if(automatic)image.format=Find(indexed.palette.size()<=2?"I1":indexed.palette.size()<=4?"I2":indexed.palette.size()<=16?"I4":"I8");}
    auto format=image.format;const auto paletteCount=format.palette?(1u<<format.bits):0;
    const uint32_t natural=(frame.width*format.bits+7)/8,align=options.at("align");const uint32_t stride=((natural+align-1)/align)*align;
    if(stride>65535)throw std::invalid_argument("Aligned image stride exceeds the LVGL binary header limit");
    image.stride=stride;size_t paletteBytes=paletteCount*4;image.data.resize(paletteBytes+size_t(stride)*frame.height+(format.code==20?size_t(stride/2)*frame.height:0));image.preview.resize(frame.pixels.size());
    for(unsigned p=0;p<paletteCount;++p){const auto color=p<indexed.palette.size()?indexed.palette[p]:liq_color{255,255,255,0};auto* out=image.data.data()+p*4;out[0]=color.b;out[1]=color.g;out[2]=color.r;out[3]=color.a;}
    const uint32_t background=options.at("background");bool dither=options.at("dither");
    for(uint32_t y=0;y<frame.height;++y){
        if((y&31)==0)Cancel(cancel);auto* row=image.data.data()+paletteBytes+size_t(y)*stride;
        for(uint32_t x=0;x<frame.width;++x){
            const auto offset=(size_t(y)*frame.width+x)*4;const auto* input=frame.pixels.data()+offset;auto* preview=image.preview.data()+offset;
            unsigned r=input[0],g=input[1],b=input[2],a=input[3];
            if(format.palette){uint8_t index=indexed.indices[size_t(y)*frame.width+x];SetBits(row,x,format.bits,index);const auto color=indexed.palette[index];preview[0]=color.r;preview[1]=color.g;preview[2]=color.b;preview[3]=color.a;continue;}
            if(format.code>=11&&format.code<=14){const unsigned alpha=a>>(8-format.bits);SetBits(row,x,format.bits,uint8_t(alpha));preview[0]=preview[1]=preview[2]=0;preview[3]=Expand(alpha,format.bits);continue;}
            if(dither&&(format.code==18||format.code==19||format.code==20||format.code==27)){unsigned threshold=((y&7)<<3)|(x&7);r=std::min(r+redThreshold[threshold],255u)&0xf8;g=std::min(g+greenThreshold[threshold],255u)&0xfc;b=std::min(b+blueThreshold[threshold],255u)&0xf8;}
            if(format.code==6||format.code==15||format.code==17||format.code==18||format.code==27){r=Composite(uint8_t(r),uint8_t(a),uint8_t(background>>16));g=Composite(uint8_t(g),uint8_t(a),uint8_t(background>>8));b=Composite(uint8_t(b),uint8_t(a),uint8_t(background));a=255;}
            preview[0]=uint8_t(r);preview[1]=uint8_t(g);preview[2]=uint8_t(b);preview[3]=uint8_t(a);
            if(format.code==6||format.code==21){uint8_t luma=Luminance(uint8_t(r),uint8_t(g),uint8_t(b));row[x*(format.code==21?2:1)]=luma;if(format.code==21)row[x*2+1]=uint8_t(a);preview[0]=preview[1]=preview[2]=luma;}
            else if(format.code==16||format.code==17||format.code==26){auto* p=row+x*4;p[0]=uint8_t(b);p[1]=uint8_t(g);p[2]=uint8_t(r);p[3]=uint8_t(a);if(format.code==26)for(unsigned c=0;c<3;++c)p[c]=uint8_t(unsigned(p[c])*a/255);}
            else if(format.code==15){auto* p=row+x*3;p[0]=uint8_t(b);p[1]=uint8_t(g);p[2]=uint8_t(r);}
            else {const uint16_t pixel=uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));auto* p=row+x*(format.code==19?3:2);p[format.code==27?1:0]=uint8_t(pixel);p[format.code==27?0:1]=uint8_t(pixel>>8);if(format.code==19)p[2]=uint8_t(a);if(format.code==20)image.data[size_t(stride)*frame.height+size_t(y)*(stride/2)+x]=uint8_t(a);preview[0]=Expand((pixel>>11)&31,5);preview[1]=Expand((pixel>>5)&63,6);preview[2]=Expand(pixel&31,5);}
        }
    }
    if(options.at("premultiply").get<bool>()&&format.code!=26){
        image.flags|=1;
        if(format.palette)for(unsigned p=0;p<paletteCount;++p)for(unsigned c=0;c<3;++c)image.data[p*4+c]=uint8_t(unsigned(image.data[p*4+c])*image.data[p*4+3]>>8);
        else for(uint32_t y=0;y<frame.height;++y)for(uint32_t x=0;x<frame.width;++x){auto* p=image.data.data()+size_t(y)*stride+x*(format.code==16?4:format.code==19?3:2);const unsigned a=format.code==20?image.data[size_t(stride)*frame.height+size_t(y)*(stride/2)+x]:p[format.code==16?3:2];if(format.code==16)for(unsigned c=0;c<3;++c)p[c]=uint8_t(unsigned(p[c])*a>>8);else{unsigned value=p[0]|unsigned(p[1])<<8;value=((((value>>11)&31)*a/255)<<11)|((((value>>5)&63)*a/255)<<5)|((value&31)*a/255);p[0]=uint8_t(value);p[1]=uint8_t(value>>8);}}
    }
    if(options.at("nemaGfx").get<bool>())for(size_t i=paletteBytes;i<image.data.size();++i)image.data[i]=uint8_t((image.data[i]>>4)|(image.data[i]<<4));
    return image;
}
void Word(Bytes& out,uint32_t word,unsigned count){for(unsigned i=0;i<count;++i)out.push_back(uint8_t(word>>(i*8)));}
Bytes Compress(const Bytes& data,const Format& format,const std::string& compression){
    if(compression=="NONE")return data;Bytes encoded;
    if(compression=="LZ4"){if(data.size()>INT_MAX)throw std::invalid_argument("Image data is too large for LZ4");encoded.resize(LZ4_compressBound(int(data.size())));int length=LZ4_compress_default(reinterpret_cast<const char*>(data.data()),reinterpret_cast<char*>(encoded.data()),int(data.size()),int(encoded.size()));if(length<=0)throw std::runtime_error("Image LZ4 compression failed");encoded.resize(length);}
    else{
        const unsigned block=(format.bits+7)/8;Bytes padded=data;padded.resize(((padded.size()+block-1)/block)*block);const size_t units=padded.size()/block;
        auto repeat=[&](size_t index){size_t length=1;while(index+length<units&&length<127&&!std::memcmp(padded.data()+index*block,padded.data()+(index+length)*block,block)){if(data.size()%block&&index+length==units-1)break;++length;}return length;};
        for(size_t index=0;index<units;){size_t length=repeat(index);if(length>=3){encoded.push_back(uint8_t(length));encoded.insert(encoded.end(),padded.begin()+index*block,padded.begin()+(index+1)*block);index+=length;}else{const size_t start=index;index+=length;while(index<units&&index-start<127){length=repeat(index);if(length>=3)break;index+=std::min(length,127-(index-start));}encoded.push_back(uint8_t(0x80|(index-start)));encoded.insert(encoded.end(),padded.begin()+start*block,padded.begin()+index*block);}}
    }
    Bytes result;Word(result,compression=="RLE"?1:2,4);Word(result,uint32_t(encoded.size()),4);Word(result,uint32_t(data.size()),4);result.insert(result.end(),encoded.begin(),encoded.end());return result;
}
Bytes Binary(const Encoded& image,const Bytes& data){Bytes out;out.reserve(12+data.size());out.push_back(0x19);out.push_back(image.format.code);Word(out,image.flags,2);Word(out,image.width,2);Word(out,image.height,2);Word(out,image.stride,2);Word(out,0,2);out.insert(out.end(),data.begin(),data.end());return out;}
Bytes CSource(const std::string& name,const Encoded& image,const Bytes& data){
    auto attribute="LV_ATTRIBUTE_"+name;for(auto& c:attribute)c=char(std::toupper(static_cast<unsigned char>(c)));
    std::ostringstream out;out<<"/* Native LVGL image conversion. */\n#if defined(__has_include)\n# if __has_include(\"lvgl.h\")\n#  include \"lvgl.h\"\n# else\n#  include \"lvgl/lvgl.h\"\n# endif\n#else\n# include \"lvgl/lvgl.h\"\n#endif\n#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n#ifndef LV_ATTRIBUTE_LARGE_CONST\n#define LV_ATTRIBUTE_LARGE_CONST\n#endif\n#ifndef "<<attribute<<"\n#define "<<attribute<<"\n#endif\n\nstatic const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST "<<attribute<<" uint8_t "<<name<<"_map[] = {";
    const auto header=out.str();Bytes source;source.reserve(header.size()+data.size()*6+512);source.insert(source.end(),header.begin(),header.end());
    constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<data.size();++i){if(i%16==0)source.insert(source.end(),{'\n',' ',' ',' ',' '});source.insert(source.end(),{'0','x',uint8_t(digits[data[i]>>4]),uint8_t(digits[data[i]&15]),','});}
    out.str(std::string{});out.clear();
    const bool raw=image.format.code<=2;out<<std::dec<<"\n};\n\nconst lv_image_dsc_t "<<name<<" = {\n    .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_"<<image.format.name<<", .flags = "<<image.flags<<", .w = "<<(raw?0:image.width)<<", .h = "<<(raw?0:image.height)<<", .stride = "<<image.stride<<", .reserved_2 = 0 },\n    .data_size = sizeof("<<name<<"_map),\n    .data = "<<name<<"_map,\n    .reserved = NULL,\n};\n";
    const auto ending=out.str();source.insert(source.end(),ending.begin(),ending.end());return source;
}
bool Identifier(const std::string& name){
    return CIdentifier::Valid(name);
}
std::string Symbol(const Input& input,const std::string& override,std::set<std::string>& names){
    auto name=override.empty()?std::filesystem::u8path(input.name).stem().u8string():override;
    if(override.empty()){for(auto& c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))c='_';if(!Identifier(name))name="image_"+name;}
    auto stem=name;unsigned suffix=2;while(!names.insert(Lower(name)).second)name=stem+"_"+std::to_string(suffix++);return name;
}
void Write(const std::filesystem::path& path,const Bytes& bytes){
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);if(!stream)throw std::runtime_error("Cannot create converted image: "+path.u8string());stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());stream.close();if(!stream)throw std::runtime_error("Cannot write converted image: "+path.u8string());
}
void WriteOutputs(const std::filesystem::path& output,const std::vector<Input>& inputs,const std::vector<Output>& files,std::vector<std::filesystem::path>& written,const std::atomic<bool>* cancel){
    if(output.empty())return;const auto root=std::filesystem::absolute(output).lexically_normal();std::filesystem::create_directories(root);
    for(const auto& file:files){auto destination=root/std::filesystem::u8path(file.name);if(std::filesystem::is_directory(destination))throw std::runtime_error("Image output name is an existing directory");if(std::filesystem::exists(destination))for(const auto& input:inputs)if(!input.path.empty()&&std::filesystem::exists(input.path)&&std::filesystem::equivalent(input.path,destination))throw std::runtime_error("Image output would overwrite an input");}
    static std::atomic<uint64_t> sequence{0};auto stage=root/(".uicreator-images-"+std::to_string(sequence++));
    while(!std::filesystem::create_directory(stage))stage=root/(".uicreator-images-"+std::to_string(sequence++));
    struct Change {std::filesystem::path destination,backup;bool installed=false;};std::vector<Change> changes;
    try{
        for(size_t i=0;i<files.size();++i){Cancel(cancel);Write(stage/std::to_string(i),files[i].bytes);}
        for(size_t i=0;i<files.size();++i){Cancel(cancel);changes.push_back({root/std::filesystem::u8path(files[i].name),stage/("backup-"+std::to_string(i)),false});auto& current=changes.back();if(std::filesystem::exists(current.destination))std::filesystem::rename(current.destination,current.backup);std::filesystem::rename(stage/std::to_string(i),current.destination);current.installed=true;}
        for(const auto& change:changes)written.push_back(change.destination);
    }catch(...){
        bool restored=true;
        for(auto it=changes.rbegin();it!=changes.rend();++it){
            std::error_code error;
            if(it->installed){std::filesystem::remove(it->destination,error);if(error)restored=false;}
            error.clear();
            if(std::filesystem::exists(it->backup,error)){error.clear();std::filesystem::rename(it->backup,it->destination,error);if(error)restored=false;}
            else if(error)restored=false;
        }
        if(!restored)throw std::runtime_error("Cannot restore an existing output; its backup was preserved in "+stage.u8string());
        std::error_code ignored;std::filesystem::remove_all(stage,ignored);throw;
    }
    std::error_code ignored;std::filesystem::remove_all(stage,ignored);
}
}
Result Convert(const std::vector<std::filesystem::path>& sources,const json& options,const std::filesystem::path& output,const std::atomic<bool>* cancel,const json& embedded){
    Result result;resvg_options* svgOptions=nullptr;bool svgFontsLoaded=false;
    try{
        Cancel(cancel);if((!options.is_object()&&!options.is_null())||!embedded.is_array())throw std::invalid_argument("Invalid image conversion request");auto config=Defaults();if(!options.is_null())config.update(options);
        auto formatName=config.at("colorFormat").get<std::string>();bool automatic=formatName=="AUTO";Format format=automatic?Find("I8"):Find(formatName);
        const auto compression=config.at("compression").get<std::string>(),exportFormat=config.at("format").get<std::string>();
        if(compression!="NONE"&&compression!="RLE"&&compression!="LZ4")throw std::invalid_argument("Unknown image compression method");
        if(exportFormat!="C"&&exportFormat!="BIN"&&exportFormat!="PNG")throw std::invalid_argument("Choose C, BIN, or PNG output");
        if(!config.at("align").is_number_integer()||config.at("align").get<int64_t>()<1||config.at("align").get<int64_t>()>4096)throw std::invalid_argument("Stride alignment must be between 1 and 4096 bytes");
        for(const char* key:{"background","chromaColor"})if(!config.at(key).is_number_integer()||config.at(key).get<int64_t>()<0||config.at(key).get<uint64_t>()>0xffffff)throw std::invalid_argument(std::string("Invalid RGB color: ")+key);
        for(const char* key:{"premultiply","dither","includeAlpha","chromaKey","bigEndian","nemaGfx","verbose"})if(!config.at(key).is_boolean())throw std::invalid_argument(std::string("Invalid checkbox option: ")+key);
        if(config.at("bigEndian").get<bool>()){if(formatName!="RGB565"&&formatName!="RGB565_SWAPPED")throw std::invalid_argument("LVGL 9 big-endian pixel output requires RGB565");format=Find("RGB565_SWAPPED");}
        const bool raw=format.code<=2;
        if(raw&&(compression!="NONE"||config.at("align")!=1||config.at("premultiply").get<bool>()))throw std::invalid_argument("RAW images cannot use pixel compression, stride alignment, or premultiplication");
        if(config.at("nemaGfx").get<bool>()&&formatName!="I8")throw std::invalid_argument("NEMA palette layout requires I8");
        if(config.at("chromaKey").get<bool>()&&(format.code==6||format.code==15||format.code==17||format.code==18||format.code==27))throw std::invalid_argument("Chroma-key transparency requires an alpha or indexed color format");
        if(config.at("premultiply").get<bool>()&&!automatic&&!format.palette&&format.code!=16&&format.code!=19&&format.code!=20&&format.code!=26)throw std::invalid_argument("Premultiplication is unsupported for this color format");
        std::vector<std::filesystem::path> paths;const std::set<std::string> extensions={".png",".jpg",".jpeg",".bmp",".gif",".svg"};
        for(const auto& source:sources){Cancel(cancel);if(!std::filesystem::exists(source))throw std::invalid_argument("Image input not found: "+source.u8string());if(std::filesystem::is_directory(source)){std::vector<std::filesystem::path> batch;for(const auto& item:std::filesystem::recursive_directory_iterator(source))if(item.is_regular_file()&&extensions.count(Lower(item.path().extension().u8string())))batch.push_back(std::filesystem::absolute(item.path()));std::sort(batch.begin(),batch.end());paths.insert(paths.end(),batch.begin(),batch.end());}else paths.push_back(std::filesystem::absolute(source));}
        if(paths.empty()&&embedded.empty())throw std::invalid_argument("Select at least one image file or a folder containing images");
        std::vector<Input> inputs;inputs.reserve(paths.size()+embedded.size());for(const auto& path:paths){Cancel(cancel);inputs.push_back({path,path.filename().u8string(),{}});}
        for(const auto& asset:embedded){Cancel(cancel);auto name=std::filesystem::u8path(asset.at("name").get<std::string>()).filename();const auto bytes=base64_decode(asset.at("sourceData").get<std::string>());if(bytes.empty()||bytes.size()>512u*1024u*1024u)throw std::invalid_argument("Embedded image source is empty or too large");inputs.push_back({name,name.u8string(),Bytes(bytes.begin(),bytes.end())});}
        const auto override=config.at("name").get<std::string>();if(!override.empty()&&(inputs.size()!=1||!Identifier(override)))throw std::invalid_argument("Image symbol must be a non-reserved C identifier for one input file");
        json assets=json::array();std::vector<Output> files;std::set<std::string> names;size_t frameOutputs=0;
        for(auto& input:inputs){
            if(input.bytes.empty())input.bytes=Read(input.path);
            auto name=Symbol(input,override,names);auto frames=Decode(input,svgOptions,svgFontsLoaded,cancel);
            json asset={{"name",input.name},{"sourceData",Base64(input.bytes)},{"conversion",config},{"deviceFrames",json::array()},{"previewFrames",json::array()},{"delays",json::array()}};
            for(size_t index=0;index<frames.size();++index){
                Cancel(cancel);auto& frame=frames[index];Prepare(frame,config);auto frameName=frames.size()==1?name:name+"_frame_"+std::to_string(index);
                if(frames.size()>1){auto stem=frameName;unsigned suffix=2;while(!names.insert(Lower(frameName)).second)frameName=stem+"_"+std::to_string(suffix++);}
                Encoded image;Bytes data,preview;
                if(raw){preview=NativePng::Encode(frame.pixels.data(),frame.width,frame.height);image.format=format;image.width=frame.width;image.height=frame.height;const auto extension=Lower(input.path.extension().u8string());data=(frames.size()==1&&extension!=".svg"&&extension!=".bmp")?input.bytes:preview;if(!output.empty())files.push_back({frameName+".c",CSource(frameName,image,data)});}
                else{
                    image=Pack(frame,format,automatic,config,cancel);preview=NativePng::Encode(image.preview.data(),frame.width,frame.height);Bytes{}.swap(image.preview);
                    if(compression=="NONE")data=std::move(image.data);else{data=Compress(image.data,image.format,compression);Bytes{}.swap(image.data);image.flags|=8;}
                    if(!output.empty()){if(exportFormat=="C")files.push_back({frameName+".c",CSource(frameName,image,data)});else if(exportFormat=="BIN")files.push_back({frameName+".bin",Binary(image,data)});else files.push_back({frameName+".png",preview});}
                }
                asset["deviceFrames"].push_back({{"formatVersion",9},{"cf",image.format.code},{"w",frame.width},{"h",frame.height},{"stride",image.stride},{"flags",image.flags},{"data",Base64(data)}});
                asset["previewFrames"].push_back(Base64(preview));asset["delays"].push_back(frame.delay);++frameOutputs;Bytes{}.swap(frame.pixels);
            }
            assets.push_back(std::move(asset));Bytes{}.swap(input.bytes);
        }
        Cancel(cancel);WriteOutputs(output,inputs,files,result.files,cancel);result.assets=std::move(assets);result.success=true;
        result.message="Converted "+std::to_string(result.assets.size())+" image(s). Settings and converted data can be applied to an image widget.";
        if(config.at("verbose").get<bool>())result.message+="\nNative C++ conversion; "+std::to_string(frameOutputs)+" frame output(s), "+compression+" compression, resvg " RESVG_VERSION ".";
    }catch(const std::exception& error){result.message=error.what();}
    if(svgOptions)resvg_options_destroy(svgOptions);return result;
}
void Apply(lv_obj_t* image,const json& asset){if(!image||lv_obj_get_class(image)!=&lv_image_class)throw std::invalid_argument("Select an Image widget first");Serialization::ImageAsset::Restore(image,asset);}
}
