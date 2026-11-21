# Project themes

The Theme gallery contains 28 coordinated screen palettes. Open **Assets → Theme gallery**, preview a palette on the current screen, and apply it to the project or selected objects. Previewing and canceling leaves the project unchanged; applying a palette supports Undo.

The original 14 palettes and their saved-project IDs remain available: Porcelain, Midnight Slate, Ocean Mist, Deep Ocean, Sage Studio, Forest Night, Warm Sand, Amber Atelier, Lavender Paper, Amethyst, Rose Quartz, Velvet Rose, Architect, and Carbon.

## Additional palettes

| Palette | Saved ID | Appearance | Style |
| --- | --- | --- | --- |
| Signal Paper | `contrast-light` | Light | Strong outlines, white surfaces, deep blue actions |
| Signal Night | `contrast-dark` | Dark | True black, yellow actions, crisp cyan details |
| Blueprint | `blueprint-light` | Light | Square technical controls, navy and teal |
| Blueprint Night | `blueprint-dark` | Dark | Technical navy, blue and turquoise highlights |
| Kiln | `copper-light` | Light | Ivory surfaces, warm clay and copper |
| Copper Foundry | `copper-dark` | Dark | Earthen surfaces, soft copper and pale teal |
| Glacier | `arctic-light` | Light | Ice-white, deep teal and indigo |
| Polar Night | `arctic-dark` | Dark | Deep teal, icy cyan and lavender |
| Citrus Grove | `citrus-light` | Light | Rounded olive controls, lime paper and orange |
| Citrus After Dark | `citrus-dark` | Dark | Rounded olive surfaces, bright citrus highlights |
| Editorial | `editorial-light` | Light | Square controls, warm paper and burgundy |
| Edition Night | `editorial-dark` | Dark | Warm charcoal, rose and calm blue details |
| Paper Ink | `paper-ink` | Light | Sharp grayscale controls and restrained spacing |
| Black Glass | `black-glass` | Dark | True-black canvas, rounded blue actions and mint |

All 14 additions provide at least 4.5:1 contrast for body and secondary text against both the canvas and surface colors, and for primary-button text. Signal Paper and Signal Night provide at least 7:1 for those pairs. These are palette checks; authored local colors, overlays and images still need their own accessibility review.

Each palette supplies 11 RGB colors plus corner-radius and spacing tokens. Existing local style overrides and authored content remain intact. QR and barcode colors remain independently controlled. Saved projects retain theme bindings; device exports resolve them into ordinary LVGL styles without the authoring metadata.

The catalog is defined in `LVSerial/Serialization/ProjectDesign.h`. Metadata, token ranges, saved IDs and contrast are checked by `Tests/Design/ProjectDesign.cpp`; native gallery tests cover real screen previews, confirmation, cancel and Undo.
