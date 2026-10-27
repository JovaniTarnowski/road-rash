#!/usr/bin/env python3
"""Build/run the isolated original steal component oracle; never installs the live proxy."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid

from agent_common import ROOT, ResourceClaims, atomic_json, sha256

REFERENCE_SHA = "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2"


def read_fixture(path):
    lines = Path(path).read_text().splitlines()
    if not lines or lines[0] != "# rr-steal-fixture 1":
        raise ValueError("invalid fixture header")
    if len(lines) < 4 or not lines[-1].startswith("end\t"):
        raise ValueError("incomplete fixture")
    header = lines[1].split("\t")
    if header != ["case", "rolls", "self_inventory", "self_active", "victim_inventory",
                  "victim_active", "self_deadline", "victim_deadline", "self_anim",
                  "victim_anim", "self_queue", "victim_queue", "expected_steal_anim"]:
        raise ValueError("invalid fixture columns")
    rows = {}
    for line in lines[2:-1]:
        values = line.split("\t")
        if len(values) != len(header) or values[0] in rows:
            raise ValueError("invalid or duplicate fixture row")
        rows[values[0]] = dict(zip(header[1:], map(int, values[1:])))
    if int(lines[-1].split("\t")[1]) != len(rows):
        raise ValueError("fixture count mismatch")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--prefix", required=True, type=Path, help="dedicated private Wine prefix")
    parser.add_argument("--clone-prefix", type=Path, help="APFS-clone an existing prefix into a NEW private prefix")
    parser.add_argument("--wine", type=Path, default=Path.home() / "Applications/Wine Staging.app/Contents/Resources/wine/bin/wine")
    parser.add_argument("--output", type=Path, default=ROOT / "analysis/oracle/steal-fixture")
    args = parser.parse_args()
    exe, prefix, output = args.exe.resolve(), args.prefix.resolve(), args.output.resolve()
    if sha256(exe) != REFERENCE_SHA:
        parser.error("reference EXE hash mismatch")
    if args.clone_prefix and (prefix.exists() or prefix == args.clone_prefix.resolve()):
        parser.error("clone destination must be a new private prefix")
    resources = [prefix, output]
    if args.clone_prefix:
        resources.append(args.clone_prefix.resolve())
    with ResourceClaims().hold("steal-component-fixture", resources):
        if args.clone_prefix:
            # APFS clone: do not copy 9 GB or modify the existing prefix.
            subprocess.run(["/bin/cp", "-cR", str(args.clone_prefix.resolve()), str(prefix)], check=True)
        if not (prefix / "system.reg").is_file():
            parser.error("private prefix is not initialized")
        run = output / uuid.uuid4().hex
        run.mkdir(parents=True)
        shutil.copy2(exe, run / "ROADRASH.EXE")
        source = ROOT / "oracle/steal_fixture.c"
        definition = ROOT / "oracle/aweman_proxy.def"
        command = ["i686-w64-mingw32-gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-shared",
                   "-static-libgcc", "-o", str(run / "AWEMAN32.DLL"), str(source), str(definition)]
        with (run / "build.log").open("w") as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        trace = prefix / "drive_c" / ("steal-fixture-" + run.name + ".tsv")
        env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG="-all", WINEDLLOVERRIDES="aweman32=n",
                   RR_STEAL_FIXTURE_OUT="C:\\" + trace.name)
        started = time.time()
        with (run / "wine.log").open("w") as log:
            result = subprocess.run([str(args.wine), str(run / "ROADRASH.EXE")], cwd=run,
                                    env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        if result.returncode:
            raise RuntimeError(f"Wine fixture failed ({result.returncode}); inspect {run / 'wine.log'}")
        dest = run / "steal.tsv"
        shutil.copy2(trace, dest)
        rows = read_fixture(dest)
        receipt = {"scope": "original resolver component fixtures, synthetic nodes and animations; fixed original mode-3 rolls",
                   "exe_sha256": sha256(exe), "dll_sha256": sha256(run / "AWEMAN32.DLL"),
                   "source_sha256": sha256(source), "trace_sha256": sha256(dest), "cases": len(rows),
                   "prefix": str(prefix), "trace": str(dest), "build_command": command,
                   "wine": str(args.wine), "exit_code": result.returncode,
                   "duration_seconds": time.time() - started}
        atomic_json(run / "receipt.json", receipt)
        print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
