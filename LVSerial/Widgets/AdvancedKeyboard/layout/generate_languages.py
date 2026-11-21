#!/usr/bin/env python3
"""Regenerate the native language tables from the retained layout definitions."""
import json
from pathlib import Path
from generate_layout import generate, write_header

HERE = Path(__file__).resolve().parent


def insert_key(text, index):
    return {"id": "accent_" + str(index), "label": text,
            "action": {"type": "insert", "text": text}}


def local_layout(locale, space, caption, lower, upper):
    definition = json.loads((HERE / "es-ES-qwerty.json").read_text(encoding="utf-8"))
    definition["id"] = locale + "-qwerty"
    for name, layout in definition["layouts"].items():
        for row in layout["rows"]:
            row["keys"] = [key for key in row["keys"] if key["label"] not in ("ñ", "Ñ")]
            for key in row["keys"]:
                if key["id"] == "space":
                    key["label"] = space
                if key["id"] == "accents":
                    key["label"] = caption
        if name in ("letters", "letters_shifted"):
            layout["rows"][1]["offset"] = 0.5
    for name, rows in (("accents", lower), ("accents_shifted", upper)):
        layout = definition["layouts"][name]
        for index, text in enumerate(rows):
            layout["rows"][index]["keys"] = [insert_key(char, index * 16 + n) for n, char in enumerate(text)]
            layout["rows"][index]["offset"] = (definition["width"] - len(text)) / 2
    return definition


def main():
    spanish = HERE / "es-ES-qwerty.json"
    definition = json.loads(spanish.read_text(encoding="utf-8"))
    definition["id"] = "es-ES-qwerty"
    spanish.write_text(json.dumps(definition, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for locale, space, caption, lower, upper in (
        ("it-IT", "spazio", "àèé", ("àèéìòù", "áíóúî"), ("ÀÈÉÌÒÙ", "ÁÍÓÚÎ")),
        ("pt-PT", "espaço", "áâã", ("áâãàéê", "íóôõúç"), ("ÁÂÃÀÉÊ", "ÍÓÔÕÚÇ")),
    ):
        path = HERE / (locale + "-qwerty.json")
        path.write_text(json.dumps(local_layout(locale, space, caption, lower, upper), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for locale, shape in (("en-US", "qwerty"), ("es-ES", "qwerty"), ("fr-FR", "azerty"),
                          ("de-DE", "qwertz"), ("it-IT", "qwerty"), ("pt-PT", "qwerty")):
        name = locale + "-" + shape
        definition = json.loads((HERE / (name + ".json")).read_text(encoding="utf-8"))
        symbol = name.replace("-", "_").lower()
        data = generate(definition).replace(b"en_us_qwerty", symbol.encode("ascii"))
        write_header(HERE.parent / (symbol + ".generated.h"), data)


if __name__ == "__main__":
    main()
