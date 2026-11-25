"""Dependency contracts; all Git operations and installs stay mocked."""
from contextlib import redirect_stdout
from copy import deepcopy
import importlib.util
import io
import json
import os
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest.mock import call, patch


CREATOR = Path(__file__).resolve().parents[3]
SCRIPT = Path(__file__).resolve().parents[1] / "dependency.py"
SPEC = importlib.util.spec_from_file_location("advanced_keyboard_dependency_tests", SCRIPT)
DEPENDENCY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = DEPENDENCY
SPEC.loader.exec_module(DEPENDENCY)
REVISION = "a" * 40
REPOSITORY = "https://github.com/rohmer/AdvancedKeyboard.git"


class DependencyTests(unittest.TestCase):
    def setUp(self):
        scratch = CREATOR / "build/advanced-keyboard-dependency-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="case-", dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name).resolve()
        self.assertTrue(self.folder.is_relative_to(scratch.resolve()))
        self.project = self.folder / "creator"
        self.project.mkdir()
        self.lock = {"version": 1, "repository": REPOSITORY, "revision": REVISION,
                     "paths": {"creator": "3rdParty/AdvancedKeyboard", "espos": "third_party/AdvancedKeyboard"}}
        self.write_lock()
        environment = patch.dict(os.environ)
        environment.start()
        self.addCleanup(environment.stop)
        os.environ.pop("ADVANCED_KEYBOARD_ROOT", None)
        # Unexpected subprocess use must fail, including an accidental real clone.
        process = patch.object(DEPENDENCY.subprocess, "run", side_effect=AssertionError("Unexpected external process"))
        self.process = process.start()
        self.addCleanup(process.stop)

    def tearDown(self):
        self.process.assert_not_called()

    def write(self, path, data=b"fixture\n"):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data.encode() if isinstance(data, str) else data)
        return path

    def write_lock(self, data=None, project=None):
        return self.write((project or self.project) / DEPENDENCY.LOCK_NAME, json.dumps(self.lock if data is None else data))

    def checkout(self, root):
        for name in ("CMakeLists.txt", "src/PredictionModel.cpp", "include/lv_advanced_keyboard.h"):
            self.write(root / name, name + "\n")
        return root.resolve()

    def main(self, *arguments, project=None):
        output = io.StringIO()
        argv = [str(SCRIPT), "--project-root", str(project or self.project), *map(str, arguments)]
        with patch.object(sys, "argv", argv), redirect_stdout(output):
            DEPENDENCY.main()
        return json.loads(output.getvalue())

    @staticmethod
    def clean_git(root, *arguments):
        values = {("rev-parse", "--show-toplevel"): str(Path(root).resolve()),
                  ("rev-parse", "HEAD"): REVISION,
                  ("status", "--porcelain", "--untracked-files=all"): ""}
        if arguments not in values:
            raise AssertionError("Unexpected Git command: " + repr(arguments))
        return values[arguments]

    @staticmethod
    def snapshot(root):
        return {path.relative_to(root).as_posix(): path.read_bytes()
                for path in root.rglob("*") if path.is_file()}

    def test_explicit_and_environment_checkouts_override_the_default_without_git(self):
        default = self.checkout(self.project / self.lock["paths"]["creator"])
        explicit = self.checkout(self.folder / "development")
        environment = self.checkout(self.folder / "environment")
        os.environ["ADVANCED_KEYBOARD_ROOT"] = str(environment)
        self.assertEqual(DEPENDENCY.resolve_root(self.project), environment)
        self.assertEqual(DEPENDENCY.resolve_root(self.project, explicit), explicit)
        result = self.main("--root", explicit)
        self.assertEqual(Path(result["root"]), explicit)
        self.assertEqual(result["status"], "resolved")
        os.environ.pop("ADVANCED_KEYBOARD_ROOT")
        self.assertEqual(DEPENDENCY.resolve_root(self.project), default)

    def test_lock_selected_path_and_espos_project_kind_are_respected(self):
        changed = deepcopy(self.lock)
        changed["paths"]["creator"] = "dependencies/native-keyboard"
        self.write_lock(changed)
        selected = self.checkout(self.project / changed["paths"]["creator"])
        self.assertEqual(DEPENDENCY.resolve_root(self.project), selected)
        peer = self.folder / "espos"
        self.write(peer / "espos.yaml", "name: fixture\n")
        self.write_lock(project=peer)
        selected = self.checkout(peer / self.lock["paths"]["espos"])
        self.assertEqual(DEPENDENCY.project_kind(peer), "espos")
        self.assertEqual(DEPENDENCY.resolve_root(peer), selected)

    def test_missing_default_never_uses_an_old_copied_widget_tree(self):
        for kind, legacy in (("creator", "LVSerial/Widgets/AdvancedKeyboard"),
                             ("espos", "Keyboard/AdvancedKeyboard")):
            with self.subTest(kind=kind):
                project = self.folder / (kind + "-missing")
                project.mkdir()
                self.write_lock(project=project)
                if kind == "espos":
                    self.write(project / "espos.yaml")
                self.write(project / legacy / "PredictionModel.cpp", "old maintained copy")
                self.write(project / legacy / "include/lv_advanced_keyboard.h")
                before = self.snapshot(project)
                with self.assertRaisesRegex(FileNotFoundError, "initialize its pinned checkout"):
                    DEPENDENCY.resolve_root(project)
                self.assertEqual(self.snapshot(project), before)

    def test_lock_rejects_invalid_repository_revision_schema_and_portable_paths(self):
        changes = [("version", True), ("version", 2), ("repository", "https://example.invalid/other.git"),
                   ("revision", "a" * 39), ("revision", "a" * 41), ("revision", "A" * 40),
                   ("revision", None), ("revision", 12), ("paths", None)]
        for key, value in changes:
            with self.subTest(key=key, value=value):
                changed = deepcopy(self.lock)
                changed[key] = value
                self.write_lock(changed)
                with self.assertRaises(ValueError):
                    DEPENDENCY.load_lock(self.project)
        for value in ("", ".", "../outside", "vendor/../../outside", r"vendor\..\outside",
                      "/outside", "C:/outside", r"C:\outside", r"\\server\share", None):
            with self.subTest(path=value):
                changed = deepcopy(self.lock)
                changed["paths"]["creator"] = value
                self.write_lock(changed)
                with self.assertRaises(ValueError):
                    DEPENDENCY.load_lock(self.project)

    def test_runtime_closure_contains_device_sources_without_offline_tools_or_catalog(self):
        root = self.checkout(self.folder / "runtime")
        included = {"CMakeLists.txt", "src/PredictionModel.cpp", "include/lv_advanced_keyboard.h",
                    "src/PredictionBuiltin.inc", "src/models/enUS16.ngtr", "src/models/enUS16.ngix",
                    "src/models/builtin.json", "include/espos_keyboard/Keyboard.h", "cmake/AdvancedKeyboardSources.cmake",
                    "LICENSE", "NOTICE.md", "idf_component.yml", "library.properties", "tools/layout/build_ngtr_index.py"}
        excluded = {"tools/ModelCodec/RadixModelBuilder.cpp", "tools/NativeTrieGenerator/source/Generator.cpp",
                    "tools/CorpusWorkbench/Main.cs", "models/bundled/catalog.json", "models/bundled/models/en-US-16.ngtr",
                    "src/build/cached.cpp", "src/.git/private", "include/bin/cached.h", "cmake/obj/cached.cmake"}
        for name in included | excluded:
            self.write(root / name, name)
        names = DEPENDENCY.runtime_files(root)
        self.assertEqual(names, sorted(included))
        output = self.folder / "generated-project/AdvancedKeyboard"
        unknown = self.write(output / "user-settings.json", "preserve user settings")
        self.assertEqual(DEPENDENCY.copy_runtime(root, output), names)
        for name in names:
            self.assertEqual((output / name).read_bytes(), (root / name).read_bytes())
        before = {name: (output / name).stat().st_mtime_ns for name in names}
        DEPENDENCY.copy_runtime(root, output)
        self.assertEqual({name: (output / name).stat().st_mtime_ns for name in names}, before)
        self.assertEqual(unknown.read_text(), "preserve user settings")
        self.assertEqual({path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()},
                         included | {"user-settings.json"})

    def test_runtime_copy_rejects_maintained_source_destinations(self):
        for relative in (".", "src/generated", "include/generated", "cmake/generated"):
            with self.subTest(relative=relative):
                root = self.checkout(self.folder / ("guard-" + relative.replace("/", "-")))
                with patch.object(DEPENDENCY.shutil, "copyfile") as copied, self.assertRaises(ValueError):
                    DEPENDENCY.copy_runtime(root, root / relative)
                copied.assert_not_called()

    def test_runtime_source_symlinks_cannot_escape_the_checkout(self):
        outside = self.write(self.folder / "outside.txt", "private fixture")
        for number, relative in enumerate(("src/escape.cpp", "tools/layout/build_ngtr_index.py")):
            with self.subTest(relative=relative):
                root = self.checkout(self.folder / ("linked-" + str(number)))
                link = root / relative
                link.parent.mkdir(parents=True, exist_ok=True)
                try:
                    link.symlink_to(outside)
                except (OSError, NotImplementedError):
                    self.skipTest("File symlinks are unavailable")
                with self.assertRaises(ValueError):
                    DEPENDENCY.runtime_files(root)
                self.assertEqual(outside.read_text(), "private fixture")

    def test_clean_checkout_verification_uses_only_its_own_git_root(self):
        root = self.checkout(self.folder / "clean")
        with patch.object(DEPENDENCY, "git", side_effect=self.clean_git) as git:
            DEPENDENCY.check_checkout(root, self.lock)
        self.assertEqual(git.call_args_list, [call(root, "rev-parse", "--show-toplevel"), call(root, "rev-parse", "HEAD"),
                                             call(root, "status", "--porcelain", "--untracked-files=all")])
        with patch.object(DEPENDENCY, "git", return_value=str(self.folder)) as git:
            with self.assertRaisesRegex(ValueError, "ancestor repository"):
                DEPENDENCY.check_checkout(root, self.lock)
        git.assert_called_once_with(root, "rev-parse", "--show-toplevel")

    def test_existing_dirty_or_different_revision_is_preserved_without_switching(self):
        root = self.checkout(self.project / self.lock["paths"]["creator"])
        self.write(root / "user-draft.txt", "keep draft")
        before = self.snapshot(root)
        for dirty in (False, True):
            with self.subTest(dirty=dirty):
                def answer(selected, *arguments):
                    self.assertEqual(Path(selected).resolve(), root)
                    if arguments == ("rev-parse", "HEAD"):
                        return REVISION if dirty else "b" * 40
                    if arguments == ("status", "--porcelain", "--untracked-files=all"):
                        return " M src/PredictionModel.cpp\n?? user-draft.txt"
                    return self.clean_git(selected, *arguments)
                with patch.object(DEPENDENCY, "git", side_effect=answer):
                    with self.assertRaisesRegex(ValueError, "local edits|pinned revision"):
                        self.main("--init")
                self.assertEqual(self.snapshot(root), before)

    def test_init_rejects_explicit_development_root_without_modification(self):
        root = self.checkout(self.folder / "development")
        before = self.snapshot(root)
        with patch.object(DEPENDENCY, "git") as git, self.assertRaisesRegex(ValueError, "explicit pinned path"):
            self.main("--init", "--root", root)
        git.assert_not_called()
        self.assertEqual(self.snapshot(root), before)

    def test_init_never_uses_the_projects_ancestor_repository(self):
        self.write(self.project / ".git", "fixture gitdir marker")
        before = self.snapshot(self.project)
        with patch.object(DEPENDENCY, "git", return_value=str(self.folder)) as git:
            with self.assertRaisesRegex(ValueError, "explicit project directory"):
                self.main("--init")
        git.assert_called_once_with(self.project, "rev-parse", "--show-toplevel")
        self.assertEqual(self.snapshot(self.project), before)

    def test_missing_espos_checkout_uses_a_mocked_clone_and_exact_detached_revision(self):
        self.write(self.project / "espos.yaml")
        destination = self.project / self.lock["paths"]["espos"]
        def clone(command):
            self.assertEqual(command, ["git", "clone", "--no-checkout", REPOSITORY, str(destination)])
            self.checkout(destination)
            self.write(destination / ".git", "fixture gitdir marker")
            return SimpleNamespace(returncode=0)
        def answer(root, *arguments):
            self.assertEqual(Path(root).resolve(), destination.resolve())
            if arguments == ("checkout", "--detach", REVISION):
                return ""
            return self.clean_git(root, *arguments)
        with patch.object(DEPENDENCY.subprocess, "run", side_effect=clone) as process, \
             patch.object(DEPENDENCY, "git", side_effect=answer) as git:
            result = self.main("--init")
        process.assert_called_once()
        self.assertIn(call(destination, "checkout", "--detach", REVISION), git.call_args_list)
        self.assertEqual(result["kind"], "espos")
        self.assertEqual(result["status"], "verified")
        self.assertEqual(Path(result["root"]), destination)
        self.assertFalse((self.project / ".git").exists())

    def test_uninitialized_submodule_absent_or_empty_path_uses_only_submodule_update(self):
        for empty in (False, True):
            with self.subTest(empty=empty):
                project = self.folder / ("submodule-" + str(empty))
                project.mkdir()
                self.write_lock(project=project)
                self.write(project / ".git", "fixture project gitdir marker")
                relative = self.lock["paths"]["creator"]
                destination = project / relative
                if empty:
                    destination.mkdir(parents=True)
                def answer(root, *arguments):
                    if Path(root).resolve() == project:
                        if arguments == ("rev-parse", "--show-toplevel"):
                            return str(project)
                        if arguments == ("ls-files", "--stage", "--", relative):
                            return f"160000 {REVISION} 0\t{relative}"
                        if arguments == ("submodule", "update", "--init", "--", relative):
                            self.checkout(destination)
                            self.write(destination / ".git", "fixture submodule gitdir marker")
                            return ""
                        raise AssertionError(arguments)
                    self.assertEqual(Path(root).resolve(), destination)
                    return self.clean_git(root, *arguments)
                with patch.object(DEPENDENCY, "git", side_effect=answer) as git:
                    result = self.main("--init", project=project)
                updates = [entry for entry in git.call_args_list if entry.args[1] == "submodule"]
                self.assertEqual(updates, [call(project, "submodule", "update", "--init", "--", relative)])
                self.assertEqual(Path(result["root"]), destination)
                self.assertEqual(result["status"], "verified")


if __name__ == "__main__":
    unittest.main()
