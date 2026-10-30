# Stage 4I.12 — First Physical CPXPCK2 Library Installation

## Authorised physical activation — PASS

Date: 2026-10-30

**PASS.** Exactly one activation command was issued for the existing verified
transaction. It completed successfully, the complete active pack readback
matched the approved Stage 4I.7 SHA-256, and the final catalogue contains
exactly the six intended visible entries. No retry, restaging, firmware write,
manual erase, abort, repair or rollback was performed.

### Pre-activation gate

- USB `2E8A:0009`;
- serial `<device-serial>`;
- COM11;
- firmware `0.4.23-stage4j3migrationfix`;
- flash `EF-40-18`, 16,777,216 bytes;
- upload idle;
- migration `verified`, received 179,360, error none;
- approved pack: 179,360 bytes, SHA-256
  `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`;
- approved staged-destination evidence: 180,224 bytes, SHA-256
  `48b90a1780a507ee09d89990a0ca68f0df73d7bc472d630f708d64456a85edc9`;
- both activation headers were independently confirmed erased before the
  activation command.

### Exact activation command

```powershell
py tools/cpc_game.py activate-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --confirm ACTIVATE --execute --port COM11
```

The documented command first repeated its read-only staged-image comparison,
then emitted exactly one `MIGRATION ACTIVATE` frame. Response:

```text
CPCU1 READY fw=0.4.23-stage4j3migrationfix flash=EF-40-18 capacity=16777216 entries=4
CPCU1 MIGRATION ACTIVE crc=ec7fd1e9 entries=6
CPCU1 MIGRATION STATUS state=active received=179360 error=none
independent active-image SHA-256: PASS
```

Activation attempts: **1**. Activation retries: **0**.

### Final read-only acceptance

```text
CPCU1 READY fw=0.4.23-stage4j3migrationfix flash=EF-40-18 capacity=16777216 entries=6
CPCU1 STATUS upload=idle flash=EF-40-18 capacity=16777216 entries=6 received=0 migration=active migration_received=179360
CPCU1 MIGRATION STATUS state=active received=179360 error=none
CPCU1 GAME 0 id=locomotive_basic title=Locomotive BASIC
CPCU1 GAME 1 id=columns title=Columns
CPCU1 GAME 2 id=dizzy title=Treasure Island Dizzy
CPCU1 GAME 3 id=knight_tyme title=Knight Tyme
CPCU1 GAME 4 id=manic_miner title=Manic Miner
CPCU1 GAME 5 id=thrust title=Thrust
CPCU1 END
```

The serial list exposes packed records in their deterministic category/A-Z
record order. The physical launcher uses the established Arcade/Simulation/A-Z
hierarchy rather than a flat six-row menu. The final identity set is exactly:

1. Locomotive BASIC
2. Knight Tyme
3. Manic Miner
4. Dizzy / Treasure Island Dizzy record
5. Columns
6. Thrust

Knight Tyme appears exactly once. Fruity Frank remains compatibility-hidden
and is absent from the visible list. All original four identities remain
present. The final USB composite target remained present and stable with serial
`<device-serial>`; firmware and flash identity were unchanged.

### Final result

| Gate | Result |
|---|---|
| Activation attempt | PASS — exactly one |
| Activation response | PASS — CRC `ec7fd1e9`, entries 6 |
| Complete active pack SHA-256 | PASS |
| Migration state | PASS — active, received 179,360, error none |
| Upload state | PASS — idle |
| Expected identity set | PASS — exactly six |
| Duplicate Knight Tyme | PASS — none |
| Fruity Frank hidden | PASS |
| Original four preserved | PASS |
| USB/device stability | PASS |
| Overall | **PASS** |

STOP: physical gameplay acceptance for Columns and Thrust is the next separate
step. No further flash-writing operation is authorised by this result.

## Stage 4I.12V.2 — resumed verified readback PASS

Date: 2026-10-30

**PASS — the existing verified transaction is ready for a separately authorised
single activation attempt.** No staging, activation, erase, abort, repair,
firmware operation or other physical write occurred in this stage.

### Resumable host correction

`tools/stage4i12v_readback.py` now validates and reuses contiguous evidence
windows, never overwrites them, saves every new verified 4 KiB window
immediately, and atomically updates `readback-manifest.json` after each window.
Any read failure reports its containing window and exits without retry.

Before COM11 access, all 25 existing windows were checked by filename, exact
offset, length and contents against the approved staged representation:

- range `0x43B000–0x453FFF`;
- 102,400 bytes;
- SHA-256 `d41ec93a5449358823cc8a0ce622089a8cf76521653f98618ebe125f42a91789`;
- expected SHA-256 identical;
- first unread offset `0x454000`.

The evidence-path filesystem regression passed. No existing window was deleted,
overwritten or reread from the device.

### Physical identity and final state

Exactly one matching target and COM interface were identified:

- USB `2E8A:0009`;
- serial `<device-serial>`;
- COM11;
- firmware `0.4.23-stage4j3migrationfix`;
- flash `EF-40-18`, 16,777,216 bytes;
- upload idle, received 0;
- migration `verified`, received 179,360, error none;
- entries 4 before activation.

The same state was confirmed again after all physical reads. USB enumeration
remained stable and every requested 64-byte CPCU1 response completed with exact
framing, offset and length.

### Complete destination proof

The session resumed at `0x454000` and completed through `0x466FFF`. The full
destination was reconstructed from the 25 preserved windows plus the new
windows and compared byte-for-byte with the protocol-defined staged image.

| Evidence | Result |
|---|---|
| Destination range | `0x43B000–0x466FFF` |
| Bytes | 180,224 |
| Saved SHA-256 | `48b90a1780a507ee09d89990a0ca68f0df73d7bc472d630f708d64456a85edc9` |
| Expected staged-destination SHA-256 | identical |
| 179,360-byte staged-image SHA-256 | `a64f80387a0df0e6d2416ee752d2d9b717e8b0fc681326e035179899b5b53b9a` |
| Expected staged-image SHA-256 | identical |
| Non-header pack bytes | exact match to approved Stage 4I.7 pack |
| Header A `0x43B000–0x43B0FF` | erased/unpublished |
| Header B `0x43C000–0x43C0FF` | erased/unpublished |
| Tail after byte 179,360 | 864 bytes, all erased |

The final approved pack remains 179,360 bytes with SHA-256
`27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`.
Its different pre-activation staged SHA is expected because the two header
pages are deliberately unpublished.

### Catalogue and legacy-payload proof

Both intentionally modified catalogue reservation slots were read and
validated structurally rather than compared blindly with their pre-staging
bytes:

| Slot | Generation | Entries | High-water | SHA-256 |
|---:|---:|---:|---:|---|
| 0 (active) | 7 | Manic Miner, Dizzy | `0x67000` | `62621846d214d17ebb61337f8180ef30583128e0d2c4e69606c2492fbeb299a6` |
| 1 | 6 | Manic Miner, Dizzy | `0x67000` | `45599e8d19ac4a04fd4609ba9f33fdfaa480579b6d5e57154bb3f0491af9c7d8` |

Both headers, descriptor CRCs, allocation ranges, snapshot identities and
payload CRCs pass. The catalogue pair SHA-256 is
`ad82380598c71654e8906c9b73d58c1b969e788d58a2500511247921eb7805a7`.

The untouched legacy payload was independently compared with Stage 4I.10:

- range `0x408000–0x43AFFF`;
- bytes 208,896;
- saved SHA-256
  `957c0df5f33a05cf5911655907d036a234ee848580070572d4f7847cbe68eae0`;
- Stage 4I.10 expected SHA-256: identical;
- byte comparison: PASS;
- Manic Miner snapshot SHA-256
  `6661c74f919e228b2d21e1c0fceba7369c80281c1e27c53d03d624b1ff6d743d`;
- Dizzy snapshot SHA-256
  `2086041545be0bbdaa4e87a60c86d0e69e7d41c27242621d78416ceed34da943`.

The complete post-staging legacy range is 241,664 bytes with SHA-256
`3963c82e1188f493b26a537d9f900e66088653290e85c2c3d11a519671539b65`.
Its difference from the pre-staging full-range hash is exactly the documented
catalogue reservation publication.

### Evidence files

Directory:

`artifacts/stage4i12v1-readback-20261030-1`

| File | Bytes | SHA-256 |
|---|---:|---|
| `staged-destination-43b000-466fff.bin` | 180,224 | `48b90a1780a507ee09d89990a0ca68f0df73d7bc472d630f708d64456a85edc9` |
| `post-stage-catalogues-400000-407fff.bin` | 32,768 | `ad82380598c71654e8906c9b73d58c1b969e788d58a2500511247921eb7805a7` |
| `legacy-payload-408000-43afff.bin` | 208,896 | `957c0df5f33a05cf5911655907d036a234ee848580070572d4f7847cbe68eae0` |
| `post-stage-legacy-400000-43afff.bin` | 241,664 | `3963c82e1188f493b26a537d9f900e66088653290e85c2c3d11a519671539b65` |
| `readback-manifest.json` | 25,886 | `617172508746be13b240db86423e16dee7cb9da2786dc8c4cacad445f035bbd7` |
| `validation.json` | 3,808 | `6dc2f0893c018f5319a9fd9fc57ffdc64feeb5d034075cf51be645ede20c750d` |

The manifest contains 103 exact 4 KiB windows spanning destination,
catalogues and legacy payload. Each window records its physical offset, length,
filename and SHA-256.

### Gates

| Gate | Result |
|---|---|
| Existing evidence validation | PASS |
| Exact device identity | PASS |
| Verified transaction unchanged | PASS |
| Complete staged destination | PASS |
| Approved pack content | PASS |
| Activation headers unpublished | PASS |
| Destination padding erased | PASS |
| Both catalogue reservations valid | PASS |
| Legacy payload byte preservation | PASS |
| Final USB/status stability | PASS |
| Ready for one separately authorised activation | **PASS** |

- physical reads resumed from `0x454000`;
- restaging: 0;
- activation: 0;
- firmware/erase/program operations: 0;
- abort/repair commands: 0;
- automatic retries: 0.

**STOP: request separate user authorisation for exactly one activation attempt.**

## Stage 4I.12V.1 — surgical evidence-path correction HOLD

Date: 2026-10-30

**HOLD.** The host filesystem defect is corrected and its regression passes,
but the new no-retry physical session stopped on a bounded CPCU1 response
timeout before the complete destination, catalogues and legacy payload were
captured. No activation or physical write operation was issued.

### Host correction

New host-only files:

- `tools/stage4i12v_readback.py`;
- `tests/stage4i12v_evidence_path_test.py`.

The capture tool uses `Path.mkdir(parents=True, exist_ok=True)`, refuses a
non-empty evidence directory, writes and reads a binary probe, removes only
that probe, and performs all of those checks before importing PySerial or
opening COM11. Every completed 4 KiB window is compared against its exact
expected bytes, saved, read back and hashed before the next window begins.
The underlying CPCU1 operations remain the validated exact 64-byte reads and
have no retry path.

Focused verification:

```text
PASS: evidence directory creation, probe cleanup and binary save/readback
```

Python syntax compilation also passed. The exact real capture destination was
then created before device access:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer\artifacts\stage4i12v1-readback-20261030-1`

It existed, contained zero entries, passed a second independent seven-byte
binary write/read comparison, and the temporary probe was deleted before
COM11 was opened.

### Device preflight

Windows exposed exactly one matching composite target and one COM11 interface:

- USB `2E8A:0009`;
- serial `<device-serial>`;
- firmware `0.4.23-stage4j3migrationfix`;
- flash `EF-40-18`, 16,777,216 bytes;
- migration `verified`, received 179,360, error none;
- upload idle.

The approved Stage 4I.7 pack and Stage 4I.10 backup were hash-validated by the
capture tool before it opened the serial port. The pack remained unchanged at
SHA-256 `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`.

### Persisted read-only evidence

Twenty-five complete destination windows were received, compared, saved and
read back successfully:

- physical range: `0x43B000–0x453FFF`;
- bytes: 102,400;
- files: `destination-000-43b000-43bfff.bin` through
  `destination-024-453000-453fff.bin` in the evidence directory;
- concatenated SHA-256:
  `d41ec93a5449358823cc8a0ce622089a8cf76521653f98618ebe125f42a91789`;
- expected staged-prefix SHA-256: identical;
- byte comparison: PASS.

Each individual evidence file is 4,096 bytes and retains its own SHA-256 in
the filesystem evidence. This proves the captured prefix, including both
intentionally erased 256-byte activation headers, matches the approved staged
representation.

### Stop condition

The next destination window was `0x454000–0x454FFF`. One of its underlying
64-byte requests did not produce an accepted protocol response before the
bounded deadline:

```text
RuntimeError: timed out waiting for CPC Explorer protocol response
```

Because the lower-level timeout exception did not include the loop index, the
specific 64-byte sub-offset inside that 4 KiB window is not available. No file
for the incomplete window was created. The command stopped immediately and
was not restarted.

### Gates and non-actions

| Gate | Result |
|---|---|
| Evidence-directory correction | PASS |
| Focused filesystem regression | PASS |
| Exact device/firmware/flash identity | PASS |
| Verified transaction state | PASS |
| Persisted staged prefix | PASS — 102,400 exact bytes |
| Complete 180,224-byte destination | **HOLD — timeout in next window** |
| Catalogue structural validation | **HOLD — not reached** |
| Legacy payload preservation | **HOLD — not reached** |
| Ready for activation | **HOLD** |

- restaging: 0;
- activation: 0;
- firmware operations: 0;
- erase/program operations: 0;
- migration abort/repair: 0;
- automatic read retries: 0.

The transaction's last confirmed physical state remains `verified`, received
179,360, error none. Activation remains prohibited because complete independent
destination, catalogue and legacy-payload evidence is still missing.

## Stage 4I.12V — read-only verified-transaction recovery HOLD

Date: 2026-10-30

**HOLD.** The existing transaction remains verified and no physical write was
issued, but the mandatory independent evidence set was not completed. Per the
no-retry instruction, the read-only capture was not restarted and activation
was not attempted.

### Device and transaction state

The new read-only session positively identified:

- USB `2E8A:0009`;
- serial `<device-serial>`;
- COM11;
- firmware `0.4.23-stage4j3migrationfix`;
- flash `EF-40-18`, 16,777,216 bytes;
- upload idle, received 0;
- migration `verified`, received 179,360, error none;
- visible legacy menu: Locomotive BASIC, Knight Tyme, Manic Miner and Dizzy.

The same values were returned again at the start of the controlled capture.
No reset, re-enumeration or device identity change was observed.

### Expected pre-activation flash state

Source review and the completed staging response establish:

- catalogue reservation `0x400000–0x407FFF`: intentionally republished with
  the original Manic Miner and Dizzy descriptors, later generations, and
  high-water `0x67000`;
- legacy payload `0x408000–0x43AFFF`: must remain byte-identical to the
  Stage 4I.10 backup;
- CPXPCK2 reservation `0x43B000–0x466FFF`: staged data plus erased tail;
- activation header A `0x43B000–0x43B0FF`: intentionally erased;
- activation header B `0x43C000–0x43C0FF`: intentionally erased.

The successful staging command had already compared all 179,360 bytes against
the protocol-defined staging image and verified CRC `ec7fd1e9`. The prior
additional session also completed a full destination read in memory before its
later legacy read failed, but deferred comparison/evidence persistence meant
that it did not produce a reusable destination capture.

### Bounded-window audit design

The recovery used the existing strict 64-byte CPCU1 reads. It was designed to:

- group them into separately persisted 4 KiB windows;
- verify every window's exact offset, length and bytes before saving it;
- compare destination windows against the approved staged representation;
- compare only legacy payload `0x408000–0x43AFFF` against Stage 4I.10;
- parse both intentionally modified catalogue slots structurally;
- require exactly Manic Miner and Dizzy, high-water `0x67000`, valid CRCs,
  non-overlapping allocations, erased activation headers and erased tail;
- perform no retries.

### Host setup failure and stop

The PowerShell setup used `New-Item -LiteralPath`, but this environment's
`New-Item` command rejected that parameter:

```text
A parameter cannot be found that matches parameter name 'LiteralPath'.
```

The script subsequently opened COM11, re-identified the correct device,
confirmed `state=verified received=179360 error=none`, and completed the first
4,096-byte destination window (`0x43B000–0x43BFFF`) with exact 64-byte protocol
framing. Persisting that already-read window then failed because the evidence
directory did not exist:

```text
FileNotFoundError: ...\Stage4I12V-20261030\destination-000-43b000-43bfff.bin
```

This is a proven host evidence-path setup defect, not a device response,
content, checksum or flash failure. Nevertheless, restarting the read would be
a retry. The session therefore stopped immediately as required.

### Gates and non-actions

| Gate | Result |
|---|---|
| Exact device identity | PASS |
| Firmware/flash identity | PASS |
| Upload idle | PASS |
| Verified migration state | PASS |
| Expected flash-state analysis | PASS |
| Complete staged destination evidence | **HOLD — not persisted** |
| Complete legacy payload comparison | **HOLD — not attempted** |
| Catalogue structural validation | **HOLD — not attempted** |
| Activation headers readback | **HOLD — incomplete evidence** |
| USB stability during performed reads | PASS |
| Ready for activation | **HOLD** |

- staging attempts in Stage 4I.12V: 0;
- activation attempts: 0;
- erase/program operations: 0;
- migration abort/repair commands: 0;
- firmware operations: 0;
- automatic retries: 0.

The exact missing evidence is a complete, persisted and validated destination,
catalogue and legacy-payload readback. A future read-only recovery attempt must
be separately authorised. It must create and validate its evidence destination
before opening COM11 and must not restage or activate the existing transaction.

## New controlled installation attempt on Stage 4J.3 — HOLD after staging

Date: 2026-10-30

The read-only preflight passed and the exact approved Stage 4I.7 pack was staged
once. Device-side verification passed and the migration transaction reached the
durable `verified` state. Activation was **not** attempted because the mandatory
independent read-only verification session later received one newline-terminated
short response while reading the legacy region.

### Result by gate

| Gate | Result |
|---|---|
| Preflight | **PASS** |
| Staging | **PASS — one attempt** |
| Device-side staged verification | **PASS — CRC `ec7fd1e9`, state `verified`** |
| Independent staged/legacy readback | **HOLD — strict 64-byte legacy read failed at one chunk** |
| Activation | **NOT ATTEMPTED** |
| Final catalogue | **NOT ACTIVATED; last confirmed visible catalogue remains the four legacy entries** |
| Device stability | **HOLD — USB stayed enumerated, but one completed response was short** |

### Preflight evidence

- USB: `2E8A:0009`, exactly one composite device;
- serial: `<device-serial>`;
- port: `COM11`, available to four consecutive exclusive read-only sessions;
- firmware: `0.4.23-stage4j3migrationfix`;
- flash: `EF-40-18`, 16,777,216 bytes;
- upload: `idle`, received 0;
- migration: `legacy`, received 0, error `none`;
- visible entries: Locomotive BASIC, Knight Tyme, Manic Miner, Dizzy;
- all three 16 MiB recovery images: read-only, 16,777,216 bytes, SHA-256
  `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`;
- Stage 4J.4R legacy evidence: 241,664 bytes, SHA-256
  `88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`,
  byte-identical to the Stage 4I.10 backup;
- Stage 4J.4R destination evidence: 180,224 erased bytes, SHA-256
  `6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`;
- corrected migration precondition, transport and backup validation tests: 3/3 PASS.

The approved pack was exactly 179,360 bytes with SHA-256
`27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`,
CRC32 `ec7fd1e9`, two valid CPXPCK2 v2 headers, records CRC `1f5e3acb`,
payload CRC `4cc05874`, and six valid 65,792-byte decompressions. The six
records were Columns, Dizzy, Fruity Frank, Knight Tyme, Manic Miner and Thrust.
Fruity Frank retained compatibility status 2 (hidden).

The firmware file used only as the staging identity proof was
`build-rp2350-stage4j3-migration-fix/cpc_explorer_migration_catalog_fix_exp.uf2`,
SHA-256 `fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53`.
No UF2 was installed in this stage.

### Exact commands

Dry run:

```powershell
py tools/cpc_game.py stage-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4j3-migration-fix/cpc_explorer_migration_catalog_fix_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53
```

Result: `DRY RUN ONLY`; no serial port opened and no write attempted.

Single staging attempt:

```powershell
py tools/cpc_game.py stage-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4j3-migration-fix/cpc_explorer_migration_catalog_fix_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53 `
  --confirm STAGE --execute --port COM11
```

Device responses:

```text
CPCU1 READY fw=0.4.23-stage4j3migrationfix flash=EF-40-18 capacity=16777216 entries=4
CPCU1 MIGRATION LAYOUT flash=16777216 firmware=000000-3fffff catalog_a=400000-403fff catalog_b=404000-407fff legacy=408000-43afff pack=43b000-466fff guard=ff0000-ffffff
CPCU1 MIGRATION BEGIN OK offset=0043b000 bytes=179360
CPCU1 MIGRATION VERIFIED crc=ec7fd1e9
CPCU1 MIGRATION STATUS state=verified received=179360 error=none
STAGED AND VERIFIED: catalogue activation was not requested
```

The staging command's own bounded readback compared all 179,360 staged bytes
against the protocol-defined staging image and passed. Transactional staging
deliberately leaves the two 256-byte CPXPCK2 header pages erased until activation;
therefore the staged representation is not expected to have the final pack SHA-256.

### Flash writes performed

Staging attempt count: **1**. Activation attempt count: **0**. Automatic retries:
**0**.

The existing protocol performed only its defined staging writes:

- both CPCLIB1 catalogue slots `0x400000–0x407FFF` were transactionally
  republished with the same two descriptors and the reserved high-water mark;
- CPXPCK2 reservation `0x43B000–0x466FFF` was erased;
- staged data pages were programmed at `0x43B100–0x43BFFF` and
  `0x43C100–0x466CFF`;
- header pages `0x43B000–0x43B0FF` and `0x43C000–0x43C0FF` were not programmed;
- no firmware, E10 guard, unrelated game allocation or post-reservation region
  was targeted.

### Independent verification failure

A new read-only session re-identified the same firmware and observed:

```text
CPCU1 MIGRATION STATUS state=verified received=179360 error=none
```

It then completed strict 64-byte framing across the full 180,224-byte destination
in memory and began the 241,664-byte legacy read. Before the deferred comparisons
and evidence-file writes could run, the device returned a newline-terminated but
short legacy response:

```text
migration readback length mismatch at chunk=3081 offset=0x430240:
expected=64 actual=50 line_bytes=129
```

The preceding 3,081 legacy chunks accounted for 197,184 bytes. The host did not
pad, truncate, accept or retry the short chunk. Because the script intentionally
wrote evidence only after both complete ranges had passed, it produced no partial
readback file. This is a transport verification failure, not evidence of a flash
content mismatch.

Per the authorised stop conditions, no status retry, activation, repair, rollback,
firmware operation, game upload or additional flash write followed.

### Exact resulting state

- approved Stage 4I.7 pack: staged once and device-verified;
- migration: last confirmed `verified`, received 179,360, error none;
- catalogue activation: not performed;
- expected authoritative menu until activation: Locomotive BASIC, Knight Tyme,
  Manic Miner and Dizzy;
- Columns and Thrust: staged but not exposed by an activated catalogue;
- Fruity Frank: hidden;
- independent readback gate: HOLD;
- overall result: **HOLD**.

A later action must be separately authorised. It must begin with read-only state
identification and must not restage the pack. Activation remains prohibited until
the staged image and preserved legacy payload receive a complete independent
verification.

## New controlled installation attempt after 4I.12R — HOLD

The complete preflight passed and exactly one staging command was issued. The
device rejected `MIGRATION BEGIN` with
`CPCU1 ERROR migration_begin_packed_catalog_state`. No pack page was accepted,
no activation command was issued, and read-only post-failure comparisons prove
that no physical flash byte changed.

### Result by gate

| Gate | Result |
|---|---|
| Preflight | **PASS** |
| Staging | **HOLD** — begin rejected before programming |
| Independent staged readback | **NOT APPLICABLE** — no staged image exists |
| Activation | **NOT ATTEMPTED** |
| Final catalogue verification | **PASS for unchanged legacy CPCLIB1** |

### Device and artifact identity

| Field | Verified value |
|---|---|
| Workspace | `%USERPROFILE%\OneDrive\Documents\CPC Explorer` |
| USB | `2E8A:0009` |
| Serial | `<device-serial>` |
| Port | COM11 |
| Firmware | `0.4.21-stage4j1qwstexp` |
| Firmware SHA-256 | `2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6` |
| Flash | `EF-40-18`, 16,777,216 bytes |
| Pack bytes | 179,360 |
| Pack SHA-256 expected/actual | `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe` / same |

All three read-only 16 MiB backup copies still have SHA-256
`c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`.
The Stage 4I.12R evidence remains 241,664 bytes, SHA-256
`88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`,
and byte-identical to the Stage 4I.10 source range.

Pack inspection passed: CPXPCK2 v2, two valid headers, six records, six valid
65,792-byte decompressions, records CRC `1f5e3acb`, payload CRC `4cc05874`, and
pack CRC `ec7fd1e9`. The fresh preflight read proved all 180,224 bytes at
`0x43b000–0x466fff` were erased.

### Exact commands

Dry run:

```powershell
py tools/cpc_game.py stage-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4j1-qwst/cpc_explorer_qwst_audio_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6
```

The dry run reported `DRY RUN ONLY: no serial port opened and no device write
attempted`.

Single staging attempt:

```powershell
py tools/cpc_game.py stage-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4j1-qwst/cpc_explorer_qwst_audio_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6 `
  --confirm STAGE --execute --port COM11
```

Response:

```text
CPCU1 READY fw=0.4.21-stage4j1qwstexp flash=EF-40-18 capacity=16777216 entries=4
CPCU1 MIGRATION LAYOUT flash=16777216 firmware=000000-3fffff catalog_a=400000-403fff catalog_b=404000-407fff legacy=408000-43afff pack=43b000-466fff guard=ff0000-ffffff
CPCU1 ERROR migration_begin_packed_catalog_state
```

Attempt counts:

- staging: **1**;
- staging retries: **0**;
- activation: **0**;
- rollback or repair: **0**.

### Proven root cause

`Transport::begin()` calls `Installer::reserve_for_packed_library()` before
pack erase or page programming. That reservation currently requires the active
CPCLIB1 catalogue to declare exactly three descriptors and later requires IDs
`knight_tyme`, `manic_miner`, and `dizzy`.

The physical library is valid but declares two external-flash descriptors:
Manic Miner and Dizzy. Locomotive BASIC and Knight Tyme are built-in launcher
entries, which is why the protocol correctly reports four visible entries
while CPCLIB1 contains two descriptors. The `declared_count() != 3` condition
therefore returns `packed_catalog_state` before `write_catalog()` or
`flash_.erase()` can execute.

This is a firmware migration precondition defect, not corrupt physical data,
a pack mismatch, or a transport failure. Correcting it requires a separately
reviewed firmware change and separate deployment approval; this installation
stage does not authorise that work.

### Exact post-failure flash state

Read-only evidence files:

- `%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I12-install-20261030\post-failed-begin-legacy-400000-43afff.bin`
- `%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I12-install-20261030\post-failed-begin-destination-43b000-466fff.bin`

Results:

- legacy `0x400000–0x43afff`: 241,664 bytes, SHA-256
  `88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec`,
  byte-identical to Stage 4I.10;
- destination `0x43b000–0x466fff`: 180,224 bytes, SHA-256
  `6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`,
  every byte `0xff`;
- flash ranges written: **none**;
- migration state: `legacy`, received 0, error `packed_catalog_state`;
- upload: idle;
- visible catalogue remains Locomotive BASIC, Knight Tyme, Manic Miner, Dizzy;
- Columns and Thrust are not installed;
- Fruity Frank remains unavailable;
- remaining flash storage is unchanged.

Overall result: **HOLD**. Do not retry staging or activate this device with the
current firmware. The smallest next action is a host-only correction of the
migration reservation precondition so it recognises the real two-descriptor
CPCLIB1 state while still independently requiring built-in Knight Tyme. Any
candidate firmware requires its own host verification and deployment approval.

## Stage 4I.12R read-only transport recovery — PASS

The blocked CPCU1 legacy-readback gate is cleared. This was a new diagnostic
session, not an automatic continuation of the failed installation attempt.
No staging, activation, erase, firmware operation, or catalogue write occurred.

### Root cause

The old `read_protocol_line()` called PySerial `readline()` once and stripped
whatever it returned. PySerial is permitted to return a non-empty partial line
when its timeout expires. If that fragment already began with
`CPCU1 MIGRATION DATA`, the old function accepted it as a complete response.
`read_migration_range()` then decoded the incomplete hexadecimal payload and
raised the generic `migration readback length mismatch` exception.

The old exception did not include chunk, offset, expected bytes, actual bytes,
or the partial line, so the exact actual byte count from the original failed
session cannot be recovered. The proven failure class is acceptance of a
timeout-fragmented line, rather than a firmware frame-size or flash-data defect.

### Surgical host correction

The receiver now:

- assembles permitted fragments of one response until newline;
- uses one bounded deadline (twice the configured serial timeout, clamped to
  2–10 seconds);
- never sends a second device READ for additional fragments of that response;
- rejects responses exceeding 4,095 bytes;
- rejects timeouts, wrong offsets, corrupt hexadecimal data, and short decoded
  payloads without padding or truncation;
- reports the exact chunk, physical offset, expected/actual decoded byte count,
  and line length for any future length failure.

Only the host receiver and diagnostics changed. Firmware and the physical
migration protocol were not modified.

Focused host tests passed for fragmented responses, a single READ command
across fragments, short responses, wrong offsets, corrupt hex, interrupted
responses, empty timeouts, interleaved telemetry, packed migration, flash
backup validation, and the game importer: **4/4 CTest groups PASS** plus the
standalone transport test PASS.

### Physical identity and state

| Field | Observed |
|---|---|
| USB | `2E8A:0009`, status OK |
| Serial | `<device-serial>` |
| Port | COM11 |
| Firmware | `0.4.21-stage4j1qwstexp` |
| Flash | `EF-40-18`, 16,777,216 bytes |
| Upload | idle, received 0 |
| Migration | legacy, received 0, error none |
| Entries | 4 |

COM11 had no telemetry recorder or serial monitor owner. Three enumeration
samples before the read and a status query after it remained stable.

### One complete CPCU1 transport read

The new session made exactly one complete legacy-region read through the CPCU1
migration transport:

| Field | Result |
|---|---|
| Start | `0x400000` |
| End inclusive | `0x43afff` |
| Bytes requested | 241,664 |
| Bytes received | 241,664 |
| SHA-256 | `88e56b1e8746e914147642e6ad33d82fc8f1f0422feba87c420f10dd016691ec` |
| Stage 4I.10 byte comparison | exact match |

Evidence:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I12R-20261030\legacy-400000-43afff.bin`

The legacy CPCLIB1 region is unchanged and remains authoritative.

### Recovery result

- Read-only transport recovery: **PASS**
- USB transport stability: **PASS**
- Physical flash writes: **0**
- Staging attempts: **0**
- Activation attempts: **0**
- Ready for a **new separately controlled Stage 4I.12 installation attempt**
- Current operation stops before staging as required

## Current authorised attempt — 2026-10-30

**HOLD — mandatory fresh legacy readback failed before staging. No physical
migration write was started.**

The user authorised the exact Stage 4I.7 CPXPCK2 pack for staging and
activation on the physically installed `0.4.21-stage4j1qwstexp` firmware. The
preflight stopped before either write command because the fresh read-only
`backup-library` operation returned `migration readback length mismatch`.
Under the fail-closed runbook this is an ambiguous USB/readback condition and
must not be retried or bypassed in the same attempt.

### Gates completed before the stop

| Gate | Result |
|---|---|
| USB identity | PASS — `2E8A:0009`, serial `<device-serial>`, COM11 |
| Firmware | PASS — `0.4.21-stage4j1qwstexp` |
| Flash | PASS — `EF-40-18`, 16,777,216 bytes |
| Upload/migration | PASS — upload idle; `state=legacy received=0 error=none` |
| Existing entries | PASS — BASIC, Knight Tyme, Manic Miner, Dizzy |
| Recovery images | PASS — three read-only 16 MiB copies, each SHA-256 `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1` |
| Pack identity | PASS — 179,360 bytes, SHA-256 `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe` |
| Pack inspection | PASS — CPXPCK2 v2, two valid headers, six 65,792-byte decompressions, expected CRCs |
| Firmware/pack audit | PASS — application UF2 remains inside the first 4 MiB plus the exact E10 exception; destination and protected ranges pass |
| Destination erased | PASS — two read-only captures of `0x43b000–0x466fff`; all 180,224 bytes are `0xff` and byte-identical to Stage 4I.10 |
| COM ownership | PASS — no telemetry recorder or serial monitor held COM11 |
| USB enumeration | PASS — three stable identity samples before the readback failure |
| Fresh legacy readback | **FAIL/HOLD — migration readback length mismatch** |

The first destination-erasure predicate initially printed false because the
host command compared against an incorrectly escaped byte literal. The raw
capture itself contained zero non-`0xff` bytes. A corrected `bytes([255])`
check and an independent second capture both passed; this was a host predicate
error, not a flash discrepancy.

The destination capture is preserved at:

`%USERPROFILE%\OneDrive\Documents\CPC Explorer Backups\Stage4I12-preflight-20261030\destination-43b000-466fff.bin`

Its SHA-256 is
`6748fc8cade005ac111394bc875bb3c64580cd82559d30836df7a9c644ccc0d6`.

### Stage 4J.1 protocol compatibility

The installed firmware was built with the complete Stage 4I.8 migration path,
and its physical read-only `migration-status` and bounded migration reads both
responded. The Windows utility's fail-closed identity allowlist was extended
only for the exact Stage 4J.1 identity and UF2 SHA-256
`2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6`.
No protocol command, flash address, CRC, staging order, or write implementation
was changed. Python compilation, importer tests, packed migration tests, backup
validator tests, and game importer tests passed.

### Exact resulting physical state

- CPXPCK2 staging attempt: **0**
- CPXPCK2 activation attempt: **0**
- Physical bytes written by Stage 4I.12: **0**
- Last complete device status: four legacy entries, upload idle, migration
  `legacy`, received 0, error none
- Staging destination: erased and unchanged from Stage 4I.10
- Catalogue authority: legacy CPCLIB1
- Visible list: BASIC, Knight Tyme, Manic Miner, Dizzy
- Columns/Thrust: not installed
- Fruity Frank: not installed and not visible
- Remaining storage: unchanged; post-activation value is not applicable

The existing physical result for Knight Tyme remains launch/music PASS. Its
idle-disappearance defect and Qw/ST face-button orientation issue are recorded
separately and were not modified during this installation attempt.

### Required next action

Keep the device in normal application mode. A new, explicitly controlled
read-only attempt must first reproduce a complete 241,664-byte legacy-region
readback and compare it byte-for-byte with Stage 4I.10. Only a clean fresh
preflight may request new staging authority. Do not activate, retry staging,
flash firmware, or restore a backup automatically.

## Overall result

**HOLD — Stage 4I.11 physical acceptance is incomplete. No migration write was
started.**

The Stage 4I.12 authorisation is explicitly conditional on complete Stage
4I.11 gameplay and telemetry acceptance. That prerequisite is not presently
satisfied, so Tasks 2–7 were not entered.

## Requested report fields

| Field | Result |
|---|---|
| Device identity | Previously verified serial `<device-serial>`; not re-accessed in this HOLD stage |
| Firmware | Previously verified `0.4.20-stage4i8migrationexp`; no firmware deployment performed |
| Stage 4I.11 acceptance | **INCOMPLETE / HOLD** |
| Backup verification | Previously secured two 16,777,216-byte copies with SHA-256 `c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1`; not modified |
| Pack SHA-256 | Host-preflight value `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`; physical staging not attempted |
| Staging result | **NOT ATTEMPTED** |
| Independent readback | **NOT APPLICABLE — no staging write** |
| Activation result | **NOT ATTEMPTED** |
| Catalogue integrity | Legacy catalogue was previously byte-identical to the Stage 4I.10 backup; no Stage 4I.12 catalogue mutation |
| Game count | Previously four legacy entries; no post-migration count because migration did not occur |
| Locomotive BASIC | Physical Stage 4I.11 launch/input result missing |
| Knight Tyme | Physical Stage 4I.11 launch/controls/audio result missing |
| Manic Miner | **PASS reported by user — gameplay and music working** |
| Dizzy | Serial evidence shows it launched; physical display/control/audio verdict missing |
| Columns | Not installed or tested |
| Thrust | Not installed or tested |
| AY audio | Manic Miner audible music passed; captured Dizzy renderer had zero underruns/overruns but no complete four-game physical verdict |
| CPC performance | Captured Dizzy settled around 80.1–82.5%; all CPC 20 ms deadlines missed; this is not Manic Miner telemetry |
| Stack telemetry | Core-1 high-water 712 bytes used of 2,048 during captured Dizzy run |
| Protected flash integrity | Last verified legacy range was byte-identical; no Stage 4I.12 write occurred |
| Overall PASS/HOLD | **HOLD** |

## Missing Stage 4I.11 evidence

The conditional gate requires all of the following. Current evidence is:

- BASIC launch: **missing**;
- Knight Tyme launch and controls: **missing**;
- Manic Miner gameplay and music: **PASS**;
- Dizzy launch: serial evidence present;
- Dizzy controls/display: **missing user verdict**;
- no unexpected crashes or resets: **not established for the complete test**;
- CPCPERF: reviewed for the captured Dizzy run;
- CPCAUDIO: reviewed for the captured Dizzy run;
- CPC deadline misses: every captured emulation slice missed 20 ms;
- audio deadline misses: zero;
- audio underruns/overruns: zero/zero;
- runtime stack high-water: 712 bytes used;
- final USB state: the capture ended with `ClearCommError`, after which no
  `2E8A:0009` Explorer was detected. Cause was not established.

The missing physical observations cannot be inferred from host tests, earlier
firmware stages, catalogue presence, or telemetry from a different game.

## Pack readiness already established offline

The preceding host-only preflight remains valid evidence but does not override
the failed conditional gate:

- exact pack size: 179,360 bytes;
- exact pack SHA-256:
  `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`;
- CPXPCK2 v2, two valid headers, six structurally valid records;
- five intended visible games: Knight Tyme, Manic Miner, Dizzy, Columns,
  Thrust;
- Fruity Frank remains the hidden record;
- authorised staging extent: `0x43b000–0x466fff`;
- complete pack inspection, decompression, CRC checks, address audit, and
  interruption simulation passed;
- `stage-library` was exercised only as a dry run with no port and no execute
  flag.

## Required next action

Reconnect the same Explorer normally and complete the missing Stage 4I.11
physical checks before requesting migration again:

1. confirm serial `<device-serial>`, firmware
   `0.4.20-stage4i8migrationexp`, EF-40-18/16 MiB, legacy state, and four
   entries;
2. launch and verify Locomotive BASIC;
3. launch Knight Tyme and verify its available controls, display, stability,
   and AY music after the established menu sequence;
4. launch Dizzy and verify display, Z/X/Space controls, gameplay, expected
   mostly-silent audio behaviour, and stability;
5. explicitly report whether any freeze, reset, USB loss, or audio distortion
   occurred.

After those results pass, a fresh Stage 4I.12 pre-write gate must re-identify
the device and recheck both recovery images, current migration state, exact
pack, flash layout, and legacy-region readback. Physical staging and activation
must still be separate transactional steps with verification between them.

## Explicit non-actions

- Firmware write: **NONE**
- CPXPCK2 staging: **NONE**
- CPXPCK2 activation: **NONE**
- Catalogue mutation: **NONE**
- Game upload/removal: **NONE**
- Automatic rollback: **NONE**
