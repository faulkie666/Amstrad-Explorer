# Stage 4F.1 fast-Z80 optimisation experiment

## Result

**CPU experiment PASS; presentation acceptance HOLD.** This is not a claim of
80% or 90% CPC speed. No COM port, BOOTSEL volume, removable drive, installed
game, catalog, or physical flash was accessed.

The subsequent display investigation found that this unmodified candidate
inherits the Stage 4E.2 fast-reconstruction centring regression for CRTC modes
narrower than 40 columns. It must not be treated as a corrected presentation
candidate. See `STAGE4F1_DISPLAY_REGRESSION.md`.

The experiment retains the Stage 4E.2 architecture and applies two bounded
changes: direct 4 KiB memory pages derived from the CPC's current memory map,
and SRAM placement of the computed-goto base dispatch tables. The reference
engine remains compiled and all storage, USB, removal, recovery, input, and E10
code paths remain present.

## Actual hot path and ARM placement

The deployed Stage 4E.2 ELF places `z80_execute_step` at `0x10016324`, size
76,212 bytes, in XIP flash. The callback adapter is entered for every Z80 RAM
read and write, even though its final operation is only `mem_rd` or `mem_wr`.
The fast engine's event scheduler and reconstruction are already small compared
with physical Z80 time, agreeing with the measured 28.84 ms Z80 bottleneck.

Enabling the pinned core's direct-page option expands the linked ARM
`z80_execute_step` to 119,316 bytes at `0x100380E0`. It remains in XIP. Its
prologue reserves 44 bytes and saves nine registers, an 80-byte local/saved
register requirement. The adapter and CPC event functions also remain in XIP:
`fast_z80_step` at `0x10015DDC`, `cpc_exec` at `0x10013E88`,
`am40010_tick` at `0x10013418`, and `run_us` at `0x10014BE8`.

A trial relocation of the whole dispatcher was rejected by the linker: the
production image overflowed SRAM by 63,404 bytes. The source section expanded
to approximately 267 KiB in that configuration, so source-level expectations
based on the old 76 KiB symbol were unsafe. The retained candidate relocates
only the two linked 1,024-byte `base_dispatch` tables. The map proves their SRAM
addresses are `0x200011A0` and `0x200015A0`; no instruction body was silently
relocated.

## Direct memory path

The candidate derives each 4 KiB direct read/write page from four consecutive
1 KiB entries in the live Chips `mem_t` page table. A page is enabled only when
all four pointers are non-null and exactly contiguous; otherwise the canonical
callback remains active. Tables are refreshed after snapshot/reset restoration
and every CPC I/O transaction, so gate-array ROM/RAM banking changes cannot
leave stale mappings. The host-only verification mode invokes the canonical
callback alongside each direct operation and compares the results.

Across the BASIC, Knight Tyme, and Manic Miner integration workloads the
verification reports reached 60,000,000 reads and 8,302,680 writes, with
68,302,679 direct-path hits and no mismatch. This specifically exercises ROM
visibility, RAM-behind-ROM writes, snapshot restoration, and title input paths.
The verification mode is rejected by CMake for firmware builds, so it adds no
device cost.

## Host verification

The clean Release host build passed 11/11 tests: CPC464 ROM/BASIC boot and
keyboard input, Knight Tyme regression, launcher/debounce, game library,
transactional install/removal and recovery, importer, UF2/E10 policy, native
Manic Miner, gameplay input, telemetry, and integrated fast engine.

One desktop run reported the following one-second emulation costs:

| Workload | Fast total | CPU component | Reference/fast ratio |
|---|---:|---:|---:|
| BASIC | 3.881 ms | 3.151 ms | 7.200x |
| Knight Tyme | 4.097 ms | 3.436 ms | 6.484x |
| Manic Miner | 4.978 ms | 3.214 ms | 6.123x |

These are desktop regression/dispatch measurements only. Relative to a recent
same-host Stage 4E.2 Manic Miner result (5.711 ms total), the direct-page build
was about 13% lower in that run, but desktop variance and different cache
behaviour make this unsuitable for an RP2350 speed prediction.

## Candidate and memory safety

- UF2: `build-rp2350-stage4f1-fastpath-table/cpc_explorer_fast_z80_opt.uf2`
- SHA-256: `00862f3b55763532596606797ce923711131c41058b62a7ee5b48b0597b72da9`
- distinct identities: `4F.1-FAST-OPT` and `0.4.7-stage4f1fast`
- raw binary: 524,408 bytes
- UF2 container: 1,049,600 bytes
- `.text`: 400,336 bytes in XIP
- `.rodata`: 109,780 bytes in XIP
- `.data`: 13,400 bytes in SRAM (including 2,048 bytes of dispatch tables)
- `.bss`: 313,132 bytes
- vector table: 272 bytes
- minimum heap reservation: 2,048 bytes
- core-0 stack reservation: 2,048 bytes in scratch-Y
- main-SRAM headroom after the minimum heap: 195,412 bytes

The UF2 validator accepted 2,050 physical blocks: 2,049 ordinary family
`0xE48BFF59` blocks targeting `0x10000000..0x100800FF`, entirely inside the
first 4 MiB, and precisely one authorized E10 block (family `0xE48BFF57`, flags
`0x0000A000`, target `0x10FFFF00`, extension `0x9957E304`, 256 bytes of `0xEF`).
There are no catalog, game-storage, or unexpected targets. The raw-image guard
reports 3,669,896 bytes of reservation margin.

## Preservation

The canonical BASIC, Knight Tyme, Stage 3, Stage 4A, E10-safe Stage 4A, Stage
4B, Stage 4B.1, Stage 4C.1B, and Stage 4C.3 UF2 files all match their recorded
hashes. The deployed Stage 4E.2 UF2 remains exactly
`4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`;
the Stage 4C.3 rollback remains exactly
`717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`.
The CPC464 ROM, Knight Tyme snapshot, native Manic Miner snapshot/package, and
MicroPython backup (21/21 hashes, zero failures) are unchanged.

## Recommendation

Use this candidate only for a separately approved physical A/B test against
Stage 4E.2, with Manic Miner and the unchanged `CPCPERF` format. Compare at
least 32 settled intervals and record Z80, peripheral, reconstruction, LCD,
total work, effective CPC speed, and presentation FPS. To reach 80% with the
other measured costs unchanged, Z80 time must fall from 28.84 ms to about
20.82 ms (roughly 28%); 90% requires about 18.04 ms (roughly 37%). The host
result does not prove either threshold. If physical Z80 improvement is small,
hold rather than expanding risky SRAM relocation or changing Z80 semantics.
