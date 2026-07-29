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
        "build\benchmarks\glide-buffer-swap-time"
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
        [string]$SwapProfileValue,
        [string]$OrdinalProfileValue
    )

    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SWAP_TIME_PROFILE", $SwapProfileValue, "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", $OrdinalProfileValue, "Process")
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
        $metricsPath = Join-Path $ResultDirectory.FullName `
            ("run-{0:D2}\metrics.json" -f $run)
        $metrics = Get-Content -Raw -LiteralPath $metricsPath |
            ConvertFrom-Json
        $rows += [pscustomobject]@{
            run = $run
            frames = [UInt64]$metrics.frames
            glide_share_percent = [double]$metrics.glide_share_percent
        }
    }
    return $rows
}

function Read-ProfileRuns
{
    param([System.IO.DirectoryInfo]$ResultDirectory)

    $rows = @()
    for ($run = 1; $run -le $Runs; ++$run)
    {
        $runName = "run-{0:D2}" -f $run
        $runDirectory = Join-Path $ResultDirectory.FullName $runName
        $text = Get-Content -Raw -LiteralPath `
            (Join-Path $runDirectory "combined.log")
        $metrics = Get-Content -Raw -LiteralPath `
            (Join-Path $runDirectory "metrics.json") |
            ConvertFrom-Json

        $summary = Get-LastMetricMatch $text `
            "Win32 Glide buffer swap timing enabled/calls/success/failure/clamped: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName swap summary"
        $cycles = Get-LastMetricMatch $text `
            "Win32 Glide buffer swap cycles setup/present/accounting/finalize/total/max-present: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName swap cycles"
        $requested = Get-LastMetricMatch $text `
            "Win32 Glide buffer swap requested interval zero/one/other/min/max/last: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName requested intervals"
        $sdl = Get-LastMetricMatch $text `
            "Win32 Glide buffer swap SDL interval queries/success/failure/value: (\d+)/(\d+)/(\d+)/(-?\d+)" `
            "$runName SDL interval"
        $ordinal = Get-LastMetricMatch $text `
            "Win32 Glide ordinal timing: ordinal=85 name=(\S+) count=(\d+) gate=(\d+) max=(\d+) rendezvous=(\d+) queue=(\d+) wake=(\d+) work=(\d+) complete=(\d+) residual=(\d+) backend_total=(\d+) direct=(\d+) direct_work=(\d+)" `
            "$runName ordinal 85"

        $calls = Get-UInt64 $summary 2
        $success = Get-UInt64 $summary 3
        $failure = Get-UInt64 $summary 4
        $clamped = Get-UInt64 $summary 5
        $setup = Get-UInt64 $cycles 1
        $present = Get-UInt64 $cycles 2
        $accounting = Get-UInt64 $cycles 3
        $finalize = Get-UInt64 $cycles 4
        $total = Get-UInt64 $cycles 5
        $maxPresent = Get-UInt64 $cycles 6
        $ordinalCount = Get-UInt64 $ordinal 2
        $ordinalRendezvous = Get-UInt64 $ordinal 5
        $ordinalWork = Get-UInt64 $ordinal 8

        if ($summary.Groups[1].Value -ne "true" -or
            $failure -ne 0 -or $clamped -ne 0)
        {
            throw "$runName swap profile is disabled, failed, or clamped"
        }
        if ($calls -ne $success -or
            $calls -ne $ordinalCount -or
            $calls -ne $ordinalRendezvous -or
            $calls -ne [UInt64]$metrics.frames)
        {
            throw "$runName swap/ordinal/frame counts differ"
        }
        if ($setup + $present + $accounting + $finalize -ne $total)
        {
            throw "$runName phase sum does not equal total"
        }
        if ((Get-UInt64 $requested 1) +
            (Get-UInt64 $requested 2) +
            (Get-UInt64 $requested 3) -ne $calls)
        {
            throw "$runName requested interval count does not equal calls"
        }
        if ((Get-UInt64 $sdl 1) -ne 1 -or
            (Get-UInt64 $sdl 2) -ne 1 -or
            (Get-UInt64 $sdl 3) -ne 0)
        {
            throw "$runName SDL interval query did not succeed exactly once"
        }
        $coverage = 100.0 * [double]$total / [double]$ordinalWork
        if ($coverage -lt 98.0 -or $coverage -gt 101.0)
        {
            throw "$runName ordinal 85 work coverage is outside 98-101%: $coverage"
        }

        $phases = [ordered]@{
            setup = $setup
            present = $present
            accounting = $accounting
            finalize = $finalize
        }
        $leadingPhase = (
            $phases.GetEnumerator() |
                Sort-Object Value -Descending |
                Select-Object -First 1).Name

        $rows += [pscustomobject]@{
            run = $run
            frames = [UInt64]$metrics.frames
            glide_share_percent = [double]$metrics.glide_share_percent
            calls = $calls
            setup_cycles = $setup
            present_cycles = $present
            accounting_cycles = $accounting
            finalize_cycles = $finalize
            total_cycles = $total
            max_present_cycles = $maxPresent
            ordinal_85_work_cycles = $ordinalWork
            ordinal_85_work_coverage_percent = $coverage
            setup_share_percent = 100.0 * [double]$setup / [double]$total
            present_share_percent = 100.0 * [double]$present / [double]$total
            accounting_share_percent =
                100.0 * [double]$accounting / [double]$total
            finalize_share_percent =
                100.0 * [double]$finalize / [double]$total
            leading_phase = $leadingPhase
            requested_zero_count = Get-UInt64 $requested 1
            requested_one_count = Get-UInt64 $requested 2
            requested_other_count = Get-UInt64 $requested 3
            requested_minimum = Get-UInt64 $requested 4
            requested_maximum = Get-UInt64 $requested 5
            requested_last = Get-UInt64 $requested 6
            observed_sdl_interval = [Int32]$sdl.Groups[4].Value
        }
    }
    return $rows
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
$controlRoot = Join-Path $sessionDirectory "control"
$profileRoot = Join-Path $sessionDirectory "profile"
New-Item -ItemType Directory -Path $controlRoot -Force | Out-Null
New-Item -ItemType Directory -Path $profileRoot -Force | Out-Null

$savedSwapProfile = [Environment]::GetEnvironmentVariable(
    "REPIU_GLIDE_SWAP_TIME_PROFILE", "Process")
$savedOrdinalProfile = [Environment]::GetEnvironmentVariable(
    "REPIU_GLIDE_ORDINAL_TIME_PROFILE", "Process")
try
{
    $controlResult = Invoke-ReleaseAxis $controlRoot $null $null
    $profileResult = Invoke-ReleaseAxis $profileRoot "1" "1"
}
finally
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SWAP_TIME_PROFILE", $savedSwapProfile, "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", $savedOrdinalProfile, "Process")
}

$controlRuns = @(Read-ControlRuns $controlResult)
$profileRuns = @(Read-ProfileRuns $profileResult)
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

$leadingPhases = @($profileRuns.leading_phase | Select-Object -Unique)
if ($leadingPhases.Count -ne 1)
{
    throw "Leading swap phase is not stable across runs: $leadingPhases"
}

$controlRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "control-runs.csv") `
    -NoTypeInformation -Encoding utf8
$profileRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "profile-runs.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    control_median_frames = $controlMedianFrames
    profile_median_frames = $profileMedianFrames
    frame_delta_percent = $frameDeltaPercent
    leading_phase = $leadingPhases[0]
    mean_present_share_percent =
        ($profileRuns.present_share_percent |
            Measure-Object -Average).Average
    mean_ordinal_85_work_coverage_percent =
        ($profileRuns.ordinal_85_work_coverage_percent |
            Measure-Object -Average).Average
    observed_sdl_intervals =
        @($profileRuns.observed_sdl_interval | Select-Object -Unique)
    control_result = $controlResult.FullName
    profile_result = $profileResult.FullName
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 354 Glide buffer-swap decomposition complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames control/profile/delta: {0:N0}/{1:N0}/{2:N2}%" -f `
    $controlMedianFrames, $profileMedianFrames, $frameDeltaPercent)
Write-Host ("Leading phase: {0}; mean present share: {1:N2}%" -f `
    $leadingPhases[0], $summary.mean_present_share_percent)
Write-Host ("Observed SDL interval(s): {0}" -f `
    ($summary.observed_sdl_intervals -join ","))
