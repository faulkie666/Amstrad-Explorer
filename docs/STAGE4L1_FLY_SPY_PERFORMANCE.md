# Stage 4L.1 — Fly Spy physical performance baseline

Date: 31 October 2026

Device: Pimoroni Explorer RP2350

Firmware: `0.4.28-stage4k7migrationrecoverfix`

Game: Fly Spy
Result: **MEASUREMENT PASS**

This was a read-only telemetry session. No firmware, catalogue, package, or
external-flash write was performed.

## Capture

- Port: `COM11`, 115200 baud, DTR asserted for diagnostics
- Requested duration: 65 seconds
- Telemetry duration: 64.973424 seconds
- Complete records: 64 `CPCPERF` and 64 `CPCAUDIO`
- Malformed records or replacement characters: 0
- Raw capture: `artifacts/stage4l1-fly-spy-performance/telemetry-20261031-gameplay.log`
- Raw size: 36,456 bytes
- Raw SHA-256: `90dd891779094c94573a70d6ed88f86c9d164114c14d2024dcdf780e0a456eb2`

The owner played Fly Spy continuously during this capture.

## Physical RP2350 results

| Measurement | Result |
|---|---:|
| Effective CPC speed, average | **76.33%** |
| Effective CPC speed, median | 76.30% |
| Effective CPC speed, minimum / maximum | 74.90% / 77.80% |
| Completed 20 ms CPC slices | 2,481 |
| Missed 20 ms deadlines | 2,481 (100%) |
| Total iteration, average | **26.148 ms** |
| Average interval's maximum iteration | 25.661–26.632 ms |
| Worst individual iteration | **38.718 ms** |
| Active work, average / worst | 26.144 ms / 38.714 ms |
| Fast-engine execution, average / worst | 23.745 ms / 29.623 ms |
| Z80 CPU work, average / worst | **21.946 ms / 23.306 ms** |
| Peripheral event work, average / worst | 0.293 ms / 0.351 ms |
| Video reconstruction, average / worst | 1.503 ms / 6.066 ms |
| LCD conversion, average / worst | 2.172 ms / 8.751 ms |
| LCD DMA wait, average / worst | 0.031 ms / 0.138 ms |
| Complete presentation rendering, average / worst | 2.223 ms / 8.959 ms |
| Presented frames | 620 |
| Reported presentation rate, average | **9.49 FPS** |
| Reported presentation rate, minimum / maximum | 8.8 / 9.9 FPS |
| Aggregate presentation rate | 9.54 FPS |

The original CPC execution target is one 20 ms slice at 50 Hz. This build is
configured to present every fourth emulated frame (12.5 Hz at full CPC speed),
so the measured presentation rate is consistent with the measured 76% emulator
speed. It is not a host benchmark.

## AY and physical audio path

The independent core-1 renderer remained in state 3 (`Running`) throughout.

| Counter | Result over capture |
|---|---:|
| PCM samples generated | 1,411,072 |
| DMA blocks played | 11,024 |
| Effective PCM generation rate | approximately 22,060 samples/s |
| Audio underruns / overruns | **0 / 0** |
| Audio renderer deadline misses | **0** |
| Queue-full events / sequence errors | **0 / 0** |
| Maximum renderer time | 988 us |
| Audio block deadline | approximately 5,805 us |
| Core-1 stack high-water | 712 bytes |

Fly Spy generated **no new AY register-write events during this gameplay
capture**: `produced=0` throughout. `consumed=16` represents the initial AY
register state seeded when the audio renderer started. This is evidence that
the measured scene did not request AY music or effects; it is not an audio
playback failure. A game that generates AY commands but produces no audible
PCM would show increasing producer/consumer event counters, which did not
occur here. This capture does not prove that every later Fly Spy state is
silent.

PCM production and DMA playback remained continuous and exact: the PCM delta
equals the played-block delta multiplied by 128 samples. The silent AY state
therefore exercised a healthy output pipeline without underruns.

## Bottleneck assessment

The measured bottleneck is Z80 execution. CPU work consumed approximately
83.9% of total iteration time and already exceeded the complete 20 ms real-time
budget on average. The whole fast-engine execution path consumed approximately
90.8% of the iteration. By comparison, average complete presentation rendering
was approximately 2.223 ms and LCD DMA waiting was only 31 us.

The occasional 38.718 ms loop maximum correlates with presentation work: the
worst conversion/reconstruction values are much higher than their averages.
Those spikes affect smoothness, but removing all average rendering cost would
still leave average Z80 CPU execution above the 20 ms target. The primary route
to full CPC speed therefore remains CPU-emulation optimisation; display work is
a secondary source of periodic stalls.

## Acceptance

- Physical measurement capture: **PASS**
- CPC full-speed timing: **FAIL** — average 76.33%, all deadlines missed
- Audio transport stability: **PASS**
- AY music/effects in the measured Fly Spy scene: **NOT OBSERVED**
- Firmware or library modification: **NONE**
