# ESPOS adapter and headless runtime regression tests

Run from a native compiler shell with an LVGL static library built using the
same `LV_CONF_PATH`. No ESPOS checkout or hardware is modified. For example,
from the Creator repository in a Visual Studio x64 Native Tools prompt:

```powershell
cmake -S Integrations/ESPOS/tests/NativeAdapter -B build/espos-native-adapter -G Ninja -DCMAKE_BUILD_TYPE=Release "-DLVGL_ARCHIVE=<absolute-path-to-matching-lvgl.lib>"
cmake --build build/espos-native-adapter --parallel 3
ctest --test-dir build/espos-native-adapter --output-on-failure
```

Adjust the archive path for your checkout. `LV_CONF_PATH` defaults to the
Creator's `3rdParty/LVGL/lv_conf_v9.h`. This fixture compiles the current
canonical sources together with the versioned ESPOS adapter templates.

`ESPOSNativeAdapter` covers legacy and portable header/API coexistence, owned
editor creation, orientation, all 14 language metadata entries and Greek,
automatic Arabic direction and manual overrides, actual trie-size dropdown
selection, multiple external target fields, Enter/X dismissal and field-click
reopening, and editor/keyboard/container deletion orders. It compiles actual
English, Arabic, and both Mandarin model sizes from the Creator catalog. Arabic
has only the 256 KiB profile compiled, verifying that changing languages picks
an available model when the previous size is unavailable, while missing-model
languages remain None and custom lookup callbacks remain attached.

`ESPOSHeadlessRuntime` builds the canonical engine and ESPOS flash runtime
without LVGL. Partition/heap adapters simulate ESP-IDF mapping and capabilities
allocation. A real English 256 KiB model exercises NGPK validation, larger
models, shared PSRAM allocation without duplicate model copies, absent PSRAM
fallback, mapping leases after the default predictor closes, malformed images,
and final allocation/mapping cleanup.

Native tests do not replace a linked ESP-IDF firmware build or hardware input,
rendering and panel rotation verification.
