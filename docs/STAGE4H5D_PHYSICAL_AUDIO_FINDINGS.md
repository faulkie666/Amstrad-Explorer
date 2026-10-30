# Stage 4H.5D — physical audio findings

## Result

Host verification is **PASS** for a corrected experimental candidate. Physical
acceptance remains pending. No COM port, BOOTSEL volume, device flash, installed
game, or catalogue was accessed during this work.

Candidate:

- identity: `0.4.17-stage4h5dayaudioexp`
- UF2: `build-rp2350-stage4h5d-audio-correction/cpc_explorer_dual_core_ay_corrected.uf2`
- size: 710,656 bytes
- SHA-256: `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`

## Physical evidence used

The investigation used the supplied Stage 4H.5B results: CPC speed 68.89%,
renderer maximum 704 us against a 5,805 us block interval, no renderer deadline
misses, queue peak 16/128, no queue-full/sequence/DMA-overrun events, 1,283 new
DMA underruns, late events equal to produced events, and `stack_high=792`.
Generated PCM sample growth exactly equalled played DMA blocks multiplied by 128.

## Proven flash-probe root cause

`probe-error:-4` is Pico SDK `PICO_ERROR_NOT_PERMITTED`. `PicoFlash::probe()`
performs the JEDEC command through `flash_safe_execute()`. Linking
`pico_multicore` selects the SDK multicore-lockout helper. That helper refuses to
execute unless the other core has registered itself as a lockout victim using
`flash_safe_execute_core_init()`.

Stage 4H.5B linked `pico_multicore`, but the launcher probed flash before
`run_emulator()` launched core 1. The required victim therefore did not exist at
the exact point of the probe. The SDK source explicitly returns
`PICO_ERROR_NOT_PERMITTED` in this state. Stage 4H.4 did not link the dual-core AY
path, so `flash_safe_execute()` used the single-core interrupt-locking path and
the same JEDEC probe succeeded.

This failure occurs before core 1 is launched. It is not a GPIO, SPI, DMA-channel,
or audio-initialization conflict: none of the Stage 4H.5B audio initialization
had run yet.

Four games still appeared because the launcher unconditionally maps and validates
the existing read-only CPCLIB catalogue through XIP. Probe success gates capacity,
recovery, upload, removal, erase, and program operations; it does not gate
read-only catalogue enumeration. The safe result was therefore capacity zero and
mutations disabled, while four already committed catalogue entries remained
visible.

### Correction

Only the Stage 4H.5D build now launches core 1 into an inert `Idle` state before
the JEDEC probe. Core 1 first calls `flash_safe_execute_core_init()`. It does not
claim DMA, configure PWM, enable the amplifier, or start AY rendering while idle.
The probe and any later menu-time flash-safe operation can consequently lock out
the registered core correctly. AY initialization still occurs only when a game
starts.

The flash backend, JEDEC allow-list, catalogue layout, upload/removal checks,
write boundaries, and E10 guard were not changed.

## Proven audio-underrun mechanism

The underrun counter increments in `audio_output.cpp::dma_isr()`, once when a DMA
block completes and `PcmBufferQueue::acquire()` finds no `Ready` successor. The
handler places PWM at midpoint and stops DMA. A later `submit()` restarts it.

Stage 4H.5B had two independent clocks:

1. PWM/DMA consumed one 128-sample block at the hardware-derived audio rate.
2. Core 1 rendered on a separate nominal 5,805 us absolute timer.

Even though rendering took at most 704 us, timer phase or small clock-rate
differences could let DMA inspect the queue just before the next block was
published. That is a short producer/consumer handoff gap, not evidence that the
renderer needed longer than an entire block. The supplied equality
`pcm_samples == played_blocks * 128`, together with zero overruns and zero
renderer deadline misses, rules out loss of generated whole blocks during the
capture. It does not prove that each of the 1,283 ISR empty observations produced
zero-duration silence; it proves that interpreting each counter increment as a
complete missing 128-sample interval is incorrect.

### Correction

Stage 4H.5D primes both buffers, then uses actual DMA queue availability as the
producer clock. Core 1 waits without spinning until the ISR frees a slot, renders
the successor immediately, and submits it. At the measured 704 us maximum there
is approximately 5.1 ms of margin before the following 5,805 us block boundary.
The two-buffer format, DMA mechanism, 22,050 Hz target, amplifier policy, and AY
synthesis are unchanged. A renderer-duration miss is still counted if rendering
itself exceeds 5,805 us.

The change is source-supported but still requires physical A/B measurement. A
rare startup or shutdown boundary indication may remain; the acceptance metric
is removal of sustained gameplay growth, not an assumed absolute zero counter.

## Late PSG events

The late-event result is expected under the present architecture. The consumer's
AY clock advances at real wall-clock audio time. The Z80 producer advances CPC
time at only about 68.89% of real time. After the initial state, most PSG writes
therefore arrive with a CPC timestamp behind the consumer's already-rendered
position. The implementation applies such events immediately and in sequence,
so oscillator frequency and register ordering remain correct. Musical state
changes—and therefore musical tempo—still follow the slower emulated CPC.

Changing timestamp scale to suppress the counter would artificially accelerate
musical tempo or distort audio/video synchronization. No timebase correction was
made. Slightly slow music remains a known consequence until CPC execution itself
approaches real time.

## Stack and SRAM safety

`stack_high=792` means the guard scan observed at most 792 bytes used from the
dedicated 2,048-byte core-1 stack, leaving 1,256 bytes untouched in that run. It
is a high-water used-byte count, not free bytes and not an overflow indicator.

The final linker gates report:

| Allocation or gate | Verified value |
|---|---:|
| `z80_execute_step` | `0x20001280`, 76,212 bytes |
| Z80 routine SHA-256 | `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b` |
| Main-SRAM headroom | 119,100 bytes |
| Required main-SRAM minimum | 118,784 bytes (116 KiB) |
| Gate margin | 316 bytes |
| AY control, scratch X | `0x20080000`, 1,216 bytes |
| Core-1 stack, scratch Y | `0x20080800`, 2,048 bytes |
| AY DC history, scratch Y | `0x20081000`, 2,048 bytes |
| Core-0 stack, scratch Y | `0x20081800`, 2,048 bytes |
| Unused scratch-X gap | 832 bytes |
| Firmware binary | 354,860 bytes |
| First-4-MiB firmware margin | 3,839,444 bytes |

The Z80 routine remains byte-identical to the established 76,212-byte image. The
116 KiB gate was not weakened, and the scratch ownership/layout is unchanged.

## Host verification

The complete host suite passed: **20/20 tests**. It covers CPC464 BASIC, Knight
Tyme, Manic Miner, launcher and controls, snapshot loading, game library,
transactional upload/removal and recovery, display fit, fast/reference engines,
AY audio, speaker policy, PCM buffering, the real-time AY prototype and traces,
and flash-layout/E10 policy tests. The focused PCM-buffer test now also proves
the free-slot query used for DMA-paced handoff.

RP2350 post-link checks passed:

- main-SRAM and 116 KiB gate;
- exact Z80 placement and size;
- scratch-X/scratch-Y ownership and both stack reservations;
- application firmware entirely in the first 4 MiB;
- 1,388 physical UF2 blocks: 1,387 ordinary blocks from `0x10000000` through
  `0x10056AFF`, family `0xE48BFF59`;
- exactly one authorized E10 block: family `0xE48BFF57`, flags `0x0000A000`,
  target `0x10FFFF00`, extension `0x9957E304`, and 256 bytes of `0xEF`;
- no catalogue or game-storage target and no other out-of-range block.

Preserved hashes remain:

- Stage 4H.5B: `ff7b3ccbae73d8e7bce563921e9318f5c06c9ccae420b743a4158a96f2e4897a`
- Stage 4H.4: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Stage 4F.6: `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`

## Remaining uncertainties and required physical test

Host evidence cannot confirm the RP2350 JEDEC result or physical underrun rate.
A separately approved test should:

1. deploy the candidate once under the established UF2 procedure;
2. verify identity `0.4.17-stage4h5dayaudioexp`;
3. run read-only detect/status/list and require `EF-40-18`, 16,777,216 bytes,
   idle upload state, and the same four entries;
4. capture settled Manic Miner `CPCPERF` and audio telemetry for the same duration
   as Stage 4H.5B;
5. compare underrun growth, renderer maximum/deadline misses, queue metrics,
   sequence errors, CPC speed, audible continuity, pitch, and tempo;
6. roll back to the preserved Stage 4H.4 UF2 if either flash detection or game
   behaviour regresses.

The corrected candidate is not a production deployment and no claim of physical
acceptance or faster CPC emulation is made.
