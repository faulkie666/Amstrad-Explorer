# Stage 4I.5 — internal game library integration

Date: 2026-10-30  
Mode: host-first; no physical device access or deployment

## Result

Stage 4I.5 is **HOLD for firmware integration**. The AY regression is fixed
and the full host suite is back to 20/20. Classic Invaders' alleged second-load
failure was disproved. A deterministic CPXPCK2 image of the three established
snapshot games was built and fully verified. No new title passed the required
gameplay-active gate, however, and a CPXPCK2 firmware reader/menu has not yet
been linked against the exceptionally small 316-byte margin above the mandatory
main-SRAM gate. No experimental UF2 was therefore produced.

No COM port, BOOTSEL volume, RP2350, USB library, catalogue, or physical flash
was accessed.

## 1. AY regression

### Reproduction and root cause

The failing target was `cpc_realtime_ay_prototype_test` in a Release build. Its
mixed tone/noise/envelope comparison first differed at sample 22: the span
renderer returned `0.000000000`, while the direct reference returned
`0.098313749`.

This was a **test-build defect**, not an AY renderer regression. The test used
standard `assert(...)` for both checks and operations with side effects, for
example:

```cpp
assert(mixed_span.enqueue(mixed_event));
```

`NDEBUG` removes the whole expression in Release, so none of those events was
enqueued. The direct reference was populated normally, producing the mismatch.
The same defect also removed many other test operations and checks.

### Correction

`tests/realtime_ay_prototype_test.cpp` now maps its test-local `assert` calls to
an always-evaluated `require_check`. Failure still aborts and reports the exact
expression and line. Expected samples, the reference renderer, the production
renderer, and its event ordering were not changed. The mixed span/reference
comparison remains sample-exact; it was not weakened.

Release result:

- 20/20 CTest targets passed;
- zero failures;
- the span renderer remained sample-exact against the direct AY reference;
- measured desktop prototype result: 8.45x span/tick speed-up (not an RP2350
  performance claim).

## 2. Classic Invaders

### Source and scratch media

| File | Bytes | SHA-256 |
|---|---:|---|
| `INVADERS` | 512 | `4c1d92bf3a0e64e905633c4a75c8308fa73d782924983379529150f447894631` |
| `INVADERS.BIN` | 17,152 | `c68d35fc7bf597ae65d8c3b83eb0993317a1a1758e604288fe89738cf9258b31` |
| `INVADERS.SCR` | 5,376 | `6d1af5aad5b19d0f2d044279ddceb524e41601ff7d3c18c9634c118e461fa1ba` |

The original files were read only. The 194,816-byte scratch DSK has SHA-256
`95a4b4303a64b85afd092834d5e3bb8a7a00fe02462a3c54f82a63d5503b72fb`.
Its directory and AMSDOS data
were rechecked:

- `INVADERS`: binary, load/execute `0x1470`, logical length 286;
- `INVADERS.BIN`: binary, load `0x1000`, logical length 17,008;
- `INVADERS.SCR`: binary, load `0x1000`, logical length 5,168;
- extracting every logical file from the DSK reproduced its source bytes.

### Second-load diagnosis

The scratch disk and FDC path are sound. An execution trace reaches firmware
`CAS IN DIRECT` at `PC=0xBC83` with `BC=0x4270`, `DE=0x1000`, and `HL=0x4200`.
The authentic loader then reaches its game entry at `PC=0x4200`. The earlier
HOLD was caused by a fixed frame wait that expired while the second load was
still running; it was not a malformed AMSDOS header, bad extent, or FDC
compatibility failure.

The run subsequently displayed the Classic Invaders selection menu, ran for
30 seconds, had the disk ejected, continued for another 20 seconds, and saved a
65,792-byte SNA. The snapshot is not promoted: the deterministic input probes
did not establish a sustained gameplay-active state. Classic Invaders remains
**HOLD at the start/menu transition**, no longer at its second binary load.

## 3. Bounded conversion batch

Three deliberately bounded follow-on conversions were attempted: two Arcade
and one Simulation. Each used copied source bytes in ignored staging,
deterministic scratch media, the authentic loader, bounded waits, screenshots,
snapshot capture, disk ejection, and a 60-second YM5 recording. Original files
and their staged copies have identical hashes.

| Category | Title | Source files (bytes; SHA-256) | Scratch DSK SHA-256 | Observed state | AY evidence | Result |
|---|---|---|---|---|---|---|
| Arcade | Thrust | `THRUST` (9,088; `255131b4b3055140f1d004400e4902db4b513910d83e9c41c03351be437e156b`), `THRUST.BIN` (15,104; `34028bded949f0ffa171bc09527787ad7dfe75ca52a59aa3c46ae8136dbf3c4e`) | `81c269e99f48150f7d6fc047a53a30f5f54feca88d91a6b45431ac6bdb021527` | loading art, then `VIES INFINIES (O/N) ?`; no gameplay | 3,000/3,000 frames had non-zero channel volume; 1,024 tone-register changes | HOLD: unanswered locale-specific yes/no prompt |
| Arcade | Bomb Jack | `BOMBJACK` (5,120; `23214f2f4cd0821e53555e07c381b1b331ddbf1592d6e6a5d9c86733fa831e28`), `BOMBJACK.BIN` (24,576; `cba7e7d572c9344ae8b2953e699f84e9c9e663ad7ee2d13850a28694483e5b6d`) | `ea4bcc46b7d41c5e2f7fdf9cdcf58e9ff5e64572513ccab5f70987d787f95eb8` | normal/infinite-lives selector; no gameplay | no AY register changes or audible-volume frames in the capture | HOLD: numeric mode selector not handled by this bounded profile |
| Simulation | Fighter Pilot | `PILOT.BAS` (384; `c0c8b47c89b43b73e304dc9ce2fd182dde4b87dc592eed0829c2bd24e94d4609`), `PILOT.BIN` (37,376; `9db04792a2265e0e5e7da0a14e1e92a6eb364e7b26855143284c330ee9604d71`) | `be664972b2f341e1f100a2647090a8cee479a0e54dd1402f68fa77263dba5e6e` | title, then options screen; no take-off/gameplay | 369 tone-register changes; volumes remained enabled, but music/effects were not classified | HOLD: option navigation plus Enter required |

The three 65,792-byte captures are diagnostic states only. They were not
compressed, packaged, or copied into `ExplorerReady`, because the mandatory
gameplay-active gate failed. This is why the bounded run reports zero new
conversions rather than three false positives. The remaining allowance of the
“up to 50” batch was intentionally not spent repeating the same missing prompt
profiles.

Batch totals:

- PASS: 0;
- HOLD: 3;
- UNSUPPORTED: 0;
- FAILED: 0;
- additional Explorer-ready games: 0.

## 4. CPXPCK2 packing

The Stage 4I.4 packer was tightened so records are physically ordered by
category, A-Z key, title, and stable ID. Inspection now rejects an unordered
record table. This allows a future menu to iterate memory-mapped records without
allocating an SRAM sorting table.

Only the three established, physically accepted snapshot games were packed.
Locomotive BASIC remains the built-in first menu entry and has no SNA payload.

| Game | Raw SNA | CPXPCK2 payload | Ratio |
|---|---:|---:|---:|
| Treasure Island Dizzy | 65,792 | 41,356 | 62.86% |
| Knight Tyme | 65,792 | 32,801 | 49.86% |
| Manic Miner | 65,792 | 27,447 | 41.72% |

The complete dual-header/dual-table image is 118,016 bytes. Its SHA-256 is
`02a8f553e56a48c2282af3021bf2ab469db5900880ec989a5c1bf6547965ad30`.
Two builds were byte-identical.

Integrity results:

- both headers and both record tables valid;
- record-table CRC-32 `f8816927`;
- payload-area CRC-32 `93fe5faa`;
- unique stable IDs;
- valid Arcade category and D/K/M A-Z order;
- 16-byte payload alignment and non-overlapping bounds;
- every compressed CRC and decoded CRC verified;
- every LZ4 RAM body round-tripped to its exact 65,792-byte SNA;
- corrupt header A fell back to valid header B;
- payload corruption invalidated both catalogues and was rejected.

The current conservative CPCLIB1 high-water remains flash offset `0x43B000`.
Placing this proof image there would end at `0x457D00`, entirely inside the
`0x408000–0xFEFFFF` game region. Of the 12,275,712 bytes currently remaining
above that frozen high-water, 12,157,696 bytes (11.59 MiB) would remain after
this image. The `0xFF0000–0xFFFFFF` E10 guard is untouched.

The three measured compressed payloads average 33,868 bytes. At that narrow,
conservative average, the post-image space corresponds to about 358 more
payloads before record-table/alignment growth. The broader six-game Stage 4I.4
sample still supports an expected figure near 461 and a measured-range estimate
of 294–647. These are capacity estimates, not compatibility counts.

No hardware catalogue was written. A future migration must first durably reserve
the CPXPCK2 range in both CPCLIB1 catalogues so rollback firmware cannot allocate
over it.

## 5. Experimental firmware integration

No firmware candidate was built. This is a deliberate safety HOLD, not a build
failure.

The proposed runtime path remains technically compact:

- validate one of two 128-byte headers and its selected 160-byte record table;
- walk flash-backed records in their stored category/A-Z/title order;
- retain only category, letter, selected record, and scroll position;
- validate bounds and CRC before launch;
- parse the 256-byte SNA header in place;
- LZ4-decode the 64 KiB body directly into the existing CPC RAM;
- restore the normal snapshot CPU/peripheral state and retain existing control
  profile and AY paths.

The decoder probe costs 194 bytes of XIP code, zero static data/BSS, and a
measured 24-byte call frame. It needs no history buffer. However, Stage 4H.5D's
linked baseline has only 119,100 bytes of main-SRAM headroom against the
mandatory 118,784-byte gate: **316 bytes of margin**. The current CPCLIB1
`library::View` also retains a 384-byte valid-index array that cannot simply be
removed because upload, removal, rollback, and the four installed entries must
remain compatible.

A source-only estimate is not linker proof. Until the flash-backed CPXPCK2 view,
hierarchical menu state, and launch path exist together in an isolated target,
claiming that they fit inside 316 bytes would be unsafe. The SRAM gate is thus
**UNPROVEN**, not weakened and not reported as passed. No stack reservation,
AY queue, framebuffer, DMA allocation, or 76,212-byte SRAM Z80 routine was
changed.

The next bounded implementation should first add a stateless CPXPCK2 parser and
decoder host test, then link the exact Stage 4H.5D configuration. It may proceed
to a UF2 only if the existing post-link gate still reports at least 118,784
bytes, all 20 host regressions remain green, and the normal UF2/E10 policy passes.

## 6. Preservation

- Stage 4H.4 UF2 SHA-256 remains
  `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`.
- Stage 4H.5D UF2 SHA-256 remains
  `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`.
- Locomotive BASIC, Knight Tyme, Manic Miner, and Dizzy remain the four
  Explorer-ready/installed compatibility entries.
- Authentic AY state and the production audio paths were not changed.
- Source collections on `D:` were read only.

## Final status

- **STAGE 4I.5:** HOLD
- **HOST TESTS:** 20/20 PASS (Release)
- **CLASSIC INVADERS:** HOLD at gameplay-start verification; authentic second AMSDOS load PASS
- **NEW GAMES CONVERTED:** 0 (3 bounded candidates HOLD)
- **TOTAL EXPLORER-READY GAMES:** 4
- **COMPRESSED PACK SIZE:** 118,016 bytes; SHA-256 `02a8f553e56a48c2282af3021bf2ab469db5900880ec989a5c1bf6547965ad30`
- **INTERNAL FLASH REMAINING:** 12,157,696 bytes after hypothetical placement at `0x43B000`
- **FIRMWARE BUILD:** not built; runtime integration held pending exact linker proof
- **SRAM GATE:** UNPROVEN for CPXPCK2 integration; baseline margin remains 316 bytes
- **AY AUDIO:** PASS; sample-exact host regression restored, authentic captured activity preserved
- **PHYSICAL DEVICE ACCESSED:** NO
- **DEPLOYMENT:** NOT AUTHORISED
