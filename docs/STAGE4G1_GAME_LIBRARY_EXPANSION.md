# Stage 4G.1 — Game Library Expansion

Date: 2026-10-29

Result: **HOLD before packaging or physical upload**

## Preservation result

- The deployed Stage 4F.6 artifact remains present and unchanged:
  `build-rp2350-stage4f6-sram-step/cpc_explorer_sram_step.uf2`
- Size: 685,056 bytes
- SHA-256: `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`
- No firmware was built or modified.
- No COM port, BOOTSEL volume, catalog, or physical flash was accessed.
- No game was uploaded, removed, overwritten, or repackaged.
- The existing BASIC, Knight Tyme, and Manic Miner installation was therefore not changed.

## Implemented loader and storage capabilities

The device-side installer accepts only deterministic `.cpcgame` containers holding one
standalone 64 KiB CPC snapshot. The payload must be exactly 65,792 bytes (256-byte SNA
header plus 65,536 bytes RAM), have the `MV - SNA` signature, and use snapshot version
1, 2, or 3. Version 2/3 snapshots must identify machine 0 (CPC464) at offset `0x6D`.
Version 1 has no explicit machine identity, so acceptance does not by itself prove that
the captured state is CPC464-compatible.

The current importer does not accept DSK, CDT, CPR, WAV, ZIP, loose BASIC, or loose BIN
files. Those inputs require a separately proved loader recipe that boots them on the
actual CPC464 core and captures a stable, playable snapshot. The project currently has
such title-specific recipes only for Knight Tyme and Manic Miner; it has no disk or tape
emulation path suitable for general conversion.

The `.cpcgame` format is a 16-byte package header, an 80-byte CPCLIB1 descriptor, and the
65,792-byte SNA payload (65,888 bytes total). Upload frames are limited to 1,024 bytes and
use explicit `BEGIN`/`CHUNK`/`COMMIT` sequencing, transport/package CRCs, payload readback
CRC, and an inactive-catalog transactional commit. Upload is available only at the launcher
and only on the validated EF-40-18 16 MiB flash.

Flash layout:

| Offset | Purpose |
|---|---|
| `0x000000-0x3FFFFF` | firmware, never writable by uploader |
| `0x400000-0x403FFF` | catalog A |
| `0x404000-0x407FFF` | catalog B |
| `0x408000-0xFEFFFF` | 4 KiB-aligned snapshot allocations |
| `0xFF0000-0xFFFFFF` | permanent E10 guard, never writable |

The snapshot region contains 12,484,608 bytes and can hold at most 179 aligned snapshot
allocations of 69,632 bytes. The catalog limit is 192 records, so allocation space is the
practical ceiling. Removed payload allocations are deliberately not reclaimed. The exact
current high-water mark was not queried because physical-device access was unnecessary
after the compatibility hold; it is known to include Manic Miner and the formerly removed
USB self-test allocation.

## Critical current-firmware input limitation

`src/gameplay_input.cpp` assigns a physical control profile only when the game ID is
exactly `manic_miner`. Every other uploaded game receives `Profile::None`; X, Z, and Y
produce no emulated CPC key events. The CPC core is also configured with
`CPC_JOYSTICK_NONE`.

Consequently a newly uploaded game may load and animate, but it is not physically playable
under the preserved Stage 4F.6 firmware. Reusing the `manic_miner` ID is both unsafe and
blocked by duplicate-ID validation. This is the decisive reason not to upload any candidate
in this stage.

## Local asset inspection

The named modern candidates were not present under `D:\Games\CPC Games`. A read-only scan
found 355 `.sna`/`.sna.gz` assets. Ten decode to a structurally importer-compatible 65,792
byte snapshot; six are game-like (`barb1`, `dizzy`, `flyshark`, `jtn`, `motos`, and
`ranarama`) and four are JavaCPC boot snapshots. These files are user-owned candidates,
but structural acceptance alone is insufficient: title/start/gameplay state and controls
have not yet been deterministically proved, and none can receive physical input from the
current firmware. They were not extracted or packaged.

## Creator-published candidate assessment

| Candidate | Published input | Observed/declared requirements and controls | Current result |
|---|---|---|---|
| Ramiro el Vampiro IV | EN/ES SNA (64 kB), CDT, DSK | Amstrad version; O/P left/right, Q or Space jump, A glide/talk, joystick | Official EN SNA downloaded read-only. It is 65,792 bytes, SNA v3, 64 KiB, SHA-256 `b4d32df4ac39f64439a11a2cec9b75cc534c65fb80cbcc114d2b08652a5164b3`, but machine byte is 2 (CPC6128). Importer correctly rejects it; changing the byte is not a conversion. Four gameplay actions also exceed the present three-button/no-profile input path. |
| KROOK | DSK/CDT/CPR only | CPC 64K; `RUN\"KROOK\"`; O/P, Q impulse, Space attack, plus reset/pause/exit | No supported standalone CPC464 SNA. Needs a proved disk/tape boot-and-capture workflow and at least four gameplay actions. Not packaged. |
| Pedrisco | DSK only (190 kB) | CPC464/664/6128 64K; `RUN\"PEDRISCO\"`; redefinable keys/joystick; AY music | Hardware claim is promising, but there is no supported DSK loader/converter. Four-direction puzzle input plus actions cannot be represented by the current three-button/no-profile path. Not packaged. |
| Domino 40th | DSK/CDT/SNA/WAV | Any key selects, Space/fire 1 places, P passes, left+fire 2 resets | Official SNA downloaded read-only. It is 34,378 bytes, SNA v3, SHA-256 `6870400c3085fa7b2d539424171085bab328196d24d7eb34ceb8609c4c2696b1`; it is a hardware-oriented/compact snapshot rather than a standalone 64 KiB SNA and reports machine byte 2. Importer correctly rejects its size. Not packaged. |
| Sean McManus collection | One `sean_2024.dsk` containing multiple BASIC/BIN games | Creator-published free disc; menu is `RUN\"disc\"`. Examples such as Wallbuster and Paclone use cursor keys; Alien Intervention has its own on-screen controls. | DSK is unsupported and several programs depend on files from the disc. A per-title boot/capture recipe would be required. Current firmware cannot inject their cursor/action keys. Not packaged. |

No paid or unauthorised commercial image was downloaded. The two official SNA downloads
remain in the Windows Downloads folder and were not added to the project or Git.

## Importer and host validation

- Ramiro import result: `unsupported SNA machine: CPC6128 (CPC464 identity 0 required)`.
- Domino import result: `only standalone 64 KiB SNA files are supported; got 34378 bytes`.
- `py tests/importer_test.py`: PASS (determinism, CPC464 machine policy, existing Knight
  package, removal confirmation).
- Host launcher and game-library tests: 2/2 PASS.
- No candidate progressed to framebuffer/gameplay validation because it failed an earlier
  format or physical-control gate.

## Performance-test matrix (proposed, not yet installed)

| Candidate | Expected workload | Repeatable scene needed after conversion | Present blocker |
|---|---|---|---|
| Ramiro | scrolling/action-adventure sprites, music | fixed room with movement, jump, and glide/talk | CPC6128 SNA identity; four actions; no profile |
| KROOK | puzzle-platform movement, falling control, attack | fixed level with moving platform, impulse, and attack | DSK/CDT only; four actions; no profile |
| Pedrisco | 16-colour grid animation, cascades, AY music | same level and deterministic multi-chain cascade | DSK only; directional/action input; no profile |
| Domino 40th | mostly static board with turn updates | fixed difficulty and opening sequence through several moves | compact/hardware SNA; no profile |
| Paclone or Wallbuster from Sean's disc | BASIC-heavy motion/sprites and sound | fixed start and 30-second cursor-controlled play | DSK dependency; cursor input; no profile |

Display modes and exact active resolutions must be measured from a genuine CPC464 snapshot;
they are intentionally not guessed from download-page descriptions.

## Recommendation

Do not upload any new game under the current immutable firmware. The smallest safe next
step is a separately scoped host-only control-profile stage: choose one title, produce or
obtain a genuine CPC464 64 KiB standalone snapshot, prove title and gameplay in the host
core, then define a verified X/Z/Y mapping only if three controls are sufficient. Of the
official candidates, Domino has the simplest interaction model, but its compact SNA still
needs a legitimate CPC464 boot-and-capture path. Of the local structurally compatible
snapshots, a game with exactly left/right/fire is the best first candidate after its actual
controls and licensing provenance are confirmed.

Physical catalogue after this investigation: unchanged — Locomotive BASIC, Knight Tyme,
and Manic Miner only.
