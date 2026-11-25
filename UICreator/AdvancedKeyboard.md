# Advanced Keyboard

Add **Advanced Widgets → Advanced Keyboard**, then select a text area or a
custom Text box. The keyboard appears and types into that field. Select a
different field to switch targets without losing either field's text. The
keyboard contains suggestions and keys; it does not contain its own text box.
Opening the keyboard or switching targets automatically scrolls the selected
field into view above it. These temporary viewport changes preserve the saved
screen layout and are restored when the keyboard closes.

The keyboard stays open when focus moves elsewhere. Press **Enter** to commit
the current word and close it, or tap **X** to dismiss it. Enter sends the field
a ready event without inserting a newline. Clicking a text field again reopens
the keyboard. Password fields suppress suggestions and autocorrect.

Select a text area or Custom Text box and set **Input mode** in its properties:
**Alpha Numeric** is the unrestricted default, **Upper case** and **Lower case**
convert letters and lock the keyboard's letter case, and **Numeric** accepts
`0`–`9` with one decimal point. Numeric targets show a keypad with digits,
decimal and delete; Enter and X remain available to close it. Pasted text and
physical keyboard input follow the same restrictions. The keyboard switches
mode automatically when you select a different text field. Existing character
and length limits still apply.

Select Advanced Keyboard in the Hierarchy to configure it:

- **Target text box:** use the focused field automatically, or restrict the
  keyboard to a particular text area/Text box.
- **Language:** choose English (US), Spanish, French, German, Italian,
  Portuguese, Mandarin Chinese (Pinyin), Arabic, Hindi, Bengali, Indonesian,
  Urdu, Russian or Greek. The keys use that language's layout and letters.
  This setting is saved per keyboard and preserved in C exports.
- **Trie size:** select a 16 KiB or 256 KiB corpus model for the chosen language.
  The bundled library contains both sizes for English, Spanish, French, German,
  Greek, Arabic, Hindi and Mandarin Chinese. New keyboards start with the English 16 KiB model.
  Changing language keeps the selected size where available. The inspector
  reports actual model and index sizes; size labels describe the corpus tier,
  and a few supplied 256 KiB models exceed that nominal size by their headers.
  **None** disables predictions. **Built-in English** selects the earlier
  constant dictionary. An explicitly loaded model remains a custom selection.
- **Right-to-left display:** Arabic and Urdu enable this by default. Toggle it
  to choose the active text box's typing direction. Saved text stays in logical
  Unicode order. The field's authored direction is restored on dismissal or a
  target switch; temporary editing styles do not enter saved projects.
- **Prediction trie:** choose built-in English, an embedded project trie, or
  None. Click **Load trie…** to select an NGTR v2, compact v3 or compressed radix v4 file for the desired
  prediction and autocorrect vocabulary. Loaded models and
  their indices are embedded and shared in the project, so designs travel
  between Windows, ESP32, Raspberry Pi/Linux and Arduino without file paths.
- **Autocorrect:** enable correction at word boundaries. Its threshold ranges
  from 0 to 1; the default is 0.3. Raise it to require more confidence.
- **Key colors/radius:** customize the key appearance. Geometry and font
  controls remain in the usual inspector.

The sixteen corpus files supplied from `D:\Corpus` are included as editor
resources. Selecting a language and size imports that model and generates its
index once; both are embedded and deduplicated in the project. Saving, moving
or creating a firmware project does not require the original corpus folder.
Created Protobuf projects and generated C include only referenced models;
the editable project's model library keeps the imported choices.

The other keyboard languages currently have no supplied corpus model. Their
inspector says so and offers **Load trie…**. Changing to one of these languages
clears a previously selected corpus bundle rather than using another language's
vocabulary. Changing the layout preserves an explicitly loaded custom trie.
See [the bundled model catalog](KeyboardModels/README.md) for original filename
aliases, byte counts and checksums.
The key labels use the bundled language font. The target field keeps its own
font; choose one containing the language's glyphs to display accented text.

Mandarin uses Pinyin composition and paged Chinese character candidates.
Type a syllable, choose a candidate, or use Space to accept the first visible
candidate. Enter commits and closes; X discards uncommitted Pinyin. Composition
is cleared when switching targets/languages or entering a password field.
Committed Han text uses character-based trie contexts and inserts predictions
without adding Latin word spaces.

Arabic, Hindi, Bengali, Indonesian, Urdu, Russian and Mandarin are also accepted
by the language-model generator and the source Corpus Workbench tools. Use
**Language Models > Generate language model...** or see [Corpus Workbench](../Tools/README.md).
The keyboard's fonts cover its keys and candidates. Target fields keep their
authored fonts, so select matching script glyphs in the field's font settings.
Native LVGL supports logical Unicode input and RTL ordering here; full
Arabic/Urdu contextual shaping and Hindi/Bengali conjunct/mark positioning remain
renderer limitations. The Windows simulator uses OS text shaping. See the
[native keyboard notes](../LVSerial/Widgets/AdvancedKeyboard/README.md).

Type a prefix to see word completions; after a word, the model uses up to two
previous words for next-word predictions. Tap a suggestion to accept it.
Autocorrect uses the ESPOS keyboard-weighted edit scorer, word frequencies and
context. It checks Space, Enter and punctuation from virtual and physical input.
Numeric/protected words and programmatic text updates remain unchanged.

Tap Shift for one uppercase insertion, or double-tap within 300 ms to lock it.
Tap Shift again to unlock. **#@!** opens symbols, **=\<** opens the second page,
**123** returns to the first, and **ABC** returns to letters. UTF-8 text and
backspace are supported. The original ESPOS EN-US layout remains the default.

The keyboard is one authored object; its private controls are recreated on
load. Settings, target references and project tries survive JSON, Protobuf,
undo/redo and C export. Earlier projects with a combined editor/keyboard migrate
their text into an independent text area. Bind application string variables to
the target field's `/textarea/text` or `/textBox/text` property.

The bundled LVGL 9.6 backend is required. C exports include the C++17 engine,
shared constant trie/index arrays and a C API. On ESP32, including Arduino,
models automatically prefer enabled, initialized PSRAM for the compact trie
and index. Keyboards using the same model share one copy. If PSRAM is absent
or cannot fit the model, embedded loaders borrow immutable flash bytes and
file loads retain owned model bytes. Windows and ordinary memory loads retain
owned assets after the input buffer is released. Each keyboard owns its
inference workspace. Inference performs no heap allocations.

Native applications can attach fields using `lv_advanced_keyboard_bind_textarea`
and select models using `lv_advanced_keyboard_set_predictor`. The predictor API
supports borrowed NGTR/NGIX ranges and a bundled English factory. Keep borrowed
bytes alive while attached and make LVGL calls on the UI thread. Source
provenance, the model notes and font license accompany the implementation.
