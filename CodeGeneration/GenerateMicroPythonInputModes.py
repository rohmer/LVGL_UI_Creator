"""Generate MicroPython case tables from the shared portable keyboard helper."""
import argparse
from pathlib import Path
import re
import importlib.util


def generate(source):
    text = Path(source).read_text(encoding="utf-8")
    version = re.search(r"Unicode ([\d.]+)", text).group(1)
    lines = ['"""Generated Unicode ' + version + ' input mappings; do not edit the tables.',
             'Run CodeGeneration/GenerateMicroPythonInputModes.py after updating UnicodeCase.inc.',
             '"""', '']
    for direction in ("Upper", "Lower"):
        body = re.search(r"k" + direction + r"Ranges\[\] = \{(.*?)\n\};", text, re.S).group(1)
        lines.append("_" + direction.upper() + "_RANGES = (")
        for first, last, delta, step in re.findall(r"\{(0x[0-9A-F]+), (0x[0-9A-F]+), (-?\d+), (\d+)\}", body):
            lines.append("    (" + ", ".join((first, last, delta, step)) + "),")
        lines += [")", ""]
        body = re.search(r"k" + direction + r"Expansions\[\] = \{(.*?)\n\};", text, re.S).group(1)
        lines.append("_" + direction.upper() + "_EXPANSIONS = {")
        for scalar, values, count in re.findall(r"\{(0x[0-9A-F]+), \{([^}]+)\}, (\d+)\}", body):
            mapped = [value.strip() for value in values.split(",")][:int(count)]
            lines.append("    " + scalar + ": (" + ", ".join(mapped) + ",),")
        lines += ["}", ""]
    lines += ['''def _case(codepoint, ranges, expansions):
    expansion = expansions.get(codepoint)
    if expansion is not None:
        return "".join(chr(value) for value in expansion)
    low, high = 0, len(ranges)
    while low < high:
        middle = (low + high) // 2
        if ranges[middle][0] <= codepoint:
            low = middle + 1
        else:
            high = middle
    if low:
        first, last, delta, step = ranges[low - 1]
        if codepoint <= last and (codepoint - first) % step == 0:
            codepoint += delta
    return chr(codepoint)


def normalize(mode, text, decimal_seen=False):
    if mode == "alphanumeric":
        return text
    if mode == "numeric":
        result = []
        for character in text:
            if "0" <= character <= "9":
                result.append(character)
            elif character == "." and not decimal_seen:
                decimal_seen = True
                result.append(character)
        return "".join(result)
    if mode == "uppercase":
        ranges, expansions = _UPPER_RANGES, _UPPER_EXPANSIONS
    elif mode == "lowercase":
        ranges, expansions = _LOWER_RANGES, _LOWER_EXPANSIONS
    else:
        raise ValueError("Invalid text input mode")
    return "".join(_case(ord(character), ranges, expansions) for character in text)
''']
    return "\n".join(lines)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--advanced-keyboard-root", type=Path)
    args = parser.parse_args()
    output = root / "CodeGeneration/MicroPython/uicreator_input_modes.py"
    resolver_path = root / "Integrations/AdvancedKeyboard/dependency.py"
    spec = importlib.util.spec_from_file_location("input_mode_keyboard_dependency", resolver_path)
    dependency = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(dependency)
    keyboard = dependency.resolve_root(root, explicit=args.advanced_keyboard_root)
    content = generate(keyboard / "src/UnicodeCase.inc")
    if args.check:
        if not output.is_file() or output.read_text(encoding="utf-8") != content:
            raise SystemExit("MicroPython Unicode input mappings need regeneration")
    else:
        output.write_text(content, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
