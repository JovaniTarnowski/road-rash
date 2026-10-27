#!/usr/bin/env python3
"""Validate measured original steal fixtures, plus synthetic parser failures.

python3 tests/steal_fixture_test.py --trace /absolute/path/to/steal.tsv
Without --trace only the four synthetic format/guard checks run.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from steal_fixture_capture import REFERENCE_SHA, read_fixture
from agent_common import sha256

SUCCESS = {
    "success-club", "success-chain", "victim-frame-one", "victim-frame-negative",
    "victim-deadline-past", "attacker-deadline-future", "inventory-differs",
    "attacker-frame-two", "other-descriptor-bit", "position-inside", "lateral-inside",
    "height-inside", "unsigned-tick-high",
}
BLOCKED = {
    "victim-frame-two", "victim-frame-127", "victim-deadline-equal", "victim-deadline-high-bit",
    "attacker-armed", "victim-fists", "victim-kick", "victim-not-attacking",
    "victim-animation-nine", "no-hit-event", "position-boundary", "lateral-boundary",
    "side-mismatch", "height-boundary", "no-scan-slice",
}
FAILED_ROLLS = {"roll-zero", "roll-one", "roll-two"}


def verify_original(path):
    receipt = json.loads((path.parent / "receipt.json").read_text())
    assert receipt["exe_sha256"] == REFERENCE_SHA, "receipt reference EXE mismatch"
    assert receipt["trace_sha256"] == sha256(path), "trace changed since capture"
    assert receipt["source_sha256"] == sha256(Path(__file__).resolve().parents[1] / "oracle/steal_fixture.c"), "fixture source changed since capture"
    assert receipt["dll_sha256"] == sha256(path.parent / "AWEMAN32.DLL"), "fixture DLL changed since capture"
    assert receipt["exit_code"] == 0 and receipt["cases"] == 31, "capture did not complete"
    rows = read_fixture(path)
    if set(rows) != SUCCESS | BLOCKED | FAILED_ROLLS:
        raise AssertionError("fixture case set differs from the 31 measured hypotheses")
    for name, row in rows.items():
        expected_rolls = int(name in SUCCESS | FAILED_ROLLS)
        assert row["rolls"] == expected_rolls, (name, "roll count", row)
        if name in SUCCESS:
            weapon = 2 if name in {"success-chain", "inventory-differs"} else 1
            assert (row["self_inventory"], row["self_active"]) == (weapon, weapon), (name, row)
            assert (row["victim_inventory"], row["victim_active"]) == (0, 0), (name, row)
            tick = 0x80000001 if name == "unsigned-tick-high" else 100
            assert row["self_deadline"] == tick + 300, (name, row)
            assert row["self_anim"] == row["expected_steal_anim"], (name, row)
            assert (row["victim_anim"], row["self_queue"], row["victim_queue"]) == (2, 2, 2), (name, row)
        else:
            own = 1 if name == "attacker-armed" else 0
            active = 0 if name == "victim-fists" else 3 if name == "victim-kick" else 1
            assert (row["self_inventory"], row["self_active"]) == (own, own), (name, row)
            assert (row["victim_inventory"], row["victim_active"]) == (1, active), (name, row)
            assert (row["self_deadline"], row["self_queue"], row["victim_queue"]) == (0, -1, -1), (name, row)
        deadline = {"victim-deadline-equal": 100, "victim-deadline-past": 99,
                    "victim-deadline-high-bit": 0x80000000, "unsigned-tick-high": 0x80000000}.get(name, 0)
        assert row["victim_deadline"] == deadline, (name, "victim cooldown changed", row)
    print(f"original resolver: {len(SUCCESS)} transfers, {len(BLOCKED)} gate rejections, "
          f"{len(FAILED_ROLLS)} rejected rolls; all 31 cases pass")


class ParserTest(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.tsv"
            path.write_text(text)
            return read_fixture(path)

    def test_missing_completion(self):
        with self.assertRaises(ValueError):
            self.parse("# rr-steal-fixture 1\ncase\trolls\na\t1\n")

    def test_wrong_columns(self):
        with self.assertRaises(ValueError):
            self.parse("# rr-steal-fixture 1\ncase\trolls\na\t1\nend\t1\n")

    def test_duplicate_rows(self):
        header = ("case\trolls\tself_inventory\tself_active\tvictim_inventory\tvictim_active\t"
                  "self_deadline\tvictim_deadline\tself_anim\tvictim_anim\tself_queue\tvictim_queue\t"
                  "expected_steal_anim\n")
        row = "duplicate" + "\t0" * 12 + "\n"
        with self.assertRaises(ValueError):
            self.parse("# rr-steal-fixture 1\n" + header + row * 2 + "end\t2\n")

    def test_foreign_executable_is_rejected_before_prefix_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / "foreign.exe"
            exe.write_bytes(b"synthetic foreign executable")
            prefix = root / "private-prefix"
            result = subprocess.run([
                sys.executable, str(Path(__file__).resolve().parents[1] / "tools/steal_fixture_capture.py"),
                "--exe", str(exe), "--prefix", str(prefix),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("reference EXE hash mismatch", result.stderr)
            self.assertFalse(prefix.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ParserTest)
    result = unittest.TextTestRunner().run(suite)
    if not result.wasSuccessful():
        raise SystemExit(1)
    if args.trace:
        verify_original(args.trace)
