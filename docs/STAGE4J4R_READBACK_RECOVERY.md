# Stage 4J.4R — read-only USB readback recovery

## Result

**PASS.** The complete legacy region is byte-identical to the Stage 4I.10
backup, and the complete CPXPCK2 destination remains erased.

No firmware flash, CPXPCK2 staging, activation, catalogue write, game upload,
or other device write occurred.

## Failure classification

The Stage 4J.4 failure was a **newline-terminated but short response at the
host boundary**:

```text
chunk=369 offset=0x40B880 expected=128 actual=77 line_bytes=183
```

`read_protocol_line()` returns a protocol line only after receiving a newline.
It had therefore not exhausted its deadline while holding an unterminated
fragment. The line contained valid hexadecimal data and decoded cleanly, but
contained only 77 data bytes instead of 128. The host correctly rejected it;
it did not pad, truncate, accept, or retry the request.

The firmware range policy accepts any nonzero request up to 128 bytes. Its
response is emitted as a prefix, one bounded `printf` per byte, and a final
newline. The evidence establishes a short completed line, but does not by
itself distinguish a dropped bounded device-stdio write from a lower USB/host
transport loss before the final newline. It does rule out the previous host
fragment-assembly defect and a response deadline expiry.

## Surgical host correction

`tools/cpc_game.py` now uses 64-byte migration reads instead of 128-byte reads.
All existing strict checks remain:

- one request for each exact offset;
- declared and received offset equality;
- exact requested byte count;
- valid CPCU1 framing and valid hexadecimal data;
- bounded response deadline;
- no padding or truncation;
- no retry after any failed request.

Only these host files changed for the recovery:

- `tools/cpc_game.py`
- `tests/migration_transport_test.py`

Focused host verification passed:

- Python syntax compilation;
- 64-byte request framing, offsets, counts, CRC and command count;
- fragmented response reassembly without a second READ command;
- terminated short responses;
- unterminated timeout responses;
- wrong offsets and corrupt hexadecimal data;
- interleaved telemetry;
- `cpc_packed_migration`, `cpc_migration_transport`, and `cpc_uf2_policy` —
  **3/3 PASS**.

## Physical session preflight

| Check | Result |
|---|---|
| Exact USB identity | PASS — `2E8A:0009`, serial `<device-serial>` |
| Matching serial ports | PASS — exactly `COM11` |
| Firmware | PASS — `0.4.23-stage4j3migrationfix` |
| Flash | PASS — `EF-40-18`, 16,777,216 bytes |
| Entries | PASS — 4 |
| Upload | PASS — idle, received 0 |
| Migration | PASS — legacy, received 0, error none |

## Legacy-region verification

Exactly one complete physical readback was performed with the validated
64-byte strategy:

- range: `0x400000–0x43AFFF`
- length: 241,664 bytes
- output:
  `artifacts/stage4j4r-readback/legacy-region-400000-43afff-64byte.bin`
- SHA-256:
  `88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`
- expected SHA-256: identical
- byte comparison against offset `0x400000` of the primary Stage 4I.10 full
  flash backup: **PASS, all 241,664 bytes identical**

## CPXPCK2 destination verification

After the legacy gate passed, one independent complete read-only pass covered:

- range: `0x43B000–0x466FFF`
- length: 180,224 bytes
- all bytes: `0xFF`
- SHA-256:
  `6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`
- expected SHA-256: identical

## Final state

Final read-only status remained:

```text
CPCU1 READY fw=0.4.23-stage4j3migrationfix flash=EF-40-18 capacity=16777216 entries=4
CPCU1 STATUS upload=idle flash=EF-40-18 capacity=16777216 entries=4 received=0 migration=legacy migration_received=0
CPCU1 MIGRATION STATUS state=legacy received=0 error=none
```

USB remained enumerated and stable throughout both complete reads and the final
status queries.

## Gates

- host receiver correction: **PASS**
- legacy-region hash: **PASS**
- legacy-region byte comparison: **PASS**
- erased CPXPCK2 destination: **PASS**
- USB/device state: **PASS**
- physical writes: **ZERO**
- overall Stage 4J.4R: **PASS**

This clears the readback integrity blocker only. CPXPCK2 staging and activation
still require their separately controlled authorisation and were not started.
