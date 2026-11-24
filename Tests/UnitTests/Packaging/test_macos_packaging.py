"""Cross-platform Mach-O integrity, safe asset paths and package preservation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Packaging'))
import PackageMacOS as package
from MacOS import assets, macho
stager_spec = importlib.util.spec_from_file_location('image_asset_stager', ROOT / 'UICreator/Images/copy-converter-assets.py')
image_stager = importlib.util.module_from_spec(stager_spec)
stager_spec.loader.exec_module(image_stager)


def signed_fixture(info=b'info', resources=b'resources', embedded=False):
    """A real SHA256 CodeDirectory with one page and seven special slots."""
    entitlements = plistlib.dumps({'com.apple.security.cs.allow-jit': True})
    blobs = {2: struct.pack('>III', 0xFADE0C01, 12, 0),
             5: struct.pack('>II', 0xFADE7171, len(entitlements) + 8) + entitlements,
             7: struct.pack('>II', 0xFADE7172, 12) + b'der!'}
    identifier = b'fixture\0'
    hash_offset = 96 + len(identifier) + 7 * 32
    directory_size = hash_offset + 32
    signature_size = 12 + 4 * 8 + directory_size + sum(len(value) for value in blobs.values())
    commands = struct.pack('<6I', 0x32, 24, 1, 0x000D0500, 0x000F0400, 0)
    commands += struct.pack('<4I', 0x1D, 16, 4096, signature_size)
    if embedded:
        commands += struct.pack('<II16s4Q4I', 0x19, 152, b'__TEXT', 0, 4096, 0, 4096, 7, 5, 1, 0)
        commands += struct.pack('<16s16sQQ8I', b'__info_plist', b'__TEXT', 256, len(info), 256, 0, 0, 0, 0, 0, 0, 0)
    header = struct.pack('<8I', 0xFEEDFACF, 0x0100000C, 0, 2, 3 if embedded else 2, len(commands), 0, 0)
    prefix = (header + commands).ljust(4096, b'\0')
    if embedded:
        prefix = prefix[:256] + info + prefix[256 + len(info):]
    special = {1: info, 2: blobs[2], 3: resources, 5: blobs[5], 7: blobs[7]}
    hashes = b''.join(hashlib.sha256(special[slot]).digest() if slot in special else bytes(32)
                      for slot in range(7, 0, -1))
    directory = struct.pack('>9I4BI', 0xFADE0C02, directory_size, 0x20500, 0x10002,
                            hash_offset, 96, 7, 1, 4096, 32, 2, 0, 12, 0)
    directory += bytes(44) + struct.pack('>II', 0x000F0000, 0)
    directory += identifier + hashes + hashlib.sha256(prefix).digest()
    blobs[0] = directory
    table, payload = b'', b''
    for kind, blob in sorted(blobs.items()):
        table += struct.pack('>II', kind, 12 + 4 * 8 + len(payload))
        payload += blob
    return prefix + struct.pack('>III', 0xFADE0CC0, signature_size, 4) + table + payload


class MacOSPackageTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='lvgl-macos-package-test-')
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)

    def fixture(self):
        path = self.base / 'program'
        path.write_bytes(signed_fixture())
        info, resources = self.base / 'Info.plist', self.base / 'CodeResources'
        info.write_bytes(b'info')
        resources.write_bytes(b'resources')
        return path, {1: info, 3: resources}

    def test_native_metadata_and_complete_signature_seals(self):
        path, external = self.fixture()
        item, = macho.inspect(path)
        self.assertEqual((item['architecture'], item['minimumMacOS']), ('arm64', '13.5.0'))
        details = macho.signature_details(path, item)
        self.assertEqual(details['flags'], 0x10002)
        self.assertEqual(details['runtimeVersion'], '15.0.0')
        self.assertEqual(details['entitlements'], {'com.apple.security.cs.allow-jit': True})
        result = macho.verify_code_pages(path, item, external)
        self.assertEqual(result['codeDirectories'][0]['specialSlots'], [1, 2, 3, 5, 7])
        self.assertEqual(result['codeDirectories'][0]['pages'], 1)

    def test_modified_native_code_is_rejected(self):
        path, external = self.fixture()
        value = bytearray(path.read_bytes())
        value[100] ^= 1
        path.write_bytes(value)
        with self.assertRaisesRegex(ValueError, 'code page does not match'):
            macho.verify_code_pages(path, macho.inspect(path)[0], external)

    def test_modified_entitlement_is_rejected(self):
        path, external = self.fixture()
        value = path.read_bytes().replace(b'allow-jit', b'allow-zip')
        path.write_bytes(value)
        with self.assertRaisesRegex(ValueError, 'special slot 5'):
            macho.verify_code_pages(path, macho.inspect(path)[0], external)

    def test_modified_bundle_resources_or_missing_external_seal_are_rejected(self):
        path, external = self.fixture()
        with self.assertRaisesRegex(ValueError, 'Missing source'):
            macho.verify_code_pages(path, macho.inspect(path)[0])
        external[3].write_bytes(b'modified resources')
        with self.assertRaisesRegex(ValueError, 'special slot 3'):
            macho.verify_code_pages(path, macho.inspect(path)[0], external)

    def test_embedded_info_plist_seal_is_verified_without_external_bundle_info(self):
        path, external = self.fixture()
        path.write_bytes(signed_fixture(embedded=True))
        external.pop(1)
        item, = macho.inspect(path)
        self.assertEqual(item['embeddedInfoPlist'], {'offset': 256, 'size': 4})
        macho.verify_code_pages(path, item, external)
        value = bytearray(path.read_bytes())
        value[256] ^= 1
        path.write_bytes(value)
        with self.assertRaisesRegex(ValueError, 'special slot 1'):
            macho.verify_code_pages(path, macho.inspect(path)[0], external)

    def test_universal_slice_bounds_and_overlaps_are_rejected(self):
        path = self.base / 'universal'
        path.write_bytes(b'\xca\xfe\xba\xbe')
        with self.assertRaisesRegex(ValueError, 'Truncated universal'):
            macho.inspect(path)
        thin = signed_fixture()
        entry = struct.pack('>5I', 0x0100000C, 0, 48, len(thin), 0)
        path.write_bytes(struct.pack('>II', 0xCAFEBABE, 2) + entry * 2 + thin)
        with self.assertRaisesRegex(ValueError, 'Overlapping universal'):
            macho.inspect(path)

    def test_download_paths_cannot_escape_and_wheels_accept_both_native_architectures(self):
        for name in ('../escape', '/absolute', 'a\\escape', 'C:drive/file'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                assets.safe_name(name)
        self.assertEqual(str(assets.safe_name('module/package.py')), 'module/package.py')
        self.assertIn('macosx_11_0_arm64', assets.wheel_platforms('arm64'))
        self.assertIn('macosx_10_9_universal2', assets.wheel_platforms('arm64'))
        self.assertIn('macosx_10_9_x86_64', assets.wheel_platforms('x86_64'))

    def test_exact_loader_rpath_resolves_installed_dylib_before_different_build_copy(self):
        bundle = self.base / package.APP_NAME
        frameworks = bundle / 'Contents/Frameworks'
        frameworks.mkdir(parents=True)
        internal = frameworks / 'libfixture.dylib'
        internal.write_bytes(signed_fixture())
        external = self.base / 'build'
        external.mkdir()
        (external / internal.name).write_bytes(signed_fixture() + b'different build/install load commands')
        consumer = frameworks / 'consumer.dylib'
        files = {consumer: [{'rpaths': ['@loader_path']}], internal: macho.inspect(internal)}
        resolved = package.dependency_target('@rpath/libfixture.dylib', consumer, bundle, files, [external])
        self.assertEqual(resolved, internal.resolve())

    def helper_source(self):
        source = self.base / 'source'
        files = {
            'UICreator/Images/image-converter-bridge.py': b'print("image v1")\n',
            '3rdParty/lvgl_image_converter/LVGLImage.py': b'print("upstream v1")\n',
            '3rdParty/lvgl_image_converter/deleted.py': b'deleted later\n',
            'UICreator/ProjectCreator/project_creator.py': b'print("project v1")\n',
            'UICreator/ProjectCreator/raspberry_pi_linux.py': b'linux\n',
            'UICreator/ProjectCreator/raspberry_pi_pico.py': b'pico\n',
        }
        for name, value in files.items():
            target = source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(value)
        return source

    def test_incremental_helper_refresh_uses_current_source_without_network_or_runtime_changes(self):
        source = self.helper_source()
        cache = self.base / 'cache'
        prepared = {}
        for architecture in assets.ARCHITECTURES:
            root = cache / 'assets' / architecture
            for name, value in {'python-runtime/bin/python3': b'signed target Python',
                                'node/bin/node': b'signed target Node',
                                'python-dependencies.json': b'prepared wheels metadata',
                                'assets.json': b'prepared asset paths'}.items():
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(value)
                prepared[target] = (value, target.stat().st_mtime_ns)
        with mock.patch.object(assets, 'fetch', side_effect=AssertionError('Unexpected network')), \
             mock.patch.object(assets, 'download', side_effect=AssertionError('Unexpected download')), \
             mock.patch.object(assets.subprocess, 'run', side_effect=AssertionError('Unexpected runtime preparation')):
            first = assets.refresh_helpers(cache, source)
            (source / 'UICreator/Images/image-converter-bridge.py').write_bytes(b'print("image v2")\n')
            (source / 'UICreator/ProjectCreator/project_creator.py').write_bytes(b'print("project v2")\n')
            (source / '3rdParty/lvgl_image_converter/LVGLImage.py').write_bytes(b'print("upstream v2")\n')
            (source / '3rdParty/lvgl_image_converter/deleted.py').unlink()
            for paths in first.values():
                (Path(paths['imageToolDirectory']) / 'UICreatorImageTool').write_text('stale wrapper')
            refreshed = assets.refresh_helpers(cache, source)
        self.assertEqual(first, refreshed)
        for architecture, paths in refreshed.items():
            image, project = Path(paths['imageToolDirectory']), Path(paths['projectToolDirectory'])
            self.assertEqual((image / 'image-converter-bridge.py').read_bytes(), b'print("image v2")\n')
            self.assertEqual((project / 'project_creator.py').read_bytes(), b'print("project v2")\n')
            self.assertEqual((image / 'upstream/LVGLImage.py').read_bytes(), b'print("upstream v2")\n')
            self.assertFalse((image / 'upstream/deleted.py').exists())
            self.assertIn('../python-runtime/bin/python3', (image / 'UICreatorImageTool').read_text())
            self.assertIn(' -I -B -c ', (image / 'UICreatorImageTool').read_text())
            self.assertIn(' -I -B -c ', (project / 'UICreatorProjectTool').read_text())
            if sys.platform != 'win32':
                self.assertTrue((image / 'UICreatorImageTool').stat().st_mode & 0o111)
        for target, (value, modified) in prepared.items():
            self.assertEqual(target.read_bytes(), value)
            self.assertEqual(target.stat().st_mtime_ns, modified)

    def test_failed_helper_refresh_keeps_previous_generated_scripts(self):
        source = self.helper_source()
        cache = self.base / 'cache'
        helpers = assets.refresh_helpers(cache, source, ('arm64',))['arm64']
        image = Path(helpers['imageToolDirectory']) / 'image-converter-bridge.py'
        previous = image.read_bytes()
        (source / 'UICreator/Images/image-converter-bridge.py').write_bytes(b'new source')
        (source / 'UICreator/ProjectCreator/raspberry_pi_pico.py').unlink()
        with self.assertRaises(FileNotFoundError):
            assets.refresh_helpers(cache, source, ('arm64',))
        self.assertEqual(image.read_bytes(), previous)

    def test_runtime_staging_removes_deleted_upstream_but_preserves_other_assets_and_native_runtime(self):
        runtime, source, output = self.base / 'prepared-image', self.base / 'upstream-source', self.base / 'bin/image-converter'
        (runtime / '_internal').mkdir(parents=True)
        (runtime / '_internal/native-runtime').write_bytes(b'signed native runtime')
        (runtime / 'UICreatorImageTool').write_bytes(b'image wrapper')
        (runtime / 'upstream').mkdir()
        (runtime / 'upstream/deleted.py').write_bytes(b'stale cached source')
        (source / 'removed-directory').mkdir(parents=True)
        (source / 'module.py').write_bytes(b'original source')
        (source / 'deleted.py').write_bytes(b'delete later')
        (source / 'removed-directory/module.py').write_bytes(b'delete directory later')
        image_stager.stage_assets(runtime, source, output)
        native = output / '_internal/native-runtime'
        preserved = {native: (native.read_bytes(), native.stat().st_mtime_ns)}
        for target, value in {output / 'unrelated-asset': b'keep unrelated root asset',
                              output.parent / 'python-runtime/bin/python3': b'keep sibling Python',
                              output.parent / 'project-creator/pico-toolkit.zip': b'keep project SDK'}.items():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(value)
            preserved[target] = (value, target.stat().st_mtime_ns)
        (source / 'module.py').write_bytes(b'updated source')
        (source / 'deleted.py').unlink()
        (source / 'removed-directory/module.py').unlink()
        (source / 'removed-directory').rmdir()
        image_stager.stage_assets(runtime, source, output)
        self.assertEqual((output / 'upstream/module.py').read_bytes(), b'updated source')
        self.assertFalse((output / 'upstream/deleted.py').exists())
        self.assertFalse((output / 'upstream/removed-directory').exists())
        for target, (value, modified) in preserved.items():
            self.assertEqual(target.read_bytes(), value)
            self.assertEqual(target.stat().st_mtime_ns, modified)

    def test_bundle_resource_hash_seals_detect_modifications(self):
        bundle = self.base / package.APP_NAME
        contents = bundle / 'Contents'
        (contents / '_CodeSignature').mkdir(parents=True)
        (contents / 'Resources').mkdir()
        resource = contents / 'Resources/data'
        resource.write_bytes(b'sealed')
        (contents / '_CodeSignature/CodeResources').write_bytes(plistlib.dumps({
            'files2': {'Resources/data': {'hash2': hashlib.sha256(b'sealed').digest()}}}))
        self.assertEqual(package.verify_resource_seal(bundle, [])['sealedResources'], 1)
        resource.write_bytes(b'modified')
        with self.assertRaisesRegex(ValueError, 'resource hash does not match'):
            package.verify_resource_seal(bundle, [])

    def test_nested_code_directory_hash_seal_detects_modifications(self):
        path, external = self.fixture()
        directory_hash = macho.verify_code_pages(path, macho.inspect(path)[0], external)['codeDirectories'][0]['cdHash']
        bundle = self.base / package.APP_NAME
        contents = bundle / 'Contents'
        (contents / '_CodeSignature').mkdir(parents=True)
        (contents / 'MacOS').mkdir()
        (contents / 'MacOS/Nested').write_bytes(path.read_bytes())
        inventory = [{'path': 'Contents/MacOS/Nested', 'codeDirectories': [{'cdHash': directory_hash}]}]
        seal = {'files2': {'MacOS/Nested': {'cdhash': bytes.fromhex(directory_hash)}}}
        target = contents / '_CodeSignature/CodeResources'
        target.write_bytes(plistlib.dumps(seal))
        package.verify_resource_seal(bundle, inventory)
        seal['files2']['MacOS/Nested']['cdhash'] = bytes(20)
        target.write_bytes(plistlib.dumps(seal))
        with self.assertRaisesRegex(ValueError, 'Nested code signature seal does not match'):
            package.verify_resource_seal(bundle, inventory)

    def test_zip_tgz_preserve_contents_modes_and_detect_changed_payload(self):
        bundle = self.base / package.APP_NAME
        (bundle / 'Contents/MacOS').mkdir(parents=True)
        executable = bundle / 'Contents/MacOS/Run'
        executable.write_bytes(b'payload')
        executable.chmod(0o755)
        output = self.base / 'output'
        output.mkdir()
        expected = package.archive_inventory(bundle)
        records = package.archives(bundle, output, 'fixture')
        self.assertEqual(len(records), 2)
        for record in records:
            package.verify_archive(output / record['file'], expected)
        with zipfile.ZipFile(output / 'tampered.zip', 'w') as archive:
            archive.writestr(package.APP_NAME + '/Contents/MacOS/Run', b'changed')
        with self.assertRaises(ValueError):
            package.verify_archive(output / 'tampered.zip', expected)

    @unittest.skipUnless(sys.platform != 'win32', 'Requires POSIX symlinks')
    def test_archives_preserve_symlinks_and_reject_escaping_bundle_links(self):
        bundle = self.base / package.APP_NAME
        bundle.mkdir()
        (bundle / 'real').write_bytes(b'payload')
        (bundle / 'link').symlink_to('real')
        output = self.base / 'output'
        output.mkdir()
        expected = package.archive_inventory(bundle)
        for record in package.archives(bundle, output, 'symlink'):
            package.verify_archive(output / record['file'], expected)
        (bundle / 'escape').symlink_to('../outside')
        with self.assertRaisesRegex(ValueError, 'symlink escapes'):
            package.native_files(bundle)


if __name__ == '__main__':
    unittest.main()
