# Stage 3: native game-selection menu

Stage 3 adds a native C++ launcher that appears before the emulator is
initialized. It uses the existing ST7789 PIO/DMA driver and the same two
640-byte RGB565 scanline buffers used by the emulator renderer. No additional
framebuffer or graphical dependency is present.

## Menu registry and startup

The static registry in `src/launcher.cpp` contains, in order:

1. `Locomotive BASIC` (`EntryType::Basic`)
2. `Knight Tyme` (`EntryType::Snapshot`, identifier `knight_tyme`)

The registry exposes a visible-window calculation so a longer future list can
scroll without changing the input controller. The emulator is not initialized
until Y launches the selected entry. BASIC performs a clean CPC464
initialization; Knight Tyme performs the same initialization and then loads the
existing ignored 64 KiB SNA. A launched emulator never returns to the menu.

## Explorer controls

The verified Explorer MicroPython backup defines these switches as inputs with
pull-ups and considers a zero reading pressed. The C++ board definition now
records the same active-low wiring:

| Button | GPIO | Menu action |
| --- | ---: | --- |
| X | 17 | Move up |
| Y | 18 | Launch selection |
| Z | 19 | Move down |

Each switch has an independent 30 ms debounce state. A press emits one event;
holding cannot repeat or relaunch. A button held while the device resets is
suppressed until it has been released and pressed again. Selection wraps at
both ends.

## Rendering

The menu is drawn in CPC blue with yellow selection highlighting and
white/light-blue text. The compact built-in 5x7 font is rendered directly into
the shared scanline buffers. Selection changes redraw only the registry area,
not the complete 320x240 display.

## Build and verification

Configure the dedicated menu build with:

```powershell
cmake -S . -B build-rp2350-menu -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCPCEXPLORER_BUILD_FIRMWARE=ON `
  -DCPCEXPLORER_STARTUP=MENU `
  -DPICO_BOARD=pimoroni_explorer
cmake --build build-rp2350-menu
```

Output copied for approval:

- `build-rp2350/cpc_explorer_menu.uf2`
- 401,408-byte UF2 container
- 200,228-byte flash binary
- SHA-256 `9de537a48d2eab6fcc9ed6b91a063e366c213ff705a18cadea4776182b0a3dbf`

The link image uses 272 bytes of RAM vectors, 8,840 bytes of initialized data,
288,636 bytes of BSS, a 2,048-byte heap and a 2,048-byte stack reservation.
Against the stated 532,480-byte SRAM capacity, 230,636 bytes remain
unallocated. Menu state is included in those figures and adds no large buffer.

Host verification covers default selection, registry order, both wrap
directions, bounce rejection, held-button suppression and one-shot launching.
The original ROM boot raster test and the Knight Tyme title/menu raster test
also pass. Both prior UF2 hashes and the snapshot hash remain unchanged.

