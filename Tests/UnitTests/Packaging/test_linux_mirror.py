"""Build caches retain real pinned checkouts without modifying consumer source."""
from pathlib import Path
import importlib.util
import json
import shlex
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT=Path(__file__).resolve().parents[3]
SPEC=importlib.util.spec_from_file_location('linux_mirror',ROOT/'Packaging/BuildLinux.py')
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)
REVISION='a'*40


class LinuxMirrorTests(unittest.TestCase):
    def setUp(self):
        work=ROOT/'build/linux-mirror-unit';work.mkdir(parents=True,exist_ok=True)
        self.temporary=tempfile.TemporaryDirectory(dir=work);self.addCleanup(self.temporary.cleanup)
        self.base=Path(self.temporary.name);self.source=self.base/'consumer';self.source.mkdir()
        self.cache=self.base/'cache';self.cache.mkdir();self.destination=self.cache/'source';self.destination.mkdir()
        self.relative=Path('3rdParty/AdvancedKeyboard');self.original=self.source/self.relative
        self.target=self.destination/self.relative
        self.lock={'repository':'https://github.com/rohmer/AdvancedKeyboard.git','revision':REVISION,
                   'paths':{'creator':self.relative.as_posix()}}

    def write(self,path,data='fixture'):
        path.parent.mkdir(parents=True,exist_ok=True);path.write_text(data);return path

    def helper(self,dirty='',revision=REVISION):
        self.write(self.source/'advanced-keyboard.lock.json','{}')
        helper=SimpleNamespace(load_lock=Mock(return_value=self.lock),check_checkout=Mock())
        def git(root,*arguments):
            if arguments==('rev-parse','--show-toplevel'):return str(Path(root).resolve())
            if arguments==('status','--porcelain','--untracked-files=all'):return dirty
            if arguments==('rev-parse','HEAD'):return revision
            return ''
        helper.git=Mock(side_effect=git)
        spec=SimpleNamespace(loader=SimpleNamespace(exec_module=Mock()))
        specs=patch('importlib.util.spec_from_file_location',return_value=spec);specs.start();self.addCleanup(specs.stop)
        modules=patch('importlib.util.module_from_spec',return_value=helper);modules.start();self.addCleanup(modules.stop)
        return helper

    def test_source_mirror_includes_helpers_and_skips_git_files(self):
        self.write(self.source/'cmake/AdvancedKeyboardDependency.cmake')
        self.write(self.source/'.github/scripts/desktop.py')
        self.write(self.source/'Tools/NativeTrieGenerator/CMakeLists.txt')
        self.write(self.source/'Tools/NativeTrieGenerator/bin/old-helper.exe','preserve original build')
        self.write(self.source/'Tools/CorpusWorkbench/bin/private-settings.json','preserve original settings')
        self.write(self.source/'Tools/CorpusCrawler/OpenNlp/java-home.txt','preserve original Java path')
        self.write(self.source/'3rdParty/Other/.git','gitdir: private')
        with patch.object(MODULE,'keyboard_checkout',return_value=None):MODULE.mirror(self.source,self.cache)
        for name in ('cmake/AdvancedKeyboardDependency.cmake','.github/scripts/desktop.py','Tools/NativeTrieGenerator/CMakeLists.txt'):
            self.assertTrue((self.destination/name).is_file())
        self.assertFalse((self.destination/'3rdParty/Other/.git').exists())
        self.assertFalse((self.destination/'Tools/CorpusWorkbench').exists())
        self.assertFalse((self.destination/'Tools/CorpusCrawler').exists())
        self.assertFalse((self.destination/'Tools/NativeTrieGenerator/bin').exists())
        self.assertEqual((self.source/'Tools/CorpusWorkbench/bin/private-settings.json').read_text(),'preserve original settings')

    def test_old_source_only_dependency_is_backed_up_before_clone(self):
        helper=self.helper();self.write(self.target/'user-draft.txt','keep exact draft')
        def clone(command,check):
            self.assertEqual(command,['git','clone','--no-hardlinks','--no-checkout','--',str(self.original.resolve()),str(self.target)])
            (self.target/'.git').mkdir(parents=True)
        with patch.object(MODULE.subprocess,'run',side_effect=clone):
            self.assertEqual(MODULE.keyboard_checkout(self.source,self.cache,self.destination),self.relative)
        backups=list((self.cache/'source-backups').glob('*/user-draft.txt'))
        self.assertEqual(len(backups),1);self.assertEqual(backups[0].read_text(),'keep exact draft')
        self.assertEqual(json.loads((self.cache/'keyboard-checkout.json').read_text())['revision'],REVISION)
        helper.git.assert_any_call(self.target,'checkout','--detach',REVISION)
        helper.git.assert_any_call(self.target,'remote','set-url','origin',self.lock['repository'])

    def test_clean_matching_checkout_is_reused_without_clone(self):
        helper=self.helper();(self.target/'.git').mkdir(parents=True)
        with patch.object(MODULE.subprocess,'run') as process:
            MODULE.keyboard_checkout(self.source,self.cache,self.destination)
        process.assert_not_called();self.assertEqual(helper.check_checkout.call_count,2)

    def test_dirty_or_unowned_different_pin_is_preserved(self):
        for dirty,revision in ((' M src/PredictionModel.cpp',REVISION),('', 'b'*40)):
            with self.subTest(dirty=dirty):
                helper=self.helper(dirty,revision);(self.target/'.git').mkdir(parents=True,exist_ok=True)
                with patch.object(MODULE.subprocess,'run') as process,self.assertRaises(ValueError):
                    MODULE.keyboard_checkout(self.source,self.cache,self.destination)
                process.assert_not_called()
                self.assertFalse(any(call.args[1] in ('fetch','checkout') for call in helper.git.call_args_list))

    def test_owned_clean_checkout_updates_only_to_locked_pin(self):
        helper=self.helper(revision='b'*40);(self.target/'.git').mkdir(parents=True)
        self.write(self.cache/'keyboard-checkout.json',json.dumps({'path':self.relative.as_posix(),'repository':self.lock['repository'],'revision':'b'*40}))
        MODULE.keyboard_checkout(self.source,self.cache,self.destination)
        helper.git.assert_any_call(self.target,'fetch','--no-tags',str(self.original.resolve()),REVISION)
        helper.git.assert_any_call(self.target,'checkout','--detach',REVISION)

    def test_old_file_inventory_never_prunes_the_managed_checkout(self):
        self.write(self.source/self.relative/'src/PredictionModel.cpp','consumer source')
        self.write(self.target/'src/PredictionModel.cpp','generated Git checkout')
        self.write(self.destination/'obsolete.txt')
        self.write(self.cache/'source-files.json',json.dumps(['3rdParty/AdvancedKeyboard/src/PredictionModel.cpp','obsolete.txt']))
        with patch.object(MODULE,'keyboard_checkout',return_value=self.relative):MODULE.mirror(self.source,self.cache)
        self.assertEqual((self.target/'src/PredictionModel.cpp').read_text(),'generated Git checkout')
        self.assertFalse((self.destination/'obsolete.txt').exists())

    def test_cache_directory_and_marker_symlinks_cannot_redirect_writes(self):
        outside=self.base/'unrelated';outside.mkdir();sentinel=self.write(outside/'sentinel.txt','keep user data')
        self.destination.rmdir()
        try:self.destination.symlink_to(outside,target_is_directory=True)
        except (OSError,NotImplementedError):self.skipTest('Symlinks unavailable')
        with self.assertRaises(ValueError):MODULE.mirror(self.source,self.cache)
        self.destination.unlink();self.destination.mkdir()
        self.helper();(self.target/'.git').mkdir(parents=True)
        (self.cache/'keyboard-checkout.json').symlink_to(sentinel)
        with self.assertRaises(ValueError):MODULE.keyboard_checkout(self.source,self.cache,self.destination)
        self.assertEqual(sentinel.read_text(),'keep user data')

    def test_native_and_cross_builds_record_the_explicit_cargo_linker(self):
        for arch,triplet,target in (('x86_64','x86_64-linux-gnu','amd64'),('aarch64','aarch64-linux-gnu','arm64')):
            with self.subTest(architecture=arch):
                cache=self.cache/(arch+" with 'quotes' $literal")
                self.write(cache/'tools/tools.json',json.dumps({'cmake':'cmake','rust':{
                    'rustupHome':str(cache/'rustup'),'cargoHome':str(cache/'cargo'),'cargo':str(cache/'cargo/bin/cargo')}}))
                self.write(cache/('sysroots/'+target+'/uicreator-sysroot.json'),'{}')
                (cache/('sysroots/'+target+'/usr/lib/gcc/'+triplet+'/11')).mkdir(parents=True)
                compiler=self.write(cache/('sysroots/amd64/usr/bin/'+triplet+'-gcc-11'))
                self.write(cache/('assets/'+arch+'/python-dependencies.json'),'{}')
                with patch.object(MODULE,'run') as build_run,patch.object(MODULE.subprocess,'check_output',return_value='GCC11 fixture\n'):
                    MODULE.build(cache,self.source,arch,1,configure_only=True)
                directory=cache/('build-'+arch+'-gcc11')
                self.assertIn('-DCMAKE_RUNTIME_OUTPUT_DIRECTORY='+str(directory/'bin'),build_run.call_args.args[0])
                self.assertIn('set(CMAKE_AR "/usr/bin/'+('' if arch=='x86_64' else 'aarch64-linux-gnu-')+'ar")',
                              (directory/'desktop-toolchain.cmake').read_text())
                metadata=json.loads((directory/'build-metadata.json').read_text())
                key='CARGO_TARGET_'+('X86_64' if arch=='x86_64' else 'AARCH64')+'_UNKNOWN_LINUX_GNU_LINKER'
                linker=Path(metadata['environment'][key])
                argv=shlex.split(linker.read_text().splitlines()[1])
                self.assertEqual(argv[:2],['exec',str(compiler)])
                self.assertIn('--sysroot='+str(cache/'sysroots'/target),argv)
                self.assertIn('-B'+str(cache/'crt'/target)+'/',argv)
                self.assertEqual(argv[-1],'$@')


if __name__=='__main__':unittest.main()
