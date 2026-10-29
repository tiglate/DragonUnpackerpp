#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Fills the .ts files of translations/ from translations_data.py (the texts translated by hand) and checks them.

usage: apply_translations.py [--check]   (--check only reports: exit code 1 if something is missing or inconsistent)

Run `cmake --build app/build --target update_translations` first when the sources of the interface changed, then add the new
texts to translations_data.py and run this script."""
import os, re, sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import translations_data as data

HERE = os.path.dirname(os.path.abspath(__file__))
TS_DIR = os.path.join(HERE, "..", "translations")


def key(source):
    return source.replace("&", "")


def placeholders(text):
    return sorted(re.findall(r"%(?:[1-9]|[a-z])", text))


def process(language, check_only):
    path = os.path.join(TS_DIR, "dragonunpacker_%s.ts" % language)
    tree = ET.parse(path)
    problems, filled = [], 0
    table = None
    if language in data.LANGUAGES:
        index = data.LANGUAGES.index(language)
        table = lambda src: data.TRANSLATIONS.get(key(src), [None] * 10)[index]
    for message in tree.getroot().iter("message"):
        source = message.find("source").text or ""
        translation = message.find("translation")
        if translation is None or translation.get("type") == "obsolete":
            continue
        current = translation.text if translation.get("type") != "unfinished" else None
        if language == "pt_BR":
            if current is None:
                current = data.PT_BR.get(key(source))
        else:
            current = table(source)
        if not current:
            problems.append("missing: %r" % source)
            continue
        if placeholders(source) != placeholders(current):
            problems.append("placeholders differ: %r -> %r" % (source, current))
        if source.count("\n") != current.count("\n"):
            problems.append("lines differ: %r -> %r" % (source, current))
        if not check_only and (translation.text != current or translation.get("type")):
            translation.text = current
            translation.attrib.pop("type", None)
            filled += 1
    if not check_only:
        tree.write(path, encoding="utf-8", xml_declaration=True)
        # Qt writes the DOCTYPE and the entities in its own way: put it back as lupdate does
        text = open(path, encoding="utf-8").read()
        text = text.replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>', 1)
        open(path, "w", encoding="utf-8").write(text)
    return filled, problems


def main():
    check_only = "--check" in sys.argv
    failed = False
    for language in ["pt_BR"] + data.LANGUAGES:
        filled, problems = process(language, check_only)
        print("%-6s %s%s" % (language, "%d filled, " % filled if not check_only else "", "%d problem(s)" % len(problems)))
        for problem in problems:
            print("   ", problem)
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
