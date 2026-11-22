# Figma import tests

The fixture is a small REST-compatible design tree containing two frames,
nested groups, rounded/bordered rectangles, an ellipse, a hidden layer and
Unicode text. It has no external resources or account requirements.

`UICreatorFigmaTests` covers the file, node-response and plugin-bundle formats;
editable widget geometry/styles; frame selection, scaling and hidden layers;
self-contained raster assets and opacity; malformed input and nesting limits;
bordered parent positioning in LVGL; native project loading; and C,
Protobuf, UI Creator XML and MicroPython export of the imported result. Raster
tests embed the repository's sample PNG without network access.

`UICreatorFigmaEditor` exercises the File menu and real SDL text input in the
importer, asynchronous review, appending/replacing screens, selected-frame
scaling, image ID/name collisions, repeated import resource reuse, preserved
variable bindings/history, a single undo/redo operation per import, failed file
loading and closing the window. It runs with SDL's dummy video/software renderer.

When Node.js is available, `UICreatorFigmaBridge` runs the exporter plugin's mock
Figma API tests from `Integrations/Figma/test.mjs`.

`UICreatorFigmaOnlineTests` uses an injected HTTPS transport to exercise link
parsing, API file/node loading, selected-frame raster requests, version-pinned
rendering, batching, image caching, CDN downloads without credentials, useful
HTTP errors, response redaction, cancellation, and C/MicroPython export plus
Protobuf runtime loading. URL/header rejection also exercises the native
transport before any network request. The editor test types into the online form,
checks password masking and rejects invalid links or missing tokens without
changing the project. These tests need no account, API token, or internet access.

Build and run from an existing configured desktop build:

```text
cmake --build <build-directory> --target UICreatorFigmaTests UICreatorFigmaOnlineTests UICreatorEditorTests
ctest --test-dir <build-directory> -R UICreatorFigma --output-on-failure
```

The tests validate the local importer and generated projects. They do not use a
live Figma account or attest to Figma plugin execution; the exporter is validated
separately against its documented API and bundled format.
