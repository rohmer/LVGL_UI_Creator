"""Exercise exported advanced controls with the real upstream LVGL binding.

Run with the LVGL-enabled MicroPython interpreter and the `advanced-generated`
directory produced by MicroPythonExport/test_export.py. No source override,
physical display or board driver is used.
"""
import gc
import json
import sys
import lvgl as lv

exported = sys.argv[1].rstrip("/")
sys.path.insert(0, exported)
initialize = getattr(lv, "init", None)
if initialize:
    initialize()
display = lv.display_create(320, 240)
display.set_color_format(lv.COLOR_FORMAT.RGB565)
pixels = bytearray(320 * 8 * 2)
display.set_buffers(pixels, None, len(pixels), lv.DISPLAY_RENDER_MODE.PARTIAL)
flushes = [0]


def flush(display, area, pixels):
    flushes[0] += 1
    display.flush_ready()


display.set_flush_cb(flush)
previous = lv.screen_active()
from uicreator_lvgl import UI


def render():
    churn = [bytearray(128) for unused in range(16)]
    del churn
    gc.collect()
    lv.tick_inc(40)
    lv.timer_handler()
    lv.refr_now(display)


for lifetime in range(3):
    app = UI(exported + "/ui.json").load()
    radio, checkbox, nested, date, menu = [app.get_object(0, oid) for oid in (1, 2, 3, 4, 5)]
    assert app.get_advanced_value(radio) == 1
    app.get_advanced_part(radio, "Option0").send_event(lv.EVENT.CLICKED, None)
    assert app.get_advanced_value(radio) == 0
    assert app.get_advanced_part(radio, "Option0").has_state(lv.STATE.CHECKED)
    assert not app.get_advanced_part(radio, "Option1").has_state(lv.STATE.CHECKED)
    checkbox.send_event(lv.EVENT.CLICKED, None)
    assert app.get_advanced_value(checkbox) is False
    app.set_advanced_value(nested, True)
    assert app.get_advanced_part(nested, "Checkbox").has_state(lv.STATE.CHECKED)
    assert app.get_advanced_part(date, "Popup") is None
    assert app.get_advanced_part(date, "Label").get_text() == "29/02/2028"
    try:
        app.set_advanced_value(date, "2028-02-30")
        raise AssertionError("Invalid leap date accepted")
    except ValueError:
        pass
    app.open_date_picker(date)
    popup, matrix = app.get_advanced_part(date, "Popup"), app.get_advanced_part(date, "Days")
    assert not popup.has_flag(lv.obj.FLAG.HIDDEN)
    assert matrix.get_button_text(0) == "Mo"
    state = app._advanced._find(date)[1]
    index = state["dates"].index("2028-02-16")
    matrix.set_selected_button(index)
    matrix.send_event(lv.EVENT.VALUE_CHANGED, None)
    assert app.get_advanced_value(date) == "2028-02-16"
    assert popup.has_flag(lv.obj.FLAG.HIDDEN)
    app.open_date_picker(date)
    roots = len(app._resources)
    for month in range(12):
        app.get_advanced_part(date, "Next").send_event(lv.EVENT.CLICKED, None)
        render()
    assert len(app._resources) == roots
    popup.send_event(lv.EVENT.CLICKED, None)
    assert popup.has_flag(lv.obj.FLAG.HIDDEN)
    app.get_advanced_part(menu, "Item2").send_event(lv.EVENT.CLICKED, None)
    assert not app.get_advanced_part(menu, "Item3").has_flag(lv.obj.FLAG.HIDDEN)
    app.get_advanced_part(menu, "Item3").send_event(lv.EVENT.CLICKED, None)
    assert app.get_advanced_value(menu) == 3
    app.get_advanced_part(menu, "Back").send_event(lv.EVENT.CLICKED, None)
    assert app.get_advanced_part(menu, "Item3").has_flag(lv.obj.FLAG.HIDDEN)
    render()
    app.deinit()
    assert lv.screen_active() == previous
    render()

assert flushes[0] > 0
print(json.dumps({"status": "passed", "hardware": False, "displayFlushes": flushes[0],
    "cases": ["five exported advanced controls created and rendered in native LVGL",
              "exclusive radio, native checkbox and whole-label toggle",
              "lazy date popup, Monday-first leap calendar, selection and dismissal",
              "twelve month-map replacements survive GC without accumulating roots",
              "nested menu selection and Back navigation",
              "three complete UI lifetimes and popup cleanup"]}))
