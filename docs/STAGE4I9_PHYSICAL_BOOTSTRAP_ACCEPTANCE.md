# Stage 4I.9 — Physical installation bootstrap proof

## Decision

The six-state bootstrap sequence is **PASS in host simulation**. Catalogue and
snapshot preservation, incomplete-pack fallback, activation, and firmware
rollback behaviour all pass.

Physical installation remains **HOLD** until a complete 16 MiB backup has been
made from the identified device, independently verified, and validated with the
new backup checker. This stage did not access a COM port, USB device, BOOTSEL
volume, catalogue, or physical flash.

## Exact artifacts

| Artifact | Identity | Size | SHA-256 |
|---|---|---:|---|
| Stage 4I.8 firmware | `0.4.20-stage4i8migrationexp` | 733,184 B UF2 | `28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114` |
| Stage 4I.7 CPXPCK2 | six records, five launchable | 179,360 B | `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe` |
| Stage 4H.4 rollback | `0.4.15-stage4h4ayaudio` | 690,688 B UF2 | `d12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0` |

The Stage 4I.8 UF2 has 1,431 ordinary family-`0xe48bff59` pages at
`0x10000000–0x100596ff`, wholly inside the first 4 MiB. Its only other block is
the exact authorized RP2350-E10 block at `0x10ffff00`. It contains no catalogue
or game-storage target.

The migration manifest pins version 1, physical flash offset `0x43b000`, length
179,360, CRC-32 `ec7fd1e9`, and the complete pack SHA-256 above. Staging erases
only `0x43b000–0x466fff`. Firmware, both legacy catalogues, existing allocations,
free storage after `0x467000`, and the E10 guard are rejected as migration write
destinations.

## Bootstrap sequence proof

The Stage 4I.8 test now models each requested state using the real CPCLIB1
parser, CPXPCK2 parser, installer, migration transport, CPCU1 framing, actual
179,360-byte pack, and a 16 MiB flash image.

| State | Host result | Authority and visible content |
|---|---|---|
| A — Stage 4H.4 baseline | PASS | BASIC plus CPCLIB1 Knight Tyme, Manic Miner and Dizzy |
| B — Stage 4I.8, no pack | PASS | CPXPCK2 mount fails cleanly; CPCLIB1 remains authoritative; migration state is `legacy` |
| C — partial pack | PASS | header pages remain unwritten; reboot gives `reserved-incomplete`; old catalogue remains authoritative |
| D — fully written/verified, not activated | PASS | staging CRC passes but both publication headers remain erased; old catalogue remains authoritative |
| E — activated | PASS | header B is verified before header A; BASIC plus Columns, Dizzy, Knight Tyme, Manic Miner and Thrust are accessible |
| F — Stage 4H.4 rollback | PASS | Stage 4H.4 ignores CPXPCK2 and mounts the preserved CPCLIB1 catalogue; original four-game experience remains available |

Power-loss injection covers catalogue-copy publication, staging erase,
mid-page programming, readback failure, pre-activation reboot, header-B write,
header-A write, and reboot immediately after the atomic activation boundary.
Before header B is valid the pack never mounts. Once header B is valid the pack
mounts even if header A was interrupted. Every state retains a valid old
catalogue and byte-identical old snapshot allocations.

An interrupted UF2 copy may leave the application itself unbootable, but the UF2
address audit proves it cannot target either catalogue or the installed game
allocations. Recovery is a separately approved BOOTSEL firmware reflash; it is
not an automatic retry. The application-level migration must not start until
Stage 4I.8 has booted and the old four-game menu has been checked.

## Rollback proof and limitation

Migration publishes both legacy catalogue copies with the same three
descriptors and advances their persistent allocator high-water mark to relative
`0x67000`. It never erases the old payloads. Stage 4H.4 reads CPCLIB1 and does not
mount CPXPCK2, so BASIC plus the three legacy snapshots remain visible after a
firmware-only rollback.

Rollback is for launching and validating the original games. Do not upload or
remove games while running the rollback firmware until its treatment of the new
reserved high-water mark has also been physically confirmed. A damaged physical
catalogue is a HOLD condition; the full backup must not be restored with
`picotool load` as part of this runbook.

## Full-flash backup feasibility

Raspberry Pi's official picotool documentation states that `save -a` saves all
flash, `save -r` saves an explicit range, `-v` verifies the save, and `verify`
independently compares a file with the device. Device selection can be restricted
by bus, address, VID, PID and serial number. See the official
[picotool README and command reference](https://github.com/raspberrypi/picotool/blob/master/README.md#save).
Raspberry Pi also publishes Windows binaries through the official
[pico-sdk-tools releases](https://github.com/raspberrypi/pico-sdk-tools/releases).

The workspace copy at
`build-rp2350-library/_deps/picotool/picotool.exe` reports v2.2.0 but also reports
`compiled without USB support`; its command set omits `save` and `verify`.
Therefore it is **not acceptable** for physical backup. No replacement was
downloaded or installed during this stage.

`tools/validate_flash_backup.py` independently validates:

- exact 16,777,216-byte image length and optional whole-image SHA-256;
- CRC, commit marker, geometry and generation of both 16 KiB catalogues;
- exact expected IDs in both copies;
- every descriptor, allocation boundary, CPC464/64 KiB SNA header, payload CRC
  and payload SHA-256;
- absence of overlapping allocations;
- separate firmware-region, catalogue, allocation and E10-guard hashes;
- extraction of both catalogues and the three external-flash snapshots.

Locomotive BASIC is built into firmware and has no fourth external game
allocation. The backup report lists it separately, followed by the three
validated external allocations. The validator's 16 MiB self-test and deliberate
payload-corruption rejection both pass.

## Prepared PowerShell runbook — not executed

Every block below is a future operator action. Read-only and write operations are
separated. A new explicit user approval is required before each block marked
**WRITE**. `Set-StrictMode`, terminating errors, exact hashes and explicit serial
selection make each block fail closed.

### 0. Local artifact and tool preflight — no device access

```powershell
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Project = '%USERPROFILE%\OneDrive\Documents\CPC Explorer'
$Firmware = Join-Path $Project 'build-rp2350-stage4i8-migration\cpc_explorer_flash_migration_exp.uf2'
$Pack = Join-Path $Project 'artifacts\stage4i7-internal-library\library.cpxpack'
$Rollback = Join-Path $Project 'build-rp2350-stage4h4-continuous-audio\cpc_explorer_ay_audio_continuous.uf2'
$ExpectedFirmware = '28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114'
$ExpectedPack = '27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe'
$ExpectedRollback = 'd12f69fde526bc51f387aa78467b7b9417a66c26b331b9f8e8d05c735d8798c0'

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing: $Path" }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) { throw "SHA-256 mismatch: $Path`n$actual" }
}

Assert-Hash $Firmware $ExpectedFirmware
Assert-Hash $Pack $ExpectedPack
Assert-Hash $Rollback $ExpectedRollback
py (Join-Path $Project 'tools\validate_uf2.py') $Firmware --expect-sha256 $ExpectedFirmware
if ($LASTEXITCODE -ne 0) { throw 'Stage 4I.8 UF2 policy failed' }

# Must point to an official USB-enabled picotool binary obtained separately.
$Picotool = 'C:\Tools\pico-sdk-tools\picotool\picotool.exe'
if (-not (Test-Path -LiteralPath $Picotool -PathType Leaf)) { throw 'USB-enabled picotool missing' }
$helpText = (& $Picotool help 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $helpText -match 'without USB support' -or
    $helpText -notmatch '(?m)^\s*save\s' -or $helpText -notmatch '(?m)^\s*verify\s') {
    throw 'This picotool build cannot perform the required read-only USB backup'
}
```

### 1. Read-only BOOTSEL identity and complete backup

This block inventories candidates first. It deliberately stops for manual serial
selection and never falls back to the first matching device.

```powershell
Set-Location -LiteralPath $Project
$inventory = (& $Picotool info -a -d --vid 0x2e8a --pid 0x000f 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'No readable Raspberry Pi BOOTSEL device' }
$inventory

# Copy the one serial printed for the physically identified Explorer.
$DeviceSerial = 'REPLACE_WITH_EXACT_PICOTOOL_SERIAL'
if ($DeviceSerial -like 'REPLACE_*' -or [string]::IsNullOrWhiteSpace($DeviceSerial)) {
    throw 'Exact device serial has not been selected'
}
$selector = @('--vid','0x2e8a','--pid','0x000f','--ser',$DeviceSerial)
$identity = (& $Picotool info -a -d @selector 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $identity -notmatch 'RP2350' -or
    $identity -notmatch 'flash size:\s*(16384K|16M|16777216)') {
    throw "Unexpected target identity or flash capacity`n$identity"
}

$bootVolumes = @(Get-Volume | Where-Object FileSystemLabel -eq 'RP2350')
if ($bootVolumes.Count -ne 1 -or -not $bootVolumes[0].DriveLetter) {
    throw 'Expected exactly one RP2350 BOOTSEL volume'
}
$BootRoot = "$($bootVolumes[0].DriveLetter):\"
$uf2Info = Get-Content -LiteralPath (Join-Path $BootRoot 'INFO_UF2.TXT') -Raw
if ($uf2Info -notmatch 'RP2350') { throw 'INFO_UF2.TXT does not identify RP2350' }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$BackupRoot = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "CPC Explorer Backups\Stage4I9-$stamp"
New-Item -ItemType Directory -Path $BackupRoot -ErrorAction Stop | Out-Null
$Backup1 = Join-Path $BackupRoot 'explorer-full-16m-read1.bin'
$Backup2 = Join-Path $BackupRoot 'explorer-full-16m-read2.bin'

& $Picotool save -r 0x10000000 0x11000000 -v $Backup1 -t bin @selector
if ($LASTEXITCODE -ne 0) { throw 'First complete flash save/verify failed' }
if ((Get-Item -LiteralPath $Backup1).Length -ne 16777216) { throw 'Backup length is not 16 MiB' }

& $Picotool verify $Backup1 -t bin -o 0x10000000 -r 0x10000000 0x11000000 @selector
if ($LASTEXITCODE -ne 0) { throw 'Independent device verification failed' }

& $Picotool save -r 0x10000000 0x11000000 -v $Backup2 -t bin @selector
if ($LASTEXITCODE -ne 0) { throw 'Second independent flash save failed' }
$BackupHash = (Get-FileHash -LiteralPath $Backup1 -Algorithm SHA256).Hash.ToLowerInvariant()
$BackupHash2 = (Get-FileHash -LiteralPath $Backup2 -Algorithm SHA256).Hash.ToLowerInvariant()
if ($BackupHash -ne $BackupHash2) { throw 'Independent backup reads differ' }

$Extract = Join-Path $BackupRoot 'validated-legacy'
py (Join-Path $Project 'tools\validate_flash_backup.py') $Backup1 `
  --expect-sha256 $BackupHash `
  --expect-id knight_tyme --expect-id manic_miner --expect-id dizzy `
  --extract-dir $Extract
if ($LASTEXITCODE -ne 0) { throw 'Catalogue/allocation validation failed' }
"$BackupHash  explorer-full-16m-read1.bin" | Set-Content -LiteralPath (Join-Path $BackupRoot 'SHA256SUMS.txt')
Write-Host "READ-ONLY BACKUP GATE PASS: $BackupRoot"
```

Do not continue unless this gate passes and the backup directory exists outside
all build directories.

### 2. Stage 4I.8 deployment — **WRITE 1, separate approval required**

Re-run block 0 and the exact-serial identity checks immediately before this
block. This performs one UF2 copy and has no automatic retry.

```powershell
$bootVolumes = @(Get-Volume | Where-Object FileSystemLabel -eq 'RP2350')
if ($bootVolumes.Count -ne 1 -or -not $bootVolumes[0].DriveLetter) { throw 'RP2350 volume ambiguous' }
$BootRoot = "$($bootVolumes[0].DriveLetter):\"
if ((Get-Content -LiteralPath (Join-Path $BootRoot 'INFO_UF2.TXT') -Raw) -notmatch 'RP2350') {
    throw 'BOOTSEL identity failed immediately before firmware copy'
}
Assert-Hash $Firmware $ExpectedFirmware
Copy-Item -LiteralPath $Firmware -Destination (Join-Path $BootRoot 'cpc_explorer_flash_migration_exp.uf2') -ErrorAction Stop
Write-Host 'ONE UF2 COPY ISSUED. DO NOT RETRY AUTOMATICALLY.'
```

If the copy outcome is ambiguous, stop. Do not run the migration.

### 3. Legacy verification under Stage 4I.8 — read-only/manual

After normal application enumeration, explicitly set the observed port:

```powershell
$Port = 'COM_REPLACE_WITH_OBSERVED_PORT'
if ($Port -like '*REPLACE*') { throw 'Application port not explicitly selected' }
py tools/cpc_game.py detect
if ($LASTEXITCODE -ne 0) { throw 'Explorer detection failed' }
py tools/cpc_game.py status --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Stage 4I.8 status failed' }
py tools/cpc_game.py list --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Legacy game list failed' }
py tools/cpc_game.py migration-status --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Migration state query failed' }
```

Required observations: firmware `0.4.20-stage4i8migrationexp`, flash `EF-40-18`,
capacity 16,777,216, migration `legacy`, and BASIC/Knight Tyme/Manic Miner/Dizzy.
Physically launch all four before approving staging.

### 4. CPXPCK2 staging/readback — **WRITE 2, separate approval required**

First run the dry-run, which opens no port:

```powershell
py tools/cpc_game.py stage-library $Pack --firmware $Firmware `
  --expect-pack-sha256 $ExpectedPack --expect-firmware-sha256 $ExpectedFirmware
if ($LASTEXITCODE -ne 0) { throw 'Local staging dry-run failed' }
```

Only after a new approval, execute exactly once:

```powershell
Assert-Hash $Firmware $ExpectedFirmware
Assert-Hash $Pack $ExpectedPack
py tools/cpc_game.py stage-library $Pack --firmware $Firmware `
  --expect-pack-sha256 $ExpectedPack --expect-firmware-sha256 $ExpectedFirmware `
  --port $Port --execute --confirm STAGE
if ($LASTEXITCODE -ne 0) { throw 'Staging failed or is ambiguous: HOLD, DO NOT RETRY' }
```

The command reserves both old catalogues, transfers once, verifies every staged
byte, and stops in `verified`; it does not activate CPXPCK2. Query status/list
read-only and require the old four-game experience to remain authoritative.

### 5. Catalogue activation — **WRITE 3, separate approval required**

Dry-run first:

```powershell
py tools/cpc_game.py activate-library $Pack --expect-pack-sha256 $ExpectedPack
if ($LASTEXITCODE -ne 0) { throw 'Activation dry-run failed' }
```

After a separate activation approval:

```powershell
Assert-Hash $Pack $ExpectedPack
py tools/cpc_game.py activate-library $Pack --expect-pack-sha256 $ExpectedPack `
  --port $Port --execute --confirm ACTIVATE
if ($LASTEXITCODE -ne 0) { throw 'Activation failed or is ambiguous: HOLD' }
```

The activation command first requires state `verified`, independently rereads
the staged image, publishes header B then header A, rereads the active image, and
requires the exact pack SHA-256 and state `active`.

### 6. Gameplay, controls, audio and persistence — read-only/manual

```powershell
py tools/cpc_game.py status --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Post-activation status failed' }
py tools/cpc_game.py list --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Post-activation list failed' }
py tools/cpc_game.py migration-status --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Post-activation migration status failed' }
```

Require BASIC plus exactly Columns, Dizzy, Knight Tyme, Manic Miner and Thrust.
Test BASIC commands, all five game launches, controls, Knight Tyme/Manic Miner AY
audio, normal reboot persistence, CPCPERF/CPCAUDIO, and both core stack
watermarks. The 256-byte SRAM margin makes stack-watermark acceptance mandatory.

### 7. Firmware rollback — **WRITE 4, separate approval required**

Rollback is not automatic. Re-enter BOOTSEL manually, repeat exact serial,
VID/PID, volume and `INFO_UF2.TXT` checks, then under a separate approval:

```powershell
Assert-Hash $Rollback $ExpectedRollback
$bootVolumes = @(Get-Volume | Where-Object FileSystemLabel -eq 'RP2350')
if ($bootVolumes.Count -ne 1 -or -not $bootVolumes[0].DriveLetter) { throw 'Rollback target ambiguous' }
$BootRoot = "$($bootVolumes[0].DriveLetter):\"
if ((Get-Content -LiteralPath (Join-Path $BootRoot 'INFO_UF2.TXT') -Raw) -notmatch 'RP2350') {
    throw 'Rollback BOOTSEL identity failed'
}
Copy-Item -LiteralPath $Rollback -Destination (Join-Path $BootRoot 'cpc_explorer_ay_audio_continuous.uf2') -ErrorAction Stop
Write-Host 'ONE ROLLBACK UF2 COPY ISSUED. DO NOT RETRY AUTOMATICALLY.'
```

After normal boot, require Stage 4H.4 and the original BASIC, Knight Tyme, Manic
Miner and Dizzy experience. Do not upload/remove games and do not restore the raw
backup automatically.

## Host results

- complete suite: **24/24 PASS** (the prior 23 plus full-backup validation)
- bootstrap states A–F: **PASS**
- complete CPCU1 framed migration: **PASS**
- interrupted transfer and activation recovery: **PASS**
- legacy catalogues and allocations after migration: **PASS**
- full-backup validator self-test and corruption rejection: **PASS**
- Stage 4I.8 UF2 and pack hashes: **PASS**
- firmware/pack format and fixed-address compatibility: **PASS**
- SRAM headroom: 119,040 bytes; mandatory 118,784; margin 256 bytes

## Remaining risks and gates

- The actual device has not been read. Physical installation remains HOLD until
  two matching 16 MiB backups and device verification pass.
- A USB-enabled official picotool binary is not currently present in the
  workspace. Its provenance/version/hash must be recorded before use.
- Real power-loss timing, USB timeouts and stack watermarks remain unmeasured.
- An interrupted UF2 copy can require a separately approved recovery reflash,
  although it cannot target the catalogue/game ranges.
- Raw-backup restoration is intentionally outside this runbook.

## Final gates

- **BOOTSTRAP SEQUENCE:** PASS (host simulation)
- **LEGACY CATALOGUE PRESERVATION:** PASS (host simulation)
- **BACKUP PROCEDURE:** PASS as a reviewed procedure and validator; NOT EXECUTED
- **HOST REGRESSIONS:** PASS, 24/24
- **PHYSICAL INSTALLATION:** HOLD pending verified full-device backup
- **PHYSICAL ACCESS:** NOT PERFORMED
- **PHYSICAL FLASH:** NOT AUTHORISED
