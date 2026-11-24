"""Replace the owned project-helper runtime without retaining retired image modules."""
import argparse
import os
from pathlib import Path
import shutil
import tempfile
import time


def rename_directory(source, destination):
    for attempt in range(8):
        try:
            source.rename(destination)
            return
        except PermissionError as error:
            if os.name != 'nt' or error.winerror not in (5, 32, 33) or attempt == 7:
                raise
            time.sleep(0.01 * 2 ** attempt)


def stage(source, output):
    source, output = Path(source).resolve(strict=True), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    destination = output / 'python-runtime'
    if (not source.is_dir() or source == destination.resolve() or
            source.is_relative_to(destination.resolve()) or
            destination.resolve().is_relative_to(source) or destination.is_symlink() or
            destination.resolve().parent != output):
        raise ValueError('Project runtime source/destination overlap or escape their owned roots')
    for path in source.rglob('*'):
        if path.is_symlink() and not path.resolve().is_relative_to(source):
            raise ValueError('Project runtime symlink escapes its source')
    if destination.exists() and not destination.is_dir():
        raise ValueError('Project runtime destination must be a directory')
    root = Path(tempfile.mkdtemp(prefix='.project-runtime-stage-', dir=output))
    cleanup = True
    try:
        pending, previous = root / 'pending', root / 'previous'
        shutil.copytree(source, pending, symlinks=True)
        existed = destination.exists()
        if existed:
            rename_directory(destination, previous)
        try:
            rename_directory(pending, destination)
        except BaseException:
            if existed:
                try:
                    rename_directory(previous, destination)
                except OSError as error:
                    cleanup = False
                    raise RuntimeError('Previous project runtime preserved at ' + str(previous)) from error
            raise
    finally:
        if cleanup:
            shutil.rmtree(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output_directory', type=Path)
    args = parser.parse_args()
    stage(args.source, args.output_directory)


if __name__ == '__main__':
    main()
