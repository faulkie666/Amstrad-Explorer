# CPC Explorer

CPC Explorer is a focused Stage 1B Amstrad CPC 464 emulator for the Pimoroni
Explorer RP2350B. It executes the original, unmodified 16 KiB CPC OS and 16 KiB
Locomotive BASIC ROMs through a cycle-stepped Z80 and renders pixels produced by
the emulated Gate Array and MC6845. There is no hard-coded welcome screen.

## Current verification status

- **A — Host-side ROM boot: proven.** The Windows test executes 20,000,000 Z80
  ticks, matches the complete 768x272 startup raster fingerprint
  `de41d6c8715908e3`, injects the exact command `PRINT 2+2` through the emulated
  keyboard, verifies the displayed result `4` with raster fingerprint
  `fd9ba9b5fa500423`, and writes both frames as PPM captures.
- **B — RP2350 firmware compilation: proven.** Pico SDK 2.2.0 and Arm GNU
  14.3.1 produce an RP2350 ARM Secure UF2 for the custom `pimoroni_explorer`
  board target.
- **C — execution on Explorer hardware: not tested.** The connected USB device
  was deliberately not flashed or switched into BOOTSEL mode.

Implemented hardware includes Z80 CPU and 64 KiB RAM, lower/upper ROM overlay,
Gate Array mode/palette/interrupt timing, MC6845 CRTC, 8255 PPI, AY-3-8912
keyboard interface, and the 4 MHz CPC timing model. Floppy, cassette, browser,
gamepad mapping and sound output are outside Stage 1B.

## ROM prerequisite

Place an original English `cpc464.rom` (32,768 bytes: OS followed by BASIC) at
`roms/cpc464.rom`. The build rejects anything except SHA-256:

```
00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e
```

The ROM is intentionally ignored by Git. See [ROM provenance](docs/ROM_PROVENANCE.md).

## Windows host build and test

Run from a Visual Studio Developer PowerShell or Developer Command Prompt:

```powershell
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

The executable writes `cpc464-ready.ppm` and
`cpc464-print-2-plus-2.ppm` in its working directory.

## RP2350 firmware build

Use the supported Pico SDK CMake workflow. Set `PICO_SDK_PATH` to a Pico SDK
2.2.0 checkout with submodules, then run from a Visual Studio developer shell
(the SDK may build its host-side `pioasm`/`picotool` helpers):

```powershell
cmake -S . -B build-rp2350 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCPCEXPLORER_BUILD_FIRMWARE=ON `
  -DPICO_BOARD=pimoroni_explorer
cmake --build build-rp2350
```

Candidate output: `build-rp2350/cpc_explorer.uf2`.

## Display path

The Explorer's ST7789-compatible 8-bit parallel LCD is driven with PIO and DMA.
The 320x200 Mode 1 image is sampled from the genuine 768x272 CPC raster into
RGB565 scanlines and centred vertically on the 320x240 panel. Only two 640-byte
conversion lines are retained; no RGB framebuffer exists in SRAM.

Pin assignments and provenance are in [hardware notes](docs/HARDWARE.md), and
the measured allocation is in [the memory report](docs/MEMORY_REPORT.md).

## First safe hardware test

Do not copy the UF2 yet. The currently connected device reports only generic
Raspberry Pi USB descriptors (`VID 2E8A`, `PID 0005`, `Board CDC`, COM10), so its
identity is not proven.

1. Obtain explicit approval to flash.
2. While the current MicroPython firmware is still running, use Thonny or
   `mpremote` to identify the board and copy every accessible file from `/` to a
   dated backup folder. Hash the backup and keep the existing
   `tufty-v3.1.1-micropython-with-filesystem.uf2` separate; do not assume it is
   an Explorer backup.
3. Disconnect USB. Hold BOOTSEL while reconnecting, then verify Windows shows a
   new `RPI-RP2` volume and inspect its `INFO_UF2.TXT` to confirm an RP2350-family
   device. Stop if the identity does not match the Explorer.
4. Copy only `build-rp2350/cpc_explorer.uf2` to that verified boot volume.
5. After reboot, observe the LCD and COM port at 115200/USB CDC. Expect the
   authentic CPC 464 startup raster centred with 20-pixel black top/bottom
   borders and a `state=...` USB diagnostic. Record a photo and serial log.
6. If the display is blank, disconnect power; do not erase flash or run
   `flash_nuke`. Restore only from the verified backup after reviewing the log.

## Upstream work

The emulator is derived from `floooh/chips` commit
`9e88298ce56319953ac7a43213a1120359f7a3a6` under its zlib/libpng licence.
The Explorer display/board reference is derived from `zenodante/RP2350-Quake`
commit `f054697bebec1c4a886f34e4a48e344111d32ee8`; its GPLv3 licence is preserved
in `licenses/`. Modified upstream files are explicitly marked near the changes.

