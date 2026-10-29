# Stage 4F.5 identity discrepancy and Stage 4F.6 SRAM plan

## Status

**Identity root cause confirmed and corrected in source only. Stage 4F.6 is
feasible on the linked memory budget, but remains unbuilt and physically
unverified.**

No firmware was rebuilt. No COM port, BOOTSEL volume, physical flash, game
library, or installed game was accessed. The deployed Stage 4F.5 UF2 remains
unchanged at SHA-256
`fe82c02a8c54817ff8c845da31ba1c8a879330d99e38250813c2ac7af3ddab23`.

## 1. Why USB reports 4C.2-DIAG

The deployed binary contains two independent identities:

1. Pico SDK binary metadata correctly reports
   `0.4.9-stage4f5activefit`. `picotool info -a` reads that exact value from
   the UF2.
2. The CPCU1 USB `HELLO` response did not use the Pico version. It selected a
   second hard-coded string in `src/main.cpp` through a preprocessor ladder.

Stage 4F.5 defines `CPCEXPLORER_ACTIVE_DISPLAY_FIT` and
`CPCEXPLORER_PERF_DIAGNOSTICS`. The old identity ladder had branches for 4F.2,
4F.1, 4E.2 and 4C.3, but no Stage 4F.5 branch. It therefore reached the generic
`CPCEXPLORER_PERF_DIAGNOSTICS` branch and emitted `4C.2-DIAG`.

This exactly explains the physical evidence: the active-display-fit macro
selects the new renderer independently, while the protocol prints the stale
literal. It is an identity-reporting defect, not evidence that old firmware or
the wrong emulator engine booted.

The candidate ELF itself contains both strings:

- correct binary metadata: `0.4.9-stage4f5activefit`;
- stale CPCU1 literal: `4C.2-DIAG`.

## 2. Source-only correction

The duplicated identity ladders have been removed from the startup message and
CPCU1 `READY` response. Both now use `PICO_PROGRAM_VERSION_STRING`, the same
value supplied by `pico_set_program_version()` and embedded in binary metadata.

This makes the CMake version the single source of truth for both offline UF2
inspection and runtime USB detection. It also prevents later diagnostic feature
macros from silently overriding a newer firmware identity.

Per instruction, this correction was **not compiled or rebuilt**. The currently
deployed Stage 4F.5 firmware will continue reporting `4C.2-DIAG`; a future,
separately approved firmware such as Stage 4F.6 will include the correction and
must be checked for matching picotool and CPCU1 versions before deployment.

## 3. Actual Stage 4F.5 SRAM map

The linked Stage 4F.5 ELF uses these RP2350 regions:

| Region | Address | Capacity | Current use |
|---|---:|---:|---:|
| Main SRAM | `0x20000000..0x2007FFFF` | 524,288 B | data, BSS, heap |
| Scratch X | `0x20080000..0x20080FFF` | 4,096 B | no linked data/core-1 stack |
| Scratch Y | `0x20081000..0x20081FFF` | 4,096 B | 2,048 B core-0 stack reservation |

Main-SRAM sections are:

- vector table: 272 B;
- `.data`: 11,424 B;
- `.bss`: 314,252 B;
- end of BSS: `0x2004F94C`, 325,964 B from main-SRAM origin;
- minimum heap: 2,048 B;
- free main SRAM after the minimum heap: **196,276 B**;
- core-0 stack: 2,048 B in Scratch Y, independent of main SRAM;
- core-1 stack: no linked reservation because the firmware does not launch
  core 1.

The heap can grow from the end of the minimum heap toward `0x20080000`. The
existing fast-Z80 allocation reports 224 B plus allocator overhead. A physical
stack high-water measurement has not been made, but the stack resides in a
separate scratch bank and does not reduce the 196,276-byte main-RAM figure.

## 4. Existing large allocations

The major linked SRAM consumers are:

| Allocation | Bytes | Notes |
|---|---:|---|
| CPC machine | 281,792 | includes 768x272 indexed framebuffer, 64 KiB CPC RAM and peripheral state |
| transactional installer | 16,788 | static; preserves library recovery |
| DAA table | 4,096 | Z80 helper table in SRAM |
| display line buffers | 1,280 | two 320-pixel RGB565 DMA source lines |
| display source-X map | 640 | Stage 4F.5 active-fit lookup |
| display source-Y map | 480 | Stage 4F.5 active-fit lookup |
| USB telemetry | 640 | static bounded output buffer |
| protocol decoder | 1,052 | static upload decoder |
| memory junk/unmapped pages | 2,048 | two 1 KiB pages |

The LCD driver claims one DMA channel and one PIO state machine. DMA reads from
the existing double line buffers; it does not allocate a full RGB framebuffer
or a hidden heap buffer. TinyUSB endpoint and protocol buffers are already in
the linked BSS totals above.

## 5. Stage 4F.6 single-function SRAM feasibility

The exact existing `z80_execute_step` is 76,212 bytes (74.43 KiB). Moving only
that function into a Pico SDK `.time_critical.fast_z80_step` section would
consume approximately 76,212 additional main-SRAM bytes, plus small alignment
or veneer overhead.

Predicted main-SRAM result before an actual link:

- current free after minimum heap: 196,276 B;
- Z80 routine: 76,212 B;
- predicted free after relocation and minimum heap: **120,064 B**;
- conservative planning value after alignment/veneers: about 119 KiB.

This is sufficient for a controlled experiment without moving the framebuffer,
DMA lines, USB buffers, heap, or stack. It is not permission to consume the
remaining space casually; the actual map and stack high-water checks remain
acceptance gates.

The flash image should remain close to its current 342,204 bytes. A Pico
time-critical function has a flash load image that startup copies into SRAM;
the routine is not expected to occupy both the normal XIP text section and the
RAM execution section. Even substantial alignment overhead remains far inside
the 4 MiB application reservation. The game-library regions and E10 guard do
not move.

## 6. Required implementation and linker proof

The existing pinned-core patch must gain a new, opt-in definition such as
`CPU_Z80_SRAM_EXECUTE_STEP`. It must decorate only the second generated core,
`z80_execute_step`, with
`section(".time_critical.fast_z80_step")`:

- `z80_execute_batch` remains in XIP and should continue to be garbage-collected
  when unused;
- `z80_step`, adapter callbacks, I/O handlers and the dispatch table remain in
  their baseline locations;
- direct 4 KiB memory pages and SRAM dispatch tables remain disabled;
- function source, compiler optimization, cycle accounting and instruction
  logic remain unchanged;
- Stage 4F.5 active-display-fit remains unchanged.

An actual Stage 4F.6 link must prove:

1. `z80_execute_step` has an execution address in main SRAM (`0x20000000` range),
   not merely a source annotation;
2. the map contains exactly one 76,212-byte time-critical step body;
3. no batch core or unrelated emulator function was pulled into SRAM;
4. the dispatch table remains the baseline XIP table;
5. main-SRAM free space after the minimum heap remains at least 118 KiB;
6. the 2 KiB core-0 stack and 2 KiB unused half of Scratch Y remain unchanged;
7. DMA buffers, CPC framebuffer, USB buffers and installer addresses do not
   overlap the relocated section;
8. flash stays inside the first 4 MiB and the sole out-of-range UF2 block is
   the exact E10 compatibility block.

## 7. Correctness gates before any physical test

Before producing a deployment candidate, run the complete 12-test suite for:

- CPC464 BASIC boot, `PRINT 2+2`, and repeated commands;
- Knight Tyme snapshot/menu/Space response;
- Manic Miner Central Cavern, Q/W movement and Shift jump;
- accepted 320x230 active-display fit and fallback geometry;
- reference-engine availability;
- launcher/input;
- game-library parsing and snapshot loading;
- USB upload/removal and transactional recovery;
- importer and UF2/E10 policy;
- telemetry accounting and 4 MHz emulated cycles.

Also compare deterministic CPU registers, RAM hashes, framebuffer hashes,
interrupt state, and instruction/cycle totals between the XIP and SRAM builds.
Relocation must not change the hot-section source or semantics.

## 8. Required physical baselines and recommendation

There is currently no measured Stage 4F.5 CPCPERF data. Before attributing any
change to SRAM, capture at least 32 settled intervals from the deployed
full-screen firmware in the same Manic Miner Central Cavern state. The stale
`fw=4C.2-DIAG` text can be documented as this confirmed reporting defect; it
does not invalidate the telemetry.

Then, after separate build review and deployment approval, compare a uniquely
identified Stage 4F.6 SRAM-only candidate against that Stage 4F.5 capture. Keep
presentation at 12.5 Hz and record:

- Z80 average/maximum;
- total work and loop duration;
- effective CPC speed and presentation FPS;
- conversion, LCD wait and render time;
- missed deadlines and emulated slices;
- bounded RP2350 XIP access/hit deltas around the Z80 interval if feasible.

The physical target remains about 25 ms total work. Relocating 76 KiB removes
instruction fetches for the monolithic Z80 routine from the 16 KiB XIP cache,
so it is technically credible. It is not guaranteed to provide the required
approximately 30% gameplay improvement: the earlier SRAM experiment used a
different core and achieved about 13%. Stage 4F.6 should therefore be treated
as a single controlled performance experiment, not a promised final fix.
