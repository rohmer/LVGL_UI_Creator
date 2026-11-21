# Professional editor workflows

The Screens dock and **View > Screens and navigation** manage screen names,
duplication, order, start screen, transition animations and click navigation.
Browsing screens is a view change; it leaves the saved project clean. Undo
restores screen edits, references and the screen being edited. The Run command
previews the current screen; **Debug > Run application from start** follows the
project start screen. Runtime previews load screens as needed.

**Debug > Performance dashboard** shows measured frame, render, flush, load,
input and prediction distributions. Enable profiling before collecting a run.
Named device budgets are targets, not device measurements: desktop measurements
remain labeled as host values. Import a device measurement report and firmware
file to compare actual hardware memory/timing and compiled size. See
`../Runtime/PERFORMANCE.md` for the measurement format.

**View > Device previews** compares isolated renders at selected device sizes
and orientations. It does not rewrite authored coordinates. **Edit > Layout
designer** edits native Flex/Grid settings. Themes preview the actual document;
Apply commits a change and Cancel restores the starting theme. Linked components
have searchable thumbnails and show affected instances before updating a master.

**View > Problems** opens the persistent panel in the Tools dock. Analysis is
debounced against document revisions. Findings include references, geometry,
known solid-background contrast, focus order and translation fit. Select a
finding to reveal its screen and object; offered fixes participate in undo.
Set `base.accessibility.tabIndex` to a nonnegative integer for explicit focus
order or -1 to exclude a control and its internal focus targets.

Use **Ctrl+S** to save to the known JSON path and **Ctrl+Shift+S** to choose a
new path. **Ctrl+Shift+P** searches available commands; **Ctrl+Shift+F** finds
properties of the current selection and focuses the matching inspector field.
Idle document reads use a revision cache. Undo stores byte patches so repeated
small edits do not duplicate large embedded image or trie strings.

The project creator can pass a generated project directly to the device tools.
Named target presets keep configure, build, run, flash and monitor settings with
the document. Builds show the current tool phase and logs. Raspberry Pi/Linux
commands can use WSL from Windows. Flashing and monitoring remain explicit user
actions.

Automated native tests cover these workflows. `Tests/Visual` provides explicit
visual-baseline updates, exact pixel comparison and measured input-replay
performance budgets. Run fixtures with the same LVGL build, fonts and renderer;
desktop timing thresholds must be calibrated for the test machine.
