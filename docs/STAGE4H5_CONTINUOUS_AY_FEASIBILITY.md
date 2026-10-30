# Stage 4H.5 — continuous AY audio feasibility

## Decision

**PASS for the bounded host architecture experiment. HOLD for firmware integration
or deployment.** The event-queue model can produce uninterrupted, pitch-correct AY
PCM while the CPC CPU is slow, but the current firmware has neither enough proven
SRAM margin for a second AY state and audio-core stack nor physical evidence that
the RP2350 can meet every 5.805 ms DMA deadline without slowing the Z80 core.

Stage 4H.4 remains the physical baseline. Its `HoldEnabled` amplifier policy is not
changed: GPIO13 stays enabled during gameplay, an empty DMA queue produces PWM
midpoint silence, and only explicit shutdown disables the amplifier.

Physical listening establishes two independent failures and therefore two separate
acceptance gates:

1. **continuity failure:** breaks, stutter and gaps; solve by generating every
   22,050 Hz PCM sample on time, without changing pitch or AY behavior;
2. **tempo failure:** the melody and game run slowly; solve only by bringing CPC
   execution materially closer to real time, not by changing playback sample rate.

## Existing cost and deficit

The supplied physical result is approximately 68% CPC speed. At that speed the
emulator creates about 14,994 samples per wall second while PWM/DMA consumes 22,050:

| Quantity | Value |
|---|---:|
| DMA block | 128 samples |
| block playback time | 5.805 ms |
| block production time at 68% | 8.537 ms |
| recurring gap | 2.732 ms per block |
| sustained deficit | 7,056 samples/s |

This proves why a larger queue or PCM buffer alone cannot fix the problem: every
finite buffer eventually drains.

The current desktop build was measured five times with identical inputs. Medians
for one emulated Manic Miner second were 8.193 ms CPU, 6.459 ms event/peripheral,
1.017 ms reconstruction and 15.625 ms total. For five emulated seconds, enabling
AY changed the median from 73.489 to 78.360 ms, a 7.133% desktop overhead. These are
host costs, not RP2350 timing. The latest physical report does not include a fresh
component-by-component CPCPERF capture, so only the 68% total is current physical
evidence. The earlier Stage 4F.6 physical result (23.00 ms Z80, 26.94 ms total)
still shows that Z80 execution is the dominant known cost; it must not be treated
as a new Stage 4H.4 measurement.

## Host prototype

The isolated prototype is under `prototypes/stage4h5_realtime_ay`. It does not
compile into firmware and does not alter CPC state, snapshots, storage or DMA.

Its design is:

1. the emulator is the single producer of resolved sound-register writes 0..13;
2. every write is retained, including repeated register 13 writes that restart an
   envelope and fine/coarse writes whose ordering is observable;
3. an 8-byte event carries a wall-audio tick, sequence, register and value;
4. a fixed single-producer/single-consumer ring publishes slots with release/acquire
   ordering, performs no allocation or locking, and fails closed on overflow;
5. a separate consumer-owned AY-3-8912 state advances from a 1 MHz AY clock and
   produces 22,050 samples/s;
6. the DMA interrupt remains limited to buffer ownership and midpoint fallback;
   synthesis must run in a bounded worker, never in the ISR.

The prototype contains both the existing reviewed tick engine and an isolated
span-driven implementation. The span engine jumps directly to the next tone,
noise, envelope, PCM-sample or queued-register boundary instead of making one
million function calls per second.

Differential testing exposed an existing batching edge case: when a register write
reduces a tone, noise or envelope period below its current counter, the old
`(counter + events) / period` formula may report multiple transitions where the
tick reference resets once on the next divider event. The prototype uses an exact
first-transition calculation and now matches the tick reference across dynamic
period reductions and repeated register-13 envelope restarts. This phase error can
alter a note immediately after a period change, but it does not cause the sustained
sample deficit, gaps or 68% musical tempo.

Prototype results:

- direct reference versus queued rendering at a 100% timebase: sample-exact for
  tone, noise, mixer, envelope, period changes and envelope restarts;
- 440/660/880 Hz sequence under a simulated 68% producer: 439.96, 656.95 and
  879.03 Hz;
- five-run desktop medians for three audio seconds: 3.429 ms span versus
  40.183 ms tick, an 11.72x reduction (desktop only);
- `ay38910_t`: 2,208 bytes;
- 64-slot queue: 528 bytes (63 usable slots);
- complete prototype object: 2,752 bytes;
- bounded queue overflow and FIFO-order tests: pass.

The frequency result demonstrates the useful property: oscillators continue at the
correct pitch while waiting for the next game write.

Full 60-second game traces were then replayed as 88.24 seconds of wall audio at a
68% producer ratio. The span engine produced every requested sample, applied every
write and had no queue overflow:

| Trace | output samples | nonzero samples | RMS | queue high-water | desktop span time/load |
|---|---:|---:|---:|---:|---:|
| Manic Miner | 1,947,262 | 1,860,168 | 0.063754 | 12 | 109.4 ms / 0.124% |
| Knight Tyme | 1,947,262 | 1,246,828 | 0.103108 | 50 | 103.6 ms / 0.117% |
| Dizzy | 1,947,262 | 3,602 | 0.000465 | 12 | 109.1 ms / 0.124% |

These are desktop measurements without DMA, interrupts or inter-core contention.
They prove bounded event consumption and continuous PCM construction, not RP2350
deadline compliance.

## Real-game trace capacity

Existing 60-second traces were re-analysed at the physical 68% producer ratio. The
counts below retain every sound-register bus write, not only writes whose value
changed:

| Game/state | writes | peak writes per 128-sample wall block | peak writes/s | envelope restarts |
|---|---:|---:|---:|---:|
| Manic Miner, no input | 7,436 | 14 | 104 | 0 |
| Knight Tyme, verified menu sequence | 9,488 | 50 | 200 | 154 |
| Dizzy, mostly silent state | 58 | 12 | 32 | 0 |

A 64-slot ring contains the observed peak but leaves only 13 slots above Knight
Tyme's busiest block. That is too little allowance for RP2350 scheduling jitter.
A 128-slot ring (1,040 bytes) is the safer bounded trial. Queue growth still does
not solve sample throughput; it only absorbs finite write bursts.

## Timing and synchronization consequences

At 68% emulation speed, a game-generated one-second note lasts about 1/0.68, or
1.47 wall seconds. Independent synthesis can keep that note's pitch correct and
remove PCM gaps, but it cannot know future register writes. Therefore:

- oscillator pitch can be real-time;
- note changes and musical tempo still follow slowed gameplay at roughly 68%;
- video and audio remain logically synchronized because writes are applied when
  emulation reaches them;
- forcing note changes onto the original CPC timebase would require future writes,
  and replaying or time-compressing late writes would move audio ahead of gameplay.

This is a continuity and pitch correction, not a way to make the game or music
tempo 100% while the Z80 is at 68%.

The future physical acceptance criteria are deliberately separate:

- **continuity:** exactly 22,050 submitted samples/s, zero sustained underruns and
  queue overruns after startup, tone frequency within 1% of the AY reference, and
  a 128-sample synthesis maximum below 5.805 ms with target p99 below 4.6 ms;
- **tempo:** target 100% CPC execution and require at least 95% sustained speed in
  the repeatable Manic Miner scene, with no skipped CPC cycles, altered sample rate
  or time-compressed PSG writes. At 90%, music would still be 10% slow.

## SRAM and multicore gate

Stage 4H.4 has 119,488 bytes main-SRAM headroom against the enforced 118,784-byte
(116 KiB) minimum, leaving only 704 bytes of permitted margin.

- exact prototype state plus 64-slot queue: 2,752 bytes; projected headroom
  116,736 bytes, 2,048 bytes below the gate before adding a worker stack;
- exact state plus recommended 128-slot queue: 3,264 bytes; projected headroom
  116,224 bytes, 2,560 bytes below the gate before adding a worker stack;
- Pico SDK 2.2.0 reserves a default 2,048-byte core-1 stack. Exact AY state,
  128 events and that stack require 5,312 bytes, exceeding the present permitted
  margin by 4,608 bytes before reset/epoch state or telemetry.

Sharing `machine.psg` between cores is rejected: the emulator mutates registers,
ports and snapshot state while synthesis mutates counters, noise LFSR, envelope,
sample timing and the DC-adjust buffer. Concurrent access would race. A compact
audio-only state might recover about 2 KiB by replacing the 512-float DC buffer,
but that would change filtering/output behaviour and requires its own differential
audio validation. Migrating ownership of the existing PSG state would affect
snapshot and PPI semantics and is not a surgical change.

The worker must use 32-bit lock-free atomics with compile-time verification on
RP2350. Reset, snapshot load and game switching require a producer/consumer epoch:
stop consumption, flush the queue, seed every sound register plus oscillator/noise/
envelope phase, then resume. A 32-bit 1 MHz timestamp wraps after about 71.6 minutes;
signed modular comparisons are safe only for events less than half that interval
ahead and must be tested around wrap.

Core 1 is available in the current design, but that is not free performance. Its
stack consumes SRAM and its XIP/data traffic can contend with core 0 and DMA. AY
synthesis must not run in a DMA ISR. The required physical deadline is a complete
128-sample packet in less than 5.805 ms, including worst-case event bursts.

## Validation

The host suite passes 20/20, including:

- BASIC and Knight Tyme regressions;
- Manic Miner native snapshot and gameplay controls;
- launcher, library, upload/removal and UF2 policy;
- display fit and performance metrics;
- tone, mixer, noise, envelope and exact fast/reference AY batching;
- DMA buffer and Stage 4H.4 amplifier policy;
- speaker sequence and Dizzy silent-state regression;
- the new ordered real-time AY prototype.

The three real-game trace replay is part of that suite and checks complete write
consumption, bounded queue high-water, continuous sample production and zero queue
overruns for Manic Miner, Knight Tyme and Dizzy.

The prototype changes no Z80 implementation. The H4 candidate remains
`d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`.
Stage 4H.1A, H3, G2.1 and F6 rollback hashes remain unchanged, both game package
hashes remain unchanged, and the MicroPython manifest re-verifies 21/21 files.

## CPC-speed path

Continuous synthesis can remove gaps and move AY work off core 0, but it cannot by
itself make a 68% CPC run at 100%. A 68% result corresponds to roughly 29.41 ms of
wall work per 20 ms CPC slice, so real time requires about 9.41 ms, or 32%, less
work. Even the earlier 23.00 ms physical Z80 result exceeds the complete 20 ms
budget before peripherals, display and audio.

The current firmware is already `-O3`, and the 76,212-byte `z80_execute_step` is in
SRAM. The rejected direct-page/expanded dispatcher must not return. The smallest
remaining ARM-locality experiment supported by the linked image is much narrower:

- `fast_z80_step`: 8 bytes in XIP;
- `z80_step`: 20 bytes in XIP plus an SRAM veneer on every instruction;
- `fast_mem_read`: 24 bytes in XIP on every emulated read;
- `fast_mem_write`: 24 bytes in XIP on every emulated write.

Relocating only those 76 bytes of wrappers/callbacks beside the existing SRAM Z80
body would fit inside the current 704-byte margin and preserve instruction logic.
It is a credible physical A/B experiment because it removes recurrent XIP fetches,
but its benefit is unknown and it is unlikely by itself to recover 9.41 ms. The
next diagnostic should also count opcode mix, memory callbacks and I/O callbacks on
the actual Manic Miner workload. Host speed cannot establish the RP2350 gain.

## Recommended next controlled experiment

Do not integrate or deploy the prototype. The host span renderer now passes tone,
noise, mixer, envelope, period-change and register-13 restart comparisons. Timestamp
wrap and reset/snapshot epochs remain to be proven. Before any firmware build,
reclaim at least 4,608 bytes plus telemetry margin and demonstrate linker-confirmed
116 KiB headroom with the explicit 2,048-byte core-1 stack.

The separately approved physical A/B should use the same Manic Miner scene and the
Stage 4H.4 rollback. Capture at least 30 settled CPCPERF/CPCAUDIO intervals plus:

- synth average, maximum and preferably p99 microseconds per 128 samples;
- queue high-water and overflow count;
- produced, submitted, played, late-buffer and underrun counts;
- core-0 Z80, event, reconstruction, render and total time;
- audible pitch, gaps, clicks and game/audio synchronization;
- Knight Tyme three-channel sequence and Dizzy silent reference.

Acceptance for continuing should require zero queue overruns, no sustained underruns
after startup, a worst-case synth time below 5.805 ms (with useful margin), unchanged
AY differential output, and no material CPC-speed regression. No physical speed,
continuity or SRAM claim is made from the desktop prototype.

That audio acceptance must remain separate from a later CPU A/B using only the
76-byte SRAM wrapper/callback relocation. CPU acceptance should compare the same
Manic Miner scene over at least 30 intervals and require a measured CPCPERF gain
without new Z80, snapshot, input or game regressions. Neither experiment should be
combined until each has independent physical evidence.

No COM port, BOOTSEL volume, physical flash, game catalog or installed game was
accessed in Stage 4H.5. No UF2 was built.
