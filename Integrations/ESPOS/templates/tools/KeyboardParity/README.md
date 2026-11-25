# Keyboard parity bridge

This project compiles the exporter maintained in the pinned
`third_party/AdvancedKeyboard/tools/KeyboardParity` dependency and references
that dependency's GBUTest project. It contains no copied scoring or replay code.
The existing `dotnet run --project tools/KeyboardParity -c Release -- ...`
arguments are unchanged. Initialize the dependency first:

```sh
python tools/setup_advanced_keyboard.py --project-root . --kind espos --init
```

Build the native replay runner from its canonical CMake project:

```sh
cmake -S third_party/AdvancedKeyboard/tools/KeyboardParity -B build/keyboard-parity -DCMAKE_BUILD_TYPE=Release
cmake --build build/keyboard-parity --config Release --parallel
```

See the canonical tool README for reference export, recorded hash verification
and replay commands. For an explicit development checkout use MSBuild
`-p:AdvancedKeyboardRoot=<checkout>` (or `ADVANCED_KEYBOARD_ROOT` in the
environment). Default source is the dependency installed at the locked ESPOS
path, and no build step downloads or modifies it.
