# Stage 4H.5B — Dual-core AY audio integration

Date: 2026-10-30  
Identity: `0.4.16-stage4h5bayaudioexp`  
Status: experimental build complete; deployment not authorized

## Gate summary

| Gate | Result | Qualification |
|---|---|---|
| Compilation and linker | **PASS** | Final RP2350 ELF and UF2 built; all memory gates passed. |
| Host functional equivalence | **PASS** | 20/20 host tests passed, including sample-exact AY comparisons and all game/storage regressions. |
| Queue and event timebase safety | **PASS (host)** | 128-entry SPSC ordering, overflow, sequence-gap, reset and 68/80/100/125% timing tests passed. |
| Core-1 lifecycle and stack safety | **PASS (static)** | Bounded start/stop and exact live stack placement are implemented. Runtime stack high-water evidence is **not tested**. |
| Flash safety | **PASS for the current control flow** | All storage writes finish before core 1 is launched; SDK lockout victim initialization is also enabled. See restrictions below. |
| UF2 address safety | **PASS** | 1,387 application blocks plus exactly one recognized E10 block; no library/catalog targets. |
| Physical performance | **NOT TESTED** | No COM, BOOTSEL or device access occurred. |
| Deployment | **NOT AUTHORIZED** | The UF2 is an experimental candidate only. |

## Experimental artifact

- UF2: `build-rp2350-stage4h5b-dual-core-ay/cpc_explorer_dual_core_ay_exp.uf2`
- size: 710,656 bytes
- SHA-256: `ff7b3ccbae73d8e7bce563921e9318f5c06c9ccae420b743a4158a96f2e4897a`
- raw firmware binary: 354,884 bytes
- application reservation remaining: 3,839,420 bytes
- Pico SDK: 2.2.0
- target: Pimoroni Explorer RP2350, ARM Secure image

The Stage 4H.4 rollback remains byte-for-byte unchanged:

- `build-rp2350-stage4h4-continuous-audio/cpc_explorer_ay_audio_continuous.uf2`
- SHA-256 `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`

## Architecture

Core 0 retains CPC emulation, input, display, USB protocol and catalogue ownership. The fast CPC I/O path reports each actual AY data write, including repeated envelope-shape writes and register 14 traffic, with a 1 MHz AY timestamp derived from the 4 MHz CPC cycle count.

Core 1 exclusively owns:

- the compact span-driven AY state;
- the 128-entry SPSC event consumer;
- 22,050 Hz sample generation;
- both PWM/DMA PCM buffers;
- `DMA_IRQ_1` handler installation and NVIC enable;
- amplifier start, underrun policy and shutdown.

The LCD claims a separate DMA channel and does not install a DMA interrupt handler. Core 0 never manipulates the audio DMA queue. Cross-core telemetry uses atomic publication; USB output remains on core 0 and cannot block synthesis.

The old synchronous CPC PCM callback is not registered in this build. `set_external_ay_renderer(true)` disables fast-engine sample generation while preserving the CPC-owned PSG registers and I/O semantics. Thus only one active renderer exists.

## Timebase semantics

Events retain their original emulated AY tick. The consumer advances at wall-clock audio speed:

- 100% producer: writes arrive at their target;
- 68% or 80% producer: writes arrive late and apply immediately;
- 125% producer: writes arrive early and wait until their emulated target.

This preserves oscillator pitch. It does not fabricate musical tempo: at 68% CPC speed a write scheduled after one emulated second arrives after about 1.47 wall-clock seconds. Continuous audio and correct pitch are separate from correct tempo and full-speed gameplay.

Register 14 events are consumed in sequence but remain audio-neutral. Same-value register 13 writes still restart the envelope. The period-shortening batching correction from Stage 4H.5A is retained.

## Queue and lifecycle

The queue uses release publication by core 0 and acquire consumption by core 1. A full queue returns failure, increments both the queue overrun counter and published `qfull` telemetry, and never overwrites an unread event.

Startup is bounded:

1. CPC state or snapshot is initialized on core 0.
2. All 16 current AY registers are copied.
3. The stopped renderer clears oscillator, noise, envelope, filter and queue state.
4. Register seed events are inserted at tick zero.
5. Core 1 is launched and registers with `flash_safe_execute_core_init()`.
6. PWM is placed at midpoint, GPIO13 is enabled, and both DMA buffers are primed.
7. The state changes to running only after audio initialization succeeds.

Shutdown removes the producer callback, requests a bounded stop, turns off GPIO13, aborts/unclaims audio DMA, returns PWM to midpoint, and only then acknowledges `Stopped`. Restart uses a new epoch and a cleared queue/state, preventing stale audio between BASIC, Knight Tyme, Manic Miner and Dizzy.

Any startup failure enters an explicit `Failed` state. Audio initialization failure invokes no fallback renderer and cannot leave an active amplifier session.

## Final SRAM proof

| Allocation | Final range | Size |
|---|---:|---:|
| renderer, queue and lifecycle control | `0x20080000..0x200804BF` | 1,216 B |
| unused gap | `0x200804C0..0x200807FF` | 832 B |
| live core-1 stack | `0x20080800..0x20080FFF` | 2,048 B |
| AY DC history | `0x20081000..0x200817FF` | 2,048 B |
| live core-0 stack | `0x20081800..0x20081FFF` | 2,048 B |

The SDK default core-1 stack is disabled with `PICO_CORE1_STACK_SIZE=0`. A single custom `.stack1.stage4h5b` input establishes the 2 KiB linker reservation, and the disassembly proves a call to `multicore_launch_core1_with_stack` with `__StackOneBottom` and 2,048 bytes. This is not inferred merely from linker symbols.

Main SRAM:

- `.data`: 88,408 bytes;
- `.bss`: 314,436 bytes;
- heap reservation: 2,048 bytes;
- main-SRAM headroom: **119,100 bytes**;
- required gate: 118,784 bytes (116 KiB);
- remaining gate margin: **316 bytes**.

The margin is small but passes the unchanged gate. No framebuffer or display buffer was reduced.

The core-1 live stack is filled with a guard pattern before launch. Core 1 periodically scans the untouched lower guard and publishes `stack_high`. This enables physical high-water measurement, but the linker result alone is not a runtime stack-safety claim.

## Z80 preservation

`z80_execute_step` remains 76,212 bytes. The final unlinked function-section SHA-256 is:

`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`

This exactly matches the Stage 4H.4/4F.6 baseline. The rejected direct-memory and SRAM-dispatch-table paths remain disabled.

## Flash-safety audit

All application erase/program calls remain confined to `pico_flash_backend.cpp`, which still validates the protected library address range and uses `flash_safe_execute`. No storage safeguard or flash layout was changed.

The current firmware control flow provides a stronger boundary:

- JEDEC probe and pending-removal recovery execute before the launcher starts;
- upload, commit, abort and removal execute only in the launcher;
- core 1 is launched only after a selected game has loaded;
- `run_emulator()` does not return to the launcher.

Therefore no application flash erase/program path is reachable while the audio core is active. Core 1 additionally calls the Pico SDK 2.2.0 `flash_safe_execute_core_init()`, enabling its SRAM multicore-lockout victim handler.

STOP condition: any future feature that returns from a running game to the launcher must first complete the bounded audio stop handshake before permitting probe, recovery, upload or removal. The present candidate must not be reused with a new storage path without repeating this audit.

## DMA and interrupt ownership

- audio DMA channel: claimed by core 1;
- audio IRQ: `DMA_IRQ_1`, exclusive handler installed/enabled on core 1;
- PCM queue: core 1 plus its own DMA ISR only;
- display DMA: independently claimed, polling-based, no `DMA_IRQ_1` registration;
- amplifier: enabled after midpoint setup and held through the audio session;
- genuine underrun: PWM returns to midpoint; amplifier remains enabled until explicit shutdown;
- shutdown/failure: amplifier disabled and DMA aborted before acknowledgement.

## Telemetry for physical acceptance

Core 0 emits the existing CPCPERF line and a bounded, nonblocking CPCAUDIO line approximately once per second when USB diagnostics are enabled:

```text
CPCAUDIO mode=core1 state=... produced=... consumed=... seqerr=...
qpeak=... qfull=... late=... pcm=... played=... underruns=...
overruns=... render_p95_us=... render_max_us=... deadline_miss=...
stack_high=...
```

The render percentile is a bounded 16-bucket histogram with 500 microsecond buckets; `render_max_us` provides the exact worst observed value. The 128-sample deadline is 5,805 microseconds.

Future physical acceptance requires:

- no sustained DMA underruns;
- `qfull=0`, `seqerr=0`;
- all render deadlines met;
- stable `stack_high < 2048` with adequate margin;
- correct pitch/noise/envelope behaviour;
- no material CPCPERF regression;
- unchanged display, controls, USB detection and persistent library.

## Host verification

All 20 registered host tests pass. Focused Stage 4H.5 tests cover:

- sample-exact span/tick/reference output;
- tone, noise, mixer, fixed volume and envelope behaviour;
- same-value envelope restart and period shortening;
- register 14 ordering;
- 68%, 80%, 100% and 125% producer arrival semantics;
- 64- and 128-entry Knight Tyme bursts;
- explicit queue-full failure;
- 100,000-event concurrent SPSC ordering;
- deliberate sequence-gap detection;
- stop/reset/restart phase equivalence and stale-queue clearing;
- Manic Miner, Knight Tyme and Dizzy trace replay with zero queue overruns.

## UF2 validation

- physical blocks: 1,388;
- ordinary application blocks: 1,387;
- application family: `0xE48BFF59`;
- application targets: `0x10000000..0x10056AFF`;
- all application data remains in the first 4 MiB reservation;
- E10 blocks: exactly one;
- E10 family: `0xE48BFF57`;
- E10 flags: `0x0000A000`;
- target: `0x10FFFF00`;
- extension: `0x9957E304`;
- payload: 256 bytes of `0xEF`;
- no game-storage or catalogue targets;
- no unexpected absolute-family blocks.

## Source changes

- `src/dual_core_ay.cpp/.h`: experimental core ownership, queue, lifecycle, telemetry and stack watermark.
- `src/cpc_core.cpp/.h`: timestamped AY write sink, register seeding and synchronous-render disable switch.
- `src/main.cpp`: post-load dual-core startup and core-0 telemetry reporting.
- `src/audio_output.cpp/.h`: submit result and atomic cross-core diagnostics; Stage 4H.4 amplifier policy retained.
- `prototypes/stage4h5_realtime_ay/`: integrated compact span renderer and private reference-symbol isolation.
- `tests/realtime_ay_prototype_test.cpp`: timebase, reset and sequence-error coverage.
- `tools/check_stage4h5b_link.py`: final-ELF scratch, stack and main-SRAM gate.
- `CMakeLists.txt`: isolated `CPCEXPLORER_STAGE4H5B_DUAL_CORE_AY` target and identity.

## Remaining risks

1. RP2350 render time, interrupt latency and DMA continuity are not measurable on the host.
2. The 2 KiB core-1 stack has only linker and guard instrumentation evidence, not a physical watermark result.
3. Main-SRAM safety margin is only 316 bytes; further static allocation must fail the existing gate.
4. Audio tempo will remain slow when game emulation is slow.
5. Any future storage operation while a game is active requires an audio stop handshake and a new flash audit.

No physical deployment, COM access, BOOTSEL access, game upload/removal or external-flash write occurred.
