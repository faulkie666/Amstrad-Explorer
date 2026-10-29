# Stage 4E fast playable CPC464 feasibility

## Result

**PASS for host feasibility; HOLD for production integration and hardware
performance.** The isolated prototype clears the requested approximate
fivefold desktop gate and passes practical checks for Manic Miner, Knight Tyme
and Locomotive BASIC. It is not a production engine, no RP2350 UF2 was built,
and no physical device was accessed.

## Selected practical compatibility target

The target is a curated CPC464 snapshot library rather than cycle-exact CPC
compatibility:

- CPC464, 64 KiB RAM, lower and upper ROM switching;
- instruction-oriented Z80 execution with cycle totals;
- PPI/AY keyboard scan and existing title-specific input injection;
- gate-array mode, ink and border register writes;
- CRTC start-address and standard 200-line memory layout;
- approximate 300 Hz interrupt schedule and scanline/VSYNC events;
- display reconstruction at a lower presentation rate while CPU/game logic
  continues at the full emulated rate.

Not promised: CPC664/6128, disks/tapes, 128 KiB, exact WAIT/contention,
mid-scanline palette/CRTC effects, light pen, precise AY waveform/audio, demos
or games dependent on unusual raster timing.

## Compatibility evidence

| Title | Host result | Evidence |
|---|---|---|
| Manic Miner | PASS | Central Cavern is recognizable; Q changes player x from 96 to 82, W to 112, and Shift changes y from 140 to 126; framebuffer and RAM hashes differ for all three inputs. |
| Knight Tyme | PASS | Menu is stable and recognizable; Space changes 16,224 framebuffer pixels and advances execution to PC `7C4F`; reference engine independently changes 15,472 pixels for Space. |
| Locomotive BASIC | PASS | CPC464 banner/Ready screen is stable; injected `PRINT 2+2` reaches BASIC, displays `4`, returns to Ready and changes 856 framebuffer pixels. |

Evidence images and the complete run log are under
`artifacts/stage4e-fast-evidence-final2`.

## Performance evidence

For one emulated second of the Manic Miner snapshot on the host PC:

| Configuration | Time | Speedup |
|---|---:|---:|
| Established per-T-state engine | 31.198 ms | 1.00x |
| Instruction core only | 4.813 ms | 6.48x |
| Existing timing-only peripheral path | 16.302 ms | 1.91x |
| Existing video path at 50 Hz | 21.513 ms | 1.45x |
| Existing video path at 12.5 Hz | 17.795 ms | 1.75x |
| Stage 4E span scheduler at 12.5 Hz | 5.846 ms | **5.337x** |
| Character-clock-compatible control | 13.694 ms | 2.278x |

The span scheduler reduces peripheral dispatch from 1,000,001 character events
to 15,975 scanline/interrupt/frame events per emulated second (98.4% fewer).
The direct renderer decoded 70,656 16-pixel character groups at the selected
presentation cadence. LCD RGB565 conversion measured 31.862 microseconds on
the desktop; that value is not transferable to RP2350 hardware.

The physical Stage 4C.2 baseline was about 15% CPC speed. Multiplying that by a
desktop-only 5.34x ratio suggests roughly 80% as an optimistic architecture
indicator, not a prediction. RP2350 compiler quality, SRAM/XIP placement,
indirect callbacks and the real LCD pipeline can materially change the result.
A reasonable pre-hardware expectation is 65-85% CPC speed with high
uncertainty. Only a separately approved diagnostic RP2350 integration can
answer whether 80-100% is achieved.

## Regressions and preservation

The established ten-test Release host suite passed: ROM boot, Knight Tyme,
launcher, game library, upload/removal, importer, UF2 policy, native Manic
Miner, gameplay input and performance telemetry.

All eight previously approved firmware hashes checked in this stage still
match their recorded values (BASIC, Knight Tyme, Stage 3, Stage 4A, Stage 4A
E10-safe, Stage 4B, Stage 4B.1 and Stage 4C.1B). The MicroPython backup manifest
is 21/21 valid. No ROM, snapshot, package, catalog, flash-layout or production
firmware source was modified by Stage 4E.

## Minimal integration plan

1. Keep the existing engine as a compile-time fallback and add the Stage 4E
   engine behind a host-only feature boundary first.
2. Port only the selected MIT Z80 core, CPC464 I/O adapter, CRTC span scheduler
   and direct renderer; keep storage, menu, upload/removal and E10 code intact.
3. Add deterministic title profiles declaring whether approximate timing is
   accepted; reject or route unknown snapshots to the compatibility engine.
4. Re-run the three-title evidence tests and all production regression tests.
5. Build a distinctly named diagnostic UF2 only after code-size/SRAM/UF2 policy
   review, then perform one separately approved physical A/B measurement.

The next step is integration profiling, not deployment. No deployable candidate
exists from this stage.
