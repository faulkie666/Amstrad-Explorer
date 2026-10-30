# Stage 4K.5 — Physical Menu Performance Fix

Status: **PASS — firmware installed and verified; navigation responsiveness pending user confirmation**

## Installation

- Firmware: `0.4.26-stage4k4menufix`
- Candidate SHA-256: `257a0775129c46e2152bdae93be75875c1b7c7ca6b2e62d178bf5b75c58fe404`
- Installation attempts: **1**
- Picotool: official USB-enabled 2.3.1
- Write result: 100%
- Verification result: 100%, `OK`
- Automatic retries: 0

The uniquely selected target was RP2350 A2/QFN80, chip ID
`<device-chip-id>`, USB serial `<device-serial>`, with 16,384 KiB flash.
BOOTSEL was `2E8A:000F`; the application returned as `2E8A:0009` on COM11.

The UF2 ordinary application targets were `0x10000000–0x1005AAFF`, plus the
single authorised E10 compatibility block at `0x10FFFF00`. No game upload,
catalogue modification, migration operation, erase, rollback, or additional
firmware attempt was performed.

## Post-installation state

- Firmware identity: PASS — `0.4.26-stage4k4menufix`
- Flash: PASS — `EF-40-18`, 16,777,216 bytes
- Migration: PASS — active
- Upload: PASS — idle
- Catalogue count: PASS — 6
- Knight Tyme: PASS — exactly once
- Fruity Frank: PASS — hidden/not enumerated
- USB stability: PASS — enumeration plus repeated status/list commands

Visible catalogue:

1. Locomotive BASIC
2. Columns
3. Treasure Island Dizzy
4. Knight Tyme
5. Manic Miner
6. Thrust

The remaining acceptance gate is the user's physical X/Z navigation test,
including Arcade/Simulation, A-Z, game-list entry/back navigation, and normal
launch selection. `Pick a game` and the bottom-right `Faulkie` signature should
remain visible.
