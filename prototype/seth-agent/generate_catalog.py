#!/usr/bin/env python3
"""Compile original, versioned Seth lessons into static C flash-ready records.

Standalone generator only: no ECU connection, command execution, or INI parsing.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT.parent / "design" / "education" / "lesson-catalog.v1.json"
CHAPTERS = [
    "configuration", "trigger", "baseline", "references", "starting",
    "idle", "steady_fuel", "steady_spark", "power", "confirmation",
]
ID = re.compile(r"^[a-z][a-z0-9_]+$")


def cstr(value):
    if not isinstance(value, str):
        raise ValueError("Non-text lesson field")
    if "\x00" in value:
        raise ValueError("Embedded NUL is not supported")
    if len(value.encode("utf-8")) > 1600:
        raise ValueError("Lesson field exceeds flash/packet budget")
    return json.dumps(value, ensure_ascii=False)


def build(source: Path, destination: Path):
    catalog = json.loads(source.read_text(encoding="utf-8"))
    if catalog.get("schema") != "brickos.guide.education.catalog.v1":
        raise ValueError("Unexpected lesson catalogue schema")
    if catalog.get("chapter_order") != CHAPTERS:
        raise ValueError("Unexpected chapter order")
    lessons = catalog.get("lessons")
    if not isinstance(lessons, list) or not lessons or len(lessons) > 48:
        raise ValueError("Lesson count exceeds fixed education capacity")
    ids = {x.get("id") for x in lessons}
    if len(ids) != len(lessons) or not all(isinstance(x, str) and ID.fullmatch(x) for x in ids):
        raise ValueError("Invalid or duplicate lesson identifier")
    lines = [
        "/* GENERATED: build-only static text catalogue. Do not edit. */",
        '#include "seth_agent.h"',
        "const char *const seth_stage_ids[SETH_STAGE_COUNT] = {",
        *[f"    {cstr(ch)}," for ch in CHAPTERS],
        "};",
        "const seth_lesson_t seth_lessons[] = {",
    ]
    for lesson in lessons:
        if lesson.get("chapter") not in CHAPTERS:
            raise ValueError("Unknown chapter")
        if any(dep not in ids for dep in lesson.get("prerequisites", [])):
            raise ValueError("Unknown prerequisite")
        if lesson.get("no_calibration_actions") is not True:
            raise ValueError("Unsafe lesson flag")
        vs = lesson["variants"]
        check = lesson["check"]
        if check.get("kind") != "concept_only":
            raise ValueError("Quiz cannot certify hardware")
        options = check["options"]
        if len(options) != 3 or check["correct_index"] not in (0, 1, 2):
            raise ValueError("Invalid check options")
        rotation = sum(lesson["id"].encode("ascii")) % 3
        rotated = options[rotation:] + options[:rotation]
        answer = (check["correct_index"] - rotation) % 3
        fingerprint = hashlib.sha256(
            json.dumps(lesson, sort_keys=True, ensure_ascii=False).encode("utf-8")
        ).digest()
        version = int.from_bytes(fingerprint[:4], "little") or 1
        fields = [
            cstr(lesson["id"]), cstr(lesson["chapter"]), cstr(lesson["learning_objective"]),
            *(cstr(vs[name]) for name in ("quick", "guided", "lesson", "technical")),
            cstr(lesson["if_confused"]["alternate"]),
            cstr(check["prompt"]), cstr(check["explanation"]),
        ]
        lines += [
            "    {",
            *(f"      {field}," for field in fields),
            "      {" + ", ".join(cstr(v) for v in rotated) + "},",
            f"      {answer}u, {version}u",
            "    },",
        ]
    lines += [
        "};",
        "const size_t seth_lesson_count = sizeof(seth_lessons)/sizeof(seth_lessons[0]);",
        "",
    ]
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines), encoding="utf-8")
    return len(lessons)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--output", type=Path, required=True)
    opts = parser.parse_args()
    print(f"Generated {build(opts.source, opts.output)} lessons into {opts.output}")
