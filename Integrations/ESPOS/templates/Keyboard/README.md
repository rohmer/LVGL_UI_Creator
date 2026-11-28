# ESPOS Advanced Keyboard

The portable keyboard, prediction engine, layouts, models, tools and legacy
typing-view adapter come from the pinned AdvancedKeyboard dependency at
`third_party/AdvancedKeyboard`. The `AdvancedKeyboard/` component is a thin build
wrapper; the parent `Keyboard` component owns board display/touch startup.
Initialize the dependency from the ESPOS project root before configuring:

```powershell
python tools/setup_advanced_keyboard.py --project-root . --kind espos --init
```

`advanced-keyboard.lock.json` pins the source revision. Configuration never
downloads or changes the dependency. Make portable fixes in the AdvancedKeyboard
repository, then deliberately update the lock for both consuming projects.

## Build and model selection

Enable `lvgl.enabled: true` in `espos.yaml` for the on-device UI. A headless
configuration still builds `ngram_trie` without LVGL or the UI fonts. The
14 layouts are en-US, es-ES, fr-FR, de-DE, it-IT, pt-PT, el-GR, zh-CN, ar-SA,
hi-IN, bn-BD, id-ID, ur-PK, and ru-RU.

The language dropdown chooses the layout and a compiled matching prediction
model. The adjacent trie dropdown lists the sizes actually included in this
firmware and a None option. Arabic and Urdu enable RTL by default; the checkbox
can override this. Enter commits and dismisses the keyboard; X dismisses it.
Selecting a bound textarea reopens the keyboard and keeps that field visible.

Every target independently supports unrestricted **Alpha Numeric** (default),
**Upper case**, **Lower case** and **Numeric** input. Configure fields with
`lv_advanced_keyboard_set_textarea_input_mode(field, mode)` and the constants
`LV_ADVANCED_KEYBOARD_INPUT_ALPHANUMERIC`, `_UPPERCASE`, `_LOWERCASE`, `_NUMERIC`.
The keyboard follows the active field immediately. Case conversion preserves
Unicode accents and scripts and applies to physical typing, paste, suggestions,
autocorrect and programmatic edits. Existing accepted-character/length limits
still apply. Numeric accepts only 0–9 and one decimal point, with a 12-key numeric
pad plus Enter/X controls; prediction/autocorrect/Pinyin are disabled there.
The lightweight `lv_textarea_input_mode.h` helper also works without a keyboard.
Legacy board integrations can use `set_typing_view_input_mode(view, mode)` and
`typing_view_input_mode(view)` for their active editor. A different selected field
retains its own restriction.

The default firmware contains the English 16 KiB corpus model. Choose the model
pairs to compile using the semicolon-separated `ADVANCED_KEYBOARD_MODELS` CMake
cache setting, for example:

```powershell
idf.py -B build-keyboard -D "ADVANCED_KEYBOARD_MODELS=en-US:16;zh-CN:16;el-GR:16" build
```

Use `all` only when the application flash partition is large enough; `none`
includes layouts and Pinyin without bundled prediction models. Available model
pairs and their exact trie/index byte sizes are in
`third_party/AdvancedKeyboard/models/bundled/catalog.json`. A generated `bundled-models.json` build
report records what was linked. The bundled NGTR label describes the model
profile, not its exact flash footprint; both NGTR and NGIX consume flash.

If an NGPK model is installed in the existing `ngram` partition and opened by
`ngram_trie::StartTrieRuntime()`, the default board factory uses it and displays
Partition / custom. Selecting a bundled size replaces that predictor. Immutable
model/index bytes prefer one shared PSRAM copy when available and use mapped
flash otherwise. Every keyboard retains its own inference workspace. The
partition's mapping remains alive until the final borrowing keyboard releases
it, even when the default runtime closes or reloads its own predictor.

For CrowPanel ESP32-P4, retain the existing board sdkconfig defaults and build
with `IDF_TARGET=esp32p4`; the board drivers are separate from the shared core.
The keyboard requires LVGL logical bidi (`CONFIG_LV_USE_BIDI=y`) and disabled
`CONFIG_LV_USE_ARABIC_PERSIAN_CHARS` to retain logical Unicode in textareas.
Existing sdkconfig values override appended defaults. See
`third_party/AdvancedKeyboard/src/KeyboardScriptFonts.md` for font licensing and script limits.

## Project-wide keyboard from UI Creator

In LVGL UI Creator, select **Project** above **Screens**, then enable
**Advanced Keyboard** in the Properties panel. Language, prediction trie,
autocorrect, right-to-left display and appearance are configured once for the
whole project. Individual text fields retain their input-mode restrictions.

The editor does not add a visible keyboard to the design or widget palette.
Run (F6), generated C and the protobuf/XML display runtimes create a hidden
keyboard for each screen, bind that screen's text fields, and open it when a
field is selected. Enter or X dismisses it; selecting a field reopens it.
Selected custom models are embedded and shared. Disabled project keyboards
are omitted from runtime exports along with their unused model assets.

Export or regenerate the application's UI to use these options on ESPOS. The
generated code uses the existing pinned portable keyboard API; no Creator
inspector code or separate keyboard implementation belongs in this project.
These authoring options do not replace the language/model controls of an
independently created legacy ESPOS typing view described below.

## Bind externally owned fields

New integrations can use the portable C facade directly. The same object can
watch multiple textareas, with no owned textbox or board dependencies:

```cpp
#include "lv_advanced_keyboard.h"
#include "espos_keyboard/BundledModels.h"

auto* keyboard = lv_advanced_keyboard_create(screen);
lv_obj_set_size(keyboard, LV_PCT(100), 260);
lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
lv_advanced_keyboard_bind_textarea(keyboard, first_field);
lv_advanced_keyboard_bind_textarea(keyboard, second_field);
lv_advanced_keyboard_set_language(keyboard, "zh-CN");
espos::keyboard::bundled::Apply(keyboard, "zh-CN", 16);
lv_advanced_keyboard_set_autocorrect(keyboard, true);
```

Run these calls on the LVGL thread. Textareas and custom lookup contexts are
borrowed. Explicit custom predictors remain caller-owned; destroy them after
detaching. `bundled::Apply` owns its predictor until replaced, cleared or the
keyboard is deleted. Mandarin Pinyin composition works independently of the trie.

## Legacy board API

`#include "espos_keyboard/TypingView.h"` remains source compatible for consumers
of the `Keyboard` component. Existing factories create an owned textarea plus
language, trie-size, RTL and optional orientation controls. Access the portable
C facade with `typing_view_advanced_keyboard(view)` to bind more textareas or use
other advanced settings; `set_typing_view_editor(view, field)` switches targets.
The raw layout-level `create_en_us_keyboard` API is still available.

The canonical low-level typing view lives in `espos::keyboard::view` and is
declared by `espos_keyboard/AdvancedTypingView.h`. The legacy public functions
remain in `espos::keyboard`, avoiding collisions or duplicate implementations
when the Creator core and ESPOS adapter are linked together.
