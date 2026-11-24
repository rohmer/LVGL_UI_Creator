# Advanced Keyboard

This portable LVGL 9.6 widget brings the ESPOS native mobile keyboard and
prediction engine to LVGL UI Creator. It targets an external textarea, with a
three-button suggestion bar above its language-specific keys. Binding a field lets focus
activate the keyboard without adding another editor to the page.

The keyboard remains open when focus moves outside it. Enter/Done commits
autocorrect, emits READY on the external field and keyboard, and closes without
inserting a newline. The X control at the right of the suggestion bar closes
it and emits CANCEL only on the keyboard. Closing clears the active target
before application callbacks; clicking a bound field reopens it. Programmatic
text, including newlines, is independent of these input actions.

`lv_advanced_keyboard_set_language` selects English US (`en-US`), Spanish
(`es-ES`), French (`fr-FR`), German (`de-DE`), Italian (`it-IT`) or Portuguese
(`pt-PT`). French uses AZERTY and German uses QWERTZ; the other layouts use
QWERTY. Spanish, French, Italian and Portuguese include a separate accents
page, and German includes umlauts and sharp S. The language metadata functions
enumerate the supported IDs and labels. Changing language preserves the
active field and its text and resets Shift and the current page. The prediction
trie is separate; the bundled vocabulary is English, so load an appropriate
trie for other languages. No extra dictionary is implied by a key layout.

Additional layouts are Mandarin Chinese Pinyin (`zh-CN`), Arabic (`ar-SA`), Hindi
(`hi-IN`), Bengali (`bn-BD`), Indonesian (`id-ID`), Urdu (`ur-PK`) and Russian
(`ru-RU`), plus Greek QWERTY (`el-GR`) with an accented-letter page. Mandarin has an independent Pinyin preedit buffer and paged Chinese
character candidates. Choose a candidate or press Space to commit; Enter commits
and dismisses. Backspace first edits preedit. Switching targets or languages,
pressing X, or entering password mode clears uncommitted composition.

Arabic and Urdu default to right-to-left. The Creator **Right-to-left display**
checkbox and `lv_advanced_keyboard_set_right_to_left()` override that direction
for the current and subsequently selected target fields. Text remains in logical
Unicode order; bytes are never reversed. Textarea fonts and explicit alignment
remain authored properties. Choose a target font that covers the selected script.
The keyboard's own keys and candidates include script fonts with control-symbol
fallbacks.

Build editable RTL screens with `LV_USE_BIDI=1`. Keep
`LV_USE_ARABIC_PERSIAN_CHARS=0` for native textarea input: upstream LVGL's label
shaping rewrites the text used by its textarea API. RTL ordering and canonical
input work without that rewriting. Stock LVGL does not provide full Arabic/Urdu
contextual shaping or Hindi/Bengali OpenType conjunct/mark positioning in these
editable fields. The Windows simulator uses native OS shaping. Applications
requiring fully shaped native editable text need a renderer with that support.

Full desktop builds enable all script fonts and Pinyin. Minimal generated SDKs
include the fonts and Pinyin data needed by the project's selected languages;
regenerate a profile after adding a language. Applications switching to additional
languages dynamically must enable their font macros and, for Mandarin,
`LV_ADVANCED_KEYBOARD_USE_PINYIN`. Setting that macro to zero removes the Pinyin
dictionary. See `KeyboardHindiFont.md` for reproducible font generation and OFL
notices for the additional fonts.

The keyboard provides lowercase and uppercase letters, one-shot Shift,
double-tap Shift lock, two symbol pages, space, backspace and Enter. It reuses
key buttons when changing pages. The bundled font covers its key labels,
including accented letters, currency and control symbols. A root text-font override applies to
the suggestion buttons and keys.
External text fields retain their own fonts; choose a field font with the
selected language's glyphs when displaying accented text.

Hold a character key for one second to open its Unicode variants in a row
beside the key. Keep holding and slide onto a variant: the option under your
finger is highlighted, and releasing inserts it through the normal text-input
path. You can also release over the original key, then tap a variant. The held key itself is suppressed; a short tap still inserts
the original character. Left-half keys open the options to their right;
right-half keys open them to their left. Options wrap into multiple rows to
fit the current display, with vertical scrolling on very small displays.
Tapping outside, changing page or language, resizing,
closing or deleting the keyboard dismisses it. One-shot Shift is consumed by
the selected variant; Shift Lock stays active.

Each insert key in `layout/*.json` may define `"variants": ["À", "Á", "Â"]`.
Lists are ordered, contain unique nonempty Unicode strings, and can include
combining sequences. Omit the property or use an empty list for no popup;
system keys cannot define variants. Existing version 1 definitions still work.
Regenerate the immutable C++ tables with `python layout/generate_languages.py`.

`include/lv_advanced_keyboard.h` is the public widget C API. Calls run on the
LVGL thread; the parent owns the returned root. External textareas are
borrowed, retain their own text/placeholder/password/length configuration and
remain usable independently. The root's LVGL user-data pointer remains free
for the application and document metadata.

The built-in English model supplies real prefix completions, contextual
next-word suggestions and keyboard-weighted spelling correction. Autocorrect
uses an explicit probability threshold; the default is 0.3. Its candidates
include keeping the typed word. Digit-bearing tokens and protected number
and calendar words are retained. Password fields suppress predictions.
Suggestion acceptance preserves capitalization and replaces the word at the
caret. Custom project tries select another vocabulary or domain model.
French, German and Greek vocabulary-filtered models keep lowercase spelling;
their query path preserves German sharp S and Greek final sigma instead of
applying a second Unicode case fold. The keyboard's precomposed script keys
match the models' NFC spelling. Original Unicode-fold APIs remain available.

`include/lv_advanced_keyboard_prediction.h` exposes the portable engine.
`lv_advanced_keyboard_predictor_create_builtin()` uses shared immutable
English NGTR/NGIX arrays. `predictor_create_borrowed` accepts NGTR v3 and NGIX v1
arrays directly from flash, without alignment restrictions. ESP32 and Arduino
ESP32 automatically copy model/index bytes into enabled, initialized PSRAM when
there is enough space. Keyboards with identical models share that allocation;
the last owner releases it. Without available PSRAM they keep borrowing the
original flash or RAM arrays. Other targets continue borrowing without copies.
`predictor_create` borrows raw NGTR bytes and builds an owned index once.
Keep borrowed bytes alive until the predictor is destroyed. Each predictor
has its own scratch workspace; inference allocates no heap memory. Instances
are independent and one is required per concurrent caller.

The default trie contains 5,691 nodes in 16,384 model bytes plus 15,627 index
bytes. Workspace size is exposed through `predictor_workspace_bytes()`
(the returned size includes the predictor's private ownership metadata). Index construction uses a
transient four-byte mass entry per node plus its output buffer, preferring
PSRAM on ESP32 and falling back to the normal heap. Exporters can
build that index ahead of time with `layout/build_ngtr_index.py`, leaving both
arrays in firmware flash.

Applications can override suggestions with
`lv_advanced_keyboard_set_suggestion_lookup`. Its folded UTF-8 token inputs
are borrowed length-delimited views. Return at most three suggestions of at
most 256 bytes each; no terminating NUL is required. Malformed UTF-8 and
embedded NUL disable the corresponding slot. Callbacks must not throw,
delete objects or mutate LVGL; their context must remain alive while
attached. All predictor symbols are isolated in
`espos::keyboard::prediction`, allowing applications to link an independent
`ngram_trie` library without conflicting headers or symbols.

Build with C++17 and LVGL button, label and textarea support. No flex layout,
LVGL keyboard widget, board driver or display rotation API is required.
`LVSERIAL_USE_ADVANCED_KEYBOARD` controls the widget;
`LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION` controls the three engine source
files and autocorrect code. `LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE`
controls default model arrays separately, so custom-only firmware can omit
them. Generated source projects include all required headers, pregenerated
layouts, Unicode tables, fonts and selected model data. Python is only needed
to regenerate layouts (`layout/generate_languages.py`) or flash indices.

See [PROVENANCE.md](PROVENANCE.md),
[models/PROVENANCE.md](models/PROVENANCE.md), and
[KeyboardFont.LICENSE.txt](KeyboardFont.LICENSE.txt) for source/data details.
