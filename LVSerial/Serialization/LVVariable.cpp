#include "LVVariable.h"
#include <stdexcept>
#include <cmath>
#include <limits>
#include <climits>
#include <map>
#include <set>
#include <tuple>
namespace Serialization
{
    namespace
    {
        using json = nlohmann::json;
        bool Valid(VariableType type, const json& value)
        {
            switch (type) {
            case VariableType::String: return value.is_string() && value.get_ref<const std::string&>().find('\0') == std::string::npos;
            case VariableType::Integer: case VariableType::DateTime:
                return value.is_number_integer() && (!value.is_number_unsigned() || value.get<uint64_t>() <= INT64_MAX);
            case VariableType::Boolean: return value.is_boolean();
            case VariableType::Float: return value.is_number() && std::isfinite(value.get<double>());
            default: return false;
            }
        }
        const json& Code(const VariableData& variable)
        {
            static const json empty = json::object();
            if (!variable.extra.is_object() || !variable.extra.contains("code")) return empty;
            if (!variable.extra["code"].is_object()) throw std::invalid_argument("Variable code settings must be an object");
            return variable.extra["code"];
        }
        std::string Leaf(const std::string& path)
        {const auto slash=path.find_last_of('/');return slash==std::string::npos?path:path.substr(slash+1);}
        size_t Screen(const json& value)
        {
            if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<int64_t>() < 0) ||
                value.get<uint64_t>() > std::numeric_limits<size_t>::max())
                throw std::invalid_argument("Variable binding screen must be a nonnegative index");
            return value.get<size_t>();
        }
        void CheckCodeValue(VariableType type,const std::string& codeType,const json& value)
        {
            if (type==VariableType::Float && codeType=="float" && std::abs(value.get<double>())>std::numeric_limits<float>::max())
                throw std::invalid_argument("Variable value exceeds float storage range");
            if (type!=VariableType::Integer) return;
            const auto number=value.get<int64_t>();int64_t low=INT64_MIN,high=INT64_MAX;
            if(codeType=="int8_t"){low=INT8_MIN;high=INT8_MAX;}
            else if(codeType=="int16_t"){low=INT16_MIN;high=INT16_MAX;}
            else if(codeType=="int32_t"){low=INT32_MIN;high=INT32_MAX;}
            else if(codeType=="uint8_t"){low=0;high=UINT8_MAX;}
            else if(codeType=="uint16_t"){low=0;high=UINT16_MAX;}
            else if(codeType=="uint32_t"){low=0;high=UINT32_MAX;}
            if(number<low||number>high)throw std::invalid_argument("Variable value exceeds configured C storage range");
        }
    }
    std::string LVVariable::CodeType(const VariableData& variable)
    {
        const auto& code=Code(variable);
        const char* fallback=variable.type==VariableType::String?"char":variable.type==VariableType::Boolean?"bool":variable.type==VariableType::Float?"double":"int64_t";
        const auto type=code.value("cType",std::string(fallback));
        const bool valid=variable.type==VariableType::String?type=="char":variable.type==VariableType::Boolean?type=="bool":
            variable.type==VariableType::Float?(type=="float"||type=="double"):variable.type==VariableType::DateTime?type=="int64_t":
            (type=="int8_t"||type=="int16_t"||type=="int32_t"||type=="int64_t"||type=="uint8_t"||type=="uint16_t"||type=="uint32_t");
        if(!valid)throw std::invalid_argument("C storage type does not match the variable data type");
        return type;
    }
    void LVVariable::ValidateValue(const VariableData& variable, const json& value)
    {
        if (!Valid(variable.type, value)) throw std::invalid_argument("Variable value has the wrong data type");
        CheckCodeValue(variable.type, CodeType(variable), value);
    }
    std::string LVVariable::CodeSymbol(const VariableData& variable)
    {
        const auto& code=Code(variable);if(!code.contains("symbol"))return {};
        if(!code["symbol"].is_string())throw std::invalid_argument("C variable symbol must be an identifier");
        const auto symbol=code["symbol"].get<std::string>();
        auto letter=[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';};
        if(symbol.empty()||!letter(static_cast<unsigned char>(symbol[0])))throw std::invalid_argument("C variable symbol must be an identifier");
        for(unsigned char c:symbol)if(!letter(c)&&!(c>='0'&&c<='9'))throw std::invalid_argument("C variable symbol must be an identifier");
        static const std::set<std::string> reserved={"alignas","alignof","and","and_eq","asm","auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t","class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue","co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast","else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or","or_eq","private","protected","public","register","reinterpret_cast","requires","restrict","return","short","signed","sizeof","static","static_assert","static_cast","struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq","_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert","_Thread_local"};
        if(reserved.count(symbol))throw std::invalid_argument("C variable symbol is a reserved keyword");
        return symbol;
    }
    json LVVariable::BindingToJSON(const VariableBinding& binding)
    {
        if(binding.objectId<0||binding.property.empty()||binding.property.find('\0')!=std::string::npos)
            throw std::invalid_argument("Variable binding needs a widget and property");
        auto result=binding.extra.is_object()?binding.extra:json::object();
        result["screen"]=binding.screen;result["oid"]=binding.objectId;result["property"]=binding.property;
        switch(binding.direction){
        case VariableBindingDirection::TwoWay:result["direction"]="two-way";break;
        case VariableBindingDirection::ToWidget:result["direction"]="to-widget";break;
        case VariableBindingDirection::FromWidget:result["direction"]="from-widget";break;
        default:throw std::invalid_argument("Invalid variable binding direction");}
        return result;
    }
    std::vector<VariableBinding> LVVariable::Bindings(const VariableData& variable)
    {
        std::vector<VariableBinding> result;
        const bool explicitList=variable.extra.is_object()&&variable.extra.contains("bindings");
        if(!explicitList){
            if(variable.objectId<0||variable.property.empty())return result;
            VariableBinding binding;binding.objectId=variable.objectId;binding.property=variable.property;
            if(variable.extra.is_object()&&variable.extra.contains("screen"))binding.screen=Screen(variable.extra["screen"]);
            BindingToJSON(binding);result.push_back(std::move(binding));return result;
        }
        const auto& list=variable.extra["bindings"];if(!list.is_array())throw std::invalid_argument("Variable bindings must be an array");
        std::set<std::tuple<size_t,int,std::string>> targets;
        for(const auto& item:list){
            if(!item.is_object()||!item.contains("oid")||!item["oid"].is_number_integer()||
                (!item["oid"].is_number_unsigned()&&item["oid"].get<int64_t>()<0)||item["oid"].get<uint64_t>()>INT_MAX)
                throw std::invalid_argument("Variable binding needs a nonnegative widget ID");
            VariableBinding binding;binding.extra=item;binding.objectId=item["oid"].get<int>();
            binding.screen=Screen(item.value("screen",json(0)));binding.property=item.value("property",std::string{});
            const auto direction=item.value("direction",std::string("two-way"));
            if(direction=="two-way")binding.direction=VariableBindingDirection::TwoWay;
            else if(direction=="to-widget")binding.direction=VariableBindingDirection::ToWidget;
            else if(direction=="from-widget")binding.direction=VariableBindingDirection::FromWidget;
            else throw std::invalid_argument("Invalid variable binding direction");
            BindingToJSON(binding);
            if(!targets.emplace(binding.screen,binding.objectId,Leaf(binding.property)).second)throw std::invalid_argument("Duplicate variable binding target");
            result.push_back(std::move(binding));
        }
        return result;
    }
    std::vector<VariableBindingProperty> LVVariable::BindingProperties(const json& widget,VariableType type)
    {
        std::vector<VariableBindingProperty> result;if(!widget.is_object())return result;
        auto add=[&](const char* path,const char* label,bool readable=false){result.push_back({path,label,readable,true});};
        const bool numeric=type!=VariableType::String;
        const bool advancedCheckbox=widget.contains("checkboxButton")||widget.contains("labeledCheckbox");
        if(numeric){
            bool checkable=widget.contains("cb")||widget.contains("switch");
            if(widget.contains("button")){
                const auto base=widget.value("base",json::object());
                const auto flags=base.is_object()?base.value("flags",json::object()):json::object();
                checkable=flags.is_object()&&flags.value("checkable",false);
                if(widget["button"].is_object()&&widget["button"].contains("toggle"))checkable=widget["button"].value("toggle",false);
            }
            if(!advancedCheckbox)add("/base/checked","Checked",checkable);
            add("/base/hidden","Hidden");add("/base/disabled","Disabled");
            add("/base/coords/x","X position");add("/base/coords/y","Y position");add("/base/coords/width","Width");add("/base/coords/height","Height");add("/base/opascale","Opacity");
        }
        if(type==VariableType::Boolean && advancedCheckbox)
            add(widget.contains("checkboxButton")?"/checkboxButton/checked":"/labeledCheckbox/checked","Checked",true);
        if(type==VariableType::Integer) {
            if(widget.contains("radioGroup"))add("/radioGroup/selected","Selected option",true);
            if(widget.contains("menu"))add("/menu/selected","Selected item",true);
        }
        if(type==VariableType::String && widget.contains("datePicker"))add("/datePicker/date","Date (yyyy-mm-dd)",true);
        if(widget.contains("text"))add("/text","Text");
        if(widget.contains("textarea"))add("/textarea/text","Text",!numeric);
        if(widget.contains("cb"))add("/cb/text","Caption");
        if(widget.contains("advancedKeyboard"))add("/advancedKeyboard/text","Target text",!numeric);
        if(widget.contains("textBox")){if(numeric)add("/textBox/value","Value",true);else add("/textBox/text","Text",true);}
        if(numeric){
            if(widget.contains("layerView"))add("/layerView/selected","Selected page",true);
            for(const auto* kind:{"arc","bar","slider","spinbox"})if(widget.contains(kind))result.push_back({"/"+std::string(kind)+"/value","Value",true,true});
            for(const auto* kind:{"dropdown","roller"})if(widget.contains(kind))result.push_back({"/"+std::string(kind)+"/selected","Selection",true,true});
            if(widget.contains("led"))add("/led/brightness","Brightness");
        }
        return result;
    }
    void LVVariable::ValidateBinding(const VariableData& variable,const VariableBinding& binding,const json& widget)
    {
        BindingToJSON(binding);const auto property=Leaf(binding.property);
        for(const auto& option:BindingProperties(widget,variable.type))if(Leaf(option.path)==property){
            if(binding.direction==VariableBindingDirection::FromWidget&&!option.readable)throw std::invalid_argument("This property cannot send widget values to a variable");
            if(binding.direction!=VariableBindingDirection::FromWidget&&!option.writable)throw std::invalid_argument("This property cannot receive variable values");
            return;
        }
        throw std::invalid_argument("Unsupported property for this widget and variable data type");
    }
    nlohmann::json LVVariable::ToJSON(const VariableData& variable, bool storeValues)
    {
        if (!Valid(variable.type, variable.defaultValue)) throw std::invalid_argument("Variable default has the wrong data type");
        const auto codeType=CodeType(variable);CodeSymbol(variable);CheckCodeValue(variable.type,codeType,variable.defaultValue);
        if (!variable.values.is_array()) throw std::invalid_argument("Variable history must be an array");
        for (const auto& value : variable.values)
            if (!Valid(variable.type, value)) throw std::invalid_argument("Variable history has the wrong data type");
            else CheckCodeValue(variable.type,codeType,value);
        auto record = variable.extra.is_object() ? variable.extra : json::object();
        record["name"] = variable.name; record["dataType"] = static_cast<int>(variable.type);
        record["default"] = variable.defaultValue; record["history"] = variable.history;
        record["oid"] = variable.objectId; record["property"] = variable.property;
        const auto bindings=Bindings(variable);
        if(record.contains("bindings")){
            record["bindings"]=json::array();for(const auto& binding:bindings)record["bindings"].push_back(BindingToJSON(binding));
            record["oid"]=bindings.empty()?-1:bindings.front().objectId;record["property"]=bindings.empty()?"":bindings.front().property;
            if(bindings.empty())record.erase("screen");else record["screen"]=bindings.front().screen;
        }
        if (storeValues && !variable.values.empty()) record["value"] = variable.values;
        else record.erase("value");
        return {{"var", record}};
    }
    VariableData LVVariable::FromJSON(const nlohmann::json& record)
    {
        if (!record.is_object() || !record.contains("var") || !record["var"].is_object()) throw std::invalid_argument("Invalid variable record");
        const auto& data = record["var"];
        const int type = data.value("dataType", 1), history = data.value("history", 1);
        if (type < 0 || type > 4 || history < 0 || history > UINT16_MAX) throw std::invalid_argument("Invalid variable type or history length");
        VariableData result;
        result.name = data.value("name", std::string{}); result.type = static_cast<VariableType>(type);
        result.defaultValue = data.value("default", type == 0 ? json("") : type == 2 ? json(false) : json(0));
        result.history = static_cast<uint16_t>(history); result.objectId = data.value("oid", -1);
        result.property = data.value("property", std::string{}); result.values = data.value("value", json::array()); result.extra = data;
        const auto normalized=ToJSON(result);
        if(data.contains("bindings")){
            result.objectId=normalized["var"]["oid"].get<int>();result.property=normalized["var"]["property"].get<std::string>();
            result.extra=normalized["var"];
        }
        return result;
    }
    Binary LVVariable::ToProtobuf(const VariableData& variable, bool storeValues)
    { return Protobuf::Encode(EntityKind::Variable, "variable", ToJSON(variable, storeValues)); }
    VariableData LVVariable::FromProtobuf(const Binary& input)
    {
        const auto record = Protobuf::Decode(input);
        if (record.kind != EntityKind::Variable || record.type != "variable") throw std::invalid_argument("Expected a variable record");
        return FromJSON(record.data);
    }
    nlohmann::json LVVariable::NormalizeList(const nlohmann::json& records)
    {
        if (!records.is_array()) throw std::invalid_argument("Variables must be an array");
        auto result = json::array();
        std::set<std::string> names;std::map<std::tuple<size_t,int,std::string>,std::string> writers;
        for (const auto& record : records){
            const auto variable=FromJSON(record);
            if(variable.name.empty()||variable.name.find('\0')!=std::string::npos||!names.insert(variable.name).second)throw std::invalid_argument("Variable names must be unique and nonempty");
            for(const auto& binding:Bindings(variable))if(binding.direction!=VariableBindingDirection::FromWidget){
                const auto key=std::make_tuple(binding.screen,binding.objectId,Leaf(binding.property));
                if(!writers.emplace(key,variable.name).second)throw std::invalid_argument("Multiple variables write the same widget property");
            }
            result.push_back(ToJSON(variable));
        }
        return result;
    }
}
