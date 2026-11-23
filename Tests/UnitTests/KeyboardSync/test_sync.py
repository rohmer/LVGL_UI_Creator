import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("keyboard_sync", ROOT / "Integrations/ESPOS/sync_keyboard.py")
sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sync)


class KeyboardSyncTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.creator, self.peer = self.base / "creator", self.base / "espos"
        self.creator.mkdir(); self.peer.mkdir()
        (self.creator / "core").mkdir()
        self.manifest = self.creator / "manifest.json"
        self.baseline = self.creator / "baseline.json"
        self.config = {"version": 1, "trees": [{"source": "core", "target": "Keyboard/AdvancedKeyboard", "include": ["**"], "exclude": []}],
                       "files": [], "tombstones": []}
        self.save_manifest()

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.config), encoding="utf-8")

    def put(self, side, name, data=b"int value = 1;\n"):
        path = (self.creator if side == "creator" else self.peer) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def engine(self, peer=True):
        return sync.Sync(self.creator, self.peer if peer else None, self.manifest, self.baseline)

    def apply(self, direction="push", bootstrap=False):
        engine = self.engine()
        plan = engine.plan(direction, bootstrap)
        self.assertFalse(plan["conflicts"], plan["conflicts"])
        return engine.apply(plan)

    def seed(self):
        self.put("creator", "core/a.cpp")
        self.put("creator", "core/b.h", b"#pragma once\n")
        self.apply()

    def test_default_check_and_push_dry_run_do_not_write(self):
        self.put("creator", "core/a.cpp")
        before = list(self.peer.rglob("*"))
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = sync.main(["--creator", str(self.creator), "--espos", str(self.peer),
                                "--manifest", str(self.manifest), "--baseline", str(self.baseline)])
        self.assertEqual(status, 1)
        summary = json.loads(output.getvalue())
        self.assertEqual(summary["status"], "differences")
        self.assertEqual(summary["files"], 1)
        self.assertEqual(summary["differences"], [{"source": "core/a.cpp", "targets": ["Keyboard/AdvancedKeyboard/a.cpp"]}])
        self.assertEqual(summary["conflicts"], [])
        self.assertNotIn("snapshots", summary)
        self.assertNotIn("nextBaseline", summary)
        plan = self.engine().plan("push")
        self.assertEqual(len(plan["actions"]), 1)
        self.assertEqual(list(self.peer.rglob("*")), before)
        self.assertFalse(self.baseline.exists())
        self.assertFalse((self.creator / "build").exists())

    def test_default_check_after_sync_is_a_compact_success(self):
        self.seed()
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = sync.main(["--creator", str(self.creator), "--espos", str(self.peer),
                                "--manifest", str(self.manifest), "--baseline", str(self.baseline)])
        summary = json.loads(output.getvalue())
        self.assertEqual(status, 0)
        self.assertEqual(summary, {"status": "in-sync", "operation": "check", "files": 2,
                                   "changedFiles": 0, "differences": [], "conflicts": []})

    def test_push_binary_preservation_and_idempotence(self):
        binary = b"NGTR\x00\xff\r\n\x01" * 200
        self.put("creator", "core/model.ngtr", binary)
        self.put("creator", "core/a.cpp", b"// UTF-8 \xce\xa9\r\n")
        result = self.apply()
        self.assertEqual(result["changedFiles"], 2)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/model.ngtr").read_bytes(), binary)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"// UTF-8 \xce\xa9\n")
        target = self.peer / "Keyboard/AdvancedKeyboard/model.ngtr"
        before = target.stat().st_mtime_ns
        self.assertEqual(self.apply()["changedFiles"], 0)
        self.assertEqual(target.stat().st_mtime_ns, before)
        self.assertEqual(self.engine().plan()["differences"], [])

    def test_saved_plan_has_full_review_details_with_concise_console_output(self):
        self.put("creator", "core/a.cpp")
        plan_path = self.creator / "build/reviewed-plan.json"
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = sync.main(["--creator", str(self.creator), "--espos", str(self.peer),
                                "--manifest", str(self.manifest), "--baseline", str(self.baseline),
                                "--push", "--dry-run", "--plan-out", str(plan_path)])
        summary = json.loads(output.getvalue())
        reviewed = json.loads(plan_path.read_text(encoding="utf-8"))
        self.assertEqual(status, 0)
        self.assertEqual(summary["status"], "planned")
        self.assertEqual(summary["changedFiles"], 1)
        self.assertNotIn("snapshots", summary)
        self.assertIn("snapshots", reviewed)
        self.assertEqual(summary["planSha256"], reviewed["planSha256"])
        self.assertFalse(self.baseline.exists())
        self.assertEqual(list(self.peer.rglob("*")), [])

    def test_cross_platform_line_endings_match_baseline(self):
        self.seed()
        self.put("creator", "core/a.cpp", b"int value = 1;\r\n")
        self.put("espos", "Keyboard/AdvancedKeyboard/b.h", b"#pragma once\r\n")
        self.assertEqual(self.engine().plan()["differences"], [])
        self.assertEqual(self.engine(False).check_source()["status"], "in-sync")

    def test_source_only_ci_detects_new_changed_removed_and_manifest(self):
        self.seed()
        self.assertEqual(self.engine(False).check_source()["status"], "in-sync")
        self.put("creator", "core/new.inc", b"new\n")
        self.assertEqual(self.engine(False).check_source()["status"], "drift")
        self.apply()
        (self.creator / "core/a.cpp").unlink()
        self.assertEqual(self.engine(False).check_source()["status"], "drift")
        self.apply()
        self.config["trees"][0]["exclude"] = ["README.md"]
        self.save_manifest()
        self.assertEqual(self.engine(False).check_source()["status"], "drift")

    def test_pull_updates_creator_and_unchanged_duplicate_views(self):
        (self.creator / "core/layout").mkdir()
        self.config["trees"].append({"source": "core/layout", "target": "Keyboards", "include": ["*.json"], "exclude": []})
        self.save_manifest()
        self.put("creator", "core/layout/en.json", b'{"keys":1}\n')
        self.apply()
        self.put("espos", "Keyboards/en.json", b'{"keys":2}\n')
        result = self.apply("pull")
        self.assertEqual(result["changedFiles"], 2)
        expected = b'{"keys":2}\n'
        self.assertEqual((self.creator / "core/layout/en.json").read_bytes(), expected)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/layout/en.json").read_bytes(), expected)

    def test_new_peer_file_can_be_pulled(self):
        self.seed()
        self.put("espos", "Keyboard/AdvancedKeyboard/new.h", b"new\n")
        self.assertTrue(self.engine().plan("push")["conflicts"])
        self.apply("pull")
        self.assertEqual((self.creator / "core/new.h").read_bytes(), b"new\n")

    def test_extra_espos_keyboard_profiles_are_not_adopted_or_deleted(self):
        (self.creator / "core/layout").mkdir()
        self.config["trees"].append({"source": "core/layout", "target": "Keyboards", "include": ["*.json"], "exclude": [],
                                      "discoverPeerAdditions": False})
        self.save_manifest(); self.put("creator", "core/layout/en.json", b"{}\n")
        extra = self.put("espos", "Keyboards/es-MX.json", b"extra user profile\n")
        self.apply()
        self.assertEqual(self.engine().plan()["differences"], [])
        self.assertEqual(extra.read_bytes(), b"extra user profile\n")
        self.assertFalse((self.creator / "core/layout/es-MX.json").exists())

    def test_wrong_direction_and_two_sided_conflict_write_nothing(self):
        self.seed()
        before = self.baseline.read_bytes()
        self.put("espos", "Keyboard/AdvancedKeyboard/a.cpp", b"peer change\n")
        self.put("creator", "core/b.h", b"creator change\n")
        engine = self.engine(); plan = engine.plan("push")
        self.assertTrue(plan["actions"])
        self.assertTrue(plan["conflicts"])
        with self.assertRaises(sync.SyncError): engine.apply(plan)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/b.h").read_bytes(), b"#pragma once\n")
        self.assertEqual(self.baseline.read_bytes(), before)
        self.put("creator", "core/a.cpp", b"competing change\n")
        self.assertTrue(self.engine().plan("pull")["conflicts"])

    def test_competing_duplicate_peer_changes_are_conflicts(self):
        (self.creator / "core/layout").mkdir()
        self.config["trees"].append({"source": "core/layout", "target": "Keyboards", "include": ["*.json"], "exclude": []})
        self.save_manifest()
        self.put("creator", "core/layout/en.json", b"{}\n"); self.apply()
        self.put("espos", "Keyboards/en.json", b"first\n")
        self.put("espos", "Keyboard/AdvancedKeyboard/layout/en.json", b"second\n")
        self.assertTrue(self.engine().plan("pull")["conflicts"])

    def test_managed_deletion_and_unknown_user_data_preservation(self):
        self.seed()
        personal = self.put("espos", "my-corpora/personal.ngtr", b"personal")
        extra = self.put("espos", "Keyboard/board-driver.cpp", b"board")
        (self.creator / "core/a.cpp").unlink()
        self.apply()
        self.assertFalse((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").exists())
        self.assertEqual(personal.read_bytes(), b"personal")
        self.assertEqual(extra.read_bytes(), b"board")
        (self.peer / "Keyboard/AdvancedKeyboard/b.h").unlink()
        self.apply("pull")
        self.assertFalse((self.creator / "core/b.h").exists())

    def test_bootstrap_requires_explicit_choice_and_preserves_peer_only_files(self):
        self.put("creator", "core/a.cpp", b"creator\n")
        self.put("espos", "Keyboard/AdvancedKeyboard/a.cpp", b"old adapter\n")
        self.assertTrue(self.engine().plan("push")["conflicts"])
        self.apply("push", bootstrap=True)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"creator\n")
        self.put("espos", "Keyboard/AdvancedKeyboard/user.cpp", b"private\n")
        self.assertTrue(self.engine().plan("push", bootstrap=True)["conflicts"])

    def test_tombstones_require_known_content_and_never_delete_directories(self):
        old = b"known old\n"
        self.config["tombstones"] = [{"target": "Keyboard/retired.cpp", "expectedSha256": sync.sha(old)}]
        self.save_manifest(); self.put("espos", "Keyboard/retired.cpp", old)
        self.apply(); self.assertFalse((self.peer / "Keyboard/retired.cpp").exists())
        self.put("espos", "Keyboard/retired.cpp", b"user edited\n")
        self.assertTrue(self.engine().plan("push")["conflicts"])
        (self.peer / "Keyboard/retired.cpp").unlink(); (self.peer / "Keyboard/retired.cpp").mkdir()
        with self.assertRaises(sync.SyncError): self.engine().plan("push")

    def test_invalid_paths_and_case_collisions_are_rejected(self):
        for name in [".", "../outside", "/absolute", "C:relative", "C:/absolute", "\\\\server\\share", ".git/config",
                     "folder/file:stream", "folder/NUL.txt", "folder/trailing.", "folder/trailing "]:
            with self.subTest(path=name), self.assertRaises(sync.SyncError): sync.relative(name)
        self.config["files"] = [{"source": "core/a.cpp", "target": "A.cpp"}, {"source": "core/b.h", "target": "a.cpp"}]
        self.put("creator", "core/a.cpp"); self.put("creator", "core/b.h")
        self.save_manifest()
        with self.assertRaises(sync.SyncError): self.engine().plan()

    def test_root_overlap_and_external_baseline_are_rejected(self):
        with self.assertRaises(sync.SyncError): sync.Sync(self.creator, self.creator, self.manifest, self.baseline)
        nested = self.creator / "nested"; nested.mkdir()
        with self.assertRaises(sync.SyncError): sync.Sync(self.creator, nested, self.manifest, self.baseline)
        with self.assertRaises(sync.SyncError): sync.Sync(self.creator, self.peer, self.manifest, self.peer / "baseline.json")

    def test_isolated_staging_cannot_advance_the_actual_baseline(self):
        self.put("creator", "core/a.cpp")
        staged = self.creator / "build/staging-espos"; staged.mkdir(parents=True)
        state = self.creator / "build/staging-state.json"
        engine = sync.Sync(self.creator, staged, self.manifest, state, staging=True)
        engine.apply(engine.plan("push"))
        self.assertTrue(state.is_file())
        self.assertFalse(self.baseline.exists())
        self.assertEqual((staged / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"int value = 1;\n")
        with self.assertRaises(sync.SyncError): sync.Sync(self.creator, staged, self.manifest, self.baseline, staging=True)
        with self.assertRaises(sync.SyncError): sync.Sync(self.creator, self.peer, self.manifest, state, staging=True)

    def test_actual_symlink_escape_is_rejected(self):
        outside = self.base / "outside"; outside.mkdir(); (outside / "a.cpp").write_bytes(b"private")
        link = self.creator / "core/escape"
        try: link.symlink_to(outside, target_is_directory=True)
        except (OSError, NotImplementedError): self.skipTest("Creating symlinks is unavailable")
        with self.assertRaises(sync.SyncError): self.engine().plan("push")

    def test_raw_source_or_target_race_after_plan_refuses_without_writes(self):
        self.seed()
        self.put("creator", "core/a.cpp", b"planned\n")
        engine = self.engine(); plan = engine.plan("push")
        backups = list((self.creator / "build/keyboard-sync/backups").glob("*.zip"))
        self.put("creator", "core/a.cpp", b"concurrent\n")
        with self.assertRaises(sync.SyncError): engine.apply(plan)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"int value = 1;\n")
        self.assertEqual(list((self.creator / "build/keyboard-sync/backups").glob("*.zip")), backups)

    def test_copy_failure_rolls_back_files_and_keeps_old_baseline(self):
        self.seed()
        before = self.baseline.read_bytes()
        self.put("creator", "core/a.cpp", b"new a\n"); self.put("creator", "core/b.h", b"new b\n")
        engine = self.engine(); plan = engine.plan("push")
        real = sync.atomic_write
        calls = 0
        def failing(path, data, mode=None):
            nonlocal calls
            calls += 1
            if calls == 2: raise OSError("injected disk failure")
            return real(path, data, mode)
        with mock.patch.object(sync, "atomic_write", side_effect=failing), self.assertRaises(sync.SyncError): engine.apply(plan)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"int value = 1;\n")
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/b.h").read_bytes(), b"#pragma once\n")
        self.assertEqual(self.baseline.read_bytes(), before)

    def test_source_changes_during_copy_trigger_rollback_without_losing_the_edit(self):
        self.seed()
        before = self.baseline.read_bytes()
        self.put("creator", "core/a.cpp", b"new a\n")
        engine = self.engine(); plan = engine.plan("push")
        real = sync.atomic_write
        def concurrent_edit(path, data, mode=None):
            real(path, data, mode)
            if path.name == "a.cpp": self.put("creator", "core/b.h", b"concurrent source\n")
        with mock.patch.object(sync, "atomic_write", side_effect=concurrent_edit), self.assertRaises(sync.SyncError): engine.apply(plan)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"int value = 1;\n")
        self.assertEqual((self.creator / "core/b.h").read_bytes(), b"concurrent source\n")
        self.assertEqual(self.baseline.read_bytes(), before)

    def test_rollback_preserves_a_concurrent_user_edit_to_an_already_written_target(self):
        self.seed()
        before = self.baseline.read_bytes()
        self.put("creator", "core/a.cpp", b"new a\n"); self.put("creator", "core/b.h", b"new b\n")
        engine = self.engine(); plan = engine.plan("push")
        real = sync.atomic_write
        def concurrent_edit(path, data, mode=None):
            if path.name == "b.h": raise OSError("injected second-file failure")
            real(path, data, mode)
            self.put("espos", "Keyboard/AdvancedKeyboard/a.cpp", b"concurrent user\n")
        with mock.patch.object(sync, "atomic_write", side_effect=concurrent_edit), self.assertRaisesRegex(sync.SyncError, "Concurrent edit preserved"):
            engine.apply(plan)
        self.assertEqual((self.peer / "Keyboard/AdvancedKeyboard/a.cpp").read_bytes(), b"concurrent user\n")
        self.assertEqual(self.baseline.read_bytes(), before)

    def test_reviewed_plan_cannot_be_modified_or_reused_after_changes(self):
        self.put("creator", "core/a.cpp")
        plan = self.engine().plan("push")
        file = self.creator / "plan.json"; file.write_text(json.dumps(plan))
        common = ["--creator", str(self.creator), "--espos", str(self.peer), "--manifest", str(self.manifest), "--baseline", str(self.baseline)]
        altered = dict(plan); altered["workspace"] = "changed"; file.write_text(json.dumps(altered))
        with contextlib.redirect_stderr(io.StringIO()): self.assertEqual(sync.main(common + ["--apply-plan", str(file)]), 2)
        file.write_text(json.dumps(plan)); self.put("creator", "core/a.cpp", b"new\n")
        with contextlib.redirect_stderr(io.StringIO()): self.assertEqual(sync.main(common + ["--apply-plan", str(file)]), 2)

    def test_nested_adapter_and_model_trees_are_not_pulled_into_portable_core(self):
        self.put("creator", "core/a.cpp")
        self.put("creator", "adapters/CMakeLists.txt", b"adapter\n")
        self.put("creator", "models/catalog.json", b"{}\n")
        self.config["files"] = [{"source": "adapters/CMakeLists.txt", "target": "Keyboard/AdvancedKeyboard/CMakeLists.txt"}]
        self.config["trees"].append({"source": "models", "target": "Keyboard/AdvancedKeyboard/bundled", "include": ["**"], "exclude": []})
        self.save_manifest(); self.apply()
        self.assertEqual(self.engine().plan()["differences"], [])
        self.assertFalse((self.creator / "core/bundled").exists())
        self.assertFalse((self.creator / "core/CMakeLists.txt").exists())

    def test_unlisted_template_is_reported_and_build_caches_are_ignored(self):
        self.config["templateRoot"] = "adapters"
        self.put("creator", "adapters/a.h")
        self.config["files"] = [{"source": "adapters/a.h", "target": "Keyboard/a.h"}]
        self.save_manifest(); self.put("creator", "adapters/__pycache__/a.pyc", b"cache")
        self.put("creator", "core/build/temp.obj", b"object")
        self.put("creator", "core/AGENTS.md", b"do not export")
        self.apply()
        self.assertFalse((self.peer / "Keyboard/AdvancedKeyboard/build").exists())
        self.assertFalse((self.peer / "Keyboard/AdvancedKeyboard/AGENTS.md").exists())
        self.put("creator", "adapters/new.h")
        with self.assertRaises(sync.SyncError): self.engine().plan()


if __name__ == "__main__":
    unittest.main()
