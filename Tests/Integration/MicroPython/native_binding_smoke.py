"""Run with the real lvgl/lv_binding_micropython interpreter on Linux.

    micropython native_binding_smoke.py /absolute/path/to/generated-export

The generated-export argument is the CMake UICreatorMicroPythonExport fixture.
No physical display or SDL window is required: a bounded partial display buffer
is captured in memory. Exported code pixels are compared with their I1 palette
and payload, while native arrays and shared keyboard maps are exercised across
garbage collections. This imports the exported runtime, not a fake LVGL.
"""
import gc
import json
import sys
import lvgl as lv

export_root = sys.argv[1].rstrip("/")
repo_root = __file__.replace("\\", "/").rsplit("/Tests/", 1)[0]
sys.path.insert(0, export_root)
if len(sys.argv) > 2 and sys.argv[2] == "--source-runtime":
    sys.path.insert(0, repo_root + "/CodeGeneration/MicroPython")
initialize = getattr(lv, "init", None)
if initialize:
    initialize()
display = lv.display_create(320, 240)
display.set_color_format(lv.COLOR_FORMAT.RGB565)
draw_buffer = bytearray(320 * 8 * 2)
framebuffer = bytearray(320 * 240 * 2)
display.set_buffers(draw_buffer, None, len(draw_buffer), lv.DISPLAY_RENDER_MODE.PARTIAL)
flushes = [0]


def flush(disp, area, pixels):
    flushes[0] += 1
    width = area.x2 - area.x1 + 1
    rows = area.y2 - area.y1 + 1
    source = pixels.__dereference__(width * rows * 2)
    for row in range(rows):
        start = ((area.y1 + row) * 320 + area.x1) * 2
        framebuffer[start:start + width * 2] = source[row * width * 2:(row + 1) * width * 2]
    disp.flush_ready()


display.set_flush_cb(flush)
previous_screen = lv.screen_active()
results = []


def collect_and_render():
    gc.collect()
    lv.tick_inc(80)
    lv.timer_handler()
    lv.refr_now(display)


def cleanup(app):
    app.deinit()
    assert lv.screen_active() == previous_screen
    assert not app.screens and not app.objects
    collect_and_render()


import ui
app = ui.create()
assert len(app.screens) == 2 and app.get_variable("level") == 7
collect_and_render()
app.set_variable("level", 42)
assert app.get_object(0, 3).get_value() == 42
assert app.get_object(0, 2).get_text() == "42"
slider = app.get_object(0, 3)
slider.set_value(85, False)
slider.send_event(lv.EVENT.VALUE_CHANGED, None)
assert app.get_variable("level") == 85
assert app.get_object(0, 2).get_text() == "85"
collect_and_render()
app.show_screen(1)
cleanup(app)
results.append("generated assets, Unicode, image animation, variables, navigation and cleanup")

from uicreator_lvgl import UI


def stress_render():
    # Reuse and collect allocations so a stale native pointer cannot survive
    # merely because its old allocation has not been overwritten yet.
    churn = [bytearray(256) for _ in range(32)]
    del churn
    collect_and_render()


def verify_code_pixels(root, record, tiled=False, authored_colors=(0, 0xffffff)):
    image = record["img"]
    assert image["colorFormat"] == lv.COLOR_FORMAT.I1
    with open(root + "/assets/" + image["mpyAsset"], "rb") as source:
        data = source.read()
    width, height, stride = image["width"], image["height"], image["stride"]
    assert len(data) == 8 + stride * height
    palette = []
    rgb_colors = []
    for index in range(2):
        blue, green, red, alpha = data[index * 4:index * 4 + 4]
        assert alpha == 255
        rgb_colors.append((red << 16) | (green << 8) | blue)
        palette.append(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3))
    assert set(rgb_colors) == set(authored_colors), (rgb_colors, authored_colors)
    target_height = 24 if tiled else height
    target_width = width + 15 if tiled else width
    sample = {"oid": 1, "par": -1, "base": {"coords": {"x": 10, "y": 10,
        "width": target_width, "height": target_height}, "nativeStyles": [{"selector": 0, "values": {
            "pad_left": 0, "pad_right": 0, "pad_top": 0, "pad_bottom": 0,
            "border_width": 0, "outline_width": 0, "image_recolor_opa": 0}}]}, "img": image}
    app = UI({"canvas": {"width": 320, "height": 240}, "obj": [sample]}, root + "/assets").load()
    try:
        obj = app.get_object(0, 1)
        obj.update_layout()
        assert obj.get_width() == target_width and obj.get_height() == target_height
        for cycle in range(3):
            obj.invalidate()
            stress_render()
            for y in range(target_height):
                for x in range(target_width):
                    sx, sy = x % width, y % height
                    bit = (data[8 + sy * stride + sx // 8] >> (7 - sx % 8)) & 1
                    offset = ((10 + y) * 320 + 10 + x) * 2
                    actual = framebuffer[offset] | (framebuffer[offset + 1] << 8)
                    assert actual == palette[bit], (image["mpyAsset"], x, y, actual, palette[bit], tiled)
    finally:
        cleanup(app)


with open(export_root + "/ui.json") as source:
    exported = json.load(source)
for oid, kind in ((5, "QR"), (6, "barcode")):
    print("Checking exported", kind, "pixels")
    record = next(item for item in exported["screens"][0]["obj"] if item["oid"] == oid)
    colors = (0x2244aa, 0xffcc88) if kind == "QR" else (0, 0xffffff)
    verify_code_pixels(export_root, record, authored_colors=colors)
    results.append("exported " + kind + " authored palette, payload geometry and rendered pixels survive GC")
tiled_root = export_root.rsplit("/", 1)[0] + "/tiled-generated"
with open(tiled_root + "/ui.json") as source:
    tiled = json.load(source)
tiled_record = next(item for item in tiled["screens"][0]["obj"] if item["oid"] == 6)
assert tiled_record["img"]["height"] == 1 and tiled_record["img"]["innerAlign"] == 12
print("Checking exported tiled barcode pixels")
verify_code_pixels(tiled_root, tiled_record, True)
results.append("exported tiled barcode repeats its encoded pixels across both object dimensions")


with open(repo_root + "/Tests/Integration/CodeGeneration/Variables/project.json") as source:
    document = json.load(source)
app = UI(document).load()
assert app.get_variable("level") == 12
assert app.get_object(0, 1).get_value() == 12
assert app.get_object(0, 2).get_value() == 12
assert app.get_object(0, 3).get_text() == "12"
assert app.get_object(1, 1).get_value() == 12
app.set_variable("level", 40)
assert app.get_object(0, 4).get_value() == 7
from_widget = app.get_object(0, 4)
from_widget.set_value(55, False)
from_widget.send_event(lv.EVENT.VALUE_CHANGED, None)
assert app.get_variable("level") == 55
assert app.get_object(1, 2).get_value() == 55
app.set_variable("caption", "Espa\u00f1ol")
assert app.get_object(0, 6).get_text() == "Espa\u00f1ol"
assert app.get_object(1, 3).get_text() == "Espa\u00f1ol"
switch = app.get_object(0, 9)
switch.add_state(lv.STATE.CHECKED)
switch.send_event(lv.EVENT.VALUE_CHANGED, None)
assert app.get_variable("enabled") is True
app.get_object(1, 7).send_event(lv.EVENT.CLICKED, None)
assert app.get_variable("level") == 64
app.get_object(1, 8).send_event(lv.EVENT.CLICKED, None)
assert app.get_variable("level") == 65
assert app.get_history("level") == [55, 64, 65]
assert app.get_history_count("level") == 3
try:
    app.set_variable("small", 1000000000)
    raise AssertionError("out-of-range value accepted")
except ValueError:
    pass
collect_and_render()
app.show_screen(1)
cleanup(app)
results.append("variable history replay, fanout, two-way events, typed validation and actions")

samples = [
    ("text", "Unicode \u00e9", {"nativeStyles": [{"selector": 0, "values": {"text_font": "montserrat_16", "text_color": 0x123456}}]}),
    ("button", {"text": "Action", "toggle": True}, {}),
    ("cb", {"text": "Check", "checked": True}, {}),
    ("arc", {"min": 0, "max": 100, "value": 50}, {}),
    ("bar", {"min": 0, "max": 100, "value": 50}, {}),
    ("btnm", {"btnMap": ["One", "Two", ""], "nativeCtrl": [1, 1], "selected": 1}, {}),
    ("cal", {"today": {"year": 2026, "month": 11, "day": 22}, "shown": {"year": 2026, "month": 11},
             "hlDates": [{"year": 2026, "month": 11, "day": 23}], "dayN": ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]}, {}),
    ("canvas", {"width": 20, "height": 10, "format": lv.COLOR_FORMAT.ARGB8888, "fillColor": 0x00ff00}, {}),
    ("img", {"sym": "Test"}, {}),
    ("dropdown", {"options": "One\nTwo", "selected": 1}, {}),
    ("roller", {"options": "One\nTwo", "selected": 1}, {}),
    ("slider", {"min": 0, "max": 100, "value": 50}, {}),
    ("spinbox", {"min": 0, "max": 100, "value": 12}, {}),
    ("switch", {"checked": True}, {}),
    ("textarea", {"text": "Type here", "oneLine": True}, {}),
    ("led", {"color": 0x112233, "brightness": 90}, {}),
    ("line", {"points": [[0, 0], [20, 30]], "invertY": False}, {}),
    ("keyboard", {"buttons": {"btnMap": ["a", "b", ""], "nativeCtrl": [1, 1]}}, {}),
    ("table", {"columns": [90, 90], "rows": [[{"text": "A"}, {"text": "B"}]], "selectedRow": 0, "selectedColumn": 1}, {}),
    ("chart", {"pointCount": 3, "axes": [{"axis": 0, "min": 0, "max": 100}],
               "series": [{"color": 0xff0000, "y": [10, 20, 30]}]}, {}),
    ("tabview", {"tabs": [{"name": "First"}, {"name": "Second"}], "active": 1}, {}),
    ("tileview", {"tiles": [{"column": 0, "row": 0}], "active": 0}, {}),
    ("window", {"title": "Window", "headerHeight": 32}, {}),
    ("messagebox", {"title": "Message", "text": "Body"}, {}),
    ("imagebutton", {}, {}),
    ("scale", {"mode": 0, "rangeMin": 0, "rangeMax": 100}, {}),
    ("qrcode", {"data": "Espa\u00f1ol", "size": 80, "quietZone": False}, {}),
    ("barcode", {"data": "123456", "scale": 1}, {"coords": {"width": 280, "height": 60}}),
    ("page", {}, {"nativeStyles": [{"selector": 0, "values": {"grid_column_dsc_array": [40, 40], "grid_row_dsc_array": [30]}}]}),
]
for kind, data, base in samples:
    print("Checking native widget", kind)
    record = {"oid": 1, "par": -1, "base": base, kind: data}
    app = UI({"canvas": {"width": 320, "height": 240}, "obj": [record]}).load()
    collect_and_render()
    cleanup(app)
    results.append("native widget " + kind)

grid_style = {"layout": lv.LAYOUT.GRID, "grid_column_dsc_array": [40, 60],
    "grid_row_dsc_array": [30, 45], "pad_left": 0, "pad_top": 0,
    "pad_right": 0, "pad_bottom": 0, "border_width": 0, "pad_column": 5, "pad_row": 7}
records = [{"oid": 1, "par": -1, "base": {"coords": {"width": 220, "height": 120},
    "nativeStyles": [{"selector": 0, "values": grid_style}]}}]
for oid, column, row, span in ((2, 0, 0, 1), (3, 1, 0, 1), (4, 0, 1, 2)):
    records.append({"oid": oid, "par": 1, "base": {"nativeStyles": [{"selector": 0, "values": {
        "grid_cell_column_pos": column, "grid_cell_column_span": span,
        "grid_cell_row_pos": row, "grid_cell_row_span": 1,
        "grid_cell_x_align": 3, "grid_cell_y_align": 3}}]}})
app = UI({"canvas": {"width": 320, "height": 240}, "obj": records}).load()
grid, first, second, wide = [app.get_object(0, oid) for oid in (1, 2, 3, 4)]
for cycle in range(8):
    stress_render()
    grid.set_width(220 + cycle % 2)  # Forces another native layout read.
    grid.update_layout()
    assert first.get_width() == 40 and first.get_height() == 30
    assert second.get_width() == 60 and second.get_x() - first.get_x() == 45
    assert wide.get_width() == 105 and wide.get_height() == 45
    assert wide.get_y() - first.get_y() == 37
cleanup(app)
results.append("native grid track buffers preserve child geometry across GC and repeated relayout")

app = UI({"canvas": {"width": 320, "height": 240}, "obj": [
    {"oid": 1, "par": -1, "base": {}, "line": {"points": [[2, 3], [20, 30], [42, 19]]}},
    {"oid": 2, "par": -1, "base": {}, "cal": {"today": {"year": 2026, "month": 11, "day": 22},
     "shown": {"year": 2026, "month": 11}, "dayN": ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"],
     "hlDates": [{"year": 2026, "month": 11, "day": 23}, {"year": 2026, "month": 11, "day": 28}]}},
]}).load()
line, calendar = app.get_object(0, 1), app.get_object(0, 2)
for cycle in range(8):
    stress_render()
    assert line.get_point_count() == 3
    points = line.get_points()
    assert [(points[index].x, points[index].y) for index in range(3)] == [(2, 3), (20, 30), (42, 19)]
    del points  # Getter wrappers must not rescue otherwise unrooted storage.
    assert calendar.get_highlighted_dates_num() == 2
    dates = calendar.get_highlighted_dates()
    assert [(dates[index].year, dates[index].month, dates[index].day) for index in range(2)] == [
        (2026, 11, 23), (2026, 11, 28)]
    del dates
    assert calendar.get_btnmatrix().get_button_text(0) == "Su"
cleanup(app)
results.append("native line points, calendar dates and day-name buffers preserve values after GC")


def keyboard_ui(label=None):
    settings = {} if label is None else {"buttons": {"btnMap": [label, "", ], "nativeCtrl": [1]}}
    return UI({"canvas": {"width": 320, "height": 240}, "obj": [
        {"oid": 1, "par": -1, "base": {}, "keyboard": settings},
    ]}).load()


owner = keyboard_ui("Owner map")
lv.screen_load(previous_screen)
borrower = keyboard_ui()
assert borrower.get_object(0, 1).get_button_text(0) == "Owner map"
lv.screen_load(previous_screen)
replacement = keyboard_ui("Replacement map")
owner.deinit()
for cycle in range(8):
    stress_render()
    assert replacement.get_object(0, 1).get_button_text(0) == "Replacement map"
    assert borrower.get_object(0, 1).get_button_text(0) == "Owner map"
cleanup(replacement)
borrower.show_screen(0)
stress_render()
assert borrower.get_object(0, 1).get_button_text(0) == "Owner map"
cleanup(borrower)
results.append("live default keyboards retain borrowed maps after replacement and original-owner deletion")

print("Checking default keyboard after deleting custom-map keyboard and collecting garbage")
app = UI({"canvas": {"width": 320, "height": 240}, "obj": [
    {"oid": 1, "par": -1, "base": {}, "textarea": {"text": "First"}},
    {"oid": 2, "par": -1, "base": {}, "textarea": {"text": "Second"}},
    {"oid": 3, "par": -1, "base": {"hidden": True}, "keyboard": {}},
]}).load()
print("Keyboard target fixture loaded")
first, second, keyboard = [app.get_object(0, oid) for oid in (1, 2, 3)]
first.send_event(lv.EVENT.FOCUSED, None)
print("First keyboard target focused")
assert keyboard.get_textarea() == first and not keyboard.has_flag(lv.obj.FLAG.HIDDEN)
first.send_event(lv.EVENT.DEFOCUSED, None)
assert not keyboard.has_flag(lv.obj.FLAG.HIDDEN)
second.send_event(lv.EVENT.FOCUSED, None)
print("Second keyboard target focused")
assert keyboard.get_textarea() == second
collect_and_render()
print("Keyboard target fixture survived garbage collection and rendering")
keyboard.send_event(lv.EVENT.READY, None)
assert keyboard.has_flag(lv.obj.FLAG.HIDDEN)
first.send_event(lv.EVENT.FOCUSED, None)
keyboard.send_event(lv.EVENT.CANCEL, None)
assert keyboard.has_flag(lv.obj.FLAG.HIDDEN)
cleanup(app)
for iteration in range(5):
    app = UI({"canvas": {"width": 320, "height": 240}, "obj": [
        {"oid": 1, "par": -1, "base": {}, "keyboard": {}},
    ]}).load()
    collect_and_render()
    cleanup(app)
results.append("keyboard target switching, persistent visibility and ready/cancel closing")
results.append("keyboard custom maps remain valid across six later UI lifecycles and garbage collections")
for mode in (-1, 4, 9, True, "0"):
    try:
        UI({"canvas": {"width": 320, "height": 240}, "obj": [
            {"oid": 1, "par": -1, "base": {}, "keyboard": {"mode": mode}},
        ]}).load()
        raise AssertionError("invalid or unassigned keyboard mode accepted: " + str(mode))
    except ValueError:
        pass
if getattr(lv.keyboard.MODE, "TEXT_ARABIC", None) != 8:
    try:
        UI({"canvas": {"width": 320, "height": 240}, "obj": [
            {"oid": 1, "par": -1, "base": {}, "keyboard": {"mode": 8}},
        ]}).load()
        raise AssertionError("unavailable Arabic keyboard mode accepted")
    except ValueError:
        pass
app = UI({"canvas": {"width": 320, "height": 240}, "obj": [
    {"oid": 1, "par": -1, "base": {}, "keyboard": {"mode": 4,
     "buttons": {"btnMap": ["x", "y", ""], "nativeCtrl": [1, 1]}}},
]}).load()
keyboard = app.get_object(0, 1)
assert keyboard.get_mode() == 4 and keyboard.get_button_text(0) == "x"
collect_and_render()
cleanup(app)
results.append("keyboard modes reject unsafe values and install custom USER maps before selecting them")
app = UI({"canvas": {"width": 320, "height": 240}, "obj": [
    {"oid": 1, "par": -1, "base": {}, "textarea": {"inputMode": "uppercase", "text": "café привет αβγ ß"}},
    {"oid": 2, "par": -1, "base": {}, "textarea": {"inputMode": "lowercase", "text": "CAFÉ ПРИВЕТ ΑΒΓ"}},
    {"oid": 3, "par": -1, "base": {}, "textarea": {"inputMode": "numeric", "text": "-12a.3.4e5"}},
    {"oid": 4, "par": -1, "base": {}, "textarea": {"inputMode": "uppercase", "accepted": "AB12", "maxLength": 3, "text": "a1b2c3"}},
    {"oid": 5, "par": -1, "base": {}, "keyboard": {}},
]}).load()
upper, lower, numeric, limited, keyboard = [app.get_object(0, oid) for oid in range(1, 6)]
assert upper.get_text() == "CAFÉ ПРИВЕТ ΑΒΓ SS"
assert lower.get_text() == "café привет αβγ"
assert numeric.get_text() == "12.345"
assert limited.get_text() == "A1B"
upper.set_text("éжω")
upper.add_text("ß!")
assert upper.get_text() == "ÉЖΩSS!"
numeric.set_text("-1.2e3.4 hello")
numeric.add_text(".5")
assert numeric.get_text() == "1.2345"
numeric.set_cursor_pos(0)
numeric.add_text("-6x.")
assert numeric.get_text() == "61.2345"
for cycle in range(5):
    upper.send_event(lv.EVENT.FOCUSED, None)
    assert keyboard.get_button_text(10) == "Q"
    lower.send_event(lv.EVENT.FOCUSED, None)
    assert keyboard.get_button_text(10) == "q"
    numeric.send_event(lv.EVENT.FOCUSED, None)
    assert keyboard.get_mode() == 7 and keyboard.get_button_text(0) == "1"
    stress_render()
keyboard.send_event(lv.EVENT.CANCEL, None)
assert keyboard.has_flag(lv.obj.FLAG.HIDDEN)
cleanup(app)
results.append("Unicode input modes, decimal-only pastes, accepted/max-length filters and target-specific keyboard maps")
assert flushes[0] > 0
print(json.dumps({"status": "passed", "cases": results, "displayFlushes": flushes[0], "hardware": False}))
