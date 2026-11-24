#!/usr/bin/env python3
"""Install the checksum-pinned official Windows protoc host executable."""
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import platform
import subprocess
import time
import urllib.error
import urllib.request
import zipfile

VERSION = '36.2'
SHA256 = 'f0c128dc0d8492eceece83bb459a4c0e316764b929ffbf1aa416357fd644edd3'
ROOT = Path(__file__).resolve().parents[2]


def main():
    if platform.system() != 'Windows' or platform.machine().lower() not in ('amd64', 'x86_64'):
        raise RuntimeError('This protoc bootstrap requires a Windows x64 host')
    filename = 'protoc-' + VERSION + '-win64.zip'
    url = 'https://github.com/protocolbuffers/protobuf/releases/download/v' + VERSION + '/' + filename
    folder = ROOT / 'build' / 'ci-tools'
    folder.mkdir(parents=True, exist_ok=True)
    archive = folder / filename
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        for attempt in range(3):
            try:
                with urllib.request.urlopen(url, timeout=90) as response:
                    data = response.read(50 * 1024 * 1024 + 1)
                if len(data) > 50 * 1024 * 1024:
                    raise ValueError('Official protoc archive exceeds the expected size limit')
                if hashlib.sha256(data).hexdigest() != SHA256:
                    raise ValueError('Official protoc archive checksum differs')
                archive.write_bytes(data)
                break
            except (urllib.error.URLError, TimeoutError):
                if attempt == 2:
                    raise
                time.sleep(2 ** attempt)
    destination = folder / ('protoc-' + VERSION + '-win64')
    with zipfile.ZipFile(io.BytesIO(archive.read_bytes())) as stream:
        if sum(item.file_size for item in stream.infolist()) > 50 * 1024 * 1024:
            raise ValueError('Official protoc ZIP expansion exceeds the size limit')
        for item in stream.infolist():
            path = PurePosixPath(item.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in item.filename or ':' in item.filename:
                raise ValueError('Unsafe protoc ZIP path')
        stream.extractall(destination)
    executable = destination / 'bin' / 'protoc.exe'
    version = subprocess.check_output([str(executable), '--version'], text=True).strip()
    if version != 'libprotoc ' + VERSION:
        raise ValueError('Unexpected downloaded protoc version: ' + version)
    if environment := os.environ.get('GITHUB_ENV'):
        with Path(environment).open('a', encoding='utf-8') as stream:
            stream.write('UICREATOR_CI_PROTOC=' + str(executable) + '\n')
    print(version + ': ' + str(executable), flush=True)


if __name__ == '__main__':
    main()
