# Stage 4H.3 physical speaker diagnostic

## Decision

**PASS for a separately approved, controlled physical test.** This is a standalone
speaker-path diagnostic. It does not initialize the CPC emulator, display, launcher,
game catalog, upload/removal protocol, or external game-storage backend.

The deployed Stage 4H.1A telemetry showed 3,106 submitted and played buffers with
2,781 underruns. That confirms AY sample production and DMA progression, while also
showing that the amplifier was being power-cycled on most buffers. This diagnostic
holds the amplifier enabled and removes CPC execution from the test.

## Candidate

- identity: `0.4.14-stage4h3speakerdiag`;
- UF2: `build-rp2350-stage4h3-speaker-diag/cpc_explorer_speaker_diag.uf2`;
- UF2 size: 64,512 bytes;
- raw binary size: 31,988 bytes;
- ELF size: 685,800 bytes;
- SHA-256: `faa2a32492126acdbb8133ba2d9d9149252c880729a3d08868510fa66046cc65`.

The linked image contains 36,084 bytes of text/data content and 3,408 bytes of BSS.
The application has no CPC core, ROM, snapshot, launcher, game-library, catalog,
installer, removal, or `flash_range_erase`/`flash_range_program` symbols.

## Hardware sequence

The implementation uses the established Explorer definitions:

- GPIO12: PWM audio, RP2350 PWM slice 6 channel A;
- GPIO13: PAM8301 amplifier enable, active high;
- PWM wrap: 255;
- requested PWM/sample rate: 22,050 Hz;
- silence midpoint: 128;
- tone modulation: conservative `+/-0.4` input, converted by the existing output
  path to approximately 109..147 PWM compare values.

Startup first drives GPIO13 low, configures and enables PWM at midpoint, configures
DMA and its IRQ, then asserts GPIO13 once. Firmware waits 10 ms before submitting
audio. Unlike the game firmware's default policy, DMA underruns do not deassert the
amplifier. `audio::shutdown()` returns PWM to midpoint, disables PWM, aborts and
unclaims DMA, and finally drives GPIO13 low.

The generated sequence is:

1. 440 Hz for 22,050 samples (1 second);
2. silence for 5,513 samples (250.02 ms);
3. 660 Hz for 22,050 samples;
4. silence for 5,513 samples;
5. 880 Hz for 22,050 samples.

Total generated content is 77,176 samples, approximately 3.500 seconds. The last
DMA block contains eight padded midpoint samples. After completion, USB CDC repeats
the bounded result line about once per second without waiting for a host:

```text
SPKDIAG RESULT fw=0.4.14-stage4h3speakerdiag rate=22050 samples=77176 submitted=603 played=603 underruns=1 overruns=0 amp=off
```

One final underrun is expected when the deliberately finite stream ends. Additional
underruns or any overrun are unexpected. If no DMA channel can be claimed, the
amplifier remains off and the firmware reports `reason=no_dma_channel`.

The LCD and buttons are intentionally not initialized. A blank LCD and unavailable
launcher are expected while this temporary diagnostic is installed.

## Host verification

Focused tests pass 4/4:

1. AY register/mixer/envelope/reference-fast equivalence;
2. bounded two-buffer PCM conversion;
3. existing AY three-tone/WAV test;
4. standalone physical-tone sequence.

The standalone generator measured 439, 659 and 879 upward crossings per second,
within the test's two-hertz tolerance of 440, 660 and 880 Hz. Both silence regions
are entirely zero and each contains exactly 5,513 samples. Total sequence length,
amplitude bounds and padding behavior pass.

## UF2 policy

`tools/validate_uf2.py` accepts the complete candidate:

- 126 physical UF2 blocks;
- 125 ordinary RP2350 ARM-S application blocks;
- application family `0xe48bff59`;
- ordinary targets `0x10000000..0x10007cff`;
- all ordinary data inside the first 4 MiB firmware reservation;
- exactly one authorized E10 compatibility block;
- E10 family `0xe48bff57`, flags `0x0000a000`, target `0x10ffff00`, extension
  `0x9957e304`, payload 256 bytes of `0xef`;
- no catalog or game-storage target and no unexpected family/address.

## Preservation and rollback

No existing artifact was overwritten. Rechecked rollback hashes are:

- deployed Stage 4H.1A:
  `f04d62d6790f93f2b9ef470e1b7dc50ac64745f5700a761e13088a05cfbe0485`;
- Stage 4H.1:
  `38b93a3e96c840f819684748c9496c5da0e14f23b4eb3a063acd974de71447ae`;
- Stage 4G.2.1:
  `37edb2731a5580d8d76366bdb7fb2ba1dd43aadf7659e292b3ab48b0c478cd9e`;
- Stage 4F.6:
  `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`;
- Dizzy package:
  `780423a9bdf589b838d564e0dc6c53726902d3ea89b094cbd444fc588cb77c5e`;
- Manic Miner package:
  `30ad9705c3532920d1652dd9b9188b8a53deed7fb2c03d8f1756c9daffcdcf1d`.

## Separately approved physical procedure

1. Recalculate the diagnostic and Stage 4H.1A rollback SHA-256 values.
2. Enter BOOTSEL and verify VID/PID `2e8a:000f`, volume label `RP2350`, and
   `INFO_UF2.TXT`; do not assume a drive letter.
3. Copy the diagnostic UF2 exactly once. Do not retry an ambiguous copy.
4. Listen for 440 Hz, silence, 660 Hz, silence, and 880 Hz over approximately
   3.5 seconds. The sequence begins shortly after boot.
5. Open the newly enumerated USB CDC port after the sequence and capture the repeated
   `SPKDIAG RESULT` line. Confirm `submitted=603`, `played=603`, `overruns=0`, and
   normally the one terminal underrun.
6. If silent, measure GPIO13 (stable high during the sequence) and GPIO12 (22.05 kHz
   carrier with modulated duty) before attributing failure to the speaker or PAM8301.
7. Stop and report. Do not automatically reflash.
8. Under separate approval, return to BOOTSEL and copy the hash-verified Stage 4H.1A
   rollback UF2 exactly once. The game catalog remains in its protected storage and
   is not accessed or altered by this diagnostic.

No COM port, BOOTSEL volume, physical flash, or installed game was accessed during
this host build and verification.
