# Stage 4J.1 - Qw/ST Pad controller and AY audio investigation

## Result

Host verification is **PASS**. A separate experimental RP2350 candidate was
built. The existing read-only Knight Tyme telemetry capture was used as
evidence; no BOOTSEL volume, catalogue, game storage, or physical flash was
written. Stage 4I.12 remains on hold.

The candidate is suitable for a separately approved physical controller and
performance acceptance test. It is not yet a production release: Qw/ST Pad
latency and its effect on physical CPCPERF/CPCAUDIO telemetry still require an
RP2350 A/B measurement.

## Controller implementation

The new `qwst_pad` driver follows Pimoroni's MIT-licensed PIM752 reference
implementation. It uses I2C0 on GPIO20 (SDA) and GPIO21 (SCL), probes only
`0x21`, `0x23`, `0x25`, and `0x27`, and uses the reference TCA9555 register
configuration:

| Register | Value |
|---|---:|
| Configuration ports | `0xF93F` |
| Polarity inversion | `0xF83F` |
| Output ports | `0x06C0` |

Every I2C operation has a 2 ms absolute deadline. A connected pad is sampled
once per 20 ms emulator iteration. Input is accepted after two equal samples;
all ten buttons are represented in one bitmask, so chords, holds, and releases
are preserved. A failed read immediately releases every emulated pad key. A
disconnected pad is reprobed at most once per second, while the original six
Explorer buttons remain active.

Official TCA9555 input assignments are A=14, B=12, X=15, Y=13, Up=1,
Down=4, Left=2, Right=3, Plus=11, and Minus=5. The official board photograph
confirms the right cluster is top X, bottom B, left Y, right A.

References:

- https://github.com/pimoroni/qwstpad-micropython
- https://shop.pimoroni.com/products/qwst-pad

## Profiles and mappings

Mappings are selected by the existing descriptor control-profile byte. The
existing values 0-5 are unchanged; new reusable values are 6 (cursor/QAOP),
7 (Knight Tyme), and 8 (WASD/Space). Existing packages therefore remain
compatible, and future packages can select a profile without hard-coded game
IDs. The three already-installed legacy snapshots use their validated payload
CRCs as the compatibility bridge.

### Generic cursor and QAOP profile

| Qw/ST Pad | CPC input |
|---|---|
| Up / Down / Left / Right | Cursor Up / Down / Left / Right |
| X (physical top) | Q |
| B (physical bottom) | A |
| Y (physical left) | O |
| A (physical right) | P |
| Minus | Space |
| Plus | Enter |

### Generic joystick profile

The directional pad drives CPC joystick 1 directions. X and A both drive
joystick fire. The established onboard joystick mapping remains unchanged.

### Game profiles

| Game | Qw/ST Pad mapping | Status |
|---|---|---|
| Knight Tyme | D-pad=cursors; X=N, B=M, Y=A, A=Z; Minus=Space; Plus=Enter | Verified input sequence is representable |
| Manic Miner | Left=Q, Right=W, X=bare Shift | Preserves validated controls |
| Dizzy | Left=Z, Right=X, X=Space | Preserves validated controls |
| Columns | D-pad=joystick 1 directions; X/A=fire | Uses existing joystick profile |
| Thrust | Left=Z, Right=X, Up=Shift, X=Space, Plus=Enter | Preserves existing five-key profile |
| Fruity Frank | No new Qw/ST mapping | Deliberately not invented |

Input is injected at the existing CPC keyboard/joystick boundary. It is
level-based, launch-button release suppression remains in place, and duplicate
physical sources for the same CPC key are reference-counted logically so that
releasing one source cannot release a key still held by another.

## AY diagnosis

### Knight Tyme

The physically captured initial Knight Tyme state reported zero produced PSG
events while DMA playback continued normally with zero underruns. That is
consistent with the snapshot waiting at its menu rather than an audio playback
failure.

The deterministic host menu sequence (Space, Space, then N/M/A/Z/Space) ran
the original snapshot for 60 seconds and produced:

- 1,170 changing AY sound-register writes;
- 847,804 non-zero PCM samples of 1,324,032;
- 24,988 PSG bus events;
- sustained three-channel music after the menu sequence.

The diagnosis is **missing menu/game controls in the previous physical input
surface**, not an AY generator or speaker defect. The new Knight profile exposes
the exact verified keys.

### Dizzy

The validated Dizzy snapshot was exercised through title, start, gameplay,
pause, and resume states for 60 seconds. It produced only 14 changing AY sound
register writes and 2,781 non-zero samples of 1,324,032. The finite initial
fade is authentic; this captured state has no sustained music. Physical silence
is therefore expected original/snapshot behaviour, not a playback defect.

No AY implementation change was made.

Audio evidence is under `artifacts/stage4j1-qwst-audio/`; the earlier recordings
and game packages were not changed.

## Verification

The complete isolated host suite passed **25/25**. Coverage includes CPC464
boot, Knight Tyme, Manic Miner, launcher, CPCLIB1, packed-library fallback,
CPXPCK2 migration, importer and UF2 policies, display fit, all AY/audio paths,
and the fast engine.

New focused tests prove:

- all official TCA9555 button bits and alternative address detection;
- configuration byte order;
- simultaneous presses, two-sample debounce, holds, releases, disconnect and
  rate-limited reconnect;
- 16-bit transition masks (including Qw/ST upper bits);
- cursor/QAOP orientation, joystick fire, per-game profiles, invalid metadata,
  and duplicate-key-source handling.

The SRAM Z80 routine is unchanged at `0x20001280`, exactly 76,212 bytes, with
SHA-256 `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.

## Candidate and safety gates

Firmware identity: `0.4.21-stage4j1qwstexp`

Candidate:

`build-rp2350-stage4j1-qwst/cpc_explorer_qwst_audio_exp.uf2`

SHA-256:

`2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6`

The binary is 369,780 bytes; the UF2 is 740,352 bytes. Compared with Stage
4I.8, flash binary size increases by 3,524 bytes and static BSS by 32 bytes.

Both unchanged linker gates passed:

```text
stage4f6 SRAM gate: step=0x20001280+76212 heap=2048 stack=2048
  headroom=119008 minimum=118784
stage4h5b link gate: control=0x20080000+1216
  core1_stack=0x20080800+2048 dc=0x20081000+2048
  core0_stack=0x20081800+2048 scratch_x_gap=832
  main_headroom=119008 minimum=118784
```

Main-SRAM margin is **224 bytes** above the mandatory gate. No stack, heap,
framebuffer, audio buffer, or safety reservation was reduced.

UF2 validation passed:

- 1,446 physical blocks;
- 1,445 ordinary application blocks, family `0xE48BFF59`;
- targets `0x10000000` through exclusive `0x1005A500`, entirely inside the
  first 4 MiB firmware reservation;
- exactly one authorised E10 block at `0x10FFFF00`, family `0xE48BFF57`, flags
  `0x0000A000`, extension `0x9957E304`, payload 256 bytes of `0xEF`;
- no catalogue or game-storage target.

Preserved artifact checks remain unchanged: Stage 4F.6
`65f526d7...b6f5`, Stage 4H.4 `d12f69fd...798c0`, Stage 4H.5D
`27dbc34b...7d35`, and Stage 4I.8 `28b9c675...8114`. The MicroPython backup
record still reports 21/21 byte-for-byte SHA-256 matches.

## Physical acceptance still required

A separately approved test should verify detection at the actual pad address,
every button and chord, disconnect/reconnect, Knight menu progression and
music, Manic Miner and Dizzy controls, Columns joystick input, and Thrust input.
It must also compare CPCPERF and CPCAUDIO against Stage 4I.8, specifically CPC
speed, audio underruns/overruns, audio deadline misses, queue peak, and stack
high-water. Host results cannot establish the RP2350 I2C polling cost.

**Physical deployment readiness: PASS for a controlled experimental A/B test;
production acceptance remains pending. Stage 4I.12 migration remains HOLD.**
