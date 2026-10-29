# Stage 4F.1 display-regression investigation

## Status

**Cause confirmed and minimal source correction host-verified; current UF2s
remain HOLD for presentation acceptance.** No firmware was rebuilt, no device
or COM port was accessed, and no physical flash operation occurred. The Stage
4E.2 and Stage 4F.1 UF2 files remain unchanged.

## Shared display geometry

Both engines expose the same 768×272 indexed CPC framebuffer. The LCD path is
also shared and unchanged:

- destination panel: 320×240;
- CPC destination: 320×200 at LCD `(0,20)`;
- source window: framebuffer `(64,36)` through `(703,235)`;
- horizontal sampling: every second source pixel, preserving the CPC's 640 to
  320 presentation ratio;
- stride: 768 pixels;
- no LCD-side stretch, changed-region scaling, or engine-specific clipping.

Thus the regression is upstream of RGB565 conversion and DMA.

## Cause

Manic Miner's saved CPC CRTC state is 32 displayed character columns and 23
displayed rows of 8 rasters: 512×184 decoded source pixels. At the established
2:1 horizontal LCD sampling this is correctly 256×184 pixels; stretching it to
320×200 would change the original CPC proportions.

The reference gate-array renderer positions the 512-pixel picture within the
768-pixel visible-beam framebuffer. In the LCD crop this gives 32 pixels of CPC
border on each side. The fast presentation reconstruction instead hard-codes
the active origin as framebuffer `(64,36)`, which is only correct for the
normal 40-column/640-pixel mode. It therefore puts Manic Miner at LCD
`x=0..255`, with the entire 64-pixel unused width on the right.

A host probe after one emulated second measured:

| Engine/title | Non-border LCD bounds | Active size |
|---|---|---:|
| Reference Manic Miner | `x=32..287, y=1..184` | 256×184 |
| Fast Manic Miner | `x=0..255, y=0..183` | 256×184 |
| Reference BASIC | `x=0..319, y=0..199` | 320×200 |
| Fast BASIC | `x=0..319, y=0..199` | 320×200 |

After compensating for `+32` horizontal and `+1` vertical placement, 99.961%
of the Manic Miner RGB bytes agree. Pixel count is identical (42,918
non-border pixels), confirming that reconstruction content, stride, scale, and
clipping are not the cause. The remaining one-row phase is the reference beam
position versus the frame-at-once reconstruction and is not the visible width
regression.

Evidence:

- `artifacts/stage4f1-reference-manic-lcd.png`
- `artifacts/stage4f1-fast-manic-lcd.png`
- probe: `prototypes/stage4f1_display_probe.cpp`

## Minimal correction implemented in source

Keep the LCD crop and 2:1 sampling unchanged. In `reconstruct_fast_frame()`,
replace the fixed horizontal active origin (`64`) with an origin computed once
per reconstructed frame:

```text
active_width = displayed_columns * 16
active_x = (AM40010_FRAMEBUFFER_WIDTH - active_width) / 2
```

For 40 columns this remains 64, so BASIC's established 320×200 presentation is
unchanged. For Manic Miner's 32 columns it becomes 128; after the existing crop
and 2:1 sampling, that produces the reference 32-pixel borders on both sides.
The decoded game pixels are neither stretched nor cropped.

Do not globally move or scale the LCD window. Retain the existing vertical
origin for the first correction: the observed one-row phase is separate and
blindly centring 184 lines would introduce an incorrect eight-pixel shift.

The calculation is outside the pixel loops and replaces an existing constant,
so its expected reconstruction overhead is effectively zero. It does not touch
Z80 execution, memory fast paths, scheduler timing, game storage, input, USB,
or flash protection.

The corrected host result is `x=32..287, y=0..183`, still exactly 256×184;
the reference is `x=32..287, y=1..184`. BASIC remains exactly 320×200 in both
engines. The corrected evidence is
`artifacts/stage4f1-fast-manic-lcd-corrected.png`.

The complete host suite passes 11/11 with a new integration assertion that the
fast and reference Manic Miner horizontal bounds agree and that the active
width remains 256 pixels. A one-second desktop benchmark remained within run
variance (BASIC fast total 3.881 ms; Manic Miner 5.013 ms). These figures are
not physical performance claims.

## Verification required before a new build

The focused host regression now requires:

1. BASIC remains exactly 320×200 in the established crop.
2. Manic Miner retains 256×184 proportions and has equal 32-pixel side borders.
3. Knight Tyme and any other non-40-column snapshot are centred without
   stretching.
4. framebuffer stride and all source accesses stay inside 768×272.
5. CPU-only benchmark results remain unchanged; reconstruction cost is reported
   separately.

A corrected firmware must be a newly named artifact and requires separate user
approval before build and deployment.
