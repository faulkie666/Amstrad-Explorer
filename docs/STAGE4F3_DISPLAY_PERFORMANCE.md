# Stage 4F.3 display coverage and physical performance investigation

## Status

**Host investigation PASS; physical performance conclusion HOLD pending current
4F.2 telemetry.** No COM port, BOOTSEL volume, game library, catalog, or device
flash was accessed. No UF2 was built or modified.

## 1. Display cause and proposed correction

The current 4F.2 centring is correct. Manic Miner's CRTC requests 32 displayed
characters by 23 rows of 8 rasters. The decoded active picture is therefore
512×184 framebuffer pixels, presented by the established 2:1 horizontal CPC
sampling as 256×184 LCD pixels.

Current physical geometry is:

- LCD: 320×240;
- CPC viewport: 320×200 at panel `(0,20)`;
- active gameplay: 256×184 at panel `x=32..287, y=20..203`;
- CPC border: 32 pixels at left and right and 16 pixels below the active image
  inside the viewport (the snapshot's red/orange border pen);
- unused panel area: 20 black rows above and below the 200-line viewport.

The active game occupies 47,104 of 76,800 LCD pixels, or **61.3%**. This is why
correct centring still looks substantially smaller than the panel. The border
colour is reconstructed correctly; the remaining issue is presentation policy,
not framebuffer corruption, CRTC start address, stride, or clipping.

The safest larger mode is an explicit **active-display fit** mode:

1. Obtain the active rectangle from CRTC display-enable geometry, never by
   scanning for a colour.
2. Remove only the measured CPC border outside that rectangle.
3. Apply one uniform scale factor to both logical axes.
4. Use nearest-neighbour/fixed-point lookup for crisp CPC pixels.
5. Centre the result on the LCD and fall back to the established viewport if
   CRTC geometry is invalid or changes incompatibly during a frame.

For Manic Miner, a uniform 1.25× scale maps 256×184 to **320×230**, placed at
panel `x=0..319, y=5..234`. Only five black rows remain above and below. Active
coverage becomes 73,600 pixels, or **95.8%** of the LCD. No gameplay pixel is
cropped and the aspect changes only from 1.391304 to 1.391304 (the exact 5:4
scale).

Host evidence is `artifacts/stage4f3-manic-proportional-320x230.png`; the
prototype is `prototypes/stage4f3_display_scaler.cpp`.

### Other titles and reference engine

- BASIC's active image is already 320×200. Width limits the uniform scale to
  1.0, so it remains 320×200 at `y=20`.
- Knight Tyme's saved 32×25-character image is logically 256×200. Uniform fit
  would be approximately 307×240, centred with six/seven black columns at the
  sides. Its snapshot/menu test must be retained before enabling this mode.
- The framebuffer and reference engine remain unchanged. If active-display fit
  is offered to the reference path, its beam-derived active origin must be
  supplied through presentation metadata; the scaler must not infer it from
  border colour. The mode can initially be opt-in for the fast path while the
  established reference presentation remains the fallback.

## 2. Expected rendering overhead

The proposed output is 73,600 pixels versus the current 64,000-pixel transfer,
a **15%** increase. A five-run host prototype using the actual indexed Manic
Miner framebuffer measured:

- current palette conversion: 36.642 microseconds median;
- proportional conversion: 50.682 microseconds median;
- host conversion ratio: 1.383×, or +14.040 microseconds.

The extra host cost exceeds the pixel-count increase because each output pixel
uses precomputed X/Y lookup tables. It remains a desktop result, not an RP2350
prediction.

The last measured physical LCD/render total was 2.09 ms for 320×200. A strictly
pixel-linear lower estimate is about 2.40 ms (+0.31 ms). Lookup and cache costs
could make it higher, so a practical planning range is **+0.3 to +0.8 ms per
presentation**. At the retained 12.5 Hz cadence that is unlikely to dominate
the emulator, but only physical conversion, LCD-wait, and total-render telemetry
can establish the real cost.

## 3. Measured performance bottlenecks

The verified Stage 4E.2 physical averages were:

| Subsystem | Time per 20 ms CPC slice | Share of 33.02 ms work |
|---|---:|---:|
| Z80 execution | 28.84 ms | 87.3% |
| CPC peripherals | 0.91 ms | 2.8% |
| frame reconstruction | 1.13 ms | 3.4% |
| LCD rendering/presentation | 2.09 ms | 6.3% |

Effective CPC speed was 60.46%. The currently deployed 4F.2-OPT-DISPLAY has a
subjective performance failure but no captured numeric CPCPERF set in this
work. Therefore the physical benefit of direct pages plus SRAM dispatch tables
is **not yet established**.

The linked 4F.2 optimized engine remains dominated by a 119,316-byte
`z80_execute_step` in XIP flash. Only two 1 KiB computed-goto tables are in SRAM.
The direct-page expansion increases the instruction body from the baseline
76,212 bytes, which may trade callback savings for additional XIP instruction
cache pressure. Desktop results cannot decide that RP2350 tradeoff.

To make gameplay 30% faster relative to a 60.46% baseline requires roughly
78.6% effective CPC speed. Total work would need to fall from 33.02 ms to about
25.4 ms. If non-Z80 work stayed near 4.18 ms, Z80 time would need to fall from
28.84 ms to about **21.2 ms**, a 26.5% reduction. Display scaling consumes a
small part of that budget rather than providing it.

## 4. Targeted CPU optimisation tested

The Stage 4F.1 direct-page integration recalculated all sixteen page mappings
after every Z80 I/O transaction. Palette, keyboard, PPI, and ordinary CRTC I/O
cannot change memory banking. A narrow host-side change now refreshes mappings
only when gate-array ROM visibility, RAM configuration, or selected upper ROM
actually changes; snapshot/reset refresh remains unconditional.

Five-run optimized-host medians before versus after the gate were:

| Workload | Before | After | Difference |
|---|---:|---:|---:|
| BASIC | 3.92 ms | 3.857 ms | 1.6% lower |
| Knight Tyme | 4.07 ms | 4.032 ms | 0.9% lower |
| Manic Miner | 5.12 ms | 4.945 ms | 3.4% lower |

This is a credible cleanup but cannot plausibly supply the requested 30% on its
own. It is not present in the deployed 4F.2 UF2, and no new firmware was built.

The special verification build compared more than 61 million reads and 8.38
million writes through both direct and canonical paths with zero mismatch. Its
performance threshold is deliberately disabled because double-executing every
access makes its timing non-representative.

## 5. Host validation

- optimized full suite after refresh gating: **11/11 passed**;
- direct/callback verification integration: **1/1 passed**, zero mismatch;
- Manic Miner: 256×184 centred geometry, Q/W movement and Shift jump retained;
- BASIC: 320×200, `PRINT 2+2`, and repeated command retained;
- Knight Tyme: snapshot/menu and Space response retained;
- upload/removal, transactional recovery, flash boundaries, E10 policy, and
  reference engine remain covered.

The proportional-scaling prototype was kept separate from production rendering
and no candidate firmware was created.

## 6. Required physical measurements

Before choosing another CPU change, capture at least 32 settled CPCPERF intervals
from the currently deployed 4F.2-OPT-DISPLAY while Manic Miner is in the same
Central Cavern state used for 4E.2. Record averages and maxima for:

- Z80 execution, peripheral scheduling, reconstruction, RGB565 conversion,
  LCD wait/transfer, total render, total work, effective speed, FPS, and missed
  deadlines;
- instruction count per slice and presentation count;
- any USB diagnostic overhead or timeouts.

For a later diagnostic build, add bounded counters for direct reads/writes,
callback fallbacks, mapping refreshes, and RP2350 XIP cache accesses/hits around
the Z80 slice. RP2350 provides cache hit and access counters; deltas would show
whether the 119 KiB dispatcher loses enough cache locality to offset direct RAM
access. Counters must be read only once per telemetry interval, not per
instruction.

## 7. Recommended next controlled experiment

**First perform a read-only telemetry capture on the already deployed 4F.2
firmware; do not flash anything yet.** This directly answers whether the Stage
4F.1 optimisations helped the real RP2350 relative to the recorded 4E.2 data.

After that evidence, request separate approval for one diagnostic candidate
that combines:

1. the explicit 320×230 active-display fit mode;
2. the validated mapping-refresh gate;
3. low-rate direct/fallback and XIP cache counters;
4. the existing CPCPERF format and 12.5 Hz presentation.

The physical acceptance should compare display coverage, Z80 time, total work,
and LCD cost separately. If current 4F.2 Z80 time is not materially below
28.84 ms, further page-table tuning is unlikely to reach the target; a more
substantial but separately reviewed instruction-dispatch/XIP experiment would
then be required.

## 8. Preservation

All firmware artifacts remain byte-for-byte unchanged:

- deployed 4F.2 OPT-DISPLAY:
  `92bb9e921612f53d4cd886400b7336f62b9de8537d79cd9a0bbe232ae8c7d6cf`;
- 4F.2 FAST-DISPLAY:
  `b97f60a8d282fecee587adaac8d800b9df0ef7190010fe9523afb357827e249b`;
- 4E.2 rollback:
  `4c27dce33b57d9cd05a1a96d3e541561f873074d1e2c074ed6763196936f32b9`;
- 4C.3 rollback:
  `717d2929f3266dd9bc6c9372b943a6e31e746711a36b4da136787667ea058915`.

The MicroPython backup remains 21/21 valid. No installed game, catalog, flash
layout, or E10 guard was modified.
