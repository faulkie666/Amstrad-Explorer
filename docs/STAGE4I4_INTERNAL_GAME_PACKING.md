# Stage 4I.4 — maximum internal game library

Date: 2026-10-30

Result: **PASS for deterministic host automation, two diskless snapshot
captures, compression selection, and the variable-length packing proof;
HOLD for Classic Invaders, the 50-game batch, and firmware integration.**

No COM port, BOOTSEL volume, RP2350, installed catalogue, or physical flash was
accessed. The original `D:` collection was read only. No firmware or UF2 was
built. `ExplorerReady/Arcade/A-Z` and `ExplorerReady/Simulation/A-Z` were
created, but contain no packages because the two newly captured games do not
yet have controls supported by the deployed firmware.

## 1. Host emulator installation and provenance

The official Windows x64 konCePCja v6.4.2 release was downloaded from
<https://github.com/ikari-pl/konCePCja/releases/tag/v6.4.2> and installed
without administrator privileges under the ignored project-local directory:

`artifacts/stage4i4-tools/koncepcja-v6.4.2-x64`

| Item | Verified value |
|---|---|
| release | `v6.4.2`, published 2026-10-07 |
| release ZIP SHA-256 | `ec78d28ba6c137ebd1b338c041e21561014e9a4a0f5d1535b5f53f9f94c2e415` |
| executable SHA-256 | `a864865b24c8a7046db13b0bfa6196e37482690ecded8ef874054c342bd57605` |
| reported build | `konCePCja v6.4.2-0c81d533a54a78962fc7f427446953569dd6366e` |
| official source archive SHA-256 | `cb9aff45bf92576a2c6413cad0f9f65cab057d4c718bb292e957ad09be173dd1` |
| machine required by harness | model 0, CPC464, 64 KiB, AMSDOS in ROM slot 7 |
| effective emulation tier | `faithful` |

The installed license permits personal, hobby, and internal tool use but does
not permit redistributing konCePCja or a fork. The binary, source archive,
configuration, and Python compression packages remain under ignored
`artifacts/`; none is part of the CPC Explorer repository.

## 2. Deterministic automation

`tools/stage4i4_koncepcja.py` starts the emulator, parses its startup manifest,
requires the CPC464/64K profile, discovers the loopback IPC port, and performs
all CPC interaction through the native IPC protocol. It uses:

- `autotype` for the authentic BASIC `RUN` command;
- `input key ... hold=N` and joystick transitions held for exact emulated
  frames;
- `wait vbl` for deterministic advancement;
- framebuffer screenshots, RAM/VRAM hashes, register inspection, YM recording,
  snapshot save/load, and disk eject commands;
- bounded command timeouts and one isolated emulator process per conversion.

No Windows clicking, mouse input, clipboard input, `SendKeys`, or timing-based
desktop automation is used. Pure headless operation works for machine control,
but v6.4.2 returns IPC error 503 for framebuffer screenshots without a renderer.
The harness therefore creates a hidden GUI rendering surface when screenshots
are required; input, timing, and capture remain native localhost IPC operations.

`tools/stage4i4_build_disk.py` uses konCePCja's disk commands to create scratch
DSKs from copied AMSDOS files while preserving their headers. The original
source files are never mounted writable. This eliminated the Stage 4I.2
keyboard-delivery problem: `N` and `1` are now accepted at the authentic
Classic Invaders prompts.

**Automation result: PASS.** The remaining Classic Invaders failure is no
longer a keyboard-automation failure.

## 3. First three game results

### Classic Invaders — HOLD

The independently constructed scratch DSK is 194,816 bytes with SHA-256
`95a4b4303a64b85afd092834d5e3bb8a7a00fe02462a3c54f82a63d5503b72fb`.
It contains the unchanged original files:

| File | Bytes | SHA-256 |
|---|---:|---|
| `INVADERS` | 512 | `4c1d92bf3a0e64e905633c4a75c8308fa73d782924983379529150f447894631` |
| `INVADERS.BIN` | 17,152 | `c68d35fc7bf597ae65d8c3b83eb0993317a1a1758e604288fe89738cf9258b31` |
| `INVADERS.SCR` | 5,376 | `6d1af5aad5b19d0f2d044279ddceb524e41601ff7d3c18c9634c118e461fa1ba` |

The harness ran `RUN"INVADERS"`, reached “Infinite lives (Y/N)?”, held `N`
for five frames, reached “Start on level (1-8)?”, and held `1` for ten frames.
The screen prompt is left correctly, so the old input blocker is fixed.

The real game still does not start. Both the Stage 4I.2 scratch disk and the new
independent disk return to BASIC before execution reaches `0x4200`. The saved
RAM at `0x1000` does not contain the 17,008-byte logical BIN payload: only 576
bytes match, the complete comparison is false, and the segment SHA-256
`2e6031e1bfbb3a9896cde5c25433a7d29168e885b9b01f334a1433f52b65eb67`
differs from the BIN payload SHA-256
`b65abd948c1a7f48c12d0b8e3e4d92f4ed3a0ae6befa67b5c0febaaadce6d2d3`.
The astronaut image is loader artwork, not gameplay. Captured SNA files from
that run are rejected as game candidates.

The exact blocker is an unresolved v6.4.2 CPC464/AMSDOS load-path failure for
the second loader file, not the CPC keyboard matrix. Bypassing or patching the
authentic loader is outside this stage.

### Fruity Frank — snapshot PASS, installability HOLD

The original `FRUITY` and `FRUITY.BIN` files remain unchanged. A 194,816-byte
scratch DSK with SHA-256
`e2a8cbb2d26d1f7fd83010505f5beebdadae7702611842dc46bd5ab356297ebf`
was booted with the authentic loader. The emulator reached the title, speed
selection, demo, and live level. A 65,792-byte CPC464 SNA was captured, the disk
was ejected, and the state continued for 20 seconds. Loading the SNA into a
fresh CPC464 instance reproduced the speed screen and active level.

| Artifact | SHA-256 / result |
|---|---|
| validated SNA | `5f1066f0ea7f8d83d5a92f1313b857a73d59b527705c9fc5cd4f66c25b1170db` |
| host `.cpcgame` | `93f69355a7ceccf59e1c61744de29bf4edc39c9e3e09871dea6063684662c28e` |
| importer | CPC464, 64 KiB, SNA v3, payload CRC `0a142089`, package CRC `453aec31` |

This is a valid diskless conversion, but not Explorer-ready. Its own control
screen requires Z for left, M for right, additional vertical/action keys, and
offers joystick control. The currently deployed generic profiles provide only
Q/W/Shift or Z/X/Space, so they cannot supply a complete playable mapping.

### Columns — snapshot PASS, installability HOLD

The unchanged six-file loader was placed on a 194,816-byte scratch DSK with
SHA-256
`3ceefd1b032d9e54d9c192dcb645ff0a012fbd7fbc7df66d935c2eae3cc4c757`.
The authentic loader reached its menu and an active timed game. The captured
65,792-byte CPC464 SNA continued for 20 seconds after disk ejection and restored
into a fresh CPC464 instance, where joystick fire reached an active game.

| Artifact | SHA-256 / result |
|---|---|
| validated SNA | `f1b7872aee6a1e86d5ded43e08a116f932983259223be78685d14c813d7ebf99` |
| host `.cpcgame` | `f4f2c672a338bd1035f2d68afc889b096f8a5cb63acb9af86fa3937c391cc824` |
| importer | CPC464, 64 KiB, SNA v3, payload CRC `ed2a9c02`, package CRC `ac952e27` |

The state is diskless and executable, but the proven start/control path uses
CPC joystick input. The physical firmware does not expose that profile, so the
package remains outside `ExplorerReady`.

### Gate decision and batch result

Two of the first three titles produced valid diskless snapshots. Classic
Invaders did not reach gameplay, and neither new snapshot has a complete
deployed control profile. The stated batch gate therefore did not pass.

**Batch conversion: 0 of 50 attempted.** This is intentional, not a hidden
failure count. Running 50 loaders before resolving the media and control gates
would create title-screen-only false positives.

## 4. Authentic AY findings

konCePCja generated standard YM5 register traces at 1 MHz PSG clock and 50 Hz.
The analysis counts actual AY register frames; it does not synthesize or add
music.

| Game/run | Duration | changing frames | volume-active frames | distinct nonzero tone periods | Classification |
|---|---:|---:|---:|---:|---|
| Classic prompt/loader | 15.0 s | 60 | 124 (16.53%) | 3 | isolated loader effects only |
| Classic failed start probe | 20.0 s | 0 | 0 | 0 | silent; no game reached |
| Fruity Frank capture | 60.0 s | 480 | 917 (30.57%) | 80 | intermittent game audio/effects |
| Fruity restored/input probe | 40.4 s | 736 | 1,512 (74.85%) | 52 | sustained AY sequences; music versus dense effects not yet isolated |
| Columns capture | 60.0 s | 2,999 | 3,000 (100%) | 71 | sustained game music |
| Columns restored/input probe | 40.4 s | 511 | 510 (25.25%) | 58 | music during the initial active interval, then silence after state transitions |

Columns is the strongest new AY regression candidate. Fruity Frank has
authentic, substantial PSG activity but needs a controlled state/listening
comparison before calling it continuous music rather than effects.

## 5. Compression measurements and decision

Four established 65,792-byte snapshots (Knight Tyme, Manic Miner, Dizzy, and
Chuckie Egg) were compressed twice, checked byte-for-byte for deterministic
output and lossless round-trip, then decoded for 100 desktop rounds. The Python
environment was 3.14.2 with lz4 4.4.5, heatshrink2 0.15.0, zstandard 0.25.0,
and zlib-ng 1.3.1.

| Codec | mean bytes | mean ratio | observed range | desktop median decode |
|---|---:|---:|---:|---:|
| LZ4 HC raw block | 28,284 | 42.99% | 12,021–41,202 | 7.6 us |
| heatshrink 11/4 | 31,019 | 47.15% | 17,368–41,578 | 352.8 us |
| deflate level 9 | 23,990 | 36.46% | 10,312–35,099 | 51.7 us |
| Zstandard level 3 | 25,707 | 39.07% | 10,839–37,604 | 31.6 us |

These are desktop timings, not RP2350 claims. Deflate gives the best mean size,
but its decoder and history/state integration are materially less attractive
with only 316 bytes above the SRAM gate. LZ4 is selected for the first firmware
experiment because it permits a bounded decoder with no history buffer and can
decode directly into the destination.

The ARM Cortex-M33 decoder probe was compiled with GCC 14.2.1, `-Os`, Thumb,
and function/data sections. Its measured object contribution is 194 bytes of
code, zero static data/BSS, and 24 bytes of static stack usage. This is not a
physical speed measurement. An RP2350 candidate must still measure decode time
before acceptance.

The SNA is deliberately split: its 256-byte machine-state header remains
uncompressed in flash and only the 65,536-byte RAM body is an LZ4 block. The
body therefore decodes directly into the existing CPC RAM. No impossible
65,792-byte temporary output buffer is required.

## 6. CPXPCK2 variable-length packing proof

`tools/stage4i4_pack_library.py` builds and verifies the host-only `CPXPCK2`
format. The six-snapshot proof image is 173,872 bytes with SHA-256
`e1ae02fe360b3dc9d8a9720bef1f03e3e6903fcb3892f4e6ac107e072c8cc5e5`.
Two independent builds were byte-identical.

Each 160-byte record stores a stable ID, descriptive title, Arcade/Simulation
category, A-Z key, CPC464/64K identity, codec and compatibility status, control
profile, offset/length, compressed and decoded CRC-32 values, and the original
source SHA-256. Payloads use 16-byte alignment rather than 4 KiB fixed slots.

The image contains two 4 KiB-aligned headers and two independent record-table
copies. Headers have generation numbers and CRCs; each points to its own
CRC-protected record table. Payload bounds and overlap are checked before any
decode, and every decoded SNA is revalidated. Host fault injection proved:

- corrupt header A: B is accepted;
- corrupt record table A: B is accepted;
- both record tables corrupt: rejected;
- payload-area corruption: rejected;
- repeated builds: identical SHA-256.

The builder/verifier additionally contains explicit fail-closed checks for
duplicate IDs, invalid metadata, out-of-bounds or overlapping payloads, and
decoded CRC mismatches.

This is a format proof, not an on-device installer. A physical bulk writer must
write payloads and both record tables first, verify readback, and publish the
two headers last. An interrupted pre-publication write then has no valid new
catalogue.

### Flash boundaries and rollback

The existing map remains unchanged:

| Flash offset | Purpose |
|---|---|
| `0x000000–0x3FFFFF` | application reservation |
| `0x400000–0x407FFF` | existing CPCLIB1 dual catalogues |
| `0x408000–0xFEFFFF` | game-data region, 12,484,608 bytes |
| `0xFF0000–0xFFFFFF` | permanent E10 guard |

The established history consumes three 69,632-byte allocations (the removed
USB test tombstone plus Manic Miner and Dizzy), so the conservative frozen
CPCLIB1 high-water is `0x43B000`; 12,275,712 bytes remain to `0xFEFFFF`.

Before any future CPXPCK2 installation, a separately approved transaction must
retain every CPCLIB1 descriptor but set its saved allocation high-water to the
end of the game region. Existing Stage 4H.4/4H.5D firmware already honors that
field: games remain readable, while old-firmware uploads fail with
`snapshot_region_full` instead of overwriting CPXPCK2. Rolling back merely to
play/list/remove existing games is therefore supportable. Rolling back and
uploading must remain prohibited unless that reservation transaction has been
durably published and verified.

No catalogue was changed in this stage.

## 7. SRAM and menu plan

The Stage 4H.5D linker result remains the baseline:

| Measurement | Bytes |
|---|---:|
| main-SRAM headroom | 119,100 |
| required minimum | 118,784 (116 KiB) |
| margin above gate | 316 |
| scratch-X AY control | 1,216 |
| scratch-Y core-1 stack | 2,048 |
| scratch-Y AY history | 2,048 |
| scratch-Y core-0 stack | 2,048 |

The proposed decoder adds 194 bytes in XIP flash, zero permanent SRAM, and a
24-byte call frame. Its input is the memory-mapped compressed block and its
output is the existing CPC RAM. The 256-byte SNA header is parsed in place.

The menu must not reproduce the current 192-entry `valid_indices_` model at
larger scale. It should keep only category, letter, selected record number, and
scroll position; binary-search or scan the sorted flash-resident records on
demand; and render titles directly from validated XIP records. No full title
list, decompression buffer, or second snapshot buffer is permitted. Temporary
state must reuse memory that is not live during gameplay.

**SRAM result: PASS for the host design and ARM object budget; firmware
integration remains gated on a new full linker map retaining at least 118,784
bytes of main-SRAM headroom and stack-watermark tests.** The safety gate is not
weakened.

## 8. Storage projection

The six-entry split-SNA proof produced compressed payloads of 12,193, 21,263,
22,373, 27,447, 32,801, and 41,356 bytes. Mean payload size is 26,239 bytes;
including a 160-byte record and typical 16-byte alignment, the expected packed
cost is about 26.4 KiB per game. The two record tables and headers are included
in the projections below.

| Projection | Size assumption | New compressed games fitting 12,275,712 bytes | Total menu entries including current four |
|---|---:|---:|---:|
| conservative | largest observed, 41,356 bytes | 294 | 298 |
| expected | six-game mean, 26,239 bytes | 461 | 465 |
| optimistic | mean of best three, 18,610 bytes | 647 | 651 |

These are storage ceilings, not compatibility forecasts. The sample is small,
and only valid gameplay-active captures count. Even the optimistic internal
estimate cannot hold all 1,694 source directories. At the observed expected
size, internal flash is likely to hold roughly 460 additional snapshots, not
the entire collection.

## 9. ExplorerReady and preservation

The requested 52 A-Z directories exist under `ExplorerReady/Arcade` and
`ExplorerReady/Simulation`. The tree is ignored by Git so copyrighted packages
cannot be committed accidentally. It currently contains zero packages:
Classic Invaders is not a game state, while Fruity Frank and Columns lack a
complete deployed control profile. Their validated host artifacts remain under
ignored `artifacts/stage4i4-internal-library`.

Preserved rollback hashes were recalculated:

| Artifact | SHA-256 | Result |
|---|---|---|
| Stage 4H.4 `cpc_explorer_ay_audio_continuous.uf2` | `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0` | unchanged |
| Stage 4H.5D `cpc_explorer_dual_core_ay_corrected.uf2` | `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35` | unchanged |

## 10. Host regression results

The current source tree rebuilt successfully. CTest completed 20 tests: 19
passed and `cpc_realtime_ay_prototype` failed reproducibly at sample 22 with
`mixed span mismatch ... span=0.000000000 direct=0.098313749`. Three retries
failed identically. The packing and conversion changes do not touch the AY
prototype, and no speculative audio fix was made, but the complete host suite
cannot be reported green while this pre-existing/current-tree regression is
present.

Separate Stage 4I.4 checks passed: snapshot import/inspection for Fruity Frank
and Columns, deterministic pack rebuild, LZ4 round-trip, A/B catalogue fallback,
corrupt-record rejection, corrupt-payload rejection, and Cortex-M33 decoder
size/stack compilation.

## 11. Recommended next implementation sequence

1. Resolve or explicitly baseline the `cpc_realtime_ay_prototype` regression.
2. Add generic joystick/directional control metadata sufficient for Columns
   and Fruity Frank; keep it independent of game IDs.
3. Integrate only the split-header/LZ4-RAM decoder and read-only CPXPCK2 parser
   in an isolated host build. Run the full suite and enforce the unchanged SRAM
   linker gate.
4. Add the flash-resident Arcade/Simulation/A-Z menu with on-demand record
   reads and no full-catalogue SRAM array.
5. Re-run the first-three gate. Keep Classic Invaders on HOLD but use three
   independently gameplay-verified titles before opening the bounded 40 Arcade
   plus 10 Simulation batch.
6. Only after host and linker acceptance, build a separately named firmware
   candidate. Physical catalogue reservation, bulk image writing, firmware
   deployment, and game installation each require new approval.

## Final status

- **AUTOMATION:** PASS
- **CLASSIC INVADERS:** HOLD — input fixed; second AMSDOS/BIN load absent
- **OTHER GAMES CONVERTED:** 2 host-valid diskless snapshots (Fruity Frank,
  Columns); 0 Explorer-ready because controls are incomplete
- **COMPRESSION METHOD:** split SNA, 256-byte header + LZ4-HC raw 64 KiB RAM block
- **AVERAGE PACKAGE SIZE:** 26,239-byte compressed payload; about 26.4 KiB packed
- **PROJECTED INTERNAL CAPACITY:** 294 conservative / 461 expected / 647
  optimistic additional games, subject to compatibility
- **FLASH FORMAT:** CPXPCK2 host proof, variable-length, dual header/table, no
  physical installation
- **SRAM SAFETY:** PASS for static host plan; full firmware linker gate pending
- **HOST TESTS:** HOLD — 19/20; deterministic AY prototype mismatch remains
- **DEVICE ACCESSED:** NO
- **DEPLOYMENT:** NOT AUTHORISED
