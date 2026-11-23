from pathlib import Path
import importlib.util
import json
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "Integrations/ESPOS/templates/Keyboard/AdvancedKeyboard/generate_bundled_models.py"
SPEC = importlib.util.spec_from_file_location("bundled_generator", SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)
CATALOG = ROOT / "UICreator/KeyboardModels/catalog.json"
BUILDER = ROOT / "LVSerial/Widgets/AdvancedKeyboard/layout/build_ngtr_index.py"


def temporary_directory():
    folder = ROOT / "build/espos-bundled-model-tests"
    folder.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix="case-", dir=folder)


class BundledGeneratorTests(unittest.TestCase):
    def test_selected_profiles_are_exact_and_deterministic(self):
        with temporary_directory() as temporary:
            output = Path(temporary) / "models.cpp"
            report = Path(temporary) / "models.json"
            result = GENERATOR.generate(CATALOG, BUILDER, "en-US:16;zh-CN:256", output, report)
            self.assertEqual([(x["language"], x["sizeKiB"]) for x in result["models"]], [("en-US", 16), ("zh-CN", 256)])
            self.assertEqual(result["trieBytes"], 16384 + 262400)
            self.assertEqual(result["indexBytes"], 14232 + 109220)
            self.assertEqual(json.loads(report.read_text()), result)
            self.assertNotIn("French", output.read_text())
            before, mtime = output.read_bytes(), output.stat().st_mtime_ns
            GENERATOR.generate(CATALOG, BUILDER, "en-US:16;zh-CN:256", output, report)
            self.assertEqual(output.read_bytes(), before)
            self.assertEqual(output.stat().st_mtime_ns, mtime)

    def test_none_and_explicit_all(self):
        catalog = json.loads(CATALOG.read_text())
        self.assertEqual(len(GENERATOR.selected_models(catalog, "all")), 16)
        with temporary_directory() as temporary:
            output = Path(temporary) / "none.cpp"
            result = GENERATOR.generate(CATALOG, BUILDER, "none", output)
            self.assertEqual(result["count"], 0)
            self.assertIn("return nullptr", output.read_text())
            self.assertNotIn("trie_0", output.read_text())

    def test_invalid_selection_does_not_replace_output(self):
        with temporary_directory() as temporary:
            output = Path(temporary) / "models.cpp"
            output.write_text("keep existing output")
            for selection in ["xx-XX:16", "en-US:32", "en-US:16;en-US:16", "en-US", ";", "all;en-US:16"]:
                with self.subTest(selection=selection), self.assertRaises(ValueError):
                    GENERATOR.generate(CATALOG, BUILDER, selection, output)
                self.assertEqual(output.read_text(), "keep existing output")

    def test_corrupt_source_does_not_publish_partial_assets(self):
        original = json.loads(CATALOG.read_text())
        records = [item for item in original["models"] if item["language"] == "en-US"]
        with temporary_directory() as temporary:
            folder = Path(temporary)
            (folder / "models").mkdir()
            for record in records:
                data = (CATALOG.parent / "models" / record["filename"]).read_bytes()
                if record["sizeKiB"] == 256:
                    data = data[:-1] + bytes([data[-1] ^ 1])
                (folder / "models" / record["filename"]).write_bytes(data)
            catalog = folder / "catalog.json"
            catalog.write_text(json.dumps({"version": 1, "models": records}))
            output, report = folder / "models.cpp", folder / "models.json"
            output.write_text("existing C++")
            report.write_text("existing report")
            with self.assertRaisesRegex(ValueError, "does not match"):
                GENERATOR.generate(catalog, BUILDER, "en-US:16;en-US:256", output, report)
            self.assertEqual(output.read_text(), "existing C++")
            self.assertEqual(report.read_text(), "existing report")
            self.assertEqual(list(folder.glob("*.tmp")), [])

    def test_catalog_rejects_path_escape_and_duplicate_profiles(self):
        for bad in ["../escape.ngtr", "..\\escape.ngtr", "/escape.ngtr", "nested/file.ngtr"]:
            record = json.loads(CATALOG.read_text())["models"][0]
            record["filename"] = bad
            with self.subTest(filename=bad), self.assertRaises(ValueError):
                GENERATOR.selected_models({"version": 1, "models": [record]}, "all")
        record = json.loads(CATALOG.read_text())["models"][0]
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            GENERATOR.selected_models({"version": 1, "models": [record, record]}, "all")

    def test_false_metadata_and_unsupported_catalog_are_rejected(self):
        record = json.loads(CATALOG.read_text())["models"][0]
        for key, value in [("sizeKiB", True), ("bytes", True), ("sha256", "wrong"), ("name", "line\nbreak"), ("language", "../en")]:
            changed = dict(record)
            changed[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                GENERATOR.selected_models({"version": 1, "models": [changed]}, "all")
        with self.assertRaises(ValueError):
            GENERATOR.selected_models({"version": 2, "models": []}, "none")


if __name__ == "__main__":
    unittest.main()
