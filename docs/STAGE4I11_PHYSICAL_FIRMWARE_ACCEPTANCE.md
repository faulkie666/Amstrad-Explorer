# Stage 4I.11 — Controlled Physical Firmware Acceptance

## Current result

**PARTIAL PASS — firmware deployment, storage integrity, Manic Miner gameplay,
audio, and runtime telemetry passed. Complete four-game physical acceptance is
still HOLD because BASIC and Knight Tyme were not reported in this test.**

CPXPCK2 pack: **NOT INSTALLED**  
Library migration: **NOT AUTHORISED / NOT PERFORMED**  
Catalogue modification: **NOT AUTHORISED / NOT PERFORMED**

## Identified target and starting state

| Field | Verified value |
|---|---|
| Target serial | `<device-serial>` |
| BOOTSEL USB | `2E8A:000F`, RP2350 revision A2, QFN80 |
| Starting firmware | `0.4.15-stage4h4ayaudio` |
| Starting flash | `EF-40-18`, 16,777,216 bytes |
| Starting upload state | `idle`, received `0` |
| Starting game list | Locomotive BASIC, Knight Tyme, Manic Miner, Dizzy |

Picotool device discovery without a serial selector completed without ambiguity;
the same target was then selected explicitly by VID, PID, and serial for the
installation. No force-reset option was used.

## Backup and rollback gates

Both independently preserved Stage 4I.10 images existed, were exactly
16,777,216 bytes, and re-hashed to:

`c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`

- Primary: `%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I10-20261030-110313\cpc-explorer-full-16m-read1.bin`
- Recovery: `%USERPROFILE%\CPCExplorerBackups\Stage4I10-20261030-110313\cpc-explorer-full-16m-verified.bin`

`validate_flash_backup.py` revalidated the primary image before installation.
Neither backup was modified.

The Stage 4H.4 rollback UF2 remains present and unchanged:

- File: `build-rp2350-stage4h4-continuous-audio/cpc_explorer_ay_audio_continuous.uf2`
- Size: 690,688 bytes
- SHA-256: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`

Rollback was not performed and remains a separately authorised future write.

## Authorised UF2 and address policy

| Field | Verified value |
|---|---|
| File | `build-rp2350-stage4i8-migration/cpc_explorer_flash_migration_exp.uf2` |
| Identity | `0.4.20-stage4i8migrationexp` |
| Size | 733,184 bytes |
| SHA-256 | `28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114` |
| Physical blocks | 1,432 |
| Application blocks | 1,431, family `0xe48bff59` |
| Application targets | `0x10000000–0x100596ff` |
| E10 exception | one exact block at `0x10ffff00`, family `0xe48bff57`, flags `0x0000a000`, extension `0x9957e304`, 256 bytes of `0xef` |

The ordinary targets are wholly inside the first 4 MiB firmware reservation.
There are no catalogue, legacy-game, CPXPCK2 staging, or other game-storage
targets and no unexpected out-of-range block.

## Programming verification

Official USB-enabled picotool 2.3.1 performed one device-specific operation:

`load -v -x <authorised UF2> -t uf2 --vid 0x2e8a --pid 0x000f --ser <device-serial>`

The entire backup, rollback, candidate, UF2-policy, candidate-identity, and
device-identity preflight was in the same fail-closed PowerShell block as this
operation. Picotool reported 100% load, 100% verification, `OK`, and rebooted
the device to the application.

Installation attempts: **1**. There was no retry. An earlier PowerShell block
was rejected by the parser before executing any command and therefore made no
device access or installation attempt.

## Boot and protocol result

The application re-enumerated as `2E8A:0009` on COM11 and reported:

```text
CPCU1 READY fw=0.4.20-stage4i8migrationexp flash=EF-40-18 capacity=16777216 entries=4
CPCU1 STATUS upload=idle flash=EF-40-18 capacity=16777216 entries=4 received=0 migration=legacy migration_received=0
```

The read-only migration query reported:

```text
CPCU1 MIGRATION LAYOUT flash=16777216 firmware=000000-3fffff catalog_a=400000-403fff catalog_b=404000-407fff legacy=408000-43afff pack=43b000-466fff guard=ff0000-ffffff
CPCU1 MIGRATION STATUS state=legacy received=0 error=none
```

This confirms the migration protocol is available, no pack is staged or
active, and the legacy catalogue remains authoritative.

## Legacy catalogue and flash integrity

The post-installation read-only list is unchanged:

0. Locomotive BASIC (`locomotive_basic`)
1. Knight Tyme (`knight_tyme`)
2. Manic Miner (`manic_miner`)
3. Dizzy (`dizzy`)

The firmware's read-only `backup-library` command returned all 241,664 bytes
covering `0x400000–0x43afff`. The result is byte-identical to the same range in
the independently captured Stage 4I.10 full-flash image.

- Post-installation legacy-region SHA-256:
  `88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`
- Readback file:
  `%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I11-postfirmware-20261030`
- First differing byte: none

Therefore both legacy catalogues and every legacy allocation are unchanged by
the firmware-only installation.

## Physical gameplay, AY audio, performance, and stack telemetry

The user reports **Manic Miner PASS**: gameplay and music both worked on the
physical Explorer.

The captured serial trace itself begins `menu launch=Dizzy` and therefore its
numeric telemetry is labelled as the exact installed Dizzy state, not Manic
Miner. It must not be used as Manic Miner performance evidence.

### Captured Dizzy telemetry

- cold/launch CPC speed: 66.4–76.4%; settled speed approximately 80.1–82.5%
  with a representative median near 81.3%;
- settled fast-Z80 execution average: approximately 22.1–22.6 ms per 20 ms CPC
  slice;
- physical presentation: approximately 7.8–10.8 FPS;
- every emulation slice missed its 20 ms real-time deadline (`missed=slices`);
- independent audio-renderer deadline misses: **0**;
- DMA audio underruns: **0**; overruns: **0**;
- renderer maximum: 618 us; queue peak: 16/128; queue-full events: 0;
- PSG sequence errors: 0;
- PSG events reached 58 and then stopped increasing; late events also reached
  58, consistent with this slower-than-real-time producer;
- PCM generation continued from 22,784 to 760,576 samples and DMA playback
  from 176 to 5,940 blocks, exactly 128 samples per block;
- core-1 stack high-water: **712 bytes used**. This is a used-byte count, not
  remaining space. Against the 2,048-byte core-1 stack it leaves 1,336 bytes
  and is below the established physical acceptance limit.

The final serial read ended with a Windows `ClearCommError` after the device
ceased to be available. All complete telemetry lines before that event were
internally consistent; the disconnect is recorded and is not concealed.

### Music/AY classification

| Entry | Evidence-based classification |
|---|---|
| Locomotive BASIC | No original game soundtrack exists. An idle BASIC session is expected to be silent; this differs from a game issuing PSG commands that happen to yield silence. |
| Knight Tyme | Established host evidence shows sustained structured three-channel AY music after its menu sequence. It was not physically reported in this acceptance run. |
| Manic Miner | Established sustained one-channel AY music; physical gameplay and audible music passed in this run. |
| Dizzy | The exact captured state issues a small, finite set of AY commands but has no sustained music-producing sequence. The physical trace reached 58 ordered events while continuous PCM/DMA output remained underrun-free. This is "AY commands but effectively silent/brief effect", not "no AY activity" and not a claim that every Dizzy release lacks music. |

BASIC and Knight Tyme physical results remain unreported. Dizzy was launched
according to serial evidence, but no user LCD/control/audio verdict was supplied.

## Issues and recovery boundary

No firmware-installation, boot, flash-identity, catalogue, legacy-data, audio
underrun, or runtime-stack issue is observed. The final USB disappearance and
the missing BASIC/Knight Tyme physical reports prevent a complete four-game
PASS. No automatic recovery or reconnect action was taken.

If a physical test fails, stop using the experimental firmware. A rollback
would require re-entering BOOTSEL, re-identifying serial `<device-serial>`,
rechecking the pinned Stage 4H.4 UF2 hash and both recovery images, then making
one separately approved verified picotool load. No rollback is authorised by
Stage 4I.11 and none has been attempted.

## Explicit non-actions

- CPXPCK2 pack: **NOT INSTALLED**
- Library migration: **NOT AUTHORISED**
- Catalogue modification: **NOT AUTHORISED**
- `stage-library`: **NOT INVOKED**
- `activate-library`: **NOT INVOKED**
- Game upload/removal: **NOT PERFORMED**
- Automatic rollback: **NOT PERFORMED**
