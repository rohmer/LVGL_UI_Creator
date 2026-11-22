"""Create the same representative project for before/after C export measurements."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
screens, ids = [], []
for screen in range(2):
    objects = []
    for index in range(250):
        oid = index * 17 + 3
        objects.append({"oid": oid, "par": -1,
                        "base": {"coords": {"x": index % 20 * 38, "y": index // 20 * 24}},
                        "text": f"Fixed telemetry caption {screen}/{index:03d}: ready for device operation"})
        ids.append((screen, oid))
    for index in range(50):
        oid = 10000 + index * 17
        objects.append({"oid": oid, "par": -1, "base": {},
                        "button": {"text": f"Action {index:02d}"}})
        ids.append((screen, oid))
    objects.append({"oid": 20000, "par": -1, "base": {},
                    "dropdown": {"options": "First option\nSecond option\nThird option"}})
    ids.append((screen, 20000))
    screens.append({"name": f"Screen {screen}", "canvas": {"width": 800, "height": 480}, "obj": objects})
project = {"formatVersion": 2, "activeScreen": 0, "screens": screens,
           "variables": [{"var": {"name": "sample", "dataType": 1, "default": 0,
                                  "history": 1024, "code": {"cType": "uint8_t"}, "bindings": []}}]}
(args.output / "project.json").write_text(json.dumps(project, indent=2), encoding="utf-8")
(args.output / "fixture_ids.h").write_text(
    "#pragma once\nstatic const struct { size_t screen; int32_t id; } fixture_ids[] = {\n" +
    "".join(f"    {{{screen}, {oid}}},\n" for screen, oid in ids) + "};\n", encoding="utf-8")
print(args.output / "project.json")
