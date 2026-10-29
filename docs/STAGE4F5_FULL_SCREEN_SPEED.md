# Stage 4F.5 full-screen gameplay and targeted speed investigation

## Status

**PASS — isolated display-fit candidate built and host verified. CPU target remains
HOLD pending a separately controlled physical experiment.**

No COM port, BOOTSEL volume, removable drive, installed game, catalog, or
physical flash was accessed. No upload or removal command was issued. The
rejected direct-page/SRAM-table optimization is disabled.

## 1. Active-display-fit implementation

The LCD path now obtains the active rectangle from the live CRTC display
geometry. It removes only the reconstructed border outside that rectangle,
computes one uniform fit into 320x240, builds bounded source lookup tables on a
geometry transition, and uses nearest-neighbour sampling for presentation.

The implementation is split into a host-testable planner in `src/display_fit.*`
and the firmware presentation path in `src/main.cpp`. Invalid, overflowed,
outside-viewport, or implausibly small geometry fails closed to the historical
320x200 viewport at y=20. A mode transition clears pixels no longer covered;
there is no per-frame full-screen clear.

### Confirmed title geometry

| Title/state | CRTC active input | LCD output | Position | Treatment |
|---|---:|---:|---:|---|
| Manic Miner, Central Cavern | 256x184 at (32,0) | 320x230 | (0,5) | exact 5:4 uniform scale |
| Locomotive BASIC | 320x200 at (0,0) | 320x200 | (0,20) | established viewport retained |
| Knight Tyme menu | 256x200 at (32,0) | 307x240 | (6,0), 7 px right margin | uniform fit, integer rounding under one pixel |

For Manic Miner, destination x=0 and x=319 map to source x=32 and x=287;
destination y=0 and y=229 map to source y=0 and y=183. Tests prove every one
of the 256 gameplay columns and 184 gameplay rows is represented. The active
picture covers 73,600 of 76,800 LCD pixels, or 95.8%, without cropping gameplay
pixels. Only the identified 32-pixel CPC side borders and 16 bottom border rows
are removed.

Framebuffer evidence:

- `artifacts/stage4f5-manic-active-fit-320x230.png`
- `artifacts/stage4f5-manic-active-fit-320x230.ppm`

## 2. Gameplay compatibility

The integrated emulator test uses the actual snapshots and live CRTC state:

- Manic Miner still renders Central Cavern; Q moves left, W moves right, and
  bare Shift jumps;
- BASIC remains 320x200 and accepts `PRINT 2+2` plus a repeated command;
- Knight Tyme loads, its menu responds to Space, and its 256x200 active area
  fits to 307x240 without dropping its final source row or column;
- the reference engine remains compiled and selectable;
- 4 MHz emulated-time accounting and 12.5 Hz presentation cadence are unchanged.

The scaler is presentation-only. It does not alter the indexed CPC framebuffer,
CRTC state, Z80 cycles, snapshots, inputs, library entries, or storage formats.

## 3. Rendering overhead

Manic Miner output increases from 64,000 to 73,600 pixels per presented frame,
exactly 15%. The steady-state path adds one SRAM lookup for the source x and y
mapping; division occurs only when geometry changes.

The implementation-backed desktop evidence tool measured a five-run median of
43.268 microseconds for the 320x230 RGB mapping loop. A simplified 320x200 RGB
copy loop measured 18.758 microseconds, a 2.307x ratio, but that copy omits the
palette conversion and LCD work present in firmware and must not be treated as
an RP2350 prediction. The earlier like-for-like Stage 4F.3 palette prototype
measured 50.682 versus 36.642 microseconds, or 1.383x.

The physical lower bound from pixel count is approximately +15% presentation
work. Based on the previously measured roughly 2.09 ms rendering path, the
planning range remains about +0.3 to +0.8 ms per presented frame. Only the
candidate's `conv`, `lcdwait`, `render`, and total-work telemetry can establish
the RP2350 cost.

## 4. ARM Z80 performance findings

The physical FAST-DISPLAY baseline is 61.34% CPC speed, 28.35 ms Z80 and
32.55 ms total work per 20 ms CPC slice. Reaching 25 ms total requires removing
7.55 ms. Allowing for display-fit overhead, the Z80 path must fall to roughly
20.5 ms, a reduction near 27.7%. No host-only result supports claiming that
improvement yet.

### Candidate CPU path is unchanged

The display candidate compiles with:

- `CPCEXPLORER_FAST_RAM_ACCESS=OFF`;
- `CPCEXPLORER_FAST_Z80_SRAM=OFF`;
- `CPCEXPLORER_STAGE4F1_EXPERIMENT=OFF`;
- Arm GNU 14.3.1, Cortex-M33, Release `-O3`;
- 150 MHz configuration and presentation divisor 4.

Its `z80_execute_step` is 76,212 bytes. The extracted hot-code section SHA-256
is `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`,
exactly matching the extracted 4F.2-FAST-DISPLAY section. Thus the new UF2 is a
display-only experiment, not a disguised CPU change.

### Size-oriented compiler experiment

A separate ARM object-only experiment compiled the existing, non-direct-page
Z80 C translation unit with `-Os`; no experimental UF2 was produced.

| Metric | `-O3` baseline | `-Os` object | Change |
|---|---:|---:|---:|
| `z80_execute_step` | 76,212 B | 71,680 B | -4,532 B / -5.9% |
| Static instructions | 31,617 | 29,707 | -6.0% |
| Loads | 6,931 | 6,592 | -4.9% |
| Branches | 3,466 | 3,379 | -2.5% |

The full 12-test host suite passed under `-Os`, but five-run CPU medians were
slower than `-O3`: BASIC 3.926 versus 3.858 ms (+1.8%), Knight Tyme 5.017 versus
4.952 ms (+1.3%), and Manic Miner 4.773 versus 4.678 ms (+2.0%) per emulated
second. Desktop timing cannot resolve RP2350 XIP behaviour, but a 5.9% size
reduction is not credible evidence for the required 27-30% physical gain.
It is not enabled in the candidate.

### Smallest credible future CPU experiment

The remaining measured bottleneck is the 76,212-byte monolithic per-instruction
dispatcher executing from XIP through a 16 KiB cache. A narrowly scoped future
experiment should relocate **only** `z80_execute_step` to SRAM while leaving its
machine code, callbacks, cycle accounting, scheduler, display cadence, and
input semantics unchanged. Current static SRAM growth from display fit is only
1,320 bytes over 4F.2-FAST-DISPLAY, and the linked image retains roughly 200 KiB
between the minimum heap and stack region; a 76 KiB relocation is feasible on
paper but requires stack high-water and linker verification.

That experiment needs physical CPCPERF and RP2350 XIP access/hit deltas. The
earlier per-T-state SRAM test improved a different engine by only 13%, so no
30% claim is made. Batch execution and the rejected direct-page path are not
recommended: the batch core is larger and introduces timing risk, while the
direct-page path already regressed physically.

## 5. Host verification

The new isolated FAST-DISPLAY host build passed **12/12**:

1. CPC464 ROM/BASIC boot;
2. Knight Tyme regression;
3. launcher logic;
4. game-library parsing;
5. upload/removal installer and transactional recovery;
6. importer;
7. UF2/E10 policy;
8. native Manic Miner loader;
9. gameplay input;
10. performance telemetry;
11. active-display-fit geometry/mapping/fallback;
12. integrated fast engine, all three titles and reference-engine availability.

The separate `-Os` host build also passed 12/12. No CPU experiment was merged
into the display candidate.

## 6. Firmware candidate

- identity/version: `0.4.9-stage4f5activefit`;
- path: `build-rp2350-stage4f5-active-fit/cpc_explorer_active_fit.uf2`;
- SHA-256: `fe82c02a8c54817ff8c845da31ba1c8a879330d99e38250813c2ac7af3ddab23`;
- UF2 size: 685,056 bytes;
- raw firmware: 342,204 bytes;
- ELF `.text`: 219,048 bytes; `.rodata`: 110,840 bytes;
- SRAM sections: 272-byte vector table, 11,424-byte `.data`, 314,252-byte
  `.bss`, 2,048-byte minimum heap, and 2,048-byte stack reservation;
- Z80 hot path: 76,212 bytes in XIP, byte-identical to FAST-DISPLAY.

UF2 validation passed:

- 1,338 physical blocks;
- 1,337 ordinary RP2350 application blocks, family `0xE48BFF59`;
- application targets `0x10000000..0x100538FF`, wholly inside the first 4 MiB;
- exactly one authorized E10 block: family `0xE48BFF57`, flags `0x0000A000`,
  target `0x10FFFF00`, extension `0x9957E304`, 256 bytes of `0xEF`;
- no catalog, game-storage, unexpected-family, or other out-of-range target;
- firmware reservation margin: 3,852,100 bytes.

## 7. Preservation

Rollback hashes were recalculated unchanged:

- 4F.2 FAST-DISPLAY:
  `b97f60a8d282fecee587adaac8d800b9df0ef7190010fe9523afb357827e249b`;
- 4F.2 OPT-DISPLAY (rejected):
  `92bb9e921612f53d4cd886400b7336f62b9de8537d79cd9a0bbe232ae8c7d6cf`;
- 4E.2-FAST:
  `4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`;
- 4C.3-SRAM:
  `717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`.

The 16 MiB flash partition map, permanent E10 guard, USB protocol, transactional
catalog recovery, reference engine, snapshots, packages, ROM, and persistent
three-game library formats are unchanged. Existing firmware files were not
overwritten.

## 8. Recommended controlled physical test

Request separate approval for a single deployment of the display-only candidate.
Use the existing preflight and single-copy procedure, then verify:

1. menu, BASIC, Knight Tyme and Manic Miner launch normally;
2. Manic Miner occupies x=0..319 and y=5..234 with no missing gameplay edge;
3. Knight Tyme remains readable at 307x240 and BASIC remains 320x200;
4. collect at least 32 settled CPCPERF intervals in the same Central Cavern
   state, recording Z80, conversion, LCD wait, render, total work, speed and FPS;
5. confirm Z80 remains near the 28.35 ms FAST-DISPLAY baseline and quantify the
   display cost independently.

Do not combine that acceptance test with a CPU experiment. After display
acceptance, separately review an SRAM-only `z80_execute_step` diagnostic with
XIP counters. It is the smallest credible route to test locality, but physical
evidence—not desktop timing—must decide whether it can materially approach the
80% target.
