#!/usr/bin/env python3
"""Prepare verified, architecture-specific Linux tools without system installs."""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, shutil, subprocess, tarfile, tempfile, urllib.request

DEFAULT_BASE=Path('/var/tmp/lvgl-ui-desktop')
CMAKE_VERSION='4.5.1'
RUST_VERSION='1.85.1'
PYTHON_RELEASE='20261110'
PYTHON_VERSION='3.11.18'
IMAGE_REQUIREMENTS=[]

def fetch(url):
    request=urllib.request.Request(url,headers={'User-Agent':'LVGL-UI-Creator-Linux-Builder'})
    with urllib.request.urlopen(request,timeout=120) as response:return response.read()

def download(url,path,digest):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==digest:return path
    payload=fetch(url)
    if hashlib.sha256(payload).hexdigest()!=digest:raise ValueError('Download checksum mismatch: '+url)
    temporary=path.with_name(path.name+'.partial');temporary.write_bytes(payload);temporary.replace(path)
    return path

def extract(archive,destination):
    destination.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive) as stream:stream.extractall(destination,filter='data')

def release_asset(repository,tag,name):
    release=json.loads(fetch('https://api.github.com/repos/'+repository+'/releases/tags/'+tag))
    asset=next(item for item in release['assets'] if item['name']==name)
    digest=asset.get('digest','')
    if not digest.startswith('sha256:'):raise ValueError('Release does not publish SHA256 for '+name)
    return asset['browser_download_url'],digest.split(':',1)[1]

def tools(base):
    output=base/'tools';downloads=output/'downloads'
    checksums=fetch('https://github.com/Kitware/CMake/releases/download/v'+CMAKE_VERSION+'/cmake-'+CMAKE_VERSION+'-SHA-256.txt').decode()
    name='cmake-'+CMAKE_VERSION+'-linux-x86_64.tar.gz'
    digest=next(line.split()[0] for line in checksums.splitlines() if line.split()[-1]==name)
    archive=download('https://github.com/Kitware/CMake/releases/download/v'+CMAKE_VERSION+'/'+name,downloads/name,digest)
    cmake=output/('cmake-'+CMAKE_VERSION+'-linux-x86_64')
    if not (cmake/'bin/cmake').is_file():extract(archive,output)
    result={'cmake':str(cmake/'bin/cmake'),'python':{},'artifacts':{name:digest}}
    def target(arch):
        python_name='cpython-'+PYTHON_VERSION+'+'+PYTHON_RELEASE+'-'+arch+'-unknown-linux-gnu-install_only_stripped.tar.gz'
        python_url,python_hash=release_asset('astral-sh/python-build-standalone',PYTHON_RELEASE,python_name)
        python_archive=download(python_url,downloads/python_name,python_hash)
        python=output/('python-'+arch)
        if not (python/'python/bin/python3').is_file():extract(python_archive,python)
        return arch,str(python/'python'),{python_name:python_hash}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for arch,python,artifacts in pool.map(target,['x86_64','aarch64']):
            result['python'][arch]=python;result['artifacts'].update(artifacts)
    (output/'tools.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)
    return result

SYSROOT_PACKAGES=['libc6-dev','libstdc++-11-dev','libgcc-11-dev','libx11-dev','libxext-dev',
    'libxcursor-dev','libxfixes-dev','libxi-dev','libxrandr-dev','libxrender-dev','libxss-dev','libxtst-dev',
    'libwayland-dev','libwayland-bin','wayland-protocols','libxkbcommon-dev','libegl1-mesa-dev',
    'libgl1-mesa-dev','libdrm-dev','libgbm-dev','libudev-dev','libdbus-1-dev','libdecor-0-dev',
    'libasound2-dev','libpulse-dev','zlib1g-dev','libbz2-dev','libpng-dev','pkg-config']

def relocate_sysroot(destination):
    for folder,dirs,names in os.walk(destination):
        for name in dirs+names:
            path=Path(folder)/name
            if path.is_symlink():
                value=os.readlink(path)
                if value.startswith('/'):
                    target=destination/value.lstrip('/')
                    path.unlink();path.symlink_to(os.path.relpath(target,path.parent))

def sysroot(base,arch):
    target='amd64' if arch=='x86_64' else 'arm64'
    destination=base/'sysroots'/target
    marker=destination/'uicreator-sysroot.json'
    requested=SYSROOT_PACKAGES+(['gcc-11','g++-11','gcc-11-aarch64-linux-gnu','g++-11-aarch64-linux-gnu'] if target=='amd64' else [])
    if marker.is_file() and set(requested)<=set(json.loads(marker.read_text()).get('requestedPackages',[])):
        relocate_sysroot(destination);return destination
    apt=base/'tools'/('apt-'+target)
    for name in ['state/lists/partial','cache/archives/partial','log']: (apt/name).mkdir(parents=True,exist_ok=True)
    (apt/'status').touch()
    mirror='https://archive.ubuntu.com/ubuntu' if target=='amd64' else 'https://ports.ubuntu.com/ubuntu-ports'
    sources=apt/'sources.list'
    sources.write_text('\n'.join('deb [arch='+target+' signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg] '+mirror+' '+suite+' main universe' for suite in ['jammy','jammy-updates','jammy-security'])+'\n')
    options=['-o','Dir::Etc::sourcelist='+str(sources),'-o','Dir::Etc::sourceparts=-',
             '-o','Dir::State='+str(apt/'state'),'-o','Dir::State::status='+str(apt/'status'),
             '-o','Dir::Cache='+str(apt/'cache'),'-o','Dir::Log='+str(apt/'log'),
             '-o','APT::Architecture='+target,'-o','APT::Architectures::='+target,
             '-o','Debug::NoLocking=true','-o','APT::Sandbox::User=root']
    with (apt/'prepare.log').open('w') as log:
        subprocess.run(['apt-get',*options,'update'],check=True,stdout=log,stderr=subprocess.STDOUT)
        subprocess.run(['apt-get',*options,'--download-only','--no-install-recommends','-y','install',*requested],check=True,stdout=log,stderr=subprocess.STDOUT)
    destination.mkdir(parents=True,exist_ok=True)
    files={}
    for package in sorted((apt/'cache/archives').glob('*.deb')):
        subprocess.run(['dpkg-deb','-x',package,destination],check=True)
        files[package.name]=hashlib.sha256(package.read_bytes()).hexdigest()
    relocate_sysroot(destination)
    marker.write_text(json.dumps({'architecture':target,'suite':'Ubuntu 22.04 LTS (jammy)','requestedPackages':requested,'packages':files},indent=2)+'\n')
    print(str(destination),'prepared',len(files),'verified packages',flush=True)
    return destination

def assets(base,source):
    data=json.loads((base/'tools/tools.json').read_text())
    host=Path(data['python']['x86_64'])/'bin/python3'
    root=base/'assets'
    for arch in ['x86_64','aarch64']:
        output=root/arch;output.mkdir(parents=True,exist_ok=True)
        runtime=output/'python-runtime'
        marker=output/'python-dependencies.json'
        key={'python':PYTHON_VERSION,'requirements':[]}
        previous=json.loads(marker.read_text()) if marker.is_file() else None
        if not (runtime/'bin/python3').is_file() or previous!=key:
            if runtime.is_symlink() or runtime.resolve().parent!=output.resolve():
                raise ValueError('Project runtime staging escaped its architecture cache')
            source_runtime=Path(data['python'][arch]).resolve(strict=True)
            if source_runtime==runtime.resolve() or source_runtime.is_relative_to(runtime.resolve()):
                raise ValueError('Project runtime source overlaps its generated destination')
            # Replace the owned runtime with its pristine stdlib-only source.
            # Old Pillow/resvg/LZ4 modules must not survive an incremental build.
            with tempfile.TemporaryDirectory(prefix='.project-runtime-',dir=output) as temporary:
                staged=Path(temporary)/'python-runtime'
                shutil.copytree(source_runtime,staged,symlinks=True)
                if runtime.exists():shutil.rmtree(runtime)
                staged.rename(runtime)
        site=runtime/'lib/python3.11/site-packages'
        # The application runs installed modules; package managers and their
        # embedded Windows launcher binaries are build tools, not app assets.
        for pattern in ['pip','pip-*.dist-info','setuptools','setuptools-*.dist-info','pkg_resources','_distutils_hack']:
            for path in site.glob(pattern):
                if not path.resolve().is_relative_to(runtime.resolve()):raise ValueError('Python tool cleanup escaped runtime')
                if path.is_dir():shutil.rmtree(path)
        for path in (runtime/'bin').glob('pip*'):
            if path.is_file():path.unlink()
        for name in ['distutils-precedence.pth','easy-install.pth']:
            path=site/name
            if path.is_file():path.unlink()
        ensurepip=runtime/'lib/python3.11/ensurepip'
        if ensurepip.is_dir() and ensurepip.resolve().is_relative_to(runtime.resolve()):shutil.rmtree(ensurepip)
        marker.write_text(json.dumps(key,indent=2)+'\n')
        project=output/'project-creator';project.mkdir(exist_ok=True)
        for name in ['project_creator.py','raspberry_pi_linux.py','raspberry_pi_pico.py']:
            shutil.copy2(source/'UICreator/ProjectCreator'/name,project/name)
        launcher = '#!/bin/sh\nset -eu\nTOOL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        launcher += 'exec "$TOOL_DIR/../python-runtime/bin/python3" -I -c \'import runpy,sys;from pathlib import Path;script=sys.argv.pop(1);sys.path.insert(0,str(Path(script).parent));runpy.run_path(script,run_name="__main__")\' "$TOOL_DIR/project_creator.py" "$@"\n'
        (project/'UICreatorProjectTool').write_text(launcher)
        (project/'UICreatorProjectTool').chmod(0o755)
        print('Prepared native helper assets:',arch,flush=True)

def rust(base):
    output=base/'tools';environment=os.environ.copy()
    environment.update(RUSTUP_HOME=str(output/'rustup'),CARGO_HOME=str(output/'cargo'))
    executable=output/'cargo/bin/cargo'
    if not executable.is_file():
        url='https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu/rustup-init'
        digest=fetch(url+'.sha256').decode().split()[0]
        installer=download(url,output/'downloads/rustup-init',digest);installer.chmod(0o755)
        subprocess.run([installer,'-y','--no-modify-path','--profile','minimal','--default-toolchain',RUST_VERSION,
                        '--target','aarch64-unknown-linux-gnu'],check=True,env=environment)
    data=json.loads((output/'tools.json').read_text());data['rust']={'version':RUST_VERSION,'cargo':str(executable),
        'rustupHome':environment['RUSTUP_HOME'],'cargoHome':environment['CARGO_HOME']}
    (output/'tools.json').write_text(json.dumps(data,indent=2)+'\n');print('Prepared pinned Linux Rust toolchain',RUST_VERSION,flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=['tools','sysroot','assets','rust','all'])
    parser.add_argument('--base',type=Path,default=DEFAULT_BASE)
    parser.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--architecture',choices=['x86_64','aarch64','both'],default='both')
    args=parser.parse_args();base=args.base.resolve();base.mkdir(parents=True,exist_ok=True)
    if args.mode in ['tools','all']:tools(base)
    if args.mode in ['rust','all']:rust(base)
    if args.mode in ['sysroot','all']:
        for arch in (['x86_64','aarch64'] if args.architecture=='both' else [args.architecture]):sysroot(base,arch)
    if args.mode in ['assets','all']:assets(base,args.source.resolve())
if __name__=='__main__':main()
