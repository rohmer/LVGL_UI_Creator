"""Embed the portable keyboard support so C export works in the packaged editor."""
from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:])
if not (source / "src/Keyboard.cpp").is_file() or not (source / "include/lv_advanced_keyboard.h").is_file():
    raise SystemExit("Initialize the pinned AdvancedKeyboard dependency before generating C export resources")
lines = ['#pragma once', '#include <map>', '#include <string>', '#include <utility>',
         'namespace CGeneration {',
         'inline std::map<std::string, std::string> AdvancedKeyboardSources() {',
         'std::map<std::string, std::string> files; std::string data;']
resources = []
for directory in (source / 'src', source / 'include'):
    for path in sorted(directory.rglob('*')):
        if path.name == 'BundledModels.cpp':
            # Exported projects own their selected trie blobs, not an offline
            # generated registry of the component's optional model catalog.
            continue
        relative = path.relative_to(directory)
        if directory.name == 'src' and (relative.parts[0] in ('include', 'espos_keyboard') or
                (len(relative.parts) == 1 and
                 (source / 'include' / relative.name).is_file())):
            # Arduino's src/ forwarding headers rely on the upstream directory
            # depth. Flat C exports use the real public include/ headers instead.
            continue
        name = relative.as_posix()
        resources.append((path, ('include/' if directory.name == 'include' else '') + name))
resources += [(source / name, name) for name in ('LICENSE', 'NOTICE.md')]
for path, name in resources:
    if not path.is_file() or not (path.suffix in ('.cpp', '.c', '.h', '.inc', '.md') or 'LICENSE' in path.name.upper()):
        continue
    content = path.read_text(encoding='utf-8-sig')
    pieces = []
    while content:
        end = min(len(content), 8000)
        if end < len(content):
            boundary = content.rfind('\n', 0, end)
            if boundary > 0:
                end = boundary + 1
        pieces.append(content[:end]); content = content[end:]
    lines.append('data.clear();')
    for piece in pieces:
        assert ')LVAK_RESOURCE"' not in piece
        lines.append('data += R"LVAK_RESOURCE(' + piece + ')LVAK_RESOURCE";')
    lines.append('files.emplace("advanced_keyboard/' + name + '", std::move(data));')
lines += ['return files;', '}', '}']
result = '\n'.join(lines) + '\n'
destination.parent.mkdir(parents=True, exist_ok=True)
if not destination.exists() or destination.read_text(encoding='utf-8') != result:
    destination.write_text(result, encoding='utf-8', newline='\n')
