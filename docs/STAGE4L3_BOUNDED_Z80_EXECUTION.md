# Stage 4L.3 — Bounded Z80 Execution Optimisation

## Result

**PASS for a separately approved physical A/B test.** The new fast path executes
up to 32 CPC T-states per SRAM-interpreter entry, while stopping at every CPC
scheduler boundary and at every interrupt-state-changing Z80 instruction. No
physical device was accessed.

Stage 4L.2 remains the rollback checkpoint:

- UF2: `build-rp2350-stage4l2-z80-callbacks/cpc_explorer_z80_callbacks_exp.uf2`
- SHA-256: `43ccf606648e6eef33daeb7332dda2224067873e7bef5b67e7f192daf23a6df2`

## Execution design

Each call is capped by the earliest of:

- 32 CPC T-states;
- the caller's 20 ms execution deadline;
- the next CRTC scanline transition;
- the next gate-array interrupt edge; or
- the next CPC frame boundary.

EI, DI, HALT, RETI and RETN use the interpreter's slow exit and terminate the
current span. If `ei_delay` is active at entry, the adapter forces a single
instruction so the delayed interrupt enable lasts exactly one instruction.
HALT remains a bounded four-T-state wake-up check. I/O and memory operations
still execute through the existing callbacks and preserve their ordering.

This removes repeated interpreter prologue/epilogue and register-cache
writeback work without changing opcode implementations, cycle values, memory
callbacks, peripheral scheduling, audio scheduling, input scanning, or the
presentation divisor (still four).

## Bound selection

Fly Spy was benchmarked at 1, 4, 8, 16, 32, 64, 128 and 256 T-state limits.
All bounds improved monotonically on the host. Bounds through 32 were
state-exact across all four real workloads. Manic Miner's AY semantic phase
diverged at 64 T-states despite matching CPU, RAM, CRTC, gate-array, PPI and
framebuffer state, so 64/128/256 were rejected.

Representative Fly Spy host timings for 5 seconds emulated:

| Bound | Host time |
|---:|---:|
| 1 | 28.455 ms |
| 8 | 25.987 ms |
| 16 | 24.373 ms |
| **32** | **23.073 ms** |
| 64 (rejected) | 22.066 ms |
| 128 (rejected) | 21.451 ms |
| 256 (rejected) | 20.800 ms |

Five-run Fly Spy median, measured with identical Release host builds:

- Stage 4L.2: 5.766 ms per emulated second
- Stage 4L.3, 32 T-states: 4.168 ms per emulated second
- Host reduction: **27.7%**

This is a desktop result, not an RP2350 speed claim.

## Differential correctness

For BASIC, Knight Tyme, Manic Miner and Fly Spy, bounds 4/8/16/32 matched the
one-instruction path after 20,000,000 CPC cycles for:

- complete fast-Z80 register state and cycle count;
- all 64 KiB RAM;
- gate-array, CRTC and PPI state;
- AY registers, tone/noise/envelope phase and sample counters; and
- the complete framebuffer.

The synthetic suite additionally compared registers, memory, I/O event logs,
interrupt acknowledgements and cycles for:

- memory reads/writes and IN/OUT;
- EI/DI delayed enable plus HALT;
- HALT wake-up; and
- IM2 interrupt vectoring/priority.

All comparisons passed at a 32-T-state bound.

Full host regression: **27/27 PASS**. This includes BASIC, Knight Tyme, Manic
Miner, launcher/library/migration, controls, display, AY/audio, UF2 policy and
the new bounded-Z80 tests.

## RP2350 memory and firmware gates

- Firmware identity: `0.4.30-stage4l3boundedz80`
- UF2: `build-rp2350-stage4l3-bounded-z80/cpc_explorer_bounded_z80_exp.uf2`
- UF2 size: 742,912 bytes
- SHA-256: `e36a05e6d93ecf5161ed93e6e4e88606b7ef6b8371a144df89a4672e5c70e686`
- ELF: text 371,956; data 3,264; BSS 316,852 bytes
- `z80_execute_step`: `0x200012b0`, 75,884 bytes, main SRAM
- Heap reservation: 2,048 bytes
- Core-0 stack: 2,048 bytes
- Main-SRAM headroom: **119,260 bytes**
- Required headroom: 118,784 bytes
- Margin: **476 bytes**

No new permanent buffer was added. The bounded-span limit occupies an existing
runtime structure and the revised interpreter code is 328 bytes smaller than
the Stage 4L.2 routine, increasing rather than reducing headroom.

UF2 validation:

- 1,451 physical blocks;
- 1,450 application blocks, family `0xE48BFF59`;
- application targets `0x10000000..0x1005A9FF`, inside the first 4 MiB;
- exactly one authorised E10 block at `0x10FFFF00`, family `0xE48BFF57`, flags
  `0x0000A000`, extension `0x9957E304`, 256 bytes of `0xEF`;
- no game-storage or catalogue targets.

## Physical expectation and next test

The host result makes this a credible candidate, but RP2350 XIP/SRAM behaviour
must be measured physically. If the 27.7% host reduction translated directly,
Fly Spy's prior 21.95 ms Z80 time would fall to roughly 15.9 ms and total work
to roughly 20.1 ms. That is only a projection; the acceptance test must compare
real CPCPERF telemetry against Stage 4L.2/4K.7 and verify gameplay, interrupts,
audio continuity and controls.

No deployment, COM access, BOOTSEL access, catalogue operation or game-library
write was performed.
