"""Exercise the application's owned XML file format through both real CLIs.

ElementTree is an independent well-formedness/escaping check. Runtime parity
and typed-value losslessness are checked separately by UICreatorXmlTests.
"""
import argparse
import base64
import copy
import json
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def run(*arguments, success=True):
    result = subprocess.run([str(argument) for argument in arguments], capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=60)
    if success and result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    if not success and result.returncode == 0:
        raise AssertionError("Invalid XML input/export unexpectedly succeeded")
    if not success and not (result.stdout or result.stderr):
        raise AssertionError("Failed export did not report an error")
    return result


def value(element):
    text = element.text or ""
    if element.tag == "object":
        pairs = [(child.attrib["name"], value(child[0])) for child in element]
        assert len(dict(pairs)) == len(pairs), "Duplicate object properties"
        return dict(pairs)
    if element.tag == "array": return [value(child) for child in element]
    if element.tag == "string": return text
    if element.tag in ("integer", "unsigned"): return int(text)
    if element.tag == "real": return float(text)
    if element.tag == "boolean":
        assert text in ("true", "false")
        return text == "true"
    if element.tag == "null": return None
    if element.tag == "binary":
        assert element.attrib["encoding"] == "base64"
        return base64.b64decode(text, validate=True)
    raise AssertionError("Unknown typed XML node " + element.tag)


def document(path):
    root = ET.parse(path).getroot()
    assert root.tag == "uicreator", "This is not the application's owned XML format"
    assert root.attrib == {"format_version": "1", "kind": "project", "type": "project"}
    assert len(root) == 1 and root[0].tag == "object"
    return value(root[0])


def tree(directory):
    return {path.relative_to(directory).as_posix(): path.read_bytes()
            for path in directory.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--codegen", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture = args.fixture / "project.json"
    source = json.loads(fixture.read_text(encoding="utf-8"))

    # The new export is one self-contained file, not an XML project directory.
    first = args.output / "from-compiler.xml"
    second = args.output / "from-codegen.xml"
    run(args.compiler, fixture, "--format", "xml", "--output", first)
    run(args.codegen, "--format", "xml", fixture, second)
    assert first.is_file() and second.is_file()
    assert first.read_bytes() == second.read_bytes(), "CLI frontends generated different XML"
    decoded = document(first)
    assert len(decoded["screens"]) == 2 and len(decoded["variables"]) == 2
    assert decoded["screens"][0]["obj"][0]["oid"] == 1
    assert decoded["screens"][0]["obj"][6]["advancedKeyboard"]["trieId"] == decoded["tries"][0]["id"]
    assert decoded["fonts"] and decoded["images"] and decoded["tries"]
    assert decoded["variables"][1]["var"]["default"] == source["variables"][1]["var"]["default"]

    again = args.output / "regenerated.xml"
    run(args.compiler, first, "--format", "xml", "--output", again)
    assert first.read_bytes() == again.read_bytes(), "XML regeneration is not deterministic"

    # Protobuf and XML inputs must feed the same C and Protobuf pipelines.
    protobuf = args.output / "from-json.pb"
    converted = args.output / "from-xml.pb"
    run(args.compiler, fixture, "--format", "protobuf", "--output", protobuf)
    run(args.compiler, first, "--format", "protobuf", "--output", converted)
    assert protobuf.read_bytes() == converted.read_bytes()
    from_pb = args.output / "from-protobuf.xml"
    run(args.codegen, "--format=xml", args.fixture / "project.pb", from_pb)
    assert document(from_pb)["variables"][0]["var"]["name"] == "level"
    c_json, c_xml = args.output / "c-from-json", args.output / "c-from-xml"
    run(args.codegen, fixture, c_json)
    run(args.codegen, first, c_xml)
    assert tree(c_json) == tree(c_xml), "XML input changed generated C output"

    # Attribute/text normalization must not lose authored whitespace or Unicode.
    unicode_dir = args.output / "Unicode \u00e9 \u4e2d\u6587 source"
    unicode_dir.mkdir(exist_ok=True)
    escaped = copy.deepcopy(source)
    special = "A & B <tag> \"quote\" 'apostrophe'\n\t\rEspa\u00f1ol \u4e2d\u6587"
    escaped["screens"][1]["obj"].append({"oid": 22, "par": -1, "base": {}, "text": special})
    escaped["metadata & <\"\u00e9"] = {"literal": special, "empty": "", "null": None,
                                       "list": [True, False, -2, 1.25, []]}
    unicode_source = unicode_dir / "project \u00e9.json"
    unicode_source.write_text(json.dumps(escaped, ensure_ascii=False), encoding="utf-8")
    unicode_xml = unicode_dir / "display \u4e2d\u6587.xml"
    run(args.compiler, unicode_source, "--format", "xml", "--output", unicode_xml)
    unicode_data = document(unicode_xml)
    assert unicode_data["screens"][1]["obj"][-1]["text"] == special
    assert unicode_data["metadata & <\"\u00e9"] == escaped["metadata & <\"\u00e9"]
    # It must remain self-contained after the original JSON has gone away.
    unicode_source.unlink()
    run(args.compiler, unicode_xml, "--format", "protobuf", "--output", unicode_dir / "standalone.pb")

    # Failed conversions preserve both prior exports and the input file.
    with tempfile.TemporaryDirectory(prefix="rejected-", dir=args.output) as temporary:
        work = Path(temporary)
        malformed = [b"<project/>", b"<uicreator/>", b"<uicreator", b"<?xml version=\"1.0\"?><globals/>",
                     b"<!DOCTYPE uicreator [<!ENTITY external SYSTEM 'file:///not-read'>]><uicreator/>"]
        for index, data in enumerate(malformed):
            bad = work / ("invalid-%d.xml" % index)
            bad.write_bytes(data)
            for command in ("compiler", "codegen"):
                destination = work / (command + ".xml")
                sentinel = b"keep the previously exported project"
                destination.write_bytes(sentinel)
                if command == "compiler": run(args.compiler, bad, "--format", "xml", "--output", destination, success=False)
                else: run(args.codegen, "--format", "xml", bad, destination, success=False)
                assert destination.read_bytes() == sentinel and bad.read_bytes() == data
        bad_project = copy.deepcopy(source)
        bad_project["screens"][0]["obj"][0]["par"] = 999999
        bad = work / "bad-parent.json"
        bad.write_text(json.dumps(bad_project), encoding="utf-8")
        destination = work / "must-not-exist.xml"
        run(args.compiler, bad, "--format", "xml", "--output", destination, success=False)
        assert not destination.exists()
        assert not list(work.glob("*.uicreator-tmp*"))
    print("Owned XML CLI files, typed XML/UTF-8, self-contained assets, JSON/PB/C equivalence and failed-export preservation passed")


if __name__ == "__main__": main()
