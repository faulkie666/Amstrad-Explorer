# Stage 4I.8 — Safe internal flash library migration transport

## Result

Host verification is **PASS**. The transport, recovery model, 16 MiB flash
simulation, firmware build, SRAM gate, and UF2 policy all pass. Physical testing
has not been performed and deployment is not authorised by this stage.

The experimental firmware is:

- identity: `0.4.20-stage4i8migrationexp`
- UF2: `build-rp2350-stage4i8-migration/cpc_explorer_flash_migration_exp.uf2`
- bytes: 733,184
- SHA-256: `28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114`

The only accepted pack is:

- file: `artifacts/stage4i7-internal-library/library.cpxpack`
- bytes: 179,360
- CRC-32: `ec7fd1e9`
- SHA-256: `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`
- format: CPXPCK2, six records, five launchable records

## Flash ownership ledger

All addresses below are byte offsets in the 16 MiB flash. Erase sectors are
4,096 bytes and program pages are 256 bytes.

| Range | Size | Owner | Migration access |
|---|---:|---|---|
| `000000–3fffff` | 4 MiB | application reservation | never read or written by migration |
| `400000–403fff` | 16 KiB | legacy CPCLIB1 catalogue A | read; transactional high-water publication only |
| `404000–407fff` | 16 KiB | legacy CPCLIB1 catalogue B | read; transactional high-water publication only |
| `408000–43afff` | 204 KiB | three existing snapshot allocations | read-only; never erased or programmed |
| `43b000–466c9f` | 179,360 B | exact CPXPCK2 image | bounded erase/program/readback |
| `466ca0–466fff` | 864 B | padding in last reserved sector | erased, otherwise unused |
| `467000–feffff` | 12,095,488 B | unallocated game-storage space | untouched by this migration |
| `ff0000–ffffff` | 64 KiB | permanent RP2350-E10 guard | never accessed |

The proposed image starts and ends on proven boundaries: `43b000` is a sector
and page boundary; the exact image ends at `466ca0`; its erase reservation ends
at the next sector boundary, `467000`. The firmware rejects all other manifest
offsets, lengths, CRCs and SHA-256 identities. Checked subtraction is used for
every range test so integer wrap cannot turn an invalid range into a valid one.

The read-only migration command is deliberately not arbitrary: it permits at
most 128 bytes per command and only inside `400000–466fff`. This is sufficient
to back up the two catalogues, all current allocations, and to verify the new
pack. It cannot read application firmware, future free storage, or the E10
guard.

## CPCU1 protocol extension

The existing version-1 frame remains `CPCU`, protocol version, command, payload
length, sequence, payload CRC-32, then payload. The 1,024-byte frame limit and
existing `HELLO`, `STATUS`, `LIST`, upload and removal behaviour are unchanged.

| Command | Value | Payload | Effect |
|---|---:|---|---|
| `MIGRATION_STATUS` | 8 | none | state, received byte count, last error |
| `MIGRATION_LAYOUT` | 9 | none | fixed flash ownership ranges |
| `MIGRATION_BEGIN` | 10 | `MIGRATE\0`, manifest v1, exact offset/size/CRC/SHA | validates, reserves, erases exact staging sectors |
| `MIGRATION_PAGE` | 11 | sequential 256-byte page; final page is 160 bytes | programs and immediately verifies one bounded page |
| `MIGRATION_VERIFY` | 12 | none | stream CRC plus complete staging readback CRC |
| `MIGRATION_ACTIVATE` | 13 | none | publishes header B, validates it, then publishes header A |
| `MIGRATION_ABORT` | 14 | none | abandons an unfinished transfer; never erases legacy data |
| `MIGRATION_READ` | 15 | physical offset and length | read-only, maximum 128 bytes, fixed whitelist only |

Mutating commands require a recent `HELLO`, the literal `MIGRATE` confirmation,
the exact manifest, supported JEDEC identity `EF-40-18`, and capacity 16,777,216.
Every page independently checks sequence, exact expected length, destination,
and readback. Duplicate begin/activate, out-of-order pages, malformed frames,
unknown command/version, overflow, or an unexpected state fail closed. The host
does not automatically retry a timeout or ambiguous result.

### Host commands

Read-only status:

```powershell
py tools/cpc_game.py migration-status --port COMxx
```

Read-only backup of both catalogue copies and every existing allocation:

```powershell
py tools/cpc_game.py backup-library --port COMxx --output artifacts/physical-backup/legacy-library.bin
```

Migration validation defaults to dry-run and does not import pyserial or open a
serial port:

```powershell
py tools/cpc_game.py migrate-library `
  artifacts/stage4i7-internal-library/library.cpxpack `
  --firmware build-rp2350-stage4i8-migration/cpc_explorer_flash_migration_exp.uf2 `
  --expect-pack-sha256 27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe `
  --expect-firmware-sha256 28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114
```

A future physical write requires all of `--port COMxx --execute --confirm
MIGRATE`, as well as separate user approval. Omitting any one of them writes
nothing.

## Transaction and recovery state machine

```text
LEGACY
  | exact manifest + both catalogues durably reserve to 0x67000
  v
RECEIVING --abort/reboot--> RESERVED-INCOMPLETE
  | all pages + stream/readback CRC
  v
VERIFIED --abort/reboot--> RESERVED-INCOMPLETE
  | write+mount header B, then write+verify header A
  v
ACTIVE

Any uncertain erase/program/readback result -> AMBIGUOUS -> inspect read-only;
never retry automatically.
```

The two legacy catalogues keep the three installed descriptors and their
payloads. Their allocator high-water value is transactionally advanced to
relative `0x67000`, preventing Stage 4B.1/4I.7 firmware from allocating over the
reserved pack after rollback. No old payload sector is erased.

The CPXPCK2 header pages at pack offsets 0 and `0x1000` are retained in the
existing 16 KiB installer scratch buffer and are not written while receiving.
Thus an incomplete transfer cannot mount as a pack. Activation writes and
mount-validates header B first. Header B is the atomic activation boundary; a
loss before it leaves `RESERVED-INCOMPLETE`, while a loss after it leaves a
mountable `ACTIVE` image even if header A was not completed. Legacy CPCLIB1
catalogues remain valid in either case, providing rollback compatibility.

## Multicore and XIP safety

The migration reuses `PicoFlashBackend`, which performs erase/program through
the Pico SDK flash-safe execution path. The established core-1 lockout victim
registration is retained. Migration commands are accepted only by the launcher:
no emulated game is running, audio/render activity is idle, and core 1 has
already reached the established safe state. Program data lives in the existing
SRAM scratch/page buffers for the duration of each operation. No DMA source is
the XIP range being modified. A failed flash-safe call is treated as ambiguous
and cannot be retried automatically.

## Host simulation and failure injection

The test uses an actual 16,777,216-byte simulated flash, 4 KiB erase geometry,
256-byte program geometry, the real uploader/installer to create the three
legacy allocations, and the real CPXPCK2 pack. It covers:

- valid reservation, transfer, independent staging readback, activation and reboot;
- CPCU1 decoding plus corrupt CRC, unknown protocol version and unknown command;
- wrong JEDEC ID, capacity, offset, size and SHA-256;
- duplicate begin, out-of-order pages and incorrect page sizes;
- corrupt image CRC and post-program readback corruption;
- flash-safe/lockout failure;
- power loss in catalogue publication, staging erase, mid-programming,
  pre-activation, header-B publication and header-A publication;
- recovery as legacy, reserved-incomplete or active at the exact atomic boundary;
- continued visibility of the three legacy descriptors after every interruption;
- bounded readback including underflow/overflow rejection.

All rejected manifests perform zero flash operations and preserve the entire old
range through `43afff` byte-for-byte. For interrupted operations, firmware
`000000–3fffff`, existing payloads `408000–43afff`, and the E10 guard
`ff0000–ffffff` remain byte-identical. Catalogue sectors may differ only when a
deliberate, CRC/commit-validated high-water reservation has been published; at
least one valid legacy catalogue remains and the simulator remounts the same
three games.

The complete host suite result is **23/23 PASS**. The new migration simulation
is one test in that suite and includes all failure cases above.

## Firmware, linker and UF2 audit

- linked binary: 366,256 bytes (`0x596b0`)
- first-4-MiB firmware margin: 3,828,048 bytes
- main SRAM headroom: 119,040 bytes (`0x1d100`)
- required minimum: 118,784 bytes
- gate margin: 256 bytes
- heap reservation: 2,048 bytes
- core-0 stack: 2,048 bytes in scratch Y
- core-1 stack: 2,048 bytes in scratch X
- Z80 SRAM routine: address `0x20001280`, 76,212 bytes
- Z80 unrelocated section SHA-256 in Stage 4I.7 and Stage 4I.8:
  `da89f1bb3ce534566a3c1b6dec4f9b7bb296212c8fb1ae61abc6b5e84fbf450b`

The Z80 section is byte-identical before link relocation; its implementation and
size are unchanged. The migration object adds no second catalogue buffer and
reuses the installer's existing 16 KiB scratch allocation.

UF2 validation:

- 1,432 physical blocks
- 1,431 ordinary application blocks, family `0xe48bff59`
- application targets `0x10000000–0x100596ff`, entirely inside first 4 MiB
- exactly one E10 block, family `0xe48bff57`, flags `0x0000a000`
- E10 target `0x10ffff00`, extension `0x9957e304`, payload 256 bytes of `0xef`
- no catalogue, game-storage, or other unexpected target

Preserved artifact hashes were rechecked:

- Stage 4H.4: `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0`
- Stage 4H.5D: `27dbc34b684aff421b4dce3ddc2384fb822ff2a27e1c28a837be36c3c3787d35`
- Stage 4I.7: `f751ab24cd3ac395f065252e380303327a485118ede8915c5381a1d7673ff60e`

## Physical acceptance procedure — not executed

This is a future procedure and requires separate explicit approval.

1. Recalculate the Stage 4I.8 UF2 and CPXPCK2 hashes shown above. Validate every
   UF2 block and the single E10 exception.
2. Verify the current application identity, `EF-40-18`, 16 MiB capacity, idle
   upload state, and exactly BASIC, Knight Tyme, Manic Miner and Dizzy. Ensure the
   launcher is displayed and no other serial client is attached.
3. Under a separately approved single-copy BOOTSEL deployment, install only the
   Stage 4I.8 UF2. Do not retry an ambiguous copy. Confirm the distinct firmware
   identity after normal enumeration.
4. Run `migration-status`. It must report `legacy` and no error.
5. Run `backup-library` twice to different files. Require both files to be
   241,664 bytes and have identical SHA-256 values. Store the files and hashes
   outside the build directory. This backs up both catalogues and all three
   existing allocations.
6. Run the migration utility without `--execute`; require exact firmware and pack
   hashes and the `DRY RUN ONLY` result.
7. Only after a new explicit write approval, run once with the exact port,
   `--execute --confirm MIGRATE`. Do not retry on timeout, disconnect or an
   ambiguous result.
8. The utility must verify every staged byte before activation, then read back the
   active pack and verify its exact SHA-256 independently.
9. Reboot normally. Verify migration state `active`, BASIC access, and Knight
   Tyme, Manic Miner and Dizzy launch/control regressions.
10. Test Columns and Thrust launch, controls and persistence. Confirm genuine AY
    output with Knight Tyme and Manic Miner and inspect CPCPERF/CPCAUDIO plus both
    core stack-watermark telemetry.
11. On any failure, stop writes. Read status and the bounded range only. If the
    state is legacy or reserved-incomplete, the original catalogue remains the
    recovery path. If active, verify the pack hash before deciding anything.
12. Rollback, if explicitly approved, is the preserved Stage 4I.7 UF2. Its
    CPCLIB1 reader sees the preserved descriptors and reserved high-water value;
    it must not reclaim the pack sectors. Never erase the whole chip or restore a
    catalogue without first comparing the saved hashes and generations.

## Outstanding physical risks

- Real RP2350 flash latency, USB disconnect behaviour and power-loss timing have
  only been simulated, not physically measured.
- The SRAM gate margin is only 256 bytes; physical stack-watermark evidence is
  mandatory before production promotion.
- The transport intentionally supports one exact pack. A future pack requires a
  reviewed firmware/manifest update rather than a generic writer.
- Physical rollback and power-cut testing remain separately authorised work.

## Final status

- **HOST VERIFICATION:** PASS (23/23)
- **TRANSPORT SIMULATION:** PASS
- **RECOVERY/ROLLBACK TESTS:** PASS (host simulation)
- **PROTECTED FLASH INTEGRITY:** PASS (byte-identity assertions)
- **FIRMWARE BUILD:** PASS
- **SRAM SAFETY:** PASS, 119,040 bytes headroom
- **PHYSICAL TESTING:** NOT TESTED
- **DEPLOYMENT:** NOT AUTHORISED
