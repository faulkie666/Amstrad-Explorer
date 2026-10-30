# Stage 4I.1 — local game-library inventory and import preparation

Date: 2026-10-30

Result: **PASS for read-only discovery and host assessment; HOLD for installation.**

The source collections were read and hashed in place. No source file was extracted into,
renamed, rewritten, moved, or deleted. No COM port, BOOTSEL volume, USB uploader, device
catalogue, or physical flash was accessed.

Complete per-game inventories are retained as CSV files:

- `artifacts/stage4i1-game-library/arcade_inventory.csv` — SHA-256
  `506bb3e36da42cc6f8dbfd1cb83e8a6e1f10bbaeb6f2aaa7305b056e46dd2001`;
- `artifacts/stage4i1-game-library/simulation_inventory.csv` — SHA-256
  `939691e27e6233fe274e6bc158e56739012706a65b98d3d383964a3a366d56fa`.

Each row records the title, source set, deterministic source-manifest SHA-256, formats,
file count, byte count, compatibility status, controls status, notes, and probable variant
group. The manifest hash covers the relative name, size, and SHA-256 of every file in a
game directory.

## 1. Inventory totals

| Collection | Game directories | Recursive directories | Files | Source bytes |
|---|---:|---:|---:|---:|
| Arcade | 1,593 | 1,700 (107 nested) | 9,593 | 94,063,228 |
| Simulation | 101 | 108 (7 nested) | 623 | 5,635,661 |
| **Total** | **1,694** | **1,808** | **10,216** | **99,698,889** |

The top-level directory is the collection's game unit. Nested directories generally hold
disk sides, levels, languages, or extracted support files rather than additional titles.

### Formats

| Format | Arcade | Simulation | Total | Current CPC Explorer input? |
|---|---:|---:|---:|---|
| Standalone SNA / SNA.GZ | 0 | 0 | 0 | Yes, if all validation gates pass |
| DSK.GZ | 95 | 6 | 101 | No |
| Plain DSK | 0 | 0 | 0 | No |
| CDT / CDT.GZ | 0 | 0 | 0 | No |
| ZIP / 7z / RAR | 0 | 0 | 0 | No |
| CPR | 1 | 0 | 1 | No |
| BAS | 402 | 61 | 463 | Only through a proved host-loading recipe |
| BIN | 2,169 | 238 | 2,407 | Not independently; load semantics are required |
| Extensionless AMSDOS-era files | 1,320 | 56 | 1,376 | Not independently |
| SCR | 106 | 5 | 111 | No; screen data is not a game image |
| `file_id.diz` metadata | 1,593 | 101 | 1,694 | Documentation only |

All 101 GZip members were inspected in memory without extracting into the collection.
Every member is a valid CPC DSK image, totalling 19,308,032 decoded bytes. There are no
decoded-image hash duplicates. No hidden SNA or CDT member was found.

## 2. Duplicates and alternatives

There are **zero exact duplicate game-directory manifests** and zero exact duplicate
decoded DSK images. A conservative normalized-title comparison found **24 probable
alternative-version groups containing 54 directories**. These include language,
publisher, lightgun/gunstick, and independently implemented versions—for example four
Antiriad languages, four Pac-Man implementations, three Super Pac implementations,
English/Spanish Double Dragon and Paperboy, and standard/lightgun Operation Wolf.

These 54 directories are alternatives, not safe byte-level duplicates. None was removed
or silently merged. The CSV `CanonicalTitle` and `VariantGroupSize` fields expose the
grouping.

Three titles explicitly advertise unsupported memory/machine editions in their names:
`Rick Dangerous 128+`, `Robotron. 6128`, and `Star Sabre 128k Edition`. Other titles are
not classified as 64 KiB merely because they lack such a label.

## 3. Exact CPC Explorer compatibility

The implemented importer accepts only a deterministic `.cpcgame` containing:

- one 65,792-byte standalone SNA (256-byte header plus 65,536-byte RAM dump);
- `MV - SNA` signature;
- SNA version 1, 2, or 3;
- exactly 64 KiB RAM;
- explicit machine identity 0 (CPC464) for SNA v2/v3;
- a validated descriptor, payload CRC, descriptor CRC, ID, and supported control profile.

SNA v1 has no machine-identity field; CPC464 compatibility must come from a proven native
construction/boot pipeline. The importer does not accept DSK, CDT, CPR, loose BASIC/BIN,
screen files, or arbitrary RAM dumps.

The firmware has no disk or tape runtime. A DSK/CDT game may become eligible only after a
host CPC464 boot reaches a complete playable state, a deterministic standalone snapshot is
captured and reloaded, and testing shows that no later media access is required. Renaming
or truncating media is not conversion.

### Compatibility counts

| Gate | Count |
|---|---:|
| Directly importable SNA files in these two source trees | 0 |
| Metadata files explicitly declaring 64K | 265 |
| Additional host-proven native CPC464 conversion (Chuckie Egg) | 1 |
| Evidence-backed 64K investigation leads | 266 |
| Fully validated standalone snapshot/package from these sources | 1 |
| Newly physically installable and controllable without more work | 0 |

The 266 figure is a discovery shortlist, not a compatibility claim: most still need boot,
media-independence, controls, and framebuffer validation. The single fully validated item
is Chuckie Egg, whose five gameplay actions do not fit the currently available direct
three-button profiles.

## 4. AY audio classification

No new candidate was labelled as musical based on filenames or reputation alone.

| Classification | Result |
|---|---|
| Proven sustained AY music | None among the new candidates |
| Proven occasional sound effect | Chuckie Egg: 20 changed sound-register writes, one approximately 1-second burst, 98.33% exact silence over the established 60-second trial |
| Proven no AY activity | None; no other candidate reached a valid standalone emulated state |
| Requires investigation | The other nine recommended candidates |

`Columns` contains `MUSICS2.BIN`/`PLAYINT9.BIN`, and Elite contains
`MUSIQUEX.BIN`/`NOTMUSIC.BIN`. Those filenames are useful leads but are not evidence that a
captured gameplay state produces sustained AY music. They remain in “requires
investigation”.

## 5. Recommended first batch

The SHA-256 below is the deterministic complete source-directory manifest from the CSV.
“Rejected” means the existing generic `amsdos-basic-bin` workflow failed closed; it does
not mean the original game is invalid.

| Area | Title / proposed ID | Source, format and manifest SHA-256 | RAM | Controls/start | AY evidence | Host/package result |
|---|---|---|---|---|---|---|
| Arcade | Chuckie Egg / `chuckie_egg` | `D:\Games\CPC Games\Arcade\Chuckie Egg`; BASIC-like `CHUCKIE` + `CHUCK.BIN`; `82cf7d04fec773418204b9b57131edfe8dbec6fba0da3e716333023c19b26e70` | Proven native CPC464 64K | S then 1 starts; Up, Down, Left, Right, Jump | Isolated effect, not music | **PASS:** deterministic SNA and package; controls remain a physical blocker |
| Arcade | Classic Invaders / `invaders` | `...\Classic Invaders`; loader + BIN + SCR; `856028f724c56b2d81baec48397c31a1c9c2624ea335b5717f215103052c67f5` | Unverified | Exact keys unverified; genre is a promising left/right/fire investigation | Not yet observed | Rejected by single-BIN workflow; screen/loader sequence must be reproduced |
| Arcade | Fruity Frank / `fruity_frank` | `...\Fruity Frank`; loader + BIN; `98b5646d5b59b1e9bdd2d7dc42ed4d86df786946218ba0527e18dc14060ec8b2` | Metadata says 64K | Exact controls unverified | Not yet observed | Rejected by current generic workflow |
| Arcade | Thrust / `thrust` | `...\Thrust`; loader + BIN; `1c4da26f7ee88a23921c6ca0f5e7996cdf1bae005d522ed56305e69f79412078` | Metadata says 64K | Space starts; gameplay controls require verification and likely exceed three actions | Not yet observed | Rejected by current generic workflow |
| Arcade | Motos / `motos` | `...\Motos`; loader + BIN + image; `1bb2e3ea4ecbc95875c4549f44ca203f7db36da5503d60e25acd3b91adfbc6cd` | Metadata says 64K | Exact controls unverified | Not yet observed | Rejected by single-BIN workflow; extra image dependency exists |
| Arcade | Bomb Jack / `bomb_jack` | `...\Bomb Jack`; loader + BIN; `977053bf4a240b65c71e16b0865eb35b29d59d19088fd8b2790869d88831a968` | Unverified | Exact movement/jump keys unverified | Not yet observed | Rejected by current generic workflow |
| Arcade | Columns / `columns` | `...\Columns`; BAS + five BIN files; `5d581009cd04cdb5f544054d4926fee2b68d1b25c67405db5ff4cbc1bdbb56a9` | Metadata says 64K | Exact keys/start sequence unverified | Music-named assets only; not execution evidence | Multi-file loader unsupported; not packaged |
| Simulation | Fighter Pilot / `fighter_pilot` | `D:\Games\CPC Games\Simulation\Fighter Pilot`; BAS + BIN; `a0bf76eb049ff64e0eb7935179ff8a4cb9729e36ae9e897e0bf1c0abf14cdee7` | Unverified | Flight controls not documented locally; expected to require more than three actions | Not yet observed | Rejected by current generic workflow |
| Simulation | Top Gun / `top_gun` | `...\Top Gun`; loader + BIN + image; `29b0de9f3e308bce8cccbca0a88e37f515f6bd8955ea87b2fecf8a46fa2653a6` | Metadata says 64K | Local metadata lists seven keys per player plus weapon selection | Not yet observed | Rejected by single-BIN workflow; physically unsuitable for three direct buttons |
| Simulation | Elite / `elite` | `...\Elite`; DSK.GZ plus extracted multi-file versions; `3627c2775a237fa3679e57deee167a0bc39768805ada7235c17e6740057d4c26` | Unverified | Keyboard-heavy simulation; later media/save dependencies must be audited | Music-named asset only; not execution evidence | DSK/multi-file runtime unsupported; not packaged |

Fifteen simple-looking BASIC/single-BIN pairs were tested through the existing generic
builder. All fifteen failed its structural pairing/loader checks with exit code 60,
including Bomb Jack, Fruity Frank, Classic Invaders, Thrust, Motos, Harrier Attack,
Fighter Pilot, Starglider, Top Gun, Auf Wiedersehen Monty, Captain Planet, Dan Dare III,
Deflektor, Dynamix, and Deadly Evil. No bypass, hand-edited snapshot, or title-specific
address hack was used.

## 6. Validated Chuckie Egg artifact

The prior deterministic conversion from the exact local `CHUCKIE` and `CHUCK.BIN` files
was rechecked with the current host emulator and importer:

| Artifact | Bytes | SHA-256 / CRC |
|---|---:|---|
| `CHUCKIE` source | 896 | `ccf60d743fd4565f473d3d061dfef1b561edd4340d449e8bbec5168dbacb5811` |
| `CHUCK.BIN` source | 9,216 | `3673e6da4ff52ca1f610b9d0ce688f8896f704dc3d2c732e2aed4c79207a655d` |
| `artifacts/stage4g2-chuckie/chuckie-egg.sna` | 65,792 | `2ce4e8f2eb14eada2e02bfc708a54872e291bdb47d85dd952a49099b3fdae08e` |
| `artifacts/stage4g2-chuckie/chuckie_egg.cpcgame` | 65,888 | `8c950a5833897dc8ab893a9d4fbcedf799eea41c4340c27fe1048179fb036323` |
| Payload / package CRC32 | — | `fbba06fe` / `c5663304` |

Importer result: ID `chuckie_egg`, CPC464, 64 KiB, SNA v1. A fresh deterministic host
reload/start probe passed. Its package currently carries control profile `none`; it must
not be installed until five-action input is solved and verified.

No additional `.cpcgame` was produced because no other candidate passed the existing
conversion and host-state gates.

## 7. Catalogue and storage audit

| Property | Value |
|---|---:|
| External flash | Winbond `EF-40-18`, 16,777,216 bytes |
| Firmware reservation | 4 MiB |
| Dual catalogues | `0x400000–0x407FFF`, two 16 KiB slots |
| Snapshot region | `0x408000–0xFEFFFF`, 12,484,608 bytes |
| Permanent E10 guard | `0xFF0000–0xFFFFFF`, 64 KiB |
| CPCLIB1 external descriptor limit | 192 |
| `.cpcgame` transport size | 65,888 bytes |
| SNA payload size | 65,792 bytes |
| Aligned flash allocation per upload | 69,632 bytes (17 sectors) |
| Absolute snapshot-allocation ceiling | 179 |

The known physical history contains two external games (Manic Miner and Dizzy) and one
removed synthetic allocation that intentionally remains above the persistent high-water
mark. On that verified history, three allocations are consumed, leaving 12,275,712 bytes
and room for **176** further snapshot allocations. The two external descriptors leave 190
descriptor slots, so storage—not catalogue count—is the binding limit.

Because COM access was prohibited, the current high-water value was not read from the
device. The 176 figure is therefore the conservative result derived from the established
transaction history, not a fresh physical status claim.

The ten proposed IDs are unique, within the 15-character package-ID limit, and do not
collide with `locomotive_basic`, `knight_tyme`, `manic_miner`, or `dizzy`. Ten eventual
valid uploads would consume 696,320 bytes and fit, but only after each independently
passes conversion, controls, audio, and media-independence gates.

## 8. Exact installation proposal

No installation is proposed in this stage. The safe staged sequence is:

1. Start with **Classic Invaders** as the next host conversion investigation because its
   likely gameplay can fit a left/right/fire profile, but first prove its actual keys and
   loader/SCR sequence rather than assuming them.
2. Capture a CPC464-native 64 KiB state only after the original loader runs correctly.
3. Reload twice, compare CPU/RAM/framebuffer state, exercise actual controls, run a
   60-second PSG trace, and prove no later media access.
4. Package with ID `invaders` only if all gates pass; inspect all CRCs and bounds.
5. In a separately authorized physical stage, read device status/list, verify the high
   water and free capacity, upload exactly that one package, verify catalogue integrity,
   and pause for LCD/control/audio acceptance.
6. Continue one game at a time. Never batch the remaining nine based only on this
   inventory.

Chuckie Egg remains package-ready but installation-HOLD because its Up/Down/Left/Right/
Jump requirement cannot be represented by the currently verified three direct game
buttons.

## 9. Required loader improvements

Host-side improvements needed for broader coverage are:

- a reusable AMSDOS BASIC/multi-file loader that executes loader semantics rather than
  merely copying files;
- controlled DSK boot/capture support for host conversion, including detection of later
  disk access;
- optional CDT host boot/capture if CDT assets are added later;
- explicit AY trace and controls evidence as part of every conversion;
- no physical DSK/CDT runtime is required or proposed here.

CPR, 128K/6128 editions, multi-disk games that need later media, lightgun titles, and
keyboard-heavy simulations remain unsupported by the current physical input/runtime.

## 10. Preservation

Protected firmware hashes remain unchanged:

- Stage 4H.4 `0.4.15-stage4h4ayaudio`:
  `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`;
- Stage 4H.5D `0.4.17-stage4h5dayaudioexp`:
  `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`.

No firmware was built, no Z80 or flash-layout source was changed, and all four installed
games remain untouched.
