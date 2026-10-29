# Stage 4G.2.1 Dizzy physical-test candidate

## Result

PASS for host build and verification. No COM port, BOOTSEL volume, RP2350 flash,
installed catalog, or game payload was accessed.

## Control-profile implementation

CPCLIB1 descriptor byte 59 now carries a validated input-profile value without
changing the 80-byte descriptor, package size, catalog version, or flash layout:

- 0: no profile
- 1: Q/W/bare Shift
- 2: Z/X/Space

The launcher chooses the profile from descriptor metadata, not the game ID. Unknown
values are rejected by the host importer, package parser, catalog reader, and device
installer. Existing zero-valued packages remain compatible. The physically installed
Manic Miner package and the already-approved Dizzy package predate this metadata. Their
validated payload CRC32 values (`9584cfc4` and `1e76d62f`) are the two compatibility
fingerprints for Q/W/Shift and Z/X/Space respectively. No game ID is used for input
selection.

The separate controls-bearing Dizzy package is:

- `artifacts/stage4g21-dizzy/dizzy_controls.cpcgame`
- SHA-256 `153c88f6deea8d82570a69077d8f5a7e7c1ed1c0edaca3c12d57f9f4b9da3f65`
- 65,888 bytes; payload CRC32 `1e76d62f`; package CRC32 `00f3a515`
- ID `dizzy`; title `Dizzy`; CPC464; 64 KiB; SNA v3; profile `zx-space`

The approved original package remains unchanged at SHA-256
`780423a9bdf589b838d564e0dc6c53726902d3ea89b094cbd444fc588cb77c5e`,
and is the package to use for the first physical test. The controls-bearing package
above proves the forward metadata path but is not required for this acceptance test.

Input remains level-based. X holds/releases CPC Z, Z holds/releases CPC X, and Y
holds/releases Space. An all-buttons-up sample is required after launch, preventing the
Y selection press from leaking into gameplay. Manic Miner remains Q/W/bare Shift.

## Candidate

- Identity: `0.4.11-stage4g21dizzy`
- UF2: `build-rp2350-stage4g21-dizzy/cpc_explorer_dizzy_controls.uf2`
- UF2 size: 685,568 bytes
- UF2 SHA-256: `37edb2731a5580d8d76366bdb7fb2ba1dd43aadf7659e292b3ab48b0c478cd9e`
- Binary size: 342,292 bytes (Stage 4F.6: 342,204; delta: +88)

The build keeps FAST engine, 12.5 Hz presentation, active-display fit, 150 MHz clock,
and only `z80_execute_step` in SRAM. The unlinked routine is byte-identical to Stage
4F.6: 76,212 bytes, SHA-256
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.

## SRAM and linker verification

| Item | Stage 4F.6 | Stage 4G.2.1 | Delta |
|---|---:|---:|---:|
| `.text` | 142,840 | 142,872 | +32 |
| `.rodata` | 110,832 | 110,888 | +56 |
| `.data` | 87,640 | 87,640 | 0 |
| `.bss` | 314,252 | 314,252 | 0 |
| minimum heap | 2,048 | 2,048 | 0 |
| core-0 stack | 2,048 | 2,048 | 0 |
| main-SRAM headroom | 120,052 | 120,052 | 0 |

`z80_execute_step` links at `0x200011a0`, size 76,212. The headroom is 117.24 KiB,
1,268 bytes above the explicit 116 KiB gate. No large static allocation was added.

## UF2 policy

- 1,339 physical blocks
- 1,338 ordinary application blocks, family `0xe48bff59`
- ordinary target range `0x10000000..0x100539ff` (end-exclusive `0x10053a00`)
- all ordinary targets are within the first 4 MiB reservation
- exactly one E10 block: family `0xe48bff57`, flags `0x0000a000`, target
  `0x10ffff00`, extension `0x9957e304`, 256 bytes of `0xef`
- no catalog or game-storage targets and no unexpected targets

Stage 4F.6 remains unchanged at SHA-256
`65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`.
The MicroPython backup was independently rehashed 21/21.

## Host verification

Release/Clang suite: 12/12 passed:

1. CPC464 ROM boot and BASIC keyboard behavior
2. Knight Tyme regression
3. launcher order, wrapping, debounce, and selection
4. game-library catalog, scrolling, snapshots, corruption, and flash boundaries
5. transactional upload/removal and recovery
6. deterministic importer and control metadata, including invalid-value rejection
7. UF2/E10 policy
8. native Manic Miner construction
9. generic input profiles, launch suppression, held/released and rapid transitions
10. performance telemetry aggregation
11. active-display fit
12. fast-engine BASIC/Knight Tyme/Manic Miner differential integration

## Guarded physical procedure (not performed)

1. Recheck both candidate and rollback SHA-256 values.
2. Verify BOOTSEL VID/PID `2e8a:000f`, RP2350 volume label, and `INFO_UF2.TXT`.
3. Copy the candidate exactly once; do not retry an ambiguous copy.
4. Verify application VID/PID `2e8a:0009`, identity
   `0.4.11-stage4g21dizzy`, flash `EF-40-18`, 16 MiB, upload idle, and the existing
   three entries unchanged.
5. Verify BASIC, Knight Tyme, and Manic Miner before any new upload.
6. Separately upload the approved `artifacts/stage4g2-controls/dizzy/dizzy.cpcgame`
   once using the transactional protocol; recheck its required SHA-256 first.
7. Confirm X=left, Z=right, Y=Space/start/jump, releases, and selection-button
   suppression. Stop after physical acceptance.

## Chuckie Egg six-button feasibility

Explorer has six active-low switches. The three currently unused buttons are C/GPIO14,
B/GPIO15, and A/GPIO16; X/Y/Z are GPIO17/18/19. Therefore five direct actions plus an
exit control are electrically feasible without chords:

- X: Left
- Z: Right
- A: Up
- B: Down
- Y: Jump
- C: guarded exit, preferably a controlled reboot to the launcher

The current firmware initializes only X/Y/Z and its emulator loop is intentionally
non-returning. A/B/C therefore require a future generic five-action profile, active-low
GPIO initialization, key-matrix mapping, release tests, and a separately verified exit
path. None of that unverified Chuckie Egg functionality is present in this candidate.
