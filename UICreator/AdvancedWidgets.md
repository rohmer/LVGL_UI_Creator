# Advanced form controls

The **Advanced Widgets** add-object palette includes five native LVGL controls inspired
by the [jQuery UI checkbox/radio examples](https://jqueryui.com/checkboxradio/),
[Datepicker](https://jqueryui.com/datepicker/) and
[Menu](https://jqueryui.com/menu/). They run locally without a browser or jQuery.
Select a control in the Hierarchy to edit its settings in **Properties → Object**.

| Control | Settings and behavior |
| --- | --- |
| Radio Group | Add/remove named options, disable individual options, select one option, and choose horizontal or vertical layout. Selecting an enabled option clears the previous selection. |
| Checkbox | A themed checkbox button with editable text, checked state and disabled state. |
| Checkbox nested in Label | A checkbox inside a clickable label/card, with optional wrapping description text. Clicking the label toggles its checkbox. |
| Date Picker | A compact date display that opens a calendar when clicked. Configure the selected date, display format, minimum/maximum dates and first weekday. Choose a day to update the value and dismiss the calendar. |
| Menu | A hierarchical list with named items, disabled items and Back navigation. Each item has an ID; `parent = 0` places it at the root, and another item's ID places it in that item's submenu. Selecting a leaf updates the selected ID. |

Date values and bounds are stored as `yyyy-mm-dd`. The display can use
`yyyy-mm-dd`, `dd/mm/yyyy` or `mm/dd/yyyy` independently of the stored value.
An empty bound leaves that end of the range unrestricted. Invalid dates and
inverted ranges are rejected while preserving the previous valid settings.

The composite controls stay together as a single object in the Hierarchy.
Their internal labels, indicators, calendar and menu rows are generated parts.
Use F6 to interact with the controls; Design mode edits the control itself.
Preview selections do not change saved defaults. Property changes support
undo/redo, and the palette and Hierarchy use matching vector icons.

Expand **Options** or **Items** in Properties to edit individual entries. The
radio selection, menu selection, menu parents, date format and first weekday
use dropdowns. Menu parent choices exclude descendants to prevent cycles;
removing a branch also removes its descendants. The basic **Check box** remains
available separately from the advanced **Checkbox** button.

Settings and live selections use the same typed records in project JSON,
Protobuf and Creator XML. C exports include the native helpers and selected
configuration; device profiles compile only the required LVGL primitives.
Only a Date Picker requires the calendar implementation. Menu uses buttons,
labels and flex layout rather than LVGL's deprecated menu widget.

Native C exports and Protobuf/XML runtimes require the bundled **LVGL 9.6**
target. MicroPython exports provide equivalent compositions through the upstream
LVGL binding, including a calendar popup sized to the display.
