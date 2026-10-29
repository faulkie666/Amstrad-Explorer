# Stage 4G.2 — Generic controls and Chuckie Egg conversion feasibility

Date: 2026-10-29

Result:

- **PASS (host):** Treasure Island Dizzy is a valid CPC464/64K snapshot with a
  completely verified three-button gameplay mapping and a deterministic package.
- **PASS (host conversion):** the original Chuckie Egg BASIC/BIN pair boots through
  its AMSDOS binary semantics, produces a deterministic standalone snapshot, reloads
  without media, reaches its title and gameplay, and packages successfully.
- **HOLD (physical Chuckie Egg):** the game requires five logical gameplay controls
  (up/down/left/right/jump). Three Explorer buttons cannot represent them directly.
- No firmware/UF2 was built. No COM port or physical device was accessed.

## Preserved physical baseline

`build-rp2350-stage4f6-sram-step/cpc_explorer_sram_step.uf2` remains byte-for-byte
unchanged:

`65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`

The generic-control changes exist only in source and host tests. They have not been
compiled into or deployed over Stage 4F.6.

## First three-button candidate: Treasure Island Dizzy

Source:

`D:\Games\CPC Games\Sta\d\dizzy.sna.gz`

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Original GZip | 35,270 | `0e655d1bd1ba743356e8512cbf244a75feadc2697483494babf83fdc54618a8f` |
| Decompressed SNA | 65,792 | `2086041545be0bbdaa4e87a60c86d0e69e7d41c27242621d78416ceed34da943` |
| `dizzy.cpcgame` | 65,888 | `780423a9bdf589b838d564e0dc6c53726902d3ea89b094cbd444fc588cb77c5e` |

Importer result: CPC464, 64 KiB RAM, SNA v3, payload CRC32 `1e76d62f`, package
CRC32 `80f7df68`.

The snapshot was reloaded from the identical state before every input trial. Space is the
only title-screen key that changes the state into gameplay. Once started, deterministic
framebuffer comparisons and visual inspection established:

| CPC key | Result |
|---|---|
| Z | move left |
| X | move right |
| Space | jump |
| P | pause |
| Q | return to title/abort game |

Proposed Explorer mapping is therefore X→Z, Z→X, Y→Space. This is a complete playable
three-button mapping; pause and abort remain unavailable but are not needed for ordinary
gameplay.

The source now provides reusable binding profiles rather than hard-coding every game to
Manic Miner's keys:

- `KeysQWShift` retains Manic Miner Q/W/bare-Shift exactly.
- `KeysZXSpace` provides Dizzy Z/X/Space.
- `profile_for_game("dizzy")` selects `KeysZXSpace`.
- Launch-button release suppression, level holds, releases, and simultaneous transitions
  remain in the shared adapter.

This establishes a host-proven, physically suitable next game, but it is not a physical
acceptance result. A future separately authorised firmware build and deployment is required
before Dizzy can be uploaded and tested on the Explorer.

Evidence:

- `artifacts/stage4g2-controls/dizzy/probe/neutral.png` — title screen.
- `artifacts/stage4g2-controls/dizzy/probe/key-20.png` — Space reaches gameplay.
- `artifacts/stage4g2-controls/dizzy/gameplay-probe/` — neutral, left, right, jump,
  pause and abort trials.

## Chuckie Egg source inspection

Original files were read only:

| File | Bytes | AMSDOS type | Load | Logical bytes | Execute | SHA-256 |
|---|---:|---:|---:|---:|---:|---|
| `CHUCKIE` | 896 | 0 (tokenized BASIC) | `0x0170` | 737 | `0x0000` | `ccf60d743fd4565f473d3d061dfef1b561edd4340d449e8bbec5168dbacb5811` |
| `CHUCK.BIN` | 9,216 | 2 (binary) | `0x8000` | 9,022 | `0x9A97` | `3673e6da4ff52ca1f610b9d0ce688f8896f704dc3d2c732e2aed4c79207a655d` |

Both AMSDOS checksums match the sum of header bytes 0–66.

The BASIC loader contains presentation/instruction lines followed by line 200:

`RUN"!CHUCK"`

The loader text explicitly says that all direction and jump keys are redefinable and that
joystick directions may be used during redefinition. The title program accepts K to begin
keyboard redefinition, S to start, and then 1–4 for the player count.

## Reusable conversion approach

`cpc_snapshot_builder amsdos-basic-bin` is a general host-only path for the restricted
case “one valid tokenized BASIC front-end whose RUN line names one valid AMSDOS binary”.
It does not contain Chuckie Egg addresses or filenames:

1. Validate both AMSDOS headers and checksums.
2. Parse tokenized BASIC lines and require a `RUN` target matching the binary's AMSDOS
   filename.
3. Boot the original CPC464 ROM.
4. Load the binary using its own header load/execute addresses.
5. Execute real Z80 code to the title state.
6. Capture a CPC464-native 64 KiB SNA.
7. Reinitialize the emulator twice, reload the snapshot, run identical durations, and
   require identical framebuffer hashes.

The current chips quickloader loads an AMSDOS BASIC payload as bytes but does not update
Locomotive BASIC program pointers. Therefore the decorative BASIC front-end is validated
structurally rather than treated as executable proof. This is safe here because its final
operation is the verified single-binary `RUN"!CHUCK"`, and the binary's own AMSDOS entry
point is what the host executes. An unrelated Manic Miner binary is correctly rejected
with exit code 60 because its AMSDOS filename does not match the loader RUN target.

This is reusable for other simple BASIC+single-BIN folders. Multi-file loaders, loaders
that modify memory before loading, and programs requiring later media access still need
separate proof and are rejected by this path.

## Chuckie Egg snapshot and package

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Native CPC464 snapshot | 65,792 | `2ce4e8f2eb14eada2e02bfc708a54872e291bdb47d85dd952a49099b3fdae08e` |
| `chuckie_egg.cpcgame` | 65,888 | `8c950a5833897dc8ab893a9d4fbcedf799eea41c4340c27fe1048179fb036323` |

Importer result: ID `chuckie_egg`, title `Chuckie Egg`, CPC464, 64 KiB, SNA v1,
payload CRC32 `fbba06fe`, package CRC32 `c5663304`.

The captured snapshot reaches the animated Chuckie Egg title. S followed by 1 reaches
actual level-one gameplay. Reinitializing and reloading the SNA reaches the same state
with framebuffer hash `4c7590ee9c228383`. Title, player selection, and gameplay all run
without disk/tape emulation, so no further media access is observed across the verified
path. The source set contains only the BASIC front-end and the one binary. Later-level
media independence is strongly indicated but has not been exhaustively proved through a
complete playthrough.

Evidence:

- `artifacts/stage4g2-chuckie/frames/running.png` — title state.
- `artifacts/stage4g2-chuckie/title-probe/key-73.png` — S prompts for players.
- `artifacts/stage4g2-chuckie/gameplay-probe/neutral.png` — level-one gameplay.
- `artifacts/stage4g2-chuckie/redefined-probe/` — deterministic control trials.

## Chuckie Egg controls and physical blocker

The game needs Up, Down, Left, Right, and Jump. Host trials verify title selection and
gameplay, and the built-in K flow permits those five actions to be redefined. Z and X were
demonstrated as working horizontal movement keys after redefinition.

Five logical actions cannot be mapped one-to-one onto the Explorer's three buttons. A
chord design such as X+Y for Up and Z+Y for Down is conceivable, but it needs a delayed
chord resolver to avoid an unwanted Jump event and must be verified against ladders and
simultaneous movement. Implementing that without evidence would be a title-specific input
hack, so it was not added.

## Verification

- Complete Release host suite: **11/11 passed**.
- BASIC ROM boot and keyboard input: PASS.
- Knight Tyme regression: PASS.
- Manic Miner native snapshot and Q/W/Shift mapping: PASS.
- Launcher, library, upload/removal, UF2/E10, display-fit and performance-metric tests:
  PASS.
- Generic profile launch suppression and held/released transitions: PASS.
- Invalid BASIC/BIN pairing rejection: PASS.
- No firmware or UF2 build; physical device untouched.

## Smallest proven path forward

1. Treat Dizzy as the first Stage 4G physical candidate. Its package and complete
   three-button mapping are ready for a future, separately approved firmware candidate.
2. Keep the generic AMSDOS BASIC+single-BIN converter; it has produced a deterministic,
   media-independent Chuckie Egg package without title-specific addresses.
3. Hold Chuckie Egg installation until a generic, host-tested five-action-on-three-button
   scheme is designed and actual ladder/jump gameplay proves it usable.
4. Do not upload either package under the currently deployed Stage 4F.6: it does not
   contain the new Dizzy profile, and Chuckie Egg remains control-incomplete.
