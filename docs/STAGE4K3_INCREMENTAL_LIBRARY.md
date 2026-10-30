# Stage 4K.3 — Incremental Library Launcher Fix

Status: **PASS — host verified; physical deployment not performed**

## Root cause

When CPXPCK2 was active, the launcher enumerated only the built-in BASIC entry
and the packed CPXPCK2 records. The transactional CPCLIB1 uploader remained
operational, but committed CPCLIB1 descriptors were not merged into the active
launcher tree. A newly uploaded game could therefore occupy valid flash and
catalogue space without becoming visible.

## Correction

- The active launcher now presents a read-only merged view: BASIC, visible
  CPXPCK2 records, then valid non-conflicting CPCLIB1 records.
- CPCLIB1 additions are exposed under `Arcade > A-Z`; CPCLIB1 has no persisted
  category field, so no category is guessed beyond Arcade.
- Deduplication is deterministic. Built-in IDs and every packed ID, including
  hidden records, take precedence over CPCLIB1. Earlier valid CPCLIB1 records
  take precedence over later duplicate IDs.
- The uploader receives the same protected-ID predicate and rejects a package
  whose ID conflicts with a built-in or packed record before any erase or
  program operation.
- CPCLIB1 control-profile validation now accepts the complete existing
  metadata range `0..8`, matching the packager and input layer. Unknown values
  still fail closed.
- No game records are copied into a new SRAM catalogue. The merged view is
  calculated on demand to stay inside the existing memory gate.

The CPCLIB1 allocator continues to use the post-migration saved high-water
mark. The Ktris simulation verifies its allocation begins at or above
`0x467000`, while existing firmware, packed-game, catalogue, and E10 ranges
remain protected.

## Files changed

- `CMakeLists.txt`
- `src/game_library.h`
- `src/game_library.cpp`
- `src/library_installer.h`
- `src/library_installer.cpp`
- `src/launcher.h`
- `src/launcher.cpp`
- `src/main.cpp`
- `tests/packed_migration_test.cpp`

Unrelated pre-existing working-tree changes were preserved.

## Verification

Focused catalogue/launcher tests: **5/5 PASS**

Complete host regression suite: **26/26 PASS**

The focused Stage 4K.3 coverage proves:

- the existing visible set remains BASIC, Columns, Dizzy, Knight Tyme,
  Manic Miner, and Thrust;
- Knight Tyme appears once;
- hidden Fruity Frank remains hidden and its ID remains protected;
- a transactional Ktris install is accepted with control profile 6;
- Ktris appears under `Arcade > K` and launches through the CPCLIB1 snapshot
  path;
- the existing games remain launchable;
- built-in, packed, hidden, duplicate, and invalid-profile conflicts fail
  before flash erase/program;
- migration, uploader, recovery, audio, controller, display, BASIC, Knight
  Tyme, and Manic Miner regressions remain green.

## Memory and firmware

- Firmware identity: `0.4.24-stage4k3incremental`
- Binary size: 370,964 bytes
- UF2 size: 742,912 bytes
- Main-SRAM headroom: 119,004 bytes
- Required minimum: 118,784 bytes
- Remaining margin: 220 bytes
- SRAM Z80 step placement: `0x20001280`, 76,212 bytes
- Heap reservation: 2,048 bytes
- Core 0 stack reservation: 2,048 bytes
- Core 1 stack reservation: 2,048 bytes

Candidate:

`build-rp2350-stage4k3-incremental/cpc_explorer_incremental_library_exp.uf2`

SHA-256:

`e9687b286e68f15847f7b648d01b2714f3207673ac7adc8eed044abaa1706c31`

UF2 policy result:

- 1,451 physical blocks
- 1,450 ordinary application blocks
- application family `0xE48BFF59`
- ordinary target range `0x10000000..0x1005A9FF`
- exactly one authorized RP2350-E10 compatibility block at `0x10FFFF00`
- no application target in catalogue or game storage
- no unexpected target

## Preservation

The Stage 4I.7 six-record pack remains 179,360 bytes with SHA-256
`27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`.
The Stage 4J.3 rollback UF2 remains unchanged with SHA-256
`fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53`.

No COM port, BOOTSEL device, physical flash, catalogue, or installed game was
accessed or modified.

## Recommendation

The candidate is ready for a separately approved, controlled firmware-only
physical deployment. After deployment, the acceptance test should first prove
the existing six-game menu is unchanged, then upload one validated Ktris
package, verify its readback/catalogue publication, and confirm it appears at
`Arcade > K` and launches. No batch upload should precede that single-game
acceptance.
