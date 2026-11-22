"""Exercise XML and Protobuf through the desktop viewer and its runtime DLL."""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--viewer', required=True, type=Path)
    parser.add_argument('--fixture', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software')
    results = []
    for extension in ('pb', 'xml'):
        snapshot = args.output / ('viewer-' + extension + '.bmp')
        command = [str(args.viewer), str(args.fixture / ('project.' + extension)),
                   '--frames', '6', '--screenshot', str(snapshot)]
        process = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=30)
        assert process.returncode == 0, process.stdout + process.stderr
        image = snapshot.read_bytes()
        assert len(image) > 54 and image[:2] == b'BM', 'Viewer did not write a bitmap'
        width, height = struct.unpack_from('<ii', image, 18)
        assert (width, abs(height)) == (640, 400), 'Viewer lost the authored display dimensions'
        offset = struct.unpack_from('<I', image, 10)[0]
        assert len(set(image[offset:])) > 8, 'Viewer produced an empty framebuffer'
        results.append({'format': extension, 'status': 'passed', 'width': width, 'height': abs(height)})
    (args.output / 'report.json').write_text(json.dumps({'status': 'passed', 'formats': results}, indent=2) + '\n')
    print('XML and Protobuf rendered through the desktop viewer/DLL with authored dimensions')


if __name__ == '__main__':
    main()
