# Agent tooling implementation

Task: implement review P0/P1. Baseline: e3136a3; dirty simulation/oracle source
and shared progress/plans belong to other sessions and are preserved.

Owned files: new agent tooling scripts/tests/docs, root/native instructions,
Makefile additions, CMake test/build-info integration, CI/packaging/Ghidra script
wrappers. `game_main.cpp` receives only an isolated build-info option; existing
hunks remain intact. No edits to race code, oracle proxy or replay implementation.
Build directory: `build-agent-tooling`. No shared Wine/Ghidra captures scheduled.

Progress: installed canonical AGENTS.md and Claude imports; implementing resource
claims, gate receipts, strict simulation validation, capture receipts, doctor,
status/evidence and build provenance.
