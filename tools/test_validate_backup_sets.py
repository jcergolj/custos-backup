import copy
import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from validate_backup_sets import main, validate


class ValidateBackupSetsTest(unittest.TestCase):
    def setUp(self):
        template = Path(__file__).resolve().parents[1] / "backup-sets.template.json"
        self.document = json.loads(template.read_text(encoding="utf-8"))

    def test_template_and_importer_defaults_are_valid(self):
        self.assertEqual(validate(self.document), [])
        minimal = {
            "application": "omacustos",
            "version": 1,
            "sets": [{
                "id": "documents",
                "name": "Documents",
                "remote_root": "/my-files/backups",
                "source_directories": ["/home/alex/Documents"],
            }],
        }
        self.assertEqual(validate(minimal), [])
        minimal["sets"] = []
        self.assertEqual(validate(minimal), [])

    def test_invalid_envelope(self):
        for document, field in (
            ([], "root"),
            ({"sets": []}, "application"),
            ({"application": "omacustos", "version": True, "sets": []}, "version"),
            ({"application": "omacustos", "version": 2, "sets": []}, "version"),
            ({"application": "omacustos", "version": 1, "sets": {}}, "sets"),
        ):
            with self.subTest(document=document):
                self.assertTrue(any(error.startswith(field + ":") for error in validate(document)))

    def test_duplicate_ids(self):
        self.document["sets"].append(copy.deepcopy(self.document["sets"][0]))
        self.assertTrue(any("sets[1].id: duplicate" in error for error in validate(self.document)))

    def test_invalid_fields_have_specific_errors(self):
        cases = [
            (("id",), " "),
            (("name",), None),
            (("remote_root",), ""),
            (("source_directories",), []),
            (("source_directories",), [42]),
            (("exclusions",), "node_modules"),
            (("exclusions",), [None]),
            (("schedule",), []),
            (("schedule", "frequency"), "hourly"),
            (("schedule", "frequency"), {}),
            (("schedule", "hour"), 24),
            (("schedule", "minute"), -1),
            (("schedule", "weekday"), 0),
            (("schedule", "day_of_month"), 32),
            (("schedule", "hour"), "2"),
            (("schedule", "hour"), True),
            (("schedule", "hour"), 2.5),
            (("retention",), 0),
            (("retention",), 2147483648),
            (("only_on_ac_power",), "false"),
        ]
        for keys, value in cases:
            with self.subTest(keys=keys, value=value):
                document = copy.deepcopy(self.document)
                target = document["sets"][0]
                for key in keys[:-1]:
                    target = target[key]
                target[keys[-1]] = value
                field = "sets[0]." + ".".join(keys)
                self.assertTrue(any(error.startswith(field) for error in validate(document)))

    def test_cli_accepts_template_and_reports_invalid_files(self):
        cases = [
            (json.dumps(self.document), 0, "Valid import file: 1 backup set."),
            ('{"application":', 1, "Invalid import file"),
            ('{"sets": [], "sets": []}', 1, "duplicate JSON key"),
            ('{"version": NaN}', 1, "not a valid JSON number"),
            ('{"sets": []}', 1, 'application: expected "omacustos"'),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "backup sets.json"
            for contents, exit_code, message in cases:
                with self.subTest(contents=contents):
                    path.write_text(contents, encoding="utf-8")
                    output, errors = io.StringIO(), io.StringIO()
                    with redirect_stdout(output), redirect_stderr(errors):
                        self.assertEqual(main([str(path)]), exit_code)
                    self.assertIn(message, output.getvalue() + errors.getvalue())
            with redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(main([str(Path(directory) / "missing.json")]), 1)
            self.assertIn("Invalid import file", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
