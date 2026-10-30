# Stage 4I.10 — Verified Physical Flash Backup

## Result

**PASS — Complete, verified 16 MiB backup secured.**

Physical flash writes: **NONE**  
Deployment: **NOT AUTHORISED / NOT PERFORMED**

## Device identity

| Field | Observed value |
|---|---|
| Normal application port | COM11 (uniquely detected before BOOTSEL) |
| Installed firmware | `0.4.15-stage4h4ayaudio` |
| Application USB | `2E8A:0009` |
| Flash JEDEC | `EF-40-18` |
| Flash capacity | 16,777,216 bytes |
| Upload state | `idle`, received `0` |
| BOOTSEL USB | `2E8A:000F` |
| BOOTSEL product | `RP2350 Boot` |
| BOOTSEL volume | `H:`, label `RP2350` |
| `INFO_UF2.TXT` | UF2 Bootloader v1.0; Model/Board-ID `Raspberry Pi RP2350` / `RP2350` |
| RP2350 revision/package | A2 / QFN80 |
| Chip ID / explicit selector | `<device-chip-id>` / `<device-serial>` |
| Reported flash size | 16,384 KiB |

The device was selected for every picotool operation with explicit VID, PID,
and serial arguments. No automatic reset or force-reset option was used.

## Catalogue contents before BOOTSEL

The read-only CPCU1 `detect`, `status`, and `list` commands reported exactly:

0. Locomotive BASIC (`locomotive_basic`)
1. Knight Tyme (`knight_tyme`)
2. Manic Miner (`manic_miner`)
3. Dizzy (`dizzy`)

There were no extra entries, active upload, or reported application error.

## Official picotool

- Version: `picotool v2.3.1 (Windows, GNU-16.2.0, Release)`
- Executable: `%USERPROFILE%\CPCExplorerHostTools\picotool-2.3.1\picotool\picotool.exe`
- Executable SHA-256: `7c1bf5b49b3b1c3b8094ca300c9aac45d15d205adbc54fcc6d6d209f6f69e60d`
- Official standalone archive: `picotool-2.3.1-x64-win.zip`
- Archive SHA-256: `68730be0813f8f35be2cca147cf7f1572662d5dcf0f5ba468e02a6dd9e85db2b`
- The archive hash matched the digest published on the official
  [Raspberry Pi SDK tools release](https://github.com/raspberrypi/pico-sdk-tools/releases/tag/v2.3.1-0).
- USB `info`, `save`, and `verify` support was confirmed from the installed
  executable and the [official picotool documentation](https://github.com/raspberrypi/picotool).

The host install is isolated from the project and existing SDK tools. No USB
driver was changed, no administrator access was requested, and the extracted
executable is not Authenticode-signed; authenticity is anchored to the exact
official release-asset digest above.

## Backup and independent verification

Primary backup:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I10-20261030-110313\cpc-explorer-full-16m-read1.bin`

| Check | Result |
|---|---|
| Exact size | 16,777,216 bytes — PASS |
| SHA-256 | `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1` |
| `picotool save -v` | PASS |
| Separate `picotool verify` over `0x10000000–0x10ffffff` | PASS |
| Second independent 16 MiB read | PASS; byte-identical SHA-256 |
| Read-only file attribute | Set |

Independent second read:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I10-20261030-110313\cpc-explorer-full-16m-read2.bin`

Separate recovery copy outside OneDrive/project builds:

`%USERPROFILE%\CPCExplorerBackups\Stage4I10-20261030-110313\cpc-explorer-full-16m-verified.bin`

The second read and separate recovery copy are each 16,777,216 bytes and have
the same SHA-256 as the primary image. Both recovery images have the Windows
read-only attribute set. Extracted catalogues, snapshots, and the validation
JSON are under `verified-extraction` beside the primary image.

## Legacy catalogue validation

`tools/validate_flash_backup.py` passed its corruption-rejection self-test and
validated the physical image's size, complete-image hash, CPCLIB1 headers,
commit markers, header/descriptor CRCs, allocation geometry, SNA headers,
payload CRCs, and non-overlap rules.

| Slot | Role | Generation | Entries | High-water | SHA-256 |
|---|---|---:|---:|---:|---|
| 0 at `0x400000` | active | 5 | 2 | 241,664 | `81e4e654649dfcf861bbd308d233416712c189be6ebd72e04eab6ff922647933` |
| 1 at `0x404000` | valid older fallback | 4 | 1 | 172,032 | `25643c50675b04b130c90642370c17c97f95651b6455b90ad58cdb031ea8b08e` |

The active catalogue contains Manic Miner and Dizzy. The older valid slot is
the expected transactional predecessor and contains Manic Miner. Catalogue
copies need not contain identical generations or entry sets before migration;
the highest valid generation is authoritative.

## Four-game integrity

Locomotive BASIC and Knight Tyme are firmware-resident built-ins. They do not
have CPCLIB1 external-flash allocations. Manic Miner and Dizzy are the two
active external allocations. Thus the four visible games are verified as:

| Game | Storage | Evidence |
|---|---|---|
| Locomotive BASIC | application firmware | reported by physical CPCU1 list; application bytes match pinned Stage 4H.4 UF2 |
| Knight Tyme | application firmware built-in snapshot | reported by physical CPCU1 list; application bytes match pinned Stage 4H.4 UF2 |
| Manic Miner | `0x419000`, 65,792 bytes | CRC `9584cfc4`; SHA-256 `6661c74f919e228b2d21e1c0fceba7369c80281c1e27c53d03d624b1ff6d743d` |
| Dizzy | `0x42a000`, 65,792 bytes | CRC `1e76d62f`; SHA-256 `2086041545be0bbdaa4e87a60c86d0e69e7d41c27242621d78416ceed34da943` |

All 1,348 ordinary application blocks in the preserved Stage 4H.4 UF2 match
the captured flash byte-for-byte (`0x10000000–0x100543ff`). Its sole authorised
E10 compatibility block is separate. No ordinary application UF2 block targets
catalogue or game storage.

## Flash-layout checks

- Firmware reservation SHA-256 (`0x000000–0x3fffff`):
  `6c1ce74c921bf5db10deefda229ac02315bcae5a4d3e53079c14819716d1965e`
- E10 guard SHA-256 (`0xff0000–0xffffff`):
  `de0b4cd0d4aad641cf95fab9beffbee666986954b5bdad6baf3dc2d84b175018`
- Proposed CPXPCK2 staging range: `0x43b000–0x466fff`, exactly 180,224
  bytes, wholly inside game storage and outside both catalogues, active
  allocations, firmware, and E10 guard.
- The proposed staging range is currently erased (`0xff` throughout); SHA-256:
  `6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`.

## Rollback material

Pinned Stage 4H.4 rollback UF2:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer\build-rp2350-stage4h4-continuous-audio\cpc_explorer_ay_audio_continuous.uf2`

- Size: 690,688 bytes
- SHA-256: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Status: unchanged

The full-flash image is the authoritative recovery material for firmware,
catalogues, payloads, unused space, and the E10 guard. A future restore is a
separate mutating operation and is not authorised by this stage.

## Next physical stage — checklist only

Each write-bearing step requires its own explicit approval and fail-closed
preflight. None was executed here.

1. Re-hash the Stage 4I.8 UF2 and CPXPCK2 pack against their pinned values.
2. Deploy the experimental firmware once through a freshly re-identified
   RP2350 BOOTSEL device; do not retry an ambiguous copy.
3. Before migration, verify normal USB identity, legacy four-game menu,
   launcher controls, and read-only migration status.
4. Stage the compressed pack once; independently read back and verify every
   byte while the legacy catalogue remains authoritative.
5. Activate only after staged-pack CRC, manifest, destination ranges, legacy
   catalogue state, and recovery image are revalidated.
6. Verify Locomotive BASIC and all migrated games launch; test controls,
   persistence, and the expected menu hierarchy.
7. Verify AY output using the accepted Knight Tyme/Manic Miner procedures and
   record audio telemetry without changing emulator timing.
8. Record runtime stack/headroom telemetry and compare against the 256-byte
   host gate.
9. If acceptance fails, stop. Roll back with the pinned Stage 4H.4 UF2 only
   under separate approval, then verify the preserved legacy experience. Use
   the full-flash image only under an independently reviewed restore plan.

No `stage-library`, `activate-library`, picotool load/erase/partition/OTP,
UF2 copy, game upload, catalogue write, or migration command was run.
