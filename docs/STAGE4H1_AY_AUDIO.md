# Stage 4H.1 — CPC464 AY audio candidate

## Result

**PASS for host verification and creation of a separate physical-test candidate.**
No COM port, BOOTSEL volume, installed game, catalog, or physical flash was accessed.
Audible quality and RP2350 performance remain pending physical measurement.

## Existing architecture audit

The vendored `chips` CPC implementation already contains a complete AY-3-8912 model:
three tone channels, noise LFSR, tone/noise mixer masks, fixed and envelope volumes,
all sixteen envelope shapes, register masks, and the AY I/O ports. The reference CPC
path configures it as `AY38910_TYPE_8912`, clocks it at 1 MHz (`4 MHz / 4`), and routes
the 8255 PPI port-A data and port-C BDIR/BC1 signals through `ay38910_iorq()`.

The fast engine already reproduced the PPI/AY address-latch and data-write transaction,
but never advanced the AY clock or delivered samples. That was the cause of silence.
Snapshot loading already restores the selected AY register and all sixteen PSG
registers. Stage 4H.1 additionally resets oscillator, noise, envelope, DC-filter and
partial-packet phase before reset/game load so a previous game cannot leak a transient
or stuck tone into the next state.

The display claims one DMA channel and one PIO state machine dynamically, with blocking
DMA completion and no DMA IRQ. Core 1 is unused. The audio candidate claims a second
DMA channel dynamically, uses PWM slice 6 on GPIO12, and owns DMA IRQ 1. No repeating
timer or multicore resource is added. The Explorer hardware pin choice follows the
[Pimoroni Explorer product](https://shop.pimoroni.com/products/explorer) and the
[Pimoroni Explorer Arduino-Pico variant](https://github.com/earlephilhower/arduino-pico/blob/master/variants/pimoroni_explorer/pins_arduino.h):
GPIO12 is PWM audio and GPIO13 is amplifier enable.

## Implementation

- Fast-engine AY time is derived from completed 4 MHz CPC cycles. AY state advances at
  one tick per four CPC cycles, never from musical-note or wall-clock guesses.
- The first direct 1 MHz tick loop failed the existing fast-engine performance gate.
  It was rejected. The final path batches only between observable AY boundaries:
  `/8` tone/noise events, `/16` envelope events, register-I/O boundaries, and 22.05 kHz
  sample boundaries. A deterministic test proves its float samples are exactly equal
  to the original per-tick model for the tested tone/noise/envelope sequence.
- Output is mono, 22,050 Hz, 8-bit PWM centred at duty 128. The chips model uses its
  conservative 0.2 magnitude and the hardware conversion limits swing to 48 PWM counts.
- Two 128-sample, 16-bit DMA buffers consume 512 payload bytes. Submission never waits;
  a full queue rejects the packet and increments `overruns`.
- On starvation, DMA stops, PWM returns to midpoint, GPIO13 goes low, and `underruns`
  increments. The amplifier remains disabled at startup and until the first complete
  packet is ready. Shutdown disables the amplifier, DMA, and PWM.
- Once per existing bounded USB telemetry interval, `CPCAUDIO` reports AY samples,
  non-zero samples, register writes, submitted/played packets, underruns, and overruns.
  It is emitted only while USB diagnostics are explicitly open and uses the existing
  bounded telemetry buffer.

Supported AY behavior is the existing CPC model's three independent tone periods,
three volumes, noise generator, tone/noise mixing, envelope period and shapes, register
masking, I/O selection, reset/silence, and SNA register restoration. No prerecorded or
placeholder audio is present.

## Host audio evidence

`cpc_ay_audio` measured a requested 500 Hz tone at **499.57 Hz**, checked register
masking, three-channel/noise/envelope output, exact batched-versus-reference samples,
and reset silence. Its one-second WAV is:

- `build-host-stage4h1/ay-audio-evidence.wav`
- 44,076 bytes
- SHA-256 `7d7d1eaed934daefc7143940e27c9be696f9f911573f4012686ca4151db7ea88`

`cpc_dizzy_audio` loaded the validated 65,792-byte Dizzy SNA, verified all sixteen
restored PSG registers, executed ten emulated seconds in the actual fast engine, and
pressed/released Space. Dizzy produced 27 changing PSG writes, 220,672 delivered
samples, and 2,781 non-zero samples. The actual-emulated-code WAV is:

- `build-host-stage4h1/dizzy-ay-evidence.wav`
- 441,388 bytes
- SHA-256 `9464693880eec6ae43d952250f3b34135f14a583305d5127ad9105a4c0bf1a19`

The queue test covers bounded double buffering, a rejected third packet, completion,
safe empty-queue silence, conservative conversion, and short-packet padding. The Dizzy
run is also the ten-second stability and game-generated-activity test.

## Host performance

A same-binary five-run A/B benchmark disabled only AY advancement through a host-only
test hook. For five emulated seconds of Manic Miner, the medians were:

| Mode | Host wall time |
|---|---:|
| AY advancement disabled | 28.496 ms |
| AY advancement enabled | 30.713 ms |
| Median measured overhead | 7.74% |

The complete integrated fast-engine test still exceeds its existing 4.5x reference
speed gate. These are desktop measurements only. They do not predict RP2350 speed.

There is an unavoidable physical acceptance risk: Stage 4F.6 currently averages about
74% of real CPC time. The candidate generates 20 ms of emulated audio per 20 ms CPC
slice, but physical production may take more than 20 ms. A finite buffer cannot correct
that long-term deficit, so `CPCAUDIO underruns` may increase and sound may contain gaps.
The candidate fails safely to midpoint silence. Changing playback pitch/rate or CPC
timing was deliberately not hidden in this stage.

## SRAM, code identity, and UF2 safety

| Item | Stage 4G.2.1 | Stage 4H.1 | Delta |
|---|---:|---:|---:|
| `.text` | 142,872 | 144,776 | +1,904 |
| `.rodata` | 110,888 | 111,024 | +136 |
| `.data` | 87,640 | 88,168 | +528 |
| `.bss` | 314,252 | 314,304 | +52 |
| minimum heap | 2,048 | 2,048 | 0 |
| core-0 stack | 2,048 | 2,048 | 0 |
| main-SRAM headroom | 120,052 | **119,488** | -564 |

The 119,488-byte headroom is 116.69 KiB and remains 704 bytes above the enforced
118,784-byte minimum. `z80_execute_step` is still at `0x200011A0`, exactly 76,212
bytes. Its freshly extracted section SHA-256 remains
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.

Final UF2 policy result:

- 1,349 physical blocks;
- 1,348 ordinary blocks, family `0xe48bff59`;
- ordinary target range `0x10000000..0x100543ff`, end-exclusive `0x10054400`;
- every ordinary target is within the first 4 MiB firmware reservation;
- exactly one E10 block: family `0xe48bff57`, flags `0x0000a000`, target
  `0x10ffff00`, extension `0x9957e304`, 256 bytes of `0xef`;
- no catalog/game-storage targets or other out-of-range blocks.

## Regression and preservation

Release/Clang host suite: **15/15 passed**:

1. CPC464 ROM boot and BASIC keyboard behavior
2. Knight Tyme regression
3. launcher navigation/selection
4. CPCLIB1 game library, scrolling, corruption and boundaries
5. transactional upload/removal and recovery
6. importer and generic control metadata
7. UF2/E10 policy
8. native Manic Miner pipeline
9. Manic Miner and Dizzy control profiles
10. CPCPERF aggregation
11. active-display fit
12. AY behavior and exact batched equivalence
13. audio buffer overrun/underrun behavior
14. actual Dizzy PSG activity and WAV capture
15. BASIC/Knight Tyme/Manic Miner fast-engine integration and performance gate

Preserved hashes were rechecked:

- Stage 4G.2.1: `37edb2731a5580d8d76366bdb7fb2ba1dd43aadf7659e292b3ab48b0c478cd9e`
- Stage 4F.6 rollback: `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`
- approved Dizzy package: `780423a9bdf589b838d564e0dc6c53726902d3ea89b094cbd444fc588cb77c5e`
- MicroPython backup: 21/21 local, manifest and original-device hashes agree.

The preserved 4E.2, 4F.2 FAST/OPT, and 4F.5 UF2 hashes also remain the recorded
values. No game package, snapshot, ROM, flash layout, upload/removal protocol, or
catalog format was changed by Stage 4H.1.

## Candidate

- Identity: `0.4.12-stage4h1ayaudio`
- UF2: `build-rp2350-stage4h1-ay-audio/cpc_explorer_ay_audio.uf2`
- UF2 size: 690,688 bytes
- UF2 SHA-256: `38b93a3e96c840f819684748c9496c5da0e14f23b4eb3a063acd974de71447ae`
- binary size: 344,860 bytes

## Separately approved physical test

1. Recheck the candidate and Stage 4G.2.1 rollback hashes.
2. Verify BOOTSEL VID/PID `2e8a:000f`, RP2350 volume label and `INFO_UF2.TXT`.
3. Copy the candidate exactly once; never retry an ambiguous copy automatically.
4. Verify application VID/PID `2e8a:0009`, identity `0.4.12-stage4h1ayaudio`, flash
   `EF-40-18`, upload idle, and the existing catalog unchanged.
5. Launch BASIC, Knight Tyme, Manic Miner, and Dizzy. Confirm controls/display first.
6. Open serial diagnostics, capture `CPCPERF` and `CPCAUDIO` for at least 30 intervals
   during a repeatable Dizzy music/effects scene, and record audible distortion,
   underruns, overruns, Z80 time, total loop time, and CPC speed.
7. Confirm no startup pop, silence before launch, conservative volume, correct tone/
   noise/envelope character, and amplifier silence after starvation/reset.
8. Do not upload/remove games during this first audio acceptance test.

Stop after measurement. Do not claim sound or performance acceptance until the physical
speaker result and telemetry are reviewed.
