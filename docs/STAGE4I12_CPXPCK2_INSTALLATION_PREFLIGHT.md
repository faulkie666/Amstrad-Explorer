# Stage 4I.12 — CPXPCK2 Installation Preflight

## Result

**HOST PREFLIGHT PASS; PHYSICAL STAGING HOLD.**

The existing CPXPCK2 artifact, Stage 4I.8 firmware identity, decompression,
checksums, address boundaries, interruption model, and staging dry run all
pass. The Explorer was not present as `2E8A:0009` when this preflight ended, and
no physical CPXPCK2 write is authorised in this stage.

Flash writes: **NONE**  
Catalogue activation: **NOT PERFORMED**  
Pack staging: **NOT PERFORMED**

## Pinned artifacts

| Artifact | Size | SHA-256 |
|---|---:|---|
| `artifacts/stage4i7-internal-library/library.cpxpack` | 179,360 | `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe` |
| `build-rp2350-stage4i8-migration/cpc_explorer_flash_migration_exp.uf2` | 733,184 | `28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114` |

The firmware identity physically installed and previously verified is
`0.4.20-stage4i8migrationexp`. Before any future write it must be observed
again from the uniquely identified Explorer.

Both Stage 4I.10 recovery images remain the required pre-write safety gate:

- 16,777,216 bytes each;
- SHA-256
  `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`.

## Pack inspection

The CPXPCK2 inspector used the preserved vendored LZ4 dependency and reported:

- format/version: CPXPCK2 v2;
- two valid publication headers and no header errors;
- six records, each decompressing to a 65,792-byte CPC snapshot;
- records CRC-32 `1f5e3acb`;
- payload CRC-32 `4cc05874`;
- complete pack CRC-32 `ec7fd1e9`;
- complete image SHA-256 exactly matching the pinned value.

| ID | Title | Compressed bytes | Source snapshot SHA-256 |
|---|---|---:|---|
| `columns` | Columns | 21,263 | `53c2647927a99f0cff4a8341321e8b8ad1afa876a59eb271acd3dd9eb8c5755b` |
| `dizzy` | Treasure Island Dizzy | 41,356 | `2086041545be0bbdaa4e87a60c86d0e69e7d41c27242621d78416ceed34da943` |
| `fruity_frank` | Fruity Frank | 22,373 | `d08e0dd8e610cafa10a1126e788c98492b31a651f0842a8f51a6a65d97a4c9bc` |
| `knight_tyme` | Knight Tyme | 32,801 | `ca5fd1c8fe5b72694038fa6263349958d0537fe04c5605e49e489cba13f65156` |
| `manic_miner` | Manic Miner | 27,447 | `6661c74f919e228b2d21e1c0fceba7369c80281c1e27c53d03d624b1ff6d743d` |
| `thrust` | Thrust | 17,691 | `1c4da26f7ee88a23921c6ca0f5e7996cdf1bae005d522ed56305e69f79412078` |

The established menu policy exposes five validated games; Fruity Frank remains
the non-visible proof/record described by Stage 4I.7 and must not silently
become a launchable entry during physical staging.

## Flash boundaries and capacity

| Region | Address range | Preflight result |
|---|---|---|
| Firmware | `0x000000–0x3fffff` | outside migration writes |
| Catalogue A | `0x400000–0x403fff` | legacy publication metadata; remains authoritative until separately approved activation |
| Catalogue B | `0x404000–0x407fff` | same protection/publication ordering |
| Legacy allocations | `0x408000–0x43afff` | preserved |
| CPXPCK2 image | `0x43b000–0x466c9f` | 179,360-byte exact image |
| Reserved staging extent | `0x43b000–0x466fff` | sector-bounded; no overlap |
| E10 guard | `0xff0000–0xffffff` | permanently excluded |

The offline audit reports 12,095,488 bytes after the reserved staging extent
before the E10 guard. The Stage 4I.10 backup proved the staging extent was
erased at capture time. A future physical preflight must not assume it remains
erased; it must query migration state and fail closed on anything other than
`state=legacy received=0 error=none`.

## Offline migration and interruption audit

`tools/stage4i7_migration_audit.py` passed with JEDEC `EF-40-18`, capacity
16,777,216, catalogue version 1, the exact pack, and the exact installed
firmware artifact. It confirms:

- partial staging never publishes the CPXPCK2 catalogue;
- legacy catalogue/game data remain the recovery authority through staging;
- publication headers are withheld until payload/table verification;
- no simulated interruption permits automatic retry;
- the active catalogue is changed only by the separately gated activation
  operation.

The audit utility is deliberately offline and has no physical writer. The
physical transport is implemented separately in `tools/cpc_game.py` and was
exercised here only in its default dry-run mode.

## Dry-run result

The exact future staging command was invoked **without** `--execute`, `--port`,
or confirmation. It revalidated pack size/CRC/SHA and firmware SHA, then
reported:

```text
DRY RUN ONLY: no serial port opened and no device write attempted
```

No `stage-library`, `activate-library`, or combined migration write was sent to
the device.

## Required physical pre-write gates

A later, separately approved staging stage must perform all checks in one
fail-closed execution block before the first byte is written:

1. Re-hash both 16 MiB recovery images, the pinned CPXPCK2 pack, and the exact
   Stage 4I.8 UF2.
2. Detect exactly one `2E8A:0009` Explorer and bind it to serial
   `<device-serial>`; do not assume COM11.
3. Require firmware `0.4.20-stage4i8migrationexp`, JEDEC `EF-40-18`, capacity
   16,777,216, upload idle, four legacy entries, and no device error.
4. Require migration `state=legacy received=0 error=none` and the exact reported
   layout.
5. Re-read `0x400000–0x43afff` and require byte identity with the Stage 4I.10
   backup before staging.
6. Confirm no other serial monitor owns the port and the launcher menu is idle.
7. Stage once with explicit port and the exact `--execute --confirm STAGE`
   tokens. Do not retry a timeout or ambiguous response.
8. Require device-side `VERIFIED`, a complete pre-activation readback matching
   the pack with both 256-byte publication header pages intentionally erased,
   and final `state=verified`.
9. Stop. Do not activate in the same approval or command.

The prepared write command, **not executed**, is:

```powershell
py tools/cpc_game.py stage-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4i8-migration/cpc_explorer_flash_migration_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114 `
  --port <REDETECTED_PORT> --execute --confirm STAGE
```

This command is documented solely for the next independently authorised stage.
It was not run with write-enabling arguments.

## Remaining holds

- The Explorer was no longer enumerated when the preflight ended; target
  identity and state must be re-established.
- Stage 4I.11 has a physical Manic Miner PASS, but BASIC and Knight Tyme were
  not reported and the capture ended with a USB disappearance.
- Physical CPXPCK2 staging requires new explicit approval.
- Catalogue activation requires a later, separate approval after independently
  verified staging/readback.
