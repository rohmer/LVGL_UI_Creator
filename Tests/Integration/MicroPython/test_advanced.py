"""Advanced control behavior against a bounded LVGL binding test double."""
from pathlib import Path
import copy
import importlib.util
import sys
import types
import unittest

from test_runtime import binding, Obj, Event

ROOT = Path(__file__).resolve().parents[3]


class Matrix(Obj):
    CTRL = types.SimpleNamespace(DISABLED=1, CHECKED=2)
    def __init__(self, parent):
        super().__init__(parent)
        self.controls, self.selected = {}, 0
    def set_map(self, value): self.map = value
    def set_one_checked(self, value): self.one_checked = value
    def set_button_ctrl(self, index, value): self.controls[index] = self.controls.get(index, 0) | value
    def clear_button_ctrl(self, index, value): self.controls[index] = self.controls.get(index, 0) & ~value
    def get_selected_button(self): return self.selected


def document():
    return {"canvas": {"width": 640, "height": 400}, "variables": [
        {"var": {"name": name, "dataType": kind, "default": default, "history": 2,
                 "bindings": [{"screen": 0, "oid": oid, "property": path, "direction": "two-way"}]}}
        for name, kind, default, oid, path in [
            ("radioSelection", 1, 0, 1, "/radioGroup/selected"),
            ("checkboxValue", 2, False, 2, "/checkboxButton/checked"),
            ("nestedValue", 2, True, 3, "/labeledCheckbox/checked"),
            ("chosenDate", 0, "2028-02-29", 4, "/datePicker/date"),
            ("menuSelection", 1, 1, 5, "/menu/selected")]], "obj": [
        {"oid": 1, "par": -1, "base": {}, "radioGroup": {"title": "Location", "selected": 0,
            "options": [{"text": "New York"}, {"text": "Paris"}, {"text": "London", "disabled": True}]}},
        {"oid": 2, "par": -1, "base": {}, "checkboxButton": {"text": "4 Star", "checked": False}},
        {"oid": 3, "par": -1, "base": {}, "labeledCheckbox": {"text": "2 Queen", "description": "Breakfast included", "checked": True}},
        {"oid": 4, "par": -1, "base": {}, "datePicker": {"date": "2028-02-29", "format": "dd/mm/yyyy",
            "minDate": "2028-01-01", "maxDate": "2028-12-31", "firstDay": 1}},
        {"oid": 5, "par": -1, "base": {}, "menu": {"title": "Menu", "selected": 1, "items": [
            {"id": 1, "parent": 0, "text": "Books"}, {"id": 2, "parent": 0, "text": "Music"},
            {"id": 3, "parent": 2, "text": "Jazz"}, {"id": 4, "parent": 2, "text": "Disabled", "disabled": True}]}}
    ]}


class AdvancedTests(unittest.TestCase):
    def setUp(self):
        Obj.created.clear()
        self.lv = binding()
        self.lv.FLEX_FLOW = types.SimpleNamespace(COLUMN=1, ROW=2, ROW_WRAP=3)
        self.lv.EVENT.KEY, self.lv.EVENT.DELETE = 7, 8
        self.lv.KEY = types.SimpleNamespace(ESC=27)
        self.lv.buttonmatrix = Matrix
        Obj.set_style_pad_all = lambda obj, value, selector: None
        Obj.set_flex_grow = lambda obj, value: setattr(obj, "grow", value)
        Obj.update_layout = lambda obj: None
        Obj.get_width = lambda obj: obj.width
        Obj.get_height = lambda obj: obj.height
        Obj.get_screen = lambda obj: obj if obj.parent is None else obj.parent.get_screen()
        Obj.send_event = lambda obj, code, parameter: obj.emit(code)
        Event.get_key = lambda event: 27
        self.original_delete = Obj.delete
        def deleting(obj):
            if not obj.deleted:
                obj.emit(8)
                self.original_delete(obj)
        Obj.delete = deleting
        sys.modules["lvgl"] = self.lv
        sys.modules.pop("uicreator_advanced", None)
        spec = importlib.util.spec_from_file_location("advanced_runtime_test", ROOT / "CodeGeneration/MicroPython/uicreator_lvgl.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.UI = module.UI

    def tearDown(self):
        Obj.delete = self.original_delete
        sys.modules.pop("lvgl", None)
        sys.modules.pop("uicreator_advanced", None)

    def test_radio_exclusivity_disabled_and_root_events(self):
        ui = self.UI(document()).load()
        radio = ui.get_object(0, 1)
        events = []
        ui._on(radio, lambda event: events.append(ui.get_advanced_value(radio)), self.lv.EVENT.VALUE_CHANGED)
        ui.get_advanced_part(radio, "Option1").emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.get_advanced_value(radio), 1)
        self.assertEqual(events, [1])
        self.assertFalse(ui.get_advanced_part(radio, "Option0").has_state(self.lv.STATE.CHECKED))
        with self.assertRaises(ValueError): ui.set_advanced_value(radio, 2)
        self.assertEqual(ui.get_advanced_value(radio), 1)
        ui.deinit()

    def test_checkbox_card_label_and_native_checkbox_share_state(self):
        ui = self.UI(document()).load()
        checkbox, nested = ui.get_object(0, 2), ui.get_object(0, 3)
        self.assertFalse(ui.get_advanced_value(checkbox))
        self.assertTrue(ui.get_advanced_value(nested))
        nested.emit(self.lv.EVENT.CLICKED)
        self.assertFalse(ui.get_advanced_value(nested))
        native = ui.get_advanced_part(nested, "Checkbox")
        native.add_state(self.lv.STATE.CHECKED)
        native.emit(self.lv.EVENT.VALUE_CHANGED)
        self.assertTrue(ui.get_advanced_value(nested))
        self.assertEqual(ui.get_advanced_part(nested, "Description").text, "Breakfast included")
        ui.deinit()

    def test_leap_date_first_weekday_selection_range_and_popup_dismissal(self):
        ui = self.UI(document()).load()
        date = ui.get_object(0, 4)
        self.assertIsNone(ui.get_advanced_part(date, "Popup"))
        self.assertEqual(ui.get_advanced_part(date, "Label").text, "29/02/2028")
        with self.assertRaises(ValueError): ui.set_advanced_value(date, "2028-02-30")
        with self.assertRaises(ValueError): ui.set_advanced_value(date, "2029-01-01")
        self.assertEqual(ui.get_advanced_value(date), "2028-02-29")
        ui.open_date_picker(date)
        popup = ui.get_advanced_part(date, "Popup")
        matrix = ui.get_advanced_part(date, "Days")
        state = ui._advanced._find(date)[1]
        self.assertEqual(matrix.map[0].str_val, "Mo")
        self.assertEqual(state["dates"][8], "2028-02-01")  # February begins Tuesday.
        matrix.selected = state["dates"].index("2028-02-16")
        matrix.emit(self.lv.EVENT.VALUE_CHANGED)
        self.assertEqual(ui.get_advanced_value(date), "2028-02-16")
        self.assertTrue(popup.has_flag(Obj.FLAG.HIDDEN))
        ui.open_date_picker(date)
        popup.emit(self.lv.EVENT.CLICKED)
        self.assertTrue(popup.has_flag(Obj.FLAG.HIDDEN))
        ui.open_date_picker(date)
        matrix.emit(self.lv.EVENT.KEY)
        self.assertTrue(popup.has_flag(Obj.FLAG.HIDDEN))
        root_count = len(ui._resources)
        for unused in range(12): ui.get_advanced_part(date, "Next").emit(self.lv.EVENT.CLICKED)
        self.assertEqual(len(ui._resources), root_count)
        ui.deinit()
        self.assertTrue(popup.deleted)

    def test_menu_nested_navigation_disabled_selection_and_back(self):
        ui = self.UI(document()).load()
        menu = ui.get_object(0, 5)
        leaf = ui.get_advanced_part(menu, "Item3")
        self.assertTrue(leaf.has_flag(Obj.FLAG.HIDDEN))
        ui.get_advanced_part(menu, "Item2").emit(self.lv.EVENT.CLICKED)
        self.assertFalse(leaf.has_flag(Obj.FLAG.HIDDEN))
        ui.get_advanced_part(menu, "Item4").emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.get_advanced_value(menu), 1)
        leaf.emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.get_advanced_value(menu), 3)
        ui.get_advanced_part(menu, "Back").emit(self.lv.EVENT.CLICKED)
        self.assertTrue(leaf.has_flag(Obj.FLAG.HIDDEN))
        ui.deinit()

    def test_date_popup_fits_a_small_device_screen(self):
        project = document()
        project["canvas"] = {"width": 320, "height": 240}
        ui = self.UI(project).load()
        date = ui.get_object(0, 4)
        ui.open_date_picker(date)
        calendar = ui.get_advanced_part(date, "Calendar")
        self.assertEqual(calendar.width, 280)
        self.assertEqual(calendar.height, 224)
        self.assertEqual(ui.get_advanced_part(date, "Days").grow, 1)
        ui.deinit()

    def test_invalid_model_rolls_back_every_created_object(self):
        project = document()
        project["obj"][4]["menu"]["items"][1]["parent"] = 3
        ui = self.UI(project)
        with self.assertRaises(ValueError): ui.load()
        self.assertFalse(ui.screens)
        self.assertTrue(all(obj.deleted for obj in Obj.created if obj is not self.lv.current))

    def test_two_way_bindings_reject_invalid_changes_before_history_or_fanout(self):
        ui = self.UI(document()).load()
        radio, date, menu = ui.get_object(0, 1), ui.get_object(0, 4), ui.get_object(0, 5)
        ui.set_variable("radioSelection", 1)
        self.assertEqual(ui.get_advanced_value(radio), 1)
        history = ui.get_history("radioSelection")
        with self.assertRaises(ValueError): ui.set_variable("radioSelection", 2)
        self.assertEqual(ui.get_variable("radioSelection"), 1)
        self.assertEqual(ui.get_history("radioSelection"), history)
        ui.set_variable("chosenDate", "2028-03-15")
        self.assertEqual(ui.get_advanced_value(date), "2028-03-15")
        with self.assertRaises(ValueError): ui.set_variable("chosenDate", "2028-02-30")
        self.assertEqual(ui.get_variable("chosenDate"), "2028-03-15")
        ui.set_variable("menuSelection", 3)
        self.assertFalse(ui.get_advanced_part(menu, "Item3").has_flag(Obj.FLAG.HIDDEN))
        ui.get_advanced_part(menu, "Item1").emit(self.lv.EVENT.CLICKED)
        self.assertEqual(ui.get_variable("menuSelection"), 1)
        checkbox = ui.get_object(0, 2)
        checkbox.emit(self.lv.EVENT.CLICKED)
        self.assertTrue(ui.get_variable("checkboxValue"))
        ui.deinit()


if __name__ == "__main__": unittest.main()
