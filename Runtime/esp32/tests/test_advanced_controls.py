"""Advanced composites opt in only to their actual native LVGL dependencies."""
import copy
import sys
from pathlib import Path
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import feature_plan as profile
from make_component import make_component


def project(*types):
    return {"formatVersion": 2, "runtimeTarget": "9.6",
            "canvas": {"width": 640, "height": 480},
            "obj": [{"oid": index, "par": -1, "base": {}, kind: {}}
                    for index, kind in enumerate(types)]}


class AdvancedControlProfiles(unittest.TestCase):
    def test_each_control_selects_only_its_native_dependencies(self):
        for kind, gate in {
            "radioGroup": "RADIO_GROUP", "checkboxButton": "CHECKBOX_BUTTON",
            "labeledCheckbox": "LABELED_CHECKBOX", "datePicker": "DATE_PICKER", "menu": "MENU"
        }.items():
            with self.subTest(kind=kind):
                document = project(kind)
                original = copy.deepcopy(document)
                plan = profile.analyze_projects([document])
                self.assertEqual(document, original)
                self.assertEqual(plan["customFeatures"], ["LVSERIAL_USE_" + gate])
                required = {"BUTTON", "LABEL", "FLEX"}
                if kind in ("checkboxButton", "labeledCheckbox"):
                    required.add("CHECKBOX")
                if kind == "datePicker":
                    required |= {"CALENDAR", "BUTTONMATRIX", "CALENDAR_HEADER_ARROW"}
                self.assertEqual(set(plan["enabledFeatures"]), required)
                config = profile.lvgl_config(plan, profile.repository_root())
                self.assertIn("#define LVSERIAL_USE_" + gate + " 1", config)
                self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD 0", config)
                sources = profile.source_plan(plan, profile.repository_root())
                self.assertFalse(any("/widgets/menu/" in source for source in sources))
                self.assertEqual(any("/widgets/checkbox/" in source for source in sources), kind in ("checkboxButton", "labeledCheckbox"))
                self.assertFalse(any("/widgets/textarea/" in source for source in sources))
                self.assertEqual(any("/widgets/calendar/" in source for source in sources), kind == "datePicker")

    def test_models_roundtrip_as_typed_properties(self):
        document = project("radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu")
        document["obj"][0]["radioGroup"] = {"options": [{"text": "Paris"}, {"text": "London", "disabled": True}], "selected": 0}
        document["obj"][1]["checkboxButton"] = {"text": "4 Star", "checked": True}
        document["obj"][2]["labeledCheckbox"] = {"text": "2 Queen", "description": "With breakfast", "checked": False}
        document["obj"][3]["datePicker"] = {"date": "2028-02-29", "format": "dd/mm/yyyy", "minDate": "2028-01-01", "maxDate": "2028-12-31"}
        document["obj"][4]["menu"] = {"items": [{"id": 1, "parent": 0, "text": "Music"}, {"id": 2, "parent": 1, "text": "Jazz"}], "selected": 2}
        for compact in (False, True):
            with self.subTest(compact=compact):
                self.assertEqual(profile.decode_project(profile.encode_project(document, omit_defaults=compact)), document)

    def test_unused_composites_do_not_increase_a_label_profile(self):
        document = project()
        document["obj"] = [{"oid": 1, "par": -1, "base": {}, "text": "Hello"}]
        document["components"] = [{"records": project("datePicker", "menu")["obj"]}]
        plan = profile.analyze_projects([document])
        self.assertEqual(plan["customFeatures"], [])
        self.assertEqual(plan["enabledFeatures"], ["LABEL"])

    def test_generated_component_contains_native_helper_and_selected_gates(self):
        import json
        with tempfile.TemporaryDirectory(prefix="advanced-controls-profile-") as temporary:
            root = Path(temporary)
            source = root / "controls.json"
            source.write_text(json.dumps(project("radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu")))
            output = root / "component"
            make_component([source], output)
            config = (output / "config/lv_conf.h").read_text()
            for gate in ("RADIO_GROUP", "CHECKBOX_BUTTON", "LABELED_CHECKBOX", "DATE_PICKER", "MENU"):
                self.assertIn("#define LVSERIAL_USE_" + gate + " 1", config)
            for header in ("AdvancedControls.h", "AdvancedControlsRuntime.h"):
                relative = Path("LVSerial/Serialization") / header
                self.assertEqual((output / "vendor" / relative).read_bytes(), (profile.repository_root() / relative).read_bytes())
            saved = profile.decode_project((output / "projects/controls.pb").read_bytes())
            self.assertEqual([set(item) & set(profile.CUSTOM) for item in saved["obj"]],
                             [{kind} for kind in ("radioGroup", "checkboxButton", "labeledCheckbox", "datePicker", "menu")])


if __name__ == "__main__":
    unittest.main()
