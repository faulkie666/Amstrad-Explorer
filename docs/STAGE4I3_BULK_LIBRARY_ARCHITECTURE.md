# Stage 4I.3 — bulk game-library architecture

Date: 2026-10-30

Result: **PASS for the host-only architecture study; HOLD for Classic Invaders
capture; direct DSK execution is UNPROVEN.**

No COM port, BOOTSEL volume, RP2350, external flash, installed catalogue, or
source game was accessed or modified. No emulator was downloaded or installed.
No firmware or UF2 was built. The stable Stage 4H.4 and experimental Stage
4H.5D artifacts were re-hashed and remain unchanged.

## Executive decision

The current fixed-snapshot library is a good compatibility format for a small
set of validated games, but it cannot be the permanent store for all 1,694
titles. Its hard limits are 192 descriptors and 179 fixed flash allocations;
the verified device history leaves 176 further allocations. Even ideal
catalogue packing would not make 16 MiB sufficient.

The recommended long-term design is a **hybrid, read-only library on removable
storage**:

1. keep the four existing internal games and CPCLIB1 recovery path unchanged;
2. prepare a deterministic `ExplorerReady/Arcade/A-Z` and
   `ExplorerReady/Simulation/A-Z` tree on the host;
3. store a compact, sorted, paged index and source media on removable storage;
4. use validated compressed standalone snapshots first;
5. add direct DSK execution only after a separate CPC464+DDI-1/AMSDOS/FDC
   prototype passes resource, timing, media-access, and audio tests.

This architecture can represent all 1,694 source directories without holding
their catalogue or media in main SRAM. It does **not** imply that all 1,694
games are compatible or immediately playable.

## 1. Classic Invaders deterministic automation

### Existing host capability

The CPC Explorer core already has deterministic, level-based keyboard injection:
`key_down()`, `key_up()`, and `run_us()`. Existing tests hold a key across
multiple emulated frames and release it explicitly. This proves that the key
matrix itself is not limited to GUI-style tap events.

That mechanism cannot finish the authentic Classic Invaders boot by itself.
The production CPC464 profile has no live disk service, while the authentic
loader is being executed from a constructed disk under CPCE. Stage 4I.2 reached
the genuine prompts but the external GUI automation could not guarantee a
key-down state across a CPC scan interval. Manual GUI automation is therefore
retired.

### konCePCja evaluation

The documented konCePCja IPC provides exactly the missing primitives:

- a Windows headless mode;
- a localhost text protocol with a startup manifest that reports the actual
  port and machine model;
- `input keydown`, `input keyup`, and `input key ... hold=N`, where the held
  form guarantees that N frames begin with the key down;
- `step frame N` and `wait vbl N` for deterministic frame advancement;
- CPC464 model selection (`model: 0` in the manifest);
- DSK loading and uPD765/FDC I/O breakpoints;
- register, RAM, CRTC, Gate Array and PSG inspection;
- framebuffer hashes/screenshots and `snapshot save`.

These features are documented in the upstream
[IPC protocol](https://github.com/ikari-pl/konCePCja/blob/master/docs/ipc-protocol.md)
and [project README](https://github.com/ikari-pl/konCePCja). The emulator is
source-available for personal, hobby and internal use, but its license does not
permit redistributing the emulator or a fork. CPC Explorer may use a locally
installed copy as a conversion tool; it must not vendor or publish it.

No local konCePCja installation was found in the project, Documents, Desktop,
or Downloads trees. The user's instruction prohibits downloading or installing
it without separate approval, so no execution proof was attempted in this
stage.

### Bounded proof procedure for a separately approved stage

Use a pinned Windows release with its release hash recorded, an isolated
working directory, and only loopback IPC. Disable unnecessary non-loopback
network access. A conversion script should:

1. start headless, parse the startup manifest, and require CPC464/64K;
2. load the Stage 4I.2 scratch DSK and type the authentic `RUN` command;
3. wait for the infinite-lives prompt, then issue a held `N` or `Y` event for
   at least three full frames;
4. wait for the level prompt and hold `1` for at least three frames;
5. wait for PC `0x4200`, advance deterministic frames, and record register,
   RAM and framebuffer hashes;
6. identify and exercise actual gameplay controls;
7. arm FDC I/O breakpoints and run long enough to prove whether later disk
   reads occur;
8. inspect PSG activity without inventing audio;
9. save a CPC464 64K SNA, reload it twice, and compare the same checkpoints;
10. validate the SNA through CPC Explorer before creating a package.

### Classic Invaders decision

**HOLD.** Deterministic emulated-frame input is a technically credible fix,
but this stage cannot honestly report PASS without running the approved tool,
reaching gameplay, proving media independence, and reloading the captured SNA.
The exact blocker is the absence of an approved local headless DSK emulator,
not an unresolved CPC keyboard-matrix defect.

## 2. Direct DSK execution feasibility

### What the current firmware actually contains

The project vendors useful upstream components, but the production build is
intentionally not a disk-capable CPC:

- `init_cpc_state()` selects `CPC_TYPE_464` and supplies only the OS and BASIC
  ROMs. It supplies no DDI-1/AMSDOS expansion ROM.
- `fdd.h` sets sides, tracks, sectors and sector size to one, leaving a
  one-byte inert disk-data slot. The source comment says this avoids reserving
  nearly one MiB for an image.
- the reference Chips path contains `upd765_iorq()` and DSK parsing helpers,
  but those helpers cannot accept a real disk with the one-slot profile;
- the deployed fast I/O path services PPI/keyboard/AY, Gate Array and CRTC
  transactions, but it does not dispatch FDC ports to the uPD765 at all;
- no menu, catalogue or package format mounts a DSK image.

Consequently, copying a DSK to flash or renaming it as a snapshot cannot work.

### Required CPC subsystem

A credible CPC464 disk design needs all of the following:

- the DDI-1 disk interface and correctly mapped 16 KiB AMSDOS expansion ROM;
- uPD765 command/status/data behavior, motor and drive state;
- sector timing, data-result ordering and interrupt/DRQ behavior required by
  loaders and protection schemes;
- standard and extended DSK parsing with strict bounds validation;
- read-only media semantics initially, or a separate copy-on-write layer;
- the same FDC I/O behavior in both the fast and reference engines;
- snapshot serialization of disk-controller state if disk-resumable snapshots
  are promised;
- audit of every title for 64K CPC464 compatibility and later disk access.

An AMSDOS ROM is not permission to redistribute it. The same provenance and
copyright controls used for the CPC ROM must apply.

### Resource estimate

Restoring a conventional whole-disk `fdd_t` buffer is unsuitable: the existing
source explicitly removed a near-1-MiB reservation, while the RP2350 has only
520 KiB SRAM and this project enforces at least 116 KiB main-SRAM headroom.
The official [Pimoroni Explorer specification](https://shop.pimoroni.com/products/explorer)
lists 520 KiB SRAM and 16 MiB QSPI flash.

A streaming, read-only sector backend is the only plausible small-memory
design. A first budget—not yet linker-proven—is:

| Component | Bounded design estimate |
|---|---:|
| FDC/controller and drive state | 1–3 KiB |
| parsed track/sector metadata cache | 3–8 KiB |
| one/two sector or track windows | 4–16 KiB |
| storage I/O/DMA buffers | 1–4 KiB |
| AMSDOS ROM | 16 KiB flash/XIP, not SRAM |
| total new main-SRAM pressure | approximately 9–31 KiB |

The low end requires on-demand sector lookup and small windows. None of these
figures is accepted until an isolated linker map preserves the unchanged
116-KiB gate.

The fast engine currently reaches only about 69–74% CPC speed in the measured
games. FDC work is intermittent rather than continuous, but the machine has no
real-time margin. Storage latency, FDC event scheduling, and contention with
display/audio DMA could worsen both game speed and AY continuity. Host results
cannot settle that question; physical telemetry would be mandatory.

### External storage options

Internal QSPI cannot hold the complete source collection. The board's existing
pin allocation uses GPIO12/13 for audio, 14–19 for buttons, 20/21 for I2C, and
26–39 for the LCD. The project pin map leaves the breadboard GPIO0–5 as the
most plausible SPI microSD attachment. That offers clock, MOSI, MISO and chip
select without sharing the LCD or speaker paths, but voltage, connector,
signal-integrity and DMA-channel ownership must be checked against the PIM720
schematic before any hardware recommendation is finalized. Qw/ST I2C is not a
sensible bulk-game transport.

A modest microSD card easily holds the measured 95.08 MiB source set and its
indexes. The firmware should mount it read-only for the first implementation.
USB-host storage and an additional parallel/QSPI flash device are not present
capabilities and would require more hardware and firmware work.

### DSK decision

**UNPROVEN.** Direct DSK execution is impossible in the current production
profile, but a read-only, streaming CPC464+DDI-1 prototype is technically
plausible. It must pass a bounded resource and timing experiment before it is
selected as the main game format. Copy-protected, nonstandard, 128K-only, and
multi-disk titles remain separate compatibility classes.

## 3. Measured storage options

### Current source and internal-flash measurements

| Measurement | Value |
|---|---:|
| game directories | 1,694 |
| recursive source files | 10,216 |
| total source bytes | 99,698,889 (95.08 MiB) |
| compressed DSK members | 101 |
| compressed DSK bytes | 6,528,947 |
| decoded DSK bytes | 19,308,032 |
| decoded DSK average | 191,169 bytes |
| fixed SNA payload | 65,792 bytes |
| current `.cpcgame` | 65,888 bytes |
| aligned flash allocation | 69,632 bytes |
| internal game-data region | 12,484,608 bytes |
| CPCLIB1 descriptor limit | 192 |
| absolute allocation ceiling | 179 |
| conservative remaining allocations | 176 |

### A. Existing snapshot packages

If every directory produced one current package:

| Representation | 1,694-title total | Fits internal game region? |
|---|---:|---|
| raw 65,792-byte SNA | 111,451,648 bytes | No; 8.93× region |
| 65,888-byte package | 111,614,272 bytes | No; 8.94× region |
| 17-sector flash allocation | 117,956,608 bytes | No; 9.45× region |

This also exceeds the 192-entry catalogue before storage is considered.

### B. Compressed snapshots

Four real CPC Explorer snapshots were compressed with zlib level 9 as an
architecture sample:

| Snapshot | Raw | Compressed | Ratio |
|---|---:|---:|---:|
| Knight Tyme | 65,792 | 26,988 | 41.0% |
| Manic Miner | 65,792 | 23,560 | 35.8% |
| Dizzy | 65,792 | 35,099 | 53.3% |
| Chuckie Egg | 65,792 | 10,312 | 15.7% |

The four-title mean is 23,989.75 bytes. Extrapolated only as a capacity
illustration, 1,694 such images would need 40,638,636 bytes before indexes and
alignment—still 3.25× the internal game region. The sample is far too small to
claim a collection-wide ratio.

Compressed snapshots are nevertheless useful on removable storage. A streaming
decoder can write directly into the already-required 64 KiB CPC RAM, using a
small bounded input window rather than a second 64 KiB buffer. Decompression
cost is a one-time launch cost and must be measured on RP2350.

### C. Original/normalised DSK images

The 101 actual DSKs occupy 6,528,947 bytes compressed and 19,308,032 bytes
decoded. Their average decoded size is about 191 KiB, but extrapolating that
average to all 1,694 directories would be misleading: most directories contain
loose AMSDOS-era files, not DSKs.

The only defensible all-source figure is the measured 99,698,889 bytes. A
normalised library will be somewhat larger if it retains both original files
and generated DSK/SNA derivatives. It should be sized from the completed
manifest, not an assumed per-game disk size.

### D. Additional storage

Internal flash can remain the protected firmware/recovery and small validated
snapshot store. A read-only microSD is the most credible bulk medium. Reserve
at least 256 MiB for the present source plus generated artifacts; 1 GiB or more
provides practical headroom and is easier to source. This is a capacity
recommendation, not approval to modify the hardware.

## 4. One-time `ExplorerReady` preparation

The original `D:` trees remain immutable. The host preparation tool writes a
new, git-ignored staging tree:

```text
ExplorerReady/
  library.cpxidx
  manifest.jsonl
  Arcade/
    A/
      <stable-id>/
        source.json
        media.dsk.gz | game.sna.z
  ...
  Simulation/
    A/
    ...
```

The A–Z bucket is derived from a normalized display title. Digits and symbols
go in `_` so no title is lost. Stable IDs should be a normalized category/title
slug plus a short source-manifest hash suffix when variants collide. IDs must
not be derived from mutable directory order.

Each `source.json`/index record contains:

- schema version, stable ID and display title;
- Arcade/Simulation and A–Z bucket;
- original relative path and SHA-256 manifest hash;
- original format and normalized media hash;
- machine model, RAM and CRTC requirements, with `unknown` allowed;
- startup command and later-media dependency classification;
- controls and control-profile compatibility;
- AY classification: music, effects, silent, or unverified;
- conversion tool/version/command and deterministic output hashes;
- status: inventory-only, conversion-ready, snapshot-validated,
  direct-DSK-candidate, unsupported, or rejected with reason.

The process must hash before reading, parse archives in memory or in isolated
scratch space, deduplicate only byte-identical media, retain alternative
versions, validate generated media, and never mutate the source tree. Game
assets remain out of Git.

This stage deliberately does not convert 1,694 titles. The next proof set
should contain Classic Invaders plus a handful of structurally different
titles: a loose BASIC/BIN/SCR loader, a simple DSK, a title with later disk
access, a multi-disk title, and an explicit 128K rejection.

## 5. Paged Arcade/Simulation/A–Z menu

Loading 1,694 full records would violate the SRAM objective. At only 80 bytes
per record, a flat table already consumes 135,520 bytes—more than the entire
116-KiB required safety margin.

Use a fixed, checksummed, read-only index with:

- a 64-byte header containing format version, record count and CRC;
- two category records;
- 52 bucket records (category × A–Z), each containing count and file offset;
- title records sorted by normalized title, stored in independent pages;
- an optional compact search prefix index later, not in the first version.

The launcher keeps only the current navigation stack and one page of about
8–12 entries in RAM. A practical cache budget is approximately 2 KiB:

| Runtime menu state | Estimate |
|---|---:|
| 52 bucket offsets/counts | 416–624 bytes |
| one 8-entry title page | 640–1,024 bytes |
| navigation/render scratch | 256–512 bytes |
| total | approximately 1.3–2.1 KiB |

The current four games remain a separate built-in/internal source. The menu
merges them at presentation time and does not rewrite CPCLIB1. A corrupt or
missing removable index must fall back to Locomotive BASIC, Knight Tyme, Manic
Miner and Dizzy. Records and media are opened only after selection; no full
catalogue or DSK is copied into main SRAM.

## 6. Compatibility, audio and protection rules

- Snapshot remains the only proven CPC Explorer game format.
- CPC464/64K must be established from machine metadata or a proven native
  boot, never inferred from the absence of “128K” in a title.
- A snapshot is accepted only after reload, gameplay, controls, framebuffer,
  AY and media-independence checks.
- Direct DSK candidates retain authentic AY writes; the loader must not add a
  soundtrack. Silent games stay silent.
- A disk title that reads media after startup cannot be converted to a
  standalone snapshot unless the captured state is genuinely self-contained.
- 128K, CPC6128, Plus, unsupported CRTC/peripheral and multi-disk requirements
  are explicit metadata, not silently downgraded.
- External media is never allowed to target the first 4 MiB firmware region,
  CPCLIB1 catalogues, or the final 64 KiB E10 guard.
- Removable media failure is fail-closed and cannot prevent the four internal
  entries from launching.

## 7. Verification and preservation

The established `build-host-stage4h5-realtime-ay` suite was rerun after this
assessment: **20/20 passed** in 13.87 seconds. It covers CPC464 ROM/BASIC,
Knight Tyme, launcher and library logic, transactional install/removal,
importer and UF2 policy, Manic Miner, controls, performance metrics, display,
AY/audio, the continuous renderer, Dizzy and fast-engine integration. This
architecture stage changes documentation only and creates no new firmware
regression claim.

Preservation recheck:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Stage 4H.4 `cpc_explorer_ay_audio_continuous.uf2` | 690,688 | `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0` |
| Stage 4H.5D `cpc_explorer_dual_core_ay_corrected.uf2` | 710,656 | `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35` |

The existing dirty worktree and user source changes were left untouched.

## 8. Recommended implementation sequence

1. **Approve one isolated konCePCja conversion experiment.** Pin and hash the
   Windows release; keep IPC localhost-only; prove Classic Invaders prompt
   holds, gameplay, controls, AY, later-media behavior and deterministic SNA.
2. **Build the host-only ExplorerReady manifest/index generator.** Run it on a
   5–10-title proof set and validate deterministic output, duplicate handling,
   bad media, long titles and A–Z paging. Do not copy assets into Git.
3. **Prototype compressed snapshot loading on the host.** Stream into CPC RAM,
   validate CRC before launch, and measure peak RAM and decode time.
4. **Prototype the paged menu on the host.** Enforce a 2-KiB cache target and
   fallback behavior with missing/corrupt removable media.
5. **Make a separate direct-DSK feasibility branch.** Add CPC464 DDI-1/AMSDOS,
   a streaming read-only sector backend and FDC dispatch to both engines.
   Linker-gate it before any RP2350 build.
6. **Differentially test five disk behaviors.** Boot-only, later-read,
   multi-disk, nonstandard/protected and 128K-rejection cases; include AY and
   controls.
7. **Only then select hardware storage and build a diagnostic firmware.** A/B
   CPU, SRAM, storage latency, AY continuity and recovery. Preserve the
   existing internal library and all flash protection.
8. **Scale conversion in batches only after acceptance.** Compatibility is a
   per-title result; inventory count is never treated as a success count.

## Final status

- **Classic Invaders automation:** **HOLD** — credible deterministic IPC route,
  but no approved local konCePCja installation was available to run the proof.
- **Direct DSK execution:** **UNPROVEN** — absent from the current firmware;
  a streaming read-only prototype is plausible but not resource/timing proven.
- **All-snapshot internal library:** **NOT FEASIBLE** for 1,694 titles.
- **Best bulk architecture:** existing protected internal library plus a
  read-only, externally stored, paged A–Z index; compressed validated snapshots
  first, direct DSK only after its own acceptance gates.
- **Physical/device action:** none.
