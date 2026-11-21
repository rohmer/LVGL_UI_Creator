# Source provenance

Ported at the user's request from their ESPOS checkout, under `Keyboard/`,
`Keyboards/` and `components/ngram_trie/`. The imported implementation is the native C++ LVGL keyboard and
typing view, rather than the separate .NET simulator.

`Key.cpp`, `Keyboard.cpp`, `LayoutData.h`, `SuggestionText.cpp/.h`, the keyboard
public declarations, generated layout data, layout JSON/schema/generator and
`KeyboardFont.c` derive from those ESPOS sources. `TypingView.cpp` retains the
fixed-size typing UI and optional suggestion callback; board/display creation
and orientation controls were removed for use as a nested Creator widget.
`PredictionModel.cpp`, `PredictionPrefix.cpp`, `Utf8Fold.cpp`, the matching
private declarations and `UnicodeFold.inc` port the complete portable model,
prefix and autocorrect engine under `espos::keyboard::prediction`. The
original UTF-8 folding, protected words, Unicode tables and keyboard-weighted
edit scoring are retained. ESP-specific partition mapping, startup and
benchmark code are excluded. `layout/build_ngtr_index.py` is the unchanged
ESPOS index generator. The C index builder and predictor lifetime facade are
Creator integration code. Built-in model details and available source notices
are retained under `models/`.

`AdvancedKeyboard.cpp`, the public C facade and the Creator serializer are
integration code for LVGL UI Creator. The source checkout supplied no separate
project-wide ESPOS code license. Its existing ownership and applicable terms
are unchanged by this port. The keyboard font's explicit Bitstream Vera,
DejaVu and Arev terms are retained verbatim in `KeyboardFont.LICENSE.txt`.

The French AZERTY and German QWERTZ definitions are retained from ESPOS.
The Spanish QWERTY definition derives from its `es-MX-qwerty.json`; it is
exposed as the Spanish `es-ES` selection without changing its key set. Italian
and Portuguese QWERTY definitions and their accent pages extend that portable
layout in Creator. `layout/generate_languages.py` regenerates the checked-in
language headers. `KeyboardFont.c` was regenerated from LVGL's DejaVu Sans
with the same 16px/4bpp settings and the extra language glyphs; the exact font
hash and converter command are retained in its source header.
