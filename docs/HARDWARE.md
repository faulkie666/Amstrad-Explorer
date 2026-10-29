# Explorer RP2350B hardware notes

The custom Pico SDK board target selects `rp2350-arm-s`, the RP2350B package,
16 MiB W25Q128-class flash and the standard 150 MHz clock. Pin assignments are
from the PIM720 Explorer reference and the working `RP2350-Quake` port:

| LCD signal | GPIO |
| --- | ---: |
| Backlight | 26 |
| CS | 27 |
| D/C (RS) | 28 |
| WR | 30 |
| RD | 31 |
| DB0..DB7 | 32..39 |

The ST7789-compatible panel uses an 8-bit parallel bus. PIO1 generates a 16 MHz
WR strobe from a 32 MHz state-machine clock, and DMA streams RGB565 bytes. The
driver is derived from `zenodante/RP2350-Quake`, which specifically targets the
Explorer RP2350B.

The Qw/ST Pad is not used in Stage 1B. Its planned Stage 2 connection is I2C0,
SDA GPIO20, SCL GPIO21, address `0x21`.

Reference repositories:

- `https://github.com/pimoroni/explorer`
- `https://github.com/zenodante/RP2350-Quake`
- `https://github.com/pimoroni/pimoroni-pico`

