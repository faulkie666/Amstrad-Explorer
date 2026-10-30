# Stage 4K.1 — first 25-game CPC library batch

Date: 2026-10-30  
Mode: host-only discovery, reuse of established conversion evidence, and CPXPCK2 proof packing

## Result

**HOLD for a 25-game installable batch. PASS for the 25-title candidate selection and
three independently validated, diskless game assets.**

Twenty-five diverse titles have been selected and given stable IDs and source-set
hashes. Columns, Thrust and Chuckie Egg meet the requested host gameplay gate: each
reaches gameplay, renders a changing framebuffer, restores from a CPC464 64 KiB SNA,
and continues without its source media. The other 22 remain explicitly quarantined;
none is promoted merely because a title screen or loader was reached.

The verified Stage 4I.7 image remains byte-identical at 179,360 bytes and SHA-256
`27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`.
No COM port, BOOTSEL volume, RP2350, catalogue, installed game, or physical flash was
accessed. No firmware was configured or built. The two `D:` source trees were read
only.

## Source inventory and supported formats

| Collection | Game directories | Files | SNA | raw DSK | CDT | ZIP | 7z | GZ | BAS | BIN | extensionless |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Arcade | 1,593 | 9,593 | 0 | 0 | 0 | 0 | 0 | 95 | 402 | 2,169 | 1,320 |
| Simulation | 101 | 623 | 0 | 0 | 0 | 0 | 0 | 6 | 61 | 238 | 56 |
| **Total** | **1,694** | **10,216** | **0** | **0** | **0** | **0** | **0** | **101** | **463** | **2,407** | **1,376** |

All 101 GZip payloads were inspected without writing into the source collections; all
decompress as DSK images and none is an SNA. CPC Explorer does not execute DSK, CDT,
BAS or arbitrary BIN files. Its installable unit is a validated 65,792-byte CPC464,
64 KiB SNA wrapped in `.cpcgame`, or an SNA-derived CPXPCK2 record. DSK and extracted
file sets therefore require a deterministic CPC464 boot, actual-gameplay proof,
snapshot capture, fresh-emulator reload, media ejection, framebuffer evidence and
control/audio analysis.

The established konCePCja v6.4.2 localhost IPC harness remains the conversion route.
It uses a CPC464/64K model, AMSDOS ROM, emulated-frame input, bounded waits, screenshots,
RAM/VRAM hashes, YM capture, snapshot save/load and disk ejection. It does not use
manual Windows GUI automation.

## Candidate batch

Full source-set hashes and machine-readable evidence are in
`artifacts/stage4k1-25-game-batch/batch-manifest.json`.

| # | Title | Source | Conversion / graphics result | Controls | AY classification | LZ4 bytes | Gate |
|---:|---|---|---|---|---|---:|---|
| 1 | Columns | BAS + five BIN | Gameplay and diskless reload; changing active-game framebuffer | joystick directions + fire | sustained music, 3,000/3,000 changing YM frames | 21,263 | **PASS** |
| 2 | Thrust | loader + BIN | Gameplay and diskless reload; changing gameplay framebuffer | Z/X/Enter/Shift/Space | authentic activity/effects; 131 register changes | 17,691 | **PASS** |
| 3 | Chuckie Egg | BASIC + BIN | Title, player selection and level-one gameplay; deterministic diskless reload | up/down/left/right/jump; S + player count | isolated effect, not sustained music | 12,193 | **PASS** |
| 4 | Fruity Frank | loader + BIN | Valid diskless SNA, but saved state falls back to game-over/speed selection | joystick + fire + F | sustained sequences/dense effects not yet separated | 22,373 | HOLD restart state |
| 5 | Classic Invaders | loader + BIN + SCR | Authentic prompts/second load, but no sustained gameplay state | selection + left/right/fire unproved | loader effects only | — | HOLD gameplay start |
| 6 | Bomb Jack | loader + BIN | Mode selector/title only; gameplay not proved | unproved | gameplay not measured | — | HOLD title |
| 7 | Arkanoid | multi-file extracted set | Not converted; dependencies must be reproduced | expected paddle/fire, exact release unproved | not measured | — | HOLD |
| 8 | Bruce Lee | loader + N01 + N02 | Not converted; multi-file loading required | unproved | not measured | — | HOLD |
| 9 | Frogger | DSK.GZ | Valid DSK container only; no gameplay capture | directions, exact release unproved | not measured | — | HOLD |
| 10 | Harrier Attack | loader + BIN | Not converted | unproved | not measured | — | HOLD |
| 11 | Motos | loader + BIN + image | Existing single-BIN path rejects the extra dependency | unproved | not measured | — | HOLD multi-file |
| 12 | Boulder Dash | extracted program | Not converted; file semantics must be established | unproved | not measured | — | HOLD |
| 13 | Star Sabre | DSK.GZ | CPC464 DSK candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 14 | Ktris | DSK.GZ | Compact DSK candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 15 | Magical Drop CPC | DSK.GZ | Puzzle-game candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 16 | Nanako Descends To Hell | DSK.GZ | Platform candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 17 | Stack 'n' Smile | DSK.GZ | Puzzle candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 18 | Betiled! | DSK.GZ | Puzzle candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 19 | Operation Nemo | DSK.GZ | Shooter candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 20 | Block Invasion | DSK.GZ | Compact DSK candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 21 | Kiloroid | DSK.GZ | Compact shooter candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 22 | Who Dares Win 2 | DSK.GZ | Shooter candidate; no gameplay capture | unproved | not measured | — | HOLD |
| 23 | Fighter Pilot | BAS + BIN | Options screen reached; take-off/gameplay not proved | flight controls unproved | gameplay not measured | — | HOLD options |
| 24 | 3D Sub | loader + BIN | Simulation candidate; not converted | unproved | not measured | — | HOLD |
| 25 | Ace | multi-file BAS/BIN | Multi-file simulation; current restricted converter is insufficient | flight controls unproved | not measured | — | HOLD multi-file |

The selection spans platform/action (Chuckie Egg, Bruce Lee, Nanako), shooters
(Thrust, Star Sabre, Operation Nemo, Kiloroid, Who Dares Win 2), puzzles
(Columns, Ktris, Magical Drop, Stack 'n' Smile, Betiled, Block Invasion), classic
arcade games, and three simulations. Columns is the strongest sustained-AY test;
Thrust provides a different, event-heavy PSG workload. No soundtrack is inferred for
an unmeasured title.

## Validated host assets and CPXPCK2 proof

The validated assets are existing immutable snapshots; the new manifest references
them rather than copying or rewriting them:

| Game | Snapshot | SHA-256 | Raw / compressed |
|---|---|---|---:|
| Chuckie Egg | `artifacts/stage4g2-chuckie/chuckie-egg.sna` | `2ce4e8f2eb14eada2e02bfc708a54872e291bdb47d85dd952a49099b3fdae08e` | 65,792 / 12,193 |
| Columns | `artifacts/stage4i4-internal-library/columns/snapshot-probe2/probe-final.sna` | `f1b7872aee6a1e86d5ded43e08a116f932983259223be78685d14c813d7ebf99` | 65,792 / 21,263 |
| Thrust | `artifacts/stage4i6-gameplay/thrust2/probe-final.sna` | `2afa403436472be94c5e0a2c58eda35c60d0fb6b14c0230655022ce19b9d0808` | 65,792 / 17,691 |

`validated-host-proof.cpxpack` is a separate host-only CPXPCK2 v2 proof image:

- size: 67,552 bytes;
- SHA-256: `de16376a5b94319b696baa5ad715805e802887973f8bada40f016ac5c81fb633`;
- two valid catalogue headers;
- three losslessly round-tripped records;
- records CRC32 `6c941c28` and payload CRC32 `19c39b27`;
- Columns and Thrust are launchable using established profiles;
- Chuckie Egg is deliberately compatibility-status 2 (hidden) because a complete
  installed-firmware control/start profile has not yet been accepted.

This image is **not an authorised installation artifact**. The deployed migration
protocol is bound to the exact Stage 4I.7 pack and destination. Installing a different
pack would require a later, explicit firmware/protocol and flash-layout acceptance.

## Capacity

The writable game region `0x408000..0xFEFFFF` contains 12,484,608 bytes. After the
reserved Stage 4I.7 destination ending at `0x466FFF`, 12,095,488 bytes remain before
the permanent E10 guard at `0xFF0000`. The three-record proof uses 67,552 bytes.

At the measured three-game mean of 17,049 compressed bytes, 25 payloads plus duplicate
record tables and aligned headers would be roughly 438 KiB. The fail-closed upper bound
for 25 uncompressed 65,792-byte snapshots is 1,644,800 bytes before modest catalogue
overhead. Capacity is therefore not the current blocker; deterministic conversion,
controls and gameplay proof are.

## Host verification

- CPXPCK2 build and independent inspect: PASS; three records, two valid headers,
  CRCs and lossless LZ4 round trips valid.
- Established Stage 4J.3 host build: **26/26 tests PASS**, including CPC464 ROM,
  Knight Tyme, Manic Miner, launcher, CPCLIB1/CPXPCK2, installer/migration,
  importer, controls/Qw-ST, AY/audio, display, UF2 policy and fast-engine integration.
- The generic `build-host` directory was stale: its first 11 available tests passed,
  while seven registered audio binaries were absent. Regeneration was rejected because
  that directory's cached compiler is MSVC and the reviewed fast Z80 core requires GNU
  computed goto. No source or production configuration was changed to work around it.
- Stage 4I.7 pack preservation hash: PASS.
- Original source trees: no writes performed.

## Recommended next batch step

Convert in small deterministic waves, starting with Frogger, Ktris, Block Invasion,
Kiloroid and Harrier Attack. The first four are compact, single-DSK candidates and
Harrier Attack is a small loader/BIN pair. For each, discover the real AMSDOS launch
command, reach gameplay with emulated-frame input, eject media, run a fresh-snapshot
probe, and only then add a launchable record. Keep every failed run in quarantine.

Do not attempt physical installation or replace the Stage 4I.7 pack until a later stage
defines a compatible variable-pack migration protocol and separately approves the
flash write.
