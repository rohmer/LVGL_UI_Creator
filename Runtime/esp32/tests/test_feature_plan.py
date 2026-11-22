import copy
import base64
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import struct
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import feature_plan as profile
from make_component import make_component


def project(*sections):
    return {"formatVersion": 2, "runtimeTarget": "9.6", "canvas": {"width": 240, "height": 320, "base": {}},
            "obj": [{"base": {"name": str(index), "nativeStyles": []}, "oid": index, "par": -1, **section}
                    for index, section in enumerate(sections)]}


class FeaturePlanTests(unittest.TestCase):
    def test_unused_component_definitions_and_baselines_are_not_runtime_features(self):
        document = project({"text": "Visible"})
        unused = {"base": {}, "oid": 6, "par": -1,
                  "advancedKeyboard": {"language": "en-US", "trieId": "unused-missing-trie"}}
        document["components"] = [{"records": [unused, {"base": {}, "chart": {}}]}]
        document["sharedStyles"] = [{"preview": {"base": {}, "qrcode": {"data": "unused"}}}]
        document["projectTheme"] = {"id": "slate-dark"}
        document["obj"][0]["base"]["design"] = {"component": {"baseline": unused},
                                                "theme": {"background": "surface"}}
        original = copy.deepcopy(document)
        optimized = profile.optimize_document(document)
        self.assertNotIn("components", optimized)
        self.assertNotIn("sharedStyles", optimized)
        self.assertEqual(optimized["projectTheme"], original["projectTheme"])
        self.assertEqual(optimized["obj"][0]["base"]["design"], {"theme": {"background": "surface"}})
        for candidate in (document, optimized, {"screens": [document], "formatVersion": 2}):
            plan = profile.analyze_projects([candidate])
            self.assertEqual(plan["enabledFeatures"], ["LABEL"])
            self.assertEqual(plan["customFeatures"], [])
            self.assertEqual(plan["referencedTries"], [])
        self.assertEqual(document, original)

    def test_wire_versions_defaults_and_values(self):
        document = project({"text": {"text": "Unicode \u2022 \U0001f642", "selected": 0}})
        document["extra"] = {"null": None, "empty": [], "nested": {"x": 0, "hidden": False, "text": ""},
                             "binary": b"\0\xff\x10", "signed": -(1 << 63), "unsigned": (1 << 64) - 1, "float": 0.125}
        for compact in (False, True):
            encoded = profile.encode_project(document, omit_defaults=compact)
            self.assertEqual(profile.decode_project(encoded), document)
            with self.assertRaises(ValueError): profile.decode_project(encoded[:-1])

    def test_real_project_default_profiles(self):
        fixtures = profile.repository_root() / "build/professional/bootstrap-generated"
        if not fixtures.is_dir(): self.skipTest("Generated fixture matrix is not present")
        for directory in fixtures.iterdir():
            if not (directory / "fixture.pb").is_file(): continue
            expected = json.loads((directory / "fixture.json").read_text(encoding="utf-8"))
            self.assertEqual(profile.decode_project((directory / "fixture.pb").read_bytes()), expected, directory.name)

    def test_dependencies_and_union(self):
        label = profile.analyze_projects([project({"text": {"text": "hello"}})])
        self.assertEqual(label["enabledFeatures"], ["LABEL"])
        qr = profile.analyze_projects([project({"qrcode": {"text": "hello"}}), project({"keyboard": {}})])
        for dependency in ("QRCODE", "CANVAS", "IMAGE", "KEYBOARD", "BUTTONMATRIX", "TEXTAREA", "LABEL"):
            self.assertIn(dependency, qr["enabledFeatures"])
        self.assertIn("I1", qr["softwareFormats"])
        self.assertNotIn("CHART", qr["enabledFeatures"])
        qr_sources = profile.source_plan(qr, profile.repository_root())
        self.assertTrue(any(source.endswith("/widgets/qrcode/lv_qrcode.c") for source in qr_sources))
        self.assertTrue(any(source.endswith("/libs/qrcode/qrcodegen.c") for source in qr_sources))
        payload = profile.analyze_projects([project({"qrcode": {"text": "\u0627\u0644\u0633\u0644\u0627\u0645"}})])
        self.assertNotIn("BIDI", payload["enabledFeatures"])
        visible = profile.analyze_projects([project({"text": {"text": "\u0627\u0644\u0633\u0644\u0627\u0645"}})])
        self.assertIn("BIDI", visible["enabledFeatures"])
        self.assertIn("ARABIC_PERSIAN_CHARS", visible["enabledFeatures"])

    def test_layout_and_custom_dependencies(self):
        document = project({"scale": {"variant": "gauge"}})
        document["obj"][0]["base"]["nativeStyles"] = [{"selector": 0, "values": {"layout": 2, "grid_row_dsc_array": [10, 20]}}]
        plan = profile.analyze_projects([document, project({"textBox": {}})])
        for name in ("SCALE", "LINE", "GRID", "FLEX", "BUTTON", "TEXTAREA", "LABEL"):
            self.assertIn(name, plan["enabledFeatures"])
        self.assertIn("LVSERIAL_USE_TEXTBOX", plan["customFeatures"])

    def test_advanced_keyboard_dependency_and_source_closure(self):
        root = profile.repository_root()
        document = project({"advancedKeyboard": {"trieId": "", "targetOid": -1,
                                                  "maxLength": 4096, "keyBackground": 0xFFFFFF,
                                                  "keyForeground": 0x202020, "keyRadius": 5}})
        plan = profile.analyze_projects([profile.decode_project(profile.encode_project(document))])
        self.assertEqual(plan["customFeatures"], ["LVSERIAL_USE_ADVANCED_KEYBOARD"])
        self.assertEqual(plan["enabledFeatures"], ["BUTTON", "LABEL", "TEXTAREA"])
        config = profile.lvgl_config(plan, root)
        self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD 1", config)
        self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION 0", config)
        self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE 0", config)
        for name in ("KEYBOARD", "BUTTONMATRIX", "DROPDOWN", "FLEX"):
            self.assertIn(f"#define LV_USE_{name} 0", config)
        sources = profile.source_plan(plan, root)
        widget = "LVSerial/Widgets/AdvancedKeyboard/"
        for name in ("Key.cpp", "Keyboard.cpp", "TypingView.cpp", "SuggestionText.cpp", "Utf8Fold.cpp", "AdvancedKeyboard.cpp", "KeyboardFont.c"):
            self.assertIn(widget + name, sources)
        label_plan = profile.analyze_projects([project({"text": {"text": "hello"}})])
        self.assertNotIn("LVSERIAL_USE_ADVANCED_KEYBOARD", label_plan["customFeatures"])
        self.assertFalse(any(source.startswith(widget) for source in profile.source_plan(label_plan, root)))
        self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD 0", profile.lvgl_config(label_plan, root))

    def test_prediction_builtin_custom_and_unused_catalogs(self):
        root = profile.repository_root()
        builtin = profile.analyze_projects([project({"advancedKeyboard": {}})])
        for name in ("PREDICTION", "BUILTIN_TRIE"):
            self.assertIn("LVSERIAL_USE_ADVANCED_KEYBOARD_" + name, builtin["customFeatures"])
        sources = profile.source_plan(builtin, root)
        for name in ("PredictionModel.cpp", "PredictionPrefix.cpp", "Prediction.cpp"):
            self.assertIn("LVSerial/Widgets/AdvancedKeyboard/" + name, sources)
        model = struct.pack("<4sIII", b"NGTR", 3, 4, 0) + b"\0\2\0\2a\1\1a\1\1"
        document = project({"advancedKeyboard": {"trieId": "custom"}})
        document["tries"] = [{"id": "custom", "name": "Model.ngtr", "format": "ngtr", "data": base64.b64encode(model).decode()}]
        prepared = profile.optimize_document(document)
        self.assertEqual(prepared["tries"][0]["data"], model)
        self.assertTrue(prepared["tries"][0]["indexData"].startswith(b"NGIX"))
        self.assertEqual(profile.decode_project(profile.encode_project(prepared)), prepared)
        plan = profile.analyze_projects([prepared])
        self.assertEqual(plan["trieAssetCount"], 1)
        self.assertEqual(plan["trieModelBytes"], len(model))
        self.assertIn("LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION", plan["customFeatures"])
        self.assertNotIn("LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE", plan["customFeatures"])
        unused = copy.deepcopy(document);unused["obj"][0]["advancedKeyboard"]["trieId"] = ""
        self.assertNotIn("tries", profile.optimize_document(unused))
        invalid = copy.deepcopy(document);invalid["obj"][0]["advancedKeyboard"]["trieId"] = "missing"
        with self.assertRaisesRegex(ValueError, "Unknown project trie"): profile.optimize_document(invalid)
        invalid = copy.deepcopy(prepared);invalid["tries"][0]["indexData"] += b"\0"
        with self.assertRaisesRegex(ValueError, "does not match"): profile.optimize_document(invalid)
        arabic = struct.pack("<4sIII", b"NGTR", 3, 6, 0) + b"\0\2\0\2\xd8\2\xd8\2\xa7\1\1\xa7\1\1"
        rtl = copy.deepcopy(document);rtl["tries"][0]["data"] = base64.b64encode(arabic).decode()
        for feature in ("BIDI", "ARABIC_PERSIAN_CHARS"):
            self.assertIn(feature, profile.analyze_projects([rtl])["enabledFeatures"])

    def test_keyboard_language_defaults_keep_unused_prediction_out(self):
        root = profile.repository_root()
        for language in ("es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT"):
            with self.subTest(language=language):
                document = project({"advancedKeyboard": {"language": language}})
                encoded = profile.encode_project(document)
                decoded = profile.decode_project(encoded)
                self.assertEqual(decoded, document)
                plan = profile.analyze_projects([decoded])
                self.assertEqual(plan["customFeatures"], ["LVSERIAL_USE_ADVANCED_KEYBOARD"])
                self.assertEqual(plan["referencedTries"], [])
                self.assertFalse(any(Path(source).name.startswith("Prediction") for source in profile.source_plan(plan, root)))
                config = profile.lvgl_config(plan, root)
                self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION 0", config)
                self.assertIn("#define LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE 0", config)
        explicit = project({"advancedKeyboard": {"language": "fr-FR", "trieId": "builtin-en-us"}})
        self.assertIn("LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE", profile.analyze_projects([explicit])["customFeatures"])
        for invalid in ("xx-XX", 12, None):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(ValueError, "Unsupported Advanced Keyboard language"):
                profile.analyze_projects([project({"advancedKeyboard": {"language": invalid}})])

    def test_raw_hardware_formats_and_wrong_targets_rejected(self):
        document = project({"img": {"asset": {"deviceFrames": [{"cf": 0x30}]}}})
        with self.assertRaisesRegex(ValueError, "unsupported"): profile.analyze_projects([document])
        document = project();document["runtimeTarget"] = "8.3"
        with self.assertRaisesRegex(ValueError, "9.6"): profile.analyze_projects([document])
        external = project({"img": {"srcType": 1, "fileName": "S:/image.png"}})
        with self.assertRaisesRegex(ValueError, "external image"): profile.analyze_projects([external])

    def test_inline_pixels_and_packed_alpha(self):
        inline = project({"img": {"srcType": 0, "formatVersion": 9, "cf": 15, "w": 1, "h": 1, "data": "AAAA"}})
        self.assertIn("RGB888", profile.analyze_projects([inline])["softwareFormats"])
        alpha = project({"img": {"asset": {"deviceFrames": [{"formatVersion": 9, "cf": 11, "data": "AA=="}]}}})
        self.assertIn("A8", profile.analyze_projects([alpha])["softwareFormats"])

    def test_device_asset_optimization_preserves_original(self):
        document = project({"img": {"asset": {"sourceData": "original", "previewFrames": ["preview"],
                                             "options": {"verbose": True}, "deviceFrames": [{"cf": 18, "data": "AAA="}], "delays": [100]}}})
        original = copy.deepcopy(document)
        result = profile.optimize_document(document)
        asset = result["obj"][0]["img"]["asset"]
        self.assertNotIn("sourceData", asset);self.assertNotIn("previewFrames", asset)
        self.assertEqual(asset["delays"], [100]);self.assertEqual(document, original)
        self.assertEqual(profile.optimize_document(document, True), original)

    def test_sources_and_configuration_are_project_specific(self):
        root = profile.repository_root()
        plan = profile.analyze_projects([project({"text": {"text": "hello"}})])
        sources = profile.source_plan(plan, root)
        self.assertTrue(any("/widgets/label/" in source for source in sources))
        self.assertFalse(any("/widgets/chart/" in source or "/drivers/" in source for source in sources))
        self.assertFalse(any("/SDL" in source or "PNGToArray" in source or "lv_bootstrap.c" in source for source in sources))
        self.assertTrue(all((root / source).is_file() for source in sources))
        config = profile.lvgl_config(plan, root)
        self.assertIn("#define LV_USE_LABEL 1", config)
        self.assertIn("#define LV_USE_CHART 0", config)
        self.assertIn("#define LV_USE_SDL 0", config)
        self.assertIn("#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB", config)

    def test_standalone_component_and_stale_profile_detection(self):
        base = profile.repository_root() / "build/esp32-agent-tests"
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as temporary:
            directory = Path(temporary)
            source = directory / "hello.pb"
            source.write_bytes(profile.encode_project(project({"text": {"text": "ESP32"}})))
            output = directory / "component"
            plan = make_component([source], output)
            self.assertLess(plan["lvglSourceFileCount"], plan["fullLvglSourceFileCount"])
            self.assertTrue((output / "vendor/3rdParty/LVGL/lvgl/src/widgets/label/lv_label.c").is_file())
            self.assertFalse((output / "vendor/3rdParty/LVGL/lvgl/src/widgets/chart/lv_chart.c").exists())
            self.assertTrue((output / "vendor/LVSerial/Proto/lvserial.pb.h").is_file())
            self.assertTrue((output / "vendor/3rdParty/JSON/json.hpp").is_file())
            check = [sys.executable, str(output / "tools/verify_inputs.py"), str(output)]
            self.assertEqual(subprocess.run(check, capture_output=True).returncode, 0)
            (output / "projects/hello.pb").write_bytes(b"changed")
            self.assertNotEqual(subprocess.run(check, capture_output=True).returncode, 0)
            with self.assertRaises(ValueError): make_component([source], output)
            make_component([source], output, replace=True)

    def test_advanced_keyboard_standalone_dependencies(self):
        base = profile.repository_root() / "build/esp32-agent-tests"
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as temporary:
            directory = Path(temporary)
            source = directory / "keyboard.pb"
            source.write_bytes(profile.encode_project(project({"advancedKeyboard": {"trieId": ""}})))
            output = directory / "component"
            plan = make_component([source], output)
            for relative in profile.CUSTOM_SOURCES["LVSERIAL_USE_ADVANCED_KEYBOARD"]:
                self.assertTrue((output / "vendor" / relative).is_file(), relative)
            for name in ("include/lv_advanced_keyboard.h", "include/espos_keyboard/TypingView.h", "include/espos_keyboard/Prediction.h",
                         "PredictionStorage.h", "Utf8Fold.h", "UnicodeFold.inc", "en_us_qwerty.generated.h", "README.md", "PROVENANCE.md"):
                self.assertTrue((output / "vendor/LVSerial/Widgets/AdvancedKeyboard" / name).is_file(), name)
            self.assertFalse((output / "vendor/LVSerial/Widgets/AdvancedKeyboard/include/ngram_trie").exists())
            self.assertTrue((output / "licenses/LVSerial/Widgets/AdvancedKeyboard/KeyboardFont.LICENSE.txt").is_file())
            self.assertFalse((output / "vendor/3rdParty/LVGL/lvgl/src/widgets/keyboard/lv_keyboard.c").exists())
            self.assertEqual(len([name for name in plan["sourceFiles"] if name.startswith("LVSerial/Widgets/AdvancedKeyboard/")]), 7)


if __name__ == "__main__": unittest.main()
