#!/usr/bin/env python3
"""Prepare pinned macOS cross-build tools on a Linux x86_64 host."""
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shlex
import shutil
import struct
import subprocess
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zlib

DEFAULT_BASE = Path('/var/tmp/lvgl-ui-macos')
CMAKE_VERSION = '4.5.1'
CMAKE_SHA256 = '83b71181fb62b4fe8f26067a9321448572e7a43bd04ff6c7f54a24c33698f16d'
RUST_VERSION = '1.85.1'
RUSTUP_VERSION = '1.28.2'
RUSTUP_SHA256 = '20a06e644b0d9bd2fbdbfd52d42540bdde820ea7df86e92e533c073da0cdd43c'
LLVM_VERSION = '21.1.8'
OSXCROSS_COMMIT = '2b863452d2fcae541f9e677cb7627f353ab5ba0e'
SDK_VERSION = '15.4'
DARWIN_TARGET = 'darwin24.4'
DEPLOYMENT_TARGET = '13.5'
APPLE_CATALOG = ('https://swscan.apple.com/content/catalogs/others/'
    'index-26-15-14-13-12-10.16-10.15-10.14-10.13-10.12-10.11-10.10-10.9-'
    'mountainlion-lion-snowleopard-leopard.merged-1.sucatalog.gz')
SDK_URL = ('https://swcdn.apple.com/content/downloads/33/19/140-17812-A_21ZLMMLY4E/'
    'zu3xwktttpoe71qiawhzgzvqss6rovawsa/CLTools_macOSLMOS_SDK.pkg')
SDK_SHA256 = '07835d08fcdc4a57b930c9a7f5c91df18666d4fa246ae994c8850cfe5aab9753'
SDK_CATALOG_DIGEST = '87b7c0da1fcf0e7f1f9de75f5f8e87e91e02a8d3'
SDK_BYTES = 55803148
REPOSITORY = 'https://github.com/tpoechtrager/osxcross'
DEPENDENCIES = ['clang-21', 'llvm-21', 'lld-21', 'libarchive-tools', 'cpio', 'ninja-build',
                'bash', 'git', 'make', 'patch', 'sed', 'tar', 'gzip', 'xz-utils', 'bzip2']
COMPILER_RT_SOURCES = {
    'compiler-rt': 'dd54ae21aee1780fac59445b51ebff601ad016b31ac3a7de3b21126fd3ccb229',
    'cmake': '85735f20fd8c81ecb0a09abb0c267018475420e93b65050cc5b7634eab744de9',
    'llvm': 'd9022ddadb40a15015f6b27e6549a7144704ded8828ba036ffe4b8165707de21',
    'third-party': '7fe99424384aea529ffaeec9cc9dfb8b451fd1852c03fc109e426fe208a1f1a7',
}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def download(url, path, digest, candidates=()):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and sha256(path) == digest:
        return path
    for candidate in candidates:
        candidate = Path(candidate)
        if candidate.is_file() and sha256(candidate) == digest:
            shutil.copy2(candidate, path)
            return path
    temporary = path.with_name(path.name + '.partial')
    request = urllib.request.Request(url, headers={'User-Agent': 'LVGL-UI-Creator-macOS-Builder'})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open('wb') as stream:
        shutil.copyfileobj(response, stream, 1024 * 1024)
    if sha256(temporary) != digest:
        raise ValueError('Download checksum mismatch: ' + url)
    temporary.replace(path)
    return path


def extract_tar(archive, destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    if not hasattr(tarfile, 'data_filter'):
        raise RuntimeError('Use Python 3.12+ (or a Python with tarfile.data_filter) for safe archive extraction')
    with tarfile.open(archive) as stream:
        stream.extractall(destination, filter='data')


def run(command, log, environment=None, cwd=None):
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w') as stream:
        subprocess.run(list(map(str, command)), cwd=cwd, env=environment, check=True,
                       stdout=stream, stderr=subprocess.STDOUT)


def checked_base(base):
    base = Path(base).expanduser().resolve()
    repository = Path(__file__).resolve().parents[1]
    if base == Path(base.anchor) or base == repository or base in repository.parents:
        raise ValueError('Choose a dedicated macOS build cache directory')
    base.mkdir(parents=True, exist_ok=True)
    return base


def ensure_dependencies(base, install=True):
    llvm = Path('/usr/lib/llvm-21/bin')
    required = [llvm / name for name in ('clang', 'clang++', 'llvm-config', 'ld64.lld',
                'llvm-ar', 'llvm-ranlib', 'llvm-nm', 'llvm-lipo', 'llvm-libtool-darwin', 'llvm-otool',
                'llvm-objdump', 'llvm-readobj', 'llvm-install-name-tool')]
    missing = [str(path) for path in required if not path.is_file()]
    missing += [name for name in ('bsdtar', 'cpio', 'bash', 'git', 'make', 'ninja', 'patch',
                                 'sed', 'tar', 'gzip', 'xz', 'bzip2') if not shutil.which(name)]
    if missing:
        if not install:
            raise RuntimeError('Missing OSXCross prerequisites: ' + ', '.join(missing))
        if not shutil.which('apt-get'):
            raise RuntimeError('Install Clang/LLVM/LLD 21, libarchive-tools and cpio on this Linux host')
        command = ['apt-get', '--simulate', '--no-install-recommends', 'install', *DEPENDENCIES]
        result = subprocess.run(command, env=dict(os.environ, LC_ALL='C'), text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
        (base / 'apt-plan.log').write_text(result.stdout)
        summary = re.search(r'(\d+) upgraded, \d+ newly installed, (\d+) to remove', result.stdout)
        if not summary or any(int(value) for value in summary.groups()):
            raise RuntimeError('The prerequisite plan would upgrade/remove existing host packages; see apt-plan.log')
        prefix = [] if os.geteuid() == 0 else ['sudo']
        run([*prefix, 'apt-get', '-y', '--no-install-recommends', 'install', *DEPENDENCIES], base / 'apt-install.log')
    version = subprocess.check_output([str(llvm / 'clang'), '--version'], text=True).splitlines()[0]
    if not re.search(r'\b' + re.escape(LLVM_VERSION) + r'\b', version):
        raise RuntimeError('Expected Clang ' + LLVM_VERSION + ', found: ' + version)
    return {'version': LLVM_VERSION, 'bin': str(llvm), 'clang': str(llvm / 'clang'),
            'clangxx': str(llvm / 'clang++'), 'identity': version}


def prepare_host_tools(base):
    root = base / 'tools'
    downloads = base / 'downloads'
    previous = Path('/var/tmp/lvgl-ui-desktop/tools/downloads')
    cmake_name = 'cmake-' + CMAKE_VERSION + '-linux-x86_64.tar.gz'
    archive = download('https://github.com/Kitware/CMake/releases/download/v' + CMAKE_VERSION + '/' + cmake_name,
                       downloads / cmake_name, CMAKE_SHA256, [previous / cmake_name])
    cmake = root / cmake_name.removesuffix('.tar.gz') / 'bin/cmake'
    if not cmake.is_file():
        extract_tar(archive, root)
    if not subprocess.check_output([str(cmake), '--version'], text=True).startswith('cmake version ' + CMAKE_VERSION + '\n'):
        raise ValueError('Cached host CMake version differs')
    return {'cmake': str(cmake), 'artifacts': {cmake_name: CMAKE_SHA256}}


def xar_payload(package, destination):
    """Verify the pinned Apple XAR and return its PBZX payload bytes."""
    package = Path(package)
    if package.stat().st_size != SDK_BYTES or sha256(package) != SDK_SHA256:
        raise ValueError('Apple SDK package bytes do not match the pinned download')
    data = package.read_bytes()
    magic, header_size, version, toc_size, expanded_size, algorithm = struct.unpack('>4sHHQQI', data[:28])
    if magic != b'xar!' or header_size != 28 or version != 1 or toc_size > 1024 * 1024 or expanded_size > 4 * 1024 * 1024:
        raise ValueError('Unexpected Apple SDK XAR header')
    compressed = data[header_size:header_size + toc_size]
    if algorithm != 1 or hashlib.sha1(compressed).hexdigest() != SDK_CATALOG_DIGEST:
        raise ValueError('Apple catalog/XAR table-of-contents digest mismatch')
    toc = zlib.decompress(compressed)
    if len(toc) != expanded_size:
        raise ValueError('Invalid XAR table-of-contents size')
    root = ET.fromstring(toc)
    files = root.findall('./toc/file')
    if {item.findtext('name') for item in files} != {'Payload', 'Bom', 'PackageInfo'}:
        raise ValueError('Unexpected SDK package members')
    payload = None
    for item in files:
        attributes = item.find('data')
        offset, length = int(attributes.findtext('offset')), int(attributes.findtext('length'))
        start = header_size + toc_size + offset
        if offset < 0 or length < 0 or start + length > len(data):
            raise ValueError('XAR member escaped the package')
        body = data[start:start + length]
        checksum = attributes.find('archived-checksum')
        if checksum.get('style') != 'sha1' or hashlib.sha1(body).hexdigest() != checksum.text:
            raise ValueError('Apple XAR member checksum mismatch')
        if item.findtext('name') == 'Payload':
            payload = body
    if not payload:
        raise ValueError('The Apple package has no SDK payload')
    Path(destination).write_bytes(payload)
    return Path(destination)


def decode_pbzx(source, destination):
    def read_exact(stream, count):
        value = stream.read(count)
        if len(value) != count:
            raise ValueError('Truncated PBZX payload')
        return value

    total = 0
    with Path(source).open('rb') as stream, Path(destination).open('wb') as output:
        if stream.read(4) != b'pbzx':
            raise ValueError('Expected Apple PBZX payload')
        flags = struct.unpack('>Q', read_exact(stream, 8))[0]
        while flags & (1 << 24):
            flags, length = struct.unpack('>QQ', read_exact(stream, 16))
            if not 0 < length <= 64 * 1024 * 1024:
                raise ValueError('Unexpected PBZX chunk length')
            raw = read_exact(stream, length)
            if raw.startswith(b'\xfd7zXZ\x00'):
                decoder = lzma.LZMADecompressor(memlimit=256 * 1024 * 1024)
                chunk = decoder.decompress(raw, max_length=32 * 1024 * 1024 + 1)
                if not decoder.eof or decoder.unused_data:
                    raise ValueError('Invalid or oversized PBZX compressed chunk')
            else:
                chunk = raw
            total += len(chunk)
            if len(chunk) > 32 * 1024 * 1024 or total > 2 * 1024 * 1024 * 1024:
                raise ValueError('SDK payload exceeds extraction limits')
            output.write(chunk)
        if not total or stream.read(1):
            raise ValueError('Empty PBZX payload or trailing data')


def prepare_sdk(base, source):
    archive = source / ('tarballs/MacOSX' + SDK_VERSION + '.sdk.tar.xz')
    package = download(SDK_URL, base / 'downloads/CLTools_macOSLMOS_SDK.pkg', SDK_SHA256,
        [Path(__file__).resolve().parents[1] / 'build/macos-cross-discovery/CLTools_macOSLMOS_SDK.pkg'])
    marker = base / 'sdk-archive.json'
    try:
        cached = json.loads(marker.read_text()) if marker.is_file() else {}
    except (OSError, ValueError):
        cached = {}
    valid = (archive.is_file() and cached.get('packageSha256') == SDK_SHA256
             and cached.get('archiveSha256') == sha256(archive))
    if not valid:
        with tempfile.TemporaryDirectory(prefix='sdk-extract-', dir=base) as temporary:
            work = Path(temporary)
            payload = xar_payload(package, work / 'Payload')
            cpio = work / 'Payload.cpio'
            decode_pbzx(payload, cpio)
            listing = subprocess.check_output(['bsdtar', '-tf', str(cpio)], text=True).splitlines()
            if not listing:
                raise ValueError('The SDK payload is empty')
            for name in listing:
                path = PurePosixPath(name)
                if path.is_absolute() or '..' in path.parts:
                    raise ValueError('Unsafe SDK archive path: ' + name)
            extracted = work / 'root'
            extracted.mkdir(exist_ok=True)
            run(['bsdtar', '-xf', cpio, '-C', extracted], base / 'sdk-extract.log')
            sdk = extracted / ('Library/Developer/CommandLineTools/SDKs/MacOSX' + SDK_VERSION + '.sdk')
            settings = json.loads((sdk / 'SDKSettings.json').read_text())
            if settings.get('Version') != SDK_VERSION or not {'x86_64', 'arm64'} <= set(settings['SupportedTargets']['macosx']['Archs']):
                raise ValueError('The SDK does not support the expected architectures/version')
            environment = dict(os.environ, XZ_OPT='-T2')
            archive.parent.mkdir(parents=True, exist_ok=True)
            partial = archive.with_name(archive.name + '.partial')
            run(['tar', '--sort=name', '-cJf', partial, '-C', sdk.parent, sdk.name], base / 'sdk-package.log', environment)
            partial.replace(archive)
    result = {'version': SDK_VERSION, 'packageUrl': SDK_URL, 'packageSha256': SDK_SHA256,
            'packageBytes': SDK_BYTES, 'catalogUrl': APPLE_CATALOG,
            'catalogDigest': SDK_CATALOG_DIGEST, 'catalogDigestKind': 'xar-compressed-toc-sha1',
            'archive': str(archive), 'archiveSha256': sha256(archive)}
    temporary = marker.with_suffix('.json.partial')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(marker)
    return result


def checkout_osxcross(base):
    source = base / 'osxcross'
    fresh = not source.exists()
    if fresh:
        run(['git', 'clone', '--no-checkout', REPOSITORY, source], base / 'osxcross-clone.log')
        run(['git', '-C', source, 'checkout', '--detach', OSXCROSS_COMMIT], base / 'osxcross-checkout.log')
    head = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True)
    if dirty:
        raise RuntimeError('The OSXCross cache has local edits; use a new --base directory')
    if head != OSXCROSS_COMMIT:
        run(['git', '-C', source, 'checkout', '--detach', OSXCROSS_COMMIT], base / 'osxcross-checkout.log')
    return source


def prepare_osxcross(base, llvm, host, jobs):
    source = checkout_osxcross(base)
    sdk = prepare_sdk(base, source)
    prefix = base / 'toolchain'
    environment = {key: value for key, value in os.environ.items() if not key.startswith('OSXCROSS_')}
    for name in ('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CPATH', 'C_INCLUDE_PATH',
                 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'COMPILER_PATH', 'SDKROOT',
                 'SDK_DIR', 'TARBALL_DIR', 'BASE_DIR', 'BUILD_DIR', 'CMAKE_TOOLCHAIN_FILE'):
        environment.pop(name, None)
    environment.update(PATH=str(Path(host['cmake']).parent) + ':' + llvm['bin'] + ':/usr/bin:/bin',
        BUILD_FLAVOR='llvm', UNATTENDED='1', ENABLE_REPLACEMENT_LIPO='0', ENABLE_ARCHS='x86_64 arm64',
        SDK_VERSION=SDK_VERSION, CMAKE=host['cmake'],
        DEFAULT_DEPLOYMENT_TARGET=DEPLOYMENT_TARGET, TARGET_DIR=str(prefix), JOBS=str(max(1, jobs)),
        CC='clang', CXX='clang++')
    wrappers = [prefix / 'bin' / (arch + '-apple-' + DARWIN_TARGET + '-cmake') for arch in ('x86_64', 'arm64')]
    if not all(path.is_file() for path in wrappers):
        run(['bash', 'build.sh'], base / 'osxcross-build.log', environment, source)
    configuration = subprocess.check_output([str(prefix / 'bin/osxcross-conf')], env=environment, text=True)
    for name, expected in (('BUILD_FLAVOR', 'llvm'), ('DEFAULT_DEPLOYMENT_TARGET', DEPLOYMENT_TARGET),
                           ('TARGET', DARWIN_TARGET), ('SDK_VERSION', SDK_VERSION)):
        if 'export OSXCROSS_' + name + '="' + expected + '"' not in configuration:
            raise ValueError('The existing OSXCross cache has incompatible settings; use a new --base directory')
    sdk['path'] = str(prefix / ('SDK/MacOSX' + SDK_VERSION + '.sdk'))
    if json.loads((Path(sdk['path']) / 'SDKSettings.json').read_text()).get('Version') != SDK_VERSION:
        raise ValueError('Installed OSXCross SDK version differs')
    architectures = {}
    for arch in ('x86_64', 'arm64'):
        tool = prefix / 'bin' / (arch + '-apple-' + DARWIN_TARGET)
        architectures[arch] = {'processor': arch, 'cc': str(tool) + '-clang', 'cxx': str(tool) + '-clang++',
            'objc': str(tool) + '-clang', 'objcxx': str(tool) + '-clang++', 'cmakeLauncher': str(tool) + '-cmake',
            'ar': str(tool) + '-ar', 'ranlib': str(tool) + '-ranlib', 'nm': str(tool) + '-nm',
            'lipo': str(tool) + '-lipo', 'installNameTool': str(tool) + '-install_name_tool',
            'rustTarget': 'aarch64-apple-darwin' if arch == 'arm64' else 'x86_64-apple-darwin'}
        for name, path in architectures[arch].items():
            if name not in ('processor', 'rustTarget') and not Path(path).is_file():
                raise ValueError('Missing installed OSXCross tool: ' + path)
    return {'commit': OSXCROSS_COMMIT, 'repository': REPOSITORY, 'prefix': str(prefix), 'target': DARWIN_TARGET,
            'flavor': 'llvm'}, sdk, architectures


def prepare_rust(base):
    folder = base / 'rust'
    cargo = folder / 'cargo/bin/cargo'
    environment = dict(os.environ, RUSTUP_HOME=str(folder / 'rustup'), CARGO_HOME=str(folder / 'cargo'),
                       RUSTUP_TOOLCHAIN=RUST_VERSION)
    url = ('https://static.rust-lang.org/rustup/archive/' + RUSTUP_VERSION +
           '/x86_64-unknown-linux-gnu/rustup-init')
    digest = RUSTUP_SHA256
    installer = download(url, base / ('downloads/rustup-init-' + RUSTUP_VERSION), digest)
    installer.chmod(0o755)
    if not cargo.is_file():
        run([installer, '-y', '--no-modify-path', '--profile', 'minimal', '--default-toolchain', RUST_VERSION],
            base / 'rust-install.log', environment)
    rustup = cargo.with_name('rustup')
    run([rustup, 'target', 'add', '--toolchain', RUST_VERSION, 'x86_64-apple-darwin', 'aarch64-apple-darwin'],
        base / 'rust-targets.log', environment)
    identity = subprocess.check_output([str(cargo), '+1.85.1', '--version'], env=environment, text=True).strip()
    return {'version': RUST_VERSION, 'cargo': str(cargo), 'rustupHome': environment['RUSTUP_HOME'],
            'cargoHome': environment['CARGO_HOME'], 'identity': identity,
            'installer': {'version': RUSTUP_VERSION, 'url': url, 'sha256': digest},
            'targets': ['x86_64-apple-darwin', 'aarch64-apple-darwin']}


def compiler_driver_text(compiler, resource):
    return ('#!/bin/sh\n# Generated by LVGL UI Creator macOS tool bootstrap\nexec ' +
            shlex.quote(str(compiler)) + ' -resource-dir=' + shlex.quote(str(resource)) + ' "$@"\n')


def prepare_compiler_rt(base, llvm, host, osxcross, architectures, jobs):
    """Build real Darwin builtins, including Clang's OS availability checks.

    Only macOS builtins are needed. Keep the Linux compiler installation intact;
    local Clang launchers select an isolated resource directory automatically.
    """
    work = base / 'compiler-rt'
    work.mkdir(exist_ok=True)
    sources = {}
    artifacts = {}
    for name, digest in COMPILER_RT_SOURCES.items():
        filename = name + '-' + LLVM_VERSION + '.src.tar.xz'
        url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-' + LLVM_VERSION + '/' + filename
        archive = download(url, base / 'downloads' / filename, digest,
                           [DEFAULT_BASE / 'downloads' / filename])
        directory = work / filename.removesuffix('.tar.xz')
        if not directory.is_dir():
            extract_tar(archive, work)
        sources[name] = directory
        artifacts[filename] = {'url': url, 'sha256': digest}
    # compiler-rt's ARM pointer authentication implementation uses this sibling.
    third_party = work / 'third-party'
    if third_party.exists() and third_party.resolve() != sources['third-party'].resolve():
        raise ValueError('The compiler-rt third-party source link differs; use a new --base directory')
    if not third_party.exists():
        third_party.symlink_to(sources['third-party'], target_is_directory=True)
    prefix = Path(osxcross['prefix'])
    resource = prefix / ('lib/clang/' + LLVM_VERSION.split('.')[0])
    archive = resource / 'lib/darwin/libclang_rt.osx.a'
    marker = work / 'runtime.json'
    try:
        cached = json.loads(marker.read_text()) if marker.is_file() else {}
    except (OSError, ValueError):
        cached = {}
    environment = {key: value for key, value in os.environ.items() if not key.startswith('OSXCROSS_')}
    environment.update(PATH=':'.join([str(Path(host['cmake']).parent), str(prefix / 'bin'),
                       llvm['bin'], '/usr/bin', '/bin']), MACOSX_DEPLOYMENT_TARGET=DEPLOYMENT_TARGET)
    for name in ('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CPATH', 'C_INCLUDE_PATH',
                 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'COMPILER_PATH', 'SDKROOT', 'CMAKE_TOOLCHAIN_FILE',
                 'LLVM_DIR', 'LLVM_CMAKE_DIR', 'LLVM_CONFIG_PATH'):
        environment.pop(name, None)
    environment['SDKROOT'] = str(prefix / ('SDK/MacOSX' + SDK_VERSION + '.sdk'))
    host_resource = Path(subprocess.check_output([llvm['clang'], '-print-resource-dir'], text=True).strip())
    valid = (archive.is_file() and cached.get('artifacts') == artifacts
             and cached.get('archiveSha256') == sha256(archive))
    if not valid:
        libraries = []
        for arch, tools in architectures.items():
            directory = work / ('build-' + arch)
            command = [tools['cmakeLauncher'], '--fresh', '-S', sources['compiler-rt'], '-B', directory,
                '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DLLVM_COMMON_CMAKE_UTILS=' + str(sources['cmake']),
                '-DLLVM_MAIN_SRC_DIR=' + str(sources['llvm']), '-DCMAKE_LINKER=' + llvm['bin'] + '/ld64.lld',
                '-DCMAKE_AR=' + tools['ar'], '-DCMAKE_RANLIB=' + tools['ranlib'], '-DCMAKE_LIPO=' + tools['lipo'],
                '-DCMAKE_OSX_DEPLOYMENT_TARGET=' + DEPLOYMENT_TARGET, '-DDARWIN_osx_ARCHS=' + arch,
                '-DDARWIN_osx_BUILTIN_ARCHS=' + arch, '-DDARWIN_osx_SKIP_CC_KEXT=ON',
                '-DCOMPILER_RT_INCLUDE_TESTS=OFF']
            # Bootstrap against the original headers if a previous runtime cache is damaged.
            command += ['-DCMAKE_' + language + '_FLAGS=-resource-dir=' + str(host_resource)
                        for language in ('C', 'CXX', 'ASM')]
            command += ['-DCOMPILER_RT_ENABLE_' + item + '=OFF'
                        for item in ('IOS', 'TVOS', 'WATCHOS', 'XROS', 'MACCATALYST')]
            command += ['-DCOMPILER_RT_BUILD_' + item + '=OFF' for item in
                        ('SANITIZERS', 'XRAY', 'LIBFUZZER', 'PROFILE', 'CTX_PROFILE', 'MEMPROF', 'ORC', 'GWP_ASAN')]
            run(command, work / ('configure-' + arch + '.log'), environment)
            run([host['cmake'], '--build', directory, '--target', 'builtins', '--parallel', max(1, jobs)],
                work / ('build-' + arch + '.log'), environment)
            # LLVM lipo cannot combine fat archives whose members are archives;
            # combine the original thin builtins archives instead.
            libraries.append(directory / ('lib/builtins/libclang_rt.builtins_' + arch + '_osx.a'))
        archive.parent.mkdir(parents=True, exist_ok=True)
        partial = archive.with_name(archive.name + '.partial')
        run([architectures['x86_64']['lipo'], '-create', *libraries, '-output', partial],
            work / 'lipo.log', environment)
        partial.replace(archive)
    headers = resource / 'include'
    if headers.exists() and headers.resolve() != (host_resource / 'include').resolve():
        raise ValueError('The isolated Clang headers differ; use a new --base directory')
    if not headers.exists():
        headers.symlink_to(host_resource / 'include', target_is_directory=True)
    for name, compiler in (('clang', llvm['clang']), ('clang++', llvm['clangxx'])):
        driver = prefix / 'bin' / name
        content = compiler_driver_text(compiler, resource)
        if driver.exists() and driver.read_text() != content:
            raise ValueError('The local Clang driver has edits; use a new --base directory')
        if not driver.exists():
            temporary = driver.with_name(driver.name + '.partial')
            temporary.write_text(content)
            temporary.chmod(0o755)
            temporary.replace(driver)
    # Exercise availability checks newer than the deployment target so Clang
    # must emit the real runtime call rather than fold the condition away.
    (work / 'availability.c').write_text('int main(void) { if (__builtin_available(macOS 15.0, *)) return 0; return 1; }\n')
    (work / 'availability.m').write_text('#import <Foundation/Foundation.h>\nint main(void) { @autoreleasepool { '
        'if (@available(macOS 15.0, *)) { NSLog(@"macOS availability"); return 0; } return 1; } }\n')
    for arch, tools in architectures.items():
        for language in ('c', 'm'):
            target = work / ('availability-' + arch + '-' + language)
            command = [tools['cc'], work / ('availability.' + language), '-v', '-o', target]
            if language == 'm':
                command += ['-framework', 'Foundation']
            log = work / ('availability-' + arch + '-' + language + '.log')
            run(command, log, environment)
            symbols = subprocess.check_output([llvm['bin'] + '/llvm-nm', str(target)], env=environment, text=True)
            if str(archive) not in log.read_text() or not re.search(
                    r'^[0-9a-fA-F]+ [tT] ___isPlatformVersionAtLeast$', symbols, re.MULTILINE):
                raise ValueError('Darwin compiler-rt availability runtime was not linked for ' + arch)
    license_path = sources['compiler-rt'] / 'LICENSE.TXT'
    if not license_path.is_file():
        raise ValueError('The pinned compiler-rt source license is missing')
    result = {'version': LLVM_VERSION, 'artifacts': artifacts, 'resourceDir': str(resource),
              'archive': str(archive), 'archiveSha256': sha256(archive),
              'license': str(license_path), 'licenseSha256': sha256(license_path),
              'architectures': list(architectures), 'availabilityChecks': ['C', 'Objective-C']}
    temporary = marker.with_suffix('.json.partial')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(marker)
    return result


def prepare(base=DEFAULT_BASE, jobs=2, install_dependencies=True):
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        raise RuntimeError('The OSXCross bootstrap supports Linux x86_64 hosts (including WSL2)')
    base = checked_base(base)
    llvm = ensure_dependencies(base, install_dependencies)
    host = prepare_host_tools(base)
    osxcross, sdk, architectures = prepare_osxcross(base, llvm, host, jobs)
    compiler_rt = prepare_compiler_rt(base, llvm, host, osxcross, architectures, jobs)
    rust = prepare_rust(base)
    result = {'schemaVersion': 1, 'host': 'x86_64-linux', 'base': str(base), 'deploymentTarget': DEPLOYMENT_TARGET,
              **host, 'llvm': llvm, 'osxcross': osxcross, 'sdk': sdk, 'architectures': architectures,
              'compilerRt': compiler_rt, 'rust': rust}
    temporary = base / 'tools.json.partial'
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(base / 'tools.json')
    print('Prepared macOS Intel/Apple Silicon toolchain:', base / 'tools.json', flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, default=DEFAULT_BASE)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--no-install-dependencies', action='store_true')
    args = parser.parse_args()
    prepare(args.base, args.jobs, not args.no_install_dependencies)


if __name__ == '__main__':
    main()
