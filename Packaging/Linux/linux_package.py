#!/usr/bin/env python3
"""Audit native desktop payloads and test extracted CPack packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import zlib

MACHINES = {"x86_64": 62, "aarch64": 183}
TOOLS = ("UICreator", "LVGLCompiler", "UICreatorCodeGen", "UICreatorViewer")
HELPERS = ("font-converter", "image-converter", "project-creator", "language-models", "python-runtime", "integrations")
# SDL's X11/Wayland backends load these by name. They do not appear in ELF
# DT_NEEDED, so declare the desktop runtime contract explicitly in the DEB.
DESKTOP_DEPENDENCIES = ("libx11-6", "libxext6", "libxcursor1", "libxi6", "libxfixes3", "libxrandr2",
                        "libxrender1", "libxss1", "libxtst6", "libwayland-client0", "libwayland-cursor0",
                        "libwayland-egl1", "libxkbcommon0", "libegl1", "libgl1", "libglx0",
                        "libglvnd0", "libopengl0", "libgles2", "libgbm1", "libdrm2", "zenity",
                        "libcurl4t64 | libcurl4", "ca-certificates")
SDKS = (
    "LVGLProtobuf-ESP32-Toolkit.zip", "LVGLProtobuf-Arduino-Toolkit.zip", "LVGLProtobuf-Pico-Toolkit.zip",
    "LVGLProtobufDisplay-windows.zip", "LVGLProtobuf-Linux-Toolkit.zip",
    "LVGLProtobuf-Linux-Keyboard-Profile.zip", "LVGLProtobuf-Linux-aarch64-keyboard.tar.gz",
    "LVGLProtobuf-Linux-x86_64.tar.gz",
)
SYSTEM_LIBRARY = re.compile(r"^(?:ld-linux.*|lib(?:c|m|dl|pthread|rt|resolv|util|anl|nss_[^.]+|stdc\+\+|gcc_s|atomic)\.so.*|lib(?:GL|EGL|OpenGL|GLES|glapi|gbm|drm)[^.]*\.so.*)$")


def run(command, **kwargs):
    result = subprocess.run([str(part) for part in command], text=True, capture_output=True, timeout=120, **kwargs)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {' '.join(map(str, command))}\n{result.stdout}{result.stderr}")
    return result.stdout


def files(paths):
    seen = set()
    for path in paths:
        path = Path(path)
        if not path.exists():
            raise ValueError(f"Missing package input: {path}")
        for candidate in sorted(path.rglob("*")) if path.is_dir() else [path]:
            if candidate.is_file() and candidate not in seen:
                seen.add(candidate)
                yield candidate


def elf_machine(path):
    with path.open("rb") as source:
        header = source.read(20)
    if header.startswith(b"MZ"):
        raise ValueError(f"Windows native executable was staged in a Linux application: {path}")
    if not header.startswith(b"\x7fELF"):
        return None
    if len(header) != 20 or header[4] != 2 or header[5] != 1:
        raise ValueError(f"Expected a 64-bit little-endian ELF: {path}")
    return struct.unpack_from("<H", header, 18)[0]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def closure(paths, architecture, readelf, library_directories=(), sysroot=None, allowed_roots=()):
    """Resolve target ELF imports without consulting the build host's ld cache."""
    sysroot=Path(sysroot).resolve() if sysroot else None

    def canonical(path):
        # Resolve every path component in the target filesystem namespace.
        # Path.resolve() alone follows absolute directory symlinks into the
        # host, which can silently authorize a foreign library directory.
        absolute=Path(path).absolute()
        pending=list(absolute.parts[1:]);current=Path(absolute.anchor);links=0
        while pending:
            part=pending.pop(0)
            if part==".":
                continue
            if part=="..":
                if current!=sysroot:
                    current=current.parent
                continue
            candidate=current/part
            if candidate.is_symlink():
                links+=1
                if links>40:
                    raise ValueError(f"Dependency symlink loop: {path}")
                target=Path(os.readlink(candidate))
                if target.is_absolute():
                    current=sysroot if sysroot and candidate.is_relative_to(sysroot) else Path(target.anchor)
                    pending=list(target.parts[1:])+pending
                else:
                    pending=list(target.parts)+pending
            else:
                current=candidate
        return current

    paths=[canonical(path) for path in paths]
    seeds=[path for path in files(paths) if elf_machine(path) is not None]
    if not seeds:
        raise ValueError("No target ELF files found")
    expected=MACHINES[architecture]
    allowed=[canonical(path) for path in allowed_roots]
    payload_roots=[path if path.is_dir() else path.parent for path in paths]
    allowed.extend(payload_roots)
    directories=[canonical(path) for path in library_directories if path]
    allowed.extend(directories)
    triplet="aarch64-linux-gnu" if architecture=="aarch64" else "x86_64-linux-gnu"
    if sysroot:
        allowed.append(sysroot)
        directories.extend(sysroot/path for path in ("lib/"+triplet,"usr/lib/"+triplet,"lib64","usr/lib64","lib","usr/lib"))
    elif not directories:
        directories.extend(Path(path) for path in ("/lib/"+triplet,"/usr/lib/"+triplet,"/lib64","/usr/lib64","/lib","/usr/lib"))
        allowed.extend(canonical(path) for path in directories)
    directories.extend(path.parent for path in seeds)

    def resolve_target(candidate):
        target=canonical(candidate)
        if not target.is_file() or not any(target.is_relative_to(root) for root in allowed):
            return None
        if elf_machine(target)!=expected:
            return None
        return target

    pending=list(seeds);visited=set();libraries={}
    while pending:
        binary=pending.pop()
        if elf_machine(binary)!=expected:
            raise ValueError(f"Wrong ELF architecture in dependency closure: {binary}")
        real=canonical(binary)
        if real in visited:
            continue
        visited.add(real)
        dynamic=run([readelf,"--dynamic",binary],env=dict(os.environ,LC_ALL="C"))
        imports=re.findall(r"\(NEEDED\).*\[([^\]]+)\]",dynamic)
        rpaths=[]
        for value in re.findall(r"\((?:RPATH|RUNPATH)\).*\[([^\]]*)\]",dynamic):
            for directory in value.split(":"):
                if directory:
                    rpaths.append(Path(directory.replace("${ORIGIN}",str(binary.parent)).replace("$ORIGIN",str(binary.parent))))
        for dependency in imports:
            if SYSTEM_LIBRARY.fullmatch(Path(dependency).name):
                continue
            origin=next((value for value in ("$ORIGIN/","${ORIGIN}/") if dependency.startswith(value)),None)
            if "/" in dependency:
                if not origin:
                    raise ValueError(f"Nonrelocatable ELF import: {binary}: {dependency}")
                candidate=binary.parent/dependency[len(origin):]
                target=resolve_target(candidate)
                if target is None or not any(target.is_relative_to(root) for root in payload_roots):
                    raise ValueError(f"Missing, foreign or escaping origin-relative ELF import: {binary}: {dependency}")
            else:
                target=None
                for directory in (*rpaths,binary.parent,*directories):
                    target=resolve_target(directory/dependency)
                    if target is not None:
                        break
                if target is None:
                    raise ValueError(f"Cannot resolve target {architecture} dependency {dependency} required by {binary}; add its target sysroot directory")
            name=Path(dependency).name
            key=(str(target),name)
            libraries[key]={"source":str(target),"name":name}
            pending.append(target)
    records=sorted(libraries.values(),key=lambda item:(item["name"],item["source"]))
    information=abi([*paths,*(item["source"] for item in records)],architecture,readelf)
    return {"abi":information,"libraries":records}


def abi(paths, architecture, readelf):
    expected = MACHINES[architecture]
    roots = [Path(path).resolve() if Path(path).is_dir() else Path(path).resolve().parent for path in paths]
    glibc, glibcxx, cxxabi, needed = set(), set(), set(), set()
    native = []
    for path in files(paths):
        machine = elf_machine(path)
        if machine is None:
            continue
        if machine != expected:
            raise ValueError(f"Wrong ELF architecture in {path}: machine {machine}, expected {expected}")
        native.append(str(path))
        versions = run([readelf, "--version-info", path], env=dict(os.environ, LC_ALL="C"))
        # Definitions provided by a bundled library are not host requirements.
        versions = versions.split("Version needs section", 1)[-1] if "Version needs section" in versions else ""
        glibc.update(tuple(map(int, value.split("."))) for value in re.findall(r"\bGLIBC_([0-9]+(?:\.[0-9]+)+)", versions))
        if "GLIBC_ABI_DT_RELR" in versions:
            glibc.add((2, 36))
        glibcxx.update(tuple(map(int, value.split("."))) for value in re.findall(r"\bGLIBCXX_([0-9]+(?:\.[0-9]+)+)", versions))
        cxxabi.update(tuple(map(int, value.split("."))) for value in re.findall(r"\bCXXABI_([0-9]+(?:\.[0-9]+)+)", versions))
        imports = re.findall(r"\(NEEDED\).*\[([^\]]+)\]", run([readelf, "--dynamic", path], env=dict(os.environ, LC_ALL="C")))
        for dependency in imports:
            if "/" not in dependency:
                continue
            prefix = next((value for value in ("$ORIGIN/", "${ORIGIN}/") if dependency.startswith(value)), None)
            if not prefix:
                raise ValueError(f"Nonrelocatable path in ELF dependency: {path}: {dependency}")
            target = (path.parent / dependency[len(prefix):]).resolve()
            if not any(target.is_relative_to(root) for root in roots) or not target.is_file():
                raise ValueError(f"Origin-relative dependency escapes or is missing from payload: {path}: {dependency}")
            if elf_machine(target) != expected:
                raise ValueError(f"Wrong ELF architecture for origin-relative dependency: {target}")
        needed.update(imports)
    if not native:
        raise ValueError("No target ELF files found")
    dependencies = []
    minimum_glibc = ".".join(map(str, max(glibc))) if glibc else None
    minimum_glibcxx = ".".join(map(str, max(glibcxx))) if glibcxx else None
    if minimum_glibc:
        dependencies.append(f"libc6 (>= {minimum_glibc})")
    if "libstdc++.so.6" in needed or glibcxx:
        patch = (max(glibcxx) + (0, 0, 0))[2] if glibcxx else 0
        # https://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html
        gcc = next((version for threshold, version in ((34,"15.1"),(33,"14.1"),(32,"13.2"),(31,"13.1"),(30,"12.1"),(29,"11.1"),(28,"9.3"),(27,"9.2"),(26,"9.1"),(25,"8.1"),(24,"7.2"),(23,"7.1"),(22,"6.1"),(21,"5.1")) if patch >= threshold), "4.4")
        abi_patch = (max(cxxabi) + (0, 0, 0))[2] if cxxabi else 0
        abi_gcc = next((version for threshold, version in ((17,"16.1"),(16,"15.1"),(15,"14.1"),(14,"13.1"),(13,"11.1"),(12,"9.1"),(11,"7.1"),(10,"6.1"),(9,"5.1")) if abi_patch >= threshold), "4.4")
        if tuple(map(int, abi_gcc.split("."))) > tuple(map(int, gcc.split("."))):
            gcc = abi_gcc
        dependencies.append(f"libstdc++6 (>= {gcc})")
    if "libgcc_s.so.1" in needed:
        dependencies.append("libgcc-s1")
    if "libatomic.so.1" in needed:
        dependencies.append("libatomic1")
    dependencies.extend(DESKTOP_DEPENDENCIES)
    return {"glibc": minimum_glibc, "glibcxx": minimum_glibcxx, "cxxabi":".".join(map(str,max(cxxabi))) if cxxabi else None,"debianDepends": ", ".join(dependencies),
            "elfFiles": native, "needed": sorted(needed), "declaredDesktopDependencies": list(DESKTOP_DEPENDENCIES)}


def manifest(args):
    root = Path(args.root).resolve()
    app = root / "lib/lvgl-ui-creator"
    for name in TOOLS:
        executable = app / name
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise ValueError(f"Missing executable desktop tool: {executable}")
    for helper in HELPERS:
        if not (app / helper).is_dir():
            raise ValueError(f"Missing Linux helper: {helper}")
    for name in ("font-converter/node", "python-runtime/bin/python3", "image-converter/UICreatorImageTool", "project-creator/UICreatorProjectTool"):
        if not (app / name).is_file() or not os.access(app / name, os.X_OK):
            raise ValueError(f"Missing executable Linux helper: {name}")
    for name in ("font-converter/bridge.js", "image-converter/image-converter-bridge.py", "image-converter/upstream/LVGLImage.py",
                 "project-creator/project_creator.py", "project-creator/raspberry_pi_linux.py",
                 "project-creator/raspberry_pi_pico.py", "project-creator/pico-toolkit.zip", "project-creator/catalog.json",
                 "integrations/figma/manifest.json", "integrations/figma/code.js", "integrations/figma/ui.html"):
        if not (app / name).is_file():
            raise ValueError(f"Missing Linux helper resource: {name}")
    if args.require_sdks:
        archives = {path.name: path for path in (app / "sdk").rglob("*") if path.is_file()}
        for name in SDKS:
            if name not in archives:
                raise ValueError(f"Missing published SDK archive: {name}")
            sidecar = archives.get(name + ".sha256")
            if not sidecar or sidecar.read_text(encoding="utf-8-sig").split()[0].lower() != sha256(archives[name]):
                raise ValueError(f"SDK checksum is absent or mismatched: {name}")
    information = abi([root], args.architecture, args.readelf)
    information["elfFiles"] = [str(Path(path).relative_to(root)) for path in information["elfFiles"]]
    target = app / "package-manifest.json"
    records = {}
    for path in files([root]):
        if path == target:
            continue
        if path.is_symlink() and not path.resolve().is_relative_to(root):
            raise ValueError(f"Nonportable symlink escapes the staged prefix: {path}")
        record = {"sha256": sha256(path), "bytes": path.stat().st_size, "mode": path.stat().st_mode & 0o7777}
        if path.is_symlink():
            record["symlink"] = os.readlink(path)
        records[str(path.relative_to(root))] = record
    result = {"schema": "lvgl-ui-creator-linux-package", "version": 1, "release": args.version,
              "architecture": args.architecture, "abi": information, "files": records}
    target.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {args.architecture} desktop payload: {len(records)} files, {len(information['elfFiles'])} ELF files, {information['debianDepends']}")


def check_manifest(root, architecture, readelf):
    root = Path(root)
    app = root / "lib/lvgl-ui-creator"
    data = json.loads((app / "package-manifest.json").read_text())
    if data["architecture"] != architecture:
        raise ValueError("Package manifest architecture mismatch")
    for name, record in data["files"].items():
        path = root / name
        if not path.is_file() or sha256(path) != record["sha256"]:
            raise ValueError(f"Extracted package file differs from manifest: {name}")
        if "mode" in record and path.stat().st_mode & 0o7777 != record["mode"]:
            raise ValueError(f"Extracted package permissions differ from manifest: {name}")
        if not path.is_symlink() and path.stat().st_mode & 0o022:
            raise ValueError(f"Packaged file is writable by other users: {name}")
    inventory = {str(path.relative_to(root)) for path in files([root]) if path != app / "package-manifest.json"}
    if inventory != set(data["files"]):
        raise ValueError("Extracted package file inventory differs from its manifest")
    abi([root], architecture, readelf)
    if not (root / "share/applications/lvgl-ui-creator.desktop").is_file():
        raise ValueError("Desktop integration is absent")
    return app, data


def smoke(app, args, work):
    environment = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
                       UICREATOR_SESSION_DIR=str(work / "session"), PYTHONDONTWRITEBYTECODE="1")
    library_path = f"{app}/lib:{app}"
    def command(executable, *arguments):
        if args.emulator:
            return [args.emulator, "-L", args.sysroot, "-E", "LD_LIBRARY_PATH=" + library_path, app / executable, *arguments]
        environment["LD_LIBRARY_PATH"] = library_path
        return [app / executable, *arguments]
    # Exercise the actual packaged interpreters, imports and image conversion.
    # ARM runs native ELF interpreters through QEMU rather than host Python/Node.
    python = "python-runtime/bin/python3"
    run(command(python, "-I", "-c", "from PIL import Image; import lz4.frame,resvg_py; print('Bundled image dependencies loaded')"), env=environment, cwd=work)
    run(command(python, "-I", app / "project-creator/project_creator.py", "--help"), env=environment, cwd=work)
    run(command("font-converter/node", "-e", "require(process.argv[1]);require(process.argv[2]);",
                app / "font-converter/lib/parse_args.js", app / "font-converter/lib/convert.js"), env=environment, cwd=work)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    image = work / "image.png"
    image.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0))
                      + chunk(b"IDAT", zlib.compress((b"\0" + bytes((30, 180, 150, 255)) * 2) * 2)) + chunk(b"IEND", b""))
    image_job = work / "image-conversion";image_job.mkdir()
    request = image_job / "request.json"
    request.write_text(json.dumps({"sources":[str(image)],"options":{"colorFormat":"ARGB8888","format":"BIN","compression":"NONE","align":1}}))
    image_script = app / "image-converter/image-converter-bridge.py"
    launch = "import runpy,sys;from pathlib import Path;script=sys.argv.pop(1);sys.path.insert(0,str(Path(script).parent/'upstream'));runpy.run_path(script,run_name='__main__')"
    image_command = command(python,"-I","-c",launch,image_script,"--request",request) if args.emulator else [app / "image-converter/UICreatorImageTool", "--request", request]
    run(image_command,env=environment,cwd=work)
    image_result = json.loads((image_job / "result.json").read_text())
    if not image_result.get("assets") or not image_result.get("files"):
        raise ValueError("Packaged image converter produced no result")
    run(command("UICreator", "--smoke-test"), env=environment, cwd=work)
    project = work / "smoke.json"
    document={"formatVersion":2,"lvglVersion":"9.6.0","canvas":{"width":320,"height":240},"variables":[],
              "obj":[{"oid":1,"par":-1,"base":{"name":"Package label","coords":{"x":10,"y":10,"width":200,"height":40}},"text":"Packaged Linux editor"}]}
    project.write_text(json.dumps(document))
    protobuf = work / "smoke.pb"
    run(command("LVGLCompiler", project, "--format", "pb", "--output", protobuf), env=environment, cwd=work)
    run(command("UICreatorCodeGen", protobuf, work / "generated"), env=environment, cwd=work)
    # Exercise both installed XML frontends and the same packaged renderer.
    for frontend in ("UICreatorCodeGen", "LVGLCompiler"):
        xml = work / ("screen-" + frontend + ".xml")
        arguments = [protobuf, xml] if frontend == "UICreatorCodeGen" else [project, "--output", xml]
        run(command(frontend, *arguments, "--format", "xml"), env=environment, cwd=work)
        import xml.etree.ElementTree as etree
        if etree.parse(xml).getroot().tag != "uicreator":
            raise ValueError("Packaged XML export has an unexpected root")
        run(command("UICreatorViewer", xml, "--frames", "4", "--screenshot", work / (frontend + "-xml.bmp")), env=environment, cwd=work)
    for frontend in ("UICreatorCodeGen", "LVGLCompiler"):
        micro = work / ("generated-micropython-" + frontend)
        arguments = [protobuf, micro] if frontend == "UICreatorCodeGen" else [project, "--output", micro]
        run(command(frontend, "--format", "micropython", *arguments), env=environment, cwd=work)
        for name in ("ui.py", "ui.json", "uicreator_lvgl.py", "main.py", "README.md", "lv_conf.defaults.h", "export-manifest.json"):
            if not (micro / name).is_file() or not (micro / name).stat().st_size:
                raise ValueError("Packaged MicroPython export omitted " + name)
        manifest = json.loads((micro / "export-manifest.json").read_text())
        micro_document = json.loads((micro / "ui.json").read_text())
        if (manifest["format"] != "micropython" or manifest["lvglVersion"] != "9.3.0" or
                manifest["binding"]["repository"] != "https://github.com/lvgl/lv_binding_micropython"):
            raise ValueError("Packaged MicroPython exporter targets an unexpected binding")
        if micro_document["screens"][0]["obj"][0]["text"] != "Packaged Linux editor":
            raise ValueError("Packaged MicroPython export lost its authored widget")
        run(command(python, "-I", "-c", "from pathlib import Path;import sys;root=Path(sys.argv[1]);"
                    "[compile((root/name).read_text(encoding='utf-8'),name,'exec') "
                    "for name in ('ui.py','main.py','uicreator_lvgl.py')]", micro), env=environment, cwd=work)
    ui=work/"ui";ui.mkdir();shutil.copy2(project,ui/"project.json");shutil.copy2(protobuf,ui/"project.pb")
    project_job=work/"project-creation";project_job.mkdir()
    catalog=json.loads((app/"project-creator/catalog.json").read_text())
    template=next(item for item in catalog if item.get("uicreatorTemplate")=="raspberry-pi-linux")
    destination=work/"new-linux-project"
    project_request=project_job/"request.json"
    project_request.write_text(json.dumps({"operation":"create","manifest":template,"destination":str(destination),"project":document,
        "format":"protobuf","lvglVersion":"template","cache":str(work/"creator-cache"),"uiSourceFolder":str(ui),
        "linuxToolkitArchive":str(app/"project-creator/linux-toolkit.zip")}))
    project_launch="import runpy,sys;from pathlib import Path;script=sys.argv.pop(1);sys.path.insert(0,str(Path(script).parent));runpy.run_path(script,run_name='__main__')"
    project_command=command(python,"-I","-c",project_launch,app/"project-creator/project_creator.py","--request",project_request) if args.emulator else [app/"project-creator/UICreatorProjectTool","--request",project_request]
    run(project_command,env=environment,cwd=work)
    creation=json.loads((project_job/"result.json").read_text())
    if not creation.get("linuxBuild"):
        raise ValueError("Packaged project creator did not create a Linux build")
    for expected in ("CMakeLists.txt","ui/project.pb","embedded/features.json","embedded/config/lv_conf.h","platform/Runtime/linux/Viewer.cpp"):
        if not (destination/expected).is_file():
            raise ValueError("Packaged Linux project is incomplete: "+expected)
    original=sha256(destination/"ui/project.pb")
    duplicate=subprocess.run([str(part) for part in project_command],env=environment,cwd=work,text=True,capture_output=True,timeout=120)
    if duplicate.returncode==0 or sha256(destination/"ui/project.pb")!=original:
        raise ValueError("Project creator overwrote an existing destination")
    run(command("UICreatorViewer", protobuf, "--frames", "4", "--screenshot", work / "viewer.bmp"), env=environment, cwd=work)
    if not (work / "generated/ui.c").is_file() or not (work / "viewer.bmp").is_file():
        raise ValueError("Packaged compiler/code generator/viewer smoke outputs are absent")


def verify(args):
    package = Path(args.package).resolve()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Verify Unix permission metadata on a Linux filesystem even when reports
    # and final archives live on a Windows-mounted workspace.
    with tempfile.TemporaryDirectory(prefix="linux-package-") as directory:
        work = Path(directory)
        extracted = work / "extracted"
        extracted.mkdir()
        metadata = {}
        if package.suffix == ".deb":
            expected = "arm64" if args.architecture == "aarch64" else "amd64"
            for field in ("Package", "Architecture", "Version", "Depends"):
                metadata[field] = run(["dpkg-deb", "--field", package, field]).strip()
            if metadata["Architecture"] != expected or metadata["Package"] != "lvgl-ui-creator":
                raise ValueError(f"Wrong DEB identity: {metadata}")
            run(["dpkg-deb", "--extract", package, extracted])
        else:
            with tarfile.open(package) as archive:
                archive.extractall(extracted, filter="data")
        manifests = list(extracted.rglob("lib/lvgl-ui-creator/package-manifest.json"))
        if len(manifests) != 1:
            raise ValueError("Expected one complete desktop application in the package")
        prefix = manifests[0].parents[2]
        app, data = check_manifest(prefix, args.architecture, args.readelf)
        if metadata and metadata["Depends"] != data["abi"]["debianDepends"]:
            raise ValueError("DEB runtime dependency metadata differs from the measured target ELF ABI")
        if args.smoke:
            smoke(app, args, work)
            shutil.copy2(work / "viewer.bmp", output / (package.name + ".viewer.bmp"))
        report = {"package":package.name,"sha256":sha256(package),"architecture":args.architecture,
                  "files":len(data["files"]),"abi":data["abi"],"deb":metadata,
                  "smoke":"qemu" if args.smoke and args.emulator else "native" if args.smoke else "not run"}
        (output / (package.name + ".verification.json")).write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: {package.name}: architecture, every file hash, desktop integration" + (", editor/compiler/codegen/viewer and native conversion helpers" if args.smoke else ""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    dependency = subparsers.add_parser("dependencies")
    dependency.add_argument("--path", action="append", required=True)
    resolution=subparsers.add_parser("closure")
    resolution.add_argument("--path",action="append",required=True)
    resolution.add_argument("--library-directory",action="append",default=[])
    resolution.add_argument("--sysroot")
    resolution.add_argument("--allowed-root",action="append",default=[])
    creation = subparsers.add_parser("manifest")
    creation.add_argument("--root", required=True)
    creation.add_argument("--version", required=True)
    creation.add_argument("--require-sdks", action="store_true")
    verification = subparsers.add_parser("verify")
    verification.add_argument("--package", required=True)
    verification.add_argument("--output", required=True)
    verification.add_argument("--smoke", action="store_true")
    verification.add_argument("--emulator")
    verification.add_argument("--sysroot", default="/")
    for command in (dependency, resolution, creation, verification):
        command.add_argument("--architecture", choices=tuple(MACHINES), required=True)
        command.add_argument("--readelf", default="readelf")
    args = parser.parse_args()
    if args.command == "dependencies":
        print(json.dumps(abi(args.path, args.architecture, args.readelf)))
    elif args.command == "closure":
        print(json.dumps(closure(args.path,args.architecture,args.readelf,args.library_directory,args.sysroot,args.allowed_root)))
    elif args.command == "manifest":
        manifest(args)
    else:
        verify(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
