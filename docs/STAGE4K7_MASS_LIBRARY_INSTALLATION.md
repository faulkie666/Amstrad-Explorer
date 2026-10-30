# Stage 4K.7 — Mass CPC Game Library Installation

Date: 2026-10-30

## Status

**HOLD before bulk uploads.** The authorised scale-fix firmware installation passed,
but its first reboot exposed a migration-recovery state bug after an incremental game
has advanced the CPCLIB1 allocation high-water mark.

No game upload was attempted in Stage 4K.7. The seven installed entries remain visible.

## Firmware deployment

- Target: RP2350 A2/QFN80, serial `<device-serial>`
- BOOTSEL identity: `2E8A:000F`, volume `H:` labelled `RP2350`
- Previous firmware confirmed by picotool: `0.4.26-stage4k4menufix`
- Installed firmware: `0.4.27-stage4k6scalefix`
- Approved UF2 SHA-256: `6b2790a70f9f153e6bdbe0f8dd9957152e09284a878dc64729ea82ff9dcd5291`
- Installation attempts: 1
- Picotool load: 100%; verification: 100%, `OK`
- UF2 targets: 1,451 application blocks in `0x10000000–0x1005AAFF` and one exact authorised E10 block at `0x10FFFF00`
- Application USB: `2E8A:0009`, COM11
- Flash: `EF-40-18`, 16,777,216 bytes
- Upload: idle

Visible entries after reboot:

1. Locomotive BASIC
2. Columns
3. Treasure Island Dizzy
4. Knight Tyme
5. Manic Miner
6. Thrust
7. Ktris

## Confirmed blocker

The post-install status reported `migration=legacy` instead of `active`. Packed games
still mounted and all seven entries remained visible.

The exact cause is `Installer::packed_region_reserved()`. Before incremental uploads,
both CPCLIB1 catalogue copies record the packed reservation high-water `0x67000`.
Ktris is correctly allocated at and beyond that boundary; publishing its descriptor
advances the new catalogue copy to `0x78000`, while the fallback catalogue retains
`0x67000`. Recovery required both values to equal `0x67000`, so a legitimate
post-upload state was mislabeled as legacy.

The fail-closed correction accepts a valid catalogue copy when its independently
validated high-water is **at least** the packed reservation boundary. Values below the
boundary remain rejected. Pack mounting, CRCs, descriptor validation, protected IDs,
and game-data range checks are unchanged.

A new regression boots an activated packed library, commits Ktris transactionally,
reconstructs the transport as on a physical reboot, and requires state `active`.

## Corrected host-only candidate

- Identity: `0.4.28-stage4k7migrationrecoverfix`
- UF2: `build-rp2350-stage4k7-migration-recovery-fix/cpc_explorer_incremental_library_exp.uf2`
- SHA-256: `fe84bcd5c439a88ba030f28f48f842987876fe8ae88d768de6a97a2561b99dbd`
- Size: 743,424 bytes
- Host tests: 26/26 PASS
- SRAM headroom: 119,004 bytes; required minimum 118,784 bytes
- Firmware bytes: 371,404; first-4-MiB margin 3,822,900 bytes
- UF2: 1,451 ordinary application blocks in `0x10000000–0x1005AAFF`, plus exactly one authorised E10 block; no library/catalogue targets

This candidate was built and verified only. It was not deployed because the one
authorised Stage 4K.7 firmware attempt had already been consumed.

## Conversion inventory

The Arcade and Simulation trees contain 10,216 files in 1,808 directories. Relevant
source counts are 101 `DSK.GZ`, 463 BAS, 2,407 BIN, and zero SNA/SNA.GZ. All source
trees remained read-only.

Existing install-ready packages not already installed:

| Game | SHA-256 | Status |
|---|---|---|
| Block Invasion | `4b35e235235edf835c2b99d8188027e9b69d0e7cb8e23f1b84d6f6890270b137` | Ready; not uploaded |
| Kiloroid | `7c0e813f8c61414a3546f1421b1ec60a805362f4ac36a204959d4cb5f6ceec8d` | Ready; not uploaded |

No additional title was promoted without gameplay, snapshot reload, disk-ejection,
controls, and integrity evidence. The existing conversion pipeline remains the route
for the 101 compressed DSK candidates; failed titles will be quarantined rather than
debugged serially.

## Gates

- Firmware 0.4.27 installation: **PASS**
- Seven-game catalogue preservation: **PASS**
- Launcher visual/physical confirmation: **PENDING USER**
- Required migration state: **HOLD** (`legacy`, expected `active`)
- Bulk physical uploads: **NOT STARTED**
- Total installed games: **7**
- Ready next packages: **2**
- Approximate remaining capacity: **12,025,856 bytes / 172 aligned slots**

## 0.4.28 physical correction acceptance

The user separately authorised one correction deployment and placed the exact device
in BOOTSEL. Preflight reconfirmed H: as `RP2350`, USB `2E8A:000F`, serial
`<device-serial>`, the installed 0.4.27 image, the candidate SHA-256, and the complete
UF2/E10 policy.

Exactly one picotool `load -v -x` was executed. Flash writing and verification both
reached 100% and returned `OK`; no retry occurred.

Post-reboot read-only results:

- application USB `2E8A:0009`, serial `<device-serial>`, COM11;
- firmware `0.4.28-stage4k7migrationrecoverfix`;
- flash `EF-40-18`, 16,777,216 bytes;
- migration `active`, received 0, error `none`;
- upload `idle`;
- seven expected entries, with Ktris present once and every original game preserved.

Firmware correction deployment: **PASS**. The remaining gate before the bulk campaign
is the requested brief user confirmation that the physical launcher displays and
navigates normally.

## Resumed bulk campaign

User physical acceptance of the launcher, existing games and Ktris passed. A fresh
preflight confirmed firmware 0.4.28, exact serial, EF-40-18 capacity, migration active,
upload idle, and seven expected entries.

Successful single-attempt additions:

| Game | Package SHA-256 | Result |
|---|---|---|
| Block Invasion | `4b35e235235edf835c2b99d8188027e9b69d0e7cb8e23f1b84d6f6890270b137` | committed, visible as entry 8 |
| Kiloroid | `7c0e813f8c61414a3546f1421b1ec60a805362f4ac36a204959d4cb5f6ceec8d` | committed, visible as entry 9 |
| Bonz! | `cece53c95597bc1ea375b5752fb58b9024029a7e00920c677390c4be34c6768e` | converted, deterministically validated, committed, visible as entry 10 |

Bonz! was booted from its original compressed DSK on CPC464/64K, reached its actual
isometric game field, continued after disk ejection, reloaded in a fresh emulator, and
responded deterministically to joystick directions and fire. Its package was rebuilt
twice with byte-identical output.

The next converted title, 4kroid, also passed the gameplay, diskless reload, deterministic
joystick-input and package-integrity gates. Its one upload transaction committed with
payload CRC `877504a8`, but the visible entry count remained 10. Read-only inspection
proved that the launcher filters legacy overlay titles unless their first character is
`A` through `Z`; `4kroid` therefore consumed an allocation and descriptor but is hidden.

This is an unexpected post-commit catalogue-visibility failure. In accordance with the
campaign stop rule, all physical writes stopped immediately. No retry, removal, repair,
rollback or subsequent upload was attempted.

Final observed device state:

- firmware `0.4.28-stage4k7migrationrecoverfix`;
- USB `2E8A:0009`, serial `<device-serial>`, COM11 stable;
- migration active, error none;
- upload idle;
- 10 visible entries, all expected and unique;
- committed high-water `0x4BC000`;
- 11,747,328 bytes / 168 aligned slots remain;
- CPCLIB1 uses 7 of 192 descriptors.

Campaign result: **HOLD**. The smallest format-preserving recovery is a separately
authorised transactional removal of hidden ID `4kroid`, followed by reinstalling the
same validated snapshot with an alphabetic title such as `Four Kroid`. Removed payload
space would remain allocated as designed. The alternative is a future firmware change
adding a numeric-title menu bucket. Neither action has been taken automatically.

## Visibility-safe recovery and resumed campaign

The user chose to preserve the valid committed `4kroid` descriptor and allocation.
The host uploader now validates the package title before port discovery and rejects
titles beginning with a digit or punctuation. Focused tests cover `4kroid`, `9 Lives`,
`! Invaders`, and an alphabetic control. Invalid titles fail before opening a serial
port, issuing `BEGIN`, erasing, or programming.

A read-only two-slot catalogue audit established the physical state rather than
inferring it from the launcher list. Before resuming, the active generation contained
seven CPCLIB1 descriptors including `4kroid`; the launcher exposed ten entries, proving
that `4kroid` was the sole hidden record. There were no duplicate IDs, invalid ranges,
or unexpected catalogue records.

Three additional games then passed authentic-load, active-gameplay, disk-ejection,
fresh snapshot reload, deterministic controls, package integrity, and alphabetic-title
gates. Each was uploaded exactly once. Every commit increased the visible count by one,
returned upload state to idle, preserved all earlier entries, and was followed by a
fresh catalogue audit.

| Game | Package SHA-256 | Controls | Physical transaction |
|---|---|---|---|
| Froggy | `1ff76a5acaec78a8c7664002a904e79a68b80ad5494db3afe934a5a6881d1a62` | QAOP | committed once; visible |
| Last of the Smileys | `734bb4b4f8583dcb5a1902342f258b33bf90e796d01ad5b0d7dde20ff767b179` | joystick + fire | committed once; visible |
| Stack n Smile | `7e5790f87d49dd5894bcdbc6e0f89ef2872f5b02064f6f7920ff91def213644a` | QAOP | committed once; visible |

Frogger was quarantined: its DSK catalogue is readable, but the authentic run path
returned to BASIC and did not establish gameplay. It was not packaged or uploaded.

Final read-only state:

- firmware `0.4.28-stage4k7migrationrecoverfix`;
- migration active, error none, upload idle;
- 13 visible games;
- 14 committed game records when the preserved hidden `4kroid` is included;
- active CPCLIB1 generation 15 with 10 valid descriptors;
- allocation high-water `0x4EF000`;
- 11,538,432 bytes and 165 aligned snapshot slots remain;
- final catalogue evidence SHA-256 `33b4ab4861d0306a1fba31ad0b2acf7d49a320d549e0cd7b436ff2c45d48e1e8`.

Recovery/resume result: **PASS**. No firmware, migration, removal, erase, retry, repair,
or rollback operation was performed.
