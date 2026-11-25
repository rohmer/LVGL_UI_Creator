from pathlib import Path
import hashlib
import importlib.util
import json
import re
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
DEPENDENCY_SPEC = importlib.util.spec_from_file_location("espos_dependency", ROOT / "Integrations/AdvancedKeyboard/dependency.py")
DEPENDENCY = importlib.util.module_from_spec(DEPENDENCY_SPEC)
DEPENDENCY_SPEC.loader.exec_module(DEPENDENCY)
KEYBOARD_ROOT = DEPENDENCY.resolve_root(ROOT)
SCRIPT = KEYBOARD_ROOT / "tools/models/generate_bundled_models.py"
SPEC = importlib.util.spec_from_file_location("bundled_generator", SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)
CATALOG = KEYBOARD_ROOT / "models/bundled/catalog.json"
BUILDER = KEYBOARD_ROOT / "tools/layout/build_ngtr_index.py"
BUILTIN = KEYBOARD_ROOT / "src/models"
BUILDER_SPEC = importlib.util.spec_from_file_location("bundled_index", BUILDER)
INDEX = importlib.util.module_from_spec(BUILDER_SPEC)
BUILDER_SPEC.loader.exec_module(INDEX)


def temporary_directory():
    folder = ROOT / "build/espos-bundled-model-tests"
    folder.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix="case-", dir=folder)


class BundledGeneratorTests(unittest.TestCase):
    def check_radix_profile(self, record, data, reserved_identity=None):
        self.assertEqual(record["formatVersion"], 4)
        self.assertEqual(record["indexVersion"], 2)
        self.assertEqual(struct.unpack_from("<4sIII", data),
                         (b"NGTR", 4, record["physicalNodeCount"], record["nodeCount"]))
        self.assertLess(record["physicalNodeCount"], record["nodeCount"])
        self.assertEqual(record["sourceNodeCount"], record["nodeCount"])
        self.assertEqual(len(data), record["bytes"])
        self.assertEqual(hashlib.sha256(data).hexdigest(), record["sha256"])
        self.assertEqual(record["sourceFormatVersion"], 3)
        self.assertGreaterEqual(record["sourceBytes"], 20)
        self.assertRegex(record["sourceSHA256"], r"\A[0-9a-f]{64}\Z")
        identity = 14695981039346656037
        for byte in data:
            identity = ((identity ^ byte) * 1099511628211) & ((1 << 64) - 1)
        self.assertEqual(record["projectTrieId"], reserved_identity or f"trie-{identity:016x}")
        index = bytes(INDEX.build_index(data))  # Full graph and canonical-varint validation.
        self.assertEqual(struct.unpack_from("<4sIII", index),
                         (b"NGIX", 2, len(data), record["physicalNodeCount"]))
        self.assertEqual(len(index), record["indexBytes"])
        self.assertEqual(hashlib.sha256(index).hexdigest(), record["indexSHA256"])
        return index

    def test_builtin_english_assets_and_embedded_arrays_are_checked_radix_v4(self):
        record = json.loads((BUILTIN / "builtin.json").read_text())
        self.assertEqual(record["id"], "builtin-en-us")
        self.assertEqual(record["language"], "en-US")
        data = (BUILTIN / "enUS16.ngtr").read_bytes()
        index = self.check_radix_profile(record, data, "builtin-en-us")
        self.assertEqual((BUILTIN / "enUS16.ngix").read_bytes(), index)
        source = (BUILTIN.parent / "PredictionBuiltin.inc").read_text()
        for symbol, expected in (("kBuiltinTrie", data), ("kBuiltinIndex", index)):
            array = re.search(rf"\b{symbol}\[\]\s*=\s*\{{(.*?)\}};", source, re.S)
            self.assertIsNotNone(array)
            actual = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", array.group(1)))
            self.assertEqual(actual, expected)

    def test_every_bundled_profile_is_checked_radix_v4(self):
        catalog = json.loads(CATALOG.read_text())
        records = GENERATOR.selected_models(catalog, "all")
        languages = {"en-US", "es-ES", "fr-FR", "de-DE", "el-GR", "ar-SA", "hi-IN", "zh-CN"}
        self.assertEqual({(record["language"], record["sizeKiB"]) for record in records},
                         {(language, size) for language in languages for size in (16, 256)})
        self.assertEqual(len(records), 16)
        self.assertEqual(len({record["id"] for record in records}), len(records))
        self.assertEqual(len({record["projectTrieId"] for record in records}), len(records))
        for record in records:
            with self.subTest(language=record["language"], sizeKiB=record["sizeKiB"]):
                data = (CATALOG.parent / "models" / record["filename"]).read_bytes()
                self.check_radix_profile(record, data)
                # The original profiles remain nominal selections; exact payload
                # and index sizes come from the losslessly compressed assets.
                self.assertEqual(record["filename"], f'{record["language"]}-{record["sizeKiB"]}.ngtr')

    def test_selected_profiles_are_exact_and_deterministic(self):
        with temporary_directory() as temporary:
            output = Path(temporary) / "models.cpp"
            report = Path(temporary) / "models.json"
            result = GENERATOR.generate(CATALOG, BUILDER, "en-US:16;zh-CN:256", output, report)
            self.assertEqual([(x["language"], x["sizeKiB"]) for x in result["models"]], [("en-US", 16), ("zh-CN", 256)])
            selected = GENERATOR.selected_models(json.loads(CATALOG.read_text()), "en-US:16;zh-CN:256")
            self.assertEqual(result["trieBytes"], sum(record["bytes"] for record in selected))
            self.assertEqual(result["indexBytes"], sum(record["indexBytes"] for record in selected))
            for number, record in enumerate(selected):
                data = (CATALOG.parent / "models" / record["filename"]).read_bytes()
                index = bytes(INDEX.build_index(data))
                for label, expected in (("trie", data), ("index", index)):
                    array = re.search(rf"const unsigned char {label}_{number}\[\] = \{{(.*?)\}};", output.read_text(), re.S)
                    self.assertIsNotNone(array)
                    actual = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", array.group(1)))
                    self.assertEqual(actual, expected)
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

    def test_forged_radix_metadata_does_not_publish_assets(self):
        original = GENERATOR.selected_models(json.loads(CATALOG.read_text()), "en-US:16")[0]
        with temporary_directory() as temporary:
            folder = Path(temporary)
            (folder / "models").mkdir()
            (folder / "models" / original["filename"]).write_bytes(
                (CATALOG.parent / "models" / original["filename"]).read_bytes())
            catalog = folder / "catalog.json"
            output, report = folder / "models.cpp", folder / "models.json"
            for key, value in (("formatVersion", 3), ("indexVersion", 1), ("indexSHA256", "0" * 64),
                               ("physicalNodeCount", original["physicalNodeCount"] + 1),
                               ("nodeCount", original["nodeCount"] + 1)):
                record = dict(original)
                record[key] = value
                catalog.write_text(json.dumps({"version": 1, "models": [record]}))
                output.write_text("existing C++")
                report.write_text("existing report")
                with self.subTest(key=key), self.assertRaises(ValueError):
                    GENERATOR.generate(catalog, BUILDER, "en-US:16", output, report)
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
