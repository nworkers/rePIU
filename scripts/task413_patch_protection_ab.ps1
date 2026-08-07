param(
    [ValidateRange(1, 20)]
    [int]$RunsPerCondition = 3,

    [ValidateRange(5, 600)]
    [int]$DurationSeconds = 60,

    [string]$Target = "pumpit3",

    [string]$ReleaseDirectory = "",

    [string]$EepromSeed = "",

    [string]$OutputRoot = ""
)

# Task 413 A/B. `wide` restores the old whole-cache VirtualProtect on the AOT
# inline-cache patch path, `narrow` is the new page window. Both conditions run
# in one binary and one session, with the guest position census OFF because its
# runs are not quotable for wall time or frames.

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repositoryRoot "build\benchmarks\patch-protection-ab"
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

# Alternate the conditions so a machine that drifts during the session cannot
# hand one condition a better half of it.
$order = @()
for ($index = 0; $index -lt $RunsPerCondition; ++$index)
{
    $order += "wide"
    $order += "narrow"
}

$rows = @()
$runIndex = 0
foreach ($condition in $order)
{
    ++$runIndex
    $runName = "run-{0:D2}-{1}" -f $runIndex, $condition
    $runDirectory = Join-Path $sessionDirectory $runName
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    $eeprom = Join-Path $runDirectory "eeprom.dat"
    if (Test-Path -LiteralPath $EepromSeed)
    {
        Copy-Item -LiteralPath $EepromSeed -Destination $eeprom -Force
    }
    $logPath = Join-Path $runDirectory "combined.log"

    $env:REPIU_EXECUTION_BACKEND = "dynamic"
    $env:REPIU_EXECUTION_TIMEOUT_MS = ($DurationSeconds * 1000).ToString()
    $env:REPIU_EEPROM_PATH = $eeprom
    Remove-Item Env:\REPIU_GUEST_POSITION_CENSUS -ErrorAction SilentlyContinue
    if ($condition -eq "wide")
    {
        $env:REPIU_AOT_PATCH_WIDE_PROTECT = "1"
    }
    else
    {
        Remove-Item Env:\REPIU_AOT_PATCH_WIDE_PROTECT -ErrorAction SilentlyContinue
    }

    Write-Host ("[{0}] running {1}s" -f $runName, $DurationSeconds)
    & cmd /c "`"$loader`" $Target > `"$logPath`" 2>&1" | Out-Null

    $text = Get-Content -Raw -LiteralPath $logPath
    $pathTraceCount = ([regex]::Matches($text, "DOS path trace #")).Count
    $swap = Get-LastMatch $text "name=_GRBUFFERSWAP@4 count=\s*(\d+)"
    $frames = if ($null -eq $swap) { 0 } else { [UInt64]$swap.Groups[1].Value }
    $generations = Get-LastMatch $text `
        "Win32 AOT generation publishes/quarantines: (\d+)/(\d+)"
    $exceptions = Get-LastMatch $text `
        ("Win32 exception census single-step/breakpoint/access-violation/" +
         "other/total: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)")
    $patches = ([regex]::Matches($text, "icache patch #(\d+)"))
    $lastPatch = if ($patches.Count -eq 0) { 0 } else {
        [UInt64]$patches[$patches.Count - 1].Groups[1].Value }

    $classification =
        if ($frames -ge 100) { "healthy" }
        elseif (($pathTraceCount -le 6) -and ($frames -le 1)) { "stalled" }
        else { "slow" }

    $rows += [pscustomobject][ordered]@{
        run = $runIndex
        condition = $condition
        classification = $classification
        frames = $frames
        path_traces = $pathTraceCount
        publishes = if ($null -eq $generations) { 0 } else { [UInt64]$generations.Groups[1].Value }
        breakpoints = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[2].Value }
        port_io = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[4].Value }
        exceptions_total = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[5].Value }
        last_patch_counter = $lastPatch
        log = $logPath
    }

    # Correctness gate: the patch path must still be running in both conditions.
    if ($lastPatch -eq 0)
    {
        throw "$runName made no inline-cache patches; the A/B is not comparable"
    }

    Write-Host ("[{0}] {1}: frames={2} traces={3} breakpoints={4}" -f `
        $runName, $classification, $frames, $pathTraceCount,
        $rows[$rows.Count - 1].breakpoints)
}

$rows | Export-Csv -LiteralPath (Join-Path $sessionDirectory "runs.csv") `
    -NoTypeInformation -Encoding utf8

function Get-ConditionSummary
{
    param([string]$Condition)

    $subset = @($rows | Where-Object { $_.condition -eq $Condition })
    return [pscustomobject]@{
        condition = $Condition
        runs = $subset.Count
        healthy = @($subset | Where-Object { $_.classification -eq "healthy" }).Count
        stalled = @($subset | Where-Object { $_.classification -eq "stalled" }).Count
        median_frames = (@($subset | ForEach-Object { [double]$_.frames } |
            Sort-Object)[[int][Math]::Floor($subset.Count / 2)])
        median_breakpoints = (@($subset | ForEach-Object { [double]$_.breakpoints } |
            Sort-Object)[[int][Math]::Floor($subset.Count / 2)])
    }
}

$summary = [pscustomobject]@{
    runs_per_condition = $RunsPerCondition
    duration_seconds = $DurationSeconds
    wide = Get-ConditionSummary "wide"
    narrow = Get-ConditionSummary "narrow"
    session = $sessionDirectory
}
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 413 patch protection A/B complete"
Write-Host ("Result: {0}" -f $sessionDirectory)
Write-Host ("wide   : healthy {0}/{1}, median frames {2}" -f `
    $summary.wide.healthy, $summary.wide.runs, $summary.wide.median_frames)
Write-Host ("narrow : healthy {0}/{1}, median frames {2}" -f `
    $summary.narrow.healthy, $summary.narrow.runs, $summary.narrow.median_frames)
