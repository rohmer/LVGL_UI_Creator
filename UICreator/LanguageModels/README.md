# Language-model editor integration

The LVGL generator window and playground stay here. The trainer, CLI, codec,
wordlists and statistical tests are maintained only in AdvancedKeyboard's
`tools/NativeTrieGenerator` and `tools/ModelCodec`. This CMake bridge compiles
those sources. See [the shared tool guide](../../3rdParty/AdvancedKeyboard/tools/README.md).
