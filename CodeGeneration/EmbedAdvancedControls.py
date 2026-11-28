"""Embed the maintained native advanced controls for offline C exports."""
from pathlib import Path
import sys

source, destination = map(Path, sys.argv[1:])
content = source.read_text(encoding="utf-8-sig")
lines = ["#pragma once", "#include <string>", "namespace CGeneration {",
         "inline std::string AdvancedControlsSource() {", "std::string result;"]
while content:
    end = min(len(content), 8000)
    if end < len(content):
        boundary = content.rfind("\n", 0, end)
        if boundary > 0:
            end = boundary + 1
    piece, content = content[:end], content[end:]
    assert ')UIADV_RESOURCE"' not in piece
    lines.append('result += R"UIADV_RESOURCE(' + piece + ')UIADV_RESOURCE";')
lines += ["return result;", "}", "}"]
result = "\n".join(lines) + "\n"
destination.parent.mkdir(parents=True, exist_ok=True)
if not destination.exists() or destination.read_text(encoding="utf-8") != result:
    destination.write_text(result, encoding="utf-8", newline="\n")
