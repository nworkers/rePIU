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

# Task 414 A/B. `off` is the old behaviour (one CPU fault per delay-loop port
# read), `on` batches the loop down to two. Both live in one binary, the census
# stays off because its runs are not quotable, and the EEPROM is isolated per
# run.

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repositoryRoot "build\benchmarks\delay-loop-ab"
}
if ([string]::IsNullOrWhiteSpace($ReleaseDirectory))
{
    $ReleaseDirectory = Join-Path $repositoryRoot "build\win32_x86_debug\Release"
}
if ([string]::IsNullOrWhiteSpace($EepromSeed))
{
    $EepromSeed = Join-Path $repositoryRoot "eeprom.dat"
}
$loader = Join-Path $ReleaseDirectory "repiu_loader_win32.exe"
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

$order = @()
for ($index = 0; $index -lt $RunsPerCondition; ++$index)
{
    $order += "off"
    $order += "on"
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
    $env:REPIU_GUEST_POSITION_CENSUS = $null
    if ($condition -eq "off")
    {
        $env:REPIU_PORT_IO_DELAY_LOOP = "0"
    }
    else
    {
        $env:REPIU_PORT_IO_DELAY_LOOP = $null
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
    $ticks = Get-LastMatch $text `
        ("Win32 timer tick delivery backlog-enabled/due/injected/coalesced/" +
         "dropped/deferred/max-backlog/remaining: \w+/(\d+)/(\d+)/(\d+)/(\d+)")
    $batch = Get-LastMatch $text `
        ("Win32 port I/O delay loop enabled/attempts/batches/skipped/max: " +
         "(\w+)/(\d+)/(\d+)/(\d+)/(\d+)")
    $delayAddress = Get-LastMatch $text `
        "port I/O address #\d+ guest/count/cache/arena/mapped/reentry: 0x0301DB22/(\d+)/"

    $due = if ($null -eq $ticks) { 0 } else { [UInt64]$ticks.Groups[1].Value }
    $injected = if ($null -eq $ticks) { 0 } else { [UInt64]$ticks.Groups[2].Value }

    $rows += [pscustomobject][ordered]@{
        run = $runIndex
        condition = $condition
        classification =
            if ($frames -ge 100) { "healthy" }
            elseif (($pathTraceCount -le 6) -and ($frames -le 1)) { "stalled" }
            else { "slow" }
        frames = $frames
        path_traces = $pathTraceCount
        publishes = if ($null -eq $generations) { 0 } else { [UInt64]$generations.Groups[1].Value }
        quarantines = if ($null -eq $generations) { 0 } else { [UInt64]$generations.Groups[2].Value }
        single_step = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[1].Value }
        breakpoints = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[2].Value }
        port_io = if ($null -eq $exceptions) { 0 } else { [UInt64]$exceptions.Groups[4].Value }
        delay_loop_faults = if ($null -eq $delayAddress) { 0 } else { [UInt64]$delayAddress.Groups[1].Value }
        ticks_due = $due
        ticks_injected = $injected
        tick_delivery_percent = if ($due -eq 0) { 0 } else { [Math]::Round(100.0 * $injected / $due, 1) }
        batches = if ($null -eq $batch) { 0 } else { [UInt64]$batch.Groups[3].Value }
        skipped_iterations = if ($null -eq $batch) { 0 } else { [UInt64]$batch.Groups[4].Value }
        log = $logPath
    }

    $row = $rows[$rows.Count - 1]
    # Mechanism gate. Batching with the switch off is always a defect. Batching
    # nothing with it on is expected for titles without the pattern -- pumpit1
    # attempts thousands of matches and batches none -- so that case warns
    # instead of failing, and the A/B for such a title is a regression check.
    if ($condition -eq "off" -and $row.batches -ne 0)
    {
        throw "$runName batched with the switch off"
    }
    if ($condition -eq "on" -and $row.batches -eq 0)
    {
        Write-Host ("[{0}] no loop matched; this title has no batchable delay loop" -f $runName)
    }

    Write-Host ("[{0}] {1}: frames={2} traces={3} delay-loop-faults={4} ticks={5}%" -f `
        $runName, $row.classification, $row.frames, $row.path_traces,
        $row.delay_loop_faults, $row.tick_delivery_percent)
}

$rows | Export-Csv -LiteralPath (Join-Path $sessionDirectory "runs.csv") `
    -NoTypeInformation -Encoding utf8

function Get-ConditionSummary
{
    param([string]$Condition)

    $subset = @($rows | Where-Object { $_.condition -eq $Condition })
    $median = {
        param($values)
        $sorted = @($values | Sort-Object)
        return $sorted[[int][Math]::Floor($sorted.Count / 2)]
    }
    return [pscustomobject]@{
        condition = $Condition
        runs = $subset.Count
        healthy = @($subset | Where-Object { $_.classification -eq "healthy" }).Count
        stalled = @($subset | Where-Object { $_.classification -eq "stalled" }).Count
        median_frames = & $median @($subset | ForEach-Object { [double]$_.frames })
        median_delay_loop_faults = & $median @($subset | ForEach-Object { [double]$_.delay_loop_faults })
        median_tick_delivery = & $median @($subset | ForEach-Object { [double]$_.tick_delivery_percent })
        median_single_step = & $median @($subset | ForEach-Object { [double]$_.single_step })
    }
}

$summary = [pscustomobject]@{
    runs_per_condition = $RunsPerCondition
    duration_seconds = $DurationSeconds
    target = $Target
    off = Get-ConditionSummary "off"
    on = Get-ConditionSummary "on"
    session = $sessionDirectory
}
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 414 delay-loop A/B complete"
Write-Host ("Result: {0}" -f $sessionDirectory)
Write-Host ("off: healthy {0}/{1}, frames {2}, delay-loop faults {3}, ticks {4}%" -f `
    $summary.off.healthy, $summary.off.runs, $summary.off.median_frames,
    $summary.off.median_delay_loop_faults, $summary.off.median_tick_delivery)
Write-Host ("on : healthy {0}/{1}, frames {2}, delay-loop faults {3}, ticks {4}%" -f `
    $summary.on.healthy, $summary.on.runs, $summary.on.median_frames,
    $summary.on.median_delay_loop_faults, $summary.on.median_tick_delivery)
