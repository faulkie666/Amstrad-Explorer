# Stage 4L.2 — CPC Explorer Z80 performance optimisation

Date: 31 October 2026

Mode: host development and firmware build only
Physical access: **not performed**

## Outcome

**PASS for an isolated physical A/B candidate.** The candidate removes one
redundant callback layer from every Z80 memory and I/O access, places the two
hot CPC memory callbacks in SRAM, and latches the CPC interrupt on its gate-array
edge instead of rewriting the same pending state before each instruction.

The change is correct and memory-safe, but it does **not** establish a route to
100% CPC speed by itself. Five-run host medians improved Fly Spy by only 0.66%.
RP2350 results may differ because the candidate removes XIP execution from the
memory callbacks, but no physical speed improvement is claimed without CPCPERF.

## Baseline and target

The Stage 4L.1 physical Fly Spy baseline was:

- effective CPC speed: 76.33%;
- total iteration: 26.148 ms average;
- Z80 CPU work: 21.946 ms average;
- other work: approximately 4.20 ms;
- presentation: 9.49 FPS;
- audio underruns: zero.

The CPC execution budget is 20 ms. Holding other costs constant requires the
Z80 portion to fall to about 15.8 ms, a reduction of approximately 28%.

## Root-cause findings

The compiled RP2350 baseline confirms that CPU interpretation dominates:

1. `z80_execute_step` is 76,212 bytes in SRAM. Every emulated instruction
   enters it separately, saves nine ARM registers, reserves 68 stack bytes,
   reloads callback state, executes one Z80 instruction, and returns.
2. Every opcode/operand memory access then traversed two indirect callback
   layers: the Z80 core called the adapter wrapper, which called the CPC memory
   handler. The baseline wrapper also loaded `op_tstate` and rearranged
   arguments even though the CPC handler does not consume that value.
3. The 1,024-byte computed-goto dispatch table remains in XIP at `0x10042D90`.
   The large interpreter body is already in SRAM, so wholesale code relocation
   is not available as another easy gain.
4. The fast loop used 64-bit absolute cycle accounting and returned to C++
   after every Z80 instruction. This is smaller than the interpreter cost but
   remains repeated work.
5. Presentation is not the primary bottleneck. Stage 4L.1 measured 2.223 ms
   average complete rendering and only 31 us average LCD DMA waiting, versus
   21.946 ms of Z80 CPU work.

The presentation divisor is still exactly four. The configured full-speed
presentation target is therefore 12.5 Hz. The measured 9.49 FPS is consistent
with 12.5 multiplied by the measured 76.33% CPC speed; presentation frequency
was not increased.

## Implemented changes

The experiment is isolated behind `CPCEXPLORER_STAGE4L2_Z80_CALLBACKS`.
Existing Stage 4K.7/0.4.28 builds retain their original source path.

### Direct callbacks

The Stage 4L.2 adapter gives the pinned Z80 core the canonical CPC callbacks
directly. The address, M1 state, value, and user pointer still follow the core's
native callback ABI. The handlers perform the same `mem_rd`, `mem_wr`, and I/O
transactions as before. No Z80 opcode implementation changed.

The old ARM forwarding functions were removed from this variant. The final
disassembly contains the direct memory handlers at:

- read: `0x20001280..0x20001297`, 24 bytes;
- write: `0x20001298..0x200012AF`, 24 bytes.

Both are in `.time_critical.fast_z80_mem`. `z80_execute_step` immediately
follows at `0x200012B0`, still exactly 76,212 bytes.

### Interrupt edge latch

The Z80 core's interrupt request is persistent until acknowledgement. The
candidate asserts it when the scheduled gate-array interrupt edge occurs and
once at the beginning of a run slice for a restored pending snapshot state.
It no longer calls the same idempotent setter before every instruction while
the line is already pending. Interrupt acknowledgement and gate-array clearing
are unchanged.

## Host performance

All values below are five-run medians on the same Windows host, compiler,
snapshots, presentation divisor, and test executable structure. These are not
RP2350 predictions.

| Workload, 1 emulated second | 0.4.28 path | Stage 4L.2 | Improvement |
|---|---:|---:|---:|
| Locomotive BASIC | 4.692 ms | 4.540 ms | 3.24% |
| Knight Tyme | 5.599 ms | 5.511 ms | 1.57% |
| Manic Miner | 6.164 ms | 5.995 ms | 2.74% |
| Fly Spy | 5.798 ms | 5.760 ms | **0.66%** |

The result varies by executed instruction and memory-access mix. It proves the
removed forwarding path had measurable cost, but the Fly Spy host result is far
below the approximately 28% Z80 reduction required by the physical target.

## Deterministic correctness

The baseline and candidate independently ran the diskless Fly Spy snapshot for
250 CPC frames (five emulated seconds). Every one of five runs produced the
same checkpoint in both builds:

- PC: `0x94C5`;
- CPC ticks: `20,000,003`;
- framebuffer FNV-1a-64: `b55852c6e10131d7`;
- peripheral dispatches per emulated second: `15,975`;
- reconstructed frames per emulated second: `12`.

The complete candidate host suite passed **26/26**. It covers CPC464 ROM boot,
BASIC input, Knight Tyme, Manic Miner movement/jump, Fly Spy snapshot reload,
launcher and packed library, upload/install/migration, Qw/ST input, display fit,
AY generation, real-time AY traces, Dizzy audio, flash-backup validation, and
UF2 policy.

## SRAM and firmware layout

| Item | Result |
|---|---:|
| `z80_execute_step` | `0x200012B0`, 76,212 bytes |
| Direct SRAM callbacks | `0x20001280`, 48 bytes |
| Main-SRAM headroom | **118,940 bytes** |
| Mandatory minimum | 118,784 bytes |
| Margin over gate | 156 bytes |
| Core-0 stack reservation | 2,048 bytes |
| Heap reservation | 2,048 bytes |
| Core-1 stack reservation | 2,048 bytes |
| ELF text / data / BSS | 372,180 / 3,264 / 316,852 bytes |
| Firmware binary | 371,348 bytes |

The fixed memory gate passes without relaxation. The margin is too small for
moving the 1 KiB dispatch table into main SRAM. The scratch-X gap is 832 bytes,
also too small for that table.

## UF2 candidate

- Identity: `0.4.29-stage4l2z80callbacks`
- File: `build-rp2350-stage4l2-z80-callbacks/cpc_explorer_z80_callbacks_exp.uf2`
- Size: 743,424 bytes
- SHA-256: `43ccf606648e6eef33daeb7332dda2224067873e7bef5b67e7f192daf23a6df2`

UF2 validation:

- 1,452 physical blocks;
- 1,451 RP2350 ARM-S application blocks, family `0xE48BFF59`;
- application targets `0x10000000..0x1005AAFF`, wholly inside the first 4 MiB;
- exactly one authorised E10 block at `0x10FFFF00`, family `0xE48BFF57`,
  flags `0x0000A000`, extension `0x9957E304`, and 256 bytes of `0xEF`;
- no catalogue, packed-library, upload-storage, or other unexpected targets.

The existing Stage 4K.7 candidate remains unchanged at SHA-256
`fe84bcd5c439a88ba030f28f48f842987876fe8ae88d768de6a97a2561b99dbd`.
No physical library or firmware was accessed.

## Ranked remaining opportunities

1. **Bounded multi-instruction SRAM execution spans — highest potential.**
   Amortising the large per-instruction prologue up to the next CRTC,
   interrupt, or frame event directly attacks the dominant repeated cost.
   It is not included because EI delay, HALT, interrupt acceptance, exact event
   overshoot, and the meaning of the instruction counter require a dedicated
   differential experiment before it can be deployment-safe.
2. **Smaller generated interpreter paths.** Splitting rare prefixed handlers
   from common opcodes could reduce pressure and prologue spills, but this is a
   larger change to the pinned third-party core and needs opcode-level tests.
3. **Dispatch-table locality.** A 1 KiB SRAM table may reduce XIP data traffic,
   but neither the 156-byte main-SRAM margin nor the 832-byte scratch-X gap can
   contain it under the current safety rules.
4. **Direct RAM page fast path — rejected for this candidate.** The earlier
   RP2350 OPT-DISPLAY experiment increased the interpreter by 43,104 bytes and
   physically reduced CPC speed from 60.46% to 52.43%, despite a desktop gain.
   It must not be reintroduced without a materially different design.
5. **Presentation changes — deferred.** Increasing presentation frequency
   would add work before the CPU meets its 20 ms budget and is explicitly out
   of scope for this stage.

## Expected physical result and next test

The candidate is ready for a separately approved physical A/B test, but the
expected gain is bounded and uncertain. The host evidence suggests a low
single-digit improvement; SRAM callback placement could help more on RP2350
than on desktop, while the extremely small Fly Spy host delta could also remain
small physically. It is not credible to predict 100% CPC speed from this build.

A physical test should flash this candidate once, retain presentation divisor
four, run the same Fly Spy scene for at least 60 seconds, and compare CPCPERF
Z80/loop values and CPCAUDIO counters directly with Stage 4L.1. The acceptance
question is whether the callback change produces a repeatable physical gain
without correctness or audio regressions. If it does not, the next useful work
is the bounded execution-span experiment, not further callback tuning.
