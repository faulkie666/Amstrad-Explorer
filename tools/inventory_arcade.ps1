param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutputCsv
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$outputParent = Split-Path -Parent $OutputCsv
if ($outputParent) { New-Item -ItemType Directory -Force -Path $outputParent | Out-Null }

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-CanonicalTitle([string]$Title) {
    $value = $Title.ToLowerInvariant()
    $value = [regex]::Replace($value, '\([^)]*\)', ' ')
    $value = [regex]::Replace($value, '\b(english|spanish|french|german|italian)\b', ' ')
    $value = [regex]::Replace($value, '\b(the|le|la|el|der|die|das)\b', ' ')
    $value = [regex]::Replace($value, '[^a-z0-9]+', '')
    return $value
}

$rows = [System.Collections.Generic.List[object]]::new()
$games = Get-ChildItem -LiteralPath $source -Directory | Sort-Object Name
$index = 0
foreach ($game in $games) {
    $index++
    if (($index % 100) -eq 0) { Write-Host "INVENTORY_PROGRESS $index/$($games.Count)" }
    $files = @(Get-ChildItem -LiteralPath $game.FullName -Recurse -File | Sort-Object FullName)
    $manifest = [System.Text.StringBuilder]::new()
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($game.FullName.Length + 1).Replace('\', '/')
        $hash = Get-Sha256 $file.FullName
        [void]$manifest.Append($relative).Append('|').Append($file.Length).Append('|').Append($hash).Append("`n")
    }
    $manifestBytes = [Text.Encoding]::UTF8.GetBytes($manifest.ToString())
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $folderHash = ([BitConverter]::ToString($sha.ComputeHash($manifestBytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }

    $dsk = @($files | Where-Object { $_.Name -match '(?i)\.dsk(\.gz)?$' })
    $cdt = @($files | Where-Object { $_.Name -match '(?i)\.cdt(\.gz)?$' })
    $sna = @($files | Where-Object { $_.Name -match '(?i)\.sna(\.gz)?$' })
    $cpr = @($files | Where-Object { $_.Name -match '(?i)\.cpr(\.gz)?$' })
    $zip = @($files | Where-Object { $_.Extension -in '.zip', '.7z', '.rar' })
    $bas = @($files | Where-Object Extension -eq '.bas')
    $bin = @($files | Where-Object Extension -eq '.bin')
    $noExt = @($files | Where-Object { [string]::IsNullOrEmpty($_.Extension) })
    $containers = @($sna + $dsk + $cdt + $cpr + $zip)
    $sourceFiles = if ($containers.Count) {
        ($containers | ForEach-Object { $_.FullName.Substring($source.Length + 1).Replace('\', '/') }) -join '; '
    } else {
        $game.FullName.Substring($source.Length + 1).Replace('\', '/') + '/ (extracted file set)'
    }
    $formats = [System.Collections.Generic.List[string]]::new()
    if ($sna.Count) { $formats.Add("SNA:$($sna.Count)") }
    if ($dsk.Count) { $formats.Add("DSK.GZ:$($dsk.Count)") }
    if ($cdt.Count) { $formats.Add("CDT:$($cdt.Count)") }
    if ($cpr.Count) { $formats.Add("CPR:$($cpr.Count)") }
    if ($zip.Count) { $formats.Add("ARCHIVE:$($zip.Count)") }
    if ($bas.Count) { $formats.Add("BAS:$($bas.Count)") }
    if ($bin.Count) { $formats.Add("BIN:$($bin.Count)") }
    if ($noExt.Count) { $formats.Add("NOEXT:$($noExt.Count)") }
    if (-not $formats.Count) { $formats.Add('EXTRACTED_OTHER') }

    $status = 'UNSUPPORTED_CURRENT_LOADER'
    $compatibility = 'Requires title-specific CPC464 boot/capture and host gameplay proof'
    $controls = 'Unverified; Stage 4F.6 has no input profile for this ID'
    $notes = 'No directly uploadable standalone CPC464 64 KiB SNA'
    if ($game.Name -eq 'Manic Miner') {
        $status = 'INSTALLED'
        $compatibility = 'Proven CPC464 native snapshot and package already installed'
        $controls = 'X=left, Z=right, Y=jump'
        $notes = 'Preserve existing manic_miner entry; do not duplicate'
    } elseif ($game.Name -match '(?i)128\s*\+|128k|6128') {
        $compatibility = 'Explicit 128K/6128 edition; unsupported by CPC464 64 KiB target'
        $notes = 'Prefer a separate verified CPC464/64K edition if one exists'
    } elseif ($cpr.Count -gt 0 -and $dsk.Count -eq 0) {
        $compatibility = 'Cartridge image unsupported by current snapshot importer'
        $notes = 'No CPR loader; requires a separately proved CPC464 snapshot capture'
    } elseif ($dsk.Count -gt 0) {
        $notes = 'DSK cannot be uploaded directly; boot/capture and post-start disk-dependency checks required'
    } elseif ($bas.Count -gt 0 -or $bin.Count -gt 0 -or $noExt.Count -gt 0) {
        $notes = 'Extracted files are not a standalone image; loading order/addresses/dependencies must be proved'
    }

    $rows.Add([pscustomobject]@{
        GameTitle = $game.Name
        CanonicalTitle = Get-CanonicalTitle $game.Name
        SourceFile = $sourceFiles
        SourceManifestSHA256 = $folderHash
        Format = $formats -join '; '
        FileCount = $files.Count
        SourceBytes = [long](($files | Measure-Object Length -Sum).Sum)
        PackageSize = if ($status -eq 'INSTALLED') { 65888 } else { '' }
        Compatibility = $compatibility
        Status = $status
        Controls = $controls
        Notes = $notes
    })
}

$groupSizes = @{}
$rows | Group-Object CanonicalTitle | ForEach-Object { $groupSizes[$_.Name] = $_.Count }
$final = foreach ($row in $rows) {
    $row | Add-Member -NotePropertyName VariantGroupSize -NotePropertyValue $groupSizes[$row.CanonicalTitle] -PassThru
}
$final | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding utf8
Write-Host "INVENTORY_COMPLETE games=$($final.Count) csv=$OutputCsv"
