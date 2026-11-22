# Menu and action icons

This catalog supplies 87 vector descriptors for menus, toolbars and window
controls. Each entry has an `action.*` key, a concise label, explicit action
aliases, and either original geometry or an intentional reuse of an object
icon. The 97 exact action bindings include all eight menu headings, all four
submenu headings, screen/start-screen controls and generic window actions.
Dynamic `recent:<path>` actions share the recent-project descriptor. Empty or
unknown actions return no icon; the catalog has no generic fallback.

Icons use the existing 32-unit object-icon geometry and primary/accent/blue/muted
palette. Runtime drawing uses the same native LVGL rectangles, lines, circles,
arcs and triangles. There are no icon fonts, bitmap assets or runtime SVG loads.
The original 84-entry add-object catalog and its enumeration remain separate.

`EditorIcons::Set` decorates existing editor buttons, preserving their caption as
child zero and their callbacks. Icon-only controls retain descriptive hover
labels; decorations never join keyboard groups or receive clicks. Menu rows
reserve separate icon, caption and shortcut columns. These assets stay in the
editor and are not included in authored screens or device display libraries.

`ObjectIcons::Action(action)` resolves an action ID, an exact `menu:`/`submenu:`
binding, a recent-file path or a canonical `action.*` key. `ActionAt(index)` and
`ActionCount()` enumerate these descriptors. `ObjectIcons::Set(icon, descriptor)`
updates an existing decorative icon, preserving its object, dimensions, focus
flags and other callbacks; unchanged or null descriptors do no work. The Run
control switches between `Action("run-toggle")` and `Action("run-stop")`.

Edit `actions.json` and regenerate the checked-in geometry, manifest and SVGs:

```sh
python UICreator/Assets/ActionIcons/generate.py
python UICreator/Assets/ActionIcons/generate.py --check
python UICreator/Assets/ActionIcons/generate.py --preview build/action-icons-preview.svg
```

The generator uses only Python's standard library and imports the existing
object-icon primitive/SVG helpers. Required action bindings, duplicate aliases,
palette roles, command geometry and stroke widths are checked before writing.
Application builds consume `ActionIconData.inc` directly and do not need Python
or an asset-generation step. SVG titles and the manifest preserve useful labels
for source review; the actual icons contain no text elements.

`UICreatorMenuToolbarIcons` verifies native rendering at 16, 18, 20 and 24 pixels,
menu mappings and compact layouts, toolbar actions, hover cleanup, and real
pointer clicks on menu, settings, close and Run/Design icons. Captures are saved
under the build's `menu-toolbar-icon-tests` directory.
