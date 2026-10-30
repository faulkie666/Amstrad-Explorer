# Stage 4K.6 — Bulk CPC Game Library Expansion

Date: 2026-10-30

## Result

Ktris transactional installation: **PASS**. Physical gameplay acceptance: **PENDING USER CONFIRMATION**.

Exactly one upload command was issued. No retry, firmware write, migration operation, catalogue reset, or further game upload occurred.

## Physical preflight

- USB identity: `2E8A:0009`, serial `<device-serial>`, `COM11`
- Firmware: `0.4.26-stage4k4menufix`
- Flash: `EF-40-18`, 16,777,216 bytes
- Migration: `active`
- Upload: `idle`
- Existing visible library: Locomotive BASIC, Columns, Treasure Island Dizzy, Knight Tyme, Manic Miner, Thrust
- Ktris package: 65,888 bytes
- Ktris SHA-256: `81cc556496609189af0a2ee9058a903cf5dfee8f91c7a73ea61ceeff6b1377db`
- Ktris payload CRC32: `5d1450dc`
- Allocated range: `0x467000–0x477FFF`

## Physical operation

Command executed once:

```powershell
py tools/cpc_game.py upload artifacts/stage4k2-rapid-library/packages/ktris.cpcgame --port COM11 --confirm UPLOAD
```

Device completion response:

```text
CPCU1 COMMITTED crc=5d1450dc entries=7
upload committed
```

Post-install read-only checks found firmware, USB, flash identity, and migration state unchanged. Upload returned to `idle`; the original six IDs remained present and `ktris` appeared once as entry 7. Software installation is therefore PASS. Launch and gameplay remain for the user to confirm.

## Capacity

After Ktris:

- Allocation high-water: `0x478000`
- Free writable game storage: 12,025,856 bytes
- Flash allocation capacity: 172 additional 68 KiB snapshot slots
- CPCLIB1 descriptor capacity: 189 additional descriptors
- Effective capacity: 172 additional games (flash is the limiting resource)

This estimate includes sector alignment, the active CPXPCK2 pack, current CPCLIB1 allocations, catalogue reservations, firmware protection, and the permanent E10 guard.

## Validated next batch

Existing Stage 4K.2 gameplay evidence was reused; passed conversions were not repeated.

| Game | Package | SHA-256 | Controls | Status |
|---|---:|---|---|---|
| Block Invasion | 65,888 bytes | `4b35e235235edf835c2b99d8188027e9b69d0e7cb8e23f1b84d6f6890270b137` | Z/X/Space | Ready, not uploaded |
| Kiloroid | 65,888 bytes | `7c0e813f8c61414a3546f1421b1ec60a805362f4ac36a204959d4cb5f6ceec8d` | Cursor/QAOP profile | Ready, not uploaded |

Both packages have deterministic hashes, CPC464 64 KiB SNA v3 payloads, reload evidence, gameplay progression, graphics/control checks, and recorded AY observations. No new conversion reached the same evidence threshold during this focused stage.

HOLD candidates remain Frogger (CPC Plus-only source), Chuckie Egg (title/start/redefinition controls incomplete), Harrier Attack (gameplay/control acceptance incomplete), Fruity Frank (restart-state failure), Classic Invaders (gameplay start unproven), and Bomb Jack (title evidence only).

## Large-catalogue launcher benchmark

The deployed 0.4.26 firmware already avoids payload CRC scans during normal navigation, but host scaling exposed a second cost: overlay duplicate detection rescanned all preceding legacy records on every count and label lookup. This becomes quadratic.

Identical 50-iteration host measurements:

| Visible games | Deployed algorithm | Mount-time deduplication |
|---:|---:|---:|
| 25 | 32.147 ms | 3.541 ms |
| 50 | 189.005 ms | 10.036 ms |
| 100 | 1,019.853 ms | 27.409 ms |
| 179 (physical maximum) | 4,124.097 ms | 59.754 ms |

These are desktop timings, not RP2350 latency claims. The correction validates IDs once at catalogue mount and stores only first occurrences in the existing index array. It adds no permanent SRAM allocation and keeps full descriptor/payload validation at mount and immediately before launch.

## Host-only scale candidate

- Identity: `0.4.27-stage4k6scalefix`
- UF2: `build-rp2350-stage4k6-scale-fix/cpc_explorer_incremental_library_exp.uf2`
- Size: 743,424 bytes
- SHA-256: `6b2790a70f9f153e6bdbe0f8dd9957152e09284a878dc64729ea82ff9dcd5291`
- SRAM headroom: 119,004 bytes (minimum 118,784)
- Host suite: 26/26 PASS
- UF2: 1,451 ordinary application blocks, range `0x10000000–0x1005AAFF`, family `0xE48BFF59`; one exact authorised E10 block at `0x10FFFF00`; no game/catalogue targets

This candidate was built only to protect navigation performance as the catalogue grows. It was not deployed and requires separate approval.

## Resumable plan

The machine-readable progress manifest is:

`artifacts/stage4k6-bulk-library/installation-manifest.json`

Next action: physically launch Ktris and confirm left/right/down and Space rotation. If accepted, the next safe upload batch is Block Invasion and Kiloroid. A separately approved deployment of the scale-fix firmware is recommended before expanding toward dozens of entries.
