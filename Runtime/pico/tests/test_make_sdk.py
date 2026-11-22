import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from make_sdk import BOARDS, MARKER, generate, update_profile_manifest


def project():
    return {"formatVersion": 2, "runtimeTarget": "9.6", "canvas": {"width": 240, "height": 320},
            "obj": [{"oid": 1, "par": -1, "base": {}, "text": "Pico display"}]}


class PicoGenerationTests(unittest.TestCase):
    def test_xml_opt_in_keeps_the_same_minimal_widget_profile(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "screen.json"
            source.write_text(json.dumps(project()))
            output = root / "xml-pico"
            generate([source], output, enable_xml=True, archive=False)
            plan = json.loads((output / "embedded/features.json").read_text())
            self.assertTrue(plan["xmlEnabled"])
            self.assertEqual(plan["enabledFeatures"], ["LABEL"])
            self.assertIn('XML input codec" ON)', (output / "embedded/CMakeLists.txt").read_text())
            self.assertTrue((output / "embedded/optional/xml/Xml.cpp").is_file())

    def test_board_profiles_minimum_sources_inputs_and_replacement(self):
        for board, platform in (("pico", "rp2040"), ("pico2", "rp2350-arm-s")):
            with self.subTest(board=board), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = root / "display.json"; source.write_text(json.dumps(project()))
                output = root / "output"
                archive = generate([source], output, board=board)
                self.assertEqual(Path(str(archive) + ".sha256").read_text().split()[0], hashlib.sha256(archive.read_bytes()).hexdigest())
                with zipfile.ZipFile(archive) as zipped: self.assertIsNone(zipped.testzip())
                metadata = json.loads((output / MARKER).read_text())
                self.assertEqual(metadata["board"], board); self.assertEqual(metadata["platform"], platform)
                plan = json.loads((output / "embedded/features.json").read_text())
                self.assertFalse(plan["xmlEnabled"])
                self.assertTrue((output / "embedded/optional/xml/Xml.cpp").is_file())
                self.assertFalse((output / "embedded/vendor/LVSerial/Serialization/Xml.cpp").exists())
                self.assertIn('XML input codec" OFF)', (output / "embedded/CMakeLists.txt").read_text())
                self.assertLess(plan["lvglSourceFileCount"], plan["fullLvglSourceFileCount"])
                self.assertTrue((output / "embedded/vendor/Runtime/common/Runtime.cpp").is_file())
                self.assertFalse((output / "embedded/vendor/3rdParty/LVGL/lvgl/src/widgets/chart/lv_chart.c").exists())
                self.assertFalse((output / "embedded/src/lvgl_pb_esp32.cpp").exists())
                cmake = (output / "CMakeLists.txt").read_text()
                self.assertIn(f'set(PICO_BOARD "{board}"', cmake)
                self.assertIn("pico_add_extra_outputs(lvgl_pico_display)", cmake)
                self.assertIn("PICO_CXX_ENABLE_EXCEPTIONS", cmake)
                self.assertIn("PICO_STACK_SIZE", cmake)
                for name, checksum in metadata["files"].items():
                    self.assertEqual(hashlib.sha256((output / name).read_bytes()).hexdigest(), checksum, name)
                command = [sys.executable, str(output / "embedded/tools/verify_inputs.py"), str(output / "embedded")]
                self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
                (output / "embedded/projects/display.pb").write_bytes(b"modified")
                self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
                with self.assertRaises(ValueError): generate([source], output, board=board)
                generate([source], output, board=board, replace=True, archive=False)

    def test_true_c_profile_does_not_include_protobuf_or_json(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "display.json"; source.write_text(json.dumps(project()))
            ui = root / "source"; ui.mkdir()
            for name in ("ui.c", "ui.h", "ui_assets.c", "CMakeLists.txt"): (ui / name).write_text("/* editor output */\n")
            with self.assertRaises(ValueError): generate([source], ui / "nested", generated_c=ui, archive=False)
            output = root / "c-output"
            generate([source], output, board="pico2", generated_c=ui, archive=False)
            self.assertTrue((output / "ui/ui.c").is_file())
            self.assertFalse((output / "src/lvgl_pb_pico.cpp").exists())
            self.assertFalse((output / "include/lvgl_pb_pico.h").exists())
            for name in ("Runtime", "LVSerial", "3rdParty/JSON", "3rdParty/nanopb"):
                self.assertFalse((output / "embedded/vendor" / name).exists(), name)
            plan = json.loads((output / "embedded/features.json").read_text())
            self.assertTrue(all(name.startswith("3rdParty/LVGL/") for name in plan["sourceFiles"]))
            self.assertIn("add_subdirectory(ui generated_ui)", (output / "CMakeLists.txt").read_text())
            second = root / "creator-output"
            generate([source], second, generated_c=ui, copy_ui=False, archive=False)
            self.assertFalse((second / "ui").exists())
            (second / "ui").mkdir(); (second / "ui/ui.c").write_text("/* supplied by creator */")
            update_profile_manifest(second)
            self.assertIn("ui/ui.c", json.loads((second / MARKER).read_text())["files"])

    def test_bad_board_and_output_source_guards(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "display.json"; source.write_text(json.dumps(project()))
            with self.assertRaises(ValueError): generate([source], root / "output", board="linux")
            with self.assertRaises(ValueError): generate([source], TOOLS / "bad-output")


if __name__ == "__main__": unittest.main()
