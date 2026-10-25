#!/usr/bin/env python3
"""Scoped LLVM 22 formatting/static analysis; never apply lint fixes."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {".cpp", ".hpp", ".cc", ".h", ".cxx", ".hh", ".c"}
FIRST_PARTY_DIRS = ("src", "tests", "fuzz", "oracle")


def llvm_tool(name):
    override = os.environ.get(name.upper().replace("-", "_"))
    candidates = [override] if override else [shutil.which(name)]
    if not override:
        # Homebrew LLVM is keg-only; do not change the user's PATH.
        for prefix in ("/opt/homebrew/opt/llvm", "/usr/local/opt/llvm"):
            candidates.append(str(Path(prefix) / "bin" / name))
        candidates.append(shutil.which(name + "-22"))
    for candidate in candidates:
        if not candidate or not Path(candidate).is_file():
            continue
        result = subprocess.run([candidate, "--version"], capture_output=True, text=True, check=True)
        if re.search(r"version 22\.", result.stdout):
            return candidate
    raise ValueError(f"{name} 22.x required; install LLVM 22 or set "
                     f"{name.upper().replace('-', '_')} to its executable")


def select_files(paths):
    if paths:
        files = [Path(p).resolve() for p in paths]
    else:
        # Includes new first-party files, excludes ignored build/vendor outputs.
        result = subprocess.run(
            ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", *FIRST_PARTY_DIRS],
            cwd=ROOT, capture_output=True, check=True,
        )
        files = [ROOT / os.fsdecode(p) for p in result.stdout.split(b"\0") if p]
        files = [p for p in files if p.is_file() and p.suffix in EXTENSIONS]
    for path in files:
        relative = path.relative_to(ROOT)
        if relative.parts[0] not in FIRST_PARTY_DIRS or path.suffix not in EXTENSIONS or not path.is_file():
            raise ValueError(f"expected an existing first-party C/C++ file under src/tests/fuzz/oracle: {path}")
    if not files:
        raise ValueError("no C++ files selected")
    return sorted(set(files))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("format", "format-check", "lint"))
    parser.add_argument("--build-dir", default="build", help="configured native compile database directory")
    parser.add_argument("files", nargs="*", help="explicit C/C++ files; default: all first-party C/C++")
    args = parser.parse_intermixed_args()
    files = select_files(args.files)
    if args.mode != "lint":
        tool = llvm_tool("clang-format")
        if args.mode == "format":
            # clang-format 22.1.4 is not idempotent on some trailing-comment
            # alignment (first pass aligns, second collapses); iterate to a fixpoint.
            for _ in range(4):
                result = subprocess.run([tool, "--style=file", "--fallback-style=none", "-i", *map(str, files)],
                                        cwd=ROOT)
                if result.returncode:
                    return result.returncode
                check = subprocess.run(
                    [tool, "--style=file", "--fallback-style=none", "--dry-run", "--Werror", *map(str, files)],
                    cwd=ROOT, capture_output=True, text=True)
                if not check.returncode:
                    return 0
                files = [ROOT / line.split(":")[0] for line in check.stderr.splitlines() if line.startswith(str(ROOT))]
                files = sorted(set(files))
                if not files:
                    return 0
            print("cpp-quality: format did not converge", file=sys.stderr)
            return 1
        return subprocess.run([tool, "--style=file", "--fallback-style=none",
                               "--dry-run", "--Werror", *map(str, files)], cwd=ROOT).returncode

    tool = llvm_tool("clang-tidy")
    build = Path(args.build_dir).resolve()
    database = build / "compile_commands.json"
    if not database.is_file():
        raise ValueError(f"missing {database}; configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON first")
    entries = json.loads(database.read_text())
    translations = set()
    for entry in entries:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        translations.add(source.resolve())
    # Headers are checked through their translation units, using actual build flags.
    sources = [p for p in files if p.suffix in {".cpp", ".cc", ".cxx"}]
    if not sources:
        raise ValueError("lint needs a .cpp/.cc/.cxx translation unit; headers are checked through it")
    missing = [str(p.relative_to(ROOT)) for p in sources if p not in translations]
    if missing:
        raise ValueError("refresh compile database; missing entries: " + ", ".join(missing))
    status = 0
    for source in sources:
        print(f"lint: {source.relative_to(ROOT)}", flush=True)
        result = subprocess.run([tool, f"-p={build}", f"--config-file={ROOT / '.clang-tidy'}", str(source)],
                                cwd=ROOT)
        if result.returncode:
            status = 1
    return status


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"cpp-quality: {error}", file=sys.stderr)
        sys.exit(2)
