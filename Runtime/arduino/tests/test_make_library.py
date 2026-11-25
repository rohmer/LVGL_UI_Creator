import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("arduino_library_generator", TOOLS / "make_library.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class ArduinoLibraryTests(unittest.TestCase):
    def test_xml_parser_is_only_copied_for_explicit_opt_in(self):
        with tempfile.TemporaryDirectory(prefix="arduino-xml-") as temporary:
            root = Path(temporary)
            source = root / "screen.json"
            source.write_text(json.dumps({"runtimeTarget": "9.6", "canvas": {"width": 240, "height": 320},
                "obj": [{"oid": 1, "par": -1, "base": {}, "text": "XML"}]}))
            output = root / "library"
            plan = generator.make_library([source], output, enable_xml=True)
            self.assertTrue(plan["xmlEnabled"])
            self.assertEqual(plan["enabledFeatures"], ["LABEL"])
            self.assertTrue((output / "src/vendor/LVSerial/Serialization/Xml.cpp").is_file())
            self.assertIn("#define LVSERIAL_USE_XML 1", (output / "src/lv_conf.h").read_text())

    def test_profile_headers_embedding_and_replace_guard(self):
        with tempfile.TemporaryDirectory(prefix="arduino-generator-") as temporary:
            root = Path(temporary).resolve()
            project = root / "dashboard.json"
            project.write_text(json.dumps({"formatVersion": 2, "runtimeTarget": "9.6",
                "canvas": {"width": 240, "height": 320, "base": {}},
                "obj": [{"base": {"name": "label"}, "oid": 1, "par": -1, "text": {"text": "Hello"}}]}))
            output = root / "LVGLProtobuf"
            plan = generator.make_library([project], output)
            self.assertFalse(plan["xmlEnabled"])
            self.assertFalse((output / "src/vendor/LVSerial/Serialization/Xml.cpp").exists())
            self.assertIn("#define LVSERIAL_USE_XML 0", (output / "src/lv_conf.h").read_text())
            self.assertEqual(plan["enabledFeatures"], ["LABEL"])
            self.assertTrue((output / "library.properties").is_file())
            self.assertIn("architectures=esp32", (output / "library.properties").read_text())
            self.assertIn("#define LVGL_PB_DEFAULT_HEIGHT 320", (output / "src/lvgl_pb_project_config.h").read_text())
            self.assertTrue((output / "src/vendor/3rdParty/LVGL/lvgl/src/widgets/label/lv_label.c").is_file())
            self.assertFalse((output / "src/vendor/3rdParty/LVGL/lvgl/src/widgets/arc/lv_arc.c").exists())
            self.assertFalse((output / "src/vendor/3rdParty/AdvancedKeyboard/src/Keyboard.cpp").exists())
            self.assertFalse((output / "src/vendor/LVSerial/Widgets/AdvancedKeyboard").exists())
            self.assertFalse(any((output / "src").rglob("lvgl_pb_esp32.cpp")))
            self.assertTrue((output / "examples/ProtobufDisplay/build_opt.h").is_file())
            self.assertIn("-std=gnu++17", (output / "examples/ProtobufDisplay/build_opt.h").read_text())
            self.assertIn("dashboard", (output / "src/lvgl_pb_projects.c").read_text())
            # Forwarders work after extraction: all their quoted includes must
            # resolve inside this library rather than the original checkout.
            for name in ("lvgl.h", "lvgl/lvgl.h", "Serialization/Project.h", "RuntimeCompat.h", "pb.h", "PredictionStorage.h"):
                header = output / "src" / name
                include = header.read_text().split('#include "')[1].split('"')[0]
                target = (header.parent / include).resolve()
                self.assertTrue(target.is_file(), name)
                self.assertIn(output, target.parents, name)
            with self.assertRaises(ValueError): generator.make_library([project], output)
            user_output = root / "existing"
            user_output.mkdir(); (user_output / "keep.txt").write_text("user work")
            with self.assertRaises(ValueError): generator.make_library([project], user_output, replace=True)
            self.assertEqual((user_output / "keep.txt").read_text(), "user work")
            generator.make_library([project], output, embed=False, color_depth=32, replace=True)
            self.assertIn("lvgl_pb_embedded_project_count = 0", (output / "src/lvgl_pb_projects.c").read_text())
            self.assertIn("#define LVGL_PB_COLOR_DEPTH 32", (output / "src/lvgl_pb_project_config.h").read_text())
            self.assertIn("LV_COLOR_FORMAT_ARGB8888", (output / "src/lv_conf.h").read_text())


if __name__ == "__main__": unittest.main()
