#!/usr/bin/env python3
"""Synthetic coverage checks for the RE evidence audit (no disc data)."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("steal_audit", Path(__file__).resolve().parents[1] / "tools/steal_capture_audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class AuditTest(unittest.TestCase):
    def run_trace(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.tsv"
            path.write_text(text)
            return audit.audit(path)["counts"]

    def test_missing_animation_is_not_eligible(self):
        c = self.run_trace("# rr-oracle 1\nframe\t0\ntick\t1\nrider.4.raw_1d4\t1\nend\n")
        self.assertEqual(c["missing_anim_frame"], 1)
        self.assertEqual(c.get("complete_weapon_animation_snapshots", 0), 0)
        self.assertEqual(c.get("armed_early_frame_snapshots", 0), 0)

    def test_signed_frame_and_deadline(self):
        c = self.run_trace("# rr-oracle 1\nframe\t0\ntick\t1\nrider.9.anim\t12\nrider.9.anim_frame\t255\nrider.9.raw_1c8\t1\nrider.9.raw_1d4\t1\nrider.9.raw_1cc\t301\nend\n")
        self.assertEqual(c["complete_weapon_animation_snapshots"], 1)
        self.assertEqual(c["armed_early_frame_snapshots"], 1)
        self.assertEqual(c["nonzero_steal_deadline_snapshots"], 1)

    def test_rows_are_not_carried_between_frames(self):
        c = self.run_trace("# rr-oracle 1\nframe\t0\ntick\t1\nrider.9.raw_1cc\t301\nframe\t1\ntick\t2\nrider.2.raw_1cc\t0\nend\n")
        self.assertEqual(c["frames"], 2)
        self.assertEqual(c["rider_snapshots"], 2)
        self.assertEqual(c["nonzero_steal_deadline_snapshots"], 1)

    def test_truncated(self):
        with self.assertRaises(ValueError):
            self.run_trace("# rr-oracle 1\nframe\t0\ntick\t1\n")

    def test_tick_gap(self):
        with self.assertRaises(ValueError):
            self.run_trace("# rr-oracle 1\nframe\t0\ntick\t1\nframe\t1\ntick\t3\nend\n")


if __name__ == "__main__":
    unittest.main()
