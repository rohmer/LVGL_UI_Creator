#pragma once
#include <filesystem>

namespace AdvancedKeyboardDependency {
inline std::filesystem::path SourceRoot()
{
#ifdef UICREATOR_ADVANCED_KEYBOARD_SOURCE_DIR
    return std::filesystem::u8path(UICREATOR_ADVANCED_KEYBOARD_SOURCE_DIR);
#else
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
        "3rdParty" / "AdvancedKeyboard";
#endif
}
}
