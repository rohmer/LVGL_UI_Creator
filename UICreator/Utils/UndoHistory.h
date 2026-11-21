#pragma once
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
// One baseline plus reversible patches. Immutable asset strings do not need
// another complete copy for every small geometry/text edit.
class UndoHistory {
public:
    struct State {std::string document;int selection=-1;std::vector<int> selections;size_t screen=0;};
    explicit UndoHistory(size_t count=100,size_t bytes=32u*1024u*1024u):maxEntries(count?count:1),maxBytes(bytes){}
    void Reset(State state){entries.clear();current=std::make_shared<State>(std::move(state));baseline=current;entries.emplace_back(*current);cursor=0;ClearCaches();BreakGroup();Trim();}
    bool CanUndo()const{return cursor>0;}
    bool CanRedo()const{return cursor+1<entries.size();}
    const State& UndoState()const{if(!CanUndo())throw std::out_of_range("No undo");if(!previous)previous=Restore(cursor-1,false);return *previous;}
    const State& RedoState()const{if(!CanRedo())throw std::out_of_range("No redo");if(!next)next=Restore(cursor+1,true);return *next;}
    void DidUndo(){UndoState();auto old=current;current=previous;--cursor;previous.reset();next=std::move(old);BreakGroup();}
    void DidRedo(){RedoState();auto old=current;current=next;++cursor;next.reset();previous=std::move(old);BreakGroup();}
    void ReplaceCurrent(State state){
        if(entries.empty()){Reset(std::move(state));return;}
        auto before=CanUndo()?std::make_shared<State>(UndoState()):nullptr;
        auto after=CanRedo()?std::make_shared<State>(RedoState()):nullptr;
        current=std::make_shared<State>(std::move(state));entries[cursor].Selection(*current);
        if(before)entries[cursor].difference=Delta(before->document,current->document);else baseline=current;
        if(after)entries[cursor+1].difference=Delta(current->document,after->document);
        ClearCaches();Trim();
    }
    bool Record(State state,const void* key=nullptr,uint64_t tick=0){
        if(entries.empty()){Reset(std::move(state));return false;}
        if(current->document==state.document){entries[cursor].Selection(state);current->selection=state.selection;current->selections=std::move(state.selections);current->screen=state.screen;return false;}
        bool merge=key&&key==group&&tick>=groupTick&&tick-groupTick<750&&!CanRedo()&&cursor>0;
        auto before=merge?std::make_shared<State>(UndoState()):current;
        while(CanRedo())entries.pop_back();
        auto value=std::make_shared<State>(std::move(state));Entry entry(*value);entry.difference=Delta(before->document,value->document);
        if(merge)entries[cursor]=std::move(entry);else{entries.push_back(std::move(entry));++cursor;}
        current=std::move(value);ClearCaches();group=key;groupTick=tick;Trim();return true;
    }
    void BreakGroup(){group=nullptr;groupTick=0;}
    size_t Size()const{return entries.size();}
    size_t StorageBytes()const{size_t bytes=baseline?baseline->document.size():0;if(current&&current!=baseline)bytes+=current->document.size();for(const auto& e:entries)bytes+=e.difference.removed.size()+e.difference.added.size();return bytes;}
    size_t LogicalBytes()const{size_t bytes=0;for(const auto& e:entries)bytes+=e.documentBytes;return bytes;}
private:
    struct Patch{size_t prefix=0;std::string removed,added;};
    struct Entry{
        Patch difference;int selection=-1;std::vector<int> selections;size_t screen=0,documentBytes=0;
        explicit Entry(const State& state){Selection(state);}
        void Selection(const State& state){selection=state.selection;selections=state.selections;screen=state.screen;documentBytes=state.document.size();}
    };
    std::deque<Entry> entries;
    std::shared_ptr<State> baseline,current;
    mutable std::shared_ptr<State> previous,next;
    size_t cursor=0,maxEntries,maxBytes;const void* group=nullptr;uint64_t groupTick=0;
    static Patch Delta(const std::string& old,const std::string& value){
        Patch patch;const auto common=(std::min)(old.size(),value.size());
        while(patch.prefix<common&&old[patch.prefix]==value[patch.prefix])++patch.prefix;
        size_t suffix=0;while(suffix<common-patch.prefix&&old[old.size()-1-suffix]==value[value.size()-1-suffix])++suffix;
        patch.removed=old.substr(patch.prefix,old.size()-patch.prefix-suffix);patch.added=value.substr(patch.prefix,value.size()-patch.prefix-suffix);return patch;
    }
    static std::string Apply(const std::string& source,const Patch& patch,bool forward){
        const auto& removed=forward?patch.removed:patch.added;const auto& added=forward?patch.added:patch.removed;
        if(patch.prefix>source.size()||removed.size()>source.size()-patch.prefix||source.compare(patch.prefix,removed.size(),removed))throw std::logic_error("Invalid undo patch");
        std::string result;result.reserve(source.size()-removed.size()+added.size());result.append(source,0,patch.prefix);result+=added;result.append(source,patch.prefix+removed.size(),std::string::npos);return result;
    }
    std::shared_ptr<State> Restore(size_t target,bool forward)const{
        const auto& metadata=entries[target];const auto& patch=entries[forward?target:cursor].difference;
        return std::make_shared<State>(State{Apply(current->document,patch,forward),metadata.selection,metadata.selections,metadata.screen});
    }
    void ClearCaches(){previous.reset();next.reset();}
    void Trim(){
        while(entries.size()>1&&(entries.size()>maxEntries+1||StorageBytes()>maxBytes)){
            if(cursor){auto value=std::make_shared<State>(State{Apply(baseline->document,entries[1].difference,true),entries[1].selection,entries[1].selections,entries[1].screen});entries.pop_front();entries.front().difference={};--cursor;baseline=cursor==0?current:std::move(value);}
            else entries.pop_back();
            ClearCaches();
        }
    }
};
