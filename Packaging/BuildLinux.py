#!/usr/bin/env python3
"""Build the complete Linux desktop editor and matching helper resources."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def prepare(base,source,arch):
    import importlib.util
    spec=importlib.util.spec_from_file_location('linux_build_tools',Path(__file__).with_name('BuildLinuxTools.py'))
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    path=base/'tools/tools.json'
    if not path.is_file():helper.tools(base)
    data=json.loads(path.read_text())
    if 'rust' not in data:helper.rust(base)
    helper.sysroot(base,'x86_64')
    if arch!='x86_64':helper.sysroot(base,arch)
    helper.assets(base,source)

def checked(base,path):
    resolved=path.resolve()
    if not resolved.is_relative_to(base.resolve()):raise ValueError('Build path escaped workspace: '+str(path))
    return resolved

def mirror(source,base):
    destination=base/'source';destination.mkdir(parents=True,exist_ok=True)
    previous_path=base/'source-files.json'
    previous=set(json.loads(previous_path.read_text())) if previous_path.is_file() else set()
    files=[];changed=0
    for folder,dirs,names in os.walk(source):
        relative=Path(folder).relative_to(source)
        # Native solution builds keep intermediate objects inside project trees.
        # A configuration name alone is also a legitimate source-directory name;
        # require MSBuild tracking artifacts before excluding the entire tree.
        configuration=relative.name.lower() in {'debug','release','relwithdebinfo','minsizerel','debug static','release static','purify'}
        tracking=any(name.lower().endswith('.tlog') for name in dirs) or any(name.lower().endswith('.vcxproj.filelistabsolute.txt') for name in names)
        if configuration and tracking:
            dirs[:]=[]
            continue
        if relative==Path('.'):
            dirs[:]=[name for name in dirs if name in {'3rdParty','CodeGeneration','ImgLib','LVSerial','Runtime','UICreator','Tests','Packaging','Docs','Docker','Examples'}]
        dirs[:]=[name for name in dirs if name not in {'.git','.agents','.codex','.vs','.idea','.venv','.cache','__pycache__','.pytest_cache'}
                 and (name!='build' or relative.as_posix()=='3rdParty/lv_font_conv/lib/freetype')
                 and (name!='node_modules' or relative.as_posix()=='3rdParty/lv_font_conv')]
        for name in names:
            path=Path(folder)/name;part=relative/name
            if path.suffix in {'.pyc','.pyo'}:continue
            target=checked(base,destination/part);target.parent.mkdir(parents=True,exist_ok=True)
            if not target.is_file() or path.stat().st_size!=target.stat().st_size or path.stat().st_mtime_ns!=target.stat().st_mtime_ns:
                shutil.copy2(path,target);changed+=1
            files.append(part.as_posix())
    for part in previous-set(files):
        target=checked(base,destination/part)
        if target.is_file():target.unlink()
    previous_path.write_text(json.dumps(files)+'\n');print('Source mirror:',len(files),'files,',changed,'updated',flush=True)
    return destination

def run(command,log,environment):
    log.with_suffix('.command.json').write_text(json.dumps(list(map(str,command)),indent=2)+'\n')
    print('Running',log.name,flush=True)
    with log.open('w') as stream:subprocess.run(list(map(str,command)),check=True,stdout=stream,stderr=subprocess.STDOUT,env=environment)
    print(log.name,'PASS',flush=True)

def build(base,source,arch,jobs,configure_only=False,sdk_directory=None,require_sdks=True):
    data=json.loads((base/'tools/tools.json').read_text())
    cmake=Path(data['cmake']);target='amd64' if arch=='x86_64' else 'arm64'
    triplet='x86_64-linux-gnu' if arch=='x86_64' else 'aarch64-linux-gnu'
    sysroot=base/'sysroots'/target
    if not (sysroot/'uicreator-sysroot.json').is_file():raise ValueError('Prepare sysroot first')
    directory=base/('build-'+arch+'-gcc11');directory.mkdir(exist_ok=True)
    assets=base/'assets'/arch
    if not (assets/'python-dependencies.json').is_file():raise ValueError('Prepare helper assets first')
    env=os.environ.copy();env.pop('PYTHONHOME',None);env.pop('PYTHONPATH',None)
    env.update(RUSTUP_HOME=data['rust']['rustupHome'],CARGO_HOME=data['rust']['cargoHome'])
    env['PATH']=str(Path(data['node']['x86_64']).parent)+':'+str(Path(data['rust']['cargo']).parent)+':'+env['PATH']
    env['PKG_CONFIG_SYSROOT_DIR']=str(sysroot)
    env['PKG_CONFIG_LIBDIR']=':'.join(str(sysroot/path) for path in ['usr/lib/'+triplet+'/pkgconfig','usr/share/pkgconfig'])
    env['QEMU_LD_PREFIX']=str(sysroot)
    # Native and cross GCC11 are extracted from signed Ubuntu packages. Keep
    # compiler builtins, C/C++ headers, startup objects and runtime in agreement.
    compiler_root=base/'sysroots/amd64'
    cc=compiler_root/('usr/bin/'+triplet+'-gcc-11')
    cxx=compiler_root/('usr/bin/'+triplet+'-g++-11')
    if not cc.is_file():raise ValueError('Prepare the amd64 compiler sysroot first')
    crt=base/'crt'/target;crt.mkdir(parents=True,exist_ok=True)
    for path in (sysroot/('usr/lib/gcc/'+triplet+'/11')).iterdir():
        if path.is_file() and path.suffix in {'.o','.a','.so'}:shutil.copy2(path,crt/path.name,follow_symlinks=True)
    assembler_directory=Path('/usr/bin') if arch=='x86_64' else Path('/usr/aarch64-linux-gnu/bin')
    compiler_prefix='-B'+str(assembler_directory)+'/ -B'+str(crt)+'/ -B'+str(sysroot/('usr/lib/'+triplet))+'/'
    builtin=compiler_root/('usr/lib/'+('gcc' if arch=='x86_64' else 'gcc-cross')+'/'+triplet+'/11/include')
    cflags=compiler_prefix+' -nostdinc -isystem '+str(builtin)+' -isystem '+str(sysroot/('usr/include/'+triplet))+' -isystem '+str(sysroot/'usr/include')
    cpp=compiler_prefix+' -nostdinc -nostdinc++ -isystem '+str(sysroot/'usr/include/c++/11')+' -isystem '+str(sysroot/('usr/include/'+triplet+'/c++/11'))+' -isystem '+str(builtin)+' -isystem '+str(sysroot/('usr/include/'+triplet))+' -isystem '+str(sysroot/'usr/include')
    link='-L'+str(sysroot/('usr/lib/gcc/'+triplet+'/11'))+' -L'+str(sysroot/('usr/lib/'+triplet))+' -Wl,-rpath-link,'+str(sysroot/('usr/lib/'+triplet))+' -Wl,-rpath-link,'+str(sysroot/('lib/'+triplet))
    toolchain=directory/'desktop-toolchain.cmake'
    content=''
    if arch=='aarch64':content+='set(CMAKE_SYSTEM_NAME Linux)\nset(CMAKE_SYSTEM_PROCESSOR aarch64)\n'
    content+='set(CMAKE_C_COMPILER "'+str(cc)+'")\n'
    content+='set(CMAKE_CXX_COMPILER "'+str(cxx)+'")\n'
    content+='set(CMAKE_SYSROOT "'+str(sysroot)+'")\nset(CMAKE_FIND_ROOT_PATH "'+str(sysroot)+'")\n'
    content+='set(CMAKE_C_FLAGS_INIT "'+cflags+'")\nset(CMAKE_CXX_FLAGS_INIT "'+cpp+'")\nset(CMAKE_ASM_FLAGS_INIT "'+compiler_prefix+'")\nset(CMAKE_EXE_LINKER_FLAGS_INIT "'+link+'")\nset(CMAKE_SHARED_LINKER_FLAGS_INIT "'+link+'")\n'
    content+='set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\nset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n'
    if arch=='aarch64':content+='set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/qemu-aarch64;-L;'+str(sysroot)+'" CACHE STRING "")\n'
    if not toolchain.is_file() or toolchain.read_text()!=content:toolchain.write_text(content)
    sdk=(sdk_directory or Path(__file__).resolve().parents[1]/'build/professional/packages').resolve()
    command=[cmake,'-S',source,'-B',directory,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_C_COMPILER='+str(cc),'-DCMAKE_CXX_COMPILER='+str(cxx),'-DCMAKE_ASM_COMPILER='+str(cc),
        '-DCMAKE_AR='+('/usr/bin/ar' if arch=='x86_64' else '/usr/bin/aarch64-linux-gnu-ar'),
        '-DCMAKE_RANLIB='+('/usr/bin/ranlib' if arch=='x86_64' else '/usr/bin/aarch64-linux-gnu-ranlib'),
        '-DCMAKE_LINKER='+('/usr/bin/ld' if arch=='x86_64' else '/usr/bin/aarch64-linux-gnu-ld'),
        '-DCMAKE_NM='+('/usr/bin/nm' if arch=='x86_64' else '/usr/bin/aarch64-linux-gnu-nm'),
        '-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DBUILD_TESTING=ON','-DUICREATOR_USE_LVGL9=ON',
        '-DCMAKE_C_FLAGS='+cflags,'-DCMAKE_CXX_FLAGS='+cpp,'-DCMAKE_ASM_FLAGS='+compiler_prefix,
        '-DCMAKE_EXE_LINKER_FLAGS='+link,'-DCMAKE_SHARED_LINKER_FLAGS='+link,
        '-DPython3_EXECUTABLE=/usr/bin/python3','-DIMAGEQUANT_CARGO='+data['rust']['cargo'],
        '-DUICREATOR_FONT_NODE='+data['node'][arch],'-DUICREATOR_FONT_NPM='+str(Path(data['node']['x86_64']).parent/'npm'),
        '-DUICREATOR_PREBUILT_IMAGE_TOOL_DIR='+str(assets/'image-converter'),
        '-DUICREATOR_PREBUILT_PROJECT_TOOL_DIR='+str(assets/'project-creator'),
        '-DUICREATOR_LINUX_PYTHON_DIR='+str(assets/'python-runtime'),
        '-DSDL_X11=ON','-DSDL_WAYLAND=ON','-DSDL_X11_SHARED=OFF','-DSDL_WAYLAND_SHARED=OFF',
        '-DSDL_TESTS=OFF','-DSDL_EXAMPLES=OFF','-DFT_DISABLE_HARFBUZZ=ON','-DFT_DISABLE_BROTLI=ON',
        '-DUICREATOR_LINUX_SDK_DIRECTORY='+str(sdk),
        '-DUICREATOR_LINUX_REQUIRE_SDKS='+('ON' if require_sdks else 'OFF'),
        '-DUICREATOR_LINUX_DEPENDENCY_DIRECTORIES='+str(sysroot/('usr/lib/'+triplet))+';'+str(sysroot/('lib/'+triplet))+';'+str(sysroot/('usr/lib/gcc/'+triplet+'/11')),
        '-DUICREATOR_LINUX_OBJDUMP='+('/usr/bin/objdump' if arch=='x86_64' else '/usr/bin/aarch64-linux-gnu-objdump')]
    signature=hashlib.sha256(json.dumps({'compiler':str(cc),'cflags':cflags,'cxxflags':cpp,'binutils':'system-target-v1'}).encode()).hexdigest()
    signature_path=directory/'toolchain-signature.txt'
    if not signature_path.is_file() or signature_path.read_text()!=signature:
        command.insert(1,'--fresh')
    run(command,directory/'configure.log',env);signature_path.write_text(signature)
    targets=['UICreator','UICreatorViewer','UICreatorCodeGen','LVGLCompiler','UICreatorEditorTests',
             'UICreatorVideoBackends','UICreatorNativeSDL3Tests','UICreatorFontConverterTests','UICreatorAssetLibraryTests',
             'UICreatorProjectCreationTests','UICreatorLanguageModelTests','UICreatorNativeFileDialogTests']
    if not configure_only:run([cmake,'--build',directory,'--parallel',jobs,'--target',*targets],directory/'build.log',env)
    metadata={'architecture':arch,'buildDirectory':str(directory),'sourceDirectory':str(source),'cmake':str(cmake),
              'sysroot':str(sysroot),'binaryDirectory':str(directory/'bin'),'targets':targets,
              'compiler':{'c':str(cc),'cxx':str(cxx),'version':subprocess.check_output([str(cxx),'--version'],text=True).splitlines()[0]},
              'targetLibraryDirectories':[str(sysroot/('usr/lib/'+triplet)),str(sysroot/('lib/'+triplet))],
              'environment':{name:env[name] for name in ['PATH','QEMU_LD_PREFIX','PKG_CONFIG_SYSROOT_DIR','PKG_CONFIG_LIBDIR','RUSTUP_HOME','CARGO_HOME']}}
    (directory/'build-metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--base',type=Path,default=Path('/var/tmp/lvgl-ui-desktop'))
    parser.add_argument('--architecture',choices=['x86_64','aarch64'],required=True)
    parser.add_argument('--jobs',type=int,default=6)
    parser.add_argument('--sdk-directory',type=Path,help='Published SDK ZIP/tar.gz archives to include in installers')
    parser.add_argument('--without-sdks',action='store_true',help='Allow development packages without the complete published SDK archive set')
    parser.add_argument('--sync-only',action='store_true');parser.add_argument('--skip-sync',action='store_true')
    parser.add_argument('--configure-only',action='store_true')
    parser.add_argument('--prepare',action='store_true',help='Fetch verified private tools/sysroot/helpers before building')
    args=parser.parse_args();base=args.base.resolve();base.mkdir(parents=True,exist_ok=True)
    if args.prepare:prepare(base,args.source.resolve(),args.architecture)
    source=base/'source' if args.skip_sync else mirror(args.source.resolve(),base)
    if not args.sync_only:build(base,source,args.architecture,args.jobs,args.configure_only,args.sdk_directory,not args.without_sdks)
if __name__=='__main__':main()
