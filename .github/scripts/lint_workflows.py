#!/usr/bin/env python3
"""Run the checksum-pinned official actionlint binary against every workflow."""
import hashlib
import io
from pathlib import Path
import platform
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
windows = platform.system() == 'Windows'
name = 'actionlint_1.7.12_' + ('windows_amd64.zip' if windows else 'linux_amd64.tar.gz')
expected = ('6e7241b51e6817ea6a047693d8e6fed13b31819c9a0dd6c5a726e1592d22f6e9' if windows
            else '8aca8db96f1b94770f1b0d72b6dddcb1ebb8123cb3712530b08cc387b349a3d8')
data = urllib.request.urlopen('https://github.com/rhysd/actionlint/releases/download/v1.7.12/' + name, timeout=120).read()
if hashlib.sha256(data).hexdigest() != expected:
    raise ValueError('actionlint download checksum differs')
folder = ROOT / 'build' / 'ci-validation'
folder.mkdir(parents=True, exist_ok=True)
binary = folder / ('actionlint.exe' if windows else 'actionlint')
if windows:
    with zipfile.ZipFile(io.BytesIO(data)) as stream:
        binary.write_bytes(stream.read('actionlint.exe'))
else:
    with tarfile.open(fileobj=io.BytesIO(data)) as stream:
        binary.write_bytes(stream.extractfile('actionlint').read())
binary.chmod(0o755)
subprocess.run([str(binary), '-color', '-shellcheck=', '-pyflakes=',
                *map(str, sorted((ROOT / '.github/workflows').glob('*.yml')))], check=True, cwd=ROOT)
