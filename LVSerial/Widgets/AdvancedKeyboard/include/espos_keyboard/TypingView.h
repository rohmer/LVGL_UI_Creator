#pragma once
#if defined(ESPOS_KEYBOARD_LEGACY_VIEW_ADAPTER) && ESPOS_KEYBOARD_LEGACY_VIEW_ADAPTER
#include "espos_keyboard/LegacyTypingView.h"
#else
#include "espos_keyboard/AdvancedTypingView.h"
namespace espos::keyboard {
using view::SuggestionLookup;
using view::create_en_us_typing_view;
using view::typing_view_editor;
using view::typing_view_keyboard;
using view::set_typing_view_editor;
using view::set_typing_view_language;
using view::set_typing_view_right_to_left;
using view::typing_view_right_to_left;
using view::reset_typing_view_composition;
using view::authored_typing_view_style;
using view::WordBoundaryHandler;
using view::set_typing_view_word_boundary_handler;
using view::DismissHandler;
using view::set_typing_view_dismiss_handler;
using view::set_typing_view_lookup;
}
#endif
