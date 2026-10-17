# Plan 33: Linux and Windows ports

Goal: the same game, built from the same sources, runs natively on Linux (x86-64 and ARM64) and Windows (x86-64 and ARM64). This follows macOS and Android; it starts once macOS is feature complete, but the portability rules below apply to all code written now.

## Portability rules (apply today)

- Platform calls go through SDL3 or the existing platform layer. No new `#ifdef __APPLE__` outside `macos/` and the packaging scripts.
- Simulation code must not depend on the compiler, endianness, `long` width or unspecified evaluation order. Debug/Release determinism is already a gate; add GCC and MSVC to it here.
- File paths use `std::filesystem` with UTF-8; no hard-coded separators.
- Tools and tests must not assume `fish`, Homebrew paths or macOS `sed`/`stat` flags.

## Tasks

1. **Linux build.** CMake preset for GCC and Clang, SDL3 from the distro or vendored build, `ctest` green, simulation goldens identical to macOS. Record the compiler and distro in `docs/VERIFICATION.md`.
2. **Linux packaging.** AppImage or Flatpak with no bundled game data; first-run disc import works with the folder and ISO pickers. Controller and Steam Deck input verified.
3. **Windows build.** MSVC and clang-cl presets, vcpkg or vendored SDL3, `ctest` green, goldens identical.
4. **Windows packaging.** Portable zip and installer, code-signing hooks, no bundled game data.
5. **CI.** macOS, Linux and Windows runners run the native gate and the determinism comparison; failures block merges.
6. **Acceptance.** On each OS: import a disc, play every course and mode, change settings, save and reload, and record the exact observations.

Status: not started. Tracked in [00-INDEX.md](00-INDEX.md).
