# Stage 4L — Fly Spy installation and gameplay acceptance

Date: 31 October 2026

## Result

| Gate | Result |
|---|---|
| Source inspection | PASS |
| CPC464/64 KiB host load | PASS |
| Actual gameplay reached | PASS |
| Fresh snapshot reload | PASS |
| Twenty-second diskless continuation | PASS |
| CPC Explorer core reload | PASS |
| Deterministic package | PASS |
| Transactional physical upload | PASS — one attempt |
| Post-commit catalogue audit | PASS |
| Owner gameplay and CPCPERF acceptance | PASS — active gameplay captured for 64.8 seconds |

No firmware, migration, removal or erase operation was performed.

## Source

The read-only source directory contains one tokenised AMSDOS BASIC loader and
four AMSDOS binary files. The original files were not changed.

| File | Bytes | AMSDOS metadata | SHA-256 |
|---|---:|---|---|
| `FLYSPY.BAS` | 384 | BASIC, 89 logical bytes | `4e3c980503d5564def196741ee5ef450bfc31b19e98985cafc54998fce1e9097` |
| `FLYSPY2.BIN` | 16,640 | load `0xC000`, 16,383 bytes | `18194804ca2120824f73b1c007df43997b4e3d222d3993d113d9cab16fa2cb49` |
| `FLYSPY3.BIN` | 40,448 | load `0x0310`, 40,179 bytes | `e593ed54d4bf707db72ed10ee5c33e445eede1a7ba9c5fe04a725c172aaa8dbf` |
| `FLYSPY4.BIN` | 7,552 | load `0xC000`, 7,264 bytes | `deaeb8109dd49caf22bfcb2aae423fbed8b61954f97de570beab0e51d50b2801` |
| `FLYSPY5.BIN` | 512 | load/execute `0xBE80`, 144 bytes | `25f74af25aee679602fb1af8239b22e73621af6a87de2a7611ab6f35e91a71ea` |

The authentic entry command is `RUN"FLYSPY"`. The BASIC bootstrap loads and
executes `FLYSPY5.BIN`; that bootstrap performs the remaining game load. A
scratch AMSDOS DSK built from byte-identical copies has SHA-256
`6a335c5ddf3908be35e4a0e2cc3d9ec5545d17bfd6b2196b25bdbcc437278db8`.

## Host evidence

The CPC464/64 KiB emulator reached the live game map after the high-score
screen. The capture contains the player craft, fuel object, lift, inventory,
score, shield/fuel bars and level geometry. Space and joystick input advance
from the initial state; all four joystick directions and fire produce distinct
RAM/framebuffer results.

The captured state continued for twenty emulated seconds after drive A was
ejected. It then reloaded in a new emulator process and in CPC Explorer's own
emulator core. The resulting standalone snapshot is 65,792 bytes, SNA v3,
CPC464, 64 KiB:

- Snapshot SHA-256: `6e3d520fb924ff20f5573ef4be42d4f55263cb262301cac62f59f34cf63f03f3`
- Game ID: `fly_spy`
- Title: `Fly Spy`
- Control profile: `joystick-fire`
- Package bytes: 65,888
- Payload CRC32: `51aed13c`
- Package CRC32: `5b119a16`
- Package SHA-256: `aff7f8ebfa14622110707d9a6e7c593de801e7d518f9dbda7218f0731cd0be25`

Rebuilding the package produced the same SHA-256.

## Controls

The original keyboard controls respond as follows:

| CPC input | Action |
|---|---|
| Q | Up |
| A | Down |
| O | Left |
| P | Right |
| Space | Fire/action |
| Joystick 1 directions | Movement |
| Joystick 1 fire | Fire/action |

The packaged snapshot uses the generic joystick profile. Explorer mappings are:

| Controller | Up | Down | Left | Right | Fire |
|---|---|---|---|---|---|
| Qw/ST Pad | D-pad Up | D-pad Down | D-pad Left | D-pad Right | X or A |
| Onboard buttons | A | B | X | Z | Y |

The snapshot starts in gameplay, so no separate start-screen selection is
required after launch.

## Video and audio

The title/high-score display uses CPC mode 1. Gameplay switches to mode 0 and
uses the normal CPC active display; the existing proportional presentation path
remains unchanged.

The 60-second YM5 trace contains 3,000 frames at 50 Hz. All three AY tone
channels are enabled, volume is active in every captured frame, and tone-period
changes occur throughout the trace. This is sustained game-generated AY
activity, not an injected soundtrack.

## Physical installation

Preflight verified the expected application USB identity and serial, firmware
`0.4.28-stage4k7migrationrecoverfix`, Winbond `EF-40-18` 16 MiB flash,
migration `active`, upload `idle`, 32 visible games and a valid generation-34
catalogue. `fly_spy` was absent and the hidden `4kroid` record was present.

One upload command was issued. The device returned:

```text
CPCU1 COMMITTED crc=51aed13c entries=33
upload committed
```

Post-install verification found:

- 33 visible games;
- `fly_spy` visible exactly once;
- 30 committed CPCLIB1 records;
- authoritative generation 35, high-water `0x243000`;
- fallback generation 34 preserved;
- hidden `4kroid` record preserved;
- migration `active`, upload `idle`, no error;
- next physical allocation `0x643000`;
- 10,145,792 bytes remain before the E10 guard.

## Owner acceptance procedure

1. Open **Arcade**, then **F**, then launch **Fly Spy**.
2. Confirm the live map and HUD appear rather than a loader or title screen.
3. Use the Qw/ST D-pad to move in all four directions.
4. Use Qw/ST X or A to fire/act.
5. Confirm movement, collisions, object interaction, scrolling and AY sound.
6. Leave the game running for at least one minute and note any corruption,
   freeze, reset, stuck key or audio dropout.

For a physical performance sample, open the USB serial port at 115200 baud while
gameplay is active and capture at least ten `CPCPERF` and `CPCAUDIO` intervals.
Relevant fields are `speed_pct_x10`, `exec_avg`, `work_avg`, `loop_avg`,
`missed`, `fps_x10`, audio queue depth, deadline misses, underruns/overruns and
core-1 stack high-water. Existing firmware measurements are workload-dependent
(roughly 69–82.5% CPC speed); Fly Spy has no physical CPCPERF result yet.

All snapshots, packages, DSKs, screenshots and traces are stored below the
ignored `artifacts/stage4l-fly-spy/` directory. Copyrighted game data must not be
committed or published.
