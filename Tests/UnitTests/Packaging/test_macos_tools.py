"""Verify download recovery, safe SDK extraction and preserving toolchain edits."""
import hashlib
import importlib.util
import io
import lzma
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('macos_tools_under_test', ROOT / 'Packaging/BuildMacOSTools.py')
tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tools)


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='lvgl-macos-tools-')
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)

    def test_corrupt_cached_download_is_replaced_only_with_verified_bytes(self):
        destination = self.base / 'download'
        destination.write_bytes(b'corrupt cache')
        candidate = self.base / 'candidate'
        candidate.write_bytes(b'verified archive')
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        with patch.object(tools.urllib.request, 'urlopen', side_effect=AssertionError('Unexpected network')):
            tools.download('https://example.invalid/file', destination, digest, [candidate])
            tools.download('https://example.invalid/file', destination, digest)
        self.assertEqual(destination.read_bytes(), candidate.read_bytes())

    def test_failed_download_preserves_previous_file(self):
        destination = self.base / 'download'
        destination.write_bytes(b'previous file')
        with patch.object(tools.urllib.request, 'urlopen', return_value=io.BytesIO(b'wrong download')):
            with self.assertRaisesRegex(ValueError, 'checksum mismatch'):
                tools.download('https://example.invalid/file', destination, '0' * 64)
        self.assertEqual(destination.read_bytes(), b'previous file')

    def test_apple_pbzx_handles_raw_and_xz_chunks(self):
        payload = self.base / 'Payload'
        compressed = lzma.compress(b'compressed SDK bytes')
        payload.write_bytes(b'pbzx' + struct.pack('>Q', 1 << 24) +
                            struct.pack('>QQ', 1 << 24, 9) + b'raw bytes' +
                            struct.pack('>QQ', 0, len(compressed)) + compressed)
        output = self.base / 'Payload.cpio'
        tools.decode_pbzx(payload, output)
        self.assertEqual(output.read_bytes(), b'raw bytescompressed SDK bytes')

    def test_apple_pbzx_rejects_incomplete_or_unexpected_content(self):
        payload = self.base / 'Payload'
        cases = [b'pbzx', b'pbzx' + struct.pack('>Q', 0),
                 b'pbzx' + struct.pack('>Q', 1 << 24) + struct.pack('>QQ', 0, 50) + b'short',
                 b'pbzx' + struct.pack('>Q', 1 << 24) + struct.pack('>QQ', 0, 1) + b'xtrailing']
        for value in cases:
            with self.subTest(value=value):
                payload.write_bytes(value)
                with self.assertRaises(ValueError):
                    tools.decode_pbzx(payload, self.base / 'output')

    @unittest.skipUnless(tools.platform.system() == 'Linux', 'Requires the supported Linux shell')
    def test_clang_driver_preserves_resource_paths_and_compiler_arguments(self):
        compiler = self.base / "compiler's driver"
        compiler.write_text('#!/bin/sh\nfor argument do printf "[%s]\\n" "$argument"; done\n')
        compiler.chmod(0o755)
        resource = self.base / "resource's directory $literal"
        driver = self.base / 'clang'
        driver.write_text(tools.compiler_driver_text(compiler, resource))
        driver.chmod(0o755)
        arguments = ['-target', 'arm64-apple-darwin', 'literal value;$(no-command)']
        result = subprocess.check_output([str(driver), *arguments], text=True).splitlines()
        self.assertEqual(result, ['[-resource-dir=' + str(resource) + ']'] + ['[' + value + ']' for value in arguments])

    @unittest.skipUnless(hasattr(tarfile, 'data_filter'), 'Requires safe tar extraction support')
    def test_archive_cannot_escape_destination(self):
        for name in ('../escaped', '/absolute-escaped'):
            with self.subTest(name=name):
                archive = self.base / 'archive.tar'
                with tarfile.open(archive, 'w') as stream:
                    entry = tarfile.TarInfo(name)
                    entry.size = 7
                    stream.addfile(entry, io.BytesIO(b'content'))
                if name.startswith('/'):
                    # tarfile's data filter makes absolute members relative.
                    tools.extract_tar(archive, self.base / 'extracted')
                    self.assertTrue((self.base / 'extracted/absolute-escaped').is_file())
                else:
                    with self.assertRaises(tarfile.FilterError):
                        tools.extract_tar(archive, self.base / 'extracted')
                self.assertFalse((self.base / 'escaped').exists())

    @unittest.skipUnless(shutil.which('git') and tools.platform.system() == 'Linux',
                         'Requires the supported Linux host and git')
    def test_fresh_pinned_checkout_and_local_edits_are_preserved(self):
        repository = self.base / 'upstream'
        repository.mkdir()
        subprocess.run(['git', 'init', str(repository)], check=True, capture_output=True)
        (repository / 'source').write_text('upstream\n')
        subprocess.run(['git', '-C', str(repository), 'add', 'source'], check=True, capture_output=True)
        subprocess.run(['git', '-C', str(repository), '-c', 'user.name=Toolchain Test',
                        '-c', 'user.email=test@example.invalid', 'commit', '-m', 'fixture'],
                       check=True, capture_output=True)
        revision = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', 'HEAD'], text=True).strip()
        cache = self.base / 'cache'
        cache.mkdir()
        with patch.object(tools, 'REPOSITORY', str(repository)), patch.object(tools, 'OSXCROSS_COMMIT', revision):
            checkout = tools.checkout_osxcross(cache)
            self.assertEqual((checkout / 'source').read_text(), 'upstream\n')
            (checkout / 'source').write_text('local changes\n')
            with self.assertRaisesRegex(RuntimeError, 'local edits'):
                tools.checkout_osxcross(cache)
            self.assertEqual((checkout / 'source').read_text(), 'local changes\n')


if __name__ == '__main__':
    unittest.main()
