"""Independent target-only ELF dependency fixtures; never inspect host ldconfig."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('linux_package_under_test',ROOT/'Packaging/Linux/linux_package.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)


@unittest.skipUnless(os.name=='posix','ELF closure fixtures require POSIX tools')
class ClosureTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='lvgl-closure-qa-')
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.payload=self.root/'payload';self.payload.mkdir()
        self.sysroot=self.root/'sysroot';self.sysroot.mkdir()
        self.target=self.sysroot/'usr/lib/x86_64-linux-gnu';self.target.mkdir(parents=True)
        self.host=self.root/'host';self.host.mkdir()
        self.log=self.root/'readelf.log'
        self.readelf=self.root/'readelf'
        self.readelf.write_text('''#!/usr/bin/python3
import json,os,sys
from pathlib import Path
if os.environ.get('LC_ALL')!='C':raise SystemExit('readelf output locale was not forced to C')
with open(os.environ['CLOSURE_QA_LOG'],'a') as log:log.write(sys.argv[1]+'\\n')
data=json.loads(Path(sys.argv[2]+'.meta').read_text())
if sys.argv[1]=='--dynamic':
 for name in data.get('needed',[]):print(' 0x1 (NEEDED) Shared library: ['+name+']')
 for key in ('RPATH','RUNPATH'):
  if key.lower() in data:print(' 0x2 ('+key+') Library path: ['+data[key.lower()]+']')
else:
 print("Version needs section '.gnu.version_r':")
 print(' GLIBC_'+data.get('glibc','2.35'))
''')
        self.readelf.chmod(0o755)
        self.environment=mock.patch.dict(os.environ,{'CLOSURE_QA_LOG':str(self.log),'LC_ALL':'not-C'})
        self.environment.start();self.addCleanup(self.environment.stop)

    def elf(self,path,needed=(),machine=62,glibc='2.35',**metadata):
        path.parent.mkdir(parents=True,exist_ok=True)
        header=bytearray(20);header[:4]=b'\x7fELF';header[4]=2;header[5]=1;struct.pack_into('<H',header,18,machine)
        path.write_bytes(header)
        path.with_name(path.name+'.meta').write_text(json.dumps({'needed':list(needed),'glibc':glibc,**metadata}))
        return path

    def resolve(self,paths=None,**options):
        return package.closure(paths or [self.payload], 'x86_64',str(self.readelf),sysroot=self.sysroot,**options)

    def test_target_sysroot_wins_over_foreign_absolute_rpath_and_locale_is_stable(self):
        self.elf(self.payload/'app',['libqa.so'],rpath=str(self.host))
        self.elf(self.host/'libqa.so',glibc='2.43')
        target=self.elf(self.target/'libqa.so')
        result=self.resolve(library_directories=[self.target])
        self.assertEqual(result['libraries'],[{'source':str(target),'name':'libqa.so'}])
        self.assertEqual(result['abi']['glibc'],'2.35')
        self.assertIn('--dynamic',self.log.read_text())
        self.assertIn('--version-info',self.log.read_text())

    def test_absent_target_never_falls_back_to_same_architecture_host_rpath(self):
        self.elf(self.payload/'app',['libqa.so'],runpath=str(self.host))
        self.elf(self.host/'libqa.so')
        with self.assertRaisesRegex(ValueError,'Cannot resolve'):
            self.resolve(library_directories=[self.target])

    def test_requester_origin_rpath_and_runpath_take_precedence_and_walk_transitively(self):
        for kind in ('rpath','runpath'):
            with self.subTest(kind=kind):
                self.elf(self.payload/'app',['libfirst.so'],**{kind:'$ORIGIN/private'})
                first=self.elf(self.payload/'private/libfirst.so',['libsecond.so'])
                second=self.elf(self.payload/'private/libsecond.so')
                self.elf(self.target/'libfirst.so',glibc='2.43')
                result=self.resolve(paths=[self.payload/'app'],library_directories=[self.target])
                self.assertEqual({record['source'] for record in result['libraries']},{str(first),str(second)})
                self.assertEqual(result['abi']['glibc'],'2.35')

    def test_both_origin_spellings_in_needed_are_contained_in_payload(self):
        self.elf(self.payload/'bin/app',['$ORIGIN/../lib/libone.so','${ORIGIN}/../lib/libtwo.so'])
        first=self.elf(self.payload/'lib/libone.so');second=self.elf(self.payload/'lib/libtwo.so')
        result=self.resolve()
        self.assertEqual({record['source'] for record in result['libraries']},{str(first),str(second)})

    def test_origin_escape_rejected_even_when_external_directory_is_declared(self):
        self.elf(self.payload/'app',['$ORIGIN/../host/libqa.so'])
        self.elf(self.host/'libqa.so')
        with self.assertRaisesRegex(ValueError,'escaping'):
            self.resolve(library_directories=[self.host])

    def test_absolute_needed_and_foreign_architecture_are_rejected(self):
        self.elf(self.payload/'app',[str(self.host/'libqa.so')]);self.elf(self.host/'libqa.so')
        with self.assertRaisesRegex(ValueError,'Nonrelocatable'):
            self.resolve(library_directories=[self.host])
        self.elf(self.payload/'app',['libqa.so']);self.elf(self.target/'libqa.so',machine=183)
        with self.assertRaisesRegex(ValueError,'Cannot resolve'):
            self.resolve(library_directories=[self.target])

    def test_absolute_file_symlink_is_rebased_into_target_sysroot(self):
        self.elf(self.payload/'app',['libqa.so'])
        real=self.elf(self.target/'libqa-real.so')
        (self.target/'libqa.so').symlink_to('/usr/lib/x86_64-linux-gnu/libqa-real.so')
        result=self.resolve(library_directories=[self.target])
        self.assertEqual(result['libraries'],[{'source':str(real),'name':'libqa.so'}])

    def test_absolute_directory_symlink_never_authorizes_or_selects_host_library(self):
        self.elf(self.payload/'app',['libqa.so'])
        self.elf(self.host/'libqa.so',glibc='2.43')
        # A sysroot's absolute symlink names target paths. Use a private fake
        # host path instead of writing to the real host /usr/lib for this case.
        virtual=self.sysroot/str(self.host).lstrip('/')
        real=self.elf(virtual/'libqa.so')
        alias=self.sysroot/'absolute-library-directory';alias.symlink_to(self.host,target_is_directory=True)
        result=self.resolve(library_directories=[alias])
        self.assertEqual(result['libraries'],[{'source':str(real),'name':'libqa.so'}])
        self.assertEqual(result['abi']['glibc'],'2.35')


if __name__=='__main__':unittest.main()
