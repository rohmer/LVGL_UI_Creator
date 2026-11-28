"""Project-owned keyboard options drive every portable source profile."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import feature_plan as profile
from make_component import make_component


def screen(width=320, height=240):
    return {"canvas": {"width": width, "height": height, "base": {}},
            "obj": [{"oid": 0, "par": -1, "base": {}, "textarea": {"text": "Input"}}]}


def project():
    return {"formatVersion": 2, "runtimeTarget": "9.6", **screen()}


def model():
    return struct.pack("<4sIII", b"NGTR", 3, 4, 0) + b"\0\2\0\2a\1\1a\1\1"


class ProjectKeyboardTests(unittest.TestCase):
    def test_disabled_default_excludes_service_language_and_missing_model(self):
        for settings in ({}, {"enabled": False, "language": "ar-SA", "trieId": "not-deployed"}):
            document = project()
            document["advancedKeyboard"] = settings
            original = copy.deepcopy(document)
            optimized = profile.optimize_document(document)
            self.assertEqual(document, original)
            self.assertNotIn("advancedKeyboard", optimized)
            self.assertEqual(len(optimized["obj"]), 1)
            plan = profile.analyze_projects([document])
            self.assertEqual(plan["enabledFeatures"], ["LABEL", "TEXTAREA"])
            self.assertEqual(plan["customFeatures"], [])
            self.assertEqual(plan["keyboardLanguages"], [])
            self.assertEqual(plan["referencedTries"], [])

    def test_global_option_materializes_each_screen_without_mutating_saved_data(self):
        document = {"formatVersion": 2, "runtimeTarget": "9.6", "screens": [screen(), screen(480, 800)],
                    "advancedKeyboard": {"enabled": True, "language": "fr-FR", "autoFocus": False,
                                         "targetOid": 40, "hideOnDefocus": True}}
        original = copy.deepcopy(document)
        optimized = profile.optimize_document(document)
        self.assertEqual(document, original)
        self.assertNotIn("advancedKeyboard", optimized)
        for page, height in zip(optimized["screens"], (144, 240)):
            keyboard = page["obj"][-1]
            self.assertEqual(keyboard["oid"], 1)
            self.assertEqual(keyboard["par"], -1)
            self.assertEqual(keyboard["base"]["coords"], {"x": 0, "y": page["canvas"]["height"]-height,
                             "width": page["canvas"]["width"], "height": height})
            self.assertFalse(keyboard["base"]["hidden"])
            self.assertFalse(keyboard["base"]["flags"]["scrollable"])
            self.assertEqual(keyboard["advancedKeyboard"]["targetOid"], -1)
            self.assertTrue(keyboard["advancedKeyboard"]["autoFocus"])
            self.assertFalse(keyboard["advancedKeyboard"]["hideOnDefocus"])
            self.assertTrue(keyboard["advancedKeyboard"]["screenManaged"])
            self.assertNotIn("enabled", keyboard["advancedKeyboard"])
        self.assertEqual(profile.optimize_document(optimized), optimized)
        self.assertEqual(profile.decode_project(profile.encode_project(optimized)), optimized)
        plan = profile.analyze_projects([document])
        self.assertEqual(plan["keyboardLanguages"], ["fr-FR"])
        self.assertEqual(plan["customFeatures"], ["LVSERIAL_USE_ADVANCED_KEYBOARD"])
        self.assertEqual(plan["enabledFeatures"], ["BUTTON", "LABEL", "TEXTAREA"])
        sources = profile.source_plan(plan, profile.repository_root())
        self.assertIn("3rdParty/AdvancedKeyboard/src/Keyboard.cpp", sources)
        self.assertFalse(any(Path(path).name.startswith("Prediction") for path in sources))
        document["advancedKeyboard"]["height"] = 500
        explicit = profile.optimize_document(document)
        self.assertEqual([page["obj"][-1]["base"]["coords"]["height"] for page in explicit["screens"]], [240, 500])

    def test_explicit_project_option_supersedes_legacy_and_preserves_its_base(self):
        document = project()
        legacy = {"oid": 7, "par": -1, "base": {"flags": {"clickable": True},
                  "nativeStyles": [{"selector": 0, "values": {"text_color": 0x123456}}]},
                  "advancedKeyboard": {"language": "ar-SA", "trieId": "missing-old-model"}}
        document["obj"].append(legacy)
        self.assertEqual(profile._runtime_document(document)["obj"][-1], legacy)
        document["advancedKeyboard"] = {"enabled": True, "language": "fr-FR"}
        optimized = profile.optimize_document(document)
        keyboard = optimized["obj"][-1]
        self.assertEqual(keyboard["oid"], 7)
        self.assertTrue(keyboard["base"]["flags"]["clickable"])
        self.assertEqual(keyboard["base"]["nativeStyles"], legacy["base"]["nativeStyles"])
        plan = profile.analyze_projects([document])
        self.assertEqual(plan["keyboardLanguages"], ["fr-FR"])
        self.assertEqual(plan["referencedTries"], [])
        self.assertNotIn("BIDI", plan["enabledFeatures"])
        document["advancedKeyboard"]["enabled"] = False
        disabled = profile.optimize_document(document)
        self.assertEqual(len(disabled["obj"]), 1)
        self.assertEqual(profile.analyze_projects([document])["customFeatures"], [])

    def test_only_global_selected_model_and_language_enter_source_closure(self):
        document = {"formatVersion": 2, "runtimeTarget": "9.6", "screens": [screen(), screen(480, 800)],
                    "advancedKeyboard": {"enabled": True, "language": "ur-PK", "trieId": "selected"},
                    "tries": [{"id": identity, "name": identity, "data": model()} for identity in ("selected", "unused")]}
        optimized = profile.optimize_document(document)
        self.assertEqual([item["id"] for item in optimized["tries"]], ["selected"])
        plan = profile.analyze_projects([document])
        self.assertEqual(plan["keyboardLanguages"], ["ur-PK"])
        self.assertEqual(plan["referencedTries"], ["selected"])
        self.assertEqual(plan["trieAssetCount"], 1)
        self.assertEqual(plan["trieModelBytes"], len(model()))
        self.assertIn("BIDI", plan["enabledFeatures"])
        sources = profile.source_plan(plan, profile.repository_root())
        self.assertIn("3rdParty/AdvancedKeyboard/src/KeyboardUrduFont.c", sources)
        self.assertIn("3rdParty/AdvancedKeyboard/src/Prediction.cpp", sources)
        self.assertNotIn("3rdParty/AdvancedKeyboard/src/KeyboardHindiFont.c", sources)
        document["advancedKeyboard"]["trieId"] = "not-present"
        with self.assertRaisesRegex(ValueError, "Unknown project trie"):
            profile.optimize_document(document)

    def test_generated_component_contains_project_service_and_minimal_sources(self):
        document = project()
        document["advancedKeyboard"] = {"enabled": True, "language": "fr-FR", "height": 100}
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "project.json"
            source.write_text(json.dumps(document), encoding="utf-8")
            output = root / "component"
            plan = make_component([source], output)
            saved = profile.decode_project((output / "projects/project.pb").read_bytes())
            self.assertNotIn("advancedKeyboard", saved)
            self.assertTrue(saved["obj"][-1]["advancedKeyboard"]["screenManaged"])
            self.assertEqual(saved["obj"][-1]["base"]["coords"]["height"], 100)
            self.assertIn("3rdParty/AdvancedKeyboard/src/Keyboard.cpp", plan["sourceFiles"])
            self.assertNotIn("3rdParty/AdvancedKeyboard/src/Prediction.cpp", plan["sourceFiles"])
            self.assertNotIn("3rdParty/LVGL/lvgl/src/widgets/keyboard/lv_keyboard.c", plan["sourceFiles"])
            for name in ("AdvancedKeyboardSettings.h", "ProjectKeyboard.h"):
                bundled = output / "vendor/LVSerial/Serialization" / name
                self.assertEqual(bundled.read_bytes(), (profile.repository_root() / "LVSerial/Serialization" / name).read_bytes())

    def test_project_option_validation_and_automatic_dimensions(self):
        invalid = ({"enabled": 1}, {"enabled": True, "height": 0}, {"height": True},
                   {"language": "xx-XX"}, {"rightToLeft": 1}, {"autocorrectThreshold": float("nan")})
        for settings in invalid:
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                profile.analyze_projects([{**project(), "advancedKeyboard": settings}])
        blank = profile.optimize_document({"obj": [], "advancedKeyboard": {"enabled": True, "trieId": "", "height": None}})
        self.assertEqual(blank["obj"][0]["base"]["coords"], {"x": 0, "y": 96, "width": 320, "height": 144})


if __name__ == "__main__":
    unittest.main()
