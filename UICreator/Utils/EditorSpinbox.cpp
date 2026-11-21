#include "EditorSpinbox.h"
#include "EditorEdit.h"
#include "EditorObjectRef.h"
#include "EditorFocus.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace EditorSpinbox {
namespace {
struct State {
    std::string buffer;
    size_t caret = 0;
    bool editing = false;
    bool updating = false;
};
void Event(lv_event_t* event) {
    auto* state=static_cast<State*>(lv_event_get_user_data(event));
    const auto code=lv_event_get_code(event);
    if(code==LV_EVENT_DELETE) {delete state;return;}
    if(code==LV_EVENT_FOCUSED || code==LV_EVENT_DEFOCUSED || code==LV_EVENT_CLICKED ||
       (code==LV_EVENT_VALUE_CHANGED && !state->updating)) {
        state->editing=false;state->buffer.clear();state->caret=0;
    }
}
State* Find(lv_obj_t* object) {
    if(!object)return nullptr;
    for(uint32_t i=0;i<lv_obj_get_event_count(object);++i) {
        auto* descriptor=lv_obj_get_event_dsc(object,i);
        if(lv_event_dsc_get_cb(descriptor)==Event)
            return static_cast<State*>(lv_event_dsc_get_user_data(descriptor));
    }
    return nullptr;
}
State* Get(lv_obj_t* object) {
    if(auto* state=Find(object))return state;
    auto* state=new State;
    lv_obj_add_event_cb(object,Event,LV_EVENT_ALL,state);
    return state;
}
bool Available(lv_obj_t* object) {
    if(!object || !lv_obj_has_class(object,&lv_spinbox_class))return false;
    return IsEditorControlAvailable(object);
}
bool Selection(lv_obj_t* object,size_t& start,size_t& end) {
    auto* label=lv_textarea_get_label(object);
    auto a=lv_label_get_text_selection_start(label),b=lv_label_get_text_selection_end(label);
    if(a==LV_LABEL_TEXT_SELECTION_OFF || b==LV_LABEL_TEXT_SELECTION_OFF || a==b)return false;
    if(a>b)std::swap(a,b);
    const auto size=std::string(lv_textarea_get_text(object)).size();
    start=std::min<size_t>(a,size);end=std::min<size_t>(b,size);return start<end;
}
unsigned FractionDigits(lv_obj_t* object) {
    const auto position=lv_spinbox_get_dec_point_pos(object);
    return position ? lv_spinbox_get_digit_count(object)-position : 0;
}
std::string Current(lv_obj_t* object) {
    const auto value=static_cast<int64_t>(lv_spinbox_get_value(object));
    const auto fraction=FractionDigits(object);
    auto text=std::to_string(value<0?-value:value);
    if(fraction) {
        if(text.size()<=fraction)text.insert(0,fraction+1-text.size(),'0');
        text.insert(text.size()-fraction,1,'.');
    }
    if(value<0)text.insert(0,1,'-');
    return text;
}
void Begin(lv_obj_t* object,State* state,bool current) {
    if(state->editing)return;
    state->editing=true;state->buffer=current?Current(object):std::string{};
    state->caret=state->buffer.size();
}
bool ValidPrefix(const std::string& text,unsigned fraction) {
    bool dot=false;unsigned after=0;
    for(size_t i=0;i<text.size();++i) {
        const char c=text[i];
        if(i==0 && (c=='-' || c=='+'))continue;
        if(c=='.' && fraction && !dot) {dot=true;continue;}
        if(c<'0' || c>'9')return false;
        if(dot && ++after>fraction)return false;
    }
    return text.size()<=32;
}
void Caret(lv_obj_t* object,const State* state) {
    const auto length=std::string(lv_textarea_get_text(object)).size();
    const auto padding=length>state->buffer.size()?length-state->buffer.size():0;
    lv_textarea_set_cursor_pos(object,static_cast<int32_t>(std::min(length,padding+state->caret)));
}
void Apply(lv_obj_t* object,State* state) {
    const auto fraction=FractionDigits(object);
    const auto& text=state->buffer;
    const bool negative=!text.empty() && text[0]=='-';
    uint64_t value=0;unsigned seenFraction=0;bool dot=false,overflow=false,hasDigit=false;
    for(char c:text) {
        if(c=='.') {dot=true;continue;}
        if(c<'0' || c>'9')continue;
        hasDigit=true;
        if(value>(std::numeric_limits<uint64_t>::max()-9)/10)overflow=true;
        else if(!overflow)value=value*10+static_cast<unsigned>(c-'0');
        if(dot)++seenFraction;
    }
    if(!hasDigit && !text.empty())return; // A sign/decimal prefix is a valid intermediate edit.
    for(unsigned i=seenFraction;i<fraction;++i) {
        if(value>std::numeric_limits<uint64_t>::max()/10)overflow=true;
        else if(!overflow)value*=10;
    }
    int64_t parsed;
    if(overflow || value>static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        parsed=negative?std::numeric_limits<int32_t>::min():std::numeric_limits<int32_t>::max();
    else parsed=negative?-static_cast<int64_t>(value):static_cast<int64_t>(value);
    const auto clamped=std::clamp<int64_t>(parsed,lv_spinbox_get_min_value(object),lv_spinbox_get_max_value(object));
    EditorObjectRef alive(object);
    state->updating=true;
    // Setting the native value also formats its fixed-point display and emits VALUE_CHANGED.
    // Application callbacks can delete the field, so rediscover its binding before touching state.
    { EditorEdit::Scope edit(object,true);lv_spinbox_set_value(object,static_cast<int32_t>(clamped)); }
    if(alive.Get() && Find(object)==state) {state->updating=false;lv_textarea_clear_selection(object);Caret(object,state);}
}
bool Insert(lv_obj_t* object,const char* input) {
    EditorObjectRef alive(object);
    auto* state=Get(object);size_t start=0,end=0;
    if(Selection(object,start,end)) {
        const std::string text=lv_textarea_get_text(object);
        state->editing=true;state->buffer=text;state->buffer.erase(start,end-start);state->caret=start;
        lv_textarea_clear_selection(object);
    }
    Begin(object,state,false);
    for(const unsigned char* c=reinterpret_cast<const unsigned char*>(input);*c;++c) {
        if(!(*c>='0' && *c<='9') && *c!='-' && *c!='+' && *c!='.')continue;
        auto next=state->buffer;next.insert(next.begin()+static_cast<std::ptrdiff_t>(state->caret),static_cast<char>(*c));
        if(!ValidPrefix(next,FractionDigits(object)))continue;
        state->buffer=std::move(next);++state->caret;Apply(object,state);
        if(!alive.Get() || Find(object)!=state)return true;
    }
    return true;
}
}

bool HandleEvent(const SDL_Event& event,lv_group_t* group) {
    auto* object=group?lv_group_get_focused(group):nullptr;
    if(!object || !lv_obj_has_class(object,&lv_spinbox_class))return false;
    if(!Available(object)) {
        if(event.type==SDL_EVENT_TEXT_INPUT)return true;
        if(event.type!=SDL_EVENT_KEY_DOWN)return false;
        const auto key=event.key.key;
        if(event.key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))
            return key==SDLK_A || key==SDLK_C || key==SDLK_X || key==SDLK_V;
        return key==SDLK_UP || key==SDLK_DOWN || key==SDLK_LEFT || key==SDLK_RIGHT ||
            key==SDLK_HOME || key==SDLK_END || key==SDLK_BACKSPACE || key==SDLK_DELETE ||
            key==SDLK_RETURN || key==SDLK_KP_ENTER;
    }
    EditorObjectRef alive(object);
    if(event.type==SDL_EVENT_TEXT_INPUT) {
        if(SDL_GetModState()&(SDL_KMOD_CTRL|SDL_KMOD_ALT))return false;
        return Insert(object,event.text.text);
    }
    if(event.type!=SDL_EVENT_KEY_DOWN)return false;
    const auto key=event.key.key;
    auto* state=Get(object);
    if(event.key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI)) {
        if(key==SDLK_A) {
            state->editing=false;
            lv_textarea_set_text_selection(object,true);
            auto* label=lv_textarea_get_label(object);
            lv_label_set_text_selection_start(label,0);
            lv_label_set_text_selection_end(label,static_cast<uint32_t>(std::string(lv_textarea_get_text(object)).size()));
            return true;
        }
        if(key==SDLK_V) {
            auto* text=SDL_GetClipboardText();if(text){Insert(object,text);SDL_free(text);}return true;
        }
        if(key==SDLK_X || key==SDLK_C) {
            size_t start=0,end=0;
            if(Selection(object,start,end)) {
                const std::string text=lv_textarea_get_text(object);
                SDL_SetClipboardText(text.substr(start,end-start).c_str());
                if(key==SDLK_X) {
                    state->editing=true;state->buffer=text;state->buffer.erase(start,end-start);state->caret=start;
                    Apply(object,state);
                }
            }
            return true;
        }
        return false;
    }
    if(event.key.mod&SDL_KMOD_ALT)return false;
    if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_LEFT || key==SDLK_RIGHT || key==SDLK_TAB || key==SDLK_ESCAPE) {
        state->editing=false;state->buffer.clear();return false;
    }
    if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
        state->editing=false;state->buffer.clear();
        lv_obj_send_event(object,LV_EVENT_READY,nullptr);return true;
    }
    if(key!=SDLK_HOME && key!=SDLK_END && key!=SDLK_BACKSPACE && key!=SDLK_DELETE)return false;
    size_t start=0,end=0;
    if((key==SDLK_BACKSPACE || key==SDLK_DELETE) && Selection(object,start,end)) {
        state->editing=true;state->buffer=lv_textarea_get_text(object);
        state->buffer.erase(start,end-start);state->caret=start;Apply(object,state);return true;
    }
    Begin(object,state,true);
    if(key==SDLK_HOME)state->caret=0;
    else if(key==SDLK_END)state->caret=state->buffer.size();
    else if(key==SDLK_BACKSPACE && state->caret) {state->buffer.erase(--state->caret,1);Apply(object,state);}
    else if(key==SDLK_DELETE && state->caret<state->buffer.size()) {state->buffer.erase(state->caret,1);Apply(object,state);}
    if(alive.Get() && Find(object)==state)Caret(object,state);
    return true;
}
}
