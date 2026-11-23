#!/usr/bin/env python3
"""Synchronize the owned Advanced Keyboard sources without consulting Git.

The default operation is read-only. UTF-8 source text is compared and copied
with LF line endings; binary assets always use their exact original bytes.
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import stat
import sys
import uuid
import zipfile
from datetime import datetime, timezone

VERSION = 1
HERE = Path(__file__).resolve().parent
DEFAULT_WORKSPACE = HERE.parents[1]
IGNORED_PARTS = {".git", ".vs", "__pycache__", "build", "bin", "obj", "node_modules"}
TEXT_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inc", ".py",
                 ".md", ".json", ".txt", ".cmake", ".yml", ".yaml", ".sh", ".ps1"}


class SyncError(RuntimeError):
    pass


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def relative(value):
    if not isinstance(value, str) or not value or "\0" in value:
        raise SyncError("Manifest paths must be nonempty relative strings")
    value = value.replace("\\", "/")
    win = PureWindowsPath(value)
    path = PurePosixPath(value)
    reserved = {"CON", "PRN", "AUX", "NUL"} | {f"{prefix}{n}" for prefix in ["COM", "LPT"] for n in range(1, 10)}
    if (not path.parts or win.drive or path.is_absolute() or
            any(part in {".", "..", ".git"} or any(char in part for char in ':<>"|?*') or
                part.rstrip(" .") != part or part.split(".")[0].upper() in reserved for part in path.parts)):
        raise SyncError("Unsafe relative path: " + value)
    return path.as_posix()


def contained(root, name):
    name = relative(name)
    root = root.resolve()
    result = (root / name).resolve()
    if not result.is_relative_to(root) or result == root:
        raise SyncError("Path escapes its declared root: " + name)
    return result


def text_bytes(path, data):
    if path.suffix.lower() in TEXT_SUFFIXES or path.name in {"LICENSE", "COPYING", ".gitignore"}:
        try:
            data.decode("utf-8")
        except UnicodeDecodeError:
            raise SyncError("Expected UTF-8 source text: " + str(path))
        return data.replace(b"\r\n", b"\n")
    return data


def fingerprint(root, name):
    path = contained(root, name)
    if not path.exists():
        return {"raw": None, "canonical": None, "data": None, "mode": None}
    if not path.is_file():
        raise SyncError("Expected a regular file: " + str(path))
    data = path.read_bytes()
    normalized = text_bytes(path, data)
    return {"raw": sha(data), "canonical": sha(normalized), "data": normalized,
            "mode": stat.S_IMODE(path.stat().st_mode)}


def matches(name, patterns):
    for pattern in patterns:
        if pattern == "**" or ("/" not in pattern and "/" not in name and fnmatch.fnmatchcase(name, pattern)):
            return True
        if "/" in pattern and fnmatch.fnmatchcase(name, pattern):
            return True
    return False


def inventory(root, base, includes, excludes):
    directory = contained(root, base)
    if not directory.exists():
        return []
    if not directory.is_dir():
        raise SyncError("Expected a source directory: " + str(directory))
    result = []
    for path in directory.rglob("*"):
        name = path.relative_to(directory).as_posix()
        if any(part in IGNORED_PARTS for part in PurePosixPath(name).parts):
            continue
        if path.name == "AGENTS.md" or not matches(name, includes) or matches(name, excludes):
            continue
        # Resolving the logical name detects symlink/junction escapes. Ordinary
        # OneDrive cloud placeholders are not rejected just for being reparses.
        contained(root, str(PurePosixPath(base) / name))
        if path.is_file():
            result.append(name)
    return sorted(result)


def read_json(path, missing=None):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return missing
    except (ValueError, UnicodeError) as error:
        raise SyncError("Invalid JSON in " + str(path) + ": " + str(error)) from error


def validate_manifest(value):
    if not isinstance(value, dict) or value.get("version") != VERSION:
        raise SyncError("Unsupported keyboard sync manifest")
    trees = value.get("trees")
    files = value.get("files", [])
    tombstones = value.get("tombstones", [])
    if not isinstance(trees, list) or not isinstance(files, list) or not isinstance(tombstones, list):
        raise SyncError("Manifest trees, files and tombstones must be arrays")
    for tree in trees:
        if not isinstance(tree, dict):
            raise SyncError("Invalid tree mapping")
        relative(tree.get("source")); relative(tree.get("target"))
        if not isinstance(tree.get("discoverPeerAdditions", True), bool):
            raise SyncError("discoverPeerAdditions must be a boolean")
        for field in ["include", "exclude"]:
            if not isinstance(tree.get(field, []), list) or any(not isinstance(p, str) or ".." in PurePosixPath(p).parts for p in tree.get(field, [])):
                raise SyncError("Invalid tree patterns")
    for item in files:
        if not isinstance(item, dict):
            raise SyncError("Invalid file mapping")
        relative(item.get("source")); relative(item.get("target"))
    retired = set()
    for item in tombstones:
        target = relative(item.get("target"))
        if target.casefold() in retired:
            raise SyncError("Duplicate tombstone target")
        retired.add(target.casefold())
        expected = item.get("expectedSha256")
        if not isinstance(expected, str) or len(expected) != 64 or any(c not in "0123456789abcdef" for c in expected):
            raise SyncError("Tombstones need a known canonical SHA-256")
    return value


def atomic_write(path, data, mode=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name("." + path.name + ".keyboard-sync-" + uuid.uuid4().hex + ".tmp")
    try:
        with temporary.open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        if mode is not None:
            os.chmod(temporary, mode)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


class Sync:
    def __init__(self, workspace, peer=None, manifest=None, baseline=None, staging=False):
        self.workspace = Path(workspace).resolve()
        self.peer = Path(peer).resolve() if peer is not None else None
        self.staging = staging
        if self.peer is not None:
            nested = self.peer.is_relative_to(self.workspace)
            if staging and (not nested or not self.peer.is_relative_to(self.workspace / "build") or
                            baseline is None or not Path(baseline).resolve().is_relative_to(self.workspace / "build")):
                raise SyncError("Staging requires a peer and an explicit isolated baseline under Creator/build")
            if self.peer == self.workspace or self.workspace.is_relative_to(self.peer) or (nested and not staging):
                raise SyncError("Creator and ESPOS roots must be separate directories")
            if not self.peer.is_dir():
                raise SyncError("The explicitly selected ESPOS directory does not exist")
        self.manifest_path = Path(manifest or self.workspace / "Integrations/ESPOS/manifest.json").resolve()
        self.baseline_path = Path(baseline or self.workspace / "Integrations/ESPOS/sync-baseline.json").resolve()
        if not self.baseline_path.is_relative_to(self.workspace):
            raise SyncError("The sync baseline must stay inside the Creator checkout")
        self.manifest = validate_manifest(read_json(self.manifest_path))
        self.manifest_sha = sha(canonical_json(self.manifest))
        self.baseline = read_json(self.baseline_path, {"version": VERSION, "files": []})
        if not isinstance(self.baseline, dict) or self.baseline.get("version") != VERSION or not isinstance(self.baseline.get("files"), list):
            raise SyncError("Invalid keyboard sync baseline")
        self.base = {}
        for item in self.baseline["files"]:
            source, target, checksum = relative(item.get("source")), relative(item.get("target")), item.get("sha256")
            if not isinstance(checksum, str) or len(checksum) != 64 or any(c not in "0123456789abcdef" for c in checksum):
                raise SyncError("Invalid file hash in keyboard sync baseline")
            key = (source, target)
            if key in self.base:
                raise SyncError("Duplicate baseline mapping")
            self.base[key] = checksum
        self.snapshots = {}
        self.payloads = {}

    def mappings(self, with_peer=True):
        pairs = set()
        explicit = {relative(item["target"]): relative(item["source"]) for item in self.manifest.get("files", [])}
        if len(explicit) != len(self.manifest.get("files", [])):
            raise SyncError("Duplicate explicit target mappings")
        trees = self.manifest["trees"]
        for tree in trees:
            source, target = relative(tree["source"]), relative(tree["target"])
            includes, excludes = tree.get("include", ["**"]), tree.get("exclude", [])
            if not contained(self.workspace, source).is_dir():
                raise SyncError("Creator source tree is missing: " + source)
            names = set(inventory(self.workspace, source, includes, excludes))
            if with_peer and self.peer is not None and tree.get("discoverPeerAdditions", True):
                names.update(inventory(self.peer, target, includes, excludes))
            for name in names:
                s, t = str(PurePosixPath(source) / name), str(PurePosixPath(target) / name)
                # Nested data trees and explicit adapters own their destinations.
                if t in explicit:
                    if (self.workspace / s).is_file() and explicit[t] != s:
                        raise SyncError("A portable file collides with an explicit adapter: " + t)
                    continue
                nested = [relative(other["target"]) for other in trees if other is not tree]
                if any(PurePosixPath(t).is_relative_to(PurePosixPath(base)) and PurePosixPath(base).is_relative_to(PurePosixPath(target)) and base != target for base in nested):
                    continue
                pairs.add((relative(s), relative(t)))
        for target, source in explicit.items():
            if not contained(self.workspace, source).is_file() and (source, target) not in self.base:
                raise SyncError("Declared adapter source is missing: " + source)
            pairs.add((source, target))
        # Every staged template must be explicitly owned by the manifest.
        template_root = self.manifest.get("templateRoot")
        if template_root:
            template_root = relative(template_root)
            expected = {source for source in explicit.values() if PurePosixPath(source).is_relative_to(PurePosixPath(template_root))}
            actual = {str(PurePosixPath(template_root) / name) for name in inventory(self.workspace, template_root, ["**"], [])}
            if actual != expected:
                raise SyncError("Adapter template list differs from manifest: " + ", ".join(sorted(actual ^ expected)))
        pairs.update(self.base)
        targets, physical = {}, {}
        for source, target in sorted(pairs):
            key = target.casefold()
            if key in targets and targets[key] != (source, target):
                raise SyncError("Case-insensitive target collision: " + target)
            targets[key] = (source, target)
            contained(self.workspace, source)
            if self.peer is not None:
                resolved = str(contained(self.peer, target)).casefold()
                if resolved in physical and physical[resolved] != target:
                    raise SyncError("Two targets resolve to the same file")
                physical[resolved] = target
        return sorted(pairs)

    def take(self, side, name):
        key = side + ":" + name
        if key not in self.snapshots:
            root = self.workspace if side == "creator" else self.peer
            if root is None:
                raise SyncError("An explicit --espos directory is required")
            self.snapshots[key] = fingerprint(root, name)
        return self.snapshots[key]

    def check_source(self):
        pairs = self.mappings(with_peer=False)
        drift = []
        if not self.baseline_path.exists() or self.baseline.get("manifestSha256") != self.manifest_sha:
            drift.append("sync manifest has not been acknowledged by a completed sync")
        current = {}
        for source, target in pairs:
            value = self.take("creator", source)["canonical"]
            if value is not None:
                current[(source, target)] = value
            if value != self.base.get((source, target)):
                drift.append(source)
        if current != self.base and not drift:
            drift.append("source inventory differs from the last completed sync")
        return {"version": VERSION, "status": "drift" if drift else "in-sync", "operation": "check-source",
                "files": len(current), "differences": sorted(set(drift))}

    def plan(self, direction="check", bootstrap=False):
        if self.peer is None:
            raise SyncError("Provide --espos explicitly; the tool never discovers an ancestor repository")
        if direction not in {"check", "push", "pull"} or (bootstrap and direction != "push"):
            raise SyncError("Bootstrap is available only for an explicitly reviewed push")
        groups = {}
        for source, target in self.mappings():
            groups.setdefault(source, []).append(target)
        actions, differences, conflicts, baseline = [], [], [], []
        for source, targets in sorted(groups.items()):
            original = self.take("creator", source)
            peers = [(target, self.take("espos", target)) for target in targets]
            bases = {self.base[(source, target)] for target in targets if (source, target) in self.base}
            if len(bases) > 1:
                raise SyncError("Baseline copies disagree for " + source)
            base = next(iter(bases)) if bases else None
            a = original["canonical"]
            values = {item["canonical"] for _, item in peers}
            equal = all(item["canonical"] == a for _, item in peers)
            if equal:
                desired, winner = a, ("creator", source, original)
            elif direction == "check":
                differences.append({"source": source, "targets": targets})
                continue
            elif direction == "push":
                if a is None and base is None:
                    conflicts.append({"source": source, "targets": targets, "reason": "Unbased ESPOS-only file was preserved; pull or register it explicitly"})
                    continue
                unsafe = [target for target, item in peers if item["canonical"] not in {base, a}]
                if unsafe and not (bootstrap and base is None):
                    conflicts.append({"source": source, "targets": unsafe, "reason": "ESPOS changed since the last common baseline"})
                    continue
                if a == base and not bootstrap:
                    conflicts.append({"source": source, "targets": targets, "reason": "Only ESPOS changed; pull or resolve it first"})
                    continue
                desired, winner = a, ("creator", source, original)
            else:
                changed = values - {base}
                if len(changed) != 1:
                    conflicts.append({"source": source, "targets": targets, "reason": "ESPOS copies disagree; resolve their competing changes"})
                    continue
                desired = next(iter(changed))
                if a not in {base, desired}:
                    conflicts.append({"source": source, "targets": targets, "reason": "Both Creator and ESPOS changed"})
                    continue
                target, value = next((t, value) for t, value in peers if value["canonical"] == desired)
                winner = ("espos", target, value)
            if desired is not None:
                baseline.extend({"source": source, "target": target, "sha256": desired} for target in targets)
            if direction == "check":
                continue
            for side, name, item in [("creator", source, original)] + [("espos", target, value) for target, value in peers]:
                if item["canonical"] == desired:
                    continue
                key = side + ":" + name
                self.payloads[key] = winner[2]["data"] if desired is not None else None
                actions.append({"side": side, "path": name, "beforeRawSha256": item["raw"],
                                "afterSha256": desired, "operation": "delete" if desired is None else "write",
                                "fromSide": winner[0], "fromPath": winner[1]})
        for tombstone in self.manifest.get("tombstones", []):
            target = relative(tombstone["target"])
            if any(target in values for values in groups.values()):
                raise SyncError("Tombstone overlaps an active mapping: " + target)
            value = self.take("espos", target)
            if value["canonical"] is None:
                continue
            if direction == "check":
                differences.append({"tombstone": target})
            elif direction != "push" or value["canonical"] != tombstone["expectedSha256"]:
                conflicts.append({"target": target, "reason": "Retired managed file has unexpected content; it was preserved"})
            else:
                self.payloads["espos:" + target] = None
                actions.append({"side": "espos", "path": target, "beforeRawSha256": value["raw"],
                                "afterSha256": None, "operation": "delete", "fromSide": None, "fromPath": None})
        baseline_value = {"version": VERSION, "manifestSha256": self.manifest_sha,
                          "files": sorted(baseline, key=lambda item: (item["source"], item["target"]))}
        plan = {"version": VERSION, "workspace": str(self.workspace), "espos": str(self.peer),
                "baseline": str(self.baseline_path), "staging": self.staging,
                "operation": direction, "bootstrap": bootstrap, "manifestSha256": self.manifest_sha,
                "baselineRawSha256": sha(self.baseline_path.read_bytes()) if self.baseline_path.exists() else None,
                "actions": sorted(actions, key=lambda item: (item["side"], item["path"])),
                "differences": differences, "conflicts": conflicts, "nextBaseline": baseline_value,
                "snapshots": {key: {"raw": value["raw"], "canonical": value["canonical"]} for key, value in sorted(self.snapshots.items())}}
        plan["planSha256"] = sha(canonical_json(plan))
        return plan

    def assert_snapshots(self, plan):
        if sha(canonical_json(validate_manifest(read_json(self.manifest_path)))) != plan["manifestSha256"]:
            raise SyncError("Sync manifest changed after planning")
        current_base = sha(self.baseline_path.read_bytes()) if self.baseline_path.exists() else None
        if current_base != plan["baselineRawSha256"]:
            raise SyncError("Sync baseline changed after planning")
        for key, expected in plan["snapshots"].items():
            side, name = key.split(":", 1)
            root = self.workspace if side == "creator" else self.peer
            if fingerprint(root, name)["raw"] != expected["raw"]:
                raise SyncError("File changed after planning; nothing was overwritten: " + key)

    def apply(self, plan, backup_dir=None):
        if plan["operation"] == "check" or plan["conflicts"]:
            raise SyncError("Cannot apply a check or a conflicting plan")
        self.assert_snapshots(plan)
        backup_root = Path(backup_dir or self.workspace / "build/keyboard-sync/backups").resolve()
        if not backup_root.is_relative_to(self.workspace):
            raise SyncError("Backups must stay inside the Creator workspace")
        backups, modes, written = {}, {}, []
        old_baseline = self.baseline_path.read_bytes() if self.baseline_path.exists() else None
        for item in plan["actions"]:
            root = self.workspace if item["side"] == "creator" else self.peer
            path = contained(root, item["path"])
            key = item["side"] + ":" + item["path"]
            backups[key] = path.read_bytes() if path.exists() else None
            modes[key] = stat.S_IMODE(path.stat().st_mode) if path.exists() else None
        backup_root.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        backup = backup_root / (stamp + "-" + uuid.uuid4().hex[:12] + ".zip")
        with zipfile.ZipFile(backup, "x", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("plan.json", json.dumps(plan, ensure_ascii=False, indent=2))
            if old_baseline is not None:
                archive.writestr("baseline-before.json", old_baseline)
            for key, data in backups.items():
                if data is not None:
                    side, name = key.split(":", 1)
                    archive.writestr(side + "/" + name, data)
        try:
            for item in plan["actions"]:
                root = self.workspace if item["side"] == "creator" else self.peer
                key = item["side"] + ":" + item["path"]
                path = contained(root, item["path"])
                if fingerprint(root, item["path"])["raw"] != item["beforeRawSha256"]:
                    raise SyncError("Target changed during sync: " + key)
                data = self.payloads[key]
                if data is None:
                    path.unlink()
                else:
                    atomic_write(path, data, modes[key])
                written.append(key)
            # Protect every untouched input too; a concurrent editor must never
            # let a stale snapshot become the new common baseline.
            changed_keys = {item["side"] + ":" + item["path"] for item in plan["actions"]}
            for key, expected in plan["snapshots"].items():
                if key in changed_keys:
                    continue
                side, name = key.split(":", 1)
                root = self.workspace if side == "creator" else self.peer
                if fingerprint(root, name)["raw"] != expected["raw"]:
                    raise SyncError("An input changed during sync: " + key)
            for item in plan["actions"]:
                root = self.workspace if item["side"] == "creator" else self.peer
                if fingerprint(root, item["path"])["canonical"] != item["afterSha256"]:
                    raise SyncError("Post-write hash differs: " + item["path"])
            if sha(canonical_json(validate_manifest(read_json(self.manifest_path)))) != plan["manifestSha256"]:
                raise SyncError("Sync manifest changed during copying")
            current_base = sha(self.baseline_path.read_bytes()) if self.baseline_path.exists() else None
            if current_base != plan["baselineRawSha256"]:
                raise SyncError("Sync baseline changed during copying")
            atomic_write(self.baseline_path, json.dumps(plan["nextBaseline"], ensure_ascii=False, indent=2).encode("utf-8") + b"\n")
        except BaseException as error:
            rollback_errors = []
            for key in reversed(written):
                side, name = key.split(":", 1)
                root = self.workspace if side == "creator" else self.peer
                try:
                    path = contained(root, name)
                    expected = sha(self.payloads[key]) if self.payloads[key] is not None else None
                    if fingerprint(root, name)["raw"] != expected:
                        rollback_errors.append("Concurrent edit preserved at " + key)
                        continue
                    if backups[key] is None:
                        if path.exists(): path.unlink()
                    else:
                        atomic_write(path, backups[key], modes[key])
                except Exception as failure:
                    rollback_errors.append(str(failure))
            message = str(error) + "; backup: " + str(backup)
            if rollback_errors:
                message += "; rollback needs attention: " + "; ".join(rollback_errors)
            raise SyncError(message) from error
        return {"status": "synchronized", "changedFiles": len(plan["actions"]), "backup": str(backup),
                "baseline": str(self.baseline_path), "files": len(plan["nextBaseline"]["files"])}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--creator", type=Path, default=DEFAULT_WORKSPACE, help="explicit Creator checkout; no Git discovery")
    parser.add_argument("--espos", type=Path, help="explicit ESPOS project directory")
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--baseline", "--state", dest="baseline", type=Path)
    parser.add_argument("--staging", action="store_true", help="allow an isolated peer and explicit state under Creator/build for validation")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="read-only comparison (default)")
    mode.add_argument("--check-source", action="store_true", help="CI check using the committed baseline, no ESPOS checkout required")
    mode.add_argument("--push", action="store_true", help="propagate Creator changes; refuse conflicting peer edits")
    mode.add_argument("--pull", action="store_true", help="propagate ESPOS changes and reconcile unchanged mirror copies")
    mode.add_argument("--apply-plan", type=Path, help="apply an unchanged, previously reviewed plan")
    parser.add_argument("--bootstrap", action="store_true", help="explicit first push replacing unbased managed files; review a dry run first")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--plan-out", type=Path, help="write a reviewable plan; no mutations with --dry-run")
    parser.add_argument("--backup-dir", type=Path)
    args = parser.parse_args(argv)
    try:
        sync = Sync(args.creator, args.espos, args.manifest, args.baseline, args.staging)
        if args.check_source:
            if args.bootstrap or args.apply_plan:
                raise SyncError("Invalid check-source options")
            result = sync.check_source()
            print(json.dumps(result, ensure_ascii=False, indent=2))
            return 0 if result["status"] == "in-sync" else 1
        direction = "push" if args.push else "pull" if args.pull else "check"
        reviewed = None
        if args.apply_plan:
            reviewed = read_json(args.apply_plan)
            if not isinstance(reviewed, dict) or reviewed.get("version") != VERSION:
                raise SyncError("Invalid reviewed sync plan")
            direction, bootstrap = reviewed.get("operation"), reviewed.get("bootstrap", False)
        else:
            bootstrap = args.bootstrap
        plan = sync.plan(direction, bootstrap)
        if reviewed is not None and canonical_json(reviewed) != canonical_json(plan):
            raise SyncError("The reviewed plan is stale or modified; create and review a new plan")
        if args.plan_out:
            args.plan_out.parent.mkdir(parents=True, exist_ok=True)
            args.plan_out.write_text(json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if direction == "check" or args.dry_run:
            retired = {relative(item["target"]) for item in sync.manifest.get("tombstones", [])}
            files = sum(key.startswith("espos:") and key[len("espos:"):] not in retired for key in plan["snapshots"])
            output = {"status": "conflicts" if plan["conflicts"] else "planned" if plan["actions"] else "differences" if plan["differences"] else "in-sync",
                      "operation": direction, "files": files, "changedFiles": len(plan["actions"]),
                      "differences": plan["differences"], "conflicts": plan["conflicts"]}
            if args.plan_out:
                output.update({"plan": str(args.plan_out.resolve()), "planSha256": plan["planSha256"]})
            print(json.dumps(output, ensure_ascii=False, indent=2))
            return 1 if plan["differences"] or plan["conflicts"] else 0
        result = sync.apply(plan, args.backup_dir)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (SyncError, OSError, KeyError, TypeError) as error:
        print("Keyboard sync failed: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
