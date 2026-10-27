# Mano-a-mano: how the original's multiplayer works

Static analysis of the unmodified disc executable (`ROADRASH.EXE`, SHA-256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`, matching
`analysis/inventory.json`) and the English string DLL. Sources: the PE import
table, `analysis/original-disassembly.txt`, the Ghidra exports in
`analysis/decompiled/`, and the menu tables read the same way
`read_frontend_data` reads them. Written October 26, 2026 as plan 09 Task 1
(analysis) and the input to [plan 34](plans/34-multiplayer.md).

**Nothing here has been observed running.** Every statement comes from code and
data. The labels mean:
- **traced**: read directly from code or data at the cited address;
- **inferred**: a reading of what traced code does, not yet confirmed by a
  capture. Plan 34 Task 1 confirms or corrects each one with a two-instance
  capture.

## Summary

Mano-a-mano (game mode 3, `0x004753b0 == 3`) is the original's **networked
multiplayer mode**. It supports **up to eight human players** over TCP/IP or IPX
on a LAN, or **two players** over a modem. It is not split-screen and not
two-player only. The netcode is **not lockstep**:
- each machine simulates its own human rider;
- each machine also simulates the AI riders it currently owns;
- each machine broadcasts snapshots and events for those riders;
- remote riders are moved from those snapshots;
- the host is a star relay and the authority for shared race state.

The old plan 09 assumed two-player lockstep. That assumption is wrong, and plan
34 supersedes plan 09.

## Front end (traced)

Main menu (screen 9) item 2 is **Mano-À-Mano**. Its handler is `0x00430140`,
and its description is "It's a fury of head-to-head mayhem as you duke it out
with your best friend. Or your worst enemy." The native front end disables this
item in the `Frontend` constructor (`src/frontend.cpp`).

The handler `0x00430140`:
- sets the enable bytes of the screen 0x0b items from the client/host flags;
- calls the restroom-enable rules for mode 3 (`0x00444fb0(3)`);
- when the session is hosted over TCP/IP or IPX, starts Win32 timer 0xb
  (500 ms, `0x004301e0`). This is inferred to be the game advertisement or
  lobby tick.

Screen **0x0b, Mano-À-Mano** (`0x0046c938`; the item count of 6 is read at start-up from `0x0046ca88`):

| Item | Caption | Handler | Description (abridged) |
| --- | --- | --- | --- |
| 0 | Host (289) | `0x0042fc40` | Pick a level, a connection mechanism and a character, then wait for your friends |
| 1 | Join (288) | `0x0042fef0` | Pick a game, then pick a character |
| 2 | Bulletin Board (54) | → screen 0x0c | Sign up for one of the races (disabled until connected) |
| 3 | Chat Messages (220) | `0x00430350` → 0x0d | Comments to scream at fellow rashers during a race |
| 4 | Restroom (6) | → 0x10 | Options |
| 5 | Disconnect (290) | `0x00431600` | Leave the session (disabled until connected) |

- **Host** opens a level popup (`0x0046c708`: Level 1–5, setter `0x004453f0`).
  The connection type comes next.
- **Join** opens a connection popup (`0x0046c858`):
  - **Modem** (388) is `0x184`;
  - **TCP/IP** (389) is `0x185`;
  - **IP/X** (390) is `0x186`.

  The value is stored in `DAT_0046c4cc` (popup index = value − 0x183).
- **Screen 0x0c** is the shared course-select array: five courses, flag
  `0x4815`, handler `0x004213c0`. Its sixth item is **Disconnect**
  (`0x00469168`). Course voting text: "Choose a race. %d/%d players have
  chosen." (312).
- **Screen 0x0d, Chat Messages** has eight editable 60-byte messages bound to
  F1–F8 (`0x00430380` copies into `0x00497408 + i*0x3c`).

Other mano-a-mano strings (ENU.DLL ids), traced as present but not yet tied to
code paths:
- **Lobby status:** "%d players connected. Room for %d more." (223);
  "Waiting for players. %d slots available." (391);
  "%d players connected. The game is full." (392).
- **Join list:** "Pick from among the available %s games" (412), with columns
  Host / Level / Avail / Status (413–416) and states Available / Full / Racing
  (409–411). Also "Public games" (426), "Unlisted games" (421), and IP/X address
  book entries (423–425).
- **Roster announcements:**
  - "Level %d race in %s." (402);
  - "Let's welcome %s." (404);
  - "%s is playing as %s." (405–406);
  - "%s has chosen %s." (407–408);
  - "%s has left the game." (420);
  - "Your request has been rejected." (417).
- **Options:**
  - "A.I. bikers" with None / Limited / Full (497–503: "Humans only",
    "Limited A.I. bikers", "Full A.I. bikers");
  - "Enable cars" / "Disable cars" (398–399).
- **Race:**
  - "%s finished #%d." (427);
  - "%s wrecked." (428);
  - "%s got busted!" (429);
  - "The race results are now official." (441);
  - "%s has terminated the race." (442);
  - host leaving: "You are the master. If you leave, everyone dies!" (430).
- **Network:**
  - "Your network address is %s-%s." (422, IPX net-node);
  - Winsock errors (444–447);
  - a Dial-Up Networking warning (436–440).
- **Modem:** phonebook, dialling and TAPI error strings (297–311, 393, 457–495).
  These include "Waiting for a call..." and "Dialing %s...".

## Transports (traced)

`DAT_0046c4cc` selects the transport. `0x0042fdb0` initialises it:

| Value | Transport | Snapshot mask `DAT_004c5afc` | Players | Code |
| --- | --- | --- | --- | --- |
| `0x184` | Modem: TAPI `lineMakeCall`/`lineAnswer`, then the COMM/DATAMODEM handle | 3 (every 4th tick, 15 Hz) | 2 | TAPI `0x00410230..0x004139e0`; serial `0x004330a0..0x00433f70` |
| `0x185` | TCP/IP: Winsock 1.1 UDP (`SOCK_DGRAM`, `AF_INET`) | 1 (every 2nd tick, 30 Hz) | up to 8 | `0x00441b4c..0x00442900` |
| `0x186` | IPX: Winsock UDP-style datagrams over `AF_IPX` | 1 | up to 8 | same functions, IPX branch |

**WSOCK32 imports, by ordinal.**

| Ordinals | Functions |
| --- | --- |
| 2, 3, 4 | `bind`, `closesocket`, `connect` |
| 6, 8, 9, 12 | `getsockname`, `htonl`, `htons`, `ioctlsocket` |
| 15, 16, 17 | `ntohs`, `recv`, `recvfrom` |
| 19, 20, 21, 23 | `send`, `sendto`, `setsockopt`, `socket` |
| 52, 57 | `gethostbyname`, `gethostname` |
| 111, 115, 116 | `WSAGetLastError`, `WSAStartup`, `WSACleanup` |

There is no `listen` or `accept`, so every socket is a datagram socket.

**Ports.**
- TCP/IP game traffic uses UDP port **6996** (`0x1b54`). Discovery broadcast is
  received on **6995** (`0x1b53`), sent with `SO_BROADCAST` (`setsockopt` level
  `0xffff`, option `0x20`) to `255.255.255.255` (`htonl(0xffffffff)`).
- IPX uses sockets `0x4242` and `0x42ff`. Its broadcast goes to net `00000000`,
  node `FFFFFFFFFFFF`.

**Peers.** `DAT_0049bd00` holds eight socket slots, one per peer.
- `0x00442070` creates a datagram socket, makes it non-blocking (`FIONBIO`) and
  binds it.
- `0x00441fb0` then `connect`s it to the peer from a helper thread. Each try
  waits 2 s, with up to three tries.
- `0x004428a0` sends with `send` and retries on `WSAEWOULDBLOCK`.
- `0x004427f0` polls every slot with `FIONREAD` and then calls `recv`.
- `0x004426c0` calls `WSAStartup(1.1)` and `gethostname`.

**Modem.** `0x0043b830` sends a frame as `0x10 0x10` + payload over the serial
handle `DAT_00497650` (`0x004336b0`). `0x00433960` runs over the payload first
and is inferred to be the checksum or escaping step. The serial object is the
"serial-port session ctor" `0x00433380` noted in `sbkloader-rea-20261012.md`.
Disconnect over modem goes through `0x004102b0` (`lineDrop`/`lineClose`).

## Session roles (traced)

- `DAT_004c5af9` is the **host** ("master") flag; `DAT_004c5b00` is the
  **client** flag. Either one makes the race networked.
- **Player table:** `0x00497304`, eight rows of 0x20 bytes, with an in-use byte
  at +3. `0x00431840` returns the highest in-use row + 1.
  `0x0042fb20(slot)` is true when a row is in use and `0x004972d8[slot] == 0`
  (inferred: not dropped).
- **Rider fields:**
  - `+0x328` is the rider's **network slot** (0..15; the low nibble of every
    record header);
  - `+0x324` is the controller: 0 local human, 1 AI, −1 remote. The October 26
    native work calls this field "bike +0x324 rider type".
- Game state `+0` (`0x004b8a10`) is the local player's network slot. The
  original clears it at race start outside mano-a-mano (`0x004160c0`).
- **Star topology.** `0x0043c640` broadcasts a record:
  - a client queues it for slot 0 (the host) only;
  - the host queues it for every in-use slot whose rider is remote.

  `0x00430570` is the host's fan-out send: every in-use slot except the source.
  Records a client sends are re-queued by the host for the other peers
  (`0x0043f170`, `0x0043f2e0`, `0x0043f3a0`).

## In-race protocol (traced layout, inferred meaning)

**Cadence.** Every simulation tick `0x0044b2e0` increments `DAT_004b7ecc`, then:
- runs `0x0043cdb0` (send) and `0x0044f040` (nearest remote riders) when
  `(DAT_004c5afc & tick) == 0`;
- on a client, every 4,096 ticks (`(tick & 0xfff) == 0xffa`), samples
  `QueryPerformanceCounter` (`0x004163b0`).

**Frames.** A frame (kind byte `0x0a`) is **0x4c bytes**:

| Bytes | Content |
| --- | --- |
| 1 | kind (`0x0a`) |
| 1 | presence mask |
| 3 × 24 | record slots; a 48-byte record uses two slots |

- Each peer has one frame buffer (`0x00499430 + peer*0x4c`, fill count
  `0x00499690[peer]`). When it holds three records it is flushed
  (`0x0043cff0` → `0x004305c0`).
- Record byte 0 = `type << 4 | slot`.
- Most records carry the 32-bit tick `DAT_004b7ecc` at bytes 1..4.

Record types. The receiver is `0x0043e9c0`. Builders are named where traced:

| Type | Builder → receiver | Contents and meaning |
| --- | --- | --- |
| 0 | `0x00442f60` → `0x00440340` | Heartbeat, every 0x39 ticks. Client to host: tick. Host to client: per-slot **handicap** byte `0x004c1200[slot]` (−128..127), computed from each human's progress `+0x1f8` around the mean (spread capped at 0x4200). Client: `0x0046ddd8 = (now − echoed tick) / 2` (inferred: one-way latency in ticks) |
| 1 / 2 | `0x0043c7e0` → `0x0043fe90` / `0x004c1400` | **Rider (type-4 object) / bike snapshot**: tick, `+0x5c>>16` and `+0x60>>16` (clamped 0x7f), flags bit 0, `+0x28`, `+0x1c`, `+0x20`, a 24-bit track sub-slice reference with a sub-slice word in the top byte, and the controller state `+0x2f4`. `0x0043fe90` keeps a short per-slot history and resynchronises when the tick gap exceeds 0x14 (inferred: interpolation or extrapolation) |
| 3 | `0x0043cc80` → `0x0043c7d0` route | **Crash** (48 bytes): tick, position words, two optional 3-word impulse vectors, a reason, `+0x1c`, `+0x28`. Low nibble 0xf has a cop meaning (`0x00432980` / `0x00407310`) |
| 4 | `0x0043cd50` → `0x00404480` | **Remount**: tick |
| 5 | → `0x0043cbb0` | Rider `+0x2f0` and bike animation words `+0x38`/`+0x3c` |
| 6 | → `0x0044e220` | **Traffic car** state, matched by car serial (`+0x208`); otherwise spawned through `0x0044e270`. A client sends it to the host, which forwards it |
| 7 | → `0x0041f3a0` | **Pair event** between two object ids: sets `+0xf0`/`+0xf4`/`+0x100` on both (inferred: hit or weapon contact) |
| 8 | → `0x004416a0` | Object placement: `+0x28`, `+0x1c`, `+0x1f8`, `+0x1e4`/`+0x1e8`/`+0x1ec`, `+0xec` |
| 0xa / 0xb | → `0x00431c20`, `0x00431ae0`, `0x00431ba0` | **AI ownership** negotiation. `0x004c4fb0[racer]` is the slot that simulates each racer (−1 none). `0x004c4e20[slot]` is a 16-bit interest mask. Grants and releases go through the host |
| 0xd | → `0x004321f0` | Cop state (the single cop `DAT_004642dc`) |
| 0xe | → `0x004c3810` | **On-foot input** for a remote walking rider, read by `0x00440410` from the `0x0040a9e0` remote branch of `0x0040a600` |
| 0xf/6, 0xf/7, 0xf/8 | `0x004326e0`, `0x004325a0` every tick; `0x004327d0` every 64 ticks | **Host broadcasts**: interest masks, the owner table, and every bike's `+0x1c8` |

The tick-loop send `0x0043cdb0`:
- snapshots the local human;
- snapshots every AI rider whose owner is this slot;
- snapshots the cop if this slot owns it;
- on the host, also runs `0x0043d610` (shared tables), `0x0043d040` (heartbeat,
  type 6/7/8 forwards) and `0x0043d7d0` (crash, type 4/5/1/2/0xe forwards, per-AI
  records).

## Simulation rule changes in mano-a-mano (traced)

- **Countdown** `0x00442dd0`: 0x168 ticks (6 s) when networked instead of 0xb4.
- **Traffic:**
  - density is multiplied by 4 when networked (`0x0044dd00`), so reloads are
    four times longer and there are fewer cars;
  - `0x0044df60` never spawns odd-kind cars when networked;
  - other spawns are suppressed between the local rider and the nearest remote
    rider ahead (within 0x4600) or behind (within 0x2d00), chosen by
    `0x0044f0d0` from riders with `+0x324 == −1` on an overlapping road. Cars
    spawned elsewhere are shared as type-6 records.
- **Random draws:** `0x00441750` returns `DAT_0049bce8` instead of `rand()` in
  mode 3 (inferred: a value agreed across peers, so call sites that must match
  on every machine do not diverge).
- **Grid size:** `0x00405600` tests both network flags at
  `0x00405614..0x00405621` and skips the offline grid count of 16. The networked
  count comes from the session (to trace).
- **Results:**
  - outcome mapping copies a network name (`0x00416010`);
  - a win does not set qualification bits;
  - the results screen sits under the mano screen;
  - "Disconnect" replaces Restroom or Panzer Klub on course select;
  - busted and wrecked replay through `0x004210a0`;
  - ~~the `DUEL` video is the mano-a-mano cutscene~~ (corrected below: no result path plays `DUEL`).
- **Other branches** that test `+0x324 == −1` (remote) include:
  - walk `0x0040a600`, arrest `0x00407f30` and cop `0x00406ee0`;
  - the rider list and names `0x00437860` / `0x00446bf0` / `0x00446c30`;
  - nearest-opponent HUD `0x0044f1b0`;
  - remount tail `0x0043cd50`;
  - lateral collision `0x004317a0`. Its first loop is the networked one, using
    the `0x00497304..0x00497404` mask (`bike-lateral-20261015.md`).

## Gameplay rules the port keeps (plan 34 Task 1, traced October 26, 2026)

Static traces from the Ghidra exports (`analysis/decompiled/<address>.c`) and
`analysis/original-disassembly.txt`. No two-instance capture was needed: every
rule plan 34 Tasks 2 and 7 use is read directly from code. "Not referenced"
means no load of that string-table offset (`DAT_00475378 + id*4`) exists in the
disassembly.

**Grid and rider count** (`0x00405600`, `0x004317a0`, `0x00431840`).
- `DAT_004642d4` is riders + 1. Offline it is `0x10` (15 riders). When
  networked it comes from the A.I. bikers byte `DAT_004b88f8` (set from the
  session record `+0x11` in `0x00430b30`, cleared by `0x0042fdb0`/`0x00430080`):
  - 2 (**Full**): `0x10`, 15 riders, as offline;
  - 1 (**Limited**): 9, so 8 riders: the humans plus A.I. up to 8;
  - 0 (**None**, "Humans only"): `0x00431840() + 1`, where `0x00431840` is the
    highest in-use row of the player table `0x00497304` plus 1. A gap in the
    rows still produces a remote rider for the empty row (character 1).
- `0x004317a0` fills the layout array: every human row other than the local
  one gets controller −1 (remote), its network slot as grid index, and its
  character through `0x00415fc0` (menu index 1..8 → rider 8,7,2,5,4,6,3,1).
  A.I. riders take the remaining entries `0x00431840()..riders−1`, grid index
  = entry index, controller 1.
- The local human is built at grid index = its own network slot
  `DAT_004b8a10`. **Humans therefore start at grid rows 0..n−1 by slot, A.I.
  behind them.**
- Grid table: `0x00464218` normally; `0x00464258` when more than four humans
  are connected (`0x0042fbb0() > 4`, `0x004056a6`). Remote humans use the
  player's bike/half tables `0x00463ba8`/`0x00463c10`; A.I. use
  `0x00463ad8`/`0x00463b40` with the level-split opponent rows
  `0x00464088`/`0x00464108` (level < 4 / ≥ 4), as offline.
- Remote humans get think `0x00407d40` (snapshot-driven). The port replaces
  that with the player think fed by the sealed control word (plan 34 Task 2).

**Handicap** (`0x00442f60` compute, `0x00440340` receive, `0x004395e0` apply).
- Every 0x39 ticks (`DAT_0049bdd0` trails the tick counter by 0x39) the host
  takes every human rider (controller 0 or −1) and reads half `+0x1f8`, the
  Q8 distance still to the finish (rider 0, the local human, always
  counts). It computes the mean, min and max, and skips the update when the
  sum is 0; the
  spread factor is `(max − min)·0x40/0x4200`, capped at 0x40 when the spread
  exceeds 0x4200.
- If the mean equals the min or the max, all handicap bytes are cleared
  (`0x004c1200..0x004c120f`). Otherwise, with `e = d − mean` for each human:
  - `e < 0` (ahead of the mean): byte = `spread·e/(mean − min)`, clamped to
    −0x80 below −0x7f;
  - `e ≥ 0` (behind): byte = `spread·e/(max − mean)`, clamped to 0x7f above
    0x7e.

  Divisions are signed and truncate toward zero; the mean is
  `sum/count` with the same truncation.
- Clients receive their own byte into `0x004c1200[0]`; the host uses its own
  slot 0 byte directly.
- **Application** (`0x004395e0:30-32`): only for the local human
  (controller 0), only when not crashed (`+0x2f4 == 0`), only when speed
  `+0xec > 0` and only when the byte is **positive**: `+0x108 = speed +
  (byte·speed >> 8)`. Leaders (negative bytes) are not slowed; trailing humans
  get up to +0x7f/256 ≈ 50% of their speed in the move step. It affects
  movement only, not the stored speed.
- The port computes it inside the simulation every 0x39 ticks from shared
  state and applies each human's own byte to that human's rider (plan 34
  Task 2). The original's one-heartbeat latency is a transport artefact.

**Countdown** (`0x00442dd0`): `DAT_004753bc = 0x168` when networked, else
0xb4.

**Chat in a race** (`0x00430690`, `0x004305f0`, `0x0042f920`, `0x00446c30`).
- F1–F8 (key messages 0x70..0x77 in the window procedure `0x004227d0:379-387`)
  call `0x00430690(n)`. A non-empty slot `0x00497408 + n*0x3c` is broadcast as
  record type 1 with the sender slot byte, then appended to the local log
  through `0x0042f920`.
- `0x0042f920` is a printf into a **six-line** log `0x004c58c0` (line count
  `DAT_004972f8`, capped at 6; the oldest line is dropped when full; a newline
  is appended when missing). There is **no timer**: lines stay until pushed
  out or until disconnect (`0x00431600`) or a new session (`0x00430b30`)
  clears the log.
- The dashboard text pass `0x00446c30:378-425` draws the log at the top of
  the race view: left 0, top 0, width = view width, height 0x15 (0x29 when
  `ChatState` `0x00474f48 == 2`), bottom-aligned so the newest lines show
  (`DrawTextA` flags 0x4c50 to measure, then 0x4850). With the inset/low
  view (`DAT_00475380 != 0`) it moves to the right half. The menus draw the
  same log through `0x0041ff80`.
- The line being typed is `0x004c5b10`, drawn in a filled bar under the log
  (colour `0xffaf93` on `0x404040`).

**Finish, wreck, bust while others race** (`0x00416010`, `0x004163d0`,
`0x00430e00`, `0x004314d0`, `0x00432440`, `0x00423345`).
- Outcome: a finish (`0x7f`) becomes **won (2) only for first place** when
  networked (`place ≤ 0`); offline it is won for places 0–2
  (`0x00416010:12-14`). Busted is 4, wrecked 5.
- When a machine's race loop ends (`0x004163d0` → `0x00442e10` true) it calls
  `0x00430e00`: it releases the A.I. it owned, sends record kind 10 with its
  slot, and leaves for the results screens. Each machine leaves on its own
  outcome; **the race keeps running for the others.**
- On receiving kind 10 (`0x004314d0`): the host marks that row done
  (`0x00497304 + slot*0x20 + 2`) and relays; `0x00432440` clears the rider's
  flags `+0xc0 & ~3` and its `+0xe4`/`+0xdc`, so the finished rider leaves the
  other screens. A client receiving kind 10 from slot 0 (the host) prints 442
  "%s has terminated the race." once and ends its own race, then 441 "The race
  results are now official."; the host prints 441 when it finishes.
- When the host's own outcome is set (`0x00416010:25-45`) its status line
  `0x004c5bd0` becomes 430 "You are the master. If you leave, everyone dies!".
- **Escape in a race** (`0x00423345`, mode 3, outcome still 1): the first press
  shows a status line instead of the offline modal prompt: 430 on the host,
  316 on a client (`0x6b8 + (host ? 0 : −0x72·4)`); the race is not paused. A
  second press quits with outcome 5.
- Strings 427 ("%s finished #%d."), 428 ("%s wrecked.") and 429 ("%s got
  busted!") are **not referenced** by this EXE. The port uses them as native
  announcements (a replacement, listed in REVERSE_ENGINEERING.md).
- Results (`0x00416120:36-60`): the same result screens as offline
  (`0x1e` won, `0x1f` lost, `0x1d` busted, `0x1c` wrecked); networked
  outcomes call `0x004210a0(local slot, …)` to place the local human in the
  standings. A win sets no qualification bits. Leaving a networked race
  returns to screen 0x0b (`0x00416120:21-34`).

**`DUEL` video.** `0x0046b1f0` ("DUEL") is referenced only by the attract-mode
second timer fire (`0x00423d6a`, random JESSIE/DUEL) and the jukebox video
table `0x00473fa0`. **No mano-a-mano result path plays it.** The earlier
reading "DUEL is the mano-a-mano cutscene" (this file, before October 26) was
wrong; mano results play the normal outcome videos of the result screens. The
port follows the code: plan 34 Task 7 plays the result-screen videos, not
`DUEL`.

**Restroom in mode 3** (`0x00444fb0`, patterns `0x00473598 + i*4`). Mode 3
selects pattern 5 (`0x004735ac`) when neither connected, 6 (`NNNNYYYY`,
`0x004735b0`) on the host, 7 (`NNNNYNYY`, `0x004735b4`) on a client, over the
13 restroom items at `0x00473860` (only the first eight letters exist, so items
8–12 keep their enable bytes). The native front end used pattern 6 for every
mano state; Task 7 selects 5/6/7. In the port every room member is a client of
`rr-netd`; the room host takes the host pattern.

**Networked menus** (`0x00421df0:142-175`): screens 8, 0x0b–0x0d, 0x10–0x13
and 0x1c–0x22 append `DAT_0046c4c8` extra rows from `0x0046c458` (the roster
and chat-log area) when connected.

## Port design (owner decision, October 26, 2026)

The owner decided that multiplayer is part of the definition of done, and that
it should use a modern internet design rather than the original's netcode or
transports. [Plan 34](plans/34-multiplayer.md) therefore uses a different
design from the original:
- a deterministic simulation that every client runs in full;
- rollback against a server-sealed input timeline;
- WebSocket and WebRTC DataChannel connections to `rr-netd` on the owner's VPS.

The trace above remains the source for the mano-a-mano **gameplay** rules the
port keeps:
- menus, strings and chat slots;
- the 0x168 countdown;
- traffic ×4 and the spawn gates;
- the handicap rubber band;
- no qualification bits; results use the normal outcome videos (`DUEL` is not a mano video, see the Task 1 traces).

The transport artefacts are dropped and recorded as deviations: snapshot
replication, AI ownership hand-off, host relay, modem, IPX and LAN discovery.

The section below was written before that decision. It describes what a
faithful port of the original netcode would need.

## Consequences for a faithful port (superseded by the owner decision)

1. Multiplayer fidelity means porting this **distributed-authority, host-relayed
   snapshot model**. Each peer is authoritative for its own rider and owned AIs.
   Rubber banding is the host's handicap byte. Remote riders are drawn from
   snapshots. Deterministic lockstep would play differently (no extrapolation,
   no ownership hand-off, input stalls), so it is not a faithful port.
2. The race tick stays the original 60 Hz integer simulation. Peers do not need
   identical `rand()` streams for most systems; only the `0x00441750` sites take
   the shared value.
3. **TCP/IP over UDP ports 6996/6995 maps onto modern sockets directly.**
   - IPX has no modern OS support.
   - Modem/TAPI has no practical modern equivalent.

   Plan 34 keeps the TCP/IP mode, presents it as "Internet/LAN", and records
   IPX and Modem as documented deviations.
4. **A two-instance capture is possible.** Two original instances under Wine
   would both bind port 6996 on one Mac. The oracle proxy can hook WSOCK32 calls
   in-process to remap ports and log every datagram. That gives wire goldens for
   plan 34 Task 1, and a native peer can later be tested against the original
   peer.
