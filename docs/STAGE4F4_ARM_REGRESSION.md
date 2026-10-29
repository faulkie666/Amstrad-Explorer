# Stage 4F.4 RP2350 Z80 performance regression

## Status

**PASS for root-cause investigation; HOLD for any deployment.** The physical
regression is real and is isolated to the Z80 execution interval. The strongest
explanation supported by the linked ARM binaries is that the inlined 4 KiB
direct-page path increased the hot instruction working set, loads, and branches
enough to lose on the RP2350 XIP path. Moving only the computed-goto tables to
SRAM did not compensate for that expansion.

No COM port, BOOTSEL volume, removable drive, game library, catalog, or device
flash was accessed. No firmware was built and no UF2 was created or modified.

## 1. Physical result

| Measurement | 4E.2-FAST | 4F.2-OPT-DISPLAY | Change |
|---|---:|---:|---:|
| Telemetry intervals | 32 | 54 | -- |
| CPC speed | 60.46% | 52.43% | -13.3% |
| Z80 time / 20 ms CPC slice | 28.84 ms | 33.85 ms | +5.01 ms / +17.4% |
| Total work / slice | 33.02 ms | 38.09 ms | +5.07 ms / +15.4% |
| Presentation | 7.54 FPS | 6.54 FPS | -13.3% |

The 5.01 ms Z80 increase accounts for 98.8% of the 5.07 ms total-loop increase.
This confirms that display centring and presentation are not the cause of this
regression. Display scaling remains a separate Stage 4F.3 proposal and is not
present in either 4F.2 candidate.

## 2. Controlled build differences

The two Stage 4F.2 builds use the same Arm GNU 14.3.1 compiler, Cortex-M33
target, Release `-O3 -DNDEBUG` flags, fast engine, 150 MHz firmware clock
configuration, presentation divisor 4, diagnostics, snapshots, peripheral
scheduler, display centring correction, and application features.

The material compile-time differences are:

| Setting | FAST-DISPLAY | OPT-DISPLAY |
|---|---|---|
| `CPCEXPLORER_FAST_RAM_ACCESS` | off | on |
| `CPU_Z80_RAM_FASTPATH` | off | on |
| `CPCEXPLORER_FAST_Z80_SRAM` | off | on |
| `CPU_Z80_SRAM_DISPATCH_TABLE` | off | on |
| Firmware identity | `4F.2-FAST-DISPLAY` | `4F.2-OPT-DISPLAY` |

`run_us()` has the same 3,090-byte size in both 4F.2 ELFs and the same high-level
call structure: one `fast_z80_step()` per instruction boundary, the same
peripheral scheduling, interrupt path, timestamps, and presentation cadence.
There is no evidence of a CPU-clock, compiler-level, or scheduling change.

## 3. ARM code evidence

### Linked sizes and placement

| Item | FAST-DISPLAY | OPT-DISPLAY | Difference |
|---|---:|---:|---:|
| ELF `.text` | 218,272 B | 400,408 B | +182,136 B / +83.4% |
| ELF `.rodata` | 110,820 B | 109,796 B | -1,024 B |
| ELF `.data` | 11,224 B | 13,400 B | +2,176 B |
| `z80_execute_step` | 76,212 B at `0x10016368` | 119,316 B at `0x10038124` | +43,104 B / +56.6% |
| base dispatch tables | one 1,024 B table in XIP at `0x1004F5F0` | two 1,024 B tables in SRAM at `0x200011A0` and `0x200015A0` | +2,048 B SRAM |
| fast-Z80 allocation | 224 B | 360 B | +136 B |

The optimized build also retains a 139,468-byte `z80_execute_batch()` in XIP.
The application hot path does **not** call it: `fast_z80_step()` branches to
`z80_step()`, which calls `z80_execute_step(..., 1)`. The unused batch body is
therefore not direct per-instruction work, although it changes flash layout and
increases the image substantially.

The Pico SDK used by these builds defines the RP2350 XIP cache as 16 KiB. The
baseline 76,212-byte dispatcher is already larger than the cache; the optimized
119,316-byte dispatcher is 7.3 times the cache capacity. This does not prove a
specific miss rate, but it makes code locality a credible physical concern.

### Static instruction counts in the actual hot function

Counts below come from `arm-none-eabi-objdump` over the exact linked
`z80_execute_step` ranges:

| Static ARM/Thumb category | FAST-DISPLAY | OPT-DISPLAY | Change |
|---|---:|---:|---:|
| All instructions | 31,617 | 45,339 | +13,722 / +43.4% |
| Loads (`ldr*`) | 6,931 | 11,197 | +4,266 / +61.5% |
| Stores (`str*`) | 4,727 | 5,703 | +976 / +20.6% |
| Branch/call/table-branch instructions | 3,466 | 7,205 | +3,739 / +107.9% |
| Calls (`bl`, `blx`) | 1,032 | 1,032 | unchanged |
| Table branches (`tbb`, `tbh`) | 9 | 9 | unchanged |

These are static code-generation counts, not dynamic retired-instruction
counters. They nevertheless show why the desktop result did not transfer: the
optimization avoids many callback calls, but it pays for an inlined enabled
test, page-index calculation, table load, null-page branch, offset calculation,
optional data-bus-latch update, and fallback branch at every generated
RD/WR/FETCH site. `z80_fp_read()` and `z80_fp_write()` are `ALWAYS_INLINE`, so
that decision tree is replicated throughout the already large opcode body.

The unchanged call count also matters: the optimization did not make the
dispatcher generally shallower. It replaced a concentrated callback path with
more inline instructions and branches distributed through the XIP-resident
handlers.

### Page-table refresh work in the deployed OPT build

The immutable 4F.2 OPT-DISPLAY ELF contains calls to
`refresh_fast_memory_pages()` from both fast I/O write and fast I/O read paths,
in addition to state reload. The built firmware can therefore rebuild the page
mapping after I/O that did not change RAM/ROM banking. The later source-only
Stage 4F.3 gate avoids such refreshes unless ROM visibility, RAM configuration,
or upper-ROM selection changed; it is **not** in the deployed 4F.2 UF2.

That refresh work is timed inside the reported Z80 interval and is a confirmed
additional operation in OPT-DISPLAY. Its share of the physical 5.01 ms penalty
has not been measured. Desktop host medians improved by only 0.9% to 3.4% after
the gate, so refresh gating alone is not a credible explanation or complete
correction for the RP2350 regression.

## 4. Confirmed findings versus hypotheses

### Confirmed

1. The physical slowdown is in the Z80 interval, not LCD presentation.
2. Compiler version, `-O3` level, target CPU, scheduler structure, and
   presentation divisor are matched.
3. OPT-DISPLAY enables two changes together: direct-page access and SRAM
   dispatch tables.
4. The active per-step function grew 56.6%; its static instruction count grew
   43.4%, loads 61.5%, and branches 107.9%.
5. Only 2 KiB of dispatch-table data moved to SRAM; the 119,316-byte executable
   hot function remains in XIP.
6. The deployed optimized build performs unnecessary page-table refreshes on
   some ordinary I/O paths.
7. Both existing Stage 4F.2 host builds still pass all 11 regressions.

### Most likely, but requiring physical counters to prove

1. **Primary:** the expanded, branch-heavy direct-page code causes more XIP
   cache misses/refills and executes more Cortex-M33 instructions than the
   callback saving is worth. This best fits the linked code and the 17.4%
   physical Z80 regression.
2. Additional page-table refreshes contribute a smaller workload-dependent
   penalty.
3. Moving the dispatch tables to SRAM may be neutral or beneficial in
   isolation, but cannot recover the cost of the expanded instruction body.
   The current two-option comparison cannot assign an independent effect to it.
4. The retained batch core and changed function address may alter XIP cache-set
   conflicts or prefetch behaviour. Because the batch function is not called,
   this is secondary to the active step-function expansion.

The evidence does **not** justify blaming SRAM latency, changing Z80 semantics,
the display correction, an altered clock, or a peripheral-scheduling change.

## 5. Host verification

The immutable host builds were rerun during this investigation:

- `build-host-stage4f2-fast-display`: **11/11 passed** in 5.36 s;
- `build-host-stage4f2-opt-display`: **11/11 passed** in 5.23 s.

Coverage includes BASIC, Knight Tyme, Manic Miner, launcher/input, game library,
USB installer/removal and recovery, importer, UF2/E10 policy, diagnostics, and
fast-engine integration. These results establish functional equivalence for
the tested states, not RP2350 performance.

## 6. Smallest corrective experiment

Do not tune or rebuild first. Use the already-built **4F.2 FAST-DISPLAY** as the
control against the physically measured **4F.2 OPT-DISPLAY**. This is the best
available controlled physical A/B because both contain the same display
centring correction and firmware features; FAST-DISPLAY simply disables the
two experimental Z80 options.

For a separately approved test:

1. single-copy deploy the existing, hash-verified FAST-DISPLAY UF2;
2. use the same Manic Miner Central Cavern state and input conditions;
3. capture at least 32 settled `CPCPERF` intervals;
4. compare Z80, peripheral, reconstruction, LCD, total-work, effective-speed,
   FPS, deadline, and slice/instruction counts with the supplied OPT-DISPLAY
   capture;
5. do not combine this test with active-picture scaling or another CPU change.

If FAST-DISPLAY restores the 4E.2-class Z80 result, the smallest production
correction is to keep the display fix and revert the combined direct-page/SRAM
options. A later diagnostic matrix may test table-only and direct-page-only
builds, but those require new candidates and separate approval. The direct-page
variant should also include low-rate RP2350 XIP hit/access deltas and
direct/fallback/refresh counters so causality can be measured rather than
inferred.

## 7. Risks and required measurements

- The existing A/B isolates the combined optimization from the display change,
  but not direct pages from dispatch-table placement individually.
- Workload and game state affect instruction mix, memory-map fallbacks, and I/O
  frequency; use the same saved state and telemetry interval count.
- USB telemetry must remain low rate and non-blocking as in the existing builds.
- XIP counter sampling, if added later, must be interval-level rather than in
  the instruction loop.
- A firmware-only A/B still requires the established bootloader, UF2-policy,
  application re-enumeration, library-preservation, and physical acceptance
  procedure. None of those operations is authorized by this report.

No physical speed value is predicted. The existing FAST-DISPLAY binary is
expected to be the lower-risk candidate because its Z80 machine code is nearly
identical to the measured 4E.2 execution path, but only physical telemetry can
confirm the result.

## 8. Preservation and immutable identities

Hashes were recalculated after the investigation:

- 4F.2 OPT-DISPLAY, 1,049,600 bytes:
  `92bb9e921612f53d4cd886400b7336f62b9de8537d79cd9a0bbe232ae8c7d6cf`;
- 4F.2 FAST-DISPLAY, 683,008 bytes:
  `b97f60a8d282fecee587adaac8d800b9df0ef7190010fe9523afb357827e249b`;
- 4E.2-FAST rollback, 683,008 bytes:
  `4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`;
- 4C.3-SRAM rollback:
  `717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`;
- obsolete pre-display-fix 4F.1 artifact:
  `00862f3b55763532596606797ce923711131c41058b62a7ee5b48b0597b72da9`.

No existing firmware or game artifact was written. The protected flash layout,
E10 guard, installed games, reference engine, and MicroPython backup were not
accessed or modified by this host-only investigation.
