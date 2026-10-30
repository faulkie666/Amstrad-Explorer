# Stage 4H.4 — in-game AY amplifier correction

## Decision

**PASS for a separately approved physical A/B test of the surgical amplifier
correction. HOLD on claiming continuous or fully stable real-time game audio.**

The Stage 4H.3 test proves that GPIO12 PWM, GPIO13 amplifier enable, PAM8301 and
speaker are functional. Stage 4H.1A Manic Miner telemetry proves that nonzero AY PCM
reaches the DMA path, but 2,781 of 3,106 completed buffers underrun. The remaining
confirmed defect is that every normal underrun deasserts GPIO13 and the next buffer
reasserts it, power-cycling the amplifier roughly once per produced audio packet.

At 68% CPC speed, the emulator produces only about 14,994 samples per wall-clock
second while DMA consumes 22,050. A 128-sample block plays in 5.805 ms but takes
about 8.54 ms to produce, leaving approximately 2.73 ms of deficit per block. No
finite buffer can remove this sustained 7,056-sample-per-second shortfall.

## Surgical correction

`audio_output` now has two explicit policies:

- `GateOnPlayback`: the original Stage 4H.1A behavior, retained as the default;
- `HoldEnabled`: establish PWM midpoint, enable GPIO13 once after successful DMA
  initialization, retain GPIO13 high through underruns, and disable it only in
  explicit `audio::shutdown()`.

Stage 4H.4 selects `HoldEnabled`. On an empty queue the DMA ISR still:

1. marks playback idle;
2. sets PWM channel A to midpoint 128;
3. increments the existing underrun counter.

It no longer deasserts GPIO13. The next buffer restarts DMA without cycling the
amplifier. DMA-claim failure leaves GPIO13 low and retains the existing
`audio=disabled reason=no_dma_channel` report. The existing submitted, played,
underrun and overrun telemetry is unchanged.

Click prevention remains bounded: GPIO13 starts low; PWM is configured, set to
midpoint and enabled before GPIO13 is asserted. Snapshot restoration occurs before
the first PCM packet, providing settling time. Explicit shutdown deasserts GPIO13
before aborting DMA and disabling PWM.

The launcher does not initialize audio, so the amplifier remains off at the menu.
It is enabled only after a game is launched successfully into the emulator path.

## Candidate

- identity: `0.4.15-stage4h4ayaudio`;
- UF2: `build-rp2350-stage4h4-continuous-audio/cpc_explorer_ay_audio_continuous.uf2`;
- UF2 size: 690,688 bytes;
- raw binary: 344,948 bytes;
- SHA-256: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`.

Compared with Stage 4H.1A, linked text grows by 88 bytes. Read-only data, initialized
data, BSS, audio buffers, DMA configuration and all emulator state sizes are unchanged.

## AY and gameplay verification

The complete host suite passes 18/18:

- Locomotive BASIC boot;
- Knight Tyme snapshot regression;
- launcher and persistent game library;
- upload, removal and recovery;
- importer and UF2 policy;
- native Manic Miner snapshot;
- gameplay input profiles;
- CPCPERF metrics and active-display fit;
- AY tone, mixer, noise and envelope behavior;
- bounded audio queue;
- explicit amplifier policy;
- synthetic and physical-diagnostic tone sequences;
- Dizzy known-silent-state regression;
- reference/fast-engine integration.

The deterministic 60-second game probes also pass:

- Manic Miner with no input: 1,324,032 PCM samples, 3,222 changed sound-register
  writes and 1,271,607 nonzero samples;
- Knight Tyme with the previously verified one-time menu sequence: 1,324,032 PCM
  samples, 1,170 changed sound-register writes and 847,804 nonzero samples. Its WAV
  SHA-256 remains `241dc67ca03932278ebc707c6a60829f4b662263e25179d30715cb708a0d2c3f`,
  exactly matching the earlier sustained three-channel evidence;
- Dizzy remains the known mostly silent reference and its regression passes.

The current physical control profile for built-in Knight Tyme remains `None`. The
menu sequence is therefore a deterministic host regression, not a claim that the
existing three Explorer gameplay buttons can reproduce it physically. No control
mapping was changed in this stage.

Five host fast-engine runs give median Manic Miner results of 29.205 ms with AY
advancement disabled and 31.520 ms enabled, or 7.93% median host overhead. This is
desktop core evidence only. The GPIO/DMA ISR policy is RP2350-specific and requires
physical CPCPERF measurement; no physical speed or audibility gain is claimed yet.

## SRAM and Z80 preservation

The enforced Stage 4F.6 memory gate passes:

- `z80_execute_step`: `0x200011a0`, 76,212 bytes;
- extracted Z80 SHA-256:
  `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`;
- Stage 4H.1A extracted Z80 SHA-256: identical;
- `.text`: 144,864 bytes (88 bytes above Stage 4H.1A);
- `.rodata`: 111,024 bytes, unchanged;
- `.data`: 88,168 bytes, unchanged;
- `.bss`: 314,304 bytes, unchanged;
- heap reservation: 2,048 bytes;
- core-0 stack reservation: 2,048 bytes;
- main-SRAM headroom: 119,488 bytes / 116.69 KiB;
- required minimum: 118,784 bytes / 116 KiB;
- margin over minimum: 704 bytes.

No Z80 instruction, timing, memory access, snapshot, input, display, USB, catalog or
flash-storage implementation changed.

## UF2 policy

The validator accepts the entire candidate:

- 1,349 physical blocks;
- 1,348 ordinary application blocks;
- family `0xe48bff59`;
- targets `0x10000000..0x100543ff`, within the first 4 MiB;
- exactly one E10 block at `0x10ffff00`, family `0xe48bff57`, flags
  `0x0000a000`, extension `0x9957e304`, payload 256 bytes of `0xef`;
- no catalog/game-storage target and no unexpected block.

## Continuous-audio feasibility

The candidate intentionally does not hide the 32% production deficit. During an
underrun the speaker receives midpoint silence, so audio should be audible without
PAM8301 restart suppression but will still contain gaps.

The technically credible independent-generator architecture would require:

1. an ordered single-producer/single-consumer queue of PSG register writes;
2. a separate AY synthesis state advancing at wall-clock 22,050 Hz;
3. exact preservation of three tone channels, noise LFSR, envelope state and mixer;
4. a core-safe DMA producer, probably on core 1 or a bounded timer worker;
5. explicit overflow recovery and snapshot/reset synchronization;
6. new SRAM, stack, execution-cost and physical timing measurements.

This would preserve oscillator pitch between writes, but register changes would still
arrive at only 68% speed. Musical note durations and game/audio synchronization would
therefore follow slowed gameplay rather than a real CPC. Accessing the existing AY
instance concurrently is unsafe; a separate audio-only PSG state is required. That is
an architectural experiment, not a surgical fix, and is not included here.

Rejected shortcuts are:

- larger buffers, because they cannot correct a sustained deficit;
- replaying the last buffer, because it invents repeated waveform content;
- lowering PWM playback to about 14,994 Hz, because it lowers pitch and silently
  changes timing;
- dropping CPC cycles, because it compromises emulator correctness.

## Preservation

Rechecked artifacts remain unchanged:

- Stage 4H.1A rollback:
  `f04d62d6790f93f2b9ef470e1b7dc50ac64745f5700a761e13088a05cfbe0485`;
- Stage 4H.3 speaker diagnostic:
  `faa2a32492126acdbb8133ba2d9d9149252c880729a3d08868510fa66046cc65`;
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

The MicroPython backup manifest independently rehashes 21/21 files successfully.
Existing source changes from Stage 4H.3 remain present and were not reset or discarded.

## Separately approved physical test

1. Recalculate the candidate and Stage 4H.1A rollback hashes.
2. Verify BOOTSEL VID/PID `2e8a:000f`, volume label `RP2350`, and RP2350 identity
   in `INFO_UF2.TXT`; do not assume a drive letter.
3. Copy the candidate exactly once. Never retry an ambiguous copy automatically.
4. Verify application VID/PID `2e8a:0009`, identity `0.4.15-stage4h4ayaudio`, flash
   `EF-40-18`, capacity 16 MiB, upload idle, and exactly the existing four entries.
5. From the launcher, start Manic Miner and capture at least 30 synchronized
   `CPCPERF`/`CPCAUDIO` intervals. Listen specifically for audible music, gap severity,
   clicks and hiss. Underruns are expected to remain high; amplifier shutdowns are not.
6. Confirm movement and jump controls and full-screen rendering are unchanged.
7. Launch BASIC and Dizzy for regression checks. Knight Tyme's three-channel music
   remains host-proven; do not treat inability to enter its menu with the current
   physical profile as an audio failure.
8. Stop and report. Do not upload/remove games or automatically reflash.
9. Under separate rollback approval, copy the hash-verified Stage 4H.1A UF2 exactly
   once and recheck firmware identity and the four-entry catalog.

No COM port, BOOTSEL volume, physical flash, installed game or external catalog was
accessed during this host implementation and build.
