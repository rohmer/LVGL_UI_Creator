#include <lv_advanced_keyboard_prediction.h>
#include "../../LVSerial/Widgets/AdvancedKeyboard/Utf8Fold.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool valid,const char* message) {if (!valid) throw std::runtime_error(message);}
void Write32(std::vector<uint8_t>& bytes,uint32_t value)
{
    for (unsigned shift=0;shift<32;shift+=8) bytes.push_back(static_cast<uint8_t>(value>>shift));
}
void WriteVar(std::vector<uint8_t>& bytes,uint32_t value)
{
    while (value>=128) {bytes.push_back(static_cast<uint8_t>(value|128));value>>=7;}
    bytes.push_back(static_cast<uint8_t>(value));
}
struct Node {uint8_t label=0;uint32_t terminal=0;std::map<uint8_t,std::unique_ptr<Node>> children;};
void Insert(Node& root,const std::string& word,uint32_t count)
{
    auto* node=&root;
    for (uint8_t byte:word) {
        auto& child=node->children[byte];
        if (!child) {child=std::make_unique<Node>();child->label=byte;}
        node=child.get();
    }
    node->terminal=count;
}
std::vector<uint8_t> Fixture()
{
    Node combined,unigrams;
    for (const auto& word:std::vector<std::pair<std::string,uint32_t>>{
        {"hello",100000},{"help",200},{"the",1000000},{"there",1000},{"their",500},{"world",100},
        {"in",500},{"in the",50000},{"in there",1},{"was in the",50000}}) {
        Insert(combined,word.first,word.second);
        if (word.first.find(' ')==std::string::npos) Insert(unigrams,word.first,word.second);
    }
    std::vector<Node*> nodes{&combined,&unigrams};
    for (size_t i=0;i<nodes.size();++i) for (const auto& child:nodes[i]->children) nodes.push_back(child.second.get());
    std::vector<uint8_t> data{'N','G','T','R'};Write32(data,3);Write32(data,static_cast<uint32_t>(nodes.size()));Write32(data,0);
    for (const auto* node:nodes) {
        data.push_back(node->label);
        WriteVar(data,static_cast<uint32_t>((node->children.size()<<1)|(node->terminal?1:0)));
        if (node->terminal) WriteVar(data,node->terminal);
    }
    return data;
}
std::string Word(const lv_advanced_keyboard_suggestion_t& word) {return {word.text,word.length};}
struct PredictorDeleter {void operator()(lv_advanced_keyboard_predictor_t* predictor)const{lv_advanced_keyboard_predictor_destroy(predictor);}};
using Predictor=std::unique_ptr<lv_advanced_keyboard_predictor_t,PredictorDeleter>;
void Corrections(lv_advanced_keyboard_predictor_t* predictor)
{
    lv_advanced_keyboard_suggestion_t word{};double probability=0;
    Check(lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,"teh",3,"in",2,"was",3,0.3,&word,&probability),"Transposition correction was not accepted");
    Check(Word(word)=="the" && probability>=0.3 && probability<=1,"Unexpected contextual transposition correction");
    Check(!lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,"teh",3,nullptr,0,nullptr,0,1,&word,&probability) && word.length==0 && probability==0,"Threshold1 did not retain typed word");
    for (const char* token:{"123","21st","monday","twenty","the"})
        Check(!lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,token,std::strlen(token),nullptr,0,nullptr,0,0.3,&word,&probability),"Protected or correct token was changed");
    Check(!lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,"\xc3",1,nullptr,0,nullptr,0,0.3,&word,&probability),"Truncated UTF8 accepted");
    Check(!lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,"teh",3,nullptr,0,nullptr,0,std::nan(""),&word,&probability),"NaN threshold accepted");
    Check(!lv_advanced_keyboard_predictor_autocorrect_utf8(predictor,nullptr,4,nullptr,0,nullptr,0,0.3,&word,&probability),"Invalid token pointer accepted");
}
}
int main()
{
    try {
        const auto trie=Fixture();void* rawIndex=nullptr;size_t indexSize=0;
        Check(lv_advanced_keyboard_build_trie_index(trie.data(),trie.size(),&rawIndex,&indexSize),"Fixture index generation failed");
        std::unique_ptr<void,decltype(&lv_advanced_keyboard_free_trie_index)> index(rawIndex,lv_advanced_keyboard_free_trie_index);
        Check(lv_advanced_keyboard_validate_trie(trie.data(),trie.size(),index.get(),indexSize),"Generated index failed structural validation");
        Predictor borrowed(lv_advanced_keyboard_predictor_create_borrowed(trie.data(),trie.size(),index.get(),indexSize));
        Predictor generated(lv_advanced_keyboard_predictor_create(trie.data(),trie.size()));
        Check(borrowed && generated,"Raw or borrowed predictor creation failed");
        std::array<lv_advanced_keyboard_suggestion_t,3> words{};
        Check(lv_advanced_keyboard_predictor_suggest_folded(borrowed.get(),"hel",3,nullptr,0,nullptr,0,words.data(),words.size())==2 &&
            Word(words[0])=="hello" && Word(words[1])=="help","Prefix completion did not rank terminal frequencies");
        Check(lv_advanced_keyboard_predictor_suggest_folded(borrowed.get(),"th",2,"in",2,"was",3,words.data(),words.size())==3 &&
            Word(words[0])=="the" && Word(words[1])=="there" && Word(words[2])=="their","Trigram/bigram/unigram fallback order changed");
        Check(lv_advanced_keyboard_predictor_suggest_folded(borrowed.get(),"missing",7,nullptr,0,nullptr,0,words.data(),3)==0,"Unknown prefix yielded suggestions");
        Corrections(borrowed.get());Corrections(generated.get());
        std::vector<uint8_t> unalignedTrie(trie.size()+1),unalignedIndex(indexSize+1);
        std::memcpy(unalignedTrie.data()+1,trie.data(),trie.size());std::memcpy(unalignedIndex.data()+1,index.get(),indexSize);
        Predictor unaligned(lv_advanced_keyboard_predictor_create_borrowed(unalignedTrie.data()+1,trie.size(),unalignedIndex.data()+1,indexSize));
        Check(bool(unaligned),"Unaligned flash ranges failed");Corrections(unaligned.get());
        for (size_t removed=1;removed<=4;++removed) {
            void* invalid=reinterpret_cast<void*>(1);size_t bytes=999;
            Check(!lv_advanced_keyboard_build_trie_index(trie.data(),trie.size()-removed,&invalid,&bytes) && !invalid && !bytes,"Truncated NGTR produced an index");
        }
        auto bad=trie;bad[4]=2;
        Check(!lv_advanced_keyboard_predictor_create(bad.data(),bad.size()),"Unsupported NGTR version accepted");
        auto badIndex=unalignedIndex;badIndex[5]=2;
        Check(!lv_advanced_keyboard_validate_trie(trie.data(),trie.size(),badIndex.data()+1,indexSize),"Unsupported NGIX version accepted");
        Check(!lv_advanced_keyboard_validate_trie(trie.data(),trie.size(),index.get(),indexSize-1),"Truncated NGIX accepted");
        const void* builtinTrie=nullptr;const void* builtinIndex=nullptr;size_t builtinSize=0,builtinIndexSize=0;
        Check(lv_advanced_keyboard_get_builtin_trie(&builtinTrie,&builtinSize,&builtinIndex,&builtinIndexSize) && builtinSize==16384,"Builtin model ranges unavailable");
        void* rebuilt=nullptr;size_t rebuiltSize=0;
        Check(lv_advanced_keyboard_build_trie_index(builtinTrie,builtinSize,&rebuilt,&rebuiltSize),"Builtin model index could not be rebuilt");
        std::unique_ptr<void,decltype(&lv_advanced_keyboard_free_trie_index)> rebuiltIndex(rebuilt,lv_advanced_keyboard_free_trie_index);
        Check(rebuiltSize==builtinIndexSize && !std::memcmp(rebuilt,builtinIndex,rebuiltSize),"C++ index differs from ESPOS Python index");
        Predictor builtin(lv_advanced_keyboard_predictor_create_builtin());
        Check(builtin && lv_advanced_keyboard_predictor_node_count(builtin.get())==5691,"Builtin predictor failed");
        Check(lv_advanced_keyboard_predictor_suggest_folded(builtin.get(),"th",2,nullptr,0,nullptr,0,words.data(),3)>0,"Builtin model has no real completions");
        lv_advanced_keyboard_suggestion_t correction{};double probability=0;
        Check(lv_advanced_keyboard_predictor_autocorrect_utf8(builtin.get(),"teh",3,nullptr,0,nullptr,0,0.3,&correction,&probability) && Word(correction)=="the","Builtin English typo correction failed");
        char folded[64]{};std::string_view normalized;
        Check(espos::keyboard::prediction::FoldUtf8("Stra\xc3\x9f" "e CAF\xc3\x89",folded,sizeof folded,normalized) && normalized=="strasse caf\xc3\xa9","Unicode folding changed");
        std::cout<<"Prediction tests passed; workspace "<<lv_advanced_keyboard_predictor_workspace_bytes()<<" bytes, builtin "<<builtinSize<<"+"<<builtinIndexSize<<" flash bytes, teh -> "<<Word(correction)<<" ("<<probability<<")\n";
        return 0;
    }
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
