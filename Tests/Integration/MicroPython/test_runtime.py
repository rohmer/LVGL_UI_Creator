"""Behavior checks using a deliberately bounded LVGL 9.3 binding surface.

This is a host test double, not proof that board firmware has the required
configuration. The separate upstream binding smoke test checks real APIs.
"""
import importlib.util
import copy
import gc
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
import weakref

ROOT = Path(__file__).resolve().parents[3]


class Event:
    def __init__(self, target, code, current=None):
        self.target, self.code, self.current = target, code, current or target
    def get_code(self): return self.code
    def get_target(self): return self.target
    def get_current_target(self): return self.current
    def get_target_obj(self): return self.target
    def get_current_target_obj(self): return self.current


class Obj:
    created = []
    FLAG = types.SimpleNamespace(HIDDEN=1, CLICKABLE=2, CHECKABLE=4, SCROLLABLE=8,
        EVENT_BUBBLE=16, FLOATING=32, SCROLL_ELASTIC=64, SCROLL_MOMENTUM=128,
        SCROLL_CHAIN_HOR=256, SCROLL_CHAIN_VER=512, CLICK_FOCUSABLE=1024,
        IGNORE_LAYOUT=2048, SCROLL_ON_FOCUS=4096)
    def __init__(self, parent):
        Obj.created.append(self)
        self.parent, self.children = parent, []
        if parent is not None: parent.children.append(self)
        self.flags, self.state, self.callbacks, self.styles = 0, 0, [], {}
        self.deleted = False
        self.x = self.y = 0
        self.width, self.height, self.scroll_calls = 100, 50, 0
    def add_event_cb(self, callback, event, user_data):
        assert user_data is None
        self.callbacks.append((callback, event))
    def emit(self, code, target=None):
        for callback, registered in list(self.callbacks):
            if registered in (code, 0): callback(Event(target or self, code, self))
    def add_flag(self, flags): self.flags |= flags
    def remove_flag(self, flags): self.flags &= ~flags
    def has_flag(self, flags): return bool(self.flags & flags)
    def add_state(self, state): self.state |= state
    def remove_state(self, state): self.state &= ~state
    def has_state(self, state): return bool(self.state & state)
    def set_x(self, value): self.x = value
    def set_y(self, value): self.y = value
    def set_width(self, value): self.width = value
    def set_height(self, value): self.height = value
    def set_size(self, width, height): self.width, self.height = width, height
    def get_child(self, index): return self.children[index] if -len(self.children) <= index < len(self.children) else None
    def get_parent(self): return self.parent
    def delete(self):
        self.deleted = True
        for child in list(self.children): child.delete()
        self.callbacks.clear()
        if self.parent: self.parent.children = [c for c in self.parent.children if c is not self]
    def clean(self):
        for child in list(self.children): child.delete()
    def scroll_to_view_recursive(self, anim): self.scroll_calls += 1
    def scroll_to(self, x, y, anim): self.scroll = x, y
    def move_to_index(self, index):
        self.foreground = index == -1
        if self.parent is not None:
            self.parent.children.remove(self)
            if index < 0: index += len(self.parent.children) + 1
            self.parent.children.insert(index, self)
    def set_scroll_dir(self, value): self.scroll_dir = value
    def set_scrollbar_mode(self, value): self.scrollbar_mode = value
    def set_scroll_snap_x(self, value): self.snap_x = value
    def set_scroll_snap_y(self, value): self.snap_y = value
    def set_flex_flow(self, value): self.flex_flow = value
    def set_flex_align(self, main, cross, track): self.flex_align = main, cross, track
    def center(self): self.centered = True


def style_setter(name):
    def set_style(self, value, selector): self.styles[name, selector] = value
    return set_style


for _name in ("bg_color bg_grad_color bg_grad_dir bg_main_stop bg_grad_stop bg_main_opa bg_grad_opa "
              "radius border_width border_color border_opa pad_left pad_right pad_top pad_bottom pad_row pad_column "
              "text_color text_opa text_letter_space text_line_space text_align text_font bg_image_src arc_image_src "
              "shadow_color shadow_width opa bg_opa width height line_width line_color grid_column_dsc_array grid_row_dsc_array").split():
    setattr(Obj, "set_style_" + _name, style_setter(_name))


class Label(Obj):
    def set_text(self, value): self.text = value
    def get_text(self): return self.text
    def set_long_mode(self, mode): self.long_mode = mode


class Textarea(Label):
    def set_placeholder_text(self, value): self.placeholder = value
    def set_accepted_chars(self, value): self.accepted = value
    def set_max_length(self, value): self.max_length = value
    def set_one_line(self, value): self.one_line = value
    def set_password_mode(self, value): self.password = value
    def set_password_bullet(self, value): self.bullet = value
    def set_password_show_time(self, value): self.password_show_time = value
    def set_cursor_pos(self, value): self.cursor = value
    def set_cursor_click_pos(self, value): self.cursor_click = value
    def set_text_selection(self, value): self.selection = value


class Slider(Obj):
    def set_range(self, minimum, maximum): self.minimum, self.maximum = minimum, maximum
    def set_mode(self, mode): self.mode = mode
    def set_orientation(self, value): self.orientation = value
    def set_value(self, value, anim): self.value = min(self.maximum, max(self.minimum, value))
    def get_value(self): return self.value
    def set_left_value(self, value, anim): self.left = value


class Bar(Slider):
    def set_start_value(self, value, anim): self.start = value


class Spinbox(Obj):
    def set_range(self, minimum, maximum): self.minimum, self.maximum = minimum, maximum
    def set_digit_format(self, digits, decimal): self.digits, self.decimal = digits, decimal
    def set_step(self, value): self.step = value
    def set_value(self, value): self.value = min(self.maximum, max(self.minimum, value))
    def get_value(self): return self.value
    def set_rollover(self, value): self.rollover = value
    def set_digit_step_direction(self, value): self.direction = value


class Dropdown(Obj):
    def set_options(self, value): self.options = value
    def set_selected(self, value): self.selected = value
    def get_selected(self): return self.selected
    def set_dir(self, value): self.direction = value
    def set_text(self, value): self.text = value
    def set_symbol(self, value): self.symbol = value
    def set_selected_highlight(self, value): self.highlight = value


class Roller(Dropdown):
    def set_options(self, value, mode): self.options, self.mode = value, mode
    def set_selected(self, value, anim): self.selected = value
    def set_visible_row_count(self, value): self.rows = value


class Keyboard(Obj):
    MODE = types.SimpleNamespace(TEXT_LOWER=0, TEXT_UPPER=1, SPECIAL=2, NUMBER=3,
                                 USER_1=4, USER_2=5, USER_3=6, USER_4=7)
    def __init__(self, parent):
        super().__init__(parent)
        self.mode = 0
        self._layout = self.maps.get(0)
        self._borrowed_layout()
        self.native_calls.append(("create", 0))
    def _borrowed_layout(self):
        if self._layout is not None:
            labels, controls = (pointer() for pointer in self._layout)
            if labels is None or controls is None:
                raise RuntimeError("Native keyboard map outlived its backing storage")
            return labels, controls
        return None
    def set_mode(self, value):
        self.native_calls.append(("mode", value))
        if 4 <= value <= 7 and value not in self.maps:
            raise RuntimeError("Native USER keyboard mode has no map")
        self.mode = value
        self._layout = self.maps.get(value)
        self._borrowed_layout()
    def set_popovers(self, value): self.popovers = value
    def set_textarea(self, target): self.target = target
    def set_map(self, mode, labels, controls):
        self.native_calls.append(("map", mode))
        # The real LVGL keyboard stores borrowed map pointers globally. Weak
        # references make premature collection observable in this host double.
        self.maps[mode] = weakref.ref(labels), weakref.ref(controls)
        if self.mode == mode: self._layout = self.maps[mode]
    def get_button_text(self, index):
        layout = self._borrowed_layout()
        return layout[0][index].str_val if layout is not None else "stock"
    def get_mode(self): return self.mode
    def set_button_ctrl(self, index, flags): pass
    def set_one_checked(self, enabled): self.one_checked = enabled
    def set_selected_button(self, index): self.selected = index


class Image(Obj):
    def set_src(self, value): self.source = value
    def set_offset_x(self, value): self.off_x = value
    def set_offset_y(self, value): self.off_y = value
    def set_rotation(self, value): self.rotation = value
    def set_scale_x(self, value): self.scale_x = value
    def set_scale_y(self, value): self.scale_y = value
    def set_antialias(self, value): self.antialias = value
    def set_blend_mode(self, value): self.blend = value
    def set_inner_align(self, value): self.align = value
    def set_pivot(self, x, y): self.pivot = x, y


class QRCode(Obj):
    def set_size(self, size): self.size = size
    def set_dark_color(self, color): self.dark = color
    def set_light_color(self, color): self.light = color
    def update(self, data, byte_count):
        assert isinstance(data, bytes) and len(data) == byte_count
        self.payload = data


class Timer:
    created = []
    def __init__(self, callback, period, user_data):
        Timer.created.append(self)
        self.callback, self.period, self.deleted = callback, period, False
    def delete(self): self.deleted = True
    def set_period(self, period): self.period = period


def binding():
    module = types.ModuleType("lvgl")
    module.obj, module.label, module.button, module.checkbox = Obj, Label, Obj, Label
    module.textarea, module.slider, module.bar, module.spinbox = Textarea, Slider, Bar, Spinbox
    module.switch, module.dropdown, module.roller = Obj, Dropdown, Roller
    module.keyboard = type("BoundKeyboard", (Keyboard,), {"maps": {}, "native_calls": [],
        "MODE": types.SimpleNamespace(**vars(Keyboard.MODE))})
    class CPointer:
        def __init__(self, count): self.entries = [types.SimpleNamespace(str_val=None) for _ in range(count)]
        def __getitem__(self, index): return self.entries[index]
        @classmethod
        def __cast__(cls, buffer):
            pointer = cls(0)
            pointer.buffer = buffer
            return pointer
    module.C_Pointer = CPointer
    module.image, module.qrcode = Image, QRCode
    module.STATE = types.SimpleNamespace(CHECKED=1, DISABLED=2, PRESSED=4, USER_1=8, USER_2=16, USER_3=32, USER_4=64)
    module.EVENT = types.SimpleNamespace(ALL=0, CLICKED=1, VALUE_CHANGED=2, READY=3, CANCEL=4, FOCUSED=5)
    module.PART = types.SimpleNamespace(MAIN=0, ITEMS=0x50000, INDICATOR=0x20000, CURSOR=0x10000)
    module.DIR = types.SimpleNamespace(TOP=1, ALL=15, VER=12)
    module.SCROLLBAR_MODE = types.SimpleNamespace(AUTO=1)
    module.COLOR_FORMAT = types.SimpleNamespace(ARGB8888=16)
    module.font_montserrat_14 = object()
    module.font_get_default = lambda: module.font_montserrat_14
    module.color_hex = lambda value: value & 0xffffff
    module.SIZE_CONTENT, module.GRID_CONTENT, module.GRID_TEMPLATE_LAST = 1073741823, 536870810, 2147483647
    module.pct, module.grid_fr = lambda n: ("percent", n), lambda n: ("fraction", n)
    module.image_dsc_t = lambda descriptor: descriptor
    module.timer_create = Timer
    module.current = Obj(None)
    module.screen_active = lambda: module.current
    module.display_get_default = lambda: module.current
    module.screen_loads, module.animations, module.pending_animation = [], [], None
    module.SCR_LOAD_ANIM = types.SimpleNamespace(FADE_IN=1, MOVE_LEFT=2, MOVE_RIGHT=3, MOVE_TOP=4, MOVE_BOTTOM=5)
    def screen_load(screen):
        if screen.deleted: raise RuntimeError("Cannot load a deleted screen")
        module.screen_loads.append(screen)
        # LVGL 9.3 leaves a pending animated load untouched when asked to load
        # the already active screen. Cleanup must interrupt it explicitly.
        if module.current is not screen:
            module.current = screen
            module.pending_animation = None
    def screen_load_anim(screen, kind, duration, delay, auto_delete):
        call = screen, kind, duration, delay, auto_delete
        module.animations.append(call)
        module.pending_animation = call
    module.screen_load = screen_load
    module.screen_load_anim = screen_load_anim
    return module


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        Obj.created.clear()
        Timer.created.clear()
        self.lv = binding()
        sys.modules["lvgl"] = self.lv
        spec = importlib.util.spec_from_file_location("runtime_under_test", ROOT / "CodeGeneration/MicroPython/uicreator_lvgl.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.UI = module.UI
        self.initial = self.lv.current
    def tearDown(self): sys.modules.pop("lvgl", None)

    def test_multiscreen_two_way_fanout_directions_and_typed_values(self):
        source = ROOT / "Tests/Integration/CodeGeneration/Variables/project.json"
        document = json.loads(source.read_text())
        ui = self.UI(document).load()
        self.assertEqual(ui.get_object(0, 1).value, 12)
        self.assertEqual(ui.get_object(1, 1).value, 12)
        self.assertEqual(ui.get_history("level"), [11, 12])
        self.assertEqual(ui.get_object(0, 4).value, 7)  # from-widget keeps its authored value
        self.assertEqual(ui.get_object(1, 5).value, 21)  # legacy single binding
        ui.set_variable("level", 45)
        self.assertEqual(ui.get_history("level"), [11, 12, 45])
        self.assertEqual(ui.get_object(0, 3).text, "45")
        self.assertEqual(ui.get_object(1, 2).value, 45)
        slider = ui.get_object(0, 1)
        slider.value = 72
        slider.emit(self.lv.EVENT.VALUE_CHANGED)
        self.assertEqual(ui.get_variable("level"), 72)
        self.assertEqual(ui.get_history("level"), [12, 45, 72])
        self.assertEqual(ui.get_object(1, 1).value, 72)
        ui.get_object(0, 4).value = 31
        ui.get_object(0, 4).emit(self.lv.EVENT.VALUE_CHANGED)
        self.assertEqual(ui.get_variable("level"), 31)
        ui.set_variable("enabled", True)
        entry = ui.get_object(0, 6)
        entry.text = "Typed"
        entry.emit(self.lv.EVENT.VALUE_CHANGED)
        self.assertEqual(ui.get_variable("caption"), "Authored nested")
        self.assertEqual(ui.get_object(1, 3).text, "Authored nested")
        with self.assertRaises(ValueError): ui.set_variable("small", 256)
        self.assertEqual(ui.get_variable("small"), 4)
        with self.assertRaises(ValueError): ui.set_variable("caption", 9)
        ui.show_screen("Monitor")
        self.assertIs(self.lv.current, ui.screens[1])
        ui.get_object(1, 7).emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.get_variable("level"), 64)
        screens = list(ui.screens)
        ui.deinit()
        self.assertIs(self.lv.current, self.initial)
        self.assertTrue(all(s.deleted for s in screens))
        self.assertFalse(ui._callbacks)

    def test_missing_display_fails_before_creating_screens(self):
        self.lv.display_get_default = lambda: None
        ui = self.UI({"obj": []})
        with self.assertRaisesRegex(RuntimeError, "display and input drivers"):
            ui.load()
        self.assertFalse(ui.screens)
        self.assertIs(self.lv.current, self.initial)

    def test_invalid_keyboard_mode_never_reaches_native_setters(self):
        for mode in (-1, 4, 5, 6, 7, 8, 9, 1000000, True, "0"):
            ui = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, "keyboard": {"mode": mode}}]})
            with self.assertRaisesRegex(ValueError, "Keyboard mode"):
                ui.load()
            self.assertFalse(ui.screens)
            self.assertIs(self.lv.current, self.initial)
        self.assertEqual(self.lv.keyboard.native_calls, [])

    def test_history_limits_inline_unbounded_and_defaults(self):
        ui = self.UI({"obj": [], "variables": [
            {"var": {"name": "one", "default": 5, "history": 1}},
            {"var": {"name": "all", "default": 9, "history": 0}}
        ]}).load()
        self.assertEqual(ui.get_history_count("one"), 0)
        for value in range(8):
            ui.set_variable("one", value)
            ui.set_variable("all", value)
        self.assertEqual(ui.get_history("one"), [7])
        self.assertEqual(ui.get_history("all"), list(range(8)))
        self.assertEqual(ui.get_history("one", 0), 7)
        with self.assertRaises(IndexError): ui.get_history("one", 1)
        with self.assertRaises(ValueError): ui.set_variable("one", float("nan"))
        self.assertEqual(ui.get_history("one"), [7])

    def test_reverse_parent_order_styles_and_missing_parent_rollback(self):
        ui = self.UI({"obj": [
            {"oid": 2, "par": 1, "base": {"coords": {"x": 9}}, "text": "nested"},
            {"oid": 1, "par": -1, "base": {"nativeStyles": [{"selector": 1,
                "values": {"bg_color": 0xff123456, "width": {"percent": 50}}}]}}
        ]}).load()
        self.assertIs(ui.get_object(0, 2).parent, ui.get_object(0, 1))
        self.assertEqual(ui.get_object(0, 2).x, 9)
        self.assertEqual(ui.get_object(0, 1).styles["bg_color", 1], 0x123456)
        self.assertEqual(ui.get_object(0, 1).styles["width", 1], ("percent", 50))
        ui.deinit()
        invalid = self.UI({"obj": [{"oid": 1, "par": 20, "base": {}, "text": "missing"}]})
        with self.assertRaisesRegex(ValueError, "Missing parent"): invalid.load()
        self.assertFalse(invalid.screens)
        self.assertIs(self.lv.current, self.initial)

    def test_keyboard_switches_target_and_only_closes_on_ready_or_cancel(self):
        ui = self.UI({"obj": [
            {"oid": 1, "par": -1, "base": {}, "textarea": {"text": "one"}},
            {"oid": 2, "par": -1, "base": {}, "textarea": {"text": "two"}},
            {"oid": 3, "par": -1, "base": {"hidden": True}, "keyboard": {}}
        ]}).load()
        first, second, keyboard = [ui.get_object(0, index) for index in (1, 2, 3)]
        first.emit(self.lv.EVENT.FOCUSED)
        self.assertIs(keyboard.target, first)
        self.assertFalse(keyboard.has_flag(self.lv.obj.FLAG.HIDDEN))
        second.emit(self.lv.EVENT.CLICKED)
        self.assertIs(keyboard.target, second)
        self.assertGreater(second.scroll_calls, 0)
        keyboard.emit(self.lv.EVENT.READY)
        self.assertTrue(keyboard.has_flag(self.lv.obj.FLAG.HIDDEN))
        self.assertIsNone(keyboard.target)
        first.emit(self.lv.EVENT.FOCUSED)
        keyboard.emit(self.lv.EVENT.CANCEL)
        self.assertTrue(keyboard.has_flag(self.lv.obj.FLAG.HIDDEN))

    def test_assets_deduplicated_animated_and_released(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "a.bin").write_bytes(b"\x00\x00\xff\xff")
            Path(directory, "b.bin").write_bytes(b"\xff\x00\x00\xff")
            asset = {"mpyAsset": "a.bin", "width": 1, "height": 1, "stride": 4, "colorFormat": 16}
            ui = self.UI({"obj": [
                {"oid": 1, "par": -1, "base": {}, "img": asset},
                {"oid": 2, "par": -1, "base": {}, "img": dict(asset, frames=[asset, dict(asset, mpyAsset="b.bin")], delays=[30, 60])}
            ]}, directory).load()
            first, second = ui.get_object(0, 1), ui.get_object(0, 2)
            self.assertIs(first.source, second.source)
            timer = ui._timers[0]
            timer.callback(timer)
            self.assertEqual(second.source["data"], b"\xff\x00\x00\xff")
            self.assertEqual(timer.period, 60)
            self.assertEqual(len(ui._images), 2)
            ui.deinit()
            self.assertTrue(timer.deleted)
            self.assertFalse(ui._images)
            self.assertFalse(ui._resources)

    def test_qrcode_utf8_byte_length_and_unsupported_features_fail(self):
        ui = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, "qrcode": {"data": "café", "quietZone": False}}]}).load()
        self.assertEqual(ui.get_object(0, 1).payload, "café".encode())
        ui.deinit()
        for kind in ("advancedKeyboard", "textBox", "layerView"):
            invalid = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, kind: {}}]})
            with self.assertRaisesRegex(ValueError, "not supported"): invalid.load()
            self.assertFalse(invalid.screens)

    def test_shared_pixel_file_keeps_distinct_image_headers(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "pixels.bin").write_bytes(bytes(range(8)))
            narrow = {"mpyAsset": "pixels.bin", "width": 1, "height": 2, "stride": 4, "colorFormat": 16}
            wide = dict(narrow, width=2, height=1, stride=8)
            ui = self.UI({"obj": [
                {"oid": 1, "par": -1, "base": {}, "img": narrow},
                {"oid": 2, "par": -1, "base": {}, "img": wide}
            ]}, directory).load()
            first, second = ui.get_object(0, 1).source, ui.get_object(0, 2).source
            self.assertEqual((first["header"]["w"], first["header"]["h"]), (1, 2))
            self.assertEqual((second["header"]["w"], second["header"]["h"]), (2, 1))
            self.assertIsNot(first, second)
            self.assertIs(first["data"], second["data"])
            self.assertEqual(len(ui._image_data), 1)
            ui.deinit()
            self.assertFalse(ui._image_data)

    def test_validation_stops_later_actions_and_does_not_handle_bubbled_events(self):
        actions = [{"action": "validate", "targetOid": 1, "min": 5},
                   {"action": "navigate", "screen": 1}]
        ui = self.UI({"screens": [{"obj": [
            {"oid": 1, "par": -1, "base": {}, "textarea": {"text": "3"}},
            {"oid": 2, "par": -1, "base": {"events": actions}, "button": {"text": "Save"}}
        ]}, {"obj": []}]}).load()
        button, entry = ui.get_object(0, 2), ui.get_object(0, 1)
        button.emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.active_screen, 0)
        self.assertTrue(entry.has_state(self.lv.STATE.USER_4))
        entry.text = "7"
        button.emit(self.lv.EVENT.CLICKED, target=button.children[0])
        self.assertEqual(ui.active_screen, 0)
        button.emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.active_screen, 1)
        self.assertFalse(entry.has_state(self.lv.STATE.USER_4))

    def test_bootstrap_actions_use_current_relatives(self):
        ui = self.UI({"obj": [
            {"oid": 1, "par": -1, "base": {}},
            {"oid": 2, "par": 1, "base": {"bootstrapAction": "collapse"}, "button": {}},
            {"oid": 3, "par": 1, "base": {}, "text": "Body"},
            {"oid": 4, "par": 1, "base": {"bootstrapAction": "dismiss"}, "button": {}}
        ]}).load()
        ui.get_object(0, 2).emit(self.lv.EVENT.CLICKED)
        self.assertTrue(ui.get_object(0, 3).has_flag(self.lv.obj.FLAG.HIDDEN))
        ui.get_object(0, 2).emit(self.lv.EVENT.CLICKED)
        self.assertFalse(ui.get_object(0, 3).has_flag(self.lv.obj.FLAG.HIDDEN))
        ui.get_object(0, 4).emit(self.lv.EVENT.CLICKED)
        self.assertTrue(ui.get_object(0, 1).has_flag(self.lv.obj.FLAG.HIDDEN))

    def test_partial_asset_load_failure_deletes_timers_and_all_created_widgets(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "first.bin").write_bytes(b"\x00\x00\xff\xff")
            Path(directory, "second.bin").write_bytes(b"\xff\x00\x00\xff")
            asset = {"mpyAsset": "first.bin", "width": 1, "height": 1, "stride": 4, "colorFormat": 16}
            animated = dict(asset, frames=[asset, dict(asset, mpyAsset="second.bin")], delays=[30, 60])
            failures = [
                ({"oid": 2, "par": -1, "base": {}, "img": dict(asset, mpyAsset="missing.bin")}, FileNotFoundError),
                ({"oid": 2, "par": -1, "base": {"nativeStyles": [{"selector": 0,
                    "values": {"missing_firmware_style": 1}}]}, "text": "Never committed"}, RuntimeError),
            ]
            for failed, exception in failures:
                with self.subTest(failure=exception.__name__):
                    created_before, timers_before = len(Obj.created), len(Timer.created)
                    ui = self.UI({"obj": [
                        {"oid": 1, "par": -1, "base": {}, "img": animated}, failed
                    ]}, directory)
                    with self.assertRaises(exception): ui.load()
                    self.assertTrue(Timer.created[timers_before:])
                    self.assertTrue(all(timer.deleted for timer in Timer.created[timers_before:]))
                    self.assertTrue(all(obj.deleted for obj in Obj.created[created_before:]))
                    self.assertIs(self.lv.current, self.initial)
                    self.assertFalse(ui.screens or ui.objects or ui.variables)
                    self.assertFalse(ui._timers or ui._callbacks or ui._resources or ui._image_data)
                    # A failed load leaves the same UI instance reusable.
                    ui.project_path = {"obj": [{"oid": 1, "par": -1, "base": {}, "text": "Retry"}]}
                    self.assertIs(ui.load(), ui)
                    self.assertEqual(ui.get_object(0, 1).get_text(), "Retry")
                    ui.deinit()

    def test_invalid_authored_defaults_and_history_replay_roll_back_the_entire_load(self):
        definitions = [
            {"name": "bad", "dataType": 1, "default": True},
            {"name": "bad", "dataType": 3, "default": float("inf")},
            {"name": "bad", "dataType": 0, "default": "embedded\x00nul"},
            {"name": "bad", "dataType": 1, "default": 4, "code": {"cType": "uint8_t"},
             "history": 2, "value": [5, 256]},
        ]
        for definition in definitions:
            with self.subTest(definition=definition):
                document = {"obj": [{"oid": 1, "par": -1, "base": {}, "text": "Before"}],
                    "variables": [{"var": {"name": "valid", "default": 1,
                        "bindings": [{"screen": 0, "oid": 1, "property": "/text", "direction": "to-widget"}]}},
                        {"var": definition}]}
                authored = copy.deepcopy(document)
                created_before = len(Obj.created)
                ui = self.UI(document)
                with self.assertRaises(ValueError): ui.load()
                self.assertEqual(document, authored)
                self.assertTrue(all(obj.deleted for obj in Obj.created[created_before:]))
                self.assertIs(self.lv.current, self.initial)
                self.assertFalse(ui.screens or ui.objects or ui.variables)
                self.assertFalse(ui._histories or ui._bindings or ui._callbacks)

    def test_duplicate_ids_parent_cycles_and_invalid_active_screen_leave_no_partial_ui(self):
        cases = [
            ({"obj": [{"oid": 1, "par": -1, "base": {}, "text": "First"},
                       {"oid": 1, "par": -1, "base": {}, "text": "Duplicate"}]}, "duplicate"),
            ({"obj": [{"oid": 1, "par": 2, "base": {}}, {"oid": 2, "par": 1, "base": {}}]}, "cyclic"),
            ({"screens": [{"obj": []}, {"obj": []}], "activeScreen": 2}, "Screen index"),
            ({"obj": [{"oid": 1, "par": -1, "base": {}},
                       {"oid": 2, "par": 1, "hostPath": [9], "base": {}, "text": "Missing host"}]}, "hostPath"),
        ]
        for document, message in cases:
            with self.subTest(message=message):
                created_before = len(Obj.created)
                ui = self.UI(document)
                with self.assertRaisesRegex(ValueError, message): ui.load()
                self.assertTrue(all(obj.deleted for obj in Obj.created[created_before:]))
                self.assertFalse(ui.screens or ui.objects or ui._callbacks)
                self.assertIs(self.lv.current, self.initial)

    def test_repeated_load_and_idempotent_deinit_keep_authored_values_and_release_old_screens(self):
        ui = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, "text": "Caption"}], "variables": [
            {"var": {"name": "value", "default": 1, "value": [2, 3], "history": 2,
                "bindings": [{"screen": 0, "oid": 1, "property": "/text", "direction": "to-widget"}]}}
        ]})
        for iteration in range(4):
            with self.subTest(iteration=iteration):
                ui.load()
                screen = ui.screens[0]
                caption = ui.get_object(0, 1)
                self.assertEqual(caption.get_text(), "3")
                self.assertEqual(ui.get_history("value"), [2, 3])
                with self.assertRaisesRegex(RuntimeError, "already loaded"): ui.load()
                self.assertIs(ui.get_object(0, 1), caption)
                ui.set_variable("value", 9)
                ui.deinit()
                ui.deinit()
                self.assertTrue(screen.deleted and caption.deleted)
                self.assertIs(self.lv.current, self.initial)
                self.assertFalse(ui.variables or ui._histories or ui._callbacks)

    def test_screen_transitions_never_delete_the_previous_screen_and_cancel_pending_loads(self):
        for initial_animated in (False, True):
            with self.subTest(initial_animated=initial_animated):
                transition = {"type": "fade", "duration": 250}
                document = {"screens": [
                    {"name": "First", "obj": [], "transition": transition if initial_animated else {}},
                    {"name": "Second", "obj": [], "transition": {"type": "slide_left", "duration": 400}}
                ]}
                created_before = len(Obj.created)
                ui = self.UI(document).load()
                if not initial_animated:
                    ui.show_screen("Second")
                    self.assertEqual(self.lv.pending_animation[1:4], (self.lv.SCR_LOAD_ANIM.MOVE_LEFT, 400, 0))
                self.assertIsNotNone(self.lv.pending_animation)
                self.assertIs(self.lv.pending_animation[-1], False)
                ui.deinit()
                self.assertIsNone(self.lv.pending_animation)
                self.assertIs(self.lv.current, self.initial)
                self.assertFalse(self.initial.deleted)
                self.assertTrue(all(obj.deleted for obj in Obj.created[created_before:]))

    def test_managed_children_and_host_paths_preserve_parentage_and_authored_order(self):
        ui = self.UI({"obj": [
            {"oid": 1, "par": -1, "base": {}, "contentChildren": [
                {"base": {}, "text": "Managed header"}, {"base": {"coords": {"x": 7}}}]},
            {"oid": 2, "par": 1, "hostPath": [1], "base": {}, "text": "First nested"},
            {"oid": 3, "par": 1, "hostPath": [1], "hostIndex": 0, "base": {}, "text": "Moved nested"}
        ]}).load()
        owner = ui.get_object(0, 1)
        host = owner.get_child(1)
        self.assertEqual(owner.get_child(0).get_text(), "Managed header")
        self.assertEqual(host.x, 7)
        self.assertIs(ui.get_object(0, 2).parent, host)
        self.assertEqual(host.children, [ui.get_object(0, 3), ui.get_object(0, 2)])
        descendants = list(Obj.created[1:])
        ui.deinit()
        self.assertTrue(all(obj.deleted for obj in descendants))

    def test_global_keyboard_maps_survive_ui_deletion_and_live_borrowers_keep_old_layouts(self):
        def keyboard_document(label, mode=0):
            settings = {"mode": mode}
            if label is not None: settings["buttons"] = {"btnMap": [label, ""], "nativeCtrl": [1]}
            return {"obj": [{"oid": 1, "par": -1, "base": {}, "keyboard": settings}]}
        original = self.UI(keyboard_document("Original")).load()
        original.deinit()
        gc.collect()
        borrower = self.UI(keyboard_document(None)).load()
        borrowed = borrower.get_object(0, 1)
        self.assertEqual(borrowed.get_button_text(0), "Original")
        replacement = self.UI(keyboard_document("Replacement")).load()
        self.assertEqual(replacement.get_object(0, 1).get_button_text(0), "Replacement")
        gc.collect()
        self.assertEqual(borrowed.get_button_text(0), "Original")
        replacement.deinit()
        borrower.deinit()
        gc.collect()
        for iteration in range(3):
            with self.subTest(iteration=iteration):
                later = self.UI(keyboard_document(None)).load()
                self.assertEqual(later.get_object(0, 1).get_button_text(0), "Replacement")
                later.deinit()
                gc.collect()
        self.assertIs(self.lv.current, self.initial)

    def test_custom_user_keyboard_map_is_installed_before_selecting_its_mode(self):
        ui = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, "keyboard": {
            "mode": 4, "buttons": {"btnMap": ["User key", ""], "nativeCtrl": [1]}}}]}).load()
        keyboard = ui.get_object(0, 1)
        self.assertEqual(keyboard.get_mode(), 4)
        self.assertEqual(keyboard.get_button_text(0), "User key")
        calls = self.lv.keyboard.native_calls
        self.assertLess(calls.index(("map", 4)), calls.index(("mode", 4)))
        ui.deinit()
        gc.collect()
        reused = self.UI({"obj": [{"oid": 1, "par": -1, "base": {}, "keyboard": {"mode": 4}}]}).load()
        self.assertEqual(reused.get_object(0, 1).get_button_text(0), "User key")
        reused.deinit()

    def test_binding_feedback_does_not_duplicate_history_or_reverse_one_way_bindings(self):
        class NotifyingSlider(Slider):
            def set_value(self, value, anim):
                super().set_value(value, anim)
                self.emit(self_lv.EVENT.VALUE_CHANGED)
        self_lv = self.lv
        self.lv.slider = NotifyingSlider
        ui = self.UI({"obj": [
            {"oid": 1, "par": -1, "base": {}, "slider": {"value": 10}},
            {"oid": 2, "par": -1, "base": {}, "slider": {"value": 20}},
            {"oid": 3, "par": -1, "base": {}, "slider": {"value": 30}}
        ], "variables": [{"var": {"name": "level", "default": 5, "history": 5, "bindings": [
            {"screen": 0, "oid": 1, "property": "/slider/value", "direction": "two-way"},
            {"screen": 0, "oid": 2, "property": "/slider/value", "direction": "to-widget"},
            {"screen": 0, "oid": 3, "property": "/slider/value", "direction": "from-widget"}
        ]}}]}).load()
        self.assertEqual(ui.get_history("level"), [])
        self.assertEqual(ui.get_object(0, 3).get_value(), 30)
        ui.set_variable("level", 40)
        self.assertEqual(ui.get_history("level"), [40])
        ui.get_object(0, 2).set_value(80, False)
        self.assertEqual(ui.get_variable("level"), 40)
        ui.get_object(0, 3).set_value(70, False)
        self.assertEqual(ui.get_history("level"), [40, 70])
        self.assertEqual(ui.get_object(0, 1).get_value(), 70)
        self.assertEqual(ui.get_object(0, 2).get_value(), 70)
        ui.deinit()

    def test_all_variable_types_and_integer_storage_boundaries_reject_invalid_updates_atomically(self):
        cases = []
        for bits in (8, 16, 32):
            for signed in (False, True):
                minimum = -(1 << (bits - 1)) if signed else 0
                maximum = (1 << (bits - (1 if signed else 0))) - 1
                ctype = ("int" if signed else "uint") + str(bits) + "_t"
                cases.append(({"name": ctype, "dataType": 1, "default": 0,
                               "code": {"cType": ctype}}, [minimum, maximum], [minimum - 1, maximum + 1, True, 1.5]))
        cases.extend([
            ({"name": "datetime", "dataType": 4, "default": 0},
             [-(1 << 63), (1 << 63) - 1], [-(1 << 63) - 1, 1 << 63, True, 1.5]),
            ({"name": "float32", "dataType": 3, "default": 0.0, "code": {"cType": "float"}},
             [-3.402823466e38, 3.402823466e38], [3.5e38, float("inf"), float("-inf"), float("nan"), True]),
            ({"name": "caption", "dataType": 0, "default": ""},
             ["", "Espa\u00f1ol \u4e2d\u6587"], ["bad\x00text", b"bytes", 1, None]),
            ({"name": "enabled", "dataType": 2, "default": False},
             [False, True], [0, 1, "true", None]),
        ])
        for definition, valid, invalid in cases:
            with self.subTest(variable=definition["name"]):
                name = definition["name"]
                ui = self.UI({"obj": [], "variables": [{"var": dict(definition, history=2)}]}).load()
                for value in valid: ui.set_variable(name, value)
                expected, history = ui.get_variable(name), ui.get_history(name)
                for value in invalid:
                    with self.subTest(rejected=value):
                        with self.assertRaises(ValueError): ui.set_variable(name, value)
                        self.assertEqual(ui.get_variable(name), expected)
                        self.assertEqual(ui.get_history(name), history)
                ui.deinit()


if __name__ == "__main__": unittest.main()
