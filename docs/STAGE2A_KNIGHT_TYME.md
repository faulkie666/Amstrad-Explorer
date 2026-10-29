# Stage 2A: Knight Tyme

## Verified local inputs

The supplied files remain outside this repository. Their physical-file hashes
and AMSDOS metadata were verified before execution:

| File | Physical bytes | SHA-256 | Load | Logical bytes | Execute |
| --- | ---: | --- | ---: | ---: | ---: |
| `KNIGHT` | 896 | `943d5f6df52806038275a8316657c326c253b2f2bbe20ddb41f00323489ce3c4` | `0170` | 696 | `0000` |
| `KT$.BIN` | 8,320 | `78e2bafba60c884d7d3933242710aa849c0908b570a4bc46c145f292b3f6cbb0` | `1388` | 8,192 | `1388` |
| `KTCODE.BIN` | 30,208 | `e0bcf8f05c5085074083b876902d9f6f49d990fc93fa2300c418470eb3366811` | `1388` | 30,000 | `1388` |
| `KTGRX.BIN` | 11,264 | `b19dc32d12dde3d22bcf0040d7beffe8ce4bff8a75af7072906f44a2c7137023` | `C000` | 11,110 | `0000` |

Each stored AMSDOS checksum equals the sum of header bytes 0 through 66. The
128-byte header is never copied into emulated RAM. Bytes after the header's
logical length are record padding and are also not loaded.

An ignored DATA-format DSK was reconstructed with iDSK 0.20. Exporting all four
files again proved that every AMSDOS header and every logical payload byte
round-trips unchanged. The generated DSK and tool checkout stay under ignored
build/research directories.

## Host execution path

The compact CPC464 core deliberately has no AMSDOS expansion ROM. Rather than
add disk state to the RP2350 build, the host test reproduces the loader's
effective sequence through its AMSDOS-aware quickload boundary:

1. Boot the original CPC464 OS and BASIC ROMs for five emulated seconds.
2. Validate and load `KT$.BIN` at `&1388`, then have BASIC execute `CALL &1388`.
3. Observe the illustrated title, press Space, and wait for KT$ to return.
4. Load `KTCODE.BIN` at `&1388`, intentionally replacing KT$.
5. Load `KTGRX.BIN` at `&C000`; make no cheat patches.
6. Have BASIC execute the final `CALL &1388`.

The result is real Z80 execution. The title raster hashes to
`97e9365cf3773e9b`; the instruction/play menu hashes to
`39a379fedd1707e3`. At capture the machine had executed 108,200,000 Z80 ticks
and the PC was in game code.

## Snapshot and startup modes

The host test writes a standard 256-byte CPC SNA header plus exactly 64 KiB of
RAM. Saving is aligned to an instruction boundary. Loading restores Z80
registers, interrupt state, RAM, ROM mapping, Gate Array palette/configuration,
CRTC registers, PPI and PSG registers, then restarts instruction prefetch.

A cold reset and SNA reload produced the identical game raster hash while the
PC continued to advance. The local SNA is ignored by Git and is embedded as a
flash/XIP `const` array only for the Knight Tyme firmware build. It is copied
directly into the emulator's existing 64 KiB RAM, so there is no second RAM
buffer.

Build selection:

```powershell
# Existing BASIC mode (default)
-DCPCEXPLORER_STARTUP=BASIC

# Local Knight Tyme snapshot mode
-DCPCEXPLORER_STARTUP=KNIGHT_TYME
```

## Input evidence

The game's own instruction text in `KTCODE.BIN` specifies:

- `N`: left
- `M`: right
- `A`: jump/up
- `Z`: down
- Space or joystick fire: menu/action
- CPC joystick directions are also supported

This is the minimal mapping required for the future I2C controller bridge.

## Performance and memory

The host executes approximately 115.3 million emulated Z80 ticks per wall
second on this machine (about 28.8 times the CPC's 4 MHz clock). Converting the
current 320x200 crop to RGB565 takes about 36.9 microseconds per frame on the
host.

The firmware currently updates at 50 Hz and transfers every pixel: 64,000
pixels or 128,000 bytes per update. The configured 16 MHz parallel write
strobe gives an 8 ms transfer floor, before conversion and 200 scanline DMA
submissions. Dirty/changed scanlines are therefore the next measured
optimization candidate; timing and clock settings were not changed in Stage
2A.

The final ELF allocates 272 bytes of RAM vectors, 8,840 bytes of initialized
data, 288,636 bytes of BSS, a 2,048-byte heap and a 2,048-byte stack reservation.
Against 532,480 available bytes, that leaves 230,636 bytes unallocated by the
link image.

