#include "../../UICreator/Fonts/BuiltinFonts.h"
#include "../../UICreator/Fonts/FontConverter.h"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

#define CHECK(value) do {if(!(value))throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": " #value);} while(0)
int main() {
    try {
        const auto& fonts=BuiltinFonts::Catalog();
        CHECK(!fonts.empty());
        for(const auto& original:{std::make_pair("badh-regular","Badh Regular"),std::make_pair("caliper-regular","Caliper Regular"),
            std::make_pair("microgramma-d-medium-extended","MicrogrammaDMedExt"),std::make_pair("ocr-a-extended","OCR A Extended")}) {
            const auto* entry=BuiltinFonts::Find(original.first);CHECK(entry&&entry->name==original.second);
        }
        const auto* popular=BuiltinFonts::Find("lato-regular");CHECK(popular&&popular->name=="Lato Regular");
        CHECK(BuiltinFonts::Find("missing-font")==nullptr);
        std::set<std::string> identities,names;
        for(const auto& font:fonts) {
            CHECK(identities.insert(font.id).second&&names.insert(font.name).second);
            CHECK(BuiltinFonts::Find(font.id)==&font);
            CHECK(font.path.is_absolute()&&std::filesystem::is_regular_file(font.path));
            std::ifstream input(font.path,std::ios::binary);
            std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
            CHECK(bytes.size()>12&&bytes[0]==0&&bytes[1]==1&&bytes[2]==0&&bytes[3]==0);
            // Font sources are prepared only after a choice is committed. The
            // result is a self-contained raster asset, with no output files.
            FontConversion::Options options;
            options.sources.push_back({font.path});
            const auto result=FontConversion::Prepare(options);
            if(!result.success)throw std::runtime_error(font.name+": "+result.message);
            CHECK(result.files.empty()&&!result.preview.empty());
            CHECK(result.font.at("lineHeight").get<int>()>0);
            CHECK(!result.font.at("dsc").at("bitmap").get<std::string>().empty());
            CHECK(result.font.at("dsc").at("GlyphDsc").size()==96);
        }
        FontConversion::Options subset;subset.size=22;subset.sources.push_back({popular->path,"0x41-0x43"});
        const auto prepared=FontConversion::Prepare(subset);CHECK(prepared.success&&prepared.files.empty());
        CHECK(prepared.font.at("dsc").at("GlyphDsc").size()==4);
        std::cout<<fonts.size()<<" deployed built-in fonts resolve by stable ID and prepare without output files; popular regular font subsets preserve selected characters\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
