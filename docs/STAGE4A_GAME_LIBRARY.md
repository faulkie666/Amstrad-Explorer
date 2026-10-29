# Stage 4A: expandable game-library design

Stage 4A adds a read-only-on-device library parser, dynamic menu enumeration,
a deterministic Windows importer, a bounded serial protocol, and simulated
transactional upload tests. The candidate firmware contains no flash erase or
program call and does not enter upload mode; hardware writes remain deliberately
disabled until the layout and recovery design pass acceptance testing.

## Flash feasibility and layout

The Explorer board definition and firmware linker both configure the external
W25Q128 device as 16 MiB. RP2350 internal ROM supplies BOOTSEL recovery; the
external flash begins with boot2 and the application image. Picotool reports the
Stage 4A image at `0x10000000..0x10031504` (201,988 bytes).

All addresses below are offsets within the external flash:

| Range | Bytes | Purpose |
| --- | ---: | --- |
| `0x000000..0x3FFFFF` | 4,194,304 | boot2, RP2350 metadata, application, and future firmware growth |
| `0x400000..0x403FFF` | 16,384 | library catalog slot A |
| `0x404000..0x407FFF` | 16,384 | library catalog slot B |
| `0x408000..0xFEFFFF` | 12,484,608 | sector-aligned snapshot allocations |
| `0xFF0000..0xFFFFFF` | 65,536 | permanent RP2350-E10 compatibility guard |

The build runs `tools/check_flash_layout.py` and fails if a firmware `.bin`
exceeds the fixed 4 MiB reservation. Current firmware therefore has a
3,992,316-byte safety margin. A 65,792-byte SNA occupies seventeen 4 KiB erase
sectors (69,632 bytes), allowing about 179 additional games by data capacity.
The version-1 catalog permits 192 descriptors, so data space is the limit.

A normal sparse UF2 contains only application addresses and should not touch
the library range. This must still be confirmed on hardware with non-copyrighted
sentinel data before enabling uploads. Existing known-good UF2 files remain the
recovery route through ROM BOOTSEL. Whole-chip erase and firmware-region writes
are never part of the proposed upload protocol.

## Versioned format

Each of two 16 KiB catalogs starts with a 64-byte `CPCLIB1` header containing
format version, generation, entry count, descriptor size, data bounds, header
CRC-32, and commit marker. The newest valid generation wins; a damaged newest
slot falls back to the other slot.

Each fixed 80-byte descriptor contains:

- 16-byte NUL-terminated game ID;
- 40-byte NUL-terminated display title;
- machine type (`1` = CPC464), RAM KiB (`64`), SNA version, and flags;
- library-relative data offset and length;
- snapshot CRC-32 and descriptor CRC-32.

Descriptors and payloads are validated independently. Unsupported or corrupt
entries are omitted without affecting Locomotive BASIC, Knight Tyme, or other
valid entries. Payload offsets must be 4 KiB aligned and remain entirely inside
the data region.

The transport file uses the `.cpcgame` extension: a deterministic 16-byte
`CPCGAME1` header, one descriptor, then one validated SNA. It contains no
timestamp or host-specific path.

## Importer

`tools/cpc_game.py` supports:

```powershell
py -3 tools/cpc_game.py import-sna game.sna `
  --id game_id --title "Game Title" --output game.cpcgame
py -3 tools/cpc_game.py inspect game.cpcgame
```

Only standalone CPC SNA versions 1, 2, or 3 with exactly a 64 KiB memory dump
are accepted. Disk, tape, 128 KiB snapshots, raw binaries, and arbitrary RAM
loads are rejected or have no import command.

The `import-knight` command invokes the compiled `cpc_snapshot_builder`, which
replays the already proven AMSDOS-checksummed `KT$.BIN`/`KTCODE.BIN`/`KTGRX.BIN`
loading recipe in the host emulator before saving and packaging a snapshot.
Other BASIC/BIN sets require an explicit, reviewed loader recipe; they are not
treated as interchangeable raw binaries. Source games, snapshots, and packages
remain ignored by Git.

## Serial upload protocol and recovery

The proposed CDC protocol is available in `src/upload_protocol.*` and in the
Windows tool's `upload` command. It requires explicit device upload mode and an
explicit host `--confirm UPLOAD`. Frames have a `CPCU` magic, protocol version,
command, sequence, payload length, and CRC-32. Commands are HELLO, BEGIN, CHUNK,
COMMIT, ABORT, and STATUS. Chunks are limited to 1,024 bytes, so no package-sized
RAM buffer is required on the device.

The flash backend to be added in the next hardware stage must:

1. stop emulator/display DMA and enter explicit upload mode;
2. select an unreferenced, 4 KiB-aligned allocation after the active high-water mark;
3. erase only complete 4 KiB sectors in the library data range;
4. program 256-byte-aligned pages using Pico SDK safe flash execution, with
   interrupts and any other core/XIP users quiesced;
5. flush/re-enable XIP, reread the payload, and verify package and snapshot CRCs;
6. write and verify the inactive catalog slot with generation + 1;
7. program the commit marker last, making the new catalog visible atomically.

If power or USB is lost before step 7, the old catalog remains active and the
unreferenced tail allocation is reusable after erase. If the newest catalog is
damaged, boot selects the older valid slot. The host can query STATUS and either
resume the expected sequence or explicitly abort. No operation is permitted
below flash offset `0x400000` or beyond `0xFEFFFF`. The final 64 KiB guard is
never a catalog, allocator, upload, or recovery target.

Stage 4A implements framing, CRC, sequence/state validation, the Windows sender,
and a simulated transactional sink. It intentionally does **not** connect a
hardware flash writer or USB command loop to the candidate firmware.

## Verification and limits

Host tests cover valid deterministic import, 128 KiB rejection, catalog
selection, valid enumeration, corrupt-payload isolation, immutable built-in
ordering, longer-list scrolling, snapshot loading, protected-range rejection,
bounded frame decoding, interrupted upload invisibility, and successful retry.

The SNA format does not reliably prove that a 64 KiB snapshot has no CPC6128 ROM
or peripheral dependency. The importer enforces structure and declared profile,
but each new title still needs a host and hardware compatibility test. Disk,
tape, 128 KiB RAM, expansion ROM, and unsupported peripheral behavior remain
out of scope.

The Stage 4A link image allocates 272 bytes of RAM vectors, 8,840 bytes of data,
289,040 bytes of BSS, a 2,048-byte heap and a 2,048-byte stack reservation:
302,248 bytes total, leaving 230,232 bytes of the configured 532,480-byte SRAM.
The library index adds 404 bytes over Stage 3 and snapshot data remains in XIP.

## Candidate

- `build-rp2350/cpc_explorer_library_e10safe.uf2`
- UF2 container: 404,992 bytes
- flash binary: 202,060 bytes
- SHA-256: `e6322765bfd8fa2b9ecbf42d4e1c94f9e0e9073ef8b42884da826ee33075b2fa`

The SDK-generated UF2 contains one exact RP2350-E10 compatibility block at
`0x10FFFF00`. It is treated as potentially actionable, so its entire containing
64 KiB is outside the writable library. `tools/validate_uf2.py` rejects any
other out-of-reservation target. This candidate was not flashed during Stage 4A.
