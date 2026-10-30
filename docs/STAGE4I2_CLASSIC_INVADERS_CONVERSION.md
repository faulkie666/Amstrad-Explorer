# Stage 4I.2 — Classic Invaders conversion

## Result

**Installation gate: HOLD.** Classic Invaders reaches its authentic loader on a
64 KiB CPC464, but a gameplay-active state was not captured and therefore no
SNA or CPCGAME package has been produced. The structural/import checks have not
been relaxed.

No serial port, BOOTSEL volume, device flash, installed catalogue, firmware
source, UF2, Z80 implementation, or source game file was accessed or changed.

## Source identity

The requested source DSK does not exist in the local Classic Invaders directory.
The actual source is a three-file AMSDOS set:

| File | Bytes | SHA-256 | AMSDOS logical data |
|---|---:|---|---|
| `D:\Games\CPC Games\Arcade\Classic Invaders\INVADERS` | 512 | `4c1d92bf3a0e64e905633c4a75c8308fa73d782924983379529150f447894631` | binary, load/execute `0x1470`, 286 bytes |
| `D:\Games\CPC Games\Arcade\Classic Invaders\INVADERS.BIN` | 17,152 | `c68d35fc7bf597ae65d8c3b83eb0993317a1a1758e604288fe89738cf9258b31` | binary, load `0x1000`, 17,008 bytes |
| `D:\Games\CPC Games\Arcade\Classic Invaders\INVADERS.SCR` | 5,376 | `6d1af5aad5b19d0f2d044279ddceb524e41601ff7d3c18c9634c118e461fa1ba` | binary, load `0x1000`, 5,168 bytes |

All work used copies under
`artifacts/stage4i2-classic-invaders`. The source files on `D:` were read only.

A standard data-format scratch disk was made solely to exercise the original
AMSDOS loader:

`artifacts/stage4i2-classic-invaders/classic-invaders-derived.dsk`

- size: 204,544 bytes
- SHA-256: `d96d7d99a206f99d29a7a35ac90ed3fb6643d7c458f5685e3c58b1201bfd1ed3`
- directory: `INVADERS`, `INVADERS.BIN`, `INVADERS.SCR`
- round-trip check: each 128-byte AMSDOS header and every logical data byte
  matched its source copy; only unused sector padding differs

This is a derived transport image, not an original game DSK, and is not an
installable artifact.

## Loader investigation

The 286-byte `INVADERS` program is a machine-code loader, not BASIC. Its entry
point at `0x1470`:

1. initializes the CPC firmware display, Mode 1, border and inks;
2. opens `INVADERS.SCR` through firmware cassette/AMSDOS calls and loads it to
   `0xC000`;
3. asks `Infinite lives (Y/N)?`;
4. asks `Start on level (1-8)?`;
5. loads `INVADERS.BIN` through the same firmware path;
6. optionally patches `0x5084` with `RET` for infinite lives;
7. writes the selected zero-based level to `0x4C7C`; and
8. jumps to the game at `0x4200`.

This proves that all three files and the loader choices matter. Treating
`INVADERS.BIN` as a self-starting binary would bypass authentic initialization
and is deliberately rejected. The existing generic `amsdos-basic-bin` builder
also rejects this title correctly because the loader is not BASIC and an
additional screen file participates in startup.

## Emulator and state-capture attempt

The scratch disk was mounted in a local CPCE 1.90 sandbox configured for the
64K CPC464 ROM with AMSDOS as an expansion ROM. `RUN"INVADERS"` executed the
real loader and displayed the source `INVADERS.SCR` artwork plus the exact
`Infinite lives (Y/N)?` prompt. This is verified boot/loader evidence, but it is
not gameplay evidence.

The automated Windows input facility generates short host key transitions.
CPCE polls host keyboard state and repeatedly missed those transitions at the
loader prompt. Repeated input and Windows on-screen-keyboard input did not
produce a reproducible transition through both prompts. Bypassing the prompt,
injecting the main binary, patching the loader, or jumping directly to `0x4200`
would violate the genuine-boot gate, so the experiment stopped there.

Consequently:

- no gameplay-active checkpoint was reached;
- no complete CPU/RAM/peripheral state was captured;
- no post-capture media-independence trace exists;
- controls and AY activity remain unverified; and
- no snapshot or game package was created.

The smallest clean next step is a host input backend that can hold and release
a key for at least one CPC scan interval, or a deterministic emulator automation
API exposing CPC keyboard-matrix state. It must drive the unmodified loader,
then save a 64K SNA at an instruction boundary after the jump to `0x4200`.

## Snapshot requirements checked

CPC Explorer currently accepts only exact 65,792-byte SNA files with the
`MV - SNA` signature and a 64 KiB RAM dump. Versions 1–3 are recognized. For
v2/v3, machine identity byte `0x6D` must be zero (CPC464); 664, 6128, Plus and
unknown identities fail closed. A v1 file has no model identity and therefore
needs independent CPC464 provenance.

The production loader restores CPU registers, PC/SP, I/R, IX/IY, interrupt
mode and IFF state; Gate Array inks/configuration/RAM configuration; selected
CRTC register and CRTC registers; ROM/PPI state; selected PSG register and AY
registers; and the full 64 KiB RAM image. The CPC Explorer core has CPC464 ROM
and RAM semantics but no production disk/tape service. Therefore the capture
must occur after all required media reads, and a substantial disk-detached host
trace is mandatory.

## Controls and AY audio

- **Controls:** UNKNOWN. No gameplay state was reached, so no key or joystick
  mapping is claimed.
- **AY classification:** UNKNOWN. Loader execution alone is not evidence of
  game sound, effects, or music.

## Package status

No `classic_invaders.sna` and no `classic_invaders.cpcgame` exist. In
particular, there is no package path or hash to install. Once a verified SNA is
available, the supported packaging command is expected to be:

```powershell
py tools/cpc_game.py import-sna <verified-65792-byte-sna> `
  --id classic_invaders --title "Classic Invaders" `
  --output artifacts/stage4i2-classic-invaders/classic_invaders.cpcgame
py tools/cpc_game.py inspect artifacts/stage4i2-classic-invaders/classic_invaders.cpcgame
```

That command is intentionally not run while the snapshot gate is unmet.

## Follow-on conversion assessment

The prompt listed eight follow-on titles (despite saying nine). None is marked
ready until the genuine-boot/capture mechanism is completed.

| Game | Local material | Classification | Reason |
|---|---|---|---|
| Fruity Frank | `FRUITY`, `FRUITY.BIN` | NEEDS INVESTIGATION | likely loader + binary; must inspect authentic sequence |
| Thrust | `THRUST`, `THRUST.BIN` | NEEDS INVESTIGATION | two-file loader; no snapshot present |
| Motos | `MOTOS`, `MOTOS.BIN`, `MOTOSIMA.POW` | NEEDS INVESTIGATION | extra asset participates in loading |
| Bomb Jack | `BOMBJACK`, `BOMBJACK.BIN` | NEEDS INVESTIGATION | machine/BASIC loader type and controls unverified |
| Columns | BASIC plus five BIN assets | NEEDS INVESTIGATION | multi-file loader; later media dependence must be tested |
| Fighter Pilot | `PILOT.BAS`, `PILOT.BIN` | NEEDS INVESTIGATION | promising BASIC/BIN pair, but capture and controls are unverified |
| Top Gun | `TOPGUN`, `TOPGUN.BIN`, `TOPGNIMA.POW` | NEEDS INVESTIGATION | extra asset and loader choices require tracing |
| Elite | compressed DSK plus many staged components | NEEDS INVESTIGATION | complex multi-file title; later disk access is a material risk |

## Host verification

The current host suite was executed from
`build-host-stage4h5-realtime-ay`: **20/20 passed**. It covers CPC464 ROM boot,
Knight Tyme, launcher, library, transactional installer/removal, importer, UF2
policy, Manic Miner, gameplay input, metrics, display fit, AY/audio policies,
realtime AY traces, Dizzy audio and fast-engine integration.

Preservation hashes were rechecked:

- Stage 4H.4: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Stage 4H.5D: `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`

The existing dirty source tree was preserved. Only this report and scratch
conversion copies were added by Stage 4I.2.

## Decision

Classic Invaders is **HOLD**, not unsupported. The authentic CPC464 loader and
all required source components are understood, so a valid conversion remains
plausible. The missing evidence is a reproducible held-key boot through the
loader, gameplay-active capture, disk-detached execution trace, verified
controls/AY behavior, and successful reload through CPC Explorer's core.
