#!/usr/bin/env python3
"""Audit steal evidence in original TSVs; post-tick snapshots are not hit-call probes."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

RIDER = re.compile(r"rider\.(\d+)\.(.+)")
FIELDS = ("anim", "anim_frame", "raw_1c8", "raw_1cc", "raw_1d4")


def audit(path):
    counts = Counter({key: 0 for key in ("frames", "rider_snapshots", "complete_weapon_animation_snapshots",
        "nonzero_steal_deadline_snapshots", "armed_attack_weapon_snapshots",
        "armed_with_frame_snapshots", "armed_early_frame_snapshots", "rows_without_weapon_window")})
    rows = {}
    tick = None
    previous_tick = None
    complete = False
    digest = hashlib.sha256()
    magic = False

    def finish():
        nonlocal previous_tick
        if tick is None:
            raise ValueError("frame without tick")
        if previous_tick is not None and tick != previous_tick + 1:
            raise ValueError("noncontiguous ticks")
        previous_tick = tick
        counts["frames"] += 1
        for row in rows.values():
            if not any(field.startswith("raw_") for field in row):
                counts["rows_without_weapon_window"] += 1
                continue
            counts["rider_snapshots"] += 1
            missing = [field for field in FIELDS if field not in row]
            for field in missing:
                counts["missing_" + field] += 1
            if not missing:
                counts["complete_weapon_animation_snapshots"] += 1
            if row.get("raw_1cc", 0) != 0:
                counts["nonzero_steal_deadline_snapshots"] += 1
            if row.get("raw_1d4") in (1, 2):
                counts["armed_attack_weapon_snapshots"] += 1
                if "anim_frame" in row:
                    counts["armed_with_frame_snapshots"] += 1
                    # 0x4097d7..0x4097dc: signed byte <= 1, NOT unsigned.
                    frame = row["anim_frame"] & 255
                    frame = frame - 256 if frame >= 128 else frame
                    if frame <= 1:
                        counts["armed_early_frame_snapshots"] += 1

    with Path(path).open("rb") as source:
        for raw in source:
            digest.update(raw)
            line = raw.decode("utf-8").rstrip("\r\n")
            if line == "# rr-oracle 1":
                magic = True
            if line == "end":
                if tick is None or complete:
                    raise ValueError("unexpected end marker")
                finish()
                complete = True
                continue
            if "\t" not in line:
                continue
            key, value = line.split("\t", 1)
            if key == "frame":
                if complete:
                    raise ValueError("frame after end marker")
                if tick is not None:
                    finish()
                rows = {}
                tick = None
            elif key == "tick":
                tick = int(value)
            else:
                match = RIDER.fullmatch(key)
                if match:
                    row = rows.setdefault(int(match[1]), {})
                    if match[2] in FIELDS:
                        row[match[2]] = int(value)
    if not magic or not complete:
        raise ValueError("missing oracle header or completion marker")
    return {
        "path": str(path), "sha256": digest.hexdigest(), "counts": dict(sorted(counts.items())),
        "scope": "Post-tick snapshots only; no resolver-call, target, reach, descriptor or RNG-roll proof. "
                 "Missing animation fields cannot establish an eligible steal attempt. "
                 "Nonzero deadlines are steal-path evidence, not a count of distinct steals."
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps([audit(path) for path in args.traces], indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
