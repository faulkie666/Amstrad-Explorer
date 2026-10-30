# Stage 4K.4M — Menu Navigation Performance Fix

Status: **PASS — host verified; physical deployment not performed**

## Root cause

Stage 4K.3 merged CPXPCK2 and CPCLIB1 correctly, but every merged enumeration
called `library::View::game()`. That function revalidated the complete
65,792-byte SNA payload CRC. Menu movement and redraw query visible counts and
labels several times, so each X/Z event repeatedly scanned both legacy
snapshots from XIP flash.

The GPIO path, active-low button handling, 30 ms debounce, selection logic,
5 ms menu-loop sleep, LCD drawing, and CPXPCK2 ordering are unchanged from the
previous firmware. The regression was the repeated CPCLIB1 payload validation,
not debounce or rendering.

## Correction

The existing mount-time catalogue cache is now used as intended:

- catalogue mount still validates every descriptor, full SNA CRC, snapshot
  header, allocation, and flash boundary;
- routine enumeration rechecks descriptor integrity and snapshot metadata but
  does not recalculate the already-validated 65,792-byte payload CRC;
- the selected CPCLIB1 payload receives a fresh full CRC validation immediately
  before launch;
- corrupt launch payloads fail closed to BASIC;
- catalogue publication, upload, or removal still remounts and fully validates
  the changed catalogue.

No new game cache, framebuffer, static allocation, or permanent SRAM object was
added.

## Measurement

The same release-build host probe performed 200 repeated X/Z navigation and
visible-label redraw iterations through the active six-game merged catalogue.

| Build | Total | Per iteration |
|---|---:|---:|
| Stage 4K.3.1 behaviour | 821,438 us | 4,107.19 us |
| Stage 4K.4M fix, five-run median | 1,386 us | 6.93 us |

Measured host-path improvement: approximately **593x**. This is not a claimed
RP2350 latency; physical responsiveness still requires acceptance testing.

## Verification

- Focused launcher/library/migration tests: 5/5 PASS.
- Complete host suite: 26/26 PASS.
- Repeated X/Z navigation: PASS.
- Enter/leave Arcade game list and A-Z view: PASS.
- Move between Arcade and Simulation: PASS.
- Existing six-game catalogue and hidden Fruity Frank: PASS.
- Incremental Ktris catalogue refresh and launch path: PASS.
- Runtime payload-corruption rejection before launch: PASS.
- Transaction/recovery and flash protection regressions: PASS.

Files changed for the correction and verification:

- `CMakeLists.txt`
- `src/game_library.h`
- `src/game_library.cpp`
- `src/launcher.cpp`
- `tests/game_library_test.cpp`
- `tests/packed_migration_test.cpp`

No Z80, AY audio, controller mapping, snapshot, packed-library content, or
flash-layout implementation was changed.

## Candidate

- Firmware identity: `0.4.26-stage4k4menufix`
- UF2: `build-rp2350-stage4k4-menu-fix/cpc_explorer_incremental_library_exp.uf2`
- UF2 size: 743,424 bytes
- SHA-256: `257a0775129c46e2152bdae93be75875c1b7c7ca6b2e62d178bf5b75c58fe404`
- Main-SRAM headroom: 119,004 bytes
- Required minimum: 118,784 bytes
- Remaining margin: 220 bytes

UF2 validation passed with 1,451 ordinary RP2350 application blocks in
`0x10000000–0x1005AAFF` and exactly one authorised E10 compatibility block at
`0x10FFFF00`. No catalogue or game-storage target is present.

Ready for separately approved physical deployment: **YES**.
