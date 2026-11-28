# Add-object icons

This catalog contains a distinct, original vector icon for every addable object:
39 addable widgets plus one retained legacy keyboard icon, 36 Bootstrap components and 13 page templates. The Welcome screen
reuses the corresponding template icons. Saved user components keep their actual
rendered thumbnails.

Hierarchy rows reuse these same catalog entries at 24 pixels. Native widgets are
identified by their type, including the gauge/line-meter variant. Bootstrap and
template roots retain their catalog key in `base.design.paletteIcon`, so renaming,
copying and saving do not change their icon. This authoring metadata is stripped
from device exports with the other `design` fields. Older records without a key
use their native widget icon.

The editor embeds compact geometry and draws it with LVGL rectangles, lines,
arcs and triangles. Icons require no icon font, image decoder, filesystem access
or per-icon bitmap allocation. Their 32-unit geometry scales with the icon's
object size. Captions remain separate and icons do not intercept clicks or focus.

Edit `widgets.json`, `bootstrap.json` or `templates.json`, preserving canonical
IDs/names and unique namespaced keys. Commands use the five roles in
`generate.py`: primary, accent, blue, muted and transparent (`none`). Keep ink
inside the 32 by 32 view box and use strokes around 1.6–2 units.

Regenerate the checked-in native geometry, catalog and 89 standalone SVG files:

```sh
python UICreator/Assets/ObjectIcons/generate.py
python UICreator/Assets/ObjectIcons/generate.py --check
python UICreator/Assets/ObjectIcons/generate.py --preview build/object-icons-preview.svg
```

The generator uses only Python's standard library. Normal application builds
consume `ObjectIconData.inc` and need no asset-generation step. Standalone SVGs
are in `svg/`, with human-readable titles and transparent backgrounds.

`UICreatorObjectIcons` tests all 89 native renders at 24, 32 and 48 pixels,
rejects blank or duplicate 32-pixel images, checks three palette widths, and
exercises actual icon clicks, template shortcuts and undo. It writes the native
contact sheet and verification manifest in the build's `object-icon-tests`.

`UICreatorHierarchyIcons` checks all 39 addable widget mappings and 49 recipe roots,
project persistence, duplication, undo/redo, export stripping, and interaction
with icons in nested, filtered and narrow hierarchy rows. Its captures are in
the build's `hierarchy-icon-tests` directory.
