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

Select Advanced Keyboard in the Hierarchy to configure it:

- **Target text box:** use the focused field automatically, or restrict the
  keyboard to a particular text area/Text box.
- **Language:** choose English (US), Spanish, French, German, Italian or
  Portuguese. The keys use that language's layout and accented letters.
  This setting is saved per keyboard and preserved in C exports.
- **Prediction trie:** choose bundled English, an imported project trie, or
  None. Click **Load trie…** to select a compact NGTR v3 file for the desired
  prediction and autocorrect vocabulary. Loaded models and
  their indices are embedded and shared in the project, so designs travel
  between Windows, ESP32, Raspberry Pi/Linux and Arduino without file paths.
- **Autocorrect:** enable correction at word boundaries. Its threshold ranges
  from 0 to 1; the default is 0.3. Raise it to require more confidence.
- **Key colors/radius:** customize the key appearance. Geometry and font
  controls remain in the usual inspector.

The included dictionary is English. Selecting another language clears the
bundled English trie; load a matching model to enable that language's
suggestions and autocorrect. Changing the layout preserves an explicitly
loaded trie. Language and trie selection are separate settings.
The key labels use the bundled language font. The target field keeps its own
font; choose one containing the language's glyphs to display accented text.

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
shared constant trie/index arrays and a C API. ESP32/Arduino embedded loaders
borrow immutable trie bytes directly from flash; Windows and ordinary memory
loads retain owned assets after the input buffer is released. Each keyboard
owns its inference workspace. Inference performs no heap allocations.

Native applications can attach fields using `lv_advanced_keyboard_bind_textarea`
and select models using `lv_advanced_keyboard_set_predictor`. The predictor API
supports borrowed NGTR/NGIX ranges and a bundled English factory. Keep borrowed
bytes alive while attached and make LVGL calls on the UI thread. Source
provenance, the model notes and font license accompany the implementation.
