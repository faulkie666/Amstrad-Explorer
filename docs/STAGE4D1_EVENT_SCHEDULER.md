# Stage 4D.1 event-scheduler feasibility report

## Recommendation

**HOLD at the Phase 1 gate.** The initial SNA time origin can be reconciled,
but the evaluated instruction core does not expose bus events at the same
T-state offsets as the established pin-level Z80. Correct CPC WAIT ordering
therefore cannot be preserved by the proposed adapter. The bounded experiment
stopped before implementing a broad peripheral rewrite.

No production source, firmware build, UF2, physical device, installed library,
flash layout, or E10 policy was changed.

## Initial timing root cause

The reference SNA loader restores CPU registers, loads CRTC/GA/PPI/AY registers,
sets the gate-array sequencer to its reset phase, and calls `z80_prefetch()`. The
resulting state is immediately before the first M1/T1.

On the first reference tick:

1. the Z80 drives M1/T1;
2. the gate-array sequencer advances from phase 0 to phase 1 and latches WAIT;
3. the Z80 samples that latched WAIT on the following tick.

After the initial fetch, ordinary generated memory/IO microsteps test the WAIT
value already latched by the preceding gate-array tick before advancing the
gate array. Stage 4D instead advanced the GA to `cycle_start + 1` for every bus
cycle, so an ordinary bus cycle could observe the next READY phase rather than
the reference phase.

The corrected common-timebase rule is:

- loaded-SNA first M1: sample at origin + 1;
- subsequent ordinary bus microsteps: sample the previously latched phase at
  the bus-cycle origin;
- differential checkpoints must also compare absolute ticks, sequencer phase,
  WAIT/INT/SYNC pins and interrupt state, not only architectural CPU registers.

With that rule, CPC464 BASIC's initial `POP BC` and Manic Miner's initial
`CP n` match after instruction 1 across CPU, RAM, framebuffer, GA, CRTC, PPI,
AY, absolute ticks, sequencer phase and WAIT state:

- BASIC: 15 reference ticks and 15 candidate ticks;
- Manic Miner: 11 reference ticks and 11 candidate ticks.

## Blocking M-cycle incompatibility

Knight Tyme begins with `CALL nn`. The established engine places operand reads,
internal T-states and stack writes at explicit pin-level microsteps. The
candidate core increments `op_tstate` by fixed 3T/4T blocks immediately before
callbacks. It reports the correct whole-instruction nominal duration, but its
callback offsets make the two operand reads and two writes appear contiguous at
different GA phases.

Applying the timebase rule therefore gives 24 candidate ticks versus 23
reference ticks for the first `CALL`. Choosing the old offset makes Knight Tyme
appear to match but reintroduces the proven one-tick BASIC/Manic error. This is
not a single global phase correction.

An every-instruction differential located the first exact divergences:

| State | First divergence | Reference | Candidate | First differing state |
|---|---:|---:|---:|---|
| BASIC | instruction 8 | 107 ticks, seq 107, WAIT high | 108 ticks, seq 108, WAIT low | absolute timing only |
| Knight Tyme | instruction 1 | 23 ticks, seq 23, WAIT high | 24 ticks, seq 24, WAIT low | absolute timing only |
| Manic Miner | instruction 1,150 | PC `0038`, 13,816 ticks | PC `B939`, 13,818 ticks | interrupt acceptance, CPU and RAM |

The Manic Miner result is direct evidence that the phase error changes interrupt
ordering, not merely an internal counter. At 50,000 instructions the candidate
remains thousands of CPC ticks out of phase, with CPU, RAM and peripheral
divergence.

Fixing this would require an opcode/micro-operation timing table or a Z80 core
which emits the exact start and WAIT-sampling point of every bus cycle. Retiming
the current candidate opcode by opcode is a Z80-core rewrite and exceeds this
bounded experiment.

## Single-scanline scheduler gate

**Not entered: correctness gate failed.** A scheduler cannot safely advance to
the next CRTC/video event when it does not know the exact time of the pending
CPU bus sample. Advancing across that unknown point could change WAIT duration,
IO side effects, interrupt acknowledgement or the pixel/CRTC state observed by
software.

The current AM40010 model has meaningful video phases twice per four CPC
T-states: one CRTC/first-video-byte event and one second-byte/decode event. In a
nominal 64-character scanline this is 256 T-states, 64 CRTC events and 128 GA
video-phase events. A correct event implementation could skip at most 128
otherwise-light GA tick positions before further span optimisation. That is a
theoretical 50% tick-dispatch reduction, not a measured implementation result.

Because no correct scanline scheduler passed the gate:

- actual eliminated per-T-state dispatches: **0**;
- single-scanline differential result: **not run / blocked by CPU bus timing**;
- repeated scanlines and 50,000-instruction event-scheduler tests: **not run**.

## Performance result

No event-scheduler benchmark is reported because benchmarking a scheduler that
cannot preserve WAIT order would be misleading. The timebase diagnostic still
performs essentially every AM40010 T-state dispatch and remains in the same
roughly 1.2x desktop range as Stage 4D. These desktop observations make no
claim about RP2350 FPS.

For 50,000 instructions the invalid hybrid performed 570,730 peripheral
dispatches versus BASIC's 591,384 reference ticks (3.49% fewer), 555,506 versus
Knight Tyme's 556,835 (0.24% fewer), and 479,928 versus Manic Miner's 493,556
(2.76% fewer). The corresponding single-run desktop ratios were 1.229x, 1.204x
and 1.219x. Those skipped ticks are timing errors, not a successful scheduler,
and are not counted as an optimisation result.

Even the theoretical removal of half the lightweight GA dispatch positions is
not a credible demonstrated route from the measured 16.9% physical speed to
90%. The remaining CRTC/video events, memory reads, pixel decode and exact bus
coordination still dominate. A credible route would first require an exact
bus-timed instruction core and then measured character/span batching.

## Upstream research and licences

No upstream code was copied.

- `rpsubc8/ESP32TinyCPC`, reviewed at
  `041dff33fc2a42bfd8598645f078d33ae4f35bf8`, is repository-licensed WTFPL.
  It executes instructions against a 256-cycle budget and calls a line renderer
  once per one of 312 scanlines. This is useful evidence that scanline batching
  is fast on an ESP32, but it does not preserve the pin-level CPC WAIT/contention
  ordering required by this project.
- `EremusOne/CPCESP_alpha`, reviewed at
  `f130f94bf60699ab466ef06c90b91a722b65298a`, declares GPLv3 and currently
  provides no source scheduler suitable for reuse.
- `ColinPitrat/caprice32`, reviewed at
  `a0d78f0f7c6d19a5d3205d1b231bd4da100ec6f3`, is GPL-2.0. It batches
  `crtc_cycle(iCycleCount >> 2)` after instruction execution. This is a useful
  architectural comparison, but its whole-instruction update boundary cannot
  serve as a bit-exact replacement for the current pin-level oracle.
- `salvogendut/1984` is GPL-2.0 and explicitly uses cycle-stepped devices rather
  than demonstrating the required event scheduler.

The licensing and timing models rule out direct reuse without both a project
licensing decision and a new compatibility proof.

## Preservation and tests

The existing top-level Release host suite remains the production regression
gate. Stage 4D.1 only changes the isolated host prototype and this report. The
prototype now reports a failing exit status when absolute timing or component
state differs.

The next technically valid step, if the project continues, is not a peripheral
scheduler. It is a narrower host-only search for a Z80 core/interface that
emits exact bus-cycle start, WAIT-sample and interrupt-ack timestamps, or an
instruction micro-op table demonstrably equivalent to the established core.
Only after that interface passes the three-game timing differential should a
single-scanline scheduler be attempted.
