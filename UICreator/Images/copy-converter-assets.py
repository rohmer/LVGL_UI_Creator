"""Stage the frozen image converter without spawning CMake's directory copier.

NMake invokes this through the already-required build Python. Merging preserves
the upstream subfolder while updating runtime files; unchanged files retain
their timestamps. Errors fail the build with the affected source/destination.
"""

import argparse
import filecmp
from pathlib import Path
import shutil


def copy_file(source, destination):
    if Path(destination).is_file() and filecmp.cmp(source, destination, shallow=False):
        return destination
    return shutil.copy2(source, destination)


def copy_directory(source, destination):
    source = Path(source).resolve(strict=True)
    destination = Path(destination).resolve()
    if not source.is_dir():
        raise ValueError(f"Asset source is not a directory: {source}")
    if destination == source or source in destination.parents:
        raise ValueError(f"Asset destination must be outside its source: {destination}")
    shutil.copytree(source, destination, copy_function=copy_file, dirs_exist_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime")
    parser.add_argument("upstream")
    parser.add_argument("destination")
    arguments = parser.parse_args()
    copy_directory(arguments.runtime, arguments.destination)
    copy_directory(arguments.upstream, Path(arguments.destination) / "upstream")


if __name__ == "__main__":
    main()
