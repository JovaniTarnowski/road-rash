# Race music: corrected parser and synthesis contract

Plan 13 Task 1 can start from these corpus/static findings. Its current SMF
parser specification and bank naming must be corrected before Task 2.

## All disc songs are RIFF MIDS

Every one of the eleven `.MID` files begins `RIFF … MIDS`. None begins `MThd`.
They are not RMID wrappers containing a Standard MIDI file either. The original
loader `0x00434150` → `0x00434230` parses MIDS directly; `0x00433e30` sends
the event buffers to `midiStreamOut`, sets time division and enables looping.

Original format and reproduction: `tools/research_after_11a.py` parses all
eleven files; detailed program/controller/tempo/block inventories are in
`analysis/oracle/post11a-research/census.json`.

| Structure | Fields (little endian) |
| --- | --- |
| RIFF header | `RIFF`, u32 size excluding first8, `MIDS` |
| `fmt ` chunk | 12-byte body: u32 time division, u32 maximum buffer bytes, u32 flags |
| `data` chunk | u32 block count, followed by blocks |
| Block | u32 tick-start metadata, u32 byte length, event bytes |
| Normal event (flags bit0 clear) | u32 delta ticks, u32 stream id, u32 event |
| Compact event (flags bit0 set) | u32 delta ticks, u32 event; stream id is implicit zero |

The original ignores the block's tick-start word when copying it to MIDIHDR;
the event deltas drive playback continuously across block boundaries. Treat
timestamps as metadata/checks, not an independent reset of song time.
`0x00434420` expands the compact representation to three words.

For a normal `dwEvent`, the high byte gives event type/flags. Short type0 packs
status in bits0–7, data1 in8–15 and data2 in16–23. Type1's low24 is tempo in
microseconds per quarter note. If high bit0x80 is set, low24 is payload length;
payload padding reaches a 4-byte boundary. Callback bit0x40 is independent.
Validate headers, RIFF extents, chunk padding, block counts/sizes, event extent,
unknown flags/types and checked tick accumulation before allocating.

All eleven corpus files have flags0, max buffer4096, no long events and event
types0/1 only. The ten Grunge/SbkGrng files use division480; Klown uses120.
Klown has112 tempo events; each other file has one. Their last accumulated
tick values are203520,134400,241920,188160,207360 respectively, and59036 for
Klown. Preserve event order at equal ticks (controller/program/note ordering).
Use rational or remainder-carrying time→sample conversion, so many Klown tempo
changes do not accumulate rounded scheduling drift.

Primary event semantics: [Microsoft MIDIEVENT](https://learn.microsoft.com/en-us/previous-versions/dd798448%28v%3Dvs.85%29)
and [stream buffer format](https://learn.microsoft.com/en-us/windows/win32/multimedia/stream-buffer-format).

Implement `read_mids` or format-dispatch `read_midi`; SMF support is optional.
TinyMidiLoader alone cannot load this corpus. Write synthetic tests covering
multi-block delta accumulation, tempo changes, callbacks, compact stream ids,
long events with padding, all truncated forms, oversized counts and unsupported
events. Add the **eleven** real files to corpus acceptance, not only five.

## Exact song and bank selection

`0x00435da0` calls `0x00433bb0` with original course index minus1. The song
pointer tables are `0x0046d908` (generic MIDI), `0x0046d920` (AWE loaded bank)
and `0x0046d938` (melodic bank); `0x00434500` chooses between them.

| Original course | Generic path | AWE path | Melodic SBK |
| --- | --- | --- | --- |
| CITY (1) | GRUNGE1.MID | SBKGRNG1.MID | GRUNGE1.SBK |
| HIWAY (2) | GRUNGE2.MID | SBKGRNG2.MID | GRUNGE2.SBK |
| MEDLY (3) | GRUNGE3.MID | SBKGRNG3.MID | GRUNGE3.SBK |
| NAPA (4) | GRUNGE4.MID | SBKGRNG4.MID | GRUNGE4.SBK |
| CANYN (5) | GRUNGE5.MID | SBKGRNG5.MID | GRUNGE5.SBK |
| Alias special case | KLOWN.MID | KLOWN.MID | GRUNGE5.SBK |

`0x00433bb0` compares alias prefixes case-insensitively through `0x004573c0`:
first6 characters `krusty` or first4 `bozo` select entry5. There is **no**
`KLOWN.SBK`, and **no** `SBKGRNG*.SBK`, on this disc. `0x00434500` loads
`RRDRUMS.SBK` into AWE bank1 and the selected `GRUNGE*.SBK` into bank2
via `0x00434620`. It unloads those banks first (ordinal2 commands0xcd), gets
an AWE handle with0x65, probes0x68, and falls back to generic songs if loading
fails. `0x00433bb0` checks mute/unavailable flags and sets MIDI output volume
through `0x00433cb0`. The proxy forwards AWEMAN calls; Wine may not supply an
actual AWE ROM renderer, so event capture and audio equivalence are distinct.

Do not substitute GRUNGE4.MID for SBKGRNG4.MID merely because both say Grunge4.
Arrangements differ; e.g. GRUNGE2's tempo521739 differs from SBKGRNG2's545455.

## Six banks are genuine SoundFont 1.0

Every bank's RIFF form is `sfbk`, `INFO/ifil = {1,0}`. The files contain:
`INFO`, `sdta/{snam,smpl}`, and
`pdta/{phdr,pbag,pmod,pgen,inst,ibag,imod,igen,shdr}`.

| Bank | Presets / instruments excluding terminal records | Sample-header records | Programs in bank0 |
| --- | --- | --- | --- |
| GRUNGE1 | 3 / 3 | 3 | 87,30,29 |
| GRUNGE2 | 2 / 2 | 3 | 36,30 |
| GRUNGE3 | 3 / 3 | 3 | 27,29,34 |
| GRUNGE4 | 5 / 5 | 5 | 36,28,30,31,37 |
| GRUNGE5 | 4 / 4 | 5 | 36,28,29,37 |
| RRDRUMS | 1 / 19 | 13 | program0 in **bank128**, not bank0 |

SF1 uses20-byte sample names separately in `sdta/snam`,16-byte `shdr` entries
(start,end,loop-start,loop-end), and6-byte `pmod`/`imod` chunks here. SF2's
46-byte sample headers and10-byte modulators cannot simply reinterpret these.
`smpl` is consistent with signed16-bit sample storage; rates/pitch metadata
must be recovered from SF1 defaults/generators, not a fictional shdr rate field.
The RRDRUMS final header is zero-length; handle it separately from live samples.

Used instrument-generator operators vary materially by bank. Union:
2,3,4,5,6,8,9,10,11,13,15,16,17,21,22,23,24,26,27,28,29,30,34,35,36,37,38,
43,48,51,52,53,54,55,57,58. Retain raw signed/packed amounts per operator.
There are filters, envelopes, LFO modulation, pitch/tuning, pan, key ranges and
loops; the proposed envelope-only minimal sampler is not automatically faithful.

Primary implementation references:

- [TiMidity SF1 reader](https://github.com/starg2/timidity41/blob/master/timidity/sffile.c)
  distinguishes the shorter SF1 headers, supplies absent sample metadata and
  adjusts SF1 loop endpoints. Inspect `load_sample_info` and `correct_samples`.
- [TiMidity generator conversion](https://github.com/starg2/timidity41/blob/master/timidity/sbkconv.c)
  and [generator merge](https://github.com/starg2/timidity41/blob/master/timidity/sndfont.c)
  demonstrate that SF1→SF2 requires parameter conversion, not just RIFF rewriting.
  These sources are GPL; they are research references, not vendored dependencies.
- [TinySoundFont](https://github.com/schellingb/TinySoundFont/blob/main/tsf.h)
  is MIT and supports SF2. Its documented omissions include modulators,
  chorus/reverb generators; preallocation APIs cover voices/channels. Pin an
  exact commit only when choosing and integrating it.

Recommendation: retain plan13 optionA as an **unproven conversion spike**.
Implement bounds-checked SF1 representation first; convert one bank and verify
generator units, key ranges, sample addressing, loop endpoints and pitched note
duration before generalizing. Do not silently drop operators to fit TinySoundFont.
Conversion can happen at load time, outside any audio callback.

## The disc banks are not a complete GM set

SbkGrng1 actually sounds program18 in addition to custom87/30/29. That preset
is absent from GRUNGE1.SBK. Klown sounds programs35,56,12,71,11,21; none is in
its GRUNGE5.SBK. The other SbkGrng songs' sounding melodic programs are covered
by their matching banks. Drums use zero-based MIDI channel9 and RRDRUMS bank128.
These are note-time program observations, not unused program-change guesses.

Inference: missing notes used factory AWE/GM instruments. Converting the six
disc banks alone cannot reproduce them. Keep this explicit in the synthesis
decision: select a separately licensed/user-supplied base GM bank, recover the
intended factory source, or document a limited synthesis deviation. Do not
bundle proprietary AWE ROM data or replace missing presets with arbitrary ones
and call it exact. A fallback RRA substitution remains a user decision under
the plan; this research does not authorize that behavior change.

## Mixer integration needs one extra interface

Current `Mixer`'s256-entry SPSC queue transports play/set/stop commands referencing
whole immutable `PcmSound` objects. It is **not** a streaming PCM sample queue.
`queued_frames()` reports SDL buffering, not free synth ring capacity. Plan13's
game-thread `MusicPlayer::update()` cannot safely overwrite a sound vector while
the callback is reading it.

Add a dedicated bounded streaming PCM ring with producer/consumer cursors, or
pre-render a whole song at load time and account for its memory. For the ring:
reserve storage once; synth ownership stays on the producer; callback copies
available frames and emits silence on underrun; stop/pause use an ordered
generation/cursor handshake so old queued samples cannot leak into the next
song. Retire old voices/resources before releasing buffers. Volume/channel
changes should reach existing voices on the next block. A full-song shortcut
must be measured against plan23's resident-memory budget.

Acceptance separates parser corpus success, deterministic scheduled events,
safe callback ownership, human listening and audio comparison with the original.
No listening or audio capture was performed in this research session.
