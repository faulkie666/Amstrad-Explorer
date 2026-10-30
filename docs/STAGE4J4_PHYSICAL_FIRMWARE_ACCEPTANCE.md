# Stage 4J.4 — physical firmware acceptance

> **Stage 4J.4R resolution:** the read-only recovery session completed both
> outstanding integrity checks with a validated 64-byte request size. See
> `STAGE4J4R_READBACK_RECOVERY.md`. The original HOLD evidence below is retained
> unchanged as the record of the first session.

## Result

**HOLD — firmware installation succeeded and verified, but the mandatory fresh
legacy-region readback was incomplete.**

No retry, rollback, CPXPCK2 staging, catalogue activation, game upload, or
game-library write was attempted after the failure.

## Preflight

| Check | Result |
|---|---|
| BOOTSEL volume | PASS — `H:`, label `RP2350`, FAT, healthy removable volume |
| `INFO_UF2.TXT` | PASS — `UF2 Bootloader v1.0`, model and board ID `RP2350` |
| USB bootloader | PASS — `2E8A:000F`, serial `<device-serial>` |
| Picotool | PASS — official USB-enabled picotool 2.3.1 |
| Device | PASS — RP2350 revision A2, QFN80, chip ID `<device-chip-id>` |
| Flash capacity | PASS — 16,384 KiB / 16,777,216 bytes |
| Matching targets | PASS — one BOOTSEL target reported |
| Previous firmware | PASS — `0.4.21-stage4j1qwstexp` |
| Candidate size | PASS — 740,352 bytes |
| Candidate SHA-256 | PASS — `fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53` |
| Recovery images | PASS — all three are 16,777,216 bytes, read-only, SHA-256 `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1` |

UF2 inspection passed with 1,446 physical blocks: 1,445 RP2350 ARM-S
application blocks spanning `0x10000000–0x1005A4FF`, plus exactly one authorised
E10 block at `0x10FFFF00`. There are no catalogue, legacy-game, or CPXPCK2
destination targets.

## Installation

Exactly one command was executed:

```powershell
& '%USERPROFILE%\CPCExplorerHostTools\picotool-2.3.1\picotool\picotool.exe' `
  load -v -x `
  '%USERPROFILE%\OneDrive\Documents\CPC Explorer\build-rp2350-stage4j3-migration-fix\cpc_explorer_migration_catalog_fix_exp.uf2' `
  -t uf2 --vid 0x2e8a --pid 0x000f --ser <device-serial>
```

- installation attempts: **1**
- write: **100%**
- verification: **100%, OK**
- result: device rebooted into application mode
- automatic retry: **none**

The UF2-authorised flash targets were `0x10000000–0x1005A4FF` and the exact
SDK-generated E10 compatibility target `0x10FFFF00`. No CPCLIB1 catalogue,
legacy allocation, or CPXPCK2 destination range was part of the UF2.

## Post-install checks

| Check | Result |
|---|---|
| Application USB | PASS — `2E8A:0009`, serial `<device-serial>` |
| Port | PASS — `COM11` |
| Firmware | PASS — `0.4.23-stage4j3migrationfix` |
| Flash | PASS — `EF-40-18`, 16,777,216 bytes |
| Upload | PASS — `idle`, received `0` |
| Migration | PASS — `legacy`, received `0`, error `none` |
| Visible entries | PASS — Locomotive BASIC, Knight Tyme, Manic Miner, Dizzy |
| Initial USB stability | PASS through identity/status/list/migration-status queries |

Observed read-only output:

```text
CPCU1 READY fw=0.4.23-stage4j3migrationfix flash=EF-40-18 capacity=16777216 entries=4
CPCU1 STATUS upload=idle flash=EF-40-18 capacity=16777216 entries=4 received=0 migration=legacy migration_received=0
CPCU1 MIGRATION STATUS state=legacy received=0 error=none
```

## Mandatory readback failure

The single fresh `backup-library` command attempted to read
`0x400000–0x43AFFF` (241,664 bytes). It stopped with:

```text
error: migration readback length mismatch at chunk=369 offset=0x40b880:
expected=128 actual=77 line_bytes=183
```

The tool writes its output file only after receiving the complete range, so no
partial legacy-region artifact was accepted. The required SHA-256
`88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`
could not be established in this session.

In accordance with the explicit stop condition, the read was not repeated.
The CPXPCK2 destination `0x43B000–0x466FFF` was not subsequently queried, so
its expected erased-region SHA-256
`6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`
is **NOT VERIFIED IN THIS SESSION**.

## Final gate

- firmware installation and picotool verification: **PASS**
- post-install firmware/device/catalogue identity: **PASS**
- complete legacy-region comparison: **HOLD — short response at chunk 369**
- destination erased-region verification: **NOT PERFORMED after stop condition**
- CPXPCK2 staging/activation: **NOT PERFORMED**
- overall Stage 4J.4 acceptance: **HOLD**
