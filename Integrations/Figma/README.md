# Importing a Figma design

Use **File → Import Figma design…** in LVGL UI Creator to import Figma frames as
editable screens. Choose an online design link or a local JSON file, select one
frame or all frames, and set the scale. The import can append screens to the current project or replace the
project. Review the conversion report before importing; the import is undoable.

## Import an online design

1. In Figma's account settings, create a personal access token with the
   **`file_content:read`** scope. Your account must have access to the design.
2. Open **File → Import Figma design…** in LVGL UI Creator and choose
   **Online Figma link** under **Source**.
3. Paste the design's `https://www.figma.com/design/...` or `/file/...` link.
   A link containing `node-id` loads that frame or node; a file link lists the
   available frames. Enter the token in **Access token**.
4. Click **Load from Figma**. Choose a frame or all frames, scale, and whether
   to include hidden layers, then click **Review conversion**.
5. Review the conversion notes and click **Import design**. The default adds
   screens to the current project; **Replace project** replaces its contents.
   Either choice can be undone with one Undo operation.

The importer reads the file through Figma's official REST API and downloads PNG
renders for complex artwork. Text and basic layers stay editable. Render requests
are batched, selected-frame imports download only the needed artwork, and repeated
reviews reuse downloaded images. These limits also apply to online imports: large
or unsupported designs may need to be split into smaller frame selections.

The token is masked and is kept only in the import window and its active request;
it is never saved in project files, generated code, logs, or request URLs. Closing
the window releases the credential. **Cancel request** stops ongoing work without
changing the project. A downloaded design and its artwork are embedded in the
project, so exported projects do not need an internet connection or Figma token.

The API requires a token even for a design shared through a public link. Figma's
file permissions and API quotas still apply. Permission failures and rate limits
are shown in the import window; for a rate limit, wait for the reported retry
interval before loading or reviewing again. See Figma's
[personal access token setup](https://developers.figma.com/docs/rest-api/personal-access-tokens/),
[access scopes](https://developers.figma.com/docs/rest-api/scopes/), and
[rate limit documentation](https://developers.figma.com/docs/rest-api/rate-limits/).

Windows uses its built-in WinHTTP HTTPS support. Linux requires the system
`libcurl.so.4` shared library and a working CA certificate bundle; no extra
compiler or libcurl development package is needed to build the importer.

## Export from Figma

The included development plugin creates a portable `.figma.json` file with
embedded images. It requires no npm build, API token, server, or network access.

1. In the Figma desktop app, open the design you want to export.
2. Under **Plugins → Development → Import plugin from manifest…**, choose
   `Integrations/Figma/manifest.json` from the source checkout. Installed builds
   include it under `integrations/figma` beside the application.
3. Select one or more frames, components, instances, or groups. Run
   **LVGL UI Creator export** from the development plugins menu.
4. Choose **Export selected frames**, or export the top-level frames on the
   current page. Frames inside sections are included when exporting the page.
5. Click **Save Figma import file**. In LVGL UI Creator, use
   **File → Import Figma design…**, keep **Local Figma JSON** selected, and choose
   that file. Click **Load design** to review the import.

The manifest uses a local development ID, as in Figma's official
[Post Message sample](https://github.com/figma/plugin-samples/blob/main/post-message/manifest.json).
If your Figma installation asks for a registered plugin ID, use
**Plugins → Development → New plugin…** to create a **Custom UI** plugin.
Copy the supplied `code.js` and `ui.html` into that plugin's folder, replacing
the generated examples. Copy the settings from the supplied `manifest.json`,
while retaining the `id` assigned by Figma in the generated manifest. Import
that generated manifest, then run the plugin. Publishing to the Community also
requires a Figma-assigned plugin ID.

Selecting a parent and its child exports the parent once. The plugin reads the
current page; it does not modify the design. Hidden layers are excluded by
default. Enable them in both the exporter and importer if you want them imported.
Cancellation completes after the current Figma export operation returns.

## What remains editable

Frames, groups, components, and instances become containers. Text becomes labels;
rectangles, circles, and simple lines become native LVGL objects. Layer names,
hierarchy, geometry, solid colors, opacity, borders, and uniform rounded corners
are retained where the corresponding LVGL property is available. Auto layout
becomes the resolved layout at the exported size.

Online import and the plugin embed PNG images for image fills, vector artwork, boolean operations,
non-circular or partial ellipses, masks, gradients, rotations, mixed text styles,
and other visual features that cannot be represented directly. Only that layer
and its descendants are flattened. A native frame stays editable when it merely
contains complex artwork. Image fallback uses the layer's absolute bounds; review
effects near the edge of a layer for clipping after import. Large raster layers
are reduced to at most 4096 pixels on an edge and 16 million pixels per image.
The export limits are 10,000 layers, 24 MiB of encoded PNGs, and 48 MiB of JSON.

Figma font files are not exported. Native text uses the nearest available LVGL
font size and reports substitutions; add the desired project fonts in the editor
to refine typography. Figma interactions, prototype links, variables, component
semantics, and responsive layout rules require configuration in LVGL UI Creator.
Imported designs still support the editor's normal C, MicroPython, Protobuf, and
UI Creator XML export workflows, subject to each exporter's widget support.

## Other JSON sources

The importer also accepts the JSON response from Figma's file or file-nodes REST
endpoints, a `JSON_REST_V1` plugin export, or a direct node document. A raw REST
response does not include rendered image bytes: vectors, image fills, and complex
visuals need the bundle produced by the included plugin for image fallback.
The importer reports approximations and missing assets.

Figma's binary `.fig` file is not an import format. Open it in Figma and export
using the plugin above. Local JSON import does not need access to your Figma account.

## Bundle format and validation

The bundle adds `uicreatorFigmaVersion: 1` to a normal REST-style document with a
`DOCUMENT` root, a `CANVAS` child, and exported frame children. The optional
`uicreatorAssets` object is keyed by the source node ID:

```json
{
  "uicreatorFigmaVersion": 1,
  "document": { "type": "DOCUMENT", "children": [] },
  "uicreatorAssets": {
    "2:14": {
      "name": "Logo.png",
      "mimeType": "image/png",
      "data": "base64-encoded PNG bytes",
      "bounds": { "x": 10, "y": 20, "width": 48, "height": 48 },
      "scale": 1,
      "reason": "vector"
    }
  }
}
```

Asset bounds are absolute design coordinates. A rasterized node includes its
children; the importer creates one image for that node and skips its descendants.
The plugin requests PNG export with `useAbsoluteBounds: true` to use the same
coordinate extent. `scale` records the raster resolution; the bounds retain the
original design size. Assets remain embedded in the imported project.

For a dependency-free plugin smoke test, run from the repository root:

```sh
node Integrations/Figma/test.mjs
```

This uses a mocked Figma API to check selection deduplication, native and raster
layer handling, cancellation, limits, errors, and bundle/download messages.
Real Figma rendering and UI operation should also be checked with a design file.
For a visual check, include a 50% opacity native container with a 50% opacity
vector child and compare its appearance with Figma: the image already includes
the child's opacity, and the editor should apply the container's opacity once.
Also check rotated artwork, image fills, masked groups, and effects at layer
bounds. The automated plugin test does not emulate Figma's image renderer.

The plugin uses the official [JSON_REST_V1 and PNG export API](https://developers.figma.com/docs/plugins/api/ExportSettings/)
and declares [no network domains](https://developers.figma.com/docs/plugins/manifest/).
