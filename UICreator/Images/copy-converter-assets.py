"""Stage the frozen image converter without spawning CMake's directory copier.

NMake invokes this through the already-required build Python. Runtime assets
are merged and unchanged files retain their timestamps. The explicitly owned
upstream source folder is replaced so deleted source files cannot stay active.
Errors fail the build with the affected source/destination.
"""

import argparse
import filecmp
from pathlib import Path
import shutil
import tempfile


def copy_file(source, destination):
    if Path(destination).is_file() and filecmp.cmp(source, destination, shallow=False):
        return destination
    return shutil.copy2(source, destination)


def copy_directory(source, destination, ignore=None):
    source = Path(source).resolve(strict=True)
    destination = Path(destination).resolve()
    if not source.is_dir():
        raise ValueError(f"Asset source is not a directory: {source}")
    if destination == source or source in destination.parents or destination in source.parents:
        raise ValueError(f"Asset destination must be outside its source: {destination}")
    shutil.copytree(source, destination, copy_function=copy_file, dirs_exist_ok=True, ignore=ignore)


def stage_assets(runtime, upstream, destination):
    runtime, upstream = Path(runtime).resolve(strict=True), Path(upstream).resolve(strict=True)
    destination = Path(destination).resolve()
    owned = destination / "upstream"
    if owned.is_symlink() or owned.resolve() != owned:
        raise ValueError(f"Upstream staging directory must not redirect to another location: {owned}")
    if owned.exists() and not owned.is_dir():
        raise ValueError(f"Upstream staging destination is not a directory: {owned}")
    if owned == upstream or owned in upstream.parents or upstream in owned.parents:
        raise ValueError(f"Upstream staging destination must be outside its source: {owned}")
    # Runtime distributions can contain an older upstream copy. Its source is
    # authoritative below, while native files and unrelated assets remain merged.
    copy_directory(runtime, destination,
                   ignore=lambda directory, names: {"upstream"} if Path(directory) == runtime else set())
    with tempfile.TemporaryDirectory(prefix=".upstream-stage-", dir=destination) as temporary:
        staged = Path(temporary) / "upstream"
        copy_directory(upstream, staged)
        # The resolved absolute target is the precise owned child checked above.
        # Build and verify the replacement before removing that generated subtree.
        if owned.exists():
            shutil.rmtree(owned)
        staged.rename(owned)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime")
    parser.add_argument("upstream")
    parser.add_argument("destination")
    arguments = parser.parse_args()
    stage_assets(arguments.runtime, arguments.upstream, arguments.destination)


if __name__ == "__main__":
    main()
