#!/usr/bin/env python3
"""Check an OmaCustos import file without changing the app's configuration."""

import argparse
import json
import sys
from pathlib import Path


def validate(document):
    """Return field-specific errors for the version-1 backup-set format."""
    errors = []

    def nonempty_string(value, path):
        if not isinstance(value, str) or not value.strip():
            errors.append(f"{path}: expected a nonempty string")

    def integer(value, path, minimum, maximum):
        if type(value) is not int or not minimum <= value <= maximum:
            errors.append(f"{path}: expected a whole number from {minimum} to {maximum}")

    if not isinstance(document, dict):
        return ["root: expected a JSON object"]
    if document.get("application") != "omacustos":
        errors.append('application: expected "omacustos"')
    if type(document.get("version")) is not int or document["version"] != 1:
        errors.append("version: expected the whole number 1")
    if "proton_binary" in document:
        nonempty_string(document["proton_binary"], "proton_binary")

    sets = document.get("sets")
    if not isinstance(sets, list):
        errors.append("sets: expected an array of backup-set objects")
        return errors

    ids = set()
    for index, backup in enumerate(sets):
        path = f"sets[{index}]"
        if not isinstance(backup, dict):
            errors.append(f"{path}: expected a backup-set object")
            continue
        for key in ("id", "name", "remote_root"):
            nonempty_string(backup.get(key), f"{path}.{key}")
        identity = backup.get("id")
        if isinstance(identity, str):
            if identity in ids:
                errors.append(f"{path}.id: duplicate id {identity!r}")
            ids.add(identity)

        sources = backup.get("source_directories")
        if not isinstance(sources, list) or not sources:
            errors.append(f"{path}.source_directories: expected a nonempty array of path strings")
        else:
            for source_index, source in enumerate(sources):
                nonempty_string(source, f"{path}.source_directories[{source_index}]")

        exclusions = backup.get("exclusions", [])
        if not isinstance(exclusions, list):
            errors.append(f"{path}.exclusions: expected an array of strings")
        else:
            for exclusion_index, exclusion in enumerate(exclusions):
                if not isinstance(exclusion, str):
                    errors.append(f"{path}.exclusions[{exclusion_index}]: expected a string")

        schedule = backup.get("schedule", {})
        if not isinstance(schedule, dict):
            errors.append(f"{path}.schedule: expected an object")
        else:
            frequency = schedule.get("frequency", "disabled")
            if frequency not in ("disabled", "daily", "weekly", "monthly"):
                errors.append(f"{path}.schedule.frequency: expected disabled, daily, weekly, or monthly")
            for key, default, minimum, maximum in (
                ("hour", 2, 0, 23),
                ("minute", 0, 0, 59),
                ("weekday", 1, 1, 7),
                ("day_of_month", 1, 1, 31),
            ):
                integer(schedule.get(key, default), f"{path}.schedule.{key}", minimum, maximum)

        integer(backup.get("retention", 3), f"{path}.retention", 1, 2147483647)
        if type(backup.get("only_on_ac_power", False)) is not bool:
            errors.append(f"{path}.only_on_ac_power: expected true or false")

    return errors


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"{value} is not a valid JSON number")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="backup-set JSON file to validate")
    args = parser.parse_args(argv)
    try:
        with args.file.open(encoding="utf-8") as source:
            document = json.load(
                source, object_pairs_hook=unique_object, parse_constant=reject_constant
            )
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        print(f"Invalid import file: {error}", file=sys.stderr)
        return 1

    errors = validate(document)
    if errors:
        print("Invalid import file:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1

    count = len(document["sets"])
    print(f"Valid import file: {count} backup set{'s' if count != 1 else ''}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
