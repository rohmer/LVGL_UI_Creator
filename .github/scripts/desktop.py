#!/usr/bin/env python3
"""Run the complete native desktop build/CTest suite on a clean CI runner."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(command, log, *, capture=False):
    command = list(map(str, command))
    print('+ ' + subprocess.list2cmdline(command), flush=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w', encoding='utf-8') as output:
        process = subprocess.Popen(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, errors='replace')
        for line in process.stdout:
            output.write(line)
            if not capture:
                print(line, end='', flush=True)
        code = process.wait()
    if code:
        if capture:
            print(log.read_text(encoding='utf-8'), flush=True)
        raise subprocess.CalledProcessError(code, command)
    return log.read_text(encoding='utf-8') if capture else ''


def check_inventory(directory, configuration, micropython):
    inventory = json.loads(run(['ctest', '--test-dir', directory, '-C', configuration,
                                '--show-only=json-v1'], directory / 'test-inventory.json', capture=True))
    names = {test['name'] for test in inventory['tests']}
    required = {prefix + version for prefix in ('LVGLGenerator', 'LVGLRuntimeBackend')
                for version in ('96', '95', '93', '92', '91', '83')}
    required |= {'UICreatorSmoke', 'UICreatorScreenMemory', 'UICreatorAdvancedKeyboard',
                 'UICreatorMicroPythonExport', 'UICreatorESP32Profile', 'UICreatorArduinoProfile',
                 'UICreatorPicoProfile', 'UICreatorKeyboardSyncSource', 'UICreatorMacOSPackaging',
                 'LVSerialProtobufConformance', 'UICreatorNativeConverters',
                 'UICreatorNativeImageConverter', 'UICreatorFontConverter'}
    if micropython:
        required.add('UICreatorMicroPythonNative')
    if missing := required - names:
        raise RuntimeError('CI coverage is missing registered tests: ' + ', '.join(sorted(missing)))
    print('Validated CI inventory: ' + str(len(names)) + ' registered tests', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--architecture', choices=('x86', 'x64', 'x86_64', 'arm64'), required=True)
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--phase', choices=('configure', 'build', 'test', 'all'), default='all')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--micropython', type=Path)
    args = parser.parse_args()
    directory = ROOT / 'build' / 'ci-desktop'
    os.environ.update(SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software')
    if args.phase in ('configure', 'all'):
        protoc = os.environ.get('UICREATOR_CI_PROTOC') or shutil.which('protoc')
        if not protoc or not Path(protoc).is_file():
            raise RuntimeError('The official protoc executable is required for the CI conformance test')
        protoc = Path(protoc).resolve()
        run([protoc, '--version'], directory / 'protoc-version.log')
        sources = ROOT / 'build' / 'compat-lvgl'
        run([sys.executable, ROOT / 'Tests/Integration/CodeGeneration/Versions/DownloadSources.py', sources],
            directory / 'download-lvgl.log')
        command = ['cmake', '-S', ROOT, '-B', directory, '-G', 'Ninja',
                   '-DCMAKE_BUILD_TYPE=' + args.configuration, '-DBUILD_TESTING=ON',
                   '-DUICREATOR_TEST_LVGL_VERSIONS=ON', '-DUICREATOR_LVGL_TEST_SOURCES=' + str(sources),
                   '-DPython3_EXECUTABLE=' + sys.executable,
                   '-DUICREATOR_PROTOC=' + str(protoc),
                   '-DFT_DISABLE_HARFBUZZ=ON',
                   '-DSDL_TESTS=OFF', '-DSDL_EXAMPLES=OFF']
        if platform.system() == 'Windows':
            command.append('-DUICREATOR_WINDOWS_ARCH=' + args.architecture)
        elif platform.system() == 'Darwin':
            command += ['-DCMAKE_OSX_ARCHITECTURES=' + args.architecture,
                        '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.5', '-DSDL_FRAMEWORK=OFF']
        else:
            command.append('-DUICREATOR_LINUX_REQUIRE_SDKS=OFF')
        if args.micropython:
            command.append('-DUICREATOR_MICROPYTHON_EXECUTABLE=' + str(args.micropython.resolve()))
        run(command, directory / 'configure.log')
        check_inventory(directory, args.configuration, args.micropython)
    if args.phase in ('build', 'all'):
        run(['cmake', '--build', directory, '--config', args.configuration, '--parallel', max(1, args.jobs)],
            directory / 'build.log')
    if args.phase in ('test', 'all'):
        check_inventory(directory, args.configuration, args.micropython)
        run(['ctest', '--test-dir', directory, '-C', args.configuration, '--output-on-failure',
             '--no-tests=error', '--parallel', '2', '--output-junit', str(directory / 'tests.xml')],
            directory / 'tests.log')


if __name__ == '__main__':
    main()
