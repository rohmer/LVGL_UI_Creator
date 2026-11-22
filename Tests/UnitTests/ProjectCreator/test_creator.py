import importlib.util
import io
import json
import errno
from pathlib import Path
import tempfile
import unittest
import zipfile
import sys
from unittest import mock
ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('creator', ROOT / 'UICreator/ProjectCreator/project_creator.py')
creator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(creator)


def archive(files):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as z:
        for name, text in files.items():
            z.writestr(name, text)
    return data.getvalue()


def manifest():
    return {'name': 'Test board', 'urlToClone': 'https://github.com/lvgl/test-board', 'branches': ['main'],
            'settings': [
                {'type': 'dropdown', 'label': 'Color depth', 'options': [{'name': '16', 'value': '16'}, {'name': '32', 'value': '32', 'default': 'true'}],
                 'actions': [{'filePath': 'lv_conf.h', 'toReplace': r'#define LV_COLOR_DEPTH \d+', 'newContent': '#define LV_COLOR_DEPTH {value}'}]},
                {'type': 'dropdown', 'label': 'Monitor', 'options': [{'name': 'On', 'value': '1', 'default': True}, {'name': 'Off', 'value': '0'}],
                 'actions': [{'filePath': 'sdkconfig.defaults', 'toAppend': 'MONITOR={value}', 'ifValue': '1'}]},
                {'type': 'always', 'actions': [{'filePath': 'setup.txt', 'toAppend': 'configured'}]}]}


def windows_publish_error(code):
    error = PermissionError(errno.EACCES, 'Directory temporarily locked')
    error.winerror = code
    return error


class PublishTests(unittest.TestCase):
    def setUp(self):
        workspace = ROOT / 'build/creator-unit'
        workspace.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=workspace)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stage, self.destination = self.root / 'stage', self.root / 'project'
        self.stage.mkdir()
        (self.stage / 'project.txt').write_text('complete staged project', encoding='utf-8')
        self.helper = creator.raspberry_pi_linux
        self.rename = self.helper.os.rename

    def test_transient_windows_access_sharing_and_lock_errors_retry_only_rename(self):
        for code in (5, 32, 33):
            with self.subTest(winerror=code):
                def rename(source, target):
                    if rename.calls < 2:
                        rename.calls += 1
                        raise windows_publish_error(code)
                    return self.rename(source, target)
                rename.calls = 0
                with mock.patch.object(self.helper, '_WINDOWS_PUBLISH', True), \
                     mock.patch.object(self.helper.os, 'rename', side_effect=rename) as attempts, \
                     mock.patch.object(self.helper.time, 'sleep') as sleep:
                    self.helper.publish_directory(self.stage, self.destination)
                self.assertEqual(attempts.call_count, 3)
                self.assertEqual([call.args[0] for call in sleep.call_args_list], [0.05, 0.1])
                self.assertEqual((self.destination / 'project.txt').read_text(), 'complete staged project')
                self.assertFalse(self.stage.exists())
                self.rename(self.destination, self.stage)

    def test_permanent_windows_error_has_a_bounded_retry_and_preserves_stage(self):
        failure = windows_publish_error(5)
        with mock.patch.object(self.helper, '_WINDOWS_PUBLISH', True), \
             mock.patch.object(self.helper.os, 'rename', side_effect=failure) as attempts, \
             mock.patch.object(self.helper.time, 'sleep') as sleep, self.assertRaises(OSError) as raised:
            self.helper.publish_directory(self.stage, self.destination)
        self.assertIs(raised.exception, failure)
        self.assertEqual(attempts.call_count, 6)
        self.assertEqual(sleep.call_count, 5)
        self.assertAlmostEqual(sum(call.args[0] for call in sleep.call_args_list), 1.55)
        self.assertFalse(self.destination.exists())
        self.assertTrue((self.stage / 'project.txt').is_file())

    def test_non_windows_and_unrelated_windows_failures_do_not_retry(self):
        for windows, code in ((False, 5), (True, 112)):
            with self.subTest(windows=windows, winerror=code), \
                 mock.patch.object(self.helper, '_WINDOWS_PUBLISH', windows), \
                 mock.patch.object(self.helper.os, 'rename', side_effect=windows_publish_error(code)) as attempts, \
                 mock.patch.object(self.helper.time, 'sleep') as sleep, self.assertRaises(OSError):
                self.helper.publish_directory(self.stage, self.destination)
            self.assertEqual(attempts.call_count, 1)
            sleep.assert_not_called()

    def test_existing_destination_even_empty_is_a_collision(self):
        self.destination.mkdir()
        with mock.patch.object(self.helper.os, 'rename') as rename, self.assertRaises(FileExistsError):
            self.helper.publish_directory(self.stage, self.destination)
        rename.assert_not_called()
        self.assertTrue((self.stage / 'project.txt').is_file())
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_destination_appearing_during_retry_is_never_overwritten(self):
        def collide(source, target):
            self.destination.mkdir()
            (self.destination / 'user.txt').write_text('owned by another creator')
            raise windows_publish_error(32)
        with mock.patch.object(self.helper, '_WINDOWS_PUBLISH', True), \
             mock.patch.object(self.helper.os, 'rename', side_effect=collide) as rename, \
             mock.patch.object(self.helper.time, 'sleep') as sleep, self.assertRaises(FileExistsError):
            self.helper.publish_directory(self.stage, self.destination)
        self.assertEqual(rename.call_count, 1)
        sleep.assert_not_called()
        self.assertEqual((self.destination / 'user.txt').read_text(), 'owned by another creator')
        self.assertTrue((self.stage / 'project.txt').is_file())


class CreatorTests(unittest.TestCase):
    def setUp(self):
        workspace = ROOT / "build/creator-unit"
        workspace.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=workspace)
        self.root = Path(self.temp.name)
        self.creator = creator.Creator(self.root / 'cache', lambda u: (_ for _ in ()).throw(RuntimeError('offline')))
        self.source = self.root / 'template.zip'
        self.source.write_bytes(archive({'board-root/lv_conf.h': '#define LV_COLOR_DEPTH 16\n', 'board-root/main.c': 'int main(void){return 0;}\n'}))

    def tearDown(self):
        self.temp.cleanup()

    def request(self, **extra):
        result = {'manifest': manifest(), 'destination': str(self.root / 'output'), 'sourceArchive': str(self.source)}
        result.update(extra)
        return result

    def test_all_published_manifests_validate_offline(self):
        catalog = self.creator.catalog()
        self.assertGreaterEqual(len(catalog), 67)
        for item in catalog:
            with self.subTest(item=item['name']):
                creator.validate_manifest(item)
        self.assertTrue((self.root / 'cache/catalog.json').is_file())

    def test_complete_creation_applies_defaults_and_always_actions(self):
        result = self.creator.create(self.request())
        out = Path(result['destination'])
        self.assertEqual((out / 'lv_conf.h').read_text(), '#define LV_COLOR_DEPTH 32\n')
        self.assertIn('MONITOR=1', (out / 'sdkconfig.defaults').read_text())
        self.assertIn('configured', (out / 'setup.txt').read_text())
        self.assertEqual(json.loads((out / 'ui/project.json').read_text())['obj'], [])
        self.assertTrue((out / 'UICREATOR_GETTING_STARTED.md').is_file())
        self.assertTrue((out / 'uicreator-project.json').is_file())

    def test_selected_values_and_conditions(self):
        self.creator.create(self.request(settings={'Color depth': '16', '1': '0'}))
        out = self.root / 'output'
        self.assertEqual((out / 'lv_conf.h').read_text(), '#define LV_COLOR_DEPTH 16\n')
        self.assertFalse((out / 'sdkconfig.defaults').exists())

    def test_bad_setting_options_leave_destination_untouched(self):
        for settings in ({'0': '123'}, {'unknown': '32'}):
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                self.creator.create(self.request(settings=settings))
            self.assertFalse((self.root / 'output').exists())
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_missing_match_and_invalid_regex_fail_transactionally(self):
        for pattern in ('not found', '['):
            m = manifest()
            m['settings'][0]['actions'][0]['toReplace'] = pattern
            with self.subTest(pattern=pattern), self.assertRaises((ValueError, creator.re.error)):
                self.creator.create(self.request(manifest=m))
            self.assertFalse((self.root / 'output').exists())

    def test_existing_files_are_never_overwritten(self):
        out = self.root / 'output'
        out.mkdir()
        (out / 'keep.txt').write_text('keep')
        with self.assertRaises(ValueError):
            self.creator.create(self.request())
        self.assertEqual((out / 'keep.txt').read_text(), 'keep')

    def test_empty_destination_is_supported(self):
        (self.root / 'output').mkdir()
        self.creator.create(self.request())
        self.assertTrue((self.root / 'output/main.c').exists())

    def test_publish_retry_does_not_restart_generation(self):
        helper = creator.raspberry_pi_linux
        rename = helper.os.rename
        attempts = 0
        def publish(source, destination):
            nonlocal attempts
            attempts += 1
            if attempts == 1:
                raise windows_publish_error(5)
            return rename(source, destination)
        with mock.patch.object(helper, '_WINDOWS_PUBLISH', True), \
             mock.patch.object(helper.os, 'rename', side_effect=publish), \
             mock.patch.object(helper.time, 'sleep'), \
             mock.patch.object(creator, 'extract_archive', wraps=creator.extract_archive) as extraction:
            self.creator.create(self.request())
        self.assertEqual(extraction.call_count, 1)
        self.assertEqual(attempts, 2)
        self.assertTrue((self.root / 'output/ui/project.json').is_file())
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_permanent_publish_failure_restores_empty_destination_and_cleans_stage(self):
        output = self.root / 'output'
        output.mkdir()
        helper = creator.raspberry_pi_linux
        with mock.patch.object(helper, '_WINDOWS_PUBLISH', True), \
             mock.patch.object(helper.os, 'rename', side_effect=windows_publish_error(5)), \
             mock.patch.object(helper.time, 'sleep'), self.assertRaises(OSError):
            self.creator.create(self.request())
        self.assertTrue(output.is_dir())
        self.assertEqual(list(output.iterdir()), [])
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_publish_collision_keeps_new_destination_and_cleans_private_stage(self):
        output = self.root / 'output'
        helper = creator.raspberry_pi_linux
        def collide(source, destination):
            output.mkdir()
            (output / 'user.txt').write_text('do not replace')
            raise windows_publish_error(5)
        with mock.patch.object(helper, '_WINDOWS_PUBLISH', True), \
             mock.patch.object(helper.os, 'rename', side_effect=collide), \
             mock.patch.object(helper.time, 'sleep'), self.assertRaises(FileExistsError):
            self.creator.create(self.request())
        self.assertEqual((output / 'user.txt').read_text(), 'do not replace')
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_archive_path_and_symlink_validation(self):
        for name in ('../outside', '/absolute', 'C:/outside', 'root/../../escape', 'root/CON.txt', 'root/path./file'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                creator.extract_archive(archive({name: 'bad'}), self.root / 'unpack')
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as z:
            info = zipfile.ZipInfo('root/link')
            info.external_attr = (0o120777 << 16)
            z.writestr(info, '/outside')
        with self.assertRaises(ValueError):
            creator.extract_archive(data.getvalue(), self.root / 'unpack')

    def test_paths_cannot_escape_through_settings_or_ui(self):
        for path in ('../x', '/x', 'C:/x', 'a/../x', '~home/x'):
            for field in ('setting', 'ui'):
                m = manifest()
                if field == 'setting':
                    m['settings'][0]['actions'][0]['filePath'] = path
                else:
                    m['uiFolderPath'] = path
                with self.subTest(path=path, field=field), self.assertRaises(ValueError):
                    self.creator.create(self.request(manifest=m))

    def test_ui_payload_and_custom_ui_folder(self):
        m = manifest()
        m['uiFolderPath'] = 'main/design'
        source = self.root / 'generated'
        source.mkdir()
        (source / 'ui.c').write_text('/* generated */')
        project = {'obj': [{'base': {}, 'text': 'hello'}]}
        self.creator.create(self.request(manifest=m, project=project, uiSourceFolder=str(source)))
        out = self.root / 'output/main/design'
        self.assertEqual((out / 'ui.c').read_text(), '/* generated */')
        self.assertEqual(json.loads((out / 'project.json').read_text()), project)

    def test_nonempty_template_ui_is_preserved(self):
        self.source.write_bytes(archive({'root/lv_conf.h': '#define LV_COLOR_DEPTH 16\n', 'root/ui/main.c': 'important'}))
        with self.assertRaises(ValueError):
            self.creator.create(self.request())
        self.assertFalse((self.root / 'output').exists())

    def test_cache_is_reusable_without_network(self):
        calls = []
        first = creator.Creator(self.root / 'network-cache', lambda u: calls.append(u) or b'cached')
        self.assertEqual(first.cached('https://example.com/file'), b'cached')
        self.assertEqual(first.cached('https://example.com/file'), b'cached')
        self.assertEqual(len(calls), 1)
        offline = creator.Creator(self.root / 'network-cache', lambda u: (_ for _ in ()).throw(RuntimeError('offline')))
        self.assertEqual(offline.cached('https://example.com/file', True), b'cached')

    def test_submodules_use_pinned_commits_and_archive_cache(self):
        sha = 'a' * 40
        child_sha = 'b' * 40
        replies = {
            'https://api.github.com/repos/lvgl/test-board/commits/main': json.dumps({'sha': sha}).encode(),
            f'https://codeload.github.com/lvgl/test-board/zip/{sha}': archive({'r/.gitmodules': '[submodule "driver"]\npath = deps/driver\nurl = https://github.com/vendor/driver.git\n', 'r/lv_conf.h': '#define LV_COLOR_DEPTH 16\n'}),
            f'https://api.github.com/repos/lvgl/test-board/contents/deps/driver?ref={sha}': json.dumps({'sha': child_sha}).encode(),
            f'https://codeload.github.com/vendor/driver/zip/{child_sha}': archive({'d/driver.c': 'driver'}),
        }
        online = creator.Creator(self.root / 'repos', lambda u: replies[u])
        request = self.request(branch='main')
        request.pop('sourceArchive')
        result = online.create(request)
        self.assertEqual((self.root / 'output/deps/driver/driver.c').read_text(), 'driver')
        self.assertEqual([r['commit'] for r in result['repositories']], [sha, child_sha])
        offline = creator.Creator(self.root / 'repos', lambda u: (_ for _ in ()).throw(RuntimeError('offline')))
        request['destination'] = str(self.root / 'offline')
        offline.create(request)
        self.assertEqual((self.root / 'offline/deps/driver/driver.c').read_text(), 'driver')

    def test_external_managed_lvgl_needs_explicit_path_for_override(self):
        with self.assertRaises(ValueError):
            self.creator.create(self.request(lvglVersion='9.6.0'))
        self.assertFalse((self.root / 'output').exists())

    def test_catalog_refresh_rejects_invalid_manifests(self):
        online = creator.Creator(self.root / 'refresh', lambda u: b'[{"name":"bad"}]')
        with self.assertRaises(ValueError):
            online.catalog(True)
        self.assertFalse((self.root / 'refresh/catalog.json').exists())

    def test_raspberry_pi_linux_entry_matches_bundled_catalog_and_filters(self):
        canonical = creator.raspberry_pi_linux.manifest()
        catalog = json.loads((ROOT / 'UICreator/ProjectCreator/catalog.json').read_text(encoding='utf-8'))
        builtins = [item for item in catalog if item.get('uicreatorTemplate') == creator.raspberry_pi_linux.TEMPLATE_ID]
        self.assertEqual(builtins, [canonical])
        self.assertEqual(canonical['hardware']['chipVendor'], 'Raspberry Pi')
        self.assertTrue({'Windows', 'Linux', 'MacOS'} <= set(canonical['hostOperatingsystem']))
        self.assertTrue({'CMake', 'Linux', 'SDL3'} <= set(canonical['environment']))
        self.assertEqual(canonical['lvglVersion'], '9.6.0')
        creator.validate_manifest(canonical)

    def test_raspberry_pi_linux_survives_cached_and_refreshed_catalogs(self):
        official = manifest()
        stale = creator.raspberry_pi_linux.manifest()
        stale['name'] = 'Stale cached local template'
        (self.root / 'cache/catalog.json').write_text(json.dumps([official, stale]), encoding='utf-8')
        cached = self.creator.catalog()
        self.assertEqual(cached, [official, creator.raspberry_pi_linux.manifest(), *creator.raspberry_pi_pico.manifests()])
        calls = []
        online = creator.Creator(self.root / 'cache', lambda url: calls.append(url) or json.dumps([official]).encode())
        refreshed = online.catalog(True)
        self.assertEqual(refreshed, cached)
        self.assertEqual(calls, [creator.CATALOG_URL])
        saved = json.loads((self.root / 'cache/catalog.json').read_text(encoding='utf-8'))
        self.assertEqual(saved, cached)

    def linux_request(self, **extra):
        source = self.root / 'current-ui'
        source.mkdir(exist_ok=True)
        (source / 'project.pb').write_bytes(b'current-screen')
        toolkit = self.root / 'linux-toolkit.zip'
        toolkit.write_bytes(archive({'toolkit/placeholder': 'not extracted until validation'}))
        request = {'manifest': creator.raspberry_pi_linux.manifest(),
                   'destination': str(self.root / 'linux-output'),
                   'uiSourceFolder': str(source), 'linuxToolkitArchive': str(toolkit),
                   'format': 'json', 'project': {'obj': []}, 'lvglVersion': 'template'}
        request.update(extra)
        return request

    def test_linux_rejects_unsupported_overrides_before_writing_project(self):
        request = self.linux_request()
        for extra in ({'lvglVersion': '9.5.0'}, {'lvglVersion': 'v9.6.0'},
                      {'lvglPath': 'lvgl'}, {'sourceArchive': str(self.source)},
                      {'branch': 'main'}, {'settings': {'arbitrary': 'value'}}):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.creator.create({**request, **extra})
            self.assertFalse((self.root / 'linux-output').exists())
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_linux_requires_current_screen_and_bundled_toolkit(self):
        request = self.linux_request()
        for extra, message in (({'uiSourceFolder': ''}, 'current screen'),
                               ({'linuxToolkitArchive': ''}, 'toolkit')):
            with self.subTest(extra=extra), self.assertRaisesRegex(ValueError, message):
                self.creator.create({**request, **extra})
            self.assertFalse((self.root / 'linux-output').exists())
        (self.root / 'current-ui/project.pb').write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'between 1 byte'):
            self.creator.create(request)

    def test_linux_unknown_template_and_custom_configuration_are_rejected(self):
        for extra in ({'uicreatorTemplate': 'arbitrary-script'}, {'uiFolderPath': 'elsewhere'},
                      {'settings': [{'type': 'always', 'actions': []}]}, {'lvglVersion': '10.0.0'}):
            value = creator.raspberry_pi_linux.manifest()
            value.update(extra)
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                creator.validate_manifest(value)

    def test_linux_checks_toolkit_integrity_before_importing_any_generator(self):
        request = self.linux_request()
        payload = 'raise RuntimeError("archive generator executed")'
        marker = {'formatVersion': 1, 'lvglVersion': '9.6.0',
                  'files': {'Runtime/linux/make_sdk.py': '0' * 64}}
        Path(request['linuxToolkitArchive']).write_bytes(archive({
            'toolkit/lvgl-pb-linux-toolkit.json': json.dumps(marker),
            'toolkit/Runtime/linux/make_sdk.py': payload}))
        with self.assertRaisesRegex(ValueError, 'integrity check'):
            self.creator.create(request)
        self.assertFalse((self.root / 'linux-output').exists())
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))

    def test_linux_minimum_project_generation_is_offline_and_keeps_authoring_ui(self):
        candidates = (ROOT / 'build/professional/bin/project-creator/linux-toolkit.zip',
                      ROOT / 'build/professional/packages/LVGLProtobuf-Linux-Toolkit.zip')
        toolkit = next((path for path in candidates if path.is_file()), None)
        if toolkit is None:
            self.skipTest('Build the bundled project-creator Linux toolkit for the real generation check')
        original_path = list(sys.path)
        try:
            sys.path.insert(0, str(ROOT / 'Runtime/esp32'))
            import feature_plan
        finally:
            sys.path[:] = original_path
        project = {'formatVersion': 2, 'lvglVersion': '9.6.0', 'canvas': {'width': 320, 'height': 240},
                   'obj': [{'oid': 0, 'par': -1, 'base': {'coords': {'x': 8, 'y': 8, 'width': 200, 'height': 40}},
                            'text': 'Raspberry Pi Linux'}], 'variables': []}
        request = self.linux_request(linuxToolkitArchive=str(toolkit), project=project, lvglVersion='9.6.0')
        source = Path(request['uiSourceFolder'])
        (source / 'project.pb').write_bytes(feature_plan.encode_project(project))
        (source / 'ui.c').write_text('/* optional C export is retained */', encoding='utf-8')
        previous_plan = sys.modules['feature_plan']
        with mock.patch.object(self.creator, 'repository', side_effect=AssertionError('Linux creation used the network')):
            result = self.creator.create(request)
        self.assertIs(sys.modules['feature_plan'], previous_plan)
        out = Path(result['destination'])
        self.assertTrue((out / 'CMakeLists.txt').is_file())
        self.assertTrue((out / 'platform/Runtime/linux/Viewer.cpp').is_file())
        self.assertTrue((out / 'platform/3rdParty/SDL3/CMakeLists.txt').is_file())
        self.assertEqual(json.loads((out / 'ui/project.json').read_text(encoding='utf-8')), project)
        self.assertEqual((out / 'ui/ui.c').read_text(), '/* optional C export is retained */')
        embedded = out / result['linuxBuild']['optimizedProject']
        self.assertEqual((out / 'ui/project.pb').read_bytes(), embedded.read_bytes())
        plan = json.loads((out / 'embedded/features.json').read_text(encoding='utf-8'))
        self.assertLess(plan['lvglSourceFileCount'], plan['fullLvglSourceFileCount'])
        self.assertEqual(plan['widgetSections'], ['text'])
        self.assertFalse(any('/widgets/chart/' in path for path in plan['sourceFiles']))
        self.assertIn('build/platform/Runtime/linux/lvgl-pb-viewer ui/project.pb',
                      (out / 'UICREATOR_GETTING_STARTED.md').read_text(encoding='utf-8'))
        data = json.loads((out / 'lvgl-pb-linux-sdk.json').read_text(encoding='utf-8'))
        import hashlib
        for name, expected in data['files'].items():
            self.assertEqual(hashlib.sha256((out / name).read_bytes()).hexdigest(), expected, name)
        self.assertIn('ui/project.pb', data['files'])
        self.assertIn('uicreator-project.json', data['files'])


class PicoCreatorTests(unittest.TestCase):
    def setUp(self):
        workspace = ROOT / 'build/creator-unit'
        workspace.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=workspace)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.creator = creator.Creator(self.root / 'cache', lambda url: self.fail('Pico creation attempted a network request'))
        self.source = self.root / 'current-ui'
        self.source.mkdir()
        (self.source / 'project.pb').write_bytes(b'current selected design')
        for name in ('ui.c', 'ui.h', 'ui_assets.c', 'ui_assets.h', 'CMakeLists.txt'):
            (self.source / name).write_text('/* generated selected design */', encoding='utf-8')
        self.toolkit = self.root / 'pico-toolkit.zip'
        generator = '''import json
from pathlib import Path
def generate(inputs, output, *, board, generated_c, archive, copy_ui):
    assert archive is False and copy_ui is False
    output.mkdir()
    mode = "c" if generated_c is not None else "protobuf"
    (output / "CMakeLists.txt").write_text("PICO_BOARD=" + board + "\\nmode=" + mode)
    (output / "lvgl-pb-pico-sdk.json").write_text(json.dumps({"formatVersion": 1, "board": board, "mode": mode, "files": {}}))
'''
        files = {'Runtime/pico/make_sdk.py': generator,
                 'Runtime/esp32/feature_plan.py': '# bundled profile analyzer',
                 'Runtime/esp32/make_component.py': '# bundled source closure',
                 'Runtime/esp32/trie_assets.py': '# bundled model validator'}
        import hashlib
        marker = {'formatVersion': 1, 'lvglVersion': '9.6.0',
                  'files': {name: hashlib.sha256(text.encode()).hexdigest() for name, text in files.items()}}
        self.toolkit.write_bytes(archive({'toolkit/' + name: text for name, text in
                                        {**files, 'lvgl-pb-pico-toolkit.json': json.dumps(marker)}.items()}))

    def request(self, board=0, output_format='protobuf'):
        return {'manifest': creator.raspberry_pi_pico.manifests()[board],
                'destination': str(self.root / 'output'), 'format': output_format,
                'lvglVersion': 'template', 'project': {'obj': [], 'name': 'Current design'},
                'uiSourceFolder': str(self.source), 'picoToolkitArchive': str(self.toolkit)}

    def test_both_pico_templates_survive_old_cache_and_catalog_refresh(self):
        official = manifest()
        for refresh in (False, True):
            if refresh:
                self.creator.fetch = lambda url: json.dumps([official]).encode()
            else:
                (self.creator.cache / 'catalog.json').write_text(json.dumps([official]))
            catalog = self.creator.catalog(refresh)
            for identity in creator.raspberry_pi_pico.TEMPLATE_BOARDS:
                self.assertEqual(sum(item.get('uicreatorTemplate') == identity for item in catalog), 1)
            self.assertTrue(any(item['name'] == official['name'] for item in catalog))
            self.assertTrue(any(item.get('uicreatorTemplate') == 'raspberry-pi-linux' for item in catalog))

    def test_pico_formats_select_the_board_and_link_mode_offline(self):
        for board, board_name in enumerate(('pico', 'pico2')):
            for output_format in ('json', 'protobuf', 'c'):
                with self.subTest(board=board_name, format=output_format):
                    request = self.request(board, output_format)
                    request['destination'] = str(self.root / (board_name + '-' + output_format))
                    with mock.patch.object(self.creator, 'repository', side_effect=AssertionError('Pico creation used a remote template')):
                        result = self.creator.create(request)
                    output = Path(result['destination'])
                    data = json.loads((output / 'lvgl-pb-pico-sdk.json').read_text())
                    self.assertEqual(data['board'], board_name)
                    self.assertEqual(data['mode'], 'c' if output_format == 'c' else 'protobuf')
                    self.assertEqual((output / 'ui/project.pb').read_bytes(), b'current selected design')
                    self.assertEqual(json.loads((output / 'ui/project.json').read_text()), request['project'])
                    self.assertIn('uicreator-project.json', data['files'])
                    self.assertIn('ui/ui.c', data['files'])
                    self.assertEqual(result['template']['uicreatorTemplate'], request['manifest']['uicreatorTemplate'])
                    self.assertEqual(result['picoBuild']['board'], board_name)
                    with self.assertRaises(ValueError):
                        self.creator.create(request)

    def test_pico_rejects_unsupported_overrides_and_incomplete_current_design(self):
        for extra in ({'lvglVersion': '8.3.11'}, {'sourceArchive': str(self.toolkit)},
                      {'branch': 'main'}, {'lvglPath': 'vendor/lvgl'}, {'settings': {'board': 'pico_w'}},
                      {'uiSourceFolder': ''}, {'picoToolkitArchive': ''}):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.creator.create({**self.request(), **extra})
            self.assertFalse((self.root / 'output').exists())
        (self.source / 'ui.c').unlink()
        with self.assertRaisesRegex(ValueError, 'complete generated C'):
            self.creator.create(self.request(output_format='c'))

    def test_pico_toolkit_integrity_is_checked_before_import(self):
        marker = {'formatVersion': 1, 'lvglVersion': '9.6.0',
                  'files': {'Runtime/pico/make_sdk.py': '0' * 64}}
        self.toolkit.write_bytes(archive({'toolkit/lvgl-pb-pico-toolkit.json': json.dumps(marker),
                                        'toolkit/Runtime/pico/make_sdk.py': 'raise AssertionError("executed before verification")'}))
        with self.assertRaisesRegex(ValueError, 'integrity check'):
            self.creator.create(self.request())
        self.assertFalse((self.root / 'output').exists())
        self.assertFalse(list(self.root.glob('.uicreator-project-*')))


if __name__ == '__main__':
    unittest.main()
