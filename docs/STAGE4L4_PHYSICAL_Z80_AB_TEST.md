# Stage 4L.4 — Physical Z80 A/B Test

Date: 31 October 2026

Device: Pimoroni Explorer RP2350, serial `C461A378690CF764`

Candidate: `0.4.30-stage4l3boundedz80`

## Installation

- Candidate SHA-256: `e36a05e6d93ecf5161ed93e6e4e88606b7ef6b8371a144df89a4672e5c70e686`
- UF2 validation: 1,450 application blocks in `0x10000000–0x1005A9FF`, plus exactly one authorised E10 block at `0x10FFFF00`
- BOOTSEL: `2E8A:000F`, serial/chip ID `C461A378690CF764`, RP2350 A2/QFN80, 16 MiB flash
- Installation attempts: **1**
- Picotool result: Flash 100%, verification 100%, `OK`
- Application USB: `2E8A:0009`, COM11, expected serial
- Physical rollback baseline: `0.4.28-stage4k7migrationrecoverfix`
- Rollback UF2 SHA-256: `fe84bcd5c439a88ba030f28f48f842987876fe8ae88d768de6a97a2561b99dbd`

No game upload, catalogue write, migration operation, manual erase, retry or rollback occurred.

## Library preservation

Post-install CPCU1 checks reported:

- firmware `0.4.30-stage4l3boundedz80`;
- flash `EF-40-18`, 16,777,216 bytes;
- migration `active`;
- upload `idle`;
- 33 visible games, byte-for-byte identity list unchanged from preflight;
- historical authoritative catalogue evidence still contains the committed hidden `4kroid` record.

## Fly Spy physical telemetry

Raw capture:

- file: `artifacts/stage4l4-physical-ab/fly-spy-stage4l3-gameplay-telemetry.log`
- duration: 64.799995 seconds
- records: 64 CPCPERF and 64 CPCAUDIO
- size: 36,582 bytes
- SHA-256: `25d63e17cfb4ccd8527e4d99a1731580affe42625897740efdae6f63b3223500`
- malformed/replacement characters: 0

| Measurement | Stage 4L.1 baseline | Stage 4L.3 physical | Change |
|---|---:|---:|---:|
| CPC speed | 76.33% | **99.95%** | +23.62 points / +30.9% relative |
| Average loop | 26.15 ms | **19.966 ms** | -23.6% |
| Z80 CPU work | 21.95 ms | **12.557 ms** | -42.8% |
| Fast execution | 23.745 ms | **14.353 ms** | -39.6% |
| Presentation | 9.49 FPS | **12.50 FPS** | +31.7% |

Additional results:

- speed range: 99.1–101.0%; median 99.8%;
- slices: 3,240;
- missed CPC 20 ms deadlines: 1,471 (45.4%); average remains within budget, but presentation-period spikes still miss individual deadlines;
- worst loop: 33.160 ms;
- worst Z80 CPU interval: 17.916 ms;
- average/worst event work: 0.294/0.345 ms;
- average/worst reconstruction: 1.499/6.046 ms;
- average/worst complete presentation: 2.228/8.973 ms.

## Audio

- underruns: 0;
- overruns: 0;
- audio deadline misses: 0;
- queue-full events: 0;
- renderer maximum: 933 us;
- core-1 stack high-water: 712 bytes;
- PCM sample delta: 1,407,232;
- DMA played-block delta: 10,994 (exactly 1,407,232 / 128);
- Fly Spy AY events: no new register writes in the measured scene (`produced` and `consumed` deltas both zero).

## Acceptance status

- Firmware installation: **PASS**
- Firmware/USB verification: **PASS**
- Library preservation: **PASS**
- Fly Spy performance: **PASS — approximately full CPC speed**
- Audio transport: **PASS**
- Launcher/BASIC/Knight Tyme/Manic Miner physical regression: **PENDING OWNER CONFIRMATION**
