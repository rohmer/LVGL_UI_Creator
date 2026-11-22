#!/usr/bin/env python3
"""Exercise the real desktop binary and native helper runtimes, optionally under QEMU."""
from pathlib import Path
import argparse, json, os, subprocess, time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base',type=Path,default=Path('/var/tmp/lvgl-ui-desktop'))
    parser.add_argument('--architecture',required=True,choices=['x86_64','aarch64'])
    parser.add_argument('--helpers-only',action='store_true')
    args=parser.parse_args();base=args.base.resolve();arch=args.architecture
    build=base/('build-'+arch+'-gcc11');assets=base/'assets'/arch
    tools=json.loads((base/'tools/tools.json').read_text());sysroot=base/'sysroots'/('amd64' if arch=='x86_64' else 'arm64')
    prefix=[] if arch=='x86_64' else ['/usr/bin/qemu-aarch64','-L',str(sysroot)]
    environment=os.environ.copy();environment['QEMU_LD_PREFIX']=str(sysroot)
    environment['SDL_VIDEO_DRIVER']='dummy';environment['SDL_RENDER_DRIVER']='software'
    output=build/'validation';output.mkdir(parents=True,exist_ok=True)
    temporary=output/'tmp';temporary.mkdir(exist_ok=True);environment['TMPDIR']=str(temporary)
    # Architecture checks must not share desktop preferences or recovery state
    # with another check run or an interactive editor on the build host.
    environment['UICREATOR_SESSION_DIR']=str(temporary/('session-'+str(os.getpid())))
    records=[]
    def run(name,command):
        started=time.monotonic()
        with (output/(name+'.log')).open('w') as stream:
            subprocess.run(list(map(str,command)),check=True,stdout=stream,stderr=subprocess.STDOUT,env=environment)
        records.append({'test':name,'seconds':round(time.monotonic()-started,3)})
        print(name,'PASS',flush=True)
    python=assets/'python-runtime/bin/python3'
    run('native-python-image-modules',[*prefix,python,'-I','-c',
        'import PIL,png,lz4.frame,resvg_py;assert lz4.frame.decompress(lz4.frame.compress(b"native wheels"))==b"native wheels";print(PIL.__version__)'])
    run('native-node',[*prefix,tools['node'][arch],'--version'])
    if not args.helpers_only:
        metadata=json.loads((build/'build-metadata.json').read_text());environment.update(metadata['environment'])
        environment['TMPDIR']=str(temporary);environment['SDL_VIDEO_DRIVER']='dummy';environment['SDL_RENDER_DRIVER']='software'
        run('compiled-desktop-backends',[*prefix,build/'bin/UICreatorVideoBackends','--require-x11','--require-wayland'])
        run('desktop-startup',[*prefix,build/'bin/UICreator','--smoke-test'])
        tests='^(UICreatorNativeSDL3|UICreatorNativeFileDialog|UICreatorFileBrowser|UICreatorSmoke|UICreatorAdvancedKeyboardEditor|UICreatorMenus|UICreatorRunMode|UICreatorPerformance|UICreatorPolishCore|UICreatorVisualAuthoring|UICreatorWorkflowProblems|UICreatorDebugScreen|UICreatorFontConverter|UICreatorAssetLibrary|UICreatorLanguageModels|UICreatorMicroPython.*|UICreatorFigma.*|UICreatorXml.*)$'
        run('focused-desktop-ctest',[Path(tools['cmake']).with_name('ctest'),'--test-dir',build,'--output-on-failure','--parallel','1','-R',tests])
    (output/'verified.json').write_text(json.dumps({'architecture':arch,'tests':records},indent=2)+'\n')
if __name__=='__main__':main()
