#!/usr/bin/env python3
"""Regenerate optional keyboard script fonts from pinned, OFL-licensed sources."""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2] if HERE.parent.name == 'Widgets' else HERE.parent
SPECS = {
    'hindi': ('Hindi', 'NotoSansDevanagari', 'notosansdevanagari', '2bc5d431b584d89e646214bf6cfa75494e76ceea', '[wdth,wght]',
              '14ec4af41f27482216d1c2229f417ff9b1425e1babb014e57d1d40d03229853e', 'a216f6f8d85c7228093e0ee5e258d9d377e6671f68acb4db1930b29583d0f331', '0x0900-0x097f,0x25cc'),
    'bengali': ('Bengali', 'NotoSansBengali', 'notosansbengali', '73b3937823dfe9d7f4cdcc0b2399a1f06940430d', '[wdth,wght]',
                'dcd42978094e584a849c84a51450eeac40c8826057d566ea6d4b9627a403a05a', '754f0e221aa7d5a915489f3bf1f20fe53ddc35ab2834a4d91656d78f9622de70', '0x0980-0x09ff,0x25cc'),
    'cyrillic': ('Cyrillic', 'NotoSans', 'notosans', '2984c575fdce412ee02b2baaba67672b9a9434d8', '[wdth,wght]',
                 'bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d', 'cee9892f9f0cc8fe882c9e9537ee6a89621d86ee7ceaf70b02e2b2b1c25c061a', '0x0400-0x052f,0x2116'),
    'urdu': ('Urdu', 'NotoNaskhArabic', 'notonaskharabic', '35ddfc50b8c0dda6a1e0eb5d48d962c26a2ec5ff', '[wght]',
             '67b5a525a661b607971fbd3f96a81b89d3a768e74534fca84f18ac97e6fab72f', 'a7a5a25eb188bf1cd96982030d53e23c33485c69b1044a562254226857ee13af', '0x0600-0x06ff,0x0750-0x077f,0x08a0-0x08ff,0xfb50-0xfdff,0xfe70-0xfeff,0x25cc'),
}

def generate(name, args):
    import urllib.parse
    title, family, folder, revision, axes, font_hash, license_hash, ranges = SPECS[name]
    base = 'https://raw.githubusercontent.com/google/fonts/' + revision + '/ofl/' + folder + '/'
    sources = {family+'.ttf': (urllib.parse.quote(family+axes+'.ttf'), font_hash),
               family+'.OFL.txt': ('OFL.txt', license_hash)}
    for file, (remote, expected) in sources.items():
        path = args.cache / file
        if not path.is_file():
            request = urllib.request.Request(base + remote, headers={'User-Agent': 'LVGL-UI-Creator-font-builder'})
            with urllib.request.urlopen(request, timeout=60) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError('Downloaded source checksum differs: ' + file)
            path.write_bytes(data)
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Cached source checksum differs: ' + file)
    output = HERE / ('Keyboard' + title + 'Font.c')
    subprocess.run([args.node, str(args.converter),
                    '--font', str(args.cache / (family + '.ttf')),
                    '--range', ranges, '--size', '16', '--bpp', '4',
                    '--no-compress', '--no-prefilter', '--format', 'lvgl', '--lv-include', 'lvgl.h',
                    '--lv-font-name', 'espos_keyboard_' + name + '_font', '-o', str(output)], check=True)
    text = output.read_text(encoding='utf-8')
    text = text.replace(str(args.cache / (family + '.ttf')), family + '.ttf')
    text = text.replace(str(output), output.name)
    guard = 'LV_ADVANCED_KEYBOARD_USE_' + name.upper() + '_FONT'
    text = text.replace('ESPOS_KEYBOARD_' + name.upper() + '_FONT', guard)
    text = text.replace('#define ' + guard + ' 1', '#define ' + guard + ' 0')
    provenance = ('/* ' + family + ', Google Fonts revision ' + revision + '\n'
                  ' * Source SHA256: ' + font_hash + '\n'
                  ' * SIL Open Font License 1.1; see Keyboard' + title + 'Font.LICENSE.txt.\n'
                  ' * Regenerate with GenerateHindiFont.py --font ' + name + ' (16 px, 4 bpp).\n'
                  ' * This glyph font does not add OpenType conjunct shaping to LVGL.\n */\n')
    output.write_text(provenance + text, encoding='utf-8', newline='\n')
    shutil.copyfile(args.cache / (family + '.OFL.txt'), HERE / ('Keyboard' + title + 'Font.LICENSE.txt'))
    print(str(output))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=ROOT / 'build/font-sources/hindi')
    parser.add_argument('--node', default='node')
    parser.add_argument('--converter', type=Path, default=ROOT / '3rdParty/lv_font_conv/lv_font_conv.js')
    parser.add_argument('--font', choices=['all', *SPECS], default='all')
    args = parser.parse_args()
    args.cache.mkdir(parents=True, exist_ok=True)
    for name in SPECS if args.font == 'all' else [args.font]:
        generate(name, args)

if __name__ == '__main__':
    main()
