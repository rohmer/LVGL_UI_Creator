from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
HELPER = ROOT / 'Integrations/ESPOS/templates/tools/advanced_keyboard_dependency.cmake'
CMAKE = shutil.which('cmake')
REQUIRED = ('CMakeLists.txt', 'cmake/AdvancedKeyboardSources.cmake', 'src/PredictionModel.cpp', 'src/PredictionPrefix.cpp', 'src/Utf8Fold.cpp',
            'src/PredictionStorage.cpp', 'src/Prediction.cpp', 'src/AdvancedKeyboard.cpp', 'src/BundledModels.cpp',
            'include/lv_advanced_keyboard.h', 'include/espos_keyboard/BundledModels.h',
            'integrations/espos/TypingView.cpp', 'integrations/espos/esp_runtime.cpp',
            'integrations/espos/include/espos_keyboard/LegacyTypingView.h', 'models/bundled/catalog.json',
            'tools/models/generate_bundled_models.py', 'tools/layout/build_ngtr_index.py')


@unittest.skipUnless(CMAKE, 'CMake is required for dependency helper tests')
class DependencyHelperTests(unittest.TestCase):
    def setUp(self):
        folder = ROOT / 'build/espos-dependency-helper-tests'
        folder.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix='case-', dir=folder)
        self.project = Path(self.temporary.name)
        (self.project / 'tools').mkdir()
        shutil.copy2(HELPER, self.project / 'tools/advanced_keyboard_dependency.cmake')
        self.dependency = self.project / 'third_party/AdvancedKeyboard'
        for name in REQUIRED:
            file = self.dependency / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('fixture')
        self.lock = {'version': 1, 'repository': 'https://github.com/rohmer/AdvancedKeyboard.git',
                     'revision': '6df59fab1cac13f2a7bc43e1b02b9e97cae65447',
                     'paths': {'creator': '3rdParty/AdvancedKeyboard', 'espos': 'third_party/AdvancedKeyboard'}}
        self.write_lock()

    def tearDown(self):
        self.temporary.cleanup()

    def write_lock(self):
        (self.project / 'advanced-keyboard.lock.json').write_text(json.dumps(self.lock))

    def run_helper(self, override=None):
        script = self.project / 'test.cmake'
        lines = ['include("${CMAKE_CURRENT_LIST_DIR}/tools/advanced_keyboard_dependency.cmake")']
        if override is not None:
            lines.append('set(ADVANCED_KEYBOARD_ROOT "' + Path(override).as_posix() + '")')
        lines += ['espos_require_advanced_keyboard()',
                  'file(WRITE "${CMAKE_CURRENT_LIST_DIR}/resolved.txt" "${ADVANCED_KEYBOARD_ROOT}|${ADVKBD_SOURCE_ROOT}")']
        script.write_text('\n'.join(lines))
        return subprocess.run([CMAKE, '-P', str(script)], capture_output=True, text=True, timeout=30)

    def test_source_archive_does_not_resolve_an_ancestor_git_repository(self):
        # The fixture lives under the Creator checkout but owns no .git.
        # Resolving an ancestor HEAD would incorrectly reject this source archive.
        result = self.run_helper()
        self.assertEqual(result.returncode, 0, result.stderr)
        values = (self.project / 'resolved.txt').read_text().split('|')
        self.assertEqual([Path(value).resolve() for value in values], [self.dependency.resolve()] * 2)

    def test_missing_source_points_to_explicit_setup_without_fetching(self):
        (self.dependency / 'src/PredictionModel.cpp').unlink()
        result = self.run_helper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('setup_advanced_keyboard.py', result.stderr)
        self.assertIn('--kind espos --init', result.stderr)
        self.assertFalse((self.dependency / '.git').exists())

    def test_missing_lock_and_path_escape_are_rejected(self):
        (self.project / 'advanced-keyboard.lock.json').unlink()
        self.assertNotEqual(self.run_helper().returncode, 0)
        self.lock['paths']['espos'] = '../outside'
        self.write_lock()
        self.assertNotEqual(self.run_helper().returncode, 0)

    def test_full_revision_is_required(self):
        self.lock['revision'] = '6df59fab'
        self.write_lock()
        self.assertNotEqual(self.run_helper().returncode, 0)

    def test_unsupported_lock_and_repository_metadata_path_are_rejected(self):
        self.lock['version'] = 2
        self.write_lock()
        self.assertNotEqual(self.run_helper().returncode, 0)
        self.lock['version'] = 1
        for relative in ('.', './', '.git', 'third_party/.git/source'):
            self.lock['paths']['espos'] = relative
            self.write_lock()
            self.assertNotEqual(self.run_helper().returncode, 0, relative)

    def test_explicit_source_override(self):
        override = self.project / 'explicit-source'
        shutil.copytree(self.dependency, override)
        result = self.run_helper(override)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(override.as_posix(), (self.project / 'resolved.txt').read_text())


if __name__ == '__main__':
    unittest.main()
