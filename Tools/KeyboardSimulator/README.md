# Keyboard Simulator

Build the Windows simulator with .NET 8:

```powershell
dotnet run --project Tools/KeyboardSimulator/KeyboardSimulator.csproj
```

In the ESPOS checkout the project is under lowercase `tools`, and keyboard
profiles come from the top-level `Keyboards` directory. The Creator mirror
loads its canonical `LVSerial/Widgets/AdvancedKeyboard/layout` definitions.

Select a language, orientation and panel size in the toolbar. Arabic and Urdu
start with RTL enabled; the RTL checkbox changes paragraph direction while
preserving logical Unicode text. Hindi/Bengali use InScript key positions,
Russian uses JCUKEN, and Indonesian uses QWERTY. Windows text controls use the
operating system's script fonts and shaping.

Mandarin uses Pinyin spelling with a shared LVGL character dictionary. Type a
syllable and select a character, or use Space/Enter to accept the first item
on the visible page. The arrow buttons page candidates; Backspace edits
uncommitted spelling. Moving the caret or switching languages clears spelling.
Pinyin works without a trie. **Load trie** adds NGTR completions and contextual
next-word suggestions. Mandarin models use Han character n-grams; committed
characters stay adjacent when accepting a suggestion. Other new languages
retain their combining signs and internal joiners when finding trie context.

Select the keyboard matching the model's generation language. A model does not
translate text or provide a complete sentence-level Chinese input method.
The Pinyin dictionary's license is included in `PinyinData.LICENSE.txt`.

Run the language input checks:

```powershell
dotnet run --project Tools/KeyboardSimulator.Tests/KeyboardSimulator.LanguageTests.csproj -- LVSerial/Widgets/AdvancedKeyboard/layout
```

The ESPOS equivalent uses `tools/KeyboardSimulator.Tests` and `Keyboards`.
An optional second argument supplies a Mandarin NGTR test model generated from
`你好世界。你好朋友。` to exercise real next-character prediction and acceptance.
