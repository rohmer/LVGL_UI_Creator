# Project themes and reusable components

Open **Assets > Theme gallery** to choose a project palette. Each tile contains a miniature native LVGL screen and coordinated status swatches. Click a tile, then choose **Apply to project** or **Apply to selection**. Project themes also become the default for newly created widgets and Bootstrap components.

The details panel also renders **your current screen** in the chosen palette, using an isolated native runtime. Choosing a palette does not change the project. **Preview and apply** opens a larger review with **Apply preview** and **Cancel preview**. The direct Apply commands remain available after inspecting the screen preview. Closing or cancelling a preview discards its candidate, and applying creates an undoable change. Project-wide theme changes resolve shared tokens on every screen.

The gallery includes fourteen palettes:

| Palette | Appearance | Character |
| --- | --- | --- |
| Porcelain | Light | Cool white, graphite actions, indigo accents |
| Midnight Slate | Dark | Layered charcoal, soft blue, lavender |
| Ocean Mist | Light | Marine blue, white, sea-glass green |
| Deep Ocean | Dark | Inky navy, cyan, turquoise |
| Sage Studio | Light | Warm white, sage, pine green |
| Forest Night | Dark | Evergreen, mint, soft gold |
| Warm Sand | Light | Linen, umber, amber |
| Amber Atelier | Dark | Espresso, warm gold, muted teal |
| Lavender Paper | Light | Lavender haze, violet, cool teal |
| Amethyst | Dark | Deep plum, lilac, ice blue |
| Rose Quartz | Light | Blush, berry, stone blue |
| Velvet Rose | Dark | Burgundy, rose, pale blue |
| Architect | Light | Sharp monochrome, electric blue |
| Carbon | Dark | Black, silver, cyan |

Each palette defines background, surface, text, muted text, primary, accent, success, warning, error, border, and text-on-primary colors, plus spacing and corner radius. The **Shared design tokens** controls let you edit the chosen palette. Enter colors as `#RRGGBB` and sizes as decimal values from 0 to 128. **Update shared token** changes participating widgets together, including widgets themed by an earlier selection-only operation with that palette.

Changing a style property directly in the inspector creates a local override for that property. Later shared-token edits preserve it and continue updating the widget's other bound properties. Reapplying a palette explicitly restores its bindings. Themes preserve widget content, dimensions, placement and fonts. QR codes and barcodes preserve their authored foreground/background contrast.

**Selected style inheritance** shows each participating property with a **shared token** or **local override** badge. Use **Refresh selected styles** after selecting another widget. **Reset selected override** reconnects only the chosen property to its shared token; other local overrides remain intact. With the screen selected, the same controls inspect its canvas style.

Open **Assets > Reusable components** to manage the project's component library:

1. Select a widget, parent container, or group on the canvas. Capture includes its descendants.
2. Enter a descriptive name and click **Capture selection**. The original selection remains an ordinary editable group.
3. Choose a saved definition and click **Insert linked instance**. Move the inserted group and edit its content on the canvas.
4. To change the shared design, edit a linked instance, select it, and click **Update selected master**. Review the changed properties, added/removed widgets and candidate screen, then click **Apply master update** or **Cancel preview**. Confirmation updates the definition and propagates it to linked instances on all screens. Existing instances keep their placement and per-property content/style overrides. If the project changes while a review is open, make a new review before applying.
5. **Detach selected instance** converts that instance to ordinary widgets. **Rename definition** changes its library name. **Delete unused** removes a definition once no linked instances use it.

Duplicating a complete instance creates a separate linked instance. Duplicating only part of an instance creates ordinary widgets. Internal Advanced Keyboard textarea targets and visual-event object targets are remapped during insertion/duplication. After deleting target widgets in a master, run screen validation to review any remaining external references.

The library includes a case-insensitive name search and a native thumbnail of the selected definition. Searching and inspecting thumbnails do not modify the project. Thumbnails show the component's captured content and styles; they do not run application actions or retain an interactive runtime.

**Export library** writes a portable `.json` bundle of definitions and the project's font, image and trie assets. Exported definitions keep their captured concrete appearance; apply a destination palette when you want them to participate in its shared tokens. **Import library** adds definitions with fresh component IDs. Matching asset IDs are reused; conflicting asset IDs are rejected so an import cannot silently replace an existing screen's assets. Import the library into a project with compatible LVGL widget support.

Theme and component changes support Undo/Redo and are included in saved project JSON and authoring protobuf metadata. Device protobuf and generated C exports contain concrete widget styles and records, with authoring metadata removed. Windows, ESP32, Linux/Raspberry Pi and Arduino runtimes display the same native LVGL widgets without a desktop theme or component engine. No additional LVGL widget dependency is introduced by a theme or component definition.

## Implementation and verification

`LVSerial/Serialization/ProjectDesign.h` contains the portable JSON transformations. `Materialize(document)` resolves live theme bindings. `Materialize(document, true)` additionally removes authoring data for device/C exports. Component instances are always stored as ordinary widget records; library updates resolve links before export. New widget defaults are applied through `ProjectAuthoring::ApplyNewDefaults`.

The standalone core test can be run from the repository root with `Tests\Design\run-isolated.cmd` on the configured Windows development machine. The editor integration suite's `--design-tools` case exercises the native gallery, shared tokens, Undo/Redo, linked instances, master updates and protobuf persistence. Preset tests check at least 4.5:1 contrast for body text on surfaces and text on primary buttons.
