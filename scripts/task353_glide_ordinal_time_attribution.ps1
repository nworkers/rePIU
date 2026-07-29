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
        "build\benchmarks\glide-ordinal-time"
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

    $matches = [regex]::Matches(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($matches.Count -eq 0)
    {
        throw "Required metric is missing: $Name"
    }
    return $matches[$matches.Count - 1]
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

function Invoke-ReleaseAxis
{
    param(
        [string]$ResultRoot,
        [string]$ProfileValue
    )

    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", $ProfileValue, "Process")
    $arguments = @{
        Runs = $Runs
        DurationSeconds = $DurationSeconds
        Target = $Target
        OutputRoot = $ResultRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($ReleaseDirectory))
    {
        $arguments.ReleaseDirectory = $ReleaseDirectory
    }
    if (-not [string]::IsNullOrWhiteSpace($EepromSeed))
    {
        $arguments.EepromSeed = $EepromSeed
    }
    if (-not [string]::IsNullOrWhiteSpace($ProbeInput))
    {
        $arguments.ProbeInput = $ProbeInput
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

function Read-ControlRuns
{
    param([System.IO.DirectoryInfo]$ResultDirectory)

    $rows = @()
    for ($run = 1; $run -le $Runs; ++$run)
    {
        $runName = "run-{0:D2}" -f $run
        $metricsPath = Join-Path $ResultDirectory.FullName `
            "$runName\metrics.json"
        $metrics = Get-Content -Raw -LiteralPath $metricsPath |
            ConvertFrom-Json
        $rows += [pscustomobject]@{
            run = $run
            frames = [UInt64]$metrics.frames
            glide_share_percent = [double]$metrics.glide_share_percent
            glide_cycles = [UInt64]$metrics.glide_cycles
        }
    }
    return $rows
}

function Read-ProfileRuns
{
    param([System.IO.DirectoryInfo]$ResultDirectory)

    $runRows = @()
    $ordinalRows = @()
    for ($run = 1; $run -le $Runs; ++$run)
    {
        $runName = "run-{0:D2}" -f $run
        $runDirectory = Join-Path $ResultDirectory.FullName $runName
        $combinedPath = Join-Path $runDirectory "combined.log"
        $metricsPath = Join-Path $runDirectory "metrics.json"
        $text = Get-Content -Raw -LiteralPath $combinedPath
        $metrics = Get-Content -Raw -LiteralPath $metricsPath |
            ConvertFrom-Json

        $summary = Get-LastMetricMatch $text `
            "Win32 Glide ordinal timing enabled/entries/completed/overflow/clamped: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName ordinal summary"
        $cycles = Get-LastMetricMatch $text `
            "Win32 Glide ordinal cycles gate/queue/wake/work/complete/residual/backend-total/direct-work: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName ordinal cycles"
        $backend = Get-LastMetricMatch $text `
            "Win32 glide gate cycles queue/wake/work/complete/residual/total: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName global backend cycles"
        $backendCounts = Get-LastMetricMatch $text `
            "Win32 glide gate timing enabled/rendezvous/direct/clamped: (true|false)/(\d+)/(\d+)/(\d+)" `
            "$runName global backend counts"
        $backendDirect = Get-LastMetricMatch $text `
            "Win32 glide gate direct cycles/max wake/work/total: (\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName global direct work"
        $ordinalCounts = Get-LastMetricMatch $text `
            "Win32 Glide ordinal backend rendezvous/direct: (\d+)/(\d+)" `
            "$runName ordinal backend counts"
        $handled = Get-LastMetricMatch $text `
            "Win32 Glide gate entries/handled/ESP: (\d+)/(\d+)/" `
            "$runName Glide handled count"

        $entryCount = Get-UInt64 $summary 2
        $completed = Get-UInt64 $summary 3
        $overflow = Get-UInt64 $summary 4
        $clamped = Get-UInt64 $summary 5
        $gateCycles = Get-UInt64 $cycles 1
        $queueCycles = Get-UInt64 $cycles 2
        $wakeCycles = Get-UInt64 $cycles 3
        $workCycles = Get-UInt64 $cycles 4
        $completeCycles = Get-UInt64 $cycles 5
        $residualCycles = Get-UInt64 $cycles 6
        $backendTotal = Get-UInt64 $cycles 7
        $directWork = Get-UInt64 $cycles 8
        $rendezvous = Get-UInt64 $ordinalCounts 1
        $direct = Get-UInt64 $ordinalCounts 2
        $handledCount = Get-UInt64 $handled 2
        $entryCountGlobal = Get-UInt64 $handled 1

        if ($summary.Groups[1].Value -ne "true" -or
            $overflow -ne 0 -or $clamped -ne 0)
        {
            throw "$runName ordinal profile is disabled, overflowed, or clamped"
        }
        if ($completed -ne $handledCount)
        {
            throw "$runName completed/handled mismatch: $completed/$handledCount"
        }
        if ($entryCountGlobal -lt $handledCount -or
            ($entryCountGlobal - $handledCount) -gt 1)
        {
            throw "$runName has more than one open Glide gate at timeout"
        }

        $coverage =
            100.0 * [double]$gateCycles / [double]$metrics.glide_cycles
        if ($coverage -lt 99.0 -or $coverage -gt 101.0)
        {
            throw "$runName global Glide coverage is outside 99-101%: $coverage"
        }

        $globalValues = @(
            (Get-UInt64 $backend 1),
            (Get-UInt64 $backend 2),
            (Get-UInt64 $backend 3),
            (Get-UInt64 $backend 4),
            (Get-UInt64 $backend 5),
            (Get-UInt64 $backend 6),
            (Get-UInt64 $backendCounts 2),
            (Get-UInt64 $backendCounts 3),
            (Get-UInt64 $backendDirect 1))
        $ordinalValues = @(
            $queueCycles, $wakeCycles, $workCycles, $completeCycles,
            $residualCycles, $backendTotal, $rendezvous, $direct,
            $directWork)
        for ($index = 0; $index -lt $globalValues.Count; ++$index)
        {
            if ($globalValues[$index] -lt $ordinalValues[$index])
            {
                throw "$runName ordinal backend field exceeds global field $index"
            }
            if ($globalValues[$index] -ne $ordinalValues[$index] -and
                ($entryCountGlobal - $handledCount) -ne 1)
            {
                throw "$runName backend delta mismatch without an open gate at field $index"
            }
        }

        $rowMatches = [regex]::Matches(
            $text,
            "Win32 Glide ordinal timing: ordinal=(\d+) name=(\S+) count=(\d+) gate=(\d+) max=(\d+) rendezvous=(\d+) queue=(\d+) wake=(\d+) work=(\d+) complete=(\d+) residual=(\d+) backend_total=(\d+) direct=(\d+) direct_work=(\d+)",
            [System.Text.RegularExpressions.RegexOptions]::Multiline)
        if ($rowMatches.Count -ne $entryCount)
        {
            throw "$runName row count does not match active entries"
        }
        foreach ($row in $rowMatches)
        {
            $rowGate = Get-UInt64 $row 4
            $ordinalRows += [pscustomobject]@{
                run = $run
                ordinal = [UInt32](Get-UInt64 $row 1)
                name = $row.Groups[2].Value
                count = Get-UInt64 $row 3
                gate_cycles = $rowGate
                gate_share_percent =
                    100.0 * [double]$rowGate / [double]$gateCycles
                max_gate_cycles = Get-UInt64 $row 5
                rendezvous_count = Get-UInt64 $row 6
                queue_cycles = Get-UInt64 $row 7
                wake_cycles = Get-UInt64 $row 8
                work_cycles = Get-UInt64 $row 9
                complete_cycles = Get-UInt64 $row 10
                residual_cycles = Get-UInt64 $row 11
                backend_total_cycles = Get-UInt64 $row 12
                direct_count = Get-UInt64 $row 13
                direct_work_cycles = Get-UInt64 $row 14
            }
        }

        $runRows += [pscustomobject]@{
            run = $run
            frames = [UInt64]$metrics.frames
            glide_share_percent = [double]$metrics.glide_share_percent
            global_glide_cycles = [UInt64]$metrics.glide_cycles
            ordinal_gate_cycles = $gateCycles
            coverage_percent = $coverage
            active_entries = $entryCount
            completed_gates = $completed
            open_gate_count = $entryCountGlobal - $handledCount
            open_queue_cycles = $globalValues[0] - $queueCycles
            rendezvous_count = $rendezvous
            wake_cycles = $wakeCycles
            work_cycles = $workCycles
            complete_cycles = $completeCycles
            backend_total_cycles = $backendTotal
            direct_work_cycles = $directWork
        }
    }
    return @{
        Runs = $runRows
        Ordinals = $ordinalRows
    }
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
$controlRoot = Join-Path $sessionDirectory "control"
$profileRoot = Join-Path $sessionDirectory "profile"
New-Item -ItemType Directory -Path $controlRoot -Force | Out-Null
New-Item -ItemType Directory -Path $profileRoot -Force | Out-Null

$savedProfile = [Environment]::GetEnvironmentVariable(
    "REPIU_GLIDE_ORDINAL_TIME_PROFILE", "Process")
try
{
    $controlResult = Invoke-ReleaseAxis $controlRoot $null
    $profileResult = Invoke-ReleaseAxis $profileRoot "1"
}
finally
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", $savedProfile, "Process")
}

$controlRuns = @(Read-ControlRuns $controlResult)
$profile = Read-ProfileRuns $profileResult
$profileRuns = @($profile.Runs)
$ordinalRows = @($profile.Ordinals)

$controlMedianFrames = Get-Median @(
    $controlRuns | ForEach-Object { [double]$_.frames })
$profileMedianFrames = Get-Median @(
    $profileRuns | ForEach-Object { [double]$_.frames })
$frameDeltaPercent =
    100.0 * ($profileMedianFrames - $controlMedianFrames) /
    $controlMedianFrames
if ($DurationSeconds -ge 60 -and
    [Math]::Abs($frameDeltaPercent) -gt 5.0)
{
    throw "Profile observer impact exceeds 5%: $frameDeltaPercent"
}

$topOrdinals = @(
    $ordinalRows |
        Group-Object run |
        ForEach-Object {
            ($_.Group | Sort-Object gate_cycles -Descending |
                Select-Object -First 1).ordinal
        })
if (@($topOrdinals | Select-Object -Unique).Count -ne 1)
{
    throw "Leading ordinal is not stable across runs: $topOrdinals"
}

$aggregateRows = @(
    $ordinalRows |
        Group-Object ordinal, name |
        ForEach-Object {
            $group = @($_.Group)
            [pscustomobject]@{
                ordinal = $group[0].ordinal
                name = $group[0].name
                runs_present = $group.Count
                count_sum =
                    ($group | ForEach-Object { $_.count } |
                        Measure-Object -Sum).Sum
                gate_cycles_sum =
                    ($group.gate_cycles | Measure-Object -Sum).Sum
                mean_gate_share_percent =
                    ($group.gate_share_percent | Measure-Object -Average).Average
                wake_cycles_sum =
                    ($group.wake_cycles | Measure-Object -Sum).Sum
                work_cycles_sum =
                    ($group.work_cycles | Measure-Object -Sum).Sum
                complete_cycles_sum =
                    ($group.complete_cycles | Measure-Object -Sum).Sum
                backend_total_cycles_sum =
                    ($group.backend_total_cycles | Measure-Object -Sum).Sum
            }
        } |
        Sort-Object gate_cycles_sum -Descending)

$controlRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "control-runs.csv") `
    -NoTypeInformation -Encoding utf8
$profileRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "profile-runs.csv") `
    -NoTypeInformation -Encoding utf8
$ordinalRows | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "ordinal-runs.csv") `
    -NoTypeInformation -Encoding utf8
$aggregateRows | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "ordinal-aggregate.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    control_median_frames = $controlMedianFrames
    profile_median_frames = $profileMedianFrames
    frame_delta_percent = $frameDeltaPercent
    top_ordinal = $topOrdinals[0]
    mean_coverage_percent =
        ($profileRuns.coverage_percent | Measure-Object -Average).Average
    control_result = $controlResult.FullName
    profile_result = $profileResult.FullName
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 353 Glide ordinal attribution complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames control/profile/delta: {0:N0}/{1:N0}/{2:N2}%" -f `
    $controlMedianFrames, $profileMedianFrames, $frameDeltaPercent)
Write-Host ("Top ordinal: {0}" -f $topOrdinals[0])
