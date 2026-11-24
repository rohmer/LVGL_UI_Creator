"""Run the shipped converter in isolation, with no script runtime or helper assets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--repository', type=Path, required=True)
    args = parser.parse_args()
    tool, repo = args.tool.resolve(), args.repository.resolve()
    with tempfile.TemporaryDirectory(prefix='native-converters-') as temporary:
        root = Path(temporary) / 'native only 日本'
        root.mkdir()
        executable = root / tool.name
        shutil.copy2(tool, executable)
        for native in tool.parent.iterdir():
            if native.is_file() and (native.suffix.lower() in ('.dll', '.dylib') or '.so' in native.name):
                shutil.copy2(native, root / native.name)
        env = dict(os.environ, PATH=str(root), PYTHONHOME=str(root / 'no-python'),
                   PYTHONPATH='', NODE_PATH='', NODE_OPTIONS='--invalid-native-converter-must-not-use-node')
        assert not (root / 'font-converter').exists()
        assert not (root / 'image-converter').exists()

        def run(*arguments):
            result = subprocess.run([str(executable), *map(str, arguments)], env=env, cwd=root,
                                    capture_output=True, text=True, timeout=60)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)

        run('--help')
        font = root / 'font 日本.ttf'
        shutil.copy2(repo / '3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf', font)
        font_job = root / 'font job'; font_job.mkdir()
        request = font_job / 'request 日本.json'
        request.write_text(json.dumps({'args': ['--font', str(font), '--range', '0x41', '--range', '0x56',
            '--symbols', 'B', '--symbols', 'C',
            '--size', '19', '--bpp', '4', '--format', 'lvgl', '--output', str(font_job / 'font.c'),
            '--lv-font-name', 'native_font', '--lv-include', 'lvgl.h']}), encoding='utf-8')
        run('font', '--request', request)
        result = json.loads((font_job / 'result.json').read_text(encoding='utf-8'))
        assert result['files'] and (font_job / 'preview.bin').stat().st_size > 0
        code = (font_job / 'font.c').read_text(encoding='utf-8')
        assert 'native_font' in code and code.count('.bitmap_index =') == 4
        for entry in result['files']:
            assert (font_job / entry['staged']).stat().st_size > 0

        image = root / 'image 日本.svg'
        image.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="13" height="9">'
                         '<defs><filter id="blur"><feGaussianBlur stdDeviation="0.4"/></filter></defs>'
                         '<rect x="1" y="1" width="11" height="7" fill="#24a8c0" filter="url(#blur)"/></svg>',
                         encoding='utf-8')
        image_job = root / 'image job'; image_job.mkdir()
        request = image_job / 'request 日本.json'
        request.write_text(json.dumps({'sources': [str(image)], 'options': {
            'colorFormat': 'ARGB8888', 'format': 'BIN', 'compression': 'LZ4', 'align': 4}}), encoding='utf-8')
        run('image', '--request', request)
        result = json.loads((image_job / 'result.json').read_text(encoding='utf-8'))
        assert result['assets'] and result['files']
        frame = result['assets'][0]['deviceFrames'][0]
        assert frame['w'] == 13 and frame['h'] == 9 and frame['flags'] & 8
        for entry in result['files']:
            assert (image_job / entry['staged']).stat().st_size > 12
        print('Native font and SVG/LZ4 image conversion passed with no helper folders or runtime PATH.')


if __name__ == '__main__':
    main()
