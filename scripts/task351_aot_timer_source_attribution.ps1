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
        "build\benchmarks\aot-timer-source"
}
$baseScript = Join-Path $PSScriptRoot `
    "task347_release_axis_reattribution.ps1"
if (-not (Test-Path -LiteralPath $baseScript -PathType Leaf))
{
    throw "Task 347 measurement script is missing: $baseScript"
}

$before = @{}
if (Test-Path -LiteralPath $OutputRoot -PathType Container)
{
    Get-ChildItem -LiteralPath $OutputRoot -Directory |
        ForEach-Object { $before[$_.FullName] = $true }
}

$savedProfile =
    [Environment]::GetEnvironmentVariable(
        "REPIU_AOT_TIMER_SOURCE_PROFILE", "Process")
try
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_AOT_TIMER_SOURCE_PROFILE", "1", "Process")
    $arguments = @{
        Runs = $Runs
        DurationSeconds = $DurationSeconds
        Target = $Target
        OutputRoot = $OutputRoot
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
}
finally
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_AOT_TIMER_SOURCE_PROFILE", $savedProfile, "Process")
}

$resultDirectory = Get-ChildItem -LiteralPath $OutputRoot -Directory |
    Where-Object { -not $before.ContainsKey($_.FullName) } |
    Sort-Object Name -Descending |
    Select-Object -First 1
if ($null -eq $resultDirectory)
{
    throw "Task 347 did not create a new result directory"
}

$sourceRows = @()
$runRows = @()
for ($run = 1; $run -le $Runs; ++$run)
{
    $runName = "run-{0:D2}" -f $run
    $runDirectory = Join-Path $resultDirectory.FullName $runName
    $combinedPath = Join-Path $runDirectory "combined.log"
    $metricsPath = Join-Path $runDirectory "metrics.json"
    if (-not (Test-Path -LiteralPath $combinedPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $metricsPath -PathType Leaf))
    {
        throw "Required Task 347 run artifact is missing: $runName"
    }
    $text = Get-Content -Raw -LiteralPath $combinedPath
    $metrics = Get-Content -Raw -LiteralPath $metricsPath |
        ConvertFrom-Json

    $profileMatches = [regex]::Matches(
        $text,
        "Win32 AOT timer source profile enabled/entries/overflow/attributed-ticks: (true|false)/(\d+)/(\d+)/(\d+)")
    if ($profileMatches.Count -eq 0)
    {
        throw "$runName did not report the timer-source profile"
    }
    $profile = $profileMatches[$profileMatches.Count - 1]
    if ($profile.Groups[1].Value -ne "true")
    {
        throw "$runName did not enable the timer-source profile"
    }
    $entryCount = [uint32]$profile.Groups[2].Value
    $overflow = [uint32]$profile.Groups[3].Value
    $attributedTicks = [uint64]$profile.Groups[4].Value
    if ($entryCount -eq 0 -or $overflow -ne 0)
    {
        throw "$runName has empty or overflowing timer-source data"
    }

    $topMatches = [regex]::Matches(
        $text,
        "Win32 AOT timer source top (\d+) guest/trap/injected/deferred/attributed-ticks/first-tick/last-tick: 0x([0-9A-Fa-f]{8})/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)")
    $topTickSum = [uint64]0
    foreach ($top in $topMatches)
    {
        $ticks = [uint64]$top.Groups[6].Value
        $topTickSum += $ticks
        $sourceRows += [pscustomobject][ordered]@{
            run = $run
            rank = [uint32]$top.Groups[1].Value
            guest_source =
                "0x" + $top.Groups[2].Value.ToUpperInvariant()
            trap_count = [uint32]$top.Groups[3].Value
            injected_count = [uint32]$top.Groups[4].Value
            deferred_count = [uint32]$top.Groups[5].Value
            attributed_ticks = $ticks
            first_global_tick = [uint32]$top.Groups[7].Value
            last_global_tick = [uint32]$top.Groups[8].Value
        }
    }
    if ($topMatches.Count -ne $entryCount)
    {
        throw "$runName source rows do not cover the complete profile"
    }
    if ($topTickSum -ne $attributedTicks)
    {
        throw "$runName source ticks do not match the profile total"
    }

    $pitDivisor = [uint32]$metrics.pit_divisor
    $sourceSeconds =
        [double]$attributedTicks * [double]$pitDivisor / 1193280.0
    $runRows += [pscustomobject][ordered]@{
        run = $run
        frames = [uint64]$metrics.frames
        entry_count = $entryCount
        overflow_count = $overflow
        attributed_ticks = $attributedTicks
        all_safe_point_source_seconds = $sourceSeconds
        all_safe_point_source_wall_percent =
            100.0 * $sourceSeconds / [double]$DurationSeconds
        guest_execution_share_percent =
            [double]$metrics.guest_execution_share_percent
        pit_divisor = $pitDivisor
        pit_frequency_hz = [double]$metrics.pit_frequency_hz
        log_path = $combinedPath
    }
}

function Get-Median
{
    param([double[]]$Values)

    $ordered = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1)
    {
        return [double]$ordered[$middle]
    }
    return ([double]$ordered[$middle - 1] +
            [double]$ordered[$middle]) / 2.0
}

$aggregate = @(
    $sourceRows |
        Group-Object -Property guest_source |
        ForEach-Object {
            [pscustomobject][ordered]@{
                guest_source = $_.Name
                runs_present = $_.Count
                trap_count_sum =
                    ($_.Group | Measure-Object trap_count -Sum).Sum
                injected_count_sum =
                    ($_.Group | Measure-Object injected_count -Sum).Sum
                deferred_count_sum =
                    ($_.Group | Measure-Object deferred_count -Sum).Sum
                attributed_ticks_sum =
                    ($_.Group | Measure-Object attributed_ticks -Sum).Sum
            }
        } |
        Sort-Object -Property attributed_ticks_sum -Descending
)

$profileSummary = [pscustomobject][ordered]@{
    generated_at = (Get-Date).ToString("o")
    target = $Target
    runs = $Runs
    duration_seconds = $DurationSeconds
    frames_median = Get-Median @(
        $runRows | ForEach-Object { [double]$_.frames })
    frames_min = ($runRows | Measure-Object frames -Minimum).Minimum
    frames_max = ($runRows | Measure-Object frames -Maximum).Maximum
    all_safe_point_source_seconds_median = Get-Median @(
        $runRows |
            ForEach-Object {
                [double]$_.all_safe_point_source_seconds
            })
    all_safe_point_source_wall_percent_median = Get-Median @(
        $runRows |
            ForEach-Object {
                [double]$_.all_safe_point_source_wall_percent
            })
    guest_execution_share_percent_median = Get-Median @(
        $runRows |
            ForEach-Object {
                [double]$_.guest_execution_share_percent
            })
    result_directory = $resultDirectory.FullName
}

$runRows | Export-Csv `
    -LiteralPath (Join-Path $resultDirectory.FullName "timer-source-runs.csv") `
    -NoTypeInformation -Encoding UTF8
$sourceRows | Export-Csv `
    -LiteralPath (Join-Path $resultDirectory.FullName "timer-sources.csv") `
    -NoTypeInformation -Encoding UTF8
$aggregate | Export-Csv `
    -LiteralPath `
        (Join-Path $resultDirectory.FullName "timer-sources-aggregate.csv") `
    -NoTypeInformation -Encoding UTF8
$profileSummary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath `
        (Join-Path $resultDirectory.FullName "timer-source-summary.json") `
        -Encoding UTF8

Write-Host ""
Write-Host "Task 351 timer-source attribution complete"
Write-Host "Result directory: $($resultDirectory.FullName)"
Write-Host ("All safe-point source median: {0:N3}s ({1:N2}% wall)" -f
            $profileSummary.all_safe_point_source_seconds_median,
            $profileSummary.all_safe_point_source_wall_percent_median)
