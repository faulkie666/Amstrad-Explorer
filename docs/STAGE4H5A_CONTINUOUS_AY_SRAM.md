# Stage 4H.5A — Continuous AY renderer and SRAM recovery

Date: 2026-10-30  
Status: **PROCEED to an isolated firmware-integration experiment; no deployable firmware was built**

## Scope and preservation

This is host-only work. No COM port, BOOTSEL volume, external flash, game catalogue, or installed game was accessed. The Stage 4H.4 rollback UF2 remains unchanged:

- `build-rp2350-stage4h4-continuous-audio/cpc_explorer_ay_audio_continuous.uf2`
- SHA-256 `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`

The link experiment deliberately emits an ELF and map only. Its build directory contains zero UF2 files.

## Renderer result

The prototype now uses a compact, span-driven AY state rather than calling the reference tick function one million times per second. It retains:

- three tone channels;
- noise LFSR and noise-period handling;
- tone/noise mixer masks;
- fixed and envelope-controlled channel volumes;
- envelope period, shape, hold, restart, and phase;
- oscillator and DC-filter phase across register updates;
- ordered PSG writes at AY-tick timestamps;
- audio-neutral but ordered register 14 events;
- explicit start, stop, and reset lifecycle;
- midpoint/silent output while stopped.

The span path and an independent per-tick path were compared sample by sample. The tests cover tone, noise, mixer changes, fixed volumes, envelope shapes and restarts, period changes, silence, disabled channels, and register 14. All tested 100% timebase streams were sample-exact.

A batching defect found during this work was corrected in the prototype: after a period is shortened below the existing counter value, a simple `(counter + events) / period` calculation can invent extra transitions. The new `advance_counter()` implementation preserves the reference tick ordering.

Producer-rate simulations at 68%, 80%, 100%, and 125% all maintained a continuous 22,050-sample/s consumer stream and the expected oscillator pitch. This does **not** make a slow game run in real time: at a 68% producer rate, a musical change scheduled after one emulated second arrives after about 1.47 wall-clock seconds.

## Event queue

`PsgEvent` is 8 bytes: 32-bit target AY tick, 16-bit sequence, register, and value. The SPSC queue uses one emulation producer and one audio consumer with acquire/release publication. It allocates no memory and has explicit start/stop/reset ownership.

| Ring | Object size | Usable entries | Knight Tyme observed peak | Margin |
|---|---:|---:|---:|---:|
| 64 | 528 B | 63 | 50 | 13 |
| 128 | 1,040 B | 127 | 50 | 77 |

Recommendation: **128 entries**. The 64-entry ring passes the captured trace, but its 13-entry margin is too small for interrupt latency and unobserved bursts. A full queue returns failure and increments an overrun counter; events are never silently overwritten or discarded. Production integration must stop/pause the producer or report a hard audio fault on that failure rather than continuing with a corrupted event stream.

A 100,000-event, two-thread stress test verified ordering and publication. Safe shutdown requires stopping the producer, observing the consumer epoch/acknowledgement, stopping DMA consumption, and only then resetting queue indices and AY state.

## Trace evidence

The bounded 128-entry queue replayed the captured game traces without an overrun:

| Trace | Writes applied | Samples | Non-zero samples | RMS | Queue peak | Overruns | Desktop render load |
|---|---:|---:|---:|---:|---:|---:|---:|
| Manic Miner | 7,436 / 7,436 | 1,947,262 | 1,860,168 | 0.063754 | 12 | 0 | 0.1266% |
| Knight Tyme | 9,488 / 9,488 | 1,947,262 | 1,246,828 | 0.103108 | 50 | 0 | 0.1172% |
| Dizzy | 58 / 58 | 1,947,262 | 3,602 | 0.000465 | 12 | 0 | 0.1259% |

Dizzy remains a mostly-silent reference. Manic Miner and Knight Tyme retain sustained output.

## Host performance

Five identical three-second runs produced these medians:

- span renderer: 3.867 ms;
- per-tick renderer: 39.749 ms;
- host speed ratio: 10.28x.

The game-trace replay cost was 0.117–0.127% of its represented audio duration on this desktop. These are desktop measurements only. They establish that the span design removes most AY tick dispatches, not that the RP2350 will have the same margin.

## Linker-proven SRAM plan

The Stage 4H.4-derived RP2350 link uses 512 KiB main SRAM plus two 4 KiB scratch banks. Before this experiment, scratch X was unused and the lower 2 KiB of scratch Y was unused; the upper 2 KiB of scratch Y holds the core-0 stack.

Main SRAM remains unchanged:

| Main SRAM item | Address | Size |
|---|---:|---:|
| `.data` | `0x20000110` | 88,168 B |
| `.bss` | `0x20015980` | 314,304 B |
| heap reservation | `0x20062540` | 2,048 B |
| remaining headroom | — | **119,488 B** |
| required safety gate | — | **118,784 B (116 KiB)** |
| gate margin | — | **704 B** |

The large existing allocations are unchanged: the CPC machine object remains 281,792 bytes (including CPC RAM, framebuffer, and peripheral state), the display line buffers remain 1,280 bytes, and the existing audio PCM queue remains 520 bytes.

The new scratch-bank plan is:

| Scratch allocation | Proven address/range | Size | Remaining bank space |
|---|---:|---:|---:|
| compact renderer, 128-event queue, synchronization and telemetry | `0x20080000..0x200804BF` | 1,216 B | 832 B before live core-1 stack |
| live core-1 stack | `0x20080800..0x20080FFF` | 2,048 B | — |
| AY DC-history buffer | `0x20081000..0x200817FF` | 2,048 B | 0 B before core-0 stack |
| live core-0 stack | `0x20081800..0x20081FFF` | 2,048 B | — |

The renderer/control allocation comprises a 1,184-byte renderer object plus 32 bytes reserved for producer/consumer epochs, shutdown state, timing telemetry, late-buffer count, and a 64-bit generated-sample counter. Its internal components include the 1,040-byte queue and 112-byte compact AY state. The 2,048-byte DC history is split out so it can occupy scratch Y.

Both stacks are explicitly 2,048 bytes. The Pico SDK linker symbols prove the live stack ranges; the `.stack1_dummy` input is only a size reservation and is not mistaken for the runtime stack address.

The automated gates report:

```text
stage4h5a SRAM probe: control=0x20080000+1216 core1_stack=0x200804c0+2048 dc=0x20081000+2048 scratch_x_free=832 scratch_y_free=0 main_headroom=119488 minimum=118784
stage4f6 SRAM gate: step=0x200011a0+76212 heap=2048 stack=2048 headroom=119488 minimum=118784
```

`core1_stack=0x200804c0` in the first line is the linker’s size-only `.stack1_dummy` section. The validated live stack is `__StackOneBottom=0x20080800` through `__StackOneTop=0x20081000`.

No framebuffer was reduced, the 76,212-byte SRAM Z80 routine was not changed, and the 116 KiB main-SRAM gate was not weakened.

## Regression result

All 20 host tests pass, including CPC464 ROM/BASIC, Knight Tyme, Manic Miner, launcher, gameplay controls, display fit, library installer, upload/removal, UF2 policy, audio, the new renderer, captured PSG traces, and fast-engine integration.

## What is and is not solved

This design can plausibly solve the first acceptance problem: uninterrupted PCM production at 22,050 samples/s with correct oscillator pitch. It cannot make note changes arrive at normal tempo while the Z80/game remains at 68% speed. Correct musical tempo still requires CPC emulation near real time or an explicitly non-authentic resynchronization policy, which is not proposed here.

The existing physical workload still needs roughly 9.4 ms removed from a 29.4 ms iteration to reach a 20 ms CPC slice. The renderer does not hide that deficit.

## Remaining gates before deployment

1. Integrate the proven scratch layout in a separately identified diagnostic firmware without changing the Stage 4H.4 rollback.
2. Run the renderer on core 1 with bounded DMA service and measure the real RP2350 render time, IRQ latency, queue high-water mark, and queue overruns.
3. Verify core-1 stack high-water usage; 2 KiB is linker-proven but still needs runtime watermark evidence.
4. Define and test snapshot/reset seeding so AY phase and pending events cannot cross a game transition.
5. Treat any queue overflow as a visible diagnostic failure; do not drop events.
6. A/B measure CPC speed, audio continuity, pitch, tempo, and audio/video drift against Stage 4H.4.

## Recommendation

**PROCEED** to an isolated, separately approved diagnostic firmware-integration experiment. The functional and memory feasibility gates now pass, and the unchanged main-SRAM safety margin is linker-proven. Physical acceptance remains pending because cross-core contention, stack watermark, DMA scheduling, and RP2350 execution cost cannot be established from desktop measurements.
