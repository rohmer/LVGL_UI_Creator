#include "ImageConverter.h"
#include <NativePng.h>
#include <Serialization/Base64.h>
#include <src/libs/lz4/lz4.h>
#include <src/libs/rle/lv_rle_private.h>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>

#define CHECK(value) do {if(!(value))throw std::runtime_error("Check failed at line "+std::to_string(__LINE__)+": " #value);}while(0)
using Bytes=std::vector<uint8_t>;
namespace {
void Write(const std::filesystem::path& path,const Bytes& bytes) {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(bool(file));}
void Write(const std::filesystem::path& path,const std::string& text) {Write(path,Bytes(text.begin(),text.end()));}
Bytes Read(const std::filesystem::path& path) {std::ifstream file(path,std::ios::binary);CHECK(bool(file));return {(std::istreambuf_iterator<char>(file)),{}};}
uint32_t Word(const Bytes& data,size_t index,unsigned length) {CHECK(index+length<=data.size());uint32_t value=0;for(unsigned i=0;i<length;++i)value|=uint32_t(data[index+i])<<(8*i);return value;}
Bytes Payload(const json& frame) {
    auto text=base64_decode(frame.at("data"));Bytes data(text.begin(),text.end());
    if(!(frame.at("flags").get<unsigned>()&8))return data;
    auto method=Word(data,0,4),compressed=Word(data,4,4),size=Word(data,8,4);CHECK(compressed==data.size()-12);
    Bytes decoded(size);
    if(method==2)CHECK(LZ4_decompress_safe(reinterpret_cast<const char*>(data.data()+12),reinterpret_cast<char*>(decoded.data()),int(compressed),int(size))==int(size));
    else {CHECK(method==1);unsigned bits=lv_color_format_get_bpp(static_cast<lv_color_format_t>(frame.at("cf").get<int>()));CHECK(lv_rle_decompress(data.data()+12,compressed,decoded.data(),size,(bits+7)/8)==size);}
    return decoded;
}
ImageConversion::Result Convert(const std::filesystem::path& path,const json& options,const std::filesystem::path& output={}) {
    auto result=ImageConversion::Convert({path},options,output);if(!result.success)throw std::runtime_error(result.message);CHECK(result.assets.size()==1);return result;
}
Bytes DisposalGif() {
    Bytes bytes={'G','I','F','8','9','a',2,0,1,0,0x81,0,0,255,255,255,255,0,0,0,255,0,0,0,255};
    auto frame=[&](uint8_t disposal,uint8_t left,uint8_t width,uint8_t first,uint8_t second){
        const uint8_t control[]={0x21,0xf9,4,uint8_t(disposal<<2),5,0,0,0,0x2c,left,0,0,0,width,0,1,0,0,2,2,first,second,0};
        bytes.insert(bytes.end(),std::begin(control),std::end(control));
    };
    frame(1,0,2,0x8c,0x0a); // Red, green.
    frame(3,0,1,0x5c,0x01); // Temporarily paint blue at the left.
    frame(1,1,1,0x5c,0x01); // Restore red on the left, paint blue at the right.
    bytes.push_back(0x3b);return bytes;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)return 2;const std::filesystem::path repo=argv[1],output=argv[2];std::filesystem::create_directories(output);
        // All conversions below deliberately run without lv_init(): native jobs
        // must not borrow the LVGL heap or require a display/UI-thread lock.
        const uint32_t width=13,height=3;Bytes rgba(size_t(width)*height*4);
        for(size_t i=0;i<rgba.size();i+=4){rgba[i]=uint8_t(17+i*3);rgba[i+1]=uint8_t(43+i*7);rgba[i+2]=uint8_t(91+i*11);rgba[i+3]=uint8_t((i/4)%3==0?0:(i/4)%3==1?128:255);}
        rgba[0]=255;rgba[1]=0;rgba[2]=0;rgba[3]=255;
        auto png=output/"pixels.png";Write(png,NativePng::Encode(rgba.data(),width,height));
        const std::vector<std::string> formats={"ARGB8888","XRGB8888","RGB888","RGB565","RGB565_SWAPPED","RGB565A8","ARGB8565","ARGB8888_PREMULTIPLIED","L8","AL88","A1","A2","A4","A8","I1","I2","I4","I8","AUTO"};
        for(const auto& format:formats) {
            auto plain=Convert(png,{{"colorFormat",format},{"align",7},{"dither",false}});
            const auto& frame=plain.assets[0]["deviceFrames"][0];auto expected=Payload(frame);
            auto bpp=lv_color_format_get_bpp(static_cast<lv_color_format_t>(frame.at("cf").get<int>()));
            size_t palette=LV_COLOR_INDEXED_PALETTE_SIZE(static_cast<lv_color_format_t>(frame.at("cf").get<int>()))*4;
            CHECK(frame["stride"].get<unsigned>()%7==0);
            size_t size=palette+frame["stride"].get<unsigned>()*height;
            if(format=="RGB565A8")size+=(frame["stride"].get<unsigned>()/2)*height;
            CHECK(expected.size()==size);
            CHECK(!Serialization::ImageAsset::Decode(plain.assets[0]).pixels.empty());
            for(const char* method:{"RLE","LZ4"}) {
                auto compressed=Convert(png,{{"colorFormat",format},{"align",7},{"dither",false},{"compression",method}});
                CHECK(Payload(compressed.assets[0]["deviceFrames"][0])==expected);
            }
        }
        auto direct=Convert(png,{{"dither",false}});auto pixels=Payload(direct.assets[0]["deviceFrames"][0]);
        for(size_t i=0;i<rgba.size();i+=4){CHECK(pixels[i]==rgba[i+2]);CHECK(pixels[i+1]==rgba[i+1]);CHECK(pixels[i+2]==rgba[i]);CHECK(pixels[i+3]==rgba[i+3]);}
        auto premultiplied=Convert(png,{{"premultiply",true}});
        auto premul=Payload(premultiplied.assets[0]["deviceFrames"][0]);CHECK(premultiplied.assets[0]["deviceFrames"][0]["flags"]==1);
        for(size_t i=0;i<rgba.size();i+=4){CHECK(premul[i]==(unsigned(rgba[i+2])*rgba[i+3]>>8));CHECK(premul[i+3]==rgba[i+3]);}
        auto transparent=Convert(png,{{"chromaKey",true},{"chromaColor",0xff0000}});CHECK(Payload(transparent.assets[0]["deviceFrames"][0])[3]==0);
        auto opaque=Convert(png,{{"includeAlpha",false},{"background",0x00ff00}});auto opaquePixels=Payload(opaque.assets[0]["deviceFrames"][0]);
        for(size_t i=3;i<opaquePixels.size();i+=4)CHECK(opaquePixels[i]==255);
        auto swapped=Convert(png,{{"colorFormat","RGB565"},{"bigEndian",true},{"dither",false}});CHECK(swapped.assets[0]["deviceFrames"][0]["cf"]==LV_COLOR_FORMAT_RGB565_SWAPPED);
        auto indexed=Convert(png,{{"colorFormat","I8"},{"dither",false}}),nema=Convert(png,{{"colorFormat","I8"},{"dither",false},{"nemaGfx",true}});
        auto normalData=Payload(indexed.assets[0]["deviceFrames"][0]),nemaData=Payload(nema.assets[0]["deviceFrames"][0]);CHECK(normalData.size()==nemaData.size());
        for(size_t i=1024;i<normalData.size();++i)CHECK(nemaData[i]==uint8_t((normalData[i]>>4)|(normalData[i]<<4)));
        for(const char* format:{"RAW","RAW_ALPHA"}) {auto raw=Convert(png,{{"colorFormat",format}});CHECK(Payload(raw.assets[0]["deviceFrames"][0])==Read(png));}
        auto embedded=ImageConversion::Convert({},{{"colorFormat","ARGB8888"}},{},nullptr,json::array({direct.assets[0]}));CHECK(embedded.success);CHECK(embedded.assets[0]["sourceData"]==direct.assets[0]["sourceData"]);
        auto binary=Convert(png,{{"format","BIN"},{"compression","LZ4"}},output/"binary");CHECK(binary.files.size()==1);auto bin=Read(binary.files[0]);
        CHECK(bin[0]==0x19);CHECK(Word(bin,4,2)==width);CHECK(Word(bin,6,2)==height);CHECK(Word(bin,2,2)&8);
        auto exported=Convert(png,{{"format","PNG"}},output/"preview");auto previewBytes=base64_decode(exported.assets[0]["previewFrames"][0]);CHECK(Read(exported.files[0])==Bytes(previewBytes.begin(),previewBytes.end()));
        auto unicode=output/std::filesystem::u8path("image_\xe6\xb8\xac\xe8\xa9\xa6.png");std::filesystem::copy_file(png,unicode,std::filesystem::copy_options::overwrite_existing);CHECK(Convert(unicode,{}).success);
        auto animation=Convert(repo/"Tests/Fixtures/Images/sample.gif",{{"colorFormat","RGB565A8"},{"compression","RLE"}},output/"gif");CHECK(animation.assets[0]["deviceFrames"].size()==2);CHECK(animation.assets[0]["delays"].size()==2);
        for(const auto& frame:animation.assets[0]["deviceFrames"])CHECK(!Payload(frame).empty());
        auto disposalPath=output/"restore-previous.gif";Write(disposalPath,DisposalGif());auto disposal=Convert(disposalPath,{});CHECK(disposal.assets[0]["deviceFrames"].size()==3);
        CHECK(Payload(disposal.assets[0]["deviceFrames"][2])==Bytes({0,0,255,255,255,0,0,255}));
        CHECK(disposal.assets[0]["delays"]==json::array({50,50,50}));
        // A tiny, structurally valid stream can request gigabytes of composed
        // frames. It must fail in preflight, before STB allocates its canvas.
        auto smallGif=DisposalGif();Bytes largeCanvas(smallGif.begin(),smallGif.begin()+25);
        largeCanvas[6]=largeCanvas[8]=0;largeCanvas[7]=largeCanvas[9]=16; // 4096 x4096 logical canvas.
        for(unsigned i=0;i<100;++i)largeCanvas.insert(largeCanvas.end(),smallGif.begin()+25,smallGif.begin()+48);
        largeCanvas.push_back(0x3b);auto oversizedGif=output/"oversized.gif";Write(oversizedGif,largeCanvas);
        auto oversized=ImageConversion::Convert({oversizedGif},{},output/"oversized-export");
        CHECK(!oversized.success);CHECK(oversized.assets.empty());CHECK(oversized.message.find("256 MiB decoded-frame limit")!=std::string::npos);CHECK(!std::filesystem::exists(output/"oversized-export"));
        Bytes truncatedGif(smallGif.begin(),smallGif.begin()+14);Write(output/"truncated.gif",truncatedGif);
        CHECK(!ImageConversion::Convert({output/"truncated.gif"},{}).success);
        auto svg=output/"filtered.svg";Write(svg,std::string("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"8\"><defs><filter id=\"blur\"><feGaussianBlur stdDeviation=\"1\"/></filter></defs><rect x=\"4\" y=\"2\" width=\"8\" height=\"4\" fill=\"red\" filter=\"url(#blur)\"/></svg>"));
        auto vector=Convert(svg,{});CHECK(vector.assets[0]["deviceFrames"][0]["w"]==16);auto vectorPixels=Payload(vector.assets[0]["deviceFrames"][0]);bool fractional=false;
        for(size_t i=3;i<vectorPixels.size();i+=4)if(vectorPixels[i]>0&&vectorPixels[i]<255)fractional=true;CHECK(fractional);
        auto textSvg=output/"text.svg";Write(textSvg,std::string("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"96\" height=\"24\"><text x=\"1\" y=\"19\" font-family=\"DejaVu Sans,Arial,Liberation Sans,serif\" font-size=\"18\" fill=\"black\">Native</text></svg>"));
        auto vectors=ImageConversion::Convert({svg,textSvg},{});
        CHECK(vectors.success);CHECK(vectors.assets.size()==2);auto textPixels=Payload(vectors.assets[1]["deviceFrames"][0]);bool textVisible=false;
        for(size_t i=3;i<textPixels.size();i+=4)if(textPixels[i])textVisible=true;CHECK(textVisible);
        auto collisionFolder=output/"collisions";std::filesystem::create_directories(collisionFolder);std::filesystem::copy_file(repo/"Tests/Fixtures/Images/sample.gif",collisionFolder/"same.gif",std::filesystem::copy_options::overwrite_existing);std::filesystem::copy_file(png,collisionFolder/"same_frame_0.png",std::filesystem::copy_options::overwrite_existing);
        auto batch=ImageConversion::Convert({collisionFolder},{},output/"batch");CHECK(batch.success);CHECK(batch.files.size()==3);CHECK(std::set<std::filesystem::path>(batch.files.begin(),batch.files.end()).size()==3);
        auto before=Read(png);CHECK(!ImageConversion::Convert({png},{{"format","PNG"}},output).success);CHECK(Read(png)==before);
        auto keyword=output/"bool.png";std::filesystem::copy_file(png,keyword,std::filesystem::copy_options::overwrite_existing);auto named=Convert(keyword,{},output/"symbols");CHECK(named.files[0].stem()=="image_bool");
        for(const char* reserved:{"bool","true","false","NULL","class","namespace","constexpr"})CHECK(!ImageConversion::Convert({png},{{"name",reserved}}).success);
        std::atomic<bool> cancelled{true};auto stopped=ImageConversion::Convert({png},{},output/"cancelled",&cancelled);CHECK(!stopped.success);CHECK(stopped.assets.empty());CHECK(!std::filesystem::exists(output/"cancelled"));
        for(auto options:{json{{"align",0}},json{{"align",65536}},json{{"colorFormat","unknown"}},json{{"format","unknown"}},json{{"colorFormat","RAW"},{"compression","RLE"}},json{{"colorFormat","L8"},{"premultiply",true}},json{{"name","../../escape"}}})CHECK(!ImageConversion::Convert({png},options).success);
        std::cout<<"Native C++ image conversion passed all pixel, compression, animation, SVG, export and cancellation checks without lv_init\n";return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
