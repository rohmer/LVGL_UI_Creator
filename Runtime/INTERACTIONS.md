# Authored interactions

Use **Events and actions** in the editor to give the selected widget behavior. Select a saved action, choose **Clicked**, **Value changed**, or **Ready / Enter**, fill in the relevant fields, and choose **Save action**. Actions run in their displayed order. Use **Run screen** to interact with an isolated copy of the design; returning to Design discards runtime values and visibility changes.

Supported actions:

| Action | Fields |
| --- | --- |
| Navigate to screen | Screen number, starting at 1 in the editor |
| Set variable | Project variable and a value of its declared type |
| Increment variable | Integer or floating point variable and numeric increment |
| Show, hide, or toggle widget | Target widget; **This widget** targets the event source |
| Validate text box | Text area or Text Box target, required flag, optional numeric minimum and maximum |

A condition can restrict any action to a variable comparison: equals, does not equal, greater than, less than, or true/nonempty. Values accept JSON literals (`3`, `true`, `"hello"`) and plain text. Define project variables before using variable actions or conditions. An invalid required/numeric value highlights the text box with a red border, scrolls it into view, and stops the remaining actions for that event. Validation reserves `LV_STATE_USER_4` on its target. Optional empty fields pass validation; nonempty bounded fields must contain a finite number.

All runtime actions run on the LVGL thread. No script interpreter is involved. Variable actions use the existing typed setters, bindings, and history. Integer increments reject overflow. Reentrant action events caused by a variable setter are suppressed. Deleted object handles become null; actions report an unavailable target instead of accessing freed objects.

## Portable project records

Bindings are stored in the widget's `base.events` array and survive JSON, Protobuf, undo, and component insertion. Object references use the screen's saved `oid`, independently of editor tree node IDs. The editor tracks live references when serializing reordered objects. A deleted target is retained as `targetOid: -2` so validation can identify the broken reference. `targetOid: -1` means the event source.

```json
{
  "oid": 7,
  "base": {
    "events": [
      {"event": "clicked", "action": "validate", "targetOid": 9,
       "required": true, "min": 1, "max": 100},
      {"event": "clicked", "action": "increment_variable",
       "variable": "count", "value": 1,
       "condition": {"variable": "enabled", "op": "truthy", "value": true}},
      {"event": "clicked", "action": "hide", "targetOid": 10}
    ]
  },
  "button": {}
}
```

Events are `clicked`, `value_changed`, and `ready`. Actions are `navigate` (`screen`, zero based), `set_variable` and `increment_variable` (`variable`, `value`), `show`, `hide`, `toggle`, and `validate` (`targetOid`, optional `targetScreen`). Object targets normally belong to the event source's screen. Conditions use `variable`, `op` (`eq`, `ne`, `gt`, `lt`, `truthy`), and `value`.

## Runtime and C export

The Protobuf runtime installs authored callbacks after all screens and project variables are loaded. The ordinary authoring loader preserves their metadata without installing them. Windows, Linux/Raspberry Pi, ESP32, and Arduino runtimes share the implementation in `LVSerial/Serialization/InteractionActions.cpp`. It uses core LVGL events, flags, states, and styles. Text validation accesses LVGL Text Area only when that widget is enabled; it does not enable additional widget families in a minimal build. Source packaging includes the new file through the serialization source glob.

C export emits ordinary C callbacks, literal object handles, and calls to generated variable setters. The generated `ui_last_action_error()` returns the latest action failure, or an empty string. Generated callbacks check tracked object handles and guard against reentrant events. They use no JSON, Protobuf, C++, filesystem, or scripting engine on the device. The target compatibility layer supports the same actions on all supported LVGL targets.

Use **Validate screens** before export to review missing assets and bindings, dangling action/keyboard targets, invalid action parameters, clipping, small touch targets, overlapping sibling controls, and missing glyphs in the active screen's resolved fonts. Geometry and touch findings are warnings; structural and broken-reference findings are errors. Scrollable content and intentional parent/child overlap are excluded. Selecting a finding selects the affected authored widget on the current screen.
