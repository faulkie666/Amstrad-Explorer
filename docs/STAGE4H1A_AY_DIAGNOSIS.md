# Stage 4H.1A AY audio diagnosis and correction

## Decision

**HOLD for physical audio deployment.** The AY generator and WAV writer pass
deterministic host verification, but the validated Dizzy snapshot does not
execute sustained music-producing PSG writes in the tested 60 seconds. It
produces one short 880 Hz channel-C fade and then explicitly silences the AY.
Deploying another firmware would not turn that captured game state into music.

The host diagnosis, diagnostic-counter correction, regression suite, SRAM
gate, and UF2 policy all pass. No COM port, BOOTSEL volume, device flash,
installed catalog, or installed game was accessed.

## Original WAV inspection

Both files are standards-compliant little-endian RIFF/WAVE PCM. Python's WAV
decoder accepts both, and each RIFF and data length agrees with the file size.

| File | Encoding | Samples / duration | Sample range | RMS | Peak | Exact silence | Frequency content |
|---|---|---:|---:|---:|---:|---:|---|
| `ay-audio-evidence.wav` | mono PCM16, 22,050 Hz | 22,016 / 0.998458 s | -4,282..13,749 | 3,901.66 (11.91% FS) | 13,749 (41.96% FS) | 0.0590% | changing mixed tone/noise/envelope signal; 100 ms upward-crossing rates vary about 1.94..2.40 kHz |
| `dizzy-ay-evidence.wav` | mono PCM16, 22,050 Hz | 220,672 / 10.007800 s | -979..2,438 | 62.86 (0.192% FS) | 2,438 (7.44% FS) | 98.7126% | approximately 880 Hz in the first 100 ms, a short tail, then zero |

Hashes remain unchanged:

- synthetic: `7d7d1eaed934daefc7143940e27c9be696f9f911573f4012686ca4151db7ea88`;
- Dizzy: `9464693880eec6ae43d952250f3b34135f14a583305d5127ad9105a4c0bf1a19`.

The original synthetic file is neither silent nor malformed. It is a poor
listening diagnostic because it deliberately mixes several AY facilities into
one complex signal. The reported silent playback cannot be attributed to
missing PCM samples, bad RIFF fields, clipping, or inadequate file-level
amplitude. No Windows playback fault is asserted without playback-path
evidence.

## Isolated AY result

`cpc_three_tone_audio_test` programs channel A through the modeled AY register
interface with tone periods calculated as `round(1 MHz / (16 * frequency))`,
mixer register 7 set to `0x3e`, and fixed volume 15. It independently counts
PCM zero crossings and reopens the written file to validate every relevant WAV
header field and the exact data length.

Measured frequencies are 439.94, 657.38, and 879.88 Hz. The output contains
66,176 samples (3.001179 s), ranges from -3,430 to 6,541, has RMS 3,252.24
(9.93% FS), peak 6,541 (19.96% FS), and only 0.0378% exact-zero samples.

- WAV: `artifacts/stage4h1a-audio/three-tone-440-660-880.wav`
- SHA-256: `fbe838b0e148c18f8e3cbf238f95b17a8df780b6eb389c62fa1a10be3cc39fd5`

The AY regression also proves register masking, a 499.57 Hz period-125 tone,
mixer/noise operation, a repeating envelope with changing amplitude, exact
reference/fast-engine PCM agreement, reset silence, and bounded PCM conversion.

## Dizzy trace

The test restores the actual 65,792-byte Dizzy SNA, verifies all 16 restored
PSG registers, runs 60 emulated seconds, presses Space to enter the game, and
then repeatedly exercises left, right, and jump. The initial relevant state is:

- selected register 10;
- register 4/5 channel-C period 71/0, approximately 880.28 Hz;
- mixer `0x38`;
- channel-C amplitude 11;
- envelope period and shape registers zero.

The trace contains 6,124 AY-bus observations. Most are register-14 keyboard
scans. After collapsing duplicate PPI strobes, the sound-relevant sequence is:

1. register 10 fades 10, 9, ... 0 from 0.007 to 0.107 seconds;
2. mixer register 7 becomes `0x3c` at 0.118 seconds;
3. at 0.846 and 1.017..1.087 seconds the game writes zero amplitudes and
   mixer values `0x3d`/`0x3f`;
4. there are no later sound-register writes through 60 seconds.

The corrected counter observes 14 changing sound-register transactions. The
60-second WAV is therefore legitimately 99.7854% silent, not a failed capture:

- WAV: `artifacts/stage4h1a-audio/dizzy-60s-gameplay.wav`
- WAV SHA-256: `1833b439ac93d2b12d55304acf194f1ef20d985cc7622ad697a4ded9d2bd3b49`
- trace: `artifacts/stage4h1a-audio/dizzy-60s-psg-trace.csv`
- trace SHA-256: `6048c5598c0c1dc9bf7a48f5611f958ec986c667d61a9280f7d8f28be7096be4`

This proves only the behavior of this captured SNA state. It does not claim
that every Dizzy release lacks music. No replacement or invented music was
introduced.

## Proven defect and correction

The previous `AudioDiagnostics.register_writes` compared all 16 AY registers
after every PPI transaction. AY port A (register 14) changes during keyboard
reads, so keyboard scanning was falsely counted as sound-register activity.
The counter now compares only sound registers 0..13. Host-only tracing records
AY latch/write bus activity and is compiled out of RP2350 firmware.

No tone generation, mixer, envelope, timing, controls, snapshot, storage, USB,
or Z80 execution semantics changed. In particular, the extracted
`z80_execute_step` section remains exactly 76,212 bytes and SHA-256
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.

## Validation and performance

The complete host suite passes 16/16: BASIC, Knight Tyme, launcher, game
library, transactional installer/removal, importer, UF2 policy, Manic Miner,
gameplay controls, CPCPERF, active-display fit, AY, bounded audio queue,
three-tone/WAV serialization, 60-second Dizzy trace, and fast-engine
integration.

Five paired desktop Manic runs give a median audio-on overhead of 8.03%
(median audio-off 28.487 ms, audio-on 30.847 ms for the test workload). One
outlying pair was retained rather than hidden. This is host evidence only and
does not predict RP2350 performance.

## Host-only candidate and memory gates

A separate diagnostic artifact was built because the counter defect is proven:

- identity: `0.4.13-stage4h1aaydiag`;
- UF2: `build-rp2350-stage4h1a-ay-diag/cpc_explorer_ay_audio_diag.uf2`;
- size: 690,688 bytes; raw binary 344,860 bytes;
- SHA-256: `f04d62d6790f93f2b9ef470e1b7dc50ac64745f5700a761e13088a05cfbe0485`.

It is not recommended for deployment while the available Dizzy state has no
sustained music. Linker results are `.text` 144,776, `.rodata` 111,024,
`.data` 88,168, and `.bss` 314,304 bytes. The 2,048-byte heap and separate
2,048-byte core-0 stack remain reserved. Main-SRAM headroom is 119,488 bytes
(116.69 KiB), 704 bytes above the enforced 116 KiB minimum.

UF2 validation accepts 1,349 physical blocks: 1,348 ordinary family
`0xe48bff59` blocks covering `0x10000000..0x100543ff`, wholly inside the first
4 MiB, plus exactly one authorized E10 block at `0x10ffff00` with family
`0xe48bff57`, flags `0x0000a000`, extension `0x9957e304`, and 256 bytes of
`0xef`. No catalog or game-storage target exists.

## Preservation

The original Stage 4H.1 candidate remains
`38b93a3e96c840f819684748c9496c5da0e14f23b4eb3a063acd974de71447ae`.
The seven historical BASIC-through-4B.1 UF2s, Stage 4F.6, Stage 4G.2.1, and
the Dizzy package all rehash to their recorded values. The MicroPython backup
manifest was independently recalculated: 21/21 files valid, zero failures.

The bounded next step is to obtain or deterministically capture a known
CPC464/64 KiB game state that demonstrably executes AY music, then run the
same bus trace and PCM comparison before considering any physical deployment.
