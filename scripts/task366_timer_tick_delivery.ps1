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
        "build\benchmarks\timer-tick-delivery"
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
if (-not (Test-Path -LiteralPath $baseScript -PathType Leaf))
{
    throw "Task 347 measurement script is missing: $baseScript"
}

function Get-LastMetricMatch
{
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Name
    )

    $found = [regex]::Matches(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($found.Count -eq 0)
    {
        throw "Required metric is missing: $Name"
    }
    return $found[$found.Count - 1]
}

function Get-UInt64
{
    param(
        [System.Text.RegularExpressions.Match]$Match,
        [int]$Group
    )

    return [UInt64]::Parse(
        $Match.Groups[$Group].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-Median
{
    param([double[]]$Values)

    $ordered = @($Values | Sort-Object)
    if ($ordered.Count -eq 0)
    {
        return 0.0
    }
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1)
    {
        return [double]$ordered[$middle]
    }
    return ([double]$ordered[$middle - 1] +
            [double]$ordered[$middle]) / 2.0
}

function Invoke-Configuration
{
    param(
        [string]$ResultRoot,
        [string]$BacklogValue
    )

    [Environment]::SetEnvironmentVariable(
        "REPIU_TIMER_TICK_BACKLOG", $BacklogValue, "Process")
    $arguments = @{
        Runs = $Runs
        DurationSeconds = $DurationSeconds
        Target = $Target
        OutputRoot = $ResultRoot
        ReleaseDirectory = $ReleaseDirectory
        EepromSeed = $EepromSeed
        ProbeInput = $ProbeInput
    }
    & $baseScript @arguments

    $result = Get-ChildItem -LiteralPath $ResultRoot -Directory |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($null -eq $result)
    {
        throw "Task 347 did not create a result under $ResultRoot"
    }
    return $result
}

function Read-Runs
{
    param(
        [System.IO.DirectoryInfo]$ResultDirectory,
        [string]$Label,
        [bool]$ExpectBacklog
    )

    $rows = @()
    for ($run = 1; $run -le $Runs; ++$run)
    {
        $runName = "$Label run-{0:D2}" -f $run
        $runDirectory = Join-Path $ResultDirectory.FullName `
            ("run-{0:D2}" -f $run)
        $text = Get-Content -Raw -LiteralPath `
            (Join-Path $runDirectory "combined.log")
        $metrics = Get-Content -Raw -LiteralPath `
            (Join-Path $runDirectory "metrics.json") |
            ConvertFrom-Json

        $ticks = Get-LastMetricMatch $text `
            "timer tick delivery backlog-enabled/due/injected/coalesced/dropped/deferred/max-backlog/remaining: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName tick delivery"
        $chain = Get-LastMetricMatch $text `
            "Win32 INT 8 chain HLE count/source/pointer/target: (\d+)" `
            "$runName INT 8 chain count"

        $enabled = $ticks.Groups[1].Value -eq "true"
        if ($enabled -ne $ExpectBacklog)
        {
            throw "$runName backlog mode is not the expected $ExpectBacklog"
        }
        $due = Get-UInt64 $ticks 2
        $injected = Get-UInt64 $ticks 3
        $coalesced = Get-UInt64 $ticks 4
        $dropped = Get-UInt64 $ticks 5
        $deferred = Get-UInt64 $ticks 6
        $maxBacklog = Get-UInt64 $ticks 7
        $remaining = Get-UInt64 $ticks 8

        # M1: every owed tick must land in exactly one bucket.
        if ($injected + $coalesced + $dropped + $remaining -ne $due)
        {
            throw ("$runName tick partition does not hold: " +
                   "$injected+$coalesced+$dropped+$remaining != $due")
        }
        if ($due -eq 0)
        {
            throw "$runName recorded no owed ticks"
        }
        # Without the backlog, owed ticks beyond the pending flag are discarded;
        # with it, coalescing must not happen at all.
        if ($ExpectBacklog -and $coalesced -ne 0)
        {
            throw "$runName coalesced ticks with the backlog enabled"
        }

        $rows += [pscustomobject][ordered]@{
            configuration = $Label
            run = $run
            frames = [UInt64]$metrics.frames
            glide_share_percent = [double]$metrics.glide_share_percent
            due = $due
            injected = $injected
            coalesced = $coalesced
            dropped = $dropped
            deferred = $deferred
            max_backlog = $maxBacklog
            remaining = $remaining
            int8_chain_count = Get-UInt64 $chain 1
            delivery_rate_percent = 100.0 * [double]$injected / [double]$due
            loss_rate_percent =
                100.0 * [double]($coalesced + $dropped) / [double]$due
            due_hz = [double]$due / [double]$DurationSeconds
            injected_hz = [double]$injected / [double]$DurationSeconds
            ticks_per_frame =
                [double]$injected / [double]$metrics.frames
        }
    }
    return @($rows)
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
$offRoot = Join-Path $sessionDirectory "backlog-off"
$onRoot = Join-Path $sessionDirectory "backlog-on"
New-Item -ItemType Directory -Path $offRoot -Force | Out-Null
New-Item -ItemType Directory -Path $onRoot -Force | Out-Null

$saved = [Environment]::GetEnvironmentVariable(
    "REPIU_TIMER_TICK_BACKLOG", "Process")
try
{
    $offResult = Invoke-Configuration $offRoot $null
    $onResult = Invoke-Configuration $onRoot "1"
}
finally
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_TIMER_TICK_BACKLOG", $saved, "Process")
}

$offRuns = Read-Runs $offResult "backlog-off" $false
$onRuns = Read-Runs $onResult "backlog-on" $true

$offMedianFrames = Get-Median @($offRuns | ForEach-Object { [double]$_.frames })
$onMedianFrames = Get-Median @($onRuns | ForEach-Object { [double]$_.frames })
$frameDeltaPercent =
    100.0 * ($onMedianFrames - $offMedianFrames) / $offMedianFrames
$offMedianDelivery = Get-Median @(
    $offRuns | ForEach-Object { $_.delivery_rate_percent })
$onMedianDelivery = Get-Median @(
    $onRuns | ForEach-Object { $_.delivery_rate_percent })
$offMedianDueHz = Get-Median @($offRuns | ForEach-Object { $_.due_hz })
$onMedianDueHz = Get-Median @($onRuns | ForEach-Object { $_.due_hz })
$offMedianInjectedHz = Get-Median @(
    $offRuns | ForEach-Object { $_.injected_hz })
$onMedianInjectedHz = Get-Median @(
    $onRuns | ForEach-Object { $_.injected_hz })
$onMaxBacklog = ($onRuns | Measure-Object max_backlog -Maximum).Maximum
$onMedianDropped = Get-Median @($onRuns | ForEach-Object { [double]$_.dropped })

# Pre-registered decisions from the design, recorded rather than enforced.
$t1 = ($onMedianDelivery -ge 98.0) -and ($frameDeltaPercent -ge 5.0)
$t2 = ($onMedianDelivery -gt $offMedianDelivery) -and
      ([Math]::Abs($frameDeltaPercent) -lt 5.0)
$t3 = $onMaxBacklog -ge 64
$t4 = $offMedianDelivery -ge 98.0

$offRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "backlog-off-runs.csv") `
    -NoTypeInformation -Encoding utf8
$onRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "backlog-on-runs.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    backlog_off_median_frames = $offMedianFrames
    backlog_on_median_frames = $onMedianFrames
    frame_delta_percent = $frameDeltaPercent
    backlog_off_median_delivery_percent = $offMedianDelivery
    backlog_on_median_delivery_percent = $onMedianDelivery
    backlog_off_median_due_hz = $offMedianDueHz
    backlog_on_median_due_hz = $onMedianDueHz
    backlog_off_median_injected_hz = $offMedianInjectedHz
    backlog_on_median_injected_hz = $onMedianInjectedHz
    backlog_on_max_backlog = $onMaxBacklog
    backlog_on_median_dropped = $onMedianDropped
    gate_m1_partition_identity = $true
    decision_t1_delivery_and_frames = $t1
    decision_t2_delivery_without_frames = $t2
    decision_t3_backlog_hit_cap = $t3
    decision_t4_no_loss_to_begin_with = $t4
    backlog_off_result = $offResult.FullName
    backlog_on_result = $onResult.FullName
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 366 timer tick delivery A/B complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames off/on/delta: {0:N0}/{1:N0}/{2:N2}%" -f `
    $offMedianFrames, $onMedianFrames, $frameDeltaPercent)
Write-Host ("Delivery off/on: {0:N1}%/{1:N1}%" -f `
    $offMedianDelivery, $onMedianDelivery)
Write-Host ("Owed/delivered Hz off: {0:N1}/{1:N1}; on: {2:N1}/{3:N1}" -f `
    $offMedianDueHz, $offMedianInjectedHz, $onMedianDueHz, $onMedianInjectedHz)
Write-Host ("Backlog peak/dropped (on): {0}/{1:N0}" -f `
    $onMaxBacklog, $onMedianDropped)
Write-Host ("Decisions T1/T2/T3/T4: {0}/{1}/{2}/{3}" -f $t1, $t2, $t3, $t4)
