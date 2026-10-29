# Stage 4G.1 — Expanded Arcade Collection Assessment

Date: 2026-10-29

Result: **HOLD — no new game currently satisfies the unchanged firmware's complete
format, host-validation, and physical-control requirements.**

The machine-readable, per-game catalogue is
`artifacts/stage4g1-arcade/arcade_inventory.csv`. It contains one row for every top-level
game directory, including the source file set, deterministic manifest SHA-256, detected
formats, size, compatibility status, control status, and variant-group information.

Inventory CSV SHA-256:
`5f29fe672d2aadd5389de5e7f0157dd11b82114b8eee9a98843b236c5cfa10db`

## Preservation and device-write result

- Source collection `D:\Games\CPC Games\Arcade` was read and hashed only. No source file
  was changed, extracted into, renamed, or deleted.
- Stage 4F.6 firmware remains unchanged at SHA-256
  `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`.
- No firmware build, UF2, BOOTSEL, COM access, USB upload, removal, catalog write, or flash
  operation occurred.
- Locomotive BASIC, Knight Tyme, and Manic Miner remain the intended installed catalogue.
  The physical catalogue was not queried because no candidate reached the upload gate and
  the user's menu/no-other-monitor condition could not be established remotely.

## Inventory

| Item | Count |
|---|---:|
| Top-level game directories | 1,593 |
| All directories, including nested support directories | 1,700 |
| Files | 9,593 |
| Total source bytes | 94,063,228 |
| SNA / SNA.GZ | 0 |
| DSK / DSK.GZ | 95 (all GZip-compressed) |
| CDT | 0 |
| ZIP / 7z / RAR | 0 |
| CPR | 1 (`Alien Tank Attack/TANK.CPR`, 3,584 bytes) |
| BAS files | 402 |
| BIN files | 2,169 |
| Extensionless files | 1,320 |
| Game folders containing DSK images | 71 |
| Game folders containing multiple DSK images | 18 |
| Game folders containing BAS | 326 |
| Game folders containing BIN | 832 |
| Game folders with nested directories | 53 |

A magic-byte scan of every file found no hidden SNA, DSK, CDT, or ZIP image under an
unusual extension. All 95 GZip streams decode; 94 contain a standard CPCEMU/extended DSK
signature and one (`Academy/academye.dsk.gz`) uses the older `MV - CPCEMU / ...` disk
signature. The decoded disk images have no exact SHA-256 duplicates.

## Duplicates and alternatives

There are no exact duplicate complete folder manifests and no exact duplicate decoded DSK
images. A conservative title-normalisation pass identified 24 probable alternative-version
groups covering 54 directories. Examples include:

- Four Antiriad language editions.
- A-Team and A-Team Gunstick.
- Double Dragon English and Spanish.
- Paperboy English and Spanish.
- Operation Wolf and Operation Wolf Lightgun.
- Four Pac-Man-labelled implementations.
- Multiple Gauntlet, Tetris, Cobra, Froggy, and Wriggler implementations.

These are marked with `CanonicalTitle` and `VariantGroupSize` in the CSV. They are not
silently deduplicated because same-name/language/gunstick editions may have different code
or controls.

Explicitly labelled unsupported-memory candidates include `Rick Dangerous 128+`,
`Robotron. 6128`, and `Star Sabre 128k Edition`. Numerous `file_id.diz` descriptions also
mention 128K or CPC6128, but those mentions are not treated as proof that the corresponding
program requires 128K. Machine requirements must be demonstrated during an actual CPC464
boot/capture test.

## Compatibility result

The existing uploader accepts only a 65,792-byte standalone CPC464 64 KiB SNA inside a
65,888-byte `.cpcgame` package. This Arcade directory contains no SNA snapshots.

The 95 compressed DSK images cannot be uploaded directly. The remaining folders are mostly
extracted AMSDOS-era file sets containing loaders, BASIC, binaries, screens, levels, and
other support files. A collection of files is not equivalent to a bootable snapshot: load
order, AMSDOS addresses, execution address, ROM state, dependencies, and post-start disk
access must all be proved. The project has no generic DSK/CDT runtime and only has verified
title-specific host-loading recipes for Knight Tyme and Manic Miner.

Manic Miner is the sole compatible directory in this tree, and it is already installed.
All other 1,592 rows are marked `UNSUPPORTED_CURRENT_LOADER`, meaning “requires a
title-specific CPC464 boot/capture and gameplay proof”, not necessarily that the original
game cannot run on a CPC464.

There is a second independent blocker: Stage 4F.6 gives physical gameplay input only to
the exact ID `manic_miner`. Every other game ID receives `Profile::None`, and the CPC core
uses `CPC_JOYSTICK_NONE`. A new title could therefore render but could not receive X/Z/Y
input. Reusing `manic_miner` would collide with and threaten the preserved entry and is
correctly rejected by the transactional installer.

No package was generated merely from loose files, and no DSK was renamed or truncated.

## Capacity

| Quantity | Value |
|---|---:|
| Snapshot storage region | 12,484,608 bytes |
| `.cpcgame` transport file | 65,888 bytes |
| SNA payload | 65,792 bytes |
| Flash allocation per installed snapshot | 69,632 bytes (17 × 4 KiB sectors) |
| Physical snapshot-allocation ceiling | 179 |
| CPCLIB1 descriptor ceiling | 192 |
| Known consumed allocations | 2 (removed USB self-test tombstone/high-water allocation plus Manic Miner) |
| Maximum additional allocations from known state | 177 |
| Remaining allocation bytes from known state | 12,345,344 |
| Additional Arcade directories excluding already-installed Manic Miner | 1,592 |
| Space if all 1,592 became valid snapshots | 110,854,144 bytes |
| Storage shortfall | 98,508,800 bytes / 1,415 games |

The catalog would allow more records than flash can hold; storage is the binding limit. A
future selection must choose no more than 177 additional snapshots unless a new, separately
approved storage/reclamation architecture is designed. Existing entries must not be removed
to increase that number.

## Installation result

No first-game upload was attempted because no new candidate passed the prerequisite gates:

1. No directly supported SNA exists in the Arcade directory.
2. No general, verified DSK or loose-file conversion exists.
3. No new title has a Stage 4F.6 physical input profile.
4. Launch, controls, post-start media independence, and deterministic host state therefore
   cannot yet be verified.

Uploading despite these failures would consume a non-reclaimable allocation and create an
unplayable menu entry. The safe installed catalogue remains unchanged.

## Benchmark-oriented future selection

The following source folders provide useful workload diversity after a legitimate CPC464
snapshot and three-button control mapping are proved:

| Workload | Candidate folders | Current source form | Gate still required |
|---|---|---|---|
| Fast arcade | `Bomb Jack`, `Commando`, Pac-Man variants | extracted BIN/extensionless sets | loader recipe, controls, CPC464 proof |
| Scrolling/shooter | `R-Type`, `Gryzor`, `Turrican II` | multi-file extracted sets | dependency/media audit, controls |
| Platformer | `Chuckie Egg`, `Bruce Lee` | small extracted sets | loader recipe and verified key mapping |
| Sprite-heavy | `Bubble Bobble`, `Gauntlet (Us Gold)` | extracted set / DSK.GZ | snapshot capture and multiplayer/input limits |
| Graphically complex | `Turrican II`; `Prehistorik 2` | extracted set / DSK.GZ | memory requirement and CPC464 proof |
| BASIC-heavy | `Astro Attack`, `Autocras-Autocrash`, `Beurk Roger` | small BAS-led sets | validated BASIC/BIN load sequence and controls |

These are recommendations for staged host investigation, not compatibility claims.

## Safest path to “as much as possible”

With firmware changes prohibited, the current maximum number of **new playable games is
zero**, because Stage 4F.6 exposes no generic control profile. The safe next step would be a
separately authorised host-only conversion study, starting with one small BAS-led or
left/right/fire title. It should prove AMSDOS metadata, loading, CPC464/64K operation,
media independence, framebuffer progression, and exact controls. Physical installation can
resume only after the unchanged deployed firmware is shown capable of controlling that exact
game; otherwise a future firmware control-profile stage is unavoidable.
