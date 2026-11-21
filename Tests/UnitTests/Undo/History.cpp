#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "../../../UICreator/Utils/UndoHistory.h"
int main(int argc,char** argv){return Catch::Session().run(argc,argv);}
TEST_CASE("History restores complete states and invalidates redo branches") {
    UndoHistory h;h.Reset({"initial",-1});CHECK_FALSE(h.CanUndo());CHECK_FALSE(h.CanRedo());
    CHECK(h.Record({"widget",0}));CHECK(h.Record({"widget+variable",1}));CHECK(h.Size()==3);
    CHECK(h.UndoState().document=="widget");h.DidUndo();CHECK(h.RedoState().document=="widget+variable");
    h.DidUndo();CHECK_FALSE(h.CanUndo());h.DidRedo();CHECK(h.CanRedo());
    CHECK(h.Record({"different widget",0}));CHECK_FALSE(h.CanRedo());CHECK(h.UndoState().document=="widget");
    CHECK_FALSE(h.Record({"different widget",2}));CHECK_FALSE(h.CanRedo());
}
TEST_CASE("Typing coalesces only within the same control and time window") {
    UndoHistory h;int fieldA,fieldB;h.Reset({"",-1});
    h.Record({"a",0},&fieldA,10);h.Record({"ab",0},&fieldA,100);CHECK(h.Size()==2);CHECK(h.UndoState().document=="");
    h.Record({"abc",0},&fieldA,1000);CHECK(h.Size()==3);CHECK(h.UndoState().document=="ab");
    h.Record({"abcd",0},&fieldB,1010);CHECK(h.Size()==4);
    h.BreakGroup();h.Record({"abcde",0},&fieldB,1020);CHECK(h.Size()==5);
    h.DidUndo();h.Record({"replacement",0},&fieldB,1030);CHECK_FALSE(h.CanRedo());CHECK(h.UndoState().document=="abcd");
}
TEST_CASE("History budgets prune old states and retain the active document") {
    UndoHistory h(2,1000);h.Reset({"0"});for(int i=1;i<8;++i)h.Record({std::to_string(i)});
    CHECK(h.Size()==3);CHECK(h.UndoState().document=="6");h.DidUndo();h.DidUndo();CHECK_FALSE(h.CanUndo());
    UndoHistory bytes(100,10);bytes.Reset({"first"});bytes.Record({"second"});CHECK(bytes.Size()==1);CHECK_FALSE(bytes.CanUndo());
    bytes.Record({"a very large current state"});CHECK(bytes.Size()==1);
}
TEST_CASE("Canonical restore can replace a state without destroying undo or redo") {
    UndoHistory h;h.Reset({"a"});h.Record({"b"});h.Record({"c"});h.DidUndo();h.ReplaceCurrent({"b canonical",4});
    CHECK(h.CanUndo());CHECK(h.CanRedo());CHECK(h.UndoState().document=="a");CHECK(h.RedoState().document=="c");
    h.DidRedo();CHECK(h.UndoState().document=="b canonical");CHECK(h.UndoState().selection==4);
}

TEST_CASE("Selection sets follow undo states without creating selection-only edits") {
    UndoHistory history;history.Reset({"document",0,{0,1}});
    CHECK_FALSE(history.Record({"document",2,{1,2}}));CHECK(history.Size()==1);
    history.Record({"moved",2,{1,2}});
    CHECK(history.UndoState().selections==std::vector<int>{1,2});
    history.DidUndo();CHECK_FALSE(history.Record({"document",0,{0,2}}));CHECK(history.CanRedo());
    history.DidRedo();CHECK(history.UndoState().selections==std::vector<int>{0,2});
}
TEST_CASE("Small edits retain large immutable assets without full snapshot copies") {
    UndoHistory history(100,16u*1024u*1024u);const std::string asset(2u*1024u*1024u,'a');
    history.Reset({asset+"|x=0"});
    for(int i=1;i<=80;++i)history.Record({asset+"|x="+std::to_string(i),i,{i},size_t(i%3)});
    CHECK(history.Size()==81);CHECK(history.LogicalBytes()>150u*1024u*1024u);CHECK(history.StorageBytes()<5u*1024u*1024u);
    for(int i=79;i>=0;--i){REQUIRE(history.UndoState().document==asset+"|x="+std::to_string(i));history.DidUndo();}
    for(int i=1;i<=80;++i){REQUIRE(history.RedoState().document==asset+"|x="+std::to_string(i));history.DidRedo();}
}
TEST_CASE("Canonical replacement rebases adjacent reversible patches") {
    UndoHistory h;h.Reset({"AAA-0-ZZZ"});h.Record({"AAA-1-ZZZ"});h.Record({"AAA-2-ZZZ"});
    h.DidUndo();h.ReplaceCurrent({"AAA-one-ZZZ",3,{3},2});
    CHECK(h.UndoState().document=="AAA-0-ZZZ");CHECK(h.RedoState().document=="AAA-2-ZZZ");
    h.DidRedo();CHECK(h.UndoState().screen==2);h.DidUndo();h.DidUndo();CHECK(h.RedoState().document=="AAA-one-ZZZ");
}
