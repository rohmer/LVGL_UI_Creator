#!/usr/bin/env python3
"""Regenerate the native language tables from the retained layout definitions."""
import json
from pathlib import Path
from generate_layout import generate, write_header

HERE = Path(__file__).resolve().parent


# Curated Unicode alternatives, grouped by letter/notation family rather than
# visual resemblance. Combining sequences are intentional (Indic nukta, Greek
# capital dialytika + tonos); a variant may contain more than one scalar.
LATIN_FAMILIES = (
    "aàáâãäåæ", "cç", "eèéêë", "iìíîï", "nñ", "oòóôõöøœ",
    "uùúûü", "yýÿ",
)
SYMBOL_FAMILIES = (
    ".…·•", "-–—−", "'’‘", '\"“”„«»', "!¡", "?¿", "%‰", "+±",
    "=≠≈", "<≤‹«", ">≥›»", "*×·", "/\\÷", "$¢€£¥₹₽₩", "©®™",
    "([\u007b", ")]\u007d",
)
DIGIT_VARIANTS = (
    "⁰₀", "¹₁½¼", "²₂", "³₃¾", "⁴₄", "⁵₅", "⁶₆", "⁷₇", "⁸₈", "⁹₉",
)
PINYIN_FAMILIES = (
    "aàáâãäåæāǎ", "eèéêëēě", "iìíîïīǐ", "oòóôõöøœōǒ", "uùúûüūǔǖǘǚǜ",
)
GREEK_FAMILIES = (
    "αά", "εέ", "ηή", "ιίϊΐ", "οό", "υύϋΰ", "ωώ", "σς",
    "ΑΆ", "ΕΈ", "ΗΉ", ("Ι", "Ί", "Ϊ", "Ϊ́"), "ΟΌ",
    ("Υ", "Ύ", "Ϋ", "Ϋ́"), "ΩΏ",
)
ARABIC_FAMILIES = ("اآأإٱ", "وؤ", "يىئ", "هة", "كک")
URDU_FAMILIES = ("اآأإٱ", "وؤ", "یيىئېےۓ", "ہھۃۀ", "نں", "کك", "هہ")
HINDI_FAMILIES = (
    "अआ", "इई", "उऊ", "ऎएऐ", "ऑओऔ", "ऋॠ", "िी", "ुू", "ॆेै", "ॉोौ", "ँं",
    ("क", "क़"), ("ख", "ख़"), ("ग", "ग़"), ("ज", "ज़"), ("ड", "ड़"),
    ("ढ", "ढ़"), ("फ", "फ़"), ("य", "य़"), ("र", "ऱ"), ("न", "ऩ"),
    ("ळ", "ऴ"),
)
BENGALI_FAMILIES = (
    "অআ", "ইঈ", "উঊ", "এঐ", "ওঔ", "ঋৠ", "িী", "ুূ", "েৈ", "োৌ", "ঁং",
    ("ড", "ড়"), ("ঢ", "ঢ়"), ("য", "য়"),
)


def apply_variants(definition):
    """Add curated long-press choices without changing any key's actions."""
    locale = definition["id"].split("-", 1)[0]
    alternatives = {}

    def family(values):
        values = list(values)
        for value in values:
            choices = alternatives.setdefault(value, [])
            for other in values:
                if other != value and other not in choices:
                    choices.append(other)

    for group in LATIN_FAMILIES:
        family(group)
        family(group.upper())
    family("sß")
    family("Sẞ")
    for group in SYMBOL_FAMILIES:
        family(group)
    for digit, choices in enumerate(DIGIT_VARIANTS):
        values = str(digit) + choices
        if locale in ("ar", "ur", "hi", "bn"):
            zero = {"ar": 0x0660, "ur": 0x06f0, "hi": 0x0966, "bn": 0x09e6}[locale]
            values += chr(zero + digit)
        family(values)
    if locale == "zh":
        for group in PINYIN_FAMILIES:
            family(group)
            family(group.upper())
    for groups, languages in (
        (GREEK_FAMILIES, ("el",)), (("её", "ЕЁ", "ий", "ИЙ"), ("ru",)),
        (ARABIC_FAMILIES, ("ar",)), (URDU_FAMILIES, ("ur",)),
        (HINDI_FAMILIES, ("hi",)), (BENGALI_FAMILIES, ("bn",)),
    ):
        if locale in languages:
            for group in groups:
                family(group)
    for layout in definition["layouts"].values():
        for row in layout["rows"]:
            for key in row["keys"]:
                # An explicit JSON list (including []) overrides these defaults.
                if key["action"]["type"] == "insert" and "variants" not in key:
                    variants = alternatives.get(key["action"]["text"], [])
                    if variants:
                        key["variants"] = variants
    return definition


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
    return apply_variants(definition)


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
                          ("de-DE", "qwertz"), ("it-IT", "qwerty"), ("pt-PT", "qwerty"),
                          ("zh-CN", "pinyin"), ("ar-SA", "arabic"), ("hi-IN", "inscript"),
                          ("bn-BD", "inscript"), ("id-ID", "qwerty"), ("ur-PK", "phonetic"), ("ru-RU", "jcuken"),
                          ("el-GR", "qwerty")):
        name = locale + "-" + shape
        path = HERE / (name + ".json")
        definition = apply_variants(json.loads(path.read_text(encoding="utf-8")))
        symbol = name.replace("-", "_").lower()
        data = generate(definition).replace(b"en_us_qwerty", symbol.encode("ascii"))
        write_header(HERE.parent / (symbol + ".generated.h"), data)


if __name__ == "__main__":
    main()
