#!/usr/bin/env python3
"""Doc-consistency checks for agent-tooling (plan 31 Task 3).

1. Every plan file has a row in 00-INDEX; every linked plan exists.
2. Relative links in docs/*.md and docs/plans/*.md resolve (code dropped first).
3. PROGRESS.md header names the configured C++ standard.
Exits nonzero with one line per problem.
"""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
problems = []
# 1. Every plan file has a row in 00-INDEX, and every linked plan exists.
index = (root / "docs/plans/00-INDEX.md").read_text()
for plan in sorted((root / "docs/plans").glob("[0-9][0-9]*.md")):
    if plan.name != "00-INDEX.md" and plan.name not in index:
        problems.append(f"00-INDEX.md: no row for {plan.name}")
for link in re.findall(r"\]\(([0-9][0-9][^)]*\.md)\)", index):
    if not (root / "docs/plans" / link).exists():
        problems.append(f"00-INDEX.md: broken link {link}")
# 2. Relative links in docs/*.md and docs/plans/*.md resolve.
for doc in list((root / "docs").glob("*.md")) + list((root / "docs/plans").glob("*.md")):
    # Drop fenced and inline code first: C++ lambdas like `[&](bool v)` look like links.
    prose = re.sub(r"`[^`\n]*`", "", re.sub(r"```.*?```", "", doc.read_text(), flags=re.S))
    for target in re.findall(r"\]\((?!https?:|#)([^)#\s]+)", prose):
        if not (doc.parent / target).exists():
            problems.append(f"{doc.relative_to(root)}: broken link {target}")
# 3. PROGRESS.md names the configured C++ standard.
std = re.search(r"CMAKE_CXX_STANDARD (\d+)", (root / "CMakeLists.txt").read_text()).group(1)
if f"C++{std}" not in (root / "docs/PROGRESS.md").read_text().split("## Timeline")[0]:
    problems.append(f"PROGRESS.md header does not mention C++{std}")
print("\n".join(problems) or "doc checks OK")
sys.exit(1 if problems else 0)
