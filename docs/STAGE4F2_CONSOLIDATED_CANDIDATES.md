# Stage 4F.2 consolidated fast-engine candidates

## Status and recommendation

**PASS — recommend Variant B, OPT-DISPLAY, for the next separately approved
physical A/B test.** No COM port, BOOTSEL volume, removable drive, installed
game, catalog, upload transaction, or physical flash was accessed.

Both variants include the same reference-matched display centring correction,
12.5 Hz presentation, reference engine, gameplay controls, library formats,
USB install/removal, transactional recovery, flash layout, and E10 guard. The
only execution difference is that Variant B enables the reviewed Stage 4F.1
direct-page and SRAM dispatch-table options.

## Variant A — FAST-DISPLAY

- identity: `4F.2-FAST-DISPLAY`
- SDK version: `0.4.8-stage4f2fastdisplay`
- UF2: `build-rp2350-stage4f2-fast-display/cpc_explorer_fast_display.uf2`
- SHA-256: `b97f60a8d282fecee587adaac8d800b9df0ef7190010fe9523afb357827e249b`
- raw firmware: 341,208 bytes
- UF2 container: 683,008 bytes
- `.text`: 218,272 bytes; `.rodata`: 110,820 bytes
- SRAM: 272-byte vector table, 11,224-byte `.data`, 313,132-byte `.bss`,
  2,048-byte minimum heap, and 2,048-byte core-0 stack reservation
- total reserved SRAM: 328,724 bytes; main-SRAM headroom after minimum heap:
  197,588 bytes
- Z80 dispatcher: 76,212 bytes at `0x10016368` in XIP
- base dispatch table: 1,024 bytes at `0x1004F5F0` in XIP
- display correction: **YES**
- regression result: **11/11 passed**

Build provenance: isolated Release build, Arm GNU 14.3.1,
`LIBRARY_FAST`, fast engine selected, presentation divisor 4,
`FAST_RAM_ACCESS=OFF`, `FAST_Z80_SRAM=OFF`, and
`STAGE4F2_VARIANT=FAST_DISPLAY`.

## Variant B — OPT-DISPLAY

- identity: `4F.2-OPT-DISPLAY`
- SDK version: `0.4.8-stage4f2optdisplay`
- UF2: `build-rp2350-stage4f2-opt-display/cpc_explorer_opt_display.uf2`
- SHA-256: `92bb9e921612f53d4cd886400b7336f62b9de8537d79cd9a0bbe232ae8c7d6cf`
- raw firmware: 524,496 bytes
- UF2 container: 1,049,600 bytes
- `.text`: 400,408 bytes; `.rodata`: 109,796 bytes
- SRAM: 272-byte vector table, 13,400-byte `.data`, 313,132-byte `.bss`,
  2,048-byte minimum heap, and 2,048-byte core-0 stack reservation
- total reserved SRAM: 330,900 bytes; main-SRAM headroom after minimum heap:
  195,412 bytes
- Z80 dispatcher: 119,316 bytes at `0x10038124` in XIP
- two 1,024-byte base dispatch tables: `0x200011A0` and `0x200015A0`
  in SRAM
- display correction: **YES**
- regression result: **11/11 passed**

Build provenance: isolated Release build with the same compiler, startup,
engine, divisor, snapshots, and source as Variant A, with only
`FAST_RAM_ACCESS=ON`, `FAST_Z80_SRAM=ON`, and
`STAGE4F2_VARIANT=OPT_DISPLAY`. The Z80 source is pinned to commit
`687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`; its existing MIT notice is
preserved.

## Display and functional verification

Both isolated host builds passed the complete 11-test suite. The integrated
test verifies:

- Manic Miner non-border bounds `x=32..287`, active size 256×184, equal
  32-pixel CPC side borders, no stretching, Q/W movement, and bare-Shift jump;
- BASIC 320×200 presentation, `PRINT 2+2`, and a repeated command;
- Knight Tyme snapshot/menu and Space response;
- launcher behavior, upload/removal, transactional recovery, importer,
  flash boundaries, and E10 policy.

The correction computes the horizontal origin once per reconstructed frame;
it does not add work inside the pixel loop or alter Z80 execution.

## Five-run host benchmark

Runs alternated A then B on the same host. Both used Release Clang 20.1.8,
identical snapshots, one emulated second per result, identical controls, and
12.5 Hz presentation. Values are five-run medians and are desktop milliseconds
per emulated CPC second.

| Workload | FAST-DISPLAY | OPT-DISPLAY | OPT difference |
|---|---:|---:|---:|
| BASIC | 4.52 ms | 3.92 ms | 13.3% lower |
| Knight Tyme | 5.48 ms | 4.07 ms | 25.7% lower |
| Manic Miner | 5.69 ms | 5.12 ms | 10.0% lower |

Component-run medians (CPU / events / reconstruction / full):

- FAST-DISPLAY BASIC: 3.91 / 0.28 / 0.41 / 4.77 ms
- OPT-DISPLAY BASIC: 3.18 / 0.34 / 0.41 / 3.95 ms
- FAST-DISPLAY Knight Tyme: 5.00 / 0.17 / 0.33 / 5.49 ms
- OPT-DISPLAY Knight Tyme: 3.43 / 0.32 / 0.34 / 4.09 ms
- FAST-DISPLAY Manic Miner: 4.68 / 0.75 / 0.28 / 5.70 ms
- OPT-DISPLAY Manic Miner: 3.20 / 1.57 / 0.30 / 5.07 ms

Component values come from separate benchmark modes, so independently rounded
medians need not sum exactly. They are host comparisons, not RP2350 speed or
FPS predictions.

## UF2 and E10 validation

Variant A contains 1,334 physical blocks: 1,333 ordinary family `0xE48BFF59`
blocks targeting `0x10000000..0x100534FF`, plus exactly one authorized E10
block. Variant B contains 2,050 physical blocks: 2,049 ordinary family
`0xE48BFF59` blocks targeting `0x10000000..0x100800FF`, plus exactly one
authorized E10 block.

For both, the E10 block is family `0xE48BFF57`, flags `0x0000A000`, target
`0x10FFFF00`, extension `0x9957E304`, and 256 bytes of `0xEF`. No catalog,
game-storage, unknown-family, or unexpected out-of-reservation target exists.

## Preservation

- deployed 4E.2-FAST remains
  `4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`;
- 4C.3-SRAM rollback remains
  `717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`;
- the obsolete pre-display-fix 4F.1 UF2 remains
  `00862f3b55763532596606797ce923711131c41058b62a7ee5b48b0597b72da9`
  and must not be deployed;
- the MicroPython backup remains 21/21 valid.

## Physical-test recommendation

Choose **OPT-DISPLAY**. It passed the same correctness and display tests and was
faster in all three five-run host medians while consuming only 2,176 additional
static SRAM bytes. The next test should be a separately approved, single-copy
deployment followed by the established Manic Miner `CPCPERF` capture and visual
confirmation of equal side borders. No physical improvement should be claimed
until that test is complete.
