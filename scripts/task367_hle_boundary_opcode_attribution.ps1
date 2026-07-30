param(
    [ValidateRange(1, 20)]
    [int]$Runs = 3,

    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 60,

    [string]$Target = "pumpit1",

    [string]$ReleaseDirectory = "",

    [string]$EepromSeed = "",

    [string]$ProbeInput = "",

    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repositoryRoot `
        "build\benchmarks\hle-boundary-opcodes"
}
if ([string]::IsNullOrWhiteSpace($ReleaseDirectory))
{
    $ReleaseDirectory = Join-Path $repositoryRoot `
        "build\win32_x86_debug\Release"
}
if ([string]::IsNullOrWhiteSpace($EepromSeed))
{
    $EepromSeed = Join-Path $repositoryRoot "eeprom.dat"
}
if ([string]::IsNullOrWhiteSpace($ProbeInput))
{
    $ProbeInput = Join-Path $repositoryRoot "MASTER\PIU_1ST\PIU.EXE"
}
$baseScript = Join-Path $PSScriptRoot `
    "task347_release_axis_reattribution.ps1"

# Known meanings for the opcodes this population is made of. `0F 0B` is UD2, the
# Glide gate's own trap, not a guest instruction.
$opcodeNames = @{
    "0F" = "two-byte escape"
    "8C" = "MOV r/m,Sreg"
    "8E" = "MOV Sreg,r/m"
    "ED" = "IN eAX,DX"
    "EE" = "OUT DX,AL"
    "EF" = "OUT DX,eAX"
    "EC" = "IN AL,DX"
    "CF" = "IRET"
    "88" = "MOV r/m8,r8"
    "1F" = "POP DS"
}

function Get-LastMetricMatch
{
    param([string]$Text, [string]$Pattern, [string]$Name)

    $found = [regex]::Matches($Text, $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($found.Count -eq 0)
    {
        throw "Required metric is missing: $Name"
    }
    return $found[$found.Count - 1]
}

function Get-Median
{
    param([double[]]$Values)

    $ordered = @($Values | Sort-Object)
    if ($ordered.Count -eq 0) { return 0.0 }
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1) { return [double]$ordered[$middle] }
    return ([double]$ordered[$middle - 1] + [double]$ordered[$middle]) / 2.0
}

function Read-OpcodeList
{
    param([string]$Text, [string]$Label)

    $body = Get-LastMetricMatch $Text `
        "Win32 AOT boundary $Label \[(.+?)\]" "$Label list"
    $entries = @()
    foreach ($pair in ($body.Groups[1].Value -split ' '))
    {
        $parts = $pair -split ':'
        if ($parts.Count -ne 2) { continue }
        $entries += [pscustomobject]@{
            opcode = $parts[0]
            count = [UInt64]$parts[1]
        }
    }
    return @($entries)
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
New-Item -ItemType Directory -Path $sessionDirectory -Force | Out-Null

$arguments = @{
    Runs = $Runs
    DurationSeconds = $DurationSeconds
    Target = $Target
    OutputRoot = $sessionDirectory
    ReleaseDirectory = $ReleaseDirectory
    EepromSeed = $EepromSeed
    ProbeInput = $ProbeInput
}
& $baseScript @arguments
$result = Get-ChildItem -LiteralPath $sessionDirectory -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
if ($null -eq $result)
{
    throw "Task 347 did not create a result under $sessionDirectory"
}

$runRows = @()
$opcodeRows = @()
for ($run = 1; $run -le $Runs; ++$run)
{
    $runName = "run-{0:D2}" -f $run
    $runDirectory = Join-Path $result.FullName $runName
    $text = Get-Content -Raw -LiteralPath `
        (Join-Path $runDirectory "combined.log")
    $metrics = Get-Content -Raw -LiteralPath `
        (Join-Path $runDirectory "metrics.json") | ConvertFrom-Json

    $census = Get-LastMetricMatch $text `
        "Win32 AOT boundary opcode census samples/escapes/prefixed/segment/opsize/truncated/prefix-overflow/empty: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
        "$runName opcode census"
    $reason = Get-LastMetricMatch $text `
        "Win32 AOT boundary reason ret/indir/direct/cond/other: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
        "$runName boundary reason"
    $provenance = Get-LastMetricMatch $text `
        "Win32 AOT breakpoint provenance hle/seg/inline/jtable/retired/probe/fixup/unknown: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
        "$runName breakpoint provenance"
    $timer = Get-LastMetricMatch $text `
        "Win32 AOT timer safe-point trap/injected/deferred: (\d+)/(\d+)/(\d+)" `
        "$runName timer traps"
    $gate = Get-LastMetricMatch $text `
        "Win32 Glide gate entries/handled/ESP: (\d+)/(\d+)/" `
        "$runName Glide gate entries"

    $samples = [UInt64]$census.Groups[1].Value
    $escapes = [UInt64]$census.Groups[2].Value
    $otherBoundaries = [UInt64]$reason.Groups[5].Value
    # N1: the census must see exactly the samples the boundary reason bucket fed
    # it, or prefix skipping is losing some.
    if ($samples -ne $otherBoundaries)
    {
        throw ("$runName census samples $samples do not match the other-boundary " +
               "count $otherBoundaries")
    }
    if ([UInt64]$census.Groups[7].Value -ne 0)
    {
        throw "$runName hit the legacy prefix bound"
    }
    # N2: the provenance decomposition plus timer traps must equal breakpoints.
    $provenanceSum = 0
    for ($g = 1; $g -le 8; ++$g)
    {
        $provenanceSum += [UInt64]$provenance.Groups[$g].Value
    }
    $breakpoints = [UInt64]$metrics.breakpoint_count
    if ($provenanceSum + [UInt64]$timer.Groups[1].Value -ne $breakpoints)
    {
        throw "$runName breakpoint provenance does not sum to $breakpoints"
    }

    $effective = Read-OpcodeList $text "effective opcodes"
    $escape = Read-OpcodeList $text "0F escape opcodes"
    $gateEntries = [UInt64]$gate.Groups[1].Value
    $ud2 = 0
    foreach ($entry in $escape)
    {
        if ($entry.opcode -eq "0B") { $ud2 = $entry.count }
    }

    foreach ($entry in $effective)
    {
        if ($entry.count -eq 0) { continue }
        $name = $opcodeNames[$entry.opcode]
        if ($null -eq $name) { $name = "(unclassified)" }
        $opcodeRows += [pscustomobject]@{
            run = $run
            kind = "effective"
            opcode = $entry.opcode
            name = $name
            count = $entry.count
            share_percent = 100.0 * [double]$entry.count / [double]$samples
        }
    }
    foreach ($entry in $escape)
    {
        if ($entry.count -eq 0) { continue }
        $name = "(0F escape)"
        if ($entry.opcode -eq "0B") { $name = "UD2 (Glide gate trap)" }
        $opcodeRows += [pscustomobject]@{
            run = $run
            kind = "escape"
            opcode = $entry.opcode
            name = $name
            count = $entry.count
            share_percent = 100.0 * [double]$entry.count / [double]$samples
        }
    }

    $runRows += [pscustomobject][ordered]@{
        run = $run
        frames = [UInt64]$metrics.frames
        exception_total = [UInt64]$metrics.exception_total
        exceptions_per_frame =
            [double]$metrics.exception_total / [double]$metrics.frames
        breakpoints = $breakpoints
        single_step = [UInt64]$metrics.single_step_count
        boundary_samples = $samples
        escapes = $escapes
        prefixed = [UInt64]$census.Groups[3].Value
        hle_provenance = [UInt64]$provenance.Groups[1].Value
        glide_gate_entries = $gateEntries
        ud2_count = $ud2
        # The finding this task turns on: the largest boundary class is the
        # Glide gate's own trap, one exception per Glide call.
        ud2_matches_glide_gates = [Math]::Abs(
            [double]$ud2 - [double]$gateEntries) -le
            (0.001 * [double]$gateEntries)
        ud2_share_percent = 100.0 * [double]$ud2 / [double]$samples
    }
}

$runRows | Export-Csv -LiteralPath (Join-Path $sessionDirectory "runs.csv") `
    -NoTypeInformation -Encoding utf8
$opcodeRows | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "opcodes.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    median_frames = Get-Median @($runRows | ForEach-Object { [double]$_.frames })
    median_exceptions_per_frame =
        Get-Median @($runRows | ForEach-Object { $_.exceptions_per_frame })
    median_boundary_samples =
        Get-Median @($runRows | ForEach-Object { [double]$_.boundary_samples })
    median_ud2_share_percent =
        Get-Median @($runRows | ForEach-Object { $_.ud2_share_percent })
    ud2_matches_glide_gates_every_run =
        (@($runRows | Where-Object { -not $_.ud2_matches_glide_gates }).Count -eq 0)
    gate_n1_sample_partition = $true
    gate_n2_provenance_identity = $true
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 367 HLE boundary opcode attribution complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames median: {0:N0}; exceptions per frame: {1:N1}" -f `
    $summary.median_frames, $summary.median_exceptions_per_frame)
Write-Host ("UD2 share of boundary samples: {0:N1}%" -f `
    $summary.median_ud2_share_percent)
Write-Host ("UD2 == Glide gate entries in every run: {0}" -f `
    $summary.ud2_matches_glide_gates_every_run)
