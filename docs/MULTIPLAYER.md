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
  - the `DUEL` video is the mano-a-mano cutscene (`jukebox-port-20261012.md`).
- **Other branches** that test `+0x324 == −1` (remote) include:
  - walk `0x0040a600`, arrest `0x00407f30` and cop `0x00406ee0`;
  - the rider list and names `0x00437860` / `0x00446bf0` / `0x00446c30`;
  - nearest-opponent HUD `0x0044f1b0`;
  - remount tail `0x0043cd50`;
  - lateral collision `0x004317a0`. Its first loop is the networked one, using
    the `0x00497304..0x00497404` mask (`bike-lateral-20261015.md`).

## Consequences for the port

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
