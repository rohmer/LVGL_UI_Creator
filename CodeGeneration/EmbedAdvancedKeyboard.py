"""Embed the portable keyboard support so C export works in the packaged editor."""
from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:])
lines = ['#pragma once', '#include <map>', '#include <string>', '#include <utility>',
         'namespace CGeneration {',
         'inline std::map<std::string, std::string> AdvancedKeyboardSources() {',
         'std::map<std::string, std::string> files; std::string data;']
for path in sorted(source.rglob('*')):
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
    name = path.relative_to(source).as_posix()
    lines.append('files.emplace("advanced_keyboard/' + name + '", std::move(data));')
lines += ['return files;', '}', '}']
result = '\n'.join(lines) + '\n'
destination.parent.mkdir(parents=True, exist_ok=True)
if not destination.exists() or destination.read_text(encoding='utf-8') != result:
    destination.write_text(result, encoding='utf-8', newline='\n')
