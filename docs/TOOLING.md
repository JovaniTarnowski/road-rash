# Tooling notes

Environment setup, analysis-tool findings and workflow lessons that are not
part of the port itself. Cross-references: [ACTORS.md](ACTORS.md),
[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md),
[VERIFICATION.md](VERIFICATION.md).

## REA (`npx rea-agents@latest`)

REA 6.4.0 ("Reverse engineer anything") is used as an **independent second
opinion** on the project's own Ghidra export. It re-imports and re-analyzes
the reference executable ephemerally; it never reads or modifies
`analysis/ghidra-project/`.

### Setup on this Mac

`rea doctor` reports the requirements. Two were missing by default:

1. **Node:** REA accepts Node 22.x (>= 22.19), 24.x (>= 24.11) or 26+. The
   system default is 25.8.1 (an unsupported non-LTS line). Use the nvm
   install:
   ```sh
   export PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"   # or v22.23.1
   ```
2. **Ghidra provider:** Homebrew's Ghidra 12.1.4 needs the *libexec* root
   (the directory that contains `Ghidra/application.properties`), plus JDK 21:
   ```sh
   export GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec
   export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
   ```

`rea doctor` still prints `healthy: false` overall: Hopper and IDA are
optional analysis engines that are not installed, and the per-agent-client
registrations are unconfigured. Neither blocks the Ghidra-backed commands;
the `ghidra*` scope checks all report healthy with the two exports above.
`rea setup --install-hopper --yes` would add Hopper, but the project's
pipeline does not need it.

### Commands used

```sh
npx rea-agents@latest doctor
npx rea-agents@latest function "../cdrom/ROADRASH/ROADRASH.EXE" 0x44dd90 --format md
npx rea-agents@latest function "../cdrom/ROADRASH/ROADRASH.EXE" 0x44dd90 \
    --format md --filter-output raw_result.value.pseudocode
```

A `function` call takes roughly one to two minutes on this machine (fresh
ephemeral analysis each time).

### Evidence value and limits

- The `0x44dd90` (traffic spawn timers) pseudocode from REA's Ghidra 12.1.4
  backend was **identical** to the project's own `tools/decompile.sh` export,
  which independently supports the fidelity of the existing export pipeline.
- Good for: spot-checking a suspicious decompile before trusting it; getting
  a second rendering of a function whose Ghidra output looks wrong (see the
  gotcha list below).
- Not good for: bulk export (the project pipeline is faster, address-keyed
  and already done) or anything needing the project's manual annotations.

## Analysis gotchas

Lessons from the traffic work (October 11, 2026) that generalise to the rest
of the port:

1. **Ghidra pointer arithmetic is word-scaled.** `piVar3[0x71]` and
   `piVar3 + 0x1c` on an `int *` are byte offsets `0x1c4` and `0x70`. Every
   object-field mapping must multiply the bracket by 4.
2. **Ghidra can mis-decompile small helpers.** `0x0040b5f0` decompiled to an
   empty loop returning void; the real code (from
   `analysis/original-disassembly.txt`) walks the `SNAC` wrapper chain. The
   disassembly is authoritative — check it whenever a decompiled function's
   control flow looks meaningless.
3. **Object slots are `int`s unless proven otherwise.** The car phase
   `+0x71` is a full int slot holding 192..319; storing it in a `uint8_t`
   wraps to 0..63 and silently freezes the car. The original writes
   `(rand & 0x7f) + 0xc0` without a byte truncate.
4. **Resource tags are reversed between disk and code.** The directory shows
   `ANIM`/`CANS`; the code searches for `0x414e494d`/`0x43414e53`
   (`"MINA"`/`"SNAC"` packed the other way). `rr-inspect` displays the
   readable form; match on the display form in native code.
5. **`CRSR`/`LBTR` container layout** (for building synthetic fixtures):
   24-byte header (`CRSR`, …, `LBTR` at +12, table offset at +16); the table
   is `LBTR` + count at +8, then 32-byte entries: tag, id, offset, size,
   16 pad bytes. Entry payloads start right after the table.
6. **Zero words in the raw EXE image are runtime-initialised.** The vehicle
   count `DAT_00474d88` and masks `0x004b8fe0` are zero on disk and filled
   from `DATA/CARS/*.CAR` at load (`0x004465f0`); the static car template and
   per-kind words at `0x00475698`/`0x00475700` *are* in the image and must be
   read from the user's executable at runtime (`read_exe_data`), never
   embedded.
7. **One shared rand stream.** The game's `rand()` is MSVC CRT
   (`seed*214013 + 2531011`, return `(seed >> 16) & 0x7fff`), shared by every
   system. Adding a consumer (traffic) shifts the stream for all other
   consumers — exactly as in the original. Consequence for verification:
   feature on/off frame diffs diverge globally, not locally, so pixel diffs
   cannot isolate a single subsystem's render effect.
8. **Numbers must be traced to their exact constant.** The traffic cruise
   uses `DAT_004b7e8c = 25`, not the spawn rate `DAT_004b7e84 = 2500`; the
   two live 0x18 bytes apart in the same initialiser (`0x0044dd00`). Reading
   "a rate-looking constant" from a nearby address produced a 100× speed bug
   caught only by headless probing.

## Multi-session workflow

Two agent sessions have worked in this repository concurrently (October 11,
2026: the traffic/session-23 work and the plan-10 oracle work).

- `git add -A` sweeps the other session's in-flight files into your commit;
  this happened twice (`37c4597` contains the traffic sources, `51cb898`
  contains plans 10–15). Stage with **explicit pathspecs** whenever
  concurrency is possible, and check `git status` / recent `git log` first.
- `ls -laT` on the files you plan to touch reveals active writers (minute-old
  mtimes mean someone is mid-task).
- Rewriting shared history while another session works is riskier than
  leaving interleaved commits; leave the history and document the
  interleaving instead (see the session 23 entries in [PROGRESS.md](PROGRESS.md)).
- Before building or running `ctest`, expect the working tree to contain the
  other session's mid-edit code; a red build may be theirs, not yours.

REA actor animation analysis (October 11 continuation):
`npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE
0x0043ae90 --format json` with the environment above produced canonical
evidence in `analysis/oracle/rea-actor-animation.json`. The pseudocode agrees with
the repository's Ghidra export. The playback port was separately validated
against 2,048 calls to the original function; REA output alone was not treated
as runtime proof.
