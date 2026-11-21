"""Manifest-compatible LVGL project creation. Standard-library-only worker/CLI.

Catalog/specification: https://github.com/lvgl/lvgl_project_creator
The upstream application is not open source; this implementation follows its public manifests.
"""
from __future__ import annotations
import argparse
import configparser
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import tempfile
import urllib.parse
import urllib.request
import uuid
import zipfile
try:
    import raspberry_pi_linux
except ModuleNotFoundError as error:
    if error.name != "raspberry_pi_linux":
        raise
    import importlib.util
    helper_spec = importlib.util.spec_from_file_location("raspberry_pi_linux", Path(__file__).with_name("raspberry_pi_linux.py"))
    raspberry_pi_linux = importlib.util.module_from_spec(helper_spec)
    helper_spec.loader.exec_module(raspberry_pi_linux)

CATALOG_URL = "https://raw.githubusercontent.com/lvgl/lvgl_project_creator/master/manifest_all_v1.1.0.json"
MAX_DOWNLOAD = 512 * 1024 * 1024
MAX_EXTRACTED = 2 * 1024 * 1024 * 1024


def safe_relative(value: str) -> Path:
    text = str(value).replace("\\", "/")
    parts = PurePosixPath(text).parts
    if not text or text.startswith(("/", "~")) or ":" in text or any(p in ("..", ".") for p in text.split("/")):
        raise ValueError(f"Expected a relative project path: {value}")
    if not parts or any("\0" in p or p.rstrip(" .") != p for p in parts):
        raise ValueError(f"Invalid project path: {value}")
    for p in parts:
        if p.split(".")[0].upper() in {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)), *(f"LPT{i}" for i in range(1, 10))}:
            raise ValueError(f"Reserved project path: {value}")
    return Path(*parts)


def inside(root: Path, relative: str) -> Path:
    result = root / safe_relative(relative)
    if not result.resolve().is_relative_to(root.resolve()):
        raise ValueError("Path escapes project directory")
    return result


def atomic_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".partial-" + uuid.uuid4().hex)
    try:
        tmp.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
        os.replace(tmp, path)
    finally:
        tmp.unlink(missing_ok=True)


def extract_archive(data: bytes, destination: Path):
    """Extract archives without links, traversal, absolute paths or unbounded expansion."""
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries = archive.infolist()
        if len(entries) > 200000 or sum(e.file_size for e in entries) > MAX_EXTRACTED:
            raise ValueError("Project archive exceeds extraction limits")
        names = [e.filename.replace("\\", "/").rstrip("/") for e in entries if e.filename.rstrip("/")]
        for name in names:
            safe_relative(name)
        roots = {name.split("/")[0] for name in names}
        strip = len(roots) == 1 and all("/" in name or any(e.is_dir() and e.filename.rstrip("/") == name for e in entries) for name in names)
        seen = set()
        for entry in entries:
            mode = entry.external_attr >> 16
            if stat.S_ISLNK(mode):
                raise ValueError("Project archives cannot contain symbolic links")
            name = entry.filename.replace("\\", "/").rstrip("/")
            if strip:
                name = "/".join(name.split("/")[1:])
            if not name:
                continue
            relative = safe_relative(name)
            key = relative.as_posix().casefold()
            if key in seen and not entry.is_dir():
                raise ValueError("Project archive contains duplicate paths")
            seen.add(key)
            target = inside(destination, relative.as_posix())
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if entry.file_size > MAX_DOWNLOAD:
                raise ValueError("Archive entry is too large")
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as source, target.open("xb") as output:
                shutil.copyfileobj(source, output)
            if mode & stat.S_IXUSR:
                target.chmod(target.stat().st_mode | stat.S_IXUSR)


def validate_manifest(manifest):
    if not isinstance(manifest, dict) or not isinstance(manifest.get("name"), str) or not manifest["name"].strip():
        raise ValueError("Template needs a name")
    github_repo(manifest.get("urlToClone", ""))
    raspberry_pi_linux.validate_manifest(manifest)
    safe_relative(manifest.get("uiFolderPath", "ui"))
    settings = manifest.get("settings", manifest.get("Settings", []))
    if not isinstance(settings, list):
        raise ValueError("Template settings must be an array")
    for setting in settings:
        if setting.get("type") not in ("dropdown", "always"):
            raise ValueError("Unknown template setting type")
        if setting["type"] == "dropdown" and not setting.get("options"):
            raise ValueError("Dropdown setting has no options")
        for action in setting.get("actions", []):
            safe_relative(action["filePath"])
            if "toReplace" in action:
                re.compile(action["toReplace"], re.MULTILINE)
                if "newContent" not in action:
                    raise ValueError("Replacement action needs newContent")
            if "toReplace" not in action and "toAppend" not in action:
                raise ValueError("Unknown template action")
    return manifest


def github_repo(url: str) -> str:
    if url.startswith("git@github.com:"):
        url = "https://github.com/" + url[len("git@github.com:"):]
    parsed = urllib.parse.urlsplit(url)
    path = parsed.path.rstrip("/").removesuffix(".git")
    if parsed.scheme != "https" or parsed.netloc.lower() != "github.com" or parsed.query or parsed.fragment or not re.fullmatch(r"/[\w.-]+/[\w.-]+", path):
        raise ValueError("Templates and submodules must use HTTPS GitHub repository URLs")
    return path[1:]


def setting_value(value):
    if value is True:
        return "true"
    if value is False:
        return "false"
    return str(value)


def apply_settings(root: Path, manifest, selections):
    settings = manifest.get("settings", manifest.get("Settings", []))
    allowed = {str(i) for i, _ in enumerate(settings)} | {s.get("label", "") for s in settings}
    if set(selections) - allowed:
        raise ValueError("Unknown template setting selection")
    applied = []
    for index, setting in enumerate(settings):
        value = ""
        if setting["type"] == "dropdown":
            options = setting["options"]
            default = next((o for o in options if str(o.get("default", "false")).lower() == "true"), options[0])
            value = setting_value(selections.get(str(index), selections.get(setting.get("label", ""), default["value"])))
            if value not in [setting_value(o["value"]) for o in options]:
                raise ValueError(f"Invalid option for {setting.get('label', index)}")
        for action in setting.get("actions", []):
            if "ifValue" in action and setting_value(action["ifValue"]) != value:
                continue
            path = inside(root, action["filePath"])
            if path.is_symlink():
                raise ValueError("Settings cannot modify symbolic links")
            content = path.read_text(encoding="utf-8") if path.exists() else ""
            if "toReplace" in action:
                if not path.is_file():
                    raise ValueError(f"Setting file is missing: {action['filePath']}")
                replacement = action["newContent"].replace("{value}", value)
                content, count = re.subn(action["toReplace"], lambda _: replacement, content, flags=re.MULTILINE)
                if not count:
                    raise ValueError(f"Setting did not match {action['filePath']}: {setting.get('label', 'always')}")
            if "toAppend" in action:
                addition = action["toAppend"].replace("{value}", value)
                content = content.rstrip("\n") + "\n" + addition.rstrip("\n") + "\n"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
            applied.append({"setting": setting.get("label", "always"), "value": value, "file": action["filePath"]})
    return applied


def verify_lvgl(folder, requested):
    expected = requested.removeprefix("v")
    for header in (folder / "include/lvgl/lv_version.h", folder / "lv_version.h", folder / "src/lv_version.h", folder / "lvgl.h"):
        if not header.is_file():
            continue
        text = header.read_text(encoding="utf-8")
        numbers = [re.search(r"#define\s+LVGL_VERSION_" + part + r"\s+(\d+)", text) for part in ("MAJOR", "MINOR", "PATCH")]
        if all(numbers):
            actual = ".".join(n[1] for n in numbers)
            if actual != expected:
                raise ValueError(f"LVGL source is {actual}, expected {expected}")
            return
    raise ValueError("Selected LVGL source has no recognized version header")


class Creator:
    def __init__(self, cache: Path, fetch=None):
        self.cache = Path(cache)
        self.cache.mkdir(parents=True, exist_ok=True)
        self.fetch = fetch or self.download
        self.repositories = []

    @staticmethod
    def download(url):
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != "https":
            raise ValueError("Downloads require HTTPS")
        headers = {"User-Agent": "LVGL-UI-Creator/1.0"}
        token = os.environ.get("GITHUB_TOKEN")
        if token and parsed.hostname == "api.github.com":
            headers["Authorization"] = "Bearer " + token
        with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as source:
            chunks, count = [], 0
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                count += len(chunk)
                if count > MAX_DOWNLOAD:
                    raise ValueError("Download exceeds supported size")
                chunks.append(chunk)
            return b"".join(chunks)

    def cached(self, url, refresh=False):
        path = self.cache / (hashlib.sha256(url.encode()).hexdigest() + ".cache")
        if path.exists() and not refresh:
            return path.read_bytes()
        try:
            data = self.fetch(url)
        except Exception:
            if path.exists():
                return path.read_bytes()
            raise
        temporary = path.with_name(path.name + ".partial-" + uuid.uuid4().hex)
        try:
            temporary.write_bytes(data)
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)
        return data

    def catalog(self, refresh=False):
        if refresh:
            data = self.cached(CATALOG_URL, True)
        else:
            cache = self.cache / "catalog.json"
            data = cache.read_bytes() if cache.exists() else ((Path(sys.executable).parent if getattr(sys, "frozen", False) else Path(__file__).parent) / "catalog.json").read_bytes()
        catalog = json.loads(data)
        if not isinstance(catalog, list):
            raise ValueError("Catalog must be an array")
        catalog = raspberry_pi_linux.merge_catalog(catalog)
        for item in catalog:
            validate_manifest(item)
        atomic_json(self.cache / "catalog.json", catalog)
        return catalog

    def preview(self, url):
        data = self.cached(url)
        if len(data) > 16 * 1024 * 1024:
            raise ValueError("Template image is too large")
        suffix = Path(urllib.parse.urlsplit(url).path).suffix.lower()
        if suffix not in (".svg", ".png", ".jpg", ".jpeg", ".gif", ".bmp"):
            raise ValueError("Unsupported template image type")
        path = self.cache / (hashlib.sha256(url.encode()).hexdigest() + suffix)
        path.write_bytes(data)
        return {"path": str(path)}

    def api(self, path, refresh=False):
        return json.loads(self.cached("https://api.github.com/" + path, refresh))

    def repository(self, url, ref, target, version="template", lvgl_archive=None, depth=0, ancestors=()):
        if depth > 16:
            raise ValueError("Submodule nesting is too deep")
        repo = github_repo(url)
        if not ref:
            ref = self.api(f"repos/{repo}")["default_branch"]
        if (repo, ref) in ancestors:
            raise ValueError("Recursive submodule cycle")
        sha = ref if re.fullmatch(r"[0-9a-fA-F]{40}", ref) else self.api(f"repos/{repo}/commits/{urllib.parse.quote(ref, safe='')}", True)["sha"]
        data = self.cached(f"https://codeload.github.com/{repo}/zip/{sha}")
        target.mkdir(parents=True, exist_ok=True)
        extract_archive(data, target)
        self.repositories.append({"repository": repo, "ref": ref, "commit": sha})
        modules = target / ".gitmodules"
        lvgl_paths = []
        if modules.is_file():
            config = configparser.ConfigParser(interpolation=None, strict=False)
            config.read_string(modules.read_text(encoding="utf-8"))
            for section in config.sections():
                path = config[section].get("path", "")
                module = inside(target, path)
                module_url = config[section].get("url", "")
                if module_url.startswith("."):
                    module_url = urllib.parse.urljoin(f"https://github.com/{repo}/", module_url)
                module_repo = github_repo(module_url)
                is_lvgl = module_repo.lower() == "lvgl/lvgl"
                if is_lvgl:
                    lvgl_paths.append(module)
                if is_lvgl and version != "template":
                    if module.exists():
                        shutil.rmtree(module)
                    if lvgl_archive:
                        module.mkdir(parents=True)
                        extract_archive(Path(lvgl_archive).read_bytes(), module)
                        verify_lvgl(module, version)
                        self.repositories.append({"repository": "lvgl/lvgl", "ref": version, "source": "bundled"})
                    else:
                        self.repository(module_url, "v" + version.removeprefix("v"), module, depth=depth+1, ancestors=(*ancestors, (repo, ref)))
                    continue
                if module.exists() and any(module.iterdir()):
                    continue
                info = self.api(f"repos/{repo}/contents/{urllib.parse.quote(path, safe='/')}?ref={sha}")
                lvgl_paths.extend(self.repository(module_url, info["sha"], module, version, lvgl_archive, depth+1, (*ancestors, (repo, ref))))
        return lvgl_paths

    def create(self, request):
        manifest = validate_manifest(request["manifest"])
        builtin_linux = manifest.get("uicreatorTemplate") == raspberry_pi_linux.TEMPLATE_ID
        if builtin_linux:
            raspberry_pi_linux.validate_request(request)
        destination = Path(request["destination"]).absolute()
        if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
            raise ValueError("Choose a new or empty destination folder")
        safe_relative(destination.name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        branch = request.get("branch", "")
        if branch and branch not in manifest.get("branches", []) and not request.get("allowCustomRef", False):
            raise ValueError("Branch is not offered by this template")
        version = request.get("lvglVersion", "template")
        if version != "template" and not re.fullmatch(r"v?\d+\.\d+\.\d+", version):
            raise ValueError("LVGL version must be a release version or template")
        workspace = request.get("workspace")
        with tempfile.TemporaryDirectory(prefix=".uicreator-project-", dir=destination.parent if not workspace else workspace) as temporary:
            stage = Path(temporary) / "project"
            linux_build = None
            source = request.get("sourceArchive")
            if builtin_linux:
                linux_build = raspberry_pi_linux.generate(request, stage, Path(temporary), extract_archive, inside)
                lvgl_paths = []
                self.repositories.append({"repository": "LVGL UI Creator Linux toolkit", "ref": "9.6.0", "source": "bundled"})
            elif source:
                stage.mkdir()
                extract_archive(Path(source).read_bytes(), stage)
                lvgl_paths = []
            else:
                lvgl_paths = self.repository(manifest["urlToClone"], branch, stage, version, request.get("lvglArchive"))
            explicit = request.get("lvglPath", "")
            if explicit:
                lvgl_paths = [inside(stage, explicit)]
            if not builtin_linux and version != "template" and (source or explicit or not lvgl_paths):
                if not lvgl_paths:
                    lvgl_paths = [p.parent for p in stage.rglob("lv_version.h") if "lvgl" in [s.lower() for s in p.relative_to(stage).parts] and p.name == "lv_version.h"]
                    lvgl_paths = list({next((parent for parent in p.parents if parent.name.lower() == "lvgl"), p) for p in lvgl_paths})
                if not lvgl_paths:
                    raise ValueError("This template manages LVGL externally; use its template version or specify its LVGL folder")
                for folder in lvgl_paths:
                    if not folder.resolve().is_relative_to(stage.resolve()):
                        raise ValueError("LVGL folder escapes project")
                    if folder.exists():
                        shutil.rmtree(folder)
                    folder.mkdir(parents=True)
                    archive = request.get("lvglArchive")
                    if archive:
                        extract_archive(Path(archive).read_bytes(), folder)
                    else:
                        self.repository("https://github.com/lvgl/lvgl", "v" + version.removeprefix("v"), folder)
                    verify_lvgl(folder, version)
            applied = [] if builtin_linux else apply_settings(stage, manifest, request.get("settings", {}))
            ui_path = manifest.get("uiFolderPath", "ui")
            ui = inside(stage, ui_path)
            if ui.exists() and any(p.name != ".gitkeep" for p in ui.iterdir()):
                raise ValueError("The template UI folder already contains files")
            ui.mkdir(parents=True, exist_ok=True)
            document = request.get("project", {"formatVersion": 2, "lvglVersion": "9.6.0", "obj": [], "variables": [], "canvas": {"width": 320, "height": 240}})
            atomic_json(ui / "project.json", document)
            ui_source = request.get("uiSourceFolder")
            if ui_source:
                for item in Path(ui_source).rglob("*"):
                    if item.is_symlink():
                        raise ValueError("Generated UI cannot contain symbolic links")
                    relative = item.relative_to(ui_source)
                    output = inside(ui, relative.as_posix())
                    if item.is_dir():
                        output.mkdir(parents=True, exist_ok=True)
                    else:
                        output.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(item, output)
            if builtin_linux:
                # The minimized library and the viewer use the same optimized
                # payload. Keep the full authored project in project.json.
                shutil.copy2(inside(stage, linux_build["optimizedProject"]), ui / "project.pb")
            instructions = manifest.get("getStartedInstructions", "")
            (stage / "UICREATOR_GETTING_STARTED.md").write_text(f"# {manifest['name']}\n\n{instructions}\n\nUI files: `{ui_path}`.\nFor generated C, add its CMake target to your board application, call ui_init() after LVGL/display setup and link generated_ui. For Protobuf, link the matching LVGLRuntime backend and load project.pb.\n", encoding="utf-8")
            result = {"name": manifest["name"], "destination": str(destination), "uiFolder": str(destination / ui_path), "repositories": self.repositories, "settings": applied, "lvglVersion": version, "instructions": instructions, "catalog": CATALOG_URL, "template": manifest, "selections": request.get("settings", {}), "branch": branch, "format": request.get("format", "json")}
            atomic_json(stage / "uicreator-project.json", result)
            if builtin_linux:
                result["linuxBuild"] = linux_build
                atomic_json(stage / "uicreator-project.json", result)
                raspberry_pi_linux.update_profile_manifest(stage)
            empty_existed = destination.exists()
            if empty_existed:
                destination.rmdir()
            try:
                raspberry_pi_linux.publish_directory(stage, destination)
            except Exception:
                if empty_existed and not destination.exists():
                    try:
                        destination.mkdir()
                    except FileExistsError:
                        pass  # A concurrent destination belongs to its creator.
                raise
            return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", required=True, type=Path)
    args = parser.parse_args()
    request = json.loads(args.request.read_text(encoding="utf-8"))
    cache = Path(request.get("cache", Path(os.environ.get("LOCALAPPDATA", Path.home() / ".cache")) / "UICreator" / "project-creator"))
    creator = Creator(cache)
    operation = request.get("operation", "create")
    if operation in ("catalog", "refresh"):
        result = {"catalog": creator.catalog(operation == "refresh")}
    elif operation == "preview":
        result = creator.preview(request["url"])
    elif operation == "create":
        result = creator.create(request)
    else:
        raise ValueError("Unknown project creator operation")
    atomic_json(args.request.parent / "result.json", result)
    print("Project creator completed", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr, flush=True)
        sys.exit(1)
