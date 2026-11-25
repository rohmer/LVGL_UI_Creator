"""The offline SDK keeps a runtime-only copy of the pinned dependency."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import keyboard_dependency
from feature_plan import repository_root
from trie_assets import index_builder


class OfflineDependencyTests(unittest.TestCase):
    def test_runtime_copy_resolves_without_the_source_checkout_or_git(self):
        root = repository_root()
        original = keyboard_dependency.resolve_root(root)
        with tempfile.TemporaryDirectory() as temporary:
            toolkit = Path(temporary)
            dependency = toolkit / keyboard_dependency.PREFIX
            keyboard_dependency.copy_runtime(root, dependency)
            keyboard_dependency.copy_integration(root, toolkit)
            with patch.dict(os.environ):
                os.environ.pop("ADVANCED_KEYBOARD_ROOT", None)
                self.assertEqual(keyboard_dependency.resolve_root(toolkit), dependency.resolve())
                for name in ("src/PredictionModel.cpp", "src/PredictionBuiltin.inc", "src/models/enUS16.ngtr",
                             "include/lv_advanced_keyboard.h", "CMakeLists.txt", "cmake/AdvancedKeyboardSources.cmake",
                             "LICENSE", "NOTICE.md", "tools/layout/build_ngtr_index.py"):
                    self.assertEqual((dependency / name).read_bytes(), (original / name).read_bytes(), name)
                self.assertEqual(bytes(index_builder(toolkit)((dependency / "src/models/enUS16.ngtr").read_bytes())),
                                 (dependency / "src/models/enUS16.ngix").read_bytes())
            self.assertEqual((toolkit / "advanced-keyboard.lock.json").read_bytes(),
                             (root / "advanced-keyboard.lock.json").read_bytes())
            self.assertTrue((toolkit / "cmake/AdvancedKeyboardDependency.cmake").is_file())
            self.assertFalse((toolkit / "LVSerial/Widgets/AdvancedKeyboard").exists())
            self.assertFalse((dependency / ".git").exists())
            self.assertFalse((dependency / "models/bundled").exists())
            for name in ("ModelCodec", "NativeTrieGenerator", "NgramTrie.Reader", "CorpusWorkbench"):
                self.assertFalse((dependency / "tools" / name).exists(), name)


if __name__ == "__main__": unittest.main()
