# Front End, Progression and Saves Implementation Plan

> **Status (October 11, 2026 review):** Tasks 1–7 are largely done (strings, fonts, front end, dashboard, career, saves, settings). The sound mix, controls and jukebox screens, the attract demo, the original save round trip and menu pixel checks are in [plan 13](13-music-and-frontend-completion.md). This plan is kept as background for addresses and findings; follow the newer plans for remaining work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. Requires plan 03 (images and sprites) and plan 04 (race start and results interfaces). Tasks 5–8 also need plan 05's `RaceResult`.

**Goal:** Everything outside a race, plus the in-race HUD:
- localised text and fonts
- the menu flow from splash to race
- Thrash (single race) and Big Game (career) modes: levels, cash, bike shop, character scenes, results
- the in-race dashboard and HUD
- pause
- save/load compatible with the original's save files
- persistent settings

**Architecture:** A front-end state machine (`src/frontend.*`) ports the original's menu dispatcher (`0x00448200` and callees). It draws into the same `rr::Framebuffer`, so menu frames can be compared with oracle frames.
- **Text:** strings come from the language DLL's string table, read natively.
- **Fonts:** GDI system fonts, which are not game data, are replaced by a bundled open-licence bitmap font scaled to the original metrics. This deviation is documented.
- **Career:** state is a plain struct serialised to the original save format.

**Tech Stack:** C++17, SDL3, CTest, oracle (plan 01).

**Spec:** [00-INDEX.md](00-INDEX.md); plan 04 (`RaceConfig`, `start_race`), plan 05 (`RaceResult`).

**Execution order:** 1, 2, 5, 6, 3, 4, 7. `Frontend` (Task 3) consumes `Career` from Task 5 and saves from Task 6.

## Global Constraints

- The UI draws only into `rr::Framebuffer` with palette indices, at the original surface size (plan 02 `ViewMode`).
- All text shown to the player comes from the original language DLLs. No English literals in UI code.
- Save files are byte-compatible with the original: a save written by the Windows game loads natively and vice versa. Proven by Task 6's round-trip test on a real original save.
- Settings and saves live under `SDL_GetPrefPath("RoadRashNative", "RoadRash")`.
- A font substitute is the only intended visual deviation in menus. The pixel oracle masks text boxes, and every mask is listed in `VERIFICATION.md`.

## Review Focus

1. **Languages other than English** (DEU, FRA, ESP, ITA DLLs; longer strings, accented characters in the Windows-1252 code page): Task 1 decodes all five, and Task 2 renders the longest string of each without overflow.
2. **Corrupted or foreign save files:** loading must reject them with a message, never crash or partially apply. Task 6 has a truncation and garbage test.
3. **Quitting mid-race or mid-shop** (window close, Android back button): the career state must remain the last saved state. Task 7 tests quit during a race.
4. **Resolution change between hi/lo modes while in menus**, if the original allows it: the menus must redraw correctly. Task 3 follows the original's rule.
5. **Money edge cases:** cash that cannot pay a fine or repair, and buying with exactly enough cash. Task 5 pins these with synthetic tests from the traced rules.

---

## File Structure

| File | Responsibility |
| --- | --- |
| `src/pe_strings.hpp/.cpp` | Read `RT_STRING` tables from the language DLLs |
| `src/text.hpp/.cpp` | Font substitute, text layout matching the original's string drawing calls |
| `src/frontend.hpp/.cpp` | Menu state machine (port of `0x00448200` dispatcher and screens) |
| `src/career.hpp/.cpp` | Big Game/Thrash rules: levels, qualification, cash, bikes, fines, repairs |
| `src/savegame.hpp/.cpp` | Original save format read/write |
| `src/settings.hpp/.cpp` | Settings formerly in the registry (`SOFTWARE\Electronic Arts\RoadRash 95`) |
| `src/hud.hpp/.cpp` | Dashboard, speedometer, place/lap/damage displays |
| `third_party/<font>` | An OFL bitmap font plus licence |
| `tests/pe_strings_test.cpp`, `tests/career_test.cpp`, `tests/savegame_test.cpp`, `tests/oracle_ui_test.cpp` | Tests |

---

### Task 1: Language string tables

`TEXT/ENU.DLL` is a PE with a `.rsrc` section holding 88 `RT_STRING` (type 6) blocks, plus a version resource. Windows packs strings in blocks of 16: block id `n` holds string ids `(n−1)*16 … (n−1)*16+15`, each stored as a `uint16` length followed by UTF-16LE characters.

**Files:**
- Create: `src/pe_strings.hpp`, `src/pe_strings.cpp`, `tests/pe_strings_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  // String id -> text (UTF-8). Throws on malformed resources.
  std::map<uint32_t, std::string> read_string_table(const std::vector<uint8_t>& pe_dll);
  enum class Language { english, german, french, spanish, italian };
  const char* language_dll(Language);   // "TEXT/ENU.DLL", "TEXT/DEU.DLL", "TEXT/FRA.DLL", "TEXT/ESP.DLL", "TEXT/ITA.DLL"
  }
  ```

- [ ] **Step 1: Failing tests**

- **Synthetic:** build in the test a minimal PE with a `.rsrc` section containing one `RT_STRING` block (id 1) holding strings 0 (`"A"`) and 2 (`"Hé"`), and check that `read_string_table` returns `{0:"A", 2:"H\xC3\xA9"}`.
- **Truncation:** every truncated length throws.
- **Corpus test** (skip 77 without the disc): every language DLL decodes. English has more than 1,000 strings. The set of ids is identical across the five DLLs; if it is not, record the differences in `REVERSE_ENGINEERING.md` and assert the documented differences instead.

- [ ] **Step 2: Implement**

Parse the DOS/PE headers and the section table, then walk the resource directory three levels deep (type → name → language) to the data entries, mapping RVAs through `.rsrc`.

- [ ] **Step 3: Trace language selection**

The game calls `GetThreadLocale`/`GetLocaleInfoA`; find where it picks the DLL. Record the rule and implement `Language default_language()` from `SDL_GetPreferredLocales`.

- [ ] **Step 4: Pass, commit**

Commit: `text: read original language string tables`.

---

### Task 2: Font substitute and text drawing

The original draws text with GDI: `CreateFontA` (face `Small Fonts` at `0x00442990`), `TextOutA`, `SetTextColor`, `SetBkMode`, `GetTextExtentPoint32A`, and `AddFontResourceA` / `RemoveFontResourceA` for a custom font. That font file's name is found in Step 1.

**Files:**
- Create: `src/text.hpp`, `src/text.cpp`
- Add: `third_party/` font with licence

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct FontSpec { int height; int weight; bool italic; std::string face; };   // arguments seen at each CreateFontA call
  class TextRenderer {
  public:
      explicit TextRenderer(const Assets&);                // loads the game's own font file if it is game data, else the bundled substitute
      int width(const FontSpec&, const std::string& utf8) const;       // replaces GetTextExtentPoint32A
      void draw(Framebuffer&, const FontSpec&, int x, int y, uint8_t colour, const std::string& utf8) const;  // TextOutA, transparent background
  };
  }
  ```

- [ ] **Step 1: Inventory text calls**

List every `CreateFontA` call site with its arguments, and find the `AddFontResourceA` argument:
```sh
grep -lE "CreateFontA|AddFontResourceA" analysis/decompiled/*.c
```
If the added font is a file on the disc, render from that file. It is game data: load it at runtime and never bundle it. Parse `.FON`/`.FNT` bitmap fonts natively, as they are simple. Otherwise choose an OFL bitmap font close to MS Sans Serif / Small Fonts metrics.

- [ ] **Step 2: Test**

Synthetic tests: width of an empty string is 0; `draw` touches only pixels inside the measured box; the colour index is used verbatim.

- [ ] **Step 3: Implement, commit**

Commit: `text: font rendering for menus and HUD`.

---

### Task 3: Front-end state machine

**Files:**
- Create: `src/frontend.hpp`, `src/frontend.cpp`, `tests/oracle_ui_test.cpp`
- Modify: `oracle/aweman_proxy.c`: dump `ui.screen` (the state variable found in Step 1) and pixels on every front-end frame

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  enum class Screen : uint8_t { splash, main_menu, options, thrash_setup, career_hub, bike_shop, character_scene, results, busted, credits /* complete list from Step 1 */ };
  struct MenuInput { bool up, down, left, right, select, back; int pointer_x = -1, pointer_y = -1; bool pointer_click = false; };
  class Frontend {
  public:
      Frontend(const Assets&, const TextRenderer&, Career&);
      // Advance one frame. Returns a RaceConfig when a race should start.
      std::optional<RaceConfig> update(const MenuInput&);
      void draw(Framebuffer&) const;
      void on_race_finished(const RaceResult&);
      Screen screen() const;
  };
  }
  ```

- [ ] **Step 1: Trace the flow**

Read `0x00448200` (startup, menus, race transition) and its screens: the `.RRI` images (`MAIN`, `OLLEYS`, `PANZER`, `STREET`, `CREDITS`, `SPLASH`, `IMAGES/BIKES/*`, `IMAGES/CHARS/<name>-{F,H,N}`, `Results/Level%d`, `Street/Home|Klub|Olley`, `Postcard/*`) and menu keyboard input (`0x0040fb50`: first held key with a binding in `0x004c5d90`). Record the screen graph, which is every transition with its trigger, in `REVERSE_ENGINEERING.md`.

- [ ] **Step 2: Oracle**

Record input that visits every screen. Capture `ui-tour`, with pixels.

- [ ] **Step 3: Tests**

`oracle_ui_test` replays the same menu inputs through `Frontend` and compares `screen()` with `ui.screen` every frame, and pixels outside text masks every frame. It fails first.

- [ ] **Step 4: Port**

Port screen by screen until the test passes.

- [ ] **Step 5: Commit**

`frontend: original menu flow`.

---

### Task 4: HUD and dashboard

`0x00416310` loads `IMAGES/kHiDash.BOB`/`kLoDash.BOB` or `mHiDash.BOB`/`mLoDash.BOB` (km/h vs mph variants, hi/lo resolution) and `BIGMETER.BOB` (190×64).

**Files:**
- Create: `src/hud.hpp`, `src/hud.cpp`

**Interfaces:**
- Produces: `struct HudAssets { std::vector<uint8_t> dash, meter; int dash_width = 0, dash_height = 0, meter_width = 190, meter_height = 64; };` (indexed pixels from the `.BOB` files `0x00416310` selects), `HudAssets load_hud_assets(const Assets&, bool metric, bool high_resolution);` and `void draw_hud(Framebuffer&, const ViewMode&, const Race&, const HudAssets&);`

- [ ] **Step 1:** Trace where the dashboard, needle, place, damage and messages are drawn in the frame (a display-list node type or a direct blit after `0x0040ff40`). Document the units setting.
- [ ] **Step 2:** Extend `oracle_frame_test` (plan 02) to the HUD region for all race goldens. Fail, port, pass.
- [ ] **Step 3:** Commit: `hud: dashboard, speedometer and race info`.

---

### Task 5: Career rules

**Files:**
- Create: `src/career.hpp`, `src/career.cpp`, `tests/career_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct Career {
      int level = 1;                         // 1..5
      int32_t cash = 0;
      int bike = 0;                          // index into BikeSpec (plan 04)
      int32_t bike_damage = 0;
      std::array<bool,5> qualified{};        // per course at the current level
      std::string rider_name;
      // Complete field list from Step 1, including everything the save file stores.
  };
  void apply_result(Career&, const RaceConfig&, const RaceResult&);   // prize money, fines, repairs, qualification, level-up
  bool can_buy(const Career&, int bike); void buy(Career&, int bike);
  }
  ```

- [ ] **Step 1:** Trace the prize table by place and level, arrest fines, wreck repair costs, bike prices and availability per level, and the level-up rule. Record the tables in `REVERSE_ENGINEERING.md`; the code reads them from the EXE at runtime, like `read_depth_table`.
- [ ] **Step 2:** Synthetic tests for each rule: prize for 1st at level 1; a fine greater than cash; buying with exactly enough cash; qualifying the fifth course triggers level-up. Expected values come from Step 1's tables, read in the test from the EXE and skipped if the disc is absent. Pure-logic cases use hand-built tables.
- [ ] **Step 3:** Implement, pass, commit: `career: prize, fines, shop and level rules`.

---

### Task 6: Save files

**Files:**
- Create: `src/savegame.hpp`, `src/savegame.cpp`, `tests/savegame_test.cpp`

**Interfaces:**
- Produces: `std::vector<uint8_t> write_save(const Career&);` and `Career read_save(const std::vector<uint8_t>&);`. The reader throws `std::runtime_error` on invalid data.

- [ ] **Step 1: Trace and capture**

Trace the writer called after `GetSaveFileNameA` (`0x00445310` → `0x0042f000`) and the reader after `GetOpenFileNameA` (`0x0045558c`). Document the format. Create real saves with the original under Wine at level 1 and after winning races; keep them in `analysis/oracle/saves/`.

- [ ] **Step 2: Tests**

- Round-trip: `write_save(read_save(original)) == original` for every captured save (skip 77 if absent).
- Synthetic: `read_save(write_save(c))` equals `c`.
- Every truncation and random garbage buffer throws.

- [ ] **Step 3: Implement, pass, commit**

Commit: `save: original-compatible save files`.

---

### Task 7: Settings, pause and full game loop

**Files:**
- Create: `src/settings.hpp`, `src/settings.cpp`
- Modify: `src/game_main.cpp` (plan 04): front end → race → results → front end; pause menu in race

**Interfaces:**
- Produces: `struct Settings { Language language; bool metric; bool high_resolution; KeyBindings keys; int music_volume, sfx_volume; /* plus every registry value found */ };`, with `Settings load_settings(); void save_settings(const Settings&);` (simple `key=value` text file in the pref path).

- [ ] **Step 1:** List every registry value read or written (functions referencing the string at `0x004633e8`: `0x004020a0`, `0x00411c70`, `0x00412250`, `0x00412e00`, `0x00412f00`, `0x00413190`, `0x004132f0`, `0x004134c0`, `0x00416120`, `0x00416910`, `0x00416b40`, `0x00416cb0`, `0x004175e0`, `0x00417710`, `0x004227d0`, `0x00424080`, `0x004303f0`, `0x004454c0`, `0x004457f0`, `0x00445900`, `0x00445990`, `0x00448b90`, `0x004492ed`, `0x0040e710`) and map each to a `Settings` field.
- [ ] **Step 2:** A test quits the game (SDL quit event) during a race and checks that the saved career equals the pre-race career.
- [ ] **Step 3:** Play a Big Game from level 1 through qualifying at least one course. Save, quit, reload.
- [ ] **Step 4:** Update docs and commit: `game: complete front end, career and saves`.
