"""Synchronize a private helper directory, removing obsolete architecture files."""
import argparse
import filecmp
from pathlib import Path
import shutil


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('destination',type=Path)
    parser.add_argument('--upstream',type=Path)
    parser.add_argument('--keep',action='append',default=[])
    args=parser.parse_args()
    source=args.source.resolve(strict=True)
    destination=args.destination.resolve()
    if destination==source or source in destination.parents or destination in source.parents:
        raise ValueError('Helper staging source and destination must be separate directories')
    if destination.name not in ('image-converter','project-creator'):
        raise ValueError('Refusing to prune an unrecognized helper destination')
    inputs=[(source,Path())]
    if args.upstream:
        inputs.append((args.upstream.resolve(strict=True),Path('upstream')))
    expected=set(args.keep)
    for directory,prefix in inputs:
        for path in directory.rglob('*'):
            if not path.is_file():continue
            relative=prefix/path.relative_to(directory)
            expected.add(relative.as_posix())
            target=destination/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            if not target.is_file() or not filecmp.cmp(path,target,shallow=False):
                shutil.copy2(path,target)
    # Both paths were checked above; inspect each resolved deletion target too.
    for path in sorted(destination.rglob('*'),key=lambda path:len(path.parts),reverse=True):
        if not path.resolve().is_relative_to(destination):
            raise ValueError('Helper staging path escapes its destination')
        if path.is_file() and path.relative_to(destination).as_posix() not in expected:
            path.unlink()
        elif path.is_dir() and not any(path.iterdir()):
            path.rmdir()


if __name__=='__main__':main()
