#!/usr/bin/env python3
"""Compile generated device profiles without connecting to or flashing hardware."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def run(command, cwd=ROOT):
    print('+ ' + subprocess.list2cmdline(list(map(str, command))), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=('esp32', 'arduino', 'pico', 'pico2'))
    args = parser.parse_args()
    work = ROOT / 'build' / ('ci-' + args.target)
    work.mkdir(parents=True, exist_ok=True)
    fixture = work / 'keyboard.pb'
    run([sys.executable, ROOT / 'Runtime/linux/tests/make_fixture.py', ROOT, fixture])
    if args.target == 'esp32':
        component = work / 'lvgl_pb'
        run([sys.executable, ROOT / 'Runtime/esp32/make_component.py', fixture, '--output', component])
        run(['idf.py', '-C', component / 'example', '-B', work / 'firmware', 'set-target', 'esp32'])
        run(['idf.py', '-C', component / 'example', '-B', work / 'firmware', 'build'])
    elif args.target == 'arduino':
        url = 'https://github.com/arduino/arduino-cli/releases/download/v1.3.1/arduino-cli_1.3.1_Linux_64bit.tar.gz'
        data = urllib.request.urlopen(url, timeout=120).read()
        if hashlib.sha256(data).hexdigest() != '376428d7d45be640c00812a71612e1742edc2f5f9ee3742a2d6da7870e079588':
            raise ValueError('Arduino CLI checksum mismatch')
        archive = work / 'arduino-cli.tar.gz'
        archive.write_bytes(data)
        cli = work / 'arduino-cli'
        with tarfile.open(archive) as stream:
            member = stream.getmember('arduino-cli')
            cli.write_bytes(stream.extractfile(member).read())
        cli.chmod(0o755)
        run([cli, 'core', 'update-index'])
        run([cli, 'core', 'install', 'arduino:esp32@2.0.18-arduino.5'])
        library = work / 'LVGLProtobuf'
        run([sys.executable, ROOT / 'Runtime/arduino/make_library.py', fixture, '--output', library])
        run([cli, 'compile', '--fqbn', 'arduino:esp32:nano_nora', '--jobs', '2',
             '--library', library, '--build-path', work / 'firmware', library / 'examples/ProtobufDisplay'])
    else:
        sdk = work / 'pico-sdk'
        run(['git', 'clone', '--filter=blob:none', 'https://github.com/raspberrypi/pico-sdk.git', sdk])
        run(['git', '-C', sdk, 'checkout', '--detach', '079c6f39023649b154152db30f1d781e884879bc'])
        run(['git', '-C', sdk, 'submodule', 'update', '--init', '--recursive', '--depth', '1'])
        project = work / 'project'
        run([sys.executable, ROOT / 'Runtime/pico/make_sdk.py', fixture, '--board', args.target, '--output', project])
        run(['cmake', '-S', project, '-B', work / 'firmware', '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=MinSizeRel', '-DPICO_SDK_PATH=' + str(sdk)])
        run(['cmake', '--build', work / 'firmware', '--parallel', '2'])
    outputs = []
    for suffix in ('*.elf', '*.bin', '*.uf2'):
        for path in sorted((work / 'firmware').rglob(suffix)):
            outputs.append({'file': path.relative_to(work).as_posix(), 'bytes': path.stat().st_size,
                            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    if not outputs:
        raise RuntimeError('Firmware compiler produced no expected output')
    if args.target in ('pico', 'pico2') and not any(item['file'].endswith('.uf2') for item in outputs):
        raise RuntimeError('The Pico build did not produce a UF2')
    (work / 'verification.json').write_text(json.dumps({'target': args.target, 'hardwareFlashed': False,
                                                     'artifacts': outputs}, indent=2) + '\n')


if __name__ == '__main__':
    main()
