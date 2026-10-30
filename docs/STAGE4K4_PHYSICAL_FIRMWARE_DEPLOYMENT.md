# Stage 4K.4 — Physical Firmware Deployment

Status: **PASS — firmware installed and verified; visual acceptance pending**

Physical firmware write attempts: **1**

## Completed preflight

| Check | Result |
|---|---|
| Candidate | PASS — 742,912 bytes |
| Candidate SHA-256 | PASS — `53e92564e46d3dd5a4f46faf17c65251799e66f28578c3b7a2e83654e58a2431` |
| UF2 application blocks | PASS — 1,450 blocks, family `0xE48BFF59`, targets `0x10000000–0x1005A9FF` |
| E10 compatibility | PASS — exactly one authorised block at `0x10FFFF00` |
| Unexpected/game-storage targets | PASS — none |
| Application USB | PASS — `2E8A:0009`, serial `<device-serial>`, COM11 |
| Current firmware | PASS — `0.4.23-stage4j3migrationfix` |
| Flash | PASS — `EF-40-18`, 16,777,216 bytes |
| Migration/upload | PASS — migration active, received 179,360; upload idle |
| Catalogue | PASS — exactly six expected entries; Knight Tyme occurs once |
| Recovery images | PASS — three complete read-only 16 MiB images, each SHA-256 `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1` |

Observed catalogue:

1. Locomotive BASIC
2. Columns
3. Treasure Island Dizzy
4. Knight Tyme
5. Manic Miner
6. Thrust

Fruity Frank is not exposed by the application catalogue.

## BOOTSEL and installation

The resumed session positively identified:

- healthy `H:` FAT volume labelled `RP2350`;
- `INFO_UF2.TXT`: UF2 Bootloader v1.0, model and board ID `RP2350`;
- USB `2E8A:000F`, serial `<device-serial>`;
- Picotool 2.3.1 target RP2350 A2/QFN80, chip ID
  `<device-chip-id>`, 16,384 KiB flash, BOOTSEL boot type;
- one matching target when selected by VID/PID, with the same chip ID.

Exactly one installation command was executed:

```powershell
& '%USERPROFILE%\CPCExplorerHostTools\picotool-2.3.1\picotool\picotool.exe' `
  load -v -x `
  '%USERPROFILE%\OneDrive\Documents\CPC Explorer\build-rp2350-stage4k31-ui-polish\cpc_explorer_incremental_library_exp.uf2' `
  -t uf2 --vid 0x2e8a --pid 0x000f --ser <device-serial>
```

Result: Flash 100%, verification 100%, `OK`; device rebooted normally. No
retry, erase, game upload, migration staging/activation, catalogue write, or
rollback was attempted.

The ordinary application write range represented by the UF2 is
`0x10000000–0x1005A9FF`, plus the single authorised E10 compatibility block at
`0x10FFFF00`.

## Post-installation acceptance

| Check | Result |
|---|---|
| Application USB | PASS — `2E8A:0009`, serial `<device-serial>`, COM11 |
| Firmware | PASS — `0.4.25-stage4k31uipolish` |
| Flash | PASS — `EF-40-18`, 16,777,216 bytes |
| Migration | PASS — active |
| Upload | PASS — idle |
| Catalogue | PASS — six expected entries |
| Knight Tyme | PASS — exactly one entry |
| Fruity Frank | PASS — hidden/not enumerated |
| USB stability | PASS — normal application enumeration and repeated CPCU1 commands succeeded |

Final visible catalogue:

1. Locomotive BASIC
2. Columns
3. Treasure Island Dizzy
4. Knight Tyme
5. Manic Miner
6. Thrust

The user must still visually confirm the exact `Pick a game` instruction, the
small bottom-right `Faulkie` signature, and normal launcher navigation/launch.
