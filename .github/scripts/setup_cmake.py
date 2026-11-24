#!/usr/bin/env python3
"""Install the official Kitware CMake/CTest/CPack binaries, independently of PyPI."""
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import subprocess
import tarfile
import urllib.request
import zipfile

VERSION = '4.5.1'
ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS = {
    'linux-x86_64': ('tar.gz', '83b71181fb62b4fe8f26067a9321448572e7a43bd04ff6c7f54a24c33698f16d'),
    'linux-aarch64': ('tar.gz', '3bec208f11ed96c173032eeaf97937c54dd246acefee22acd7794a6b4e70d536'),
    'macos-universal': ('tar.gz', 'e1fb288543e4eb2a410e385ecebc64323bda51f0205782f2b094ccd5d572ca2d'),
    'windows-x86_64': ('zip', 'bfe6eadb245dc8213e0d3feea316b520cb2aae92f5667ffd76059ba6b05c430c'),
}


def main():
    system = platform.system()
    machine = platform.machine().lower()
    if system == 'Darwin':
        target = 'macos-universal'
    elif system == 'Windows' and machine in ('amd64', 'x86_64'):
        target = 'windows-x86_64'
    elif system == 'Linux' and machine in ('aarch64', 'arm64', 'x86_64', 'amd64'):
        target = 'linux-' + ('aarch64' if machine in ('aarch64', 'arm64') else 'x86_64')
    else:
        raise RuntimeError('Unsupported CMake host: ' + system + ' ' + machine)
    suffix, expected = ARTIFACTS[target]
    stem = 'cmake-' + VERSION + '-' + target
    filename = stem + '.' + suffix
    url = 'https://github.com/Kitware/CMake/releases/download/v' + VERSION + '/' + filename
    folder = ROOT / 'build' / 'ci-tools'
    folder.mkdir(parents=True, exist_ok=True)
    archive = folder / filename
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        data = urllib.request.urlopen(url, timeout=120).read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Official CMake archive checksum differs: ' + filename)
        archive.write_bytes(data)
    if suffix == 'zip':
        with zipfile.ZipFile(archive) as stream:
            for item in stream.infolist():
                path = PurePosixPath(item.filename)
                if path.is_absolute() or '..' in path.parts:
                    raise ValueError('Unsafe CMake ZIP path')
            stream.extractall(folder)
    else:
        with tarfile.open(archive) as stream:
            stream.extractall(folder, filter='data')
    binary = folder / stem / ('CMake.app/Contents/bin' if system == 'Darwin' else 'bin')
    for tool in ('cmake', 'ctest', 'cpack'):
        executable = binary / (tool + ('.exe' if system == 'Windows' else ''))
        version = subprocess.check_output([str(executable), '--version'], text=True).splitlines()[0]
        if version != tool + ' version ' + VERSION:
            raise ValueError('Unexpected downloaded tool version: ' + version)
        print(version, flush=True)
    for variable, line in (('GITHUB_PATH', str(binary)),
                           ('GITHUB_ENV', 'UICREATOR_CI_CMAKE_BIN=' + str(binary))):
        if destination := os.environ.get(variable):
            with Path(destination).open('a', encoding='utf-8') as stream:
                stream.write(line + '\n')
    (folder / 'cmake-receipt.json').write_text(json.dumps({'url': url, 'sha256': expected,
                                                         'bin': str(binary)}, indent=2) + '\n')
    print('Prepared official CMake toolset: ' + str(binary), flush=True)


if __name__ == '__main__':
    main()
