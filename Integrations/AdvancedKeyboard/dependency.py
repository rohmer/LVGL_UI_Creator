"""Resolve/install the pinned AdvancedKeyboard repository; never mirror sources."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import subprocess

DEFAULT_CREATOR_PATH = "3rdParty/AdvancedKeyboard"
DEFAULT_ESPOS_PATH = "third_party/AdvancedKeyboard"
LOCK_NAME = "advanced-keyboard.lock.json"
IGNORED = {".git", "build", "bin", "obj", "__pycache__"}


def load_lock(project_root):
    data = json.loads((Path(project_root) / LOCK_NAME).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or type(data.get("version")) is not int or data["version"] != 1 or data.get("repository") != "https://github.com/rohmer/AdvancedKeyboard.git":
        raise ValueError("Unsupported AdvancedKeyboard dependency lock")
    revision = data.get("revision", "")
    if not isinstance(revision, str) or len(revision) != 40 or any(ch not in "0123456789abcdef" for ch in revision):
        raise ValueError("Dependency revision must be a complete Git commit")
    if not isinstance(data.get("paths"), dict):
        raise ValueError("Dependency lock paths must be an object")
    for name in ("creator", "espos"):
        value = data["paths"].get(name)
        if not isinstance(value, str) or not value or value in (".", "./", ".\\"):
            raise ValueError("Dependency checkout path must be a nonempty relative directory")
        path = PurePosixPath(value.replace("\\", "/"))
        if not path.parts or path.is_absolute() or ".." in path.parts or ".git" in path.parts or PureWindowsPath(value).drive or ":" in value:
            raise ValueError("Dependency checkout path escapes the project")
    return data


def project_kind(project_root):
    root = Path(project_root)
    return "espos" if (root / "espos.yaml").is_file() and not (root / "LVSerial").is_dir() else "creator"


def resolve_root(project_root, explicit=None):
    project = Path(project_root).resolve()
    selected = explicit or os.environ.get("ADVANCED_KEYBOARD_ROOT")
    if selected:
        root = Path(selected).resolve()
    else:
        kind = project_kind(project)
        relative = DEFAULT_ESPOS_PATH if kind == "espos" else DEFAULT_CREATOR_PATH
        if (project / LOCK_NAME).is_file():
            relative = load_lock(project)["paths"][kind]
        root = (project / relative).resolve()
    for name in ("src/PredictionModel.cpp", "include/lv_advanced_keyboard.h"):
        if not (root / name).is_file():
            raise FileNotFoundError("AdvancedKeyboard repository is missing at " + str(root) +
                                    "; initialize its pinned checkout before building")
    return root


def runtime_files(dependency_root):
    root = Path(dependency_root).resolve()
    result = []
    for prefix in ("src", "include", "cmake"):
        for folder, directories, files in os.walk(root / prefix):
            directories[:] = [name for name in directories if name not in IGNORED]
            for name in sorted(files):
                path = Path(folder) / name
                if not path.resolve().is_relative_to(root):
                    raise ValueError("Dependency source path escaped its checkout")
                result.append(path.relative_to(root).as_posix())
    for name in ("CMakeLists.txt", "LICENSE", "NOTICE.md", "idf_component.yml", "library.properties",
                 "tools/layout/build_ngtr_index.py"):
        if (root / name).is_file():
            if not (root / name).resolve().is_relative_to(root):
                raise ValueError("Dependency source path escaped its checkout")
            result.append(name)
    return sorted(set(result))


def copy_runtime(dependency_root, destination):
    source, output = Path(dependency_root).resolve(), Path(destination).resolve()
    if output.is_relative_to(source) and not output.is_relative_to(source / "build"):
        raise ValueError("Runtime output must be separate from the maintained source")
    names = runtime_files(source)
    for name in names:
        target = output / name
        if not target.resolve().is_relative_to(output):
            raise ValueError("Runtime output path escaped its destination")
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or target.read_bytes() != (source / name).read_bytes():
            shutil.copyfile(source / name, target)
    return names


def prepare_tool_resources(project_root, dependency_root, kind=None):
    """Reuse ignored, user-owned OpenNLP downloads without copying tool source."""
    project, dependency = Path(project_root).resolve(), Path(dependency_root).resolve()
    source = project / ("tools" if (kind or project_kind(project)) == "espos" else "Tools") / "CorpusCrawler/OpenNlp"
    target = dependency / "tools/CorpusCrawler/OpenNlp"
    copied = 0
    candidates = list((source / "dependencies").glob("*.jar")) if (source / "dependencies").is_dir() else []
    if (source / "java-home.txt").is_file():
        candidates.append(source / "java-home.txt")
    for path in candidates:
        if not path.resolve().is_relative_to(project):
            raise ValueError("Preserved tool resource escaped the explicit project")
        output = target / path.relative_to(source)
        if not output.resolve().is_relative_to(dependency):
            raise ValueError("Ignored tool resource output escaped the dependency")
        if not output.exists():
            output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, output)
            copied += 1
    return copied


def git(root, *args):
    process = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
    if process.returncode:
        raise ValueError(process.stderr.strip() or "Git operation failed")
    return process.stdout.strip()


def check_checkout(root, lock):
    if Path(git(root, "rev-parse", "--show-toplevel")).resolve() != root.resolve():
        raise ValueError("Dependency is not its own repository; an ancestor repository is never used")
    if git(root, "rev-parse", "HEAD") != lock["revision"]:
        raise ValueError("AdvancedKeyboard checkout differs from its pinned revision")
    if git(root, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("AdvancedKeyboard checkout has local edits; preserved without updating")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    parser.add_argument("--kind", choices=("creator", "espos"))
    parser.add_argument("--root", type=Path, help="explicit development checkout for read-only resolution")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--init", action="store_true", help="clone the exact pinned dependency if missing")
    mode.add_argument("--check", action="store_true", help="verify exact revision and clean source")
    args = parser.parse_args()
    project = args.project_root.resolve()
    lock = load_lock(project)
    kind = args.kind or project_kind(project)
    destination = (project / lock["paths"][kind]).absolute()
    if not destination.resolve().is_relative_to(project) or destination.resolve() == project:
        raise ValueError("Dependency destination escaped the explicit project root")
    if args.init:
        if args.root:
            raise ValueError("--init only operates on the project's explicit pinned path")
        tracked_submodule = False
        if (project / ".git").exists():
            if Path(git(project, "rev-parse", "--show-toplevel")).resolve() != project:
                raise ValueError("Project Git root differs from the explicit project directory")
            index = git(project, "ls-files", "--stage", "--", lock["paths"][kind])
            tracked_submodule = index.startswith("160000 ")
        if tracked_submodule and not (destination / ".git").exists():
            git(project, "submodule", "update", "--init", "--", lock["paths"][kind])
        elif not destination.exists() or not any(destination.iterdir()):
            destination.parent.mkdir(parents=True, exist_ok=True)
            process = subprocess.run(["git", "clone", "--no-checkout", lock["repository"], str(destination)])
            if process.returncode:
                raise ValueError("Could not clone AdvancedKeyboard; existing data was preserved")
            git(destination, "checkout", "--detach", lock["revision"])
        check_checkout(destination, lock)
        prepare_tool_resources(project, destination, kind)
    root = resolve_root(project, args.root or destination)
    if args.check:
        check_checkout(root, lock)
    print(json.dumps({"root": str(root), "revision": lock["revision"], "kind": kind,
                      "status": "verified" if args.check or args.init else "resolved"}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
