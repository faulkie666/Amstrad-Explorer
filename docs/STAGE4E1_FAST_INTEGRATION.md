# Stage 4E.1 fast CPC464 production integration

## Status

**PASS for the host-only integration candidate.** Firmware generation and
physical testing remain explicitly deferred to Stage 4E.2.

Starting Git identity: `1df23e66d16e0b447579ac4fa386bfcb2b9601c9` with the
existing accumulated uncommitted Stage 2-4E work. Stage 4E.1 was implemented
without resetting or overwriting that work.

## Engine selection and ownership

`cpcexplorer::EngineKind` provides explicit `Reference` and `Fast` selection.
The reference engine remains the default. A host or future firmware build must
enable the fast implementation with `CPCEXPLORER_ENABLE_FAST_ENGINE=ON`; a
future firmware can select it with `CPCEXPLORER_DEFAULT_ENGINE=FAST`. Selecting
FAST without compiling it is a configuration error.

Both paths share the existing CPC machine state, RAM, framebuffer, ROM mapping,
PPI/keyboard, AY registers, CRTC registers and gate-array memory/ink handling.
This avoids duplicating the 284 KiB CPC state. Launcher, library, USB protocol,
transactional removal, catalogs, flash backend and protected ranges are not
duplicated or modified.

The fast path uses:

- the pinned MIT cpu-z80 instruction core;
- an adapter for memory, I/O and interrupt acknowledgement;
- an absolute 4 MHz cycle budget that carries instruction overshoot into the
  next `run_us()` call;
- 52-scanline gate-array interrupt scheduling independent of host FPS;
- arithmetic CRTC scanline/VSYNC span advancement;
- direct standard CPC display reconstruction every fourth 50 Hz frame;
- the existing keyboard matrix and snapshot loader.

The LCD loop consumes a newly presented frame only when available. CPU
simulation still advances by 20 ms every loop; skipped presentations never
skip emulated time. The existing synchronous renderer and DMA buffer ownership
remain unchanged.

## Correctness results

The focused integration test proves:

- Reference is the deterministic default and remains selectable.
- 50 calls of `run_us(20000)` advance 4,000,000 CPC cycles, allowing only the
  final instruction-boundary overshoot.
- Interrupt and presentation schedules are independent of host duration.
- Twelve frames are reconstructed during one emulated second (12.5 Hz).
- Locomotive BASIC accepts `PRINT 2+2`, then a second command.
- Manic Miner loads Central Cavern; Q moves left, W moves right, bare Shift
  jumps, releases work and background changes remain localized.
- Knight Tyme loads and Space advances its menu. Its physical three-button
  gameplay limitation remains documented and is not disguised.
- Reinitializing after game execution produces a clean subsequent BASIC launch.
- Existing launch-button release suppression remains covered by the production
  gameplay-input regression.

The complete host suite passes 11/11: the existing ten production tests plus
the new fast-engine integration test.

## Release host benchmarks

Five one-second runs were used for the representative medians:

| Workload | Reference | Integrated fast | Speedup | Fast dispatches | Presented frames |
|---|---:|---:|---:|---:|---:|
| BASIC | 28.929 ms | 4.525 ms | 6.39x | 16,245 | 12 |
| Knight Tyme | 26.903 ms | 5.407 ms | 4.98x | 15,975 | 12 |
| Manic Miner | 31.550 ms | 5.714 ms | 5.52x | 15,975 | 12 |

The standalone Stage 4E Manic Miner result was 5.337x; integration therefore
retains rather than loses its useful advantage. These are desktop measurements,
not RP2350 speed claims.

An isolated five-second benchmark decomposition produced representative costs
per emulated second:

| Workload | CPU | Event scheduling | Reconstruction | Full fast path |
|---|---:|---:|---:|---:|
| BASIC | 4.35 ms | below benchmark resolution | 0.37 ms | 4.61 ms |
| Knight Tyme | 5.13 ms | 0.14 ms | 0.36 ms | 5.63 ms |
| Manic Miner | 4.74 ms | 0.82 ms | 0.16 ms | 5.72 ms |

The decomposition disables subsystems in separate runs, so small differences
include changed interrupt workload and timer noise; full-path measurements are
the acceptance values.

## Memory and code-size assessment

The CPC state is shared. Runtime diagnostic sizes are 284,416 bytes for the
reference path and 284,896 bytes for the fast path, an additional 480-byte
runtime allocation. The selected core's host object also contains 4,865 bytes
of BSS and the integration runtime contributes 80 bytes of initialized data,
for an estimated incremental SRAM requirement of about **5.4 KiB** including
the core allocation.

On Windows x86-64 COFF objects, the integration adds approximately 208.7 KiB
of text plus 9.6 KiB of read-only tables. This is not an RP2350 code-size
measurement. ARM code size and final flash/SRAM headroom must be measured in
Stage 4E.2 before any UF2 is accepted.

## Licensing and provenance

Selected dependency: `michalhucik/z80-mz800`, revision
`687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`, MIT, Copyright 2026 Michal
Hucik. CMake pins that exact revision. The upstream files are compiled without
source edits; the only build-local symbol adaptation renames its `z80_reset`
symbol to avoid collision with the retained chips reference core.

CPC Explorer additions are the C adapter and CPC-specific integration. The MIT
notice is preserved at `third_party/licenses/cpu-z80-MIT.txt`. No GPL or LGPL
source was imported.

## Known limitations

- The fast engine is for compatible CPC464 64 KiB snapshots; exact WAIT,
  contention, raster tricks and AY waveform timing are not promised.
- Fast-mode snapshot **loading** is tested. Snapshot creation remains restricted
  to the exact reference engine rather than emitting approximate peripheral
  state.
- Presentation is currently 12.5 Hz even when CPU simulation runs faster.
- Knight Tyme still needs controls unavailable on the three physical buttons.
- RP2350 code size, SRAM placement, XIP behavior and physical speed remain
  unmeasured because no firmware build was authorized.

## Preservation

The Stage 4C.3-SRAM UF2 remains
`717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`.
All recorded historical firmware checks remain unchanged, and the CPC ROM,
Knight Tyme snapshot, Manic Miner snapshot/package and MicroPython 21-file
manifest still match. No device, COM port, BOOTSEL volume, catalog or flash
storage was accessed.
