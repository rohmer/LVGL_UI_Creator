"""Run the exported UI on LVGL-enabled MicroPython, directly or through WSL."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interpreter', required=True)
    parser.add_argument('--export', required=True, type=Path)
    parser.add_argument('--wsl-distribution', default='')
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    script = Path(__file__).with_name('native_binding_smoke.py').resolve()
    exported = args.export.resolve()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    result = {'status': 'failed', 'export': str(exported), 'sourceRuntimeOverride': False}
    try:
        for name in ('ui.py', 'ui.json', 'uicreator_lvgl.py', 'export-manifest.json'):
            if not (exported / name).is_file():
                raise ValueError('Missing generated fixture: ' + name + '; run the MicroPython export setup test')
        manifest = json.loads((exported / 'export-manifest.json').read_text(encoding='utf-8'))
        result['requestedBinding'] = manifest['binding']
        result['lvglVersion'] = manifest['lvglVersion']
        if args.wsl_distribution:
            if os.name != 'nt':
                raise ValueError('--wsl-distribution is only needed on a Windows host')
            prefix = ['wsl.exe', '-d', args.wsl_distribution, '--exec']

            def linux_path(path):
                return subprocess.check_output(prefix + ['wslpath', '-a', '-u', Path(path).as_posix()],
                                               text=True, encoding='utf-8', timeout=15).strip()

            interpreter = args.interpreter if args.interpreter.startswith('/') else linux_path(Path(args.interpreter).resolve())
            command = prefix + [interpreter, linux_path(script), linux_path(exported)]
        else:
            command = [args.interpreter, str(script), str(exported)]
        result['command'] = command
        process = subprocess.run(command, text=True, encoding='utf-8', errors='replace',
                                 capture_output=True, timeout=70)
        result.update(exitCode=process.returncode, stdout=process.stdout, stderr=process.stderr)
        sys.stdout.write(process.stdout)
        sys.stderr.write(process.stderr)
        if process.returncode:
            raise RuntimeError('Native MicroPython test failed with exit code ' + str(process.returncode))
        native = json.loads(process.stdout.strip().splitlines()[-1])
        if native.get('status') != 'passed' or native.get('hardware') is not False or not native.get('cases') or native.get('displayFlushes', 0) <= 0:
            raise ValueError('The native interpreter did not report successful rendering checks')
        result.update(status='passed', native=native)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyError, IndexError) as error:
        result['error'] = str(error)
        print('Native MicroPython verification failed: ' + str(error), file=sys.stderr)
    args.report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
