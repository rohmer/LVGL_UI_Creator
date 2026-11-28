# LVGL UI Creator 0.9.4

## New advanced widgets

The Advanced Widgets palette now includes five native LVGL controls inspired by
the [jQuery UI examples](https://jqueryui.com/):

- **Radio Group:** named options, exclusive selection, disabled options, and
  horizontal or vertical layout.
- **Checkbox:** a themed checkbox button with editable text and checked/disabled
  state.
- **Checkbox nested in Label:** a clickable label/card with a checkbox and
  optional wrapping description.
- **Date Picker:** a compact date display with an on-demand calendar, three
  display formats, date limits, all seven first-weekday options, leap-date
  validation, keyboard focus and popup dismissal.
- **Menu:** nested items with stable IDs, disabled branches, Back navigation,
  and keyboard navigation.

Each control has a matching palette/Hierarchy icon and editable inspector
properties. Property changes support undo/redo. F6 interactions preserve the
authored defaults, and controls remain a single authored object in the
Hierarchy. See [the advanced widget guide](https://github.com/rohmer/LVGL_UI_Creator/blob/0.9.4/UICreator/AdvancedWidgets.md).

## Exports, runtime and bindings

- New controls persist through project JSON, Protobuf and Creator XML.
- Selected options, selected menu items, checkbox values and ISO date strings
  support typed two-way variable bindings.
- Invalid dates and unavailable selections are rejected before changing bound
  variables, application storage or history.
- C exports use the same small native helpers as the loader and enable only
  controls present in the project. Native controls require the bundled LVGL 9.6
  target.
- MicroPython exports include equivalent interactive controls using the upstream
  LVGL binding. Calendar popups fit small displays and retain callback/map
  storage correctly across garbage collection.
- Device source profiles select only required LVGL primitives. Date Picker can
  compile its button matrix without enabling the built-in keyboard. Menu uses
  buttons, labels and flex layout without LVGL's deprecated menu API.
- New native helper code uses dedicated LVGL 9.6 flag setters and avoids the
  compiler warnings found during Windows/Linux validation.

## Packages

Application versions, installer versions, filenames and SDK metadata now use
**0.9.4** consistently. The release includes Windows x64/x86 installers and
portable archives, Linux x64/Raspberry Pi ARM64 DEB/TGZ packages, Intel/Apple
Silicon macOS archives, Windows Corpus Workbench/Keyboard Simulator companion
archives, and refreshed ESP32, Arduino, Pico/Pico 2, Linux and Windows SDKs.

The existing project-level Advanced Keyboard remains configured through
**Project → Properties → Object**. Shared keyboard code, tools and models remain
pinned to the AdvancedKeyboard dependency. Keyboard-only profiles retain their
minimal feature selection; the full source toolkits include an advanced-controls
example.

macOS archives are cross-built and ad-hoc signed. macOS execution/notarization,
physical board testing and a separate 32-bit Windows operating system are not
part of the packaging checks. ARM64 desktop execution checks use QEMU.

Every published package has a SHA256 sidecar. The release manifests and
`validation.json` record the source commit, artifact hashes and completed checks.
