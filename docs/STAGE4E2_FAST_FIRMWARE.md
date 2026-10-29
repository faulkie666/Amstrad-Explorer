# Stage 4E.2 fast-engine firmware build and preflight

## Status

**PASS — ready for separately approved physical testing.** No device, COM
port, BOOTSEL volume, removable drive, catalog, upload transaction, or physical
flash was accessed.

The Stage 4E.1 source fingerprint was verified before the Stage 4E.2 changes as
`cde45a5c07a8012d0b3b61d488c0f27385fa2fc1330c52548214bd547dc3e782`.
The current physical rollback UF2 remains
`717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`.

## Candidate identity

- UF2: `build-rp2350-stage4e2-fast/cpc_explorer_fast.uf2`
- SHA-256: `4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`
- UF2 container: 683,008 bytes
- Raw firmware binary: 341,120 bytes
- Protocol identity: `4E.2-FAST`
- SDK binary version: `0.4.6-stage4e2fast`
- Isolated configuration: Release, `LIBRARY_FAST`, fast engine enabled and
  selected, presentation divisor 4

The ARM disassembly begins the emulator path by loading `EngineKind::Fast` and
calling `cpcexplorer::select_engine`. The linked ELF also contains the
`4E.2-FAST`, `0.4.6-stage4e2fast`, and CPCPERF strings. CMake rejects a
`LIBRARY_FAST` build unless the fast engine is compiled and explicitly selected.
The normal cache default remains `REFERENCE`, and the reference engine is still
linked and host-tested.

## ARM linker and memory results

The final ELF reports 345,216 text-classified bytes and 313,404 BSS-classified
bytes under Berkeley `size`. The more useful section breakdown is:

| Item | Bytes | Placement |
|---|---:|---|
| `.text` executable code | 218,200 | XIP flash from `0x10000000` |
| `.rodata` | 110,804 | XIP flash |
| initialized `.data` | 11,224 | stored in flash, copied to SRAM |
| raw binary span | 341,120 | `0x10000000..0x1005347F` |
| RAM vector table | 272 | SRAM |
| `.data` | 11,224 | SRAM |
| `.bss` | 313,132 | SRAM |
| minimum heap reservation | 2,048 | main SRAM |
| core-0 stack reservation | 2,048 | scratch-Y bank |
| total statically reserved SRAM | 328,724 | 61.7% of 532,480 bytes |
| remaining aggregate SRAM | 203,756 | 38.3% |

The main-SRAM gap after the minimum heap reservation is 197,588 bytes. Newlib
may grow the heap into this gap; the fast core's two allocations total only 496
bytes. Core 1 is unused and has no linked stack allocation. The main/emulator
stack frame is 612 bytes plus saved registers; the 640-byte telemetry buffer was
placed in BSS instead of the 2 KiB stack. Stack high-water usage is not measured
until a physical diagnostic run.

The shared `cpc_t` machine is at `0x20002D00`, size 281,792 bytes. It owns the
64 KiB CPC RAM, 208,896-byte indexed framebuffer, and CPC peripheral state, so
there is no second large machine allocation. The two RGB565 scanlines occupy
1,280 bytes in SRAM. Fast runtime metadata occupies 104 initialized SRAM bytes.
The fast core's 4,096-byte DAA table and three 256-byte flag/parity tables are in
SRAM; its 256-byte indirect table is in XIP flash.

The selected Z80 instruction engine and adapter execute from XIP flash:
`z80_execute_step` is at `0x10016324`, `fast_z80_step` at `0x10016104`, and the
CPC fast callbacks are also in `0x100...`. The Stage 4C.3 reference hot-code
SRAM relocation is deliberately not enabled for this target: copying the large
new instruction dispatcher into SRAM would consume headroom without physical
evidence of a benefit. The reference `z80_tick`, `cpc_exec`, and
`am40010_tick` remain linked as the compatibility path.

## UF2 and protected flash

The independent validator accepted all 1,334 physical blocks:

- 1,333 ordinary RP2350 application blocks, family `0xE48BFF59`;
- targets `0x10000000..0x100534FF`, wholly inside the first 4 MiB allocation;
- no catalog target (`0x10400000..0x10407FFF`);
- no game-storage target (`0x10408000..0x10FEFFFF`);
- precisely one E10 compatibility block: family `0xE48BFF57`, flags
  `0x0000A000`, target `0x10FFFF00`, extension `0x9957E304`, and 256 bytes of
  `0xEF` payload;
- no other absolute-family or out-of-reservation target.

The post-link raw-binary guard also passed with 3,853,184 bytes of margin in the
4 MiB firmware reservation.

## Diagnostics and presentation

The bounded CPCPERF line now reports emulated microseconds and wall interval,
effective speed, actual presented-frame count/FPS, missed deadlines, and average
and maximum costs for instruction execution, peripheral scheduling,
reconstruction, RGB565 conversion, LCD wait/transfer, total rendering, work,
and loop duration. FPS uses `presented_frames`, never emulated slice count.

Fine timing is enabled only while USB CDC reports an attached/open diagnostic
host. Normal gameplay performs no per-event timer reads and emits no periodic
telemetry. Output remains one bounded 640-byte buffer approximately once per
second, with the established 1 ms USB timeout.

The candidate uses the known 12.5 Hz mode (divisor 4). An isolated divisor-2
host build passed the integration and telemetry tests, reconstructed exactly 25
frames per emulated second, and kept emulated time at 4 MHz. It costs additional
reconstruction work, so 25 Hz is supported as an explicit comparison build but
is not forced into this first hardware candidate. LCD DMA ownership remains the
existing synchronous implementation.

## Verification

The complete Release host suite passed 11/11: CPC464 ROM boot, BASIC keyboard
and `PRINT 2+2`, Knight Tyme load/menu/Space, launcher/debounce and launch-button
suppression, CPCLIB parsing, upload/removal and transactional recovery, importer,
UF2/E10 policy, native Manic Miner, Q/W/bare-Shift hold/release, performance
metrics, repeated launches, and the integrated fast engine. The 25 Hz focused
build also passed.

All nine recorded production/rollback UF2 hashes rechecked exactly: BASIC,
Knight Tyme, Stage 3, Stage 4A, Stage 4A E10-safe, Stage 4B, Stage 4B.1, Stage
4C.1B, and Stage 4C.3-SRAM. The CPC464 ROM remains
`00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e`;
Knight Tyme SNA remains
`ca5fd1c8fe5b72694038fa6263349958d0537fe04c5605e49e489cba13f65156`;
the native Manic Miner SNA remains
`6661c74f919e228b2d21e1c0fceba7369c80281c1e27c53d03d624b1ff6d743d`;
and its CPCGAME remains
`30ad9705c3532920d1652dd9b9188b8a53deed7fb2c03d8f1756c9daffcdcf1d`.
The MicroPython backup independently rehashed 21/21 files successfully.

The selected third-party Z80 revision remains pinned to
`687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`; its MIT notice remains at
`third_party/licenses/cpu-z80-MIT.txt`. No GPL/LGPL component was added.

## Remaining limitations

- No RP2350 speed, FPS, display quality, stack high-water, or gameplay result is
  claimed from this host/build preflight. Those require a separately approved
  physical A/B diagnostic run.
- The fast engine remains limited to compatible CPC464 64 KiB snapshots and
  does not promise exact WAIT/contention, raster tricks, or AY waveform timing.
- Fast-mode snapshot creation remains disabled; snapshot loading is tested.
- Knight Tyme's three-button physical control limitation remains unchanged.
- The first candidate retains 12.5 Hz presentation; 25 Hz needs physical CPU
  and LCD-overhead evidence before adoption.
