# Stage 4D fast-emulation prototype report

## Outcome

**HOLD.** The isolated instruction-oriented prototype reduces Z80 dispatches,
but the tested hybrid scheduler is neither cycle-equivalent nor materially
faster. It must not replace the deployed engine.

No firmware target references this prototype. No UF2 was built and no physical
device interface was accessed.

## Recommended architecture

The credible architecture is not a simple instruction core wrapped around the
current per-T-state gate-array loop. It requires three layers:

1. An instruction-oriented Z80 which exposes every memory/I/O machine cycle,
   its offset within the instruction, interrupt acknowledge, and injected WAIT
   states. Instruction boundaries alone are insufficient.
2. An absolute CPC T-state scheduler. Memory contention and WAIT sampling must
   be resolved at the exact machine-cycle phase; EI delay, HALT and interrupt
   acknowledge must remain CPU-core responsibilities.
3. Event/span-driven peripherals. CRTC and gate-array state should advance to
   the next edge (character, HSYNC, scanline, interrupt, or bus transaction),
   with video pixels emitted in spans. PPI and AY work should occur only on I/O
   events. A cycle-accurate slow path remains necessary around contended bus and
   edge cases.

Reusable components are the existing SNA reader/writer, CPC memory/ROM mapping,
keyboard matrix, PPI/AY register semantics, framebuffer format, and current
engine as a differential oracle. The existing `am40010_tick()` implementation
is useful as that oracle, but keeping it in the production hot path once per
T-state defeats most of the new architecture's purpose.

## Prototype

The standalone implementation is in `prototypes/stage4d`. It restores BASIC,
Knight Tyme and Manic Miner states into both engines. The alternative core is
stepped by instruction; memory and I/O callbacks include their M-cycle T-state
offset, while the adapter advances the existing CPC peripherals and attempts to
insert gate-array WAITs. It records CPU, RAM, framebuffer, gate-array, CRTC, PPI
and PSG hashes at 0, 1, 10, 100, 1,000, 10,000 and 50,000 instructions.

Manic Miner receives the same deterministic Q, W and bare-Shift hold/release
schedule in both engines at fixed instruction indices.

## Differential results

The corrected instruction-boundary run produced these results:

| State | CPU agreement | First overall difference | 50,000-instruction result |
|---|---:|---:|---|
| CPC464 BASIC | through 10,000 | instruction 1 (GA/CRTC phase) | CPU and RAM differ |
| Knight Tyme | through 50,000 | instruction 10,000 (GA/CRTC phase) | CPU still matches; RAM and peripherals differ |
| Manic Miner + deterministic input | through 1,000 | instruction 1 (GA/CRTC phase) | CPU, RAM, framebuffer and peripherals differ |

Examples of the accumulated timing error are BASIC 591,384 reference ticks vs
577,026 candidate ticks, Knight Tyme 556,835 vs 555,491, and Manic Miner
493,556 vs 480,070 at 50,000 instructions. These differences localize the
failure to initial peripheral phase, contention/WAIT accounting, and later
interrupt ordering rather than hiding it as a display-only mismatch.

## Host performance

Release Clang measurements on this desktop, for 50,000 instructions (median of
seven runs; the ratio range shows host scheduling/cache noise):

| State | Established engine | Prototype | Median ratio (range) |
|---|---:|---:|---:|
| BASIC | 4.42 ms | 3.65 ms | 1.21x (1.01–1.23x) |
| Knight Tyme | 3.86 ms | 3.29 ms | 1.18x (1.02–1.20x) |
| Manic Miner + input | 4.00 ms | 3.23 ms | 1.24x (1.23–1.26x) |

These are desktop microbenchmarks, not RP2350 FPS predictions. Dispatch count
did fall from roughly 0.49–0.59 million Z80 tick calls to 50,000 instruction
calls, but the adapter still performs roughly 0.48–0.58 million peripheral
ticks. That explains why dispatch reduction alone produced almost no speedup.

The current prototype therefore does not demonstrate a credible route from the
measured 16.9% physical speed to 90–100%. A sixfold gain would require the
event/span-driven gate-array/CRTC layer as well as the instruction CPU. That is
a new emulation architecture, not a surgical core swap, and would need physical
measurement after deterministic equivalence is achieved.

## Compatibility and licensing

The selected evaluation core is `cpu-z80` from `michalhucik/z80-mz800`, pinned
at revision `687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`, under the MIT licence. It
exposes M-cycle timing and wait injection and reports ZEXALL coverage upstream.
The prototype fetches it only for this isolated executable; no third-party code
was added to production firmware. Its GNU computed-goto implementation builds
with Clang/GCC, not MSVC.

Other reviewed candidates: `superzazu/z80` is MIT but lacks the needed M-cycle
interface; `redcode/Z80` is LGPL-3.0; `z80ex` is GPL-2.0. The latter licences
would require a separate distribution decision before embedded integration.
See `prototypes/stage4d/THIRD_PARTY_REVIEW.md` for the review record.

## Regression and preservation

A fresh Release host build passed all 10 tests: CPC464 ROM boot, Knight Tyme,
launcher, game library, installer/upload/removal, importer, UF2 policy, native
Manic Miner generation, gameplay input, and performance telemetry. The CPC ROM,
Knight Tyme SNA, native Manic Miner SNA/package, all existing UF2 files, flash
layout code, E10 policy, and installed-device state were not modified. The
MicroPython manifest was recalculated locally: 21/21 files match, zero failures.

## Minimal next experiment

Do not integrate this core yet. The smallest useful next experiment is a
host-only event scheduler for one CRTC scanline:

- precompute the exact CPC contention/WAIT phase from the current oracle;
- batch non-edge gate-array time into character/span events;
- fall back to individual T-states around bus operations and sync/interrupt
  edges;
- require exact CPU/RAM/peripheral/framebuffer equality over at least one frame
  for all three states and deterministic Manic Miner input;
- benchmark only after equality passes.

If that scanline experiment cannot remove most peripheral dispatches while
remaining bit-for-bit equivalent, Stage 4D should remain HOLD.
