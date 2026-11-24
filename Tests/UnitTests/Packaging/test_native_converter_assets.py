"""Incremental packaging drops obsolete converter runtimes without touching user data."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location('native_converter_linux_tools', ROOT / 'Packaging/BuildLinuxTools.py')
tools = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tools)
STAGE_SPEC = importlib.util.spec_from_file_location('native_project_runtime_stager', ROOT / 'Packaging/StageProjectRuntime.py')
stager = importlib.util.module_from_spec(STAGE_SPEC)
STAGE_SPEC.loader.exec_module(stager)


class NativeConverterAssetTests(unittest.TestCase):
    def test_runtime_output_drops_removed_modules_and_keeps_sibling_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / 'pristine', root / 'bin'
            (source / 'bin').mkdir(parents=True)
            (source / 'bin/python3').write_bytes(b'native target interpreter')
            (output / 'python-runtime/lib/PIL').mkdir(parents=True)
            (output / 'python-runtime/lib/PIL/legacy.py').write_bytes(b'old converter')
            keep = output / 'user-project.json'; keep.write_bytes(b'project')
            stager.stage(source, output)
            self.assertFalse((output / 'python-runtime/lib/PIL').exists())
            self.assertEqual((output / 'python-runtime/bin/python3').read_bytes(), b'native target interpreter')
            self.assertEqual(keep.read_bytes(), b'project')
            self.assertEqual(list(output.glob('.project-runtime-stage-*')), [])

    def test_project_runtime_replaces_legacy_image_modules_and_preserves_other_caches(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, source = root / 'cache', root / 'source'
            (base / 'tools').mkdir(parents=True)
            python = {}
            preserved = []
            for arch in ('x86_64', 'aarch64'):
                pristine = root / ('pristine-' + arch)
                (pristine / 'bin').mkdir(parents=True)
                (pristine / 'bin/python3').write_bytes(b'native target Python')
                (pristine / 'lib/python3.11/site-packages').mkdir(parents=True)
                (pristine / 'lib/python3.11/zipfile.py').write_text('# stdlib')
                python[arch] = str(pristine)
                output = base / 'assets' / arch
                legacy = output / 'python-runtime/lib/python3.11/site-packages/PIL'
                legacy.mkdir(parents=True)
                (legacy / 'Image.py').write_text('# obsolete image converter')
                (output / 'python-dependencies.json').write_text(json.dumps({'requirements': ['pillow==12.1.1']}))
                unrelated = output / 'saved-projects/do-not-change.json'
                unrelated.parent.mkdir(); unrelated.write_bytes(b'user project')
                preserved.append(unrelated)
            (base / 'tools/tools.json').write_text(json.dumps({'python': python}))
            project = source / 'UICreator/ProjectCreator'; project.mkdir(parents=True)
            for name in ('project_creator.py', 'raspberry_pi_linux.py', 'raspberry_pi_pico.py'):
                (project / name).write_text('# current source')
            tools.assets(base, source)
            for arch in python:
                output = base / 'assets' / arch
                self.assertFalse((output / 'python-runtime/lib/python3.11/site-packages/PIL').exists())
                self.assertTrue((output / 'python-runtime/bin/python3').is_file())
                self.assertTrue((output / 'project-creator/UICreatorProjectTool').is_file())
                self.assertFalse((output / 'font-converter').exists())
                self.assertFalse((output / 'image-converter').exists())
                self.assertEqual(json.loads((output / 'python-dependencies.json').read_text())['requirements'], [])
            for path in preserved:
                self.assertEqual(path.read_bytes(), b'user project')


if __name__ == '__main__':
    unittest.main()
