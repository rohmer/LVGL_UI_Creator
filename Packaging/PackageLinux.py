#!/usr/bin/env python3
"""Create and smoke-test separate Linux desktop packages from a native/cross build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def execute(command, environment=None):
    print("+ " + " ".join(map(str,command)), flush=True)
    subprocess.run([str(item) for item in command], check=True, env=environment)


def sha256(path):
    digest=hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda:source.read(1024*1024),b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory",required=True,type=Path)
    parser.add_argument("--architecture",required=True,choices=("x86_64","aarch64","arm64"))
    parser.add_argument("--cmake",default="cmake")
    parser.add_argument("--configuration",default="Release")
    parser.add_argument("--output",type=Path)
    parser.add_argument("--emulator")
    parser.add_argument("--sysroot",default="/")
    parser.add_argument("--readelf",default="readelf")
    parser.add_argument("--jobs",type=int,default=6)
    parser.add_argument("--skip-smoke",action="store_true",help="Audit payloads without executing their applications")
    args=parser.parse_args()
    build=args.build_directory.resolve()
    environment=os.environ.copy()
    metadata_path=build/"build-metadata.json"
    if metadata_path.is_file():
        metadata=json.loads(metadata_path.read_text())
        if metadata.get("architecture") != ("aarch64" if args.architecture=="arm64" else args.architecture):
            raise ValueError("Build metadata architecture does not match the requested package")
        # Regeneration must keep the same pkg-config sysroot and private Rust
        # toolchain as compilation, rather than discovering host libraries.
        environment.update(metadata.get("environment",{}))
    cmake=Path(shutil.which(args.cmake) or args.cmake).resolve()
    cpack=cmake.with_name("cpack")
    if not cmake.is_file() or not cpack.is_file():
        raise ValueError("Select a complete Linux CMake/CPack toolset with --cmake")
    version=subprocess.check_output([str(cmake),"--version"],text=True).splitlines()[0]
    cpack_version=subprocess.check_output([str(cpack),"--version"],text=True).splitlines()[0]
    if version.split()[-1]!=cpack_version.split()[-1]:
        raise ValueError("CMake and CPack versions differ")
    config=build/"CPackConfig.cmake"
    if not config.is_file():
        raise ValueError("Configure a complete Linux editor build before packaging")
    match=re.search(r'set\(CPACK_PACKAGE_VERSION "([^"]+)"\)',config.read_text())
    if not match:
        raise ValueError("Cannot determine configured package version")
    release=match[1]
    architecture="aarch64" if args.architecture=="arm64" else args.architecture
    suffix="arm64" if architecture=="aarch64" else "x86_64"
    output=(args.output or build/"packages").resolve();output.mkdir(parents=True,exist_ok=True)
    execute([cmake,"--build",build,"--config",args.configuration,"--target","UICreator","LVGLCompiler","UICreatorCodeGen","UICreatorViewer","--parallel",max(1,args.jobs)],environment)
    # DrvFS/NTFS may report every staged file as 0777 even after chmod. Keep
    # CPack's data tree on the Linux temporary filesystem, then atomically
    # publish the completed archives to the requested (possibly Windows) path.
    with tempfile.TemporaryDirectory(prefix="lvgl-ui-cpack-") as temporary:
        execute([cpack,"--config",config,"-C",args.configuration,"-G","DEB;TGZ","-B",temporary],environment)
        for extension in ("deb","tar.gz"):
            name=f"LVGL-UI-Creator-{release}-Linux-{suffix}.{extension}"
            source=Path(temporary)/name
            partial=output/(name+".partial")
            shutil.copy2(source,partial)
            if sha256(source)!=sha256(partial):
                raise ValueError("Published Linux package checksum mismatch: "+name)
            partial.replace(output/name)
    reports=[]
    audit=Path(__file__).parent/"Linux/linux_package.py"
    for extension in ("deb","tar.gz"):
        artifact=output/f"LVGL-UI-Creator-{release}-Linux-{suffix}.{extension}"
        digest=sha256(artifact)
        artifact.with_name(artifact.name+".sha256").write_text(f"{digest}  {artifact.name}\n")
        command=[sys.executable,audit,"verify","--package",artifact,"--architecture",architecture,"--readelf",args.readelf,"--output",output/"verification"]
        if not args.skip_smoke:
            command.append("--smoke")
        if args.emulator:
            command.extend(("--emulator",args.emulator,"--sysroot",args.sysroot))
        execute(command,environment)
        reports.append({"file":artifact.name,"bytes":artifact.stat().st_size,"sha256":digest})
    manifest={"schema":"lvgl-ui-creator-linux-artifacts","version":1,"release":release,"architecture":architecture,
              "cmake":version,"cpack":cpack_version,"packages":reports,"applicationSmoke":"skipped" if args.skip_smoke else "qemu" if args.emulator else "native"}
    (output/f"Linux-{suffix}.manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(f"Verified separate Linux {suffix} desktop packages in {output}")


if __name__=="__main__":
    try:
        main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        print(str(error),file=sys.stderr);sys.exit(1)
