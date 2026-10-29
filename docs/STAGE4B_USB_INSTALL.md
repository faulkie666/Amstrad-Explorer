# Stage 4B USB game installation

Stage 4B enables transactional installation of validated CPC464 64 KiB SNA
packages while the launcher menu is visible. It does not accept arbitrary
`.BIN`, `.DSK`, or `.CDT` files. Loose BASIC/BIN folders still require an
explicit, proven loader recipe before they can be converted to a snapshot.

## Safety model

The fixed 16 MiB flash map is:

| Flash offset | Purpose | Device uploader access |
|---|---|---|
| `0x000000-0x3FFFFF` | application firmware | never writable |
| `0x400000-0x403FFF` | CPCLIB1 catalog A | inactive slot only |
| `0x404000-0x407FFF` | CPCLIB1 catalog B | inactive slot only |
| `0x408000-0xFEFFFF` | 4 KiB-aligned snapshot allocations | allocated writes only |
| `0xFF0000-0xFFFFFF` | permanent RP2350-E10 guard | never writable |

Uploads are enabled only when the runtime JEDEC probe returns the supported
Winbond W25Q128 identity `EF-40-18`, which declares 16 MiB. Every erase and
program request is range- and overflow-checked again in the physical backend.
Erase operations are 4096-byte aligned and program operations are 256-byte
aligned. Pico SDK `flash_safe_execute`, `flash_range_erase`, and
`flash_range_program` coordinate XIP, interrupts, and the other core. USB
responses are emitted only after each flash call returns.

The installer writes a new, unreferenced snapshot allocation, verifies its
complete CRC from XIP readback, then writes the inactive 16 KiB catalog. The
catalog commit marker is programmed last. Until that final page program and
catalog verification succeed, the prior generation remains authoritative.
Interrupted data is invisible and can be erased/reused by the next attempt.

Connecting USB performs no writes. A valid binary `HELLO` is required before
`BEGIN`; chunks are limited to 1024 bytes and must have consecutive sequence
numbers and valid frame CRCs. A whole-package CRC is checked before commit.
Partial frames time out after 2 seconds and transactions after 30 seconds.
Uploads are handled only in the launcher loop, never while CPC emulation runs.

## Windows commands

Install pyserial once if needed:

```powershell
py -m pip install pyserial
```

Detect the Pico SDK application (`VID 2E8A`, `PID 0009`), inspect status, and
list games:

```powershell
py tools/cpc_game.py detect
py tools/cpc_game.py status
py tools/cpc_game.py list
```

Pass `--port COM11` (or the currently assigned port) to `status`, `list`, or
`upload` if auto-detection is ambiguous. Create and inspect a deterministic
package:

```powershell
py tools/cpc_game.py import-sna game.sna --id game_id --title "Game Title" --output game.cpcgame
py tools/cpc_game.py inspect game.cpcgame
```

Upload requires an explicit confirmation token and reports acknowledged byte
progress. Success is reported only when the device's readback payload CRC
matches the package:

```powershell
py tools/cpc_game.py upload game.cpcgame --confirm UPLOAD
```

## Host verification

`cpc_library_installer_test` uses generated, non-copyrighted SNA bytes and a
simulated 16 MiB NOR flash. It checks runtime capacity rejection; sector/page
alignment; firmware and E10 sentinels; payload and transport CRC failures;
multiple installs; duplicate rejection; interruption during payload,
catalog erase/program, and final activation; older-catalog fallback; retry;
and persistence across a simulated firmware replacement. The existing ROM,
Knight Tyme, launcher, CPCLIB1, importer, and UF2 policy tests remain enabled.

Stage 4B has not yet been exercised on physical flash. The first hardware test
must use an explicitly approved disposable sentinel package and verify reboot
persistence before any real game is installed.
