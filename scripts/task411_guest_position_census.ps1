param(
    [ValidateRange(1, 40)]
    [int]$Runs = 6,

    [ValidateRange(5, 600)]
    [int]$DurationSeconds = 60,

    [string]$Target = "pumpit3",

    [ValidateRange(1, 1000)]
    [int]$IntervalMilliseconds = 10,

    # 0 leaves the census off, which is how the control run is taken.
    [ValidateSet("on", "off")]
    [string]$Census = "on",

    [string]$ReleaseDirectory = "",

    [string]$EepromSeed = "",

    [string]$OutputRoot = ""
)

# Task 411. Repeats the pumpit3 stall reproduction with the guest position
# census enabled and classifies each run stalled or healthy by the signature in
# docs/guides/pumpit3-stall-reproduction.md. The census is only readable when
# its own checks hold, so `sum == total` and `overflow == 0` are enforced here
# rather than left to the reader.

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repositoryRoot "build\benchmarks\guest-position-census"
}
if ([string]::IsNullOrWhiteSpace($ReleaseDirectory))
{
    $ReleaseDirectory = Join-Path $repositoryRoot "build\win32_x86_debug\Release"
}
if ([string]::IsNullOrWhiteSpace($EepromSeed))
{
    $EepromSeed = Join-Path $repositoryRoot "eeprom.dat"
}

$loader = Join-Path $ReleaseDirectory "repiu.exe"
if (-not (Test-Path -LiteralPath $loader))
{
    throw "Loader is missing: $loader"
}
# The loader resolves roms\ and build\runtime_mounts\ relative to the working
# directory, so it must run from the repository root regardless of where the
# script was invoked from.
Set-Location -LiteralPath $repositoryRoot

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
New-Item -ItemType Directory -Path $sessionDirectory -Force | Out-Null

function Get-LastMatch
{
    param([string]$Text, [string]$Pattern)

    $found = [regex]::Matches($Text, $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($found.Count -eq 0) { return $null }
    return $found[$found.Count - 1]
}

$rows = @()
for ($run = 1; $run -le $Runs; ++$run)
{
    $runName = "run-{0:D2}" -f $run
    $runDirectory = Join-Path $sessionDirectory $runName
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null

    # Per-run EEPROM: sharing one file leaks persistent state between runs and
    # invalidates the comparison (reproduction guide section 1).
    $eeprom = Join-Path $runDirectory "eeprom.dat"
    if (Test-Path -LiteralPath $EepromSeed)
    {
        Copy-Item -LiteralPath $EepromSeed -Destination $eeprom -Force
    }
    $logPath = Join-Path $runDirectory "combined.log"
    $dumpPath = Join-Path $runDirectory "guest_position_census.txt"

    $env:REPIU_EXECUTION_BACKEND = "dynamic"
    $env:REPIU_EXECUTION_TIMEOUT_MS = ($DurationSeconds * 1000).ToString()
    $env:REPIU_EEPROM_PATH = $eeprom
    if ($Census -eq "on")
    {
        $env:REPIU_GUEST_POSITION_CENSUS = "1"
        $env:REPIU_GUEST_POSITION_CENSUS_MS = $IntervalMilliseconds.ToString()
        $env:REPIU_GUEST_POSITION_CENSUS_DUMP = $dumpPath
    }
    else
    {
        Remove-Item Env:\REPIU_GUEST_POSITION_CENSUS -ErrorAction SilentlyContinue
        Remove-Item Env:\REPIU_GUEST_POSITION_CENSUS_MS -ErrorAction SilentlyContinue
        Remove-Item Env:\REPIU_GUEST_POSITION_CENSUS_DUMP -ErrorAction SilentlyContinue
    }

    Write-Host ("[{0}] running {1} for {2}s" -f $runName, $Target, $DurationSeconds)
    # cmd redirection, not PowerShell's: PowerShell truncates at the console
    # width and cuts census values in half (reproduction guide section 1).
    & cmd /c "`"$loader`" $Target > `"$logPath`" 2>&1" | Out-Null

    $text = Get-Content -Raw -LiteralPath $logPath
    $pathTraceCount = ([regex]::Matches($text, "DOS path trace #")).Count
    $swap = Get-LastMatch $text "name=_GRBUFFERSWAP@4 count=\s*(\d+)"
    $frames = if ($null -eq $swap) { 0 } else { [UInt64]$swap.Groups[1].Value }
    $generations = Get-LastMatch $text `
        "Win32 AOT generation publishes/quarantines: (\d+)/(\d+)"
    $censusLine = Get-LastMatch $text `
        ("Win32 guest position census enabled/total/distinct/overflow/" +
         "capture-failures/interval-ms: (\w+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)")
    # Task 412 lines. Absent on builds before that task, so a missing match is
    # tolerated and reported as zero rather than failing the run.
    $threadTimeLine = Get-LastMatch $text `
        ("Win32 guest position thread time valid/kernel-ms/user-ms/wall-ms/" +
         "cpu-share: (\w+)/([\d.]+)/([\d.]+)/(\d+)/([\d.]+)%")
    $scanLine = Get-LastMatch $text `
        ("Win32 guest position host scan samples/sited/no-site/failed/" +
         "distinct/overflow/parts-match: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\w+)")
    $originLine = Get-LastMatch $text `
        ("Win32 guest position origin arena/cache-mapped/cache-unmapped/host/" +
         "sum-matches-total: (\d+)/(\d+)/(\d+)/(\d+)/(\w+)")

    # Three classes, not two. A run that renders a handful of frames is neither
    # the documented stall (six path traces, 79 publishes) nor a healthy run
    # (1,300+ frames in 60 s); calling it "healthy" hid exactly that in the
    # first Task 411 batch.
    $classification =
        if ($frames -ge 100) { "healthy" }
        elseif (($pathTraceCount -le 6) -and ($frames -le 1)) { "stalled" }
        else { "slow" }
    $row = [pscustomobject][ordered]@{
        run = $run
        classification = $classification
        path_traces = $pathTraceCount
        frames = $frames
        publishes = if ($null -eq $generations) { 0 } else { [UInt64]$generations.Groups[1].Value }
        quarantines = if ($null -eq $generations) { 0 } else { [UInt64]$generations.Groups[2].Value }
        census_enabled = if ($null -eq $censusLine) { "missing" } else { $censusLine.Groups[1].Value }
        census_total = if ($null -eq $censusLine) { 0 } else { [UInt64]$censusLine.Groups[2].Value }
        census_distinct = if ($null -eq $censusLine) { 0 } else { [UInt64]$censusLine.Groups[3].Value }
        census_overflow = if ($null -eq $censusLine) { 0 } else { [UInt64]$censusLine.Groups[4].Value }
        census_capture_failures = if ($null -eq $censusLine) { 0 } else { [UInt64]$censusLine.Groups[5].Value }
        arena = if ($null -eq $originLine) { 0 } else { [UInt64]$originLine.Groups[1].Value }
        cache_mapped = if ($null -eq $originLine) { 0 } else { [UInt64]$originLine.Groups[2].Value }
        cache_unmapped = if ($null -eq $originLine) { 0 } else { [UInt64]$originLine.Groups[3].Value }
        host = if ($null -eq $originLine) { 0 } else { [UInt64]$originLine.Groups[4].Value }
        sum_matches_total = if ($null -eq $originLine) { "missing" } else { $originLine.Groups[5].Value }
        cpu_share_percent = if ($null -eq $threadTimeLine) { 0 } else { [double]$threadTimeLine.Groups[5].Value }
        thread_kernel_ms = if ($null -eq $threadTimeLine) { 0 } else { [double]$threadTimeLine.Groups[2].Value }
        thread_user_ms = if ($null -eq $threadTimeLine) { 0 } else { [double]$threadTimeLine.Groups[3].Value }
        host_scan_sited = if ($null -eq $scanLine) { 0 } else { [UInt64]$scanLine.Groups[2].Value }
        host_scan_no_site = if ($null -eq $scanLine) { 0 } else { [UInt64]$scanLine.Groups[3].Value }
        host_scan_failed = if ($null -eq $scanLine) { 0 } else { [UInt64]$scanLine.Groups[4].Value }
        host_scan_parts_match = if ($null -eq $scanLine) { "missing" } else { $scanLine.Groups[7].Value }
        log = $logPath
        dump = if (Test-Path -LiteralPath $dumpPath) { $dumpPath } else { "" }
    }
    $rows += $row

    if ($Census -eq "on")
    {
        # Pre-registered reading rules: a census that fails either check is not
        # read as a distribution (design section 5).
        if ($row.census_enabled -ne "true")
        {
            throw "$runName census did not run"
        }
        if ($row.sum_matches_total -ne "true")
        {
            throw "$runName origin counts do not sum to the total"
        }
        if ($row.census_overflow -ne 0)
        {
            throw "$runName census overflowed its table"
        }
        # Task 412's own gate: a scan whose parts do not sum to its samples is
        # not read as a distribution.
        if ($row.host_scan_parts_match -eq "False")
        {
            throw "$runName host scan parts do not sum to its samples"
        }
    }

    Write-Host ("[{0}] {1}: frames={2} traces={3} samples={4} cpu={5}%" -f `
        $runName, $row.classification, $row.frames, $row.path_traces,
        $row.census_total, $row.cpu_share_percent)
}

$rows | Export-Csv -LiteralPath (Join-Path $sessionDirectory "runs.csv") `
    -NoTypeInformation -Encoding utf8

$stalledRuns = @($rows | Where-Object { $_.classification -eq "stalled" })
$healthyRuns = @($rows | Where-Object { $_.classification -eq "healthy" })
$slowRuns = @($rows | Where-Object { $_.classification -eq "slow" })
$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    census = $Census
    interval_ms = $IntervalMilliseconds
    stalled_runs = $stalledRuns.Count
    healthy_runs = $healthyRuns.Count
    slow_runs = $slowRuns.Count
    median_frames = ((@($rows | ForEach-Object { [double]$_.frames }) |
        Sort-Object)[[int][Math]::Floor($Runs / 2)])
    session = $sessionDirectory
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 411 guest position census complete"
Write-Host ("Result: {0}" -f $sessionDirectory)
Write-Host ("Stalled {0} / healthy {1} / slow {2} of {3}" -f `
    $stalledRuns.Count, $healthyRuns.Count, $slowRuns.Count, $Runs)
