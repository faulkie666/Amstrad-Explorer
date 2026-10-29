# Memory and image report

Measured from the Release RP2350B build on 2026-10-29 with Pico SDK 2.2.0 and
Arm GNU Toolchain 14.3.1.

## Emulator state

| Allocation | Bytes |
| --- | ---: |
| Complete ARM `cpc_t` state | 281,792 |
| Indexed 768x272 framebuffer (inside `cpc_t`) | 208,896 |
| CPC 464 RAM (inside `cpc_t`) | 65,536 |
| Two RGB565 LCD scanlines | 1,280 |
| ROM storage | 32,768 in XIP flash; 0 in SRAM |

The remainder of `cpc_t` contains Z80, Gate Array, CRTC, PPI, AY, keyboard,
memory-map and deliberately inert minimal FDC compatibility state. The 6128
second 64 KiB bank, DSK image storage, AMSDOS ROM, cassette storage and full
RGB framebuffer are absent.

## Linker totals

`arm-none-eabi-size -B build-rp2350/cpc_explorer.elf` reports:

| Segment class | Bytes |
| --- | ---: |
| text/flash-classified | 133,248 |
| BSS | 288,908 |
| Total classified | 422,156 |

The generated binary spans `0x10000000..0x1001f880`, exactly 129,152 bytes in
flash. The UF2 container is 259,072 bytes.

Release candidate hashes:

| Artifact | SHA-256 |
| --- | --- |
| `cpc_explorer.uf2` | `87429b36fdfdb1c1d252be9986b4852c30d6d23243d86592dcb6ee01dbe78a2e` |
| `cpc_explorer.bin` | `e24b86398c9b9c432b9242343ac8739cddc135a1e47a4954848132f0c216b9f7` |

SRAM reservations from section accounting are 301,844 bytes: 272-byte vector
table, 8,840-byte `.data`, 288,636-byte `.bss`, 2,048-byte heap and 2,048-byte
stack. Against the RP2350's 520 KiB aggregate SRAM (512 KiB main plus two 4 KiB
scratch banks), this leaves 230,636 bytes unreserved. Stack demand has not yet
been measured with a hardware high-water mark; the linker reserves 2 KiB and
the main loop uses two static scanlines rather than a frame-sized stack buffer.

Largest BSS symbols:

| Symbol | Bytes |
| --- | ---: |
| emulator machine | 281,792 |
| LCD scanline pair | 1,280 |
| USB endpoint state | 1,024 |
| unmapped memory page | 1,024 |
| write-junk memory page | 1,024 |

No overclock is configured; the SDK/board default 150 MHz system clock is used.

