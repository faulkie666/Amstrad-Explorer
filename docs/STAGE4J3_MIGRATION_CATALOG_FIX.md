# Stage 4J.3 — migration catalogue precondition correction

## Result

**PASS — host verification only. No device or COM access was performed.**

The corrected experimental firmware is:

- identity: `0.4.23-stage4j3migrationfix`
- UF2: `build-rp2350-stage4j3-migration-fix/cpc_explorer_migration_catalog_fix_exp.uf2`
- size: 740,352 bytes
- SHA-256: `fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53`

It has not been deployed. CPXPCK2 staging and activation were not attempted.

## Root cause

`storage::Installer::reserve_for_packed_library()` required the mounted legacy
CPCLIB1 catalogue to contain exactly three valid descriptors and then searched
those descriptors for `knight_tyme`, `manic_miner`, and `dizzy`.

That did not match the verified physical layout. Knight Tyme is the immutable
built-in snapshot entry in `launcher.cpp`; only Manic Miner and Dizzy are
CPCLIB1 flash descriptors. The valid two-descriptor catalogue therefore failed
with `packed_catalog_state` before an erase or program operation.

The three namespaces are now treated separately:

- built-in launcher identity: canonical snapshot ID `knight_tyme`;
- legacy CPCLIB1 identities: exactly one `manic_miner` and one `dizzy`;
- CPXPCK2 records: the six validated pack records, of which five are launchable.

No display-name comparison is used for the precondition and no Knight Tyme
flash descriptor is fabricated.

## Corrected validation

The production migration call obtains the Knight Tyme predicate from the
authoritative static launcher registry through
`launcher::has_builtin_snapshot_id("knight_tyme")`. `Transport::begin()` rejects
a false predicate before calling the installer or performing a flash operation.

The installer still mounts and validates CPCLIB1, but now requires exactly two
declared and valid descriptors. It requires the canonical IDs `manic_miner` and
`dizzy` exactly once each. The existing checks remain in force for catalogue
headers and generations, descriptor CRCs, snapshot CRCs and compatibility,
sector alignment, flash ranges, both catalogue copies, saved high-water,
device identity/capacity, exact migration manifest, and migration state.

Only after all those checks pass does the existing dual-catalogue reservation
write run. Pack erasure still occurs later in `Transport::begin()`. Atomic
header-B-then-header-A activation and interruption recovery are unchanged.

After activation the validated pack produces the exact visible set:

1. Locomotive BASIC
2. Knight Tyme
3. Manic Miner
4. Dizzy
5. Columns
6. Thrust

Fruity Frank remains non-launchable, and the launcher test proves that Knight
Tyme occurs exactly once.

## Files changed for Stage 4J.3

- `src/library_installer.cpp`
- `src/packed_migration.h`
- `src/packed_migration.cpp`
- `src/launcher.h`
- `src/launcher.cpp`
- `src/main.cpp`
- `tests/packed_migration_test.cpp`
- `tests/launcher_test.cpp`
- `CMakeLists.txt`
- `tools/cpc_game.py` (adds the exact new firmware hash/identity to the existing
  fail-closed migration allowlist)

`src/gameplay_input.cpp` and its focused test gained a build guard so the
pending Stage 4J.2C Knight Tyme remap remains available only when its dedicated
build option is enabled. The Stage 4J.3 build has
`CPCEXPLORER_STAGE4J2C_KNIGHT=OFF`, so it retains the installed Stage 4J.1
mapping and does not bundle that unaccepted correction.

## Host verification

The isolated Clang Release build at `build-host-stage4j3` passed **26/26** CTest
tests. Focused migration coverage includes:

- the physical two-descriptor catalogue plus authoritative built-in Knight;
- rejection of missing built-in Knight, missing Dizzy, unexpected identity,
  duplicate descriptor ID, and the obsolete/fabricated three-descriptor layout;
- proof that each precondition failure performs zero flash operations;
- corrupt manifest/device identity, catalogue write failures, transfer CRC and
  readback failures;
- interrupted reservation, programming, and both activation boundaries;
- reboot recovery and legacy rollback;
- exact packed visible identities, one Knight Tyme, and hidden Fruity Frank;
- the fragmented USB migration receiver regression.

The complete suite also passed BASIC, Knight Tyme, Manic Miner, Dizzy/audio,
Qw/ST, gameplay input, display, AY, upload/removal, flash backup validation,
UF2 policy, and fast-engine integration tests.

## Link and flash gates

- firmware binary: 369,828 bytes
- firmware reservation margin: 3,824,476 bytes
- `z80_execute_step`: `0x20001280`, 76,212 bytes
- extracted Z80 section SHA-256:
  `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`
- main-SRAM headroom: 119,008 bytes
- mandatory minimum: 118,784 bytes
- margin above gate: 224 bytes
- heap reservation: 2,048 bytes
- core-0 stack: 2,048 bytes
- core-1 stack: 2,048 bytes in the established scratch-SRAM layout

UF2 validation:

- 1,446 physical blocks;
- 1,445 application blocks, family `0xE48BFF59`;
- application targets `0x10000000–0x1005A4FF`, entirely inside the first 4 MiB;
- no catalogue or game-storage targets;
- exactly one authorised E10 block: family `0xE48BFF57`, flags `0x0000A000`,
  target `0x10FFFF00`, extension `0x9957E304`, 256 bytes of `0xEF`.

## Preserved references

The following were re-hashed after the build and remain unchanged:

- Stage 4F.6: `65f526d7d9da343887a8eb8bd580e849eb0fe26059e73178525110f4ae62b6f5`
- Stage 4H.4: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Stage 4H.5D: `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`
- Stage 4I.8: `28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114`
- Stage 4J.1: `2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6`
- pending Stage 4J.2C: `16e112ed4fb1174908225f7bbf8b8ee79c65a13ad79746b59eae9ede3857a6cf`
- CPXPCK2 pack: `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`

All three 16,777,216-byte Stage 4I.10 recovery images remain read-only and
retain SHA-256
`c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`.

## Gate

Host development: **PASS**. Physical installation: **NOT AUTHORISED / NOT
PERFORMED**. The existing legacy catalogue and game data were not accessed or
modified.
