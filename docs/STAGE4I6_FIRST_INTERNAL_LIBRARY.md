# Stage 4I.6 — first playable internal compressed game library

Date: 2026-10-30  
Mode: host verification and experimental build only

## Outcome

**PASS for a host-verified experimental firmware candidate. Physical status is
NOT TESTED.**

The Stage 4I.5 format proof is now a linkable RP2350 implementation. It mounts
a CPXPCK2 image directly from XIP, validates the dual catalogue and every
selected record, decodes the LZ4 RAM body directly into existing CPC RAM,
restores CPU/peripheral/AY state, and provides a flash-backed
Arcade/Simulation → A-Z → Game menu. It does not allocate a snapshot buffer or
an in-memory catalogue.

The final RP2350 link preserves the mandatory 116 KiB main-SRAM headroom gate,
both core stacks, the dual-core AY scratch layout, and the byte-identical SRAM
Z80 routine. A separate experimental UF2 was produced. Neither it nor the
offline library image was written to hardware. The Arcade and Simulation
source trees were read only; no COM port, BOOTSEL volume, catalogue, installed
game, or physical flash was accessed.

## A. Reader, decoder, and memory proof

### Runtime design

`src/packed_library.cpp` implements the bounded CPXPCK2 reader. Its permanent
RP2350 state is a 20-byte `packed::View`: two XIP pointers, image size, record
count, and generation. Records remain in flash and are scanned when needed.

The reader verifies:

- two possible 128-byte headers, header identity and CRC;
- generation selection and equal-generation agreement;
- record-table size, CRC, bounds and category/A-Z/title ordering;
- unique IDs and valid CPC464/64 KiB/codec/control metadata;
- image and payload-area CRC;
- per-record compressed CRC before decode;
- SNA signature, version, RAM size, and v2/v3 CPC464 identity;
- all additions, input bounds, output size, and input/output non-overlap;
- raw 65,792-byte SNA CRC after decode.

The first 256 SNA bytes stay uncompressed in XIP. The raw LZ4 block expands
straight into `machine.ram`; the decoder has no history buffer or persistent
workspace. CPU registers, alternate registers, interrupt state, ROM/RAM
banking, gate array, CRTC, PPI and all 16 AY registers are then restored with
the same semantics as the existing SNA loader. The fast engine reloads its CPU
state only after restoration succeeds.

Malformed, absent, truncated, overlapping or corrupt data causes the mount or
launch to fail. If mounting fails, `packed::View` is cleared and the existing
CPCLIB1/BASIC/Knight Tyme launcher remains active.

### Exact linked memory map

| Item | Stage 4H.5D | Stage 4I.6 | Change |
|---|---:|---:|---:|
| main-SRAM `__bss_end__` | implied by 119,100-byte headroom | `0x200626D8` | +20 bytes persistent state |
| main-SRAM headroom | 119,100 | 119,080 | -20 |
| mandatory minimum | 118,784 | 118,784 | unchanged |
| margin above gate | 316 | **296** | -20 |
| minimum heap | 2,048 | 2,048 | unchanged |
| core-0 stack | 2,048 at `0x20081800` | 2,048 at `0x20081800` | unchanged |
| core-1 stack | 2,048 at `0x20080800` | 2,048 at `0x20080800` | unchanged |
| AY control, scratch X | 1,216 at `0x20080000` | identical | unchanged |
| AY history, scratch Y | 2,048 at `0x20081000` | identical | unchanged |
| unused scratch-X gap | 832 | 832 | unchanged |

The menu controller adds only scalar navigation state on the existing core-0
stack. No stack reservation was reduced. Its deepest operation scans XIP one
record at a time and does not recurse.

`z80_execute_step` remains at `0x20001280`, exactly 76,212 bytes. The extracted
routine is byte-identical to Stage 4H.5D, SHA-256
`da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`.

Both unchanged post-link scripts passed:

```text
stage4f6 SRAM gate: step=0x20001280+76212 heap=2048 stack=2048
  headroom=119080 minimum=118784
stage4h5b link gate: control=0x20080000+1216
  core1_stack=0x20080800+2048 dc=0x20081000+2048
  core0_stack=0x20081800+2048 scratch_x_gap=832
  main_headroom=119080 minimum=118784
```

## B. Gameplay automation

All new interaction used konCePCja v6.4.2 IPC commands. There were no GUI
clicks or SendKeys calls. Inputs were held for deterministic emulated frames,
followed by bounded 500-VBL observation intervals, screenshots, VRAM/RAM hashes,
PSG reads, YM capture and final snapshot save.

The reproducible classification and hashes are in
`artifacts/stage4i6-gameplay/validation-manifest.json`.

### Results

| Game | Loader/state result | Gameplay evidence | Diskless restore | Controls | Classification |
|---|---|---|---|---|---|
| Knight Tyme | established snapshot | established menu/game regression | PASS | existing profile | PASS |
| Manic Miner | native CPC464 snapshot | movement/jump regression | PASS | Q/W/Shift profile | PASS |
| Dizzy | established snapshot | established physical gameplay | PASS | Z/X/Space profile | PASS |
| Columns | restored directly into active timed game | falling columns and timer continued | PASS | proven path uses CPC joystick | HOLD_CONTROLS |
| Fruity Frank | speed selection reached a live level | character/level visible, state later reached game over | PASS | needs more actions than the current profile | HOLD_CONTROLS |
| Thrust | `O` answered the French infinite-lives prompt; Space started | live level, fuel/lives/score and ship visible | PASS | Z/X/Return/Shift/Space: five actions | HOLD_CONTROLS |
| Classic Invaders | authentic second load and `0x4200` entry | bounded inputs remain at game selector | n/a | unproven | HOLD_GAMEPLAY_START |
| Bomb Jack | numeric mode selector and title reached | title did not progress under bounded fire/Space probes | n/a | unproven | HOLD_TITLE |
| Fighter Pilot | title and options reached | no take-off state reached | n/a | many simulation inputs expected | HOLD_OPTIONS |

This stage did not claim ten new games. Repeated attempts exposed a common,
specific blocker: valid diskless states exist, but complete Explorer control
profiles do not. Processing dozens more loaders before solving that bounded
input problem would again manufacture title-screen false positives.

No new title is marked Explorer-playable. The useful growth is three additional
disk-independent host snapshots (Columns, Fruity Frank, and Thrust), of which
Columns and Fruity Frank are retained in the offline image as compatibility
status 2. The firmware filters status-2 records from the launchable menu until
their controls are implemented. Thrust is not packed because it requires five
direct gameplay actions.

### Authentic AY observations

- Columns retains the Stage 4I.4 sustained-music evidence (3,000/3,000 changing
  YM frames).
- Fruity Frank's Stage 4I.6 probe recorded 755 AY register changes and 917
  volume-active frames over 2,525 frames.
- Thrust recorded 131 changes and 326 volume-active frames over 2,525 frames.
- Bomb Jack's bounded title probe had no AY activity.
- No soundtrack was synthesized or added to any title.

## C. Deterministic pack and storage ledger

The final offline CPXPCK2 image contains five disk-independent snapshots:

| Record | Status | Raw | Compressed payload |
|---|---|---:|---:|
| Columns | control HOLD | 65,792 | 21,263 |
| Treasure Island Dizzy | launchable | 65,792 | 41,356 |
| Fruity Frank | control HOLD | 65,792 | 22,373 |
| Knight Tyme | launchable | 65,792 | 32,801 |
| Manic Miner | launchable | 65,792 | 27,447 |

The image is 161,664 bytes with SHA-256
`19e87ce24698aeba613a6f691babf3334f1b8e87c627b9a0d50df82bc4cac1c0`.
Two independent builds were byte-identical. Both headers and record tables,
category/A-Z ordering, unique IDs, all payload CRCs and all decompressed SNA
hashes passed. The image is an offline artifact only.

### Exact hypothetical flash ledger

| Flash offset | Bytes | Purpose |
|---|---:|---|
| `0x000000–0x3FFFFF` | 4,194,304 | application reservation |
| `0x400000–0x403FFF` | 16,384 | legacy CPCLIB1 catalogue A |
| `0x404000–0x407FFF` | 16,384 | legacy CPCLIB1 catalogue B |
| `0x408000–0x43AFFF` | 208,896 | three frozen 69,632-byte legacy allocations |
| `0x43B000–0x46277F` | 161,664 | proposed CPXPCK2 image |
| `0x462780–0xFEFFFF` | **12,114,048** | truly free after hypothetical migration |
| `0xFF0000–0xFFFFFF` | 65,536 | permanent E10 guard |

The 208,896-byte discrepancy requested in this stage is exact: subtracting only
the pack from the original 12,484,608-byte game region ignores the three frozen
legacy allocations. `3 × 69,632 = 208,896`. Therefore:

```text
12,484,608 original game region
- 208,896 legacy high-water
- 161,664 CPXPCK2 image
= 12,114,048 true remaining bytes
```

Those allocations cannot be double-counted as free merely because an earlier
test entry was removed; the transactional removal design intentionally does not
reclaim payload storage.

Before any physical CPXPCK2 write, both legacy catalogues must transactionally
publish a saved allocation high-water that reserves the complete CPXPCK2/tail
region. This makes old-firmware uploads fail closed instead of overwriting the
pack. Existing descriptors must remain unchanged. That migration was not
performed here.

## D. Menu and experimental firmware

When a valid pack is present, the menu is:

1. Locomotive BASIC
2. Arcade
3. Simulation

Selecting a category opens only populated A-Z letters, then launchable games.
Each submenu includes `< Back`. X/Z/Y retain up/down/select semantics. Record
ordering is stored in flash; no index or title table is copied to SRAM.
Compatibility-status-2 records remain auditable in the pack but are hidden.

If the pack is absent or malformed, mounting clears the packed view and the
unchanged legacy four-game experience is used. Upload/removal, flash probing,
dual-core AY, active-display fit, the fast engine and recovery remain linked.

Experimental firmware:

- identity: `0.4.18-stage4i6internalexp`;
- UF2: `build-rp2350-stage4i6-internal-library/cpc_explorer_internal_library_exp.uf2`;
- size: 722,432 bytes;
- SHA-256: `949dade192c24f248f2d5f6c65466eb93d5c69abae16b4ea720d11f6ad5fa31a`;
- linked binary end: `0x10058180`;
- firmware binary: 360,832 bytes;
- first-4-MiB reserve margin: 3,833,472 bytes.

UF2 policy result:

- 1,411 physical blocks;
- 1,410 ordinary RP2350 ARM-S blocks, family `0xE48BFF59`;
- application targets `0x10000000–0x100581FF` (exclusive validator end
  `0x10058200`), entirely within the first 4 MiB;
- exactly one authorized E10 block at `0x10FFFF00`, family `0xE48BFF57`, flags
  `0x0000A000`, extension `0x9957E304`, 256 bytes of `0xEF`;
- no catalogue or game-storage UF2 targets.

## Verification

The original 20 host regressions pass, plus the new focused CPXPCK2 test:
**21/21 PASS**.

The focused test proves A/B fallback, catalogue ordering, hierarchy navigation,
truncation and corruption rejection, duplicate/bounds policy, overlap rejection,
exact LZ4 RAM round-trip, CPU state, framebuffer and AY-register equivalence,
and launch selection. Existing BASIC, Knight Tyme, Manic Miner, Dizzy, upload,
removal, flash/E10, display, controls, fast/reference engine and AY tests remain
green.

Preserved rollback hashes:

- Stage 4H.4:
  `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`;
- Stage 4H.5D:
  `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`.

## Next separately approved physical acceptance

No physical action is authorized by this report. A future acceptance plan must
separate the two writes:

1. re-verify rollback, candidate and pack hashes;
2. transactionally reserve the pack/tail in both CPCLIB1 catalogues and verify
   readback before any bulk write;
3. program and read back the CPXPCK2 image at `0x43B000` with interruption-safe
   publication ordering;
4. only then deploy the experimental UF2 once through verified RP2350 BOOTSEL;
5. verify fallback first with no/invalid pack in a host or disposable image;
6. on hardware verify BASIC, Arcade → D/K/M, compressed launch, Manic controls,
   Dizzy controls, Knight Tyme, AY audio, USB status, and reset persistence;
7. do not expose Columns or Fruity Frank until their control profiles pass.

## Final status

- **STAGE 4I.6:** PASS (host/build), PHYSICAL NOT TESTED
- **EXISTING HOST SUITE:** 20/20 PASS
- **FOCUSED CPXPCK2 TEST:** PASS (21/21 total)
- **LINKER SRAM GATE:** PASS, 119,080 bytes headroom, 296-byte margin
- **PLAYABLE COMPRESSED SUBSET:** BASIC system entry plus Knight Tyme, Manic Miner, Dizzy
- **NEW EXPLORER-PLAYABLE GAMES:** 0
- **NEW DISK-INDEPENDENT HOST STATES:** 3 (Columns, Fruity Frank, Thrust)
- **OFFLINE PACK:** 5 snapshots, 161,664 bytes, SHA-256 `19e87ce24698aeba613a6f691babf3334f1b8e87c627b9a0d50df82bc4cac1c0`
- **TRUE FLASH REMAINING AFTER HYPOTHETICAL MIGRATION:** 12,114,048 bytes
- **EXPERIMENTAL UF2:** built and host-verified; SHA-256 `949dade192c24f248f2d5f6c65466eb93d5c69abae16b4ea720d11f6ad5fa31a`
- **AY AUDIO:** snapshot registers preserved; host tests PASS; physical not tested
- **PHYSICAL DEVICE ACCESSED:** NO
- **DEPLOYMENT:** NOT AUTHORIZED
