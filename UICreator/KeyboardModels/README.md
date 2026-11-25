# Keyboard catalog integration

The catalog and all bundled tries are maintained only in
[`3rdParty/AdvancedKeyboard/models/bundled`](../../3rdParty/AdvancedKeyboard/models/bundled).
This directory contains the editor adapter that reads that catalog and loads
selected models. It contains no copied models or generated catalog table.
Builds stage dependency resources beside the executable; projects embed only
selected models. Rebuild models in AdvancedKeyboard, commit/push there, then
advance the shared dependency pin.
