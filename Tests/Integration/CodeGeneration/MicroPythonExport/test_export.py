"""Exercise the real C++ export CLI, assets, compatibility errors and PB input."""
import argparse
import base64
import copy
import json
from pathlib import Path
import subprocess
import tempfile


def run(*args, ok=True):
    result = subprocess.run([str(item) for item in args], text=True, capture_output=True)
    if ok and result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    if not ok and result.returncode == 0:
        raise AssertionError('Invalid export unexpectedly succeeded')
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--codegen', required=True, type=Path)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sample = args.source_root / 'Tests/Fixtures/Images/sample.gif'
    image = {'asset': {'name': sample.name, 'sourceData': base64.b64encode(sample.read_bytes()).decode()}}
    image['innerAlign'] = 15  # LVGL 9.6 Cover becomes 14 in the binding's LVGL 9.3.
    widgets = [
        {'oid': 1, 'par': -1, 'base': {'coords': {'width': 300, 'height': 200}, 'nativeStyles': [
            {'selector': 0, 'values': {'bg_color': 0x123456, 'radius': 6}}]}},
        {'oid': 2, 'par': 1, 'base': {'coords': {'width': 536871002}}, 'text': 'Label "quoted"\nUnicode é'},
        {'oid': 3, 'par': 1, 'base': {}, 'slider': {'min': 0, 'max': 100, 'value': 7}},
        {'oid': 4, 'par': 1, 'base': {}, 'textarea': {'text': 'Type here'}},
        {'oid': 5, 'par': 1, 'base': {}, 'qrcode': {'data': 'https://lvgl.io', 'size': 64, 'quietZone': True,
                                                'darkColor': 0xff2244aa, 'lightColor': 0xffffcc88}},
        {'oid': 6, 'par': 1, 'base': {'coords': {'width': 360, 'height': 40}},
         'barcode': {'data': '12345678', 'scale': 2, 'direction': 3}},
        {'oid': 7, 'par': 1, 'base': {}, 'img': image},
        {'oid': 8, 'par': 1, 'base': {}, 'canvas': {'width': 2, 'height': 2, 'format': 16,
                                                'fillColor': 0x336699, 'fillOpacity': 255}},
    ]
    project = {'screens': [{'name': 'Main', 'canvas': {'width': 320, 'height': 240}, 'obj': widgets},
                           {'name': 'Other', 'canvas': {'width': 320, 'height': 240}, 'obj': []}],
               'activeScreen': 0, 'deviceTargets': [{'private': 'not part of runtime'}],
               'variables': [{'var': {'name': 'level', 'dataType': 1, 'default': 7, 'history': 2,
                                      'code': {'cType': 'int32_t', 'symbol': 'private_sensor'},
                                      'bindings': [{'screen': 0, 'oid': 3, 'property': '/slider/value', 'direction': 'two-way'},
                                                   {'screen': 0, 'oid': 2, 'property': '/text', 'direction': 'to-widget'}]}}]}
    source = args.output / 'fixture.json'
    source.write_text(json.dumps(project), encoding='utf-8')
    destination = args.output / 'generated'
    run(args.codegen, '--format', 'micropython', source, destination)
    manifest = json.loads((destination / 'export-manifest.json').read_text())
    assert manifest['binding']['repository'] == 'https://github.com/lvgl/lv_binding_micropython'
    assert manifest['binding']['revision'] == '032dda4b7e8419702a4b9c4c8748a4d85d51be61'
    assert manifest['lvglVersion'] == '9.3.0'
    assert len(manifest['staticCodeWidgets']) == 2
    for filename in ('ui.py', 'main.py', 'uicreator_lvgl.py'):
        compile((destination / filename).read_text(encoding='utf-8'), filename, 'exec')
    exported = json.loads((destination / 'ui.json').read_text())
    encoded = json.dumps(exported)
    assert 'deviceTargets' not in exported and 'private_sensor' not in encoded and 'sourceData' not in encoded
    records = exported['screens'][0]['obj']
    assert records[1]['base']['coords']['width'] == {'percent': 90}
    assert 'qrcode' not in records[4] and 'barcode' not in records[5]
    assert records[4]['img']['mpyAsset'] and records[5]['img']['mpyAsset']
    # Code 128 keeps its encoded module width (99 * scale 2), while the
    # non-tiled perpendicular bitmap dimension follows the authored height.
    assert records[5]['img']['width'] == 198 and records[5]['img']['height'] == 40
    assert records[5]['base']['coords'] == {'width': 360, 'height': 40}
    assert records[5]['img']['innerAlign'] == 0
    assert len(records[6]['img']['frames']) >= 2
    assert records[6]['img']['innerAlign'] == 14
    assert records[7]['canvas']['mpyAsset'] and 'data' not in records[7]['canvas']
    for asset in manifest['assets']:
        assert (destination / asset['file']).stat().st_size == asset['bytes']
    requirements = (destination / 'lv_conf.defaults.h').read_text()
    assert 'LV_USE_IMAGE' in requirements and 'LV_USE_BARCODE' not in requirements and 'LV_USE_QRCODE' not in requirements
    # The public compiler frontend must provide the same deterministic export.
    second = args.output / 'compiler-generated'
    run(args.compiler, source, '--format', 'micropython', '--output', second)
    assert (destination / 'ui.json').read_bytes() == (second / 'ui.json').read_bytes()
    proto = args.output / 'fixture.pb'
    run(args.compiler, source, '--format', 'protobuf', '--output', proto)
    proto_destination = args.output / 'protobuf-generated'
    run(args.codegen, '--format=micropython', proto, proto_destination)
    proto_data = json.loads((proto_destination / 'ui.json').read_text())
    assert proto_data['screens'][0]['obj'][4]['img'] == records[4]['img']
    assert len(proto_data['variables'][0]['var']['bindings']) == 2
    # Rejected projects must not create the destination or partially copy assets.
    with tempfile.TemporaryDirectory(prefix='rejected-', dir=args.output) as temporary:
        temporary = Path(temporary)
        tiled = copy.deepcopy(project)
        tiled['screens'][0]['obj'][5]['barcode']['tiled'] = True
        tiled_source = temporary / 'tiled.json'
        tiled_source.write_text(json.dumps(tiled), encoding='utf-8')
        # Native tests consume this separately so a one-row TILE payload is
        # rendered using the shipped runtime, after this process has exited.
        tiled_destination = args.output / 'tiled-generated'
        run(args.codegen, '--format', 'micropython', tiled_source, tiled_destination)
        tiled_record = json.loads((tiled_destination / 'ui.json').read_text())['screens'][0]['obj'][5]
        assert tiled_record['img']['width'] == 198 and tiled_record['img']['height'] == 1
        assert tiled_record['img']['innerAlign'] == 12  # Native TILE, preserved from the encoder.
        arabic = copy.deepcopy(project)
        arabic['screens'][0]['obj'].append({'oid': 9, 'par': -1, 'base': {}, 'keyboard': {'mode': 8}})
        arabic_source = temporary / 'arabic.json'
        arabic_source.write_text(json.dumps(arabic), encoding='utf-8')
        arabic_destination = temporary / 'arabic'
        run(args.codegen, '--micropython', arabic_source, arabic_destination)
        assert '#define LV_USE_ARABIC_PERSIAN_CHARS 1' in (arabic_destination / 'lv_conf.defaults.h').read_text()
        cases = []
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'][2]['par'] = 9999
        cases.append((bad, 'parent'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'].append({'oid': 9, 'par': -1, 'base': {}, 'advancedKeyboard': {}})
        cases.append((bad, 'Advanced Keyboard'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'][0]['base']['flags'] = {'event_trickle': True}
        cases.append((bad, 'flag'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'].append({'oid': 9, 'par': -1, 'base': {}, 'spinner': {}})
        cases.append((bad, 'lv.spinner'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'][0]['base']['accessibility'] = {'tabIndex': 2}
        cases.append((bad, 'tabIndex'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'][5]['base']['coords']['width'] = 536871002
        cases.append((bad, 'pixel dimensions'))
        bad = copy.deepcopy(project)
        bad['screens'][0]['obj'][6]['img']['innerAlign'] = 14
        cases.append((bad, 'Contain-downscale'))
        for mode in (-1, 9, True, 1.5, 4294967295):
            bad = copy.deepcopy(project)
            bad['screens'][0]['obj'].append({'oid': 9, 'par': -1, 'base': {}, 'keyboard': {'mode': mode}})
            cases.append((bad, 'Keyboard mode'))
        for mode in (4, 5, 6, 7):
            bad = copy.deepcopy(project)
            bad['screens'][0]['obj'].append({'oid': 9, 'par': -1, 'base': {}, 'keyboard': {'mode': mode}})
            cases.append((bad, 'Keyboard user mode'))
        for index, (document, expected) in enumerate(cases):
            bad_source = temporary / ('bad-%d.json' % index)
            bad_source.write_text(json.dumps(document), encoding='utf-8')
            bad_destination = temporary / ('must-not-exist-%d' % index)
            result = run(args.codegen, '--micropython', bad_source, bad_destination, ok=False)
            assert not bad_destination.exists()
            assert expected.lower() in result.stderr.lower(), result.stderr
        result = run(args.compiler, source, '--format', 'micropython', '--target', '9.6',
                     '--output', temporary / 'wrong-version', ok=False)
        assert '9.3' in result.stderr
    print('MicroPython native CLI, assets, static QR/barcode, PB input and no-write rejection checks passed')


if __name__ == '__main__':
    main()
