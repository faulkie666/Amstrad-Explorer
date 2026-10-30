# Stage 4I.7 — internal library controls and physical readiness

Date: 2026-10-30  
Mode: host development, offline migration simulation and experimental build only

## Outcome

**Host verification PASS. Firmware build PASS. Physical migration HOLD. Physical
testing NOT TESTED and deployment NOT AUTHORISED.**

Stage 4I.7 uses all six active-low switches fitted to the Pimoroni Explorer
PIM720. Columns and Thrust now have complete, direct, metadata-selected control
profiles and pass deterministic tests through the actual CPC core. They join
Knight Tyme, Manic Miner and Treasure Island Dizzy, giving five launchable game
records plus the separate Locomotive BASIC system entry.

Fruity Frank remains hidden. Although a six-button profile can represent its
start key and joystick, the preserved snapshot returns to game-over/speed
selection before a durable movement and restart path can be established. No
title was exposed merely because its inputs fit electrically.

No COM port, BOOTSEL volume, catalogue, installed game, or physical flash was
accessed. The original Arcade and Simulation source trees were read only.

## 1. Hardware controls and profiles

The board schematic/pin header and earlier physical evidence agree on six
switches. They connect to ground when pressed and use GPIO pull-ups:

| Explorer button | GPIO |
|---|---:|
| C | 14 |
| B | 15 |
| A | 16 |
| X | 17 |
| Y | 18 |
| Z | 19 |

Stage 4I.6 initialized only X/Y/Z. Stage 4I.7 initializes A/B/C as the same
active-low, pull-up inputs. The launcher still responds only to X/Z/Y for
up/down/select. A/B/C have no menu side effects. Gameplay remains level based:
holding a switch holds its CPC key/joystick line, releasing it releases the
line, and the launch-selection press is suppressed until every switch has been
released.

Profiles remain generic metadata values rather than game-ID conditionals:

| Value | Profile | Physical mapping |
|---:|---|---|
| 0 | none | no injected input |
| 1 | Q/W/Shift | X=Q, Z=W, Y=bare Shift |
| 2 | Z/X/Space | X=Z, Z=X, Y=Space |
| 3 | digital joystick | X=left, Z=right, A=up, B=down, Y=fire |
| 4 | joystick plus start | profile 3 plus C=F; retained for Fruity Frank HOLD only |
| 5 | Thrust five-key | X=Z, Z=X, A=Return, B=bare Shift, Y=Space |

Profile 3 selects the CPC core's digital-joystick matrix before input is
applied. Other profiles explicitly restore keyboard/no-joystick mode. No
essential action uses a chord.

### Columns — newly unlocked

The genuine game state accepts CPC joystick input. Deterministic konCePCja IPC
tests exercised fire, left, right, down and up from the diskless snapshot. Each
input produced a distinct RAM/video transition. A focused test repeats those
five actions through the project's actual CPC core and requires a deterministic
state difference from the neutral run.

- Snapshot SHA-256:
  `f1b7872aee6a1e86d5ded43e08a116f932983259223be78685d14c813d7ebf99`
- Raw/compressed: 65,792 / 21,263 bytes
- Control profile: 3
- Evidence: `artifacts/stage4i7-controls/columns`
- Media after restore: none
- AY: sustained prior evidence, 3,000/3,000 changing YM frames

### Thrust — newly unlocked

The authentic `RUN"THRUST"` conversion reached live gameplay after answering
the infinite-lives prompt and starting with Space. Its documented state uses
five direct actions, all now available without chords:

- X -> CPC Z: rotate left
- Z -> CPC X: rotate right
- A -> Return: fire
- B -> bare Shift: thrust
- Y -> Space: shield/tractor beam (and the established start action)

All five keys were replayed through konCePCja IPC and the actual project core.
The focused test requires every action to alter deterministic CPC state.

- Snapshot SHA-256:
  `2afa403436472be94c5e0a2c58eda35c60d0fb6b14c0230655022ce19b9d0808`
- Raw/compressed: 65,792 / 17,691 bytes
- Control profile: 5
- Evidence: `artifacts/stage4i7-controls/thrust`
- Media after restore: none
- AY: 131 register changes and 326 volume-active frames in 2,525 frames

### Fruity Frank — remains HOLD

Profile 4 can map directions/fire directly and retain C=F for the speed screen.
The electrical/input problem is therefore solved, but the snapshot acceptance
problem is not. Repeated bounded probes select F and then return too quickly to
game-over/speed selection to prove reliable movement, loss/restart and
continued play. It remains compatibility status 2 and cannot be selected.

### Other bounded candidates

Ten candidates were bounded by existing deterministic conversion evidence and
the new control probes:

| Result | Titles | Reason |
|---|---|---|
| PASS (2) | Columns, Thrust | diskless gameplay plus complete direct controls |
| HOLD (5) | Fruity Frank, Chuckie Egg, Classic Invaders, Bomb Jack, Fighter Pilot | respectively restart state, title/redefinition start, gameplay start, title transition, take-off/options |
| UNSUPPORTED (3) | Motos, Top Gun, Elite | multi-file dependency, excessive controls, DSK/media plus excessive controls |

Chuckie Egg's five gameplay actions now fit the board, but its K/S/player-count
flow is not yet a self-contained physical profile; it is not promoted. Full
hashes and evidence are recorded in
`artifacts/stage4i7-controls/validation-manifest.json`.

## 2. Menu and compressed pack

The menu remains:

```text
Locomotive BASIC
Arcade
  A-Z
    launchable games only
Simulation
  A-Z
    launchable games only
```

Records are scanned directly in XIP in category/letter/title order. No complete
catalogue or title index is copied into SRAM. Compatibility-status-2 records
remain in the image for audit but `launchable_count`, A-Z enumeration and game
selection exclude them.

The deterministic Stage 4I.7 pack has six records:

| Game | Compatibility | Profile | Compressed bytes |
|---|---:|---:|---:|
| Columns | launchable | 3 | 21,263 |
| Treasure Island Dizzy | launchable | 2 | 41,356 |
| Fruity Frank | hidden HOLD | 4 | 22,373 |
| Knight Tyme | launchable | 0 | 32,801 |
| Manic Miner | launchable | 1 | 27,447 |
| Thrust | launchable | 5 | 17,691 |

- Image: `artifacts/stage4i7-internal-library/library.cpxpack`
- Bytes: 179,360
- SHA-256:
  `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`
- Record-table CRC-32: `1f5e3acb`
- Payload-area CRC-32: `4cc05874`
- Valid headers: 2/2
- Determinism: two builds are byte-identical

The CPXPCK2 inspector decompressed and CRC-checked every 65,792-byte snapshot.
Authentic AY registers remain part of snapshot restoration; no soundtrack was
invented or substituted.

## 3. Linker and firmware proof

Experimental firmware:

- identity: `0.4.19-stage4i7controlsexp`
- file:
  `build-rp2350-stage4i7-controls/cpc_explorer_internal_library_controls_exp.uf2`
- UF2 bytes: 723,456
- SHA-256:
  `f751ab24cd3ac395f065252e380303327a485118ede8915c5381a1d7673ff60e`
- linked binary end: `0x100583A4`
- firmware binary: 361,380 bytes
- first-4-MiB margin: 3,832,924 bytes

Link gates:

```text
stage4f6 SRAM gate: step=0x20001280+76212 heap=2048 stack=2048
  headroom=119080 minimum=118784
stage4h5b link gate: control=0x20080000+1216
  core1_stack=0x20080800+2048 dc=0x20081000+2048
  core0_stack=0x20081800+2048 scratch_x_gap=832
  main_headroom=119080 minimum=118784
```

Main-SRAM headroom is unchanged from Stage 4I.6: 119,080 bytes, 296 bytes
above the mandatory gate. Both stacks, heap reservation and dual-core AY
scratch layout are unchanged. The SRAM `z80_execute_step` routine remains
76,212 bytes with SHA-256
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`,
byte-identical to Stage 4H.5D.

UF2 policy:

- 1,413 physical blocks;
- 1,412 application blocks, family `0xE48BFF59`;
- targets `0x10000000` to exclusive `0x10058400`, inside the first 4 MiB;
- one exact E10 compatibility block at `0x10FFFF00`, family `0xE48BFF57`,
  flags `0x0000A000`, extension `0x9957E304`, 256 bytes of `0xEF`;
- no catalogue, legacy snapshot, CPXPCK2, or other game-storage UF2 targets.

Preserved hashes:

- Stage 4I.6:
  `949dade192c24f248f2d5f6c65466eb93d5c69abae16b4ea720d11f6ad5fa31a`
- Stage 4H.4:
  `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Stage 4H.5D:
  `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`

## 4. Host verification

The complete Release suite is **22/22 PASS**. This is the prior 21 tests plus
the Stage 4I.7 actual-core control test. Coverage includes BASIC, Knight Tyme,
Manic Miner, Dizzy, launcher, upload/removal, flash/E10 policy, reference/fast
engines, display, AY, CPXPCK2 mount/fallback/CRC/LZ4 and the new six-button
profiles.

The profile test proves held/released transitions, simultaneous A/B/Y input,
launch-button suppression, invalid metadata rejection and exact key bindings.
The core test restores Columns and Thrust independently for every essential
action and compares the resulting deterministic CPC state with a neutral run.

## 5. Exact hypothetical flash layout

The pack is not present on the physical Explorer. The following is the proposed
layout, based on the established three frozen 69,632-byte legacy allocations:

| Flash offset | Bytes | Purpose |
|---|---:|---|
| `0x000000-0x3FFFFF` | 4,194,304 | application reservation |
| `0x400000-0x403FFF` | 16,384 | CPCLIB1 catalogue A |
| `0x404000-0x407FFF` | 16,384 | CPCLIB1 catalogue B |
| `0x408000-0x43AFFF` | 208,896 | three preserved legacy snapshot allocations |
| `0x43B000-0x466C9F` | 179,360 | CPXPCK2 bytes |
| `0x466CA0-0x466FFF` | 864 | erased/reserved sector tail |
| `0x467000-0xFEFFFF` | **12,095,488** | free after completed migration |
| `0xFF0000-0xFFFFFF` | 65,536 | permanent E10 guard |

The legacy catalogue high-water is relative to `0x400000`; both copies must
therefore be advanced from `0x3B000` to `0x67000`. Existing descriptors and
payloads remain unchanged. Stage 4H.4 understands and validates this CPCLIB1
high-water field, so rollback remains bootable and future old-firmware uploads
start after the reserved pack rather than overwriting it.

This layout is conditional on a physical read confirming the expected flash
identity, capacity, catalogue version, descriptor set, generations, CRCs and
current high-water. No such read was authorised in this stage.

## 6. Fail-closed installation and rollback plan

`tools/stage4i7_migration_audit.py` is intentionally offline-only and contains
no COM, USB, drive or flash backend. It verifies the supplied pack/UF2 hashes,
UF2/E10 policy, expected `EF-40-18` JEDEC identity, 16,777,216-byte capacity,
catalogue version and bounds. It rejects mismatches. Its recorded simulation is
`artifacts/stage4i7-internal-library/migration-audit.json`.

A future separately approved implementation must execute these phases without
automatic retries:

1. Read only: require exactly one normal Explorer; require firmware/USB identity,
   `EF-40-18`, 16 MiB and idle upload/removal state.
2. Read and validate both complete CPCLIB1 catalogues. Require version 1, valid
   generations/CRCs, exactly the three expected external snapshots and no
   pending transaction. Preserve byte-for-byte backups.
3. Verify Stage 4H.4, Stage 4H.5D, Stage 4I.6, Stage 4I.7 and pack SHA-256 values.
4. Publish a higher-generation inactive catalogue with unchanged descriptors
   and saved high-water `0x67000`; read it back and validate it.
5. Publish/verify the other catalogue with the same safe high-water. Do not
   erase any pack sector until both copies validate. A stop after only one copy
   leaves the pack untouched.
6. Erase only `0x43B000-0x466FFF`. Program payload and both record tables first.
   Read back and verify those bytes before publishing either header.
7. Program and verify header B, then header A. A partial image fails CPXPCK2
   CRC validation and Stage 4I.7 falls back to the legacy launcher. This is
   ordered recovery, not an atomic flash-write claim.
8. Read back `0x43B000-0x466C9F`; require exact SHA-256 and a complete CPXPCK2
   inspection. On ambiguous timeout, stop and perform read-only diagnosis.
9. Only after catalogue reservation and pack verification, request separate
   BOOTSEL approval and copy the validated Stage 4I.7 UF2 exactly once.
10. Verify normal USB identity/status, the five visible games, hidden Fruity
    record, BASIC, all controls, AY audio and reset persistence. Never retry a
    failed/ambiguous flash operation automatically.

Interruption simulation covers every publication boundary. Before both
catalogues are verified, programming the pack is forbidden. After both are
safe, an absent/partial pack leaves the legacy entries bootable and the new
firmware falls back. After header B publication the fully verified secondary
copy can mount; header A completes redundancy. Stage 4H.4 rollback ignores the
pack but respects the advanced high-water.

### Readiness blocker

The current USB protocol supports transactional individual SNA installation
and removal only. It has no command to update catalogue high-water without
changing descriptors, no bounded CPXPCK2 region writer, and no pack readback
operation. The offline audit cannot replace those missing device operations.
Consequently **FLASH MIGRATION READINESS is HOLD** until a narrowly scoped,
host-tested migration transport exists and receives separate approval. No
physical procedure should improvise raw offsets through the existing uploader.

## Final status

- **HOST VERIFICATION:** PASS — 22/22
- **FIRMWARE BUILD:** PASS — isolated experimental candidate
- **PLAYABLE GAME COUNT:** 5, plus Locomotive BASIC
- **NEWLY UNLOCKED:** Columns and Thrust
- **COMPRESSED PACK:** PASS — six records, one hidden HOLD
- **MAIN-SRAM GATE:** PASS — 119,080 >= 118,784 bytes
- **FLASH MIGRATION READINESS:** HOLD — safe physical writer/readback protocol not implemented
- **PHYSICAL TESTING:** NOT TESTED
- **DEPLOYMENT:** NOT AUTHORISED

Stop here. Explicit approval plus the missing migration transport and a fresh
read-only device preflight are required before any physical operation.
