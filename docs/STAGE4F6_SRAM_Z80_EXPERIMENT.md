# Stage 4F.6 SRAM Z80 execution experiment

## Result

**PASS for a separately approved physical A/B test.** The isolated candidate
relocates only `z80_execute_step`; it does not change that function's machine
code or enable the rejected direct-memory/SRAM-dispatch-table path. No COM port,
BOOTSEL volume, device flash, catalog, or installed game was accessed.

## Candidate

- identity: `0.4.10-stage4f6sramstep`
- UF2: `build-rp2350-stage4f6-sram-step/cpc_explorer_sram_step.uf2`
- SHA-256: `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`
- UF2 size: 685,056 bytes
- raw firmware: 342,204 bytes
- compiler: Arm GNU 14.3.1, Release `-O3`, Cortex-M33
- renderer: Stage 4F.5 active-display-fit, unchanged
- presentation divisor: 4 (12.5 Hz configured presentation cadence)

Both startup and `CPCU1 READY` use `PICO_PROGRAM_VERSION_STRING`. The candidate
ELF contains `0.4.10-stage4f6sramstep` and contains neither the stale
`4C.2-DIAG` string nor the Stage 4F.5 version string.

## Exact code-placement result

The final linker map places:

- `z80_execute_step`: `0x200011A0..0x20013B53`, 76,212 bytes, section
  `.time_critical.fast_z80_step`;
- the surviving per-step `base_dispatch`: `0x1003CF30`, 1,024 bytes in XIP;
- `z80_execute_batch`: unchanged source and not retained by the production
  link because it is unused, exactly as in Stage 4F.5;
- a call veneer: 8 bytes in XIP.

The rejected `CPU_Z80_RAM_FASTPATH` and `CPU_Z80_SRAM_DISPATCH_TABLE` options
are absent. Memory callbacks and CPC peripheral scheduling remain unchanged.

The unlinked Stage 4F.5 and Stage 4F.6 function sections are both exactly
76,212 bytes and have the identical SHA-256:
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.
This proves the experiment changes placement, not the generated routine.

## SRAM accounting and build gate

| Item | Bytes | Placement |
|---|---:|---|
| main SRAM | 524,288 | `0x20000000..0x2007FFFF` |
| RAM vector table | 272 | main SRAM |
| `.data`, including relocated step | 87,640 | main SRAM |
| `.bss` | 314,252 | main SRAM |
| minimum heap | 2,048 | main SRAM |
| free main SRAM after minimum heap | **120,052** | 117.24 KiB |
| core-0 stack | 2,048 | Scratch Y, `0x20081800..0x20081FFF` |
| core-1 stack | 0 | core 1 unused |
| Scratch X | 4,096 free | separate bank |
| unused lower Scratch Y | 2,048 | separate bank |

The relocation increases `.data` by 76,216 bytes: the 76,212-byte routine plus
four bytes of alignment. Existing allocations—including the 281,792-byte CPC
machine/indexed framebuffer, USB state, two 640-byte DMA scanlines, display-fit
maps, installer, telemetry, and Z80 tables—are already included in `.bss` and
`.data`. The only application heap allocations are the small fast-Z80 wrapper
and core objects; the 2 KiB minimum heap remains intact.

`tools/check_stage4f6_sram.py` is a mandatory post-link gate. It requires:

- exactly one 76,212-byte step section in main SRAM;
- no SRAM dispatch-table section;
- unchanged 2 KiB core-0 stack placement;
- at least a 2 KiB minimum heap; and
- at least **116 KiB (118,784 bytes)** free main SRAM after that heap.

The 116 KiB threshold leaves 1,268 bytes of deterministic link-layout tolerance
below this build while retaining the explicit heap, separate stack, and over
116 KiB unused main SRAM. The final link passed with 120,052 bytes.

## Host correctness

The isolated Clang Release host build enabled the same placement macro while
leaving execution options unchanged. The complete suite passed **12/12**:

1. CPC464 ROM/BASIC boot and keyboard execution;
2. Knight Tyme regression;
3. launcher controls;
4. game library;
5. USB installer/removal and transactional recovery;
6. importer;
7. UF2/E10 policy;
8. native Manic Miner load/gameplay;
9. gameplay button mapping;
10. CPCPERF aggregation;
11. active-display-fit geometry and bounds;
12. fast-engine integration, deterministic title behavior, and reference-engine availability.

## UF2 and flash safety

The independent validator accepted:

- 1,338 physical blocks;
- 1,337 ordinary RP2350 blocks, family `0xE48BFF59`;
- targets `0x10000000..0x100538FF`, entirely within the first 4 MiB;
- exactly one E10 compatibility block, family `0xE48BFF57`, flags
  `0x0000A000`, target `0x10FFFF00`, extension `0x9957E304`, and 256 bytes of
  `0xEF`;
- no catalog, game-storage, or unexpected target.

The raw application remains 342,204 bytes with 3,852,100 bytes of margin in the
4 MiB firmware reservation.

## Preservation

The seven original preserved UF2s rehash to their recorded values: BASIC,
Knight Tyme, Stage 3, Stage 4A, Stage 4A E10-safe, Stage 4B, and Stage 4B.1.
Stage 4C.1B, 4C.3-SRAM, 4E.2-FAST, 4F.2-FAST-DISPLAY, and the deployed Stage
4F.5 artifact also remain unchanged. In particular Stage 4F.5 remains
`fe82c02a8c54817ff8c845da31ba1c8a879330d99e38250813c2ac7af3ddab23`.

The CPC ROM, Knight Tyme snapshot, native Manic Miner snapshot/package, flash
layout, E10 guard, persistent library formats, reference engine, USB protocol,
and transactional removal are unchanged. The MicroPython backup revalidated
21/21.

## Separately approved physical procedure

1. Recheck the candidate SHA-256 and UF2 policy.
2. Identify the RP2350 BOOTSEL device by VID/PID, volume label, and
   `INFO_UF2.TXT`; copy the UF2 exactly once only after explicit approval.
3. Confirm application USB identity and that `detect/status/list` report
   firmware `0.4.10-stage4f6sramstep`, idle upload state, and the existing
   three entries.
4. Confirm the full-screen Manic Miner presentation remains unchanged.
5. Use the same Central Cavern state and controls as the 58-interval Stage 4F.5
   baseline. Capture at least 58 settled `CPCPERF` intervals.
6. Compare Z80 time first (baseline 29.10 ms), then total work (33.91 ms), CPC
   speed (58.89%), presentation FPS (7.37), rendering components, deadlines,
   and stability.
7. Do not upload/remove games or automatically reflash on an ambiguous result.

No physical speed improvement is claimed by this host-only build. Only the
physical A/B telemetry can determine whether moving the routine improves RP2350
execution locality.
