"""Source mirroring must preserve platform code without copying MSBuild outputs."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('linux_build_under_test',ROOT/'Packaging/BuildLinux.py')
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)


class MirrorTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory(prefix='lvgl-linux-mirror-')
        self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.source=self.root/'repository';self.base=self.root/'workspace'
        self.source.mkdir();self.base.mkdir()

    def write(self,name,text='source'):
        path=self.source/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)

    def test_only_configurations_with_msbuild_tracking_are_excluded(self):
        self.write('UICreator/Debug/UICreator.tlog/CL.command.1.tlog')
        self.write('UICreator/Debug/UI.obj')
        self.write('UICreator/Debug/generated/generated.cpp')
        self.write('LVSerial/x64/Release/LVSerial.vcxproj.FileListAbsolute.txt')
        self.write('LVSerial/x64/Release/Runtime.obj')
        self.write('3rdParty/DirectFB/Win32/direct.cpp')
        self.write('UICreator/Release/actual-source.cpp')
        self.write('3rdParty/lv_font_conv/lib/freetype/build/CMakeLists.txt')
        destination=build.mirror(self.source,self.base)
        self.assertFalse((destination/'UICreator/Debug/generated/generated.cpp').exists())
        self.assertFalse((destination/'LVSerial/x64/Release/Runtime.obj').exists())
        for name in ('3rdParty/DirectFB/Win32/direct.cpp','UICreator/Release/actual-source.cpp',
                     '3rdParty/lv_font_conv/lib/freetype/build/CMakeLists.txt'):
            self.assertTrue((destination/name).is_file(),name)
        inventory=json.loads((self.base/'source-files.json').read_text())
        self.assertEqual(len(inventory),3)

    def test_previously_copied_outputs_are_removed_on_next_sync(self):
        self.write('UICreator/Debug/UI.obj')
        destination=build.mirror(self.source,self.base)
        self.assertTrue((destination/'UICreator/Debug/UI.obj').exists())
        self.write('UICreator/Debug/UICreator.tlog/CL.command.1.tlog')
        build.mirror(self.source,self.base)
        self.assertFalse((destination/'UICreator/Debug/UI.obj').exists())
        self.assertEqual(json.loads((self.base/'source-files.json').read_text()),[])


if __name__=='__main__':
    unittest.main()
