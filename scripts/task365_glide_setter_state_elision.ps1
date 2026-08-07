param(
    [ValidateRange(1, 20)]
    [int]$Runs = 3,

    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 60,

    [ValidateRange(0, 600)]
    [int]$VisualDurationSeconds = 30,

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
        "build\benchmarks\glide-setter-elision"
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

# The seven batch-one ordinals from the Task 365 design, by export ordinal (not
# gate id -- they are different numbering spaces).
$elisionOrdinals = @(91, 79, 89, 82, 101, 94, 96)

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

function Get-AllMetricMatches
{
    param(
        [string]$Text,
        [string]$Pattern
    )

    return @([regex]::Matches(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline))
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

$censusLinePattern =
    "Win32 Glide setter census: ordinal=(\d+) name=(\S+) calls=(\d+) " +
    "first=(\d+) same=(\d+) changed=(\d+) failure=(\d+) unsupported=(\d+) " +
    "key_overflow=(\d+) distinct=(\d+) distinct_overflow=(\d+) max_run=(\d+) " +
    "max_frame_calls=(\d+) max_frame_changes=(\d+) elided=(\d+) applied=(\d+)"

function Invoke-Configuration
{
    param(
        [string]$ResultRoot,
        [string]$ElideValue
    )

    # Census and ordinal timing stay on in BOTH configurations, so their cost is
    # present on both sides and the frame delta isolates the elision. Gate E1 also
    # needs the census in the elide-on run.
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SETTER_ELIDE", $ElideValue, "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SETTER_CENSUS", "1", "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", "1", "Process")
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
        [bool]$ExpectElision
    )

    $runRows = @()
    $setterRows = @()
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

        $elision = Get-LastMetricMatch $text `
            "Win32 Glide setter elision enabled/entries/elided/applied/voided/invalidations/ordinal-overflow/texture-generation: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName elision summary"
        $census = Get-LastMetricMatch $text `
            "Win32 Glide setter census enabled/entries/calls/first/same/changed/failure/unsupported: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName census summary"
        $gate = Get-LastMetricMatch $text `
            "Win32 Glide gate entries/handled/ESP: (\d+)/(\d+)/" `
            "$runName gate counts"
        $ordinalSummary = Get-LastMetricMatch $text `
            "Win32 Glide ordinal timing enabled/entries/completed/overflow/clamped: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName ordinal summary"

        $elisionEnabled = $elision.Groups[1].Value -eq "true"
        $elidedTotal = Get-UInt64 $elision 3
        if ($ExpectElision)
        {
            if (-not $elisionEnabled -or $elidedTotal -eq 0)
            {
                throw "$runName expected elision to be active"
            }
        }
        elseif ($elidedTotal -ne 0)
        {
            throw "$runName elided calls with the kill switch set"
        }
        # E4 stability: no clamp or overflow from either instrument.
        if ((Get-UInt64 $elision 7) -ne 0 -or
            (Get-UInt64 $ordinalSummary 4) -ne 0 -or
            (Get-UInt64 $ordinalSummary 5) -ne 0)
        {
            throw "$runName reported an overflow or clamped sample"
        }
        # E3 ABI: a completed ordinal is always a handled gate.
        if ((Get-UInt64 $ordinalSummary 3) -gt (Get-UInt64 $gate 2))
        {
            throw "$runName completed ordinals exceed handled gates"
        }
        $censusCalls = Get-UInt64 $census 3
        if ((Get-UInt64 $census 4) + (Get-UInt64 $census 5) +
            (Get-UInt64 $census 6) + (Get-UInt64 $census 7) +
            (Get-UInt64 $census 8) -ne $censusCalls)
        {
            throw "$runName census outcome classes do not partition calls"
        }

        $censusLines = Get-AllMetricMatches $text $censusLinePattern
        $byOrdinal = @{}
        foreach ($line in $censusLines)
        {
            $byOrdinal[[string](Get-UInt64 $line 1)] = $line
        }

        $targetCalls = [UInt64]0
        $targetSame = [UInt64]0
        $targetElided = [UInt64]0
        foreach ($ordinal in $elisionOrdinals)
        {
            $key = [string]$ordinal
            if (-not $byOrdinal.ContainsKey($key))
            {
                throw "$runName is missing census data for ordinal $ordinal"
            }
            $line = $byOrdinal[$key]
            $calls = Get-UInt64 $line 3
            $same = Get-UInt64 $line 5
            $elided = Get-UInt64 $line 15
            $targetCalls += $calls
            $targetSame += $same
            $targetElided += $elided
            # E1, the decisive gate. The census is a pure observer, so if what it
            # independently counted as an exact repeat equals what was actually
            # skipped, the elision fired on observed duplicates and nothing else.
            if ($ExpectElision -and $elided -ne $same)
            {
                throw ("$runName ordinal $ordinal elided $elided against " +
                       "$same observed duplicates (gate E1)")
            }
            if (-not $ExpectElision -and $elided -ne 0)
            {
                throw "$runName ordinal $ordinal elided with elision off"
            }
            $setterRows += [pscustomobject][ordered]@{
                configuration = $Label
                run = $run
                ordinal = $ordinal
                name = $line.Groups[2].Value
                calls = $calls
                first = Get-UInt64 $line 4
                same = $same
                changed = Get-UInt64 $line 6
                failure = Get-UInt64 $line 7
                unsupported = Get-UInt64 $line 8
                elided = $elided
                applied = Get-UInt64 $line 16
                repetition_rate_percent =
                    100.0 * [double]$same / [double]$calls
            }
        }
        if ($ExpectElision -and $targetElided -ne $targetSame)
        {
            throw "$runName aggregate elided/observed mismatch (gate E1)"
        }

        $runRows += [pscustomobject][ordered]@{
            configuration = $Label
            run = $run
            frames = [UInt64]$metrics.frames
            guest_run_cycles = [UInt64]$metrics.guest_run_cycles
            glide_cycles = [UInt64]$metrics.glide_cycles
            glide_share_percent = [double]$metrics.glide_share_percent
            guest_execution_share_percent =
                [double]$metrics.guest_execution_share_percent
            gate_entries = Get-UInt64 $gate 1
            gate_handled = Get-UInt64 $gate 2
            census_calls = $censusCalls
            census_failure = Get-UInt64 $census 7
            census_unsupported = Get-UInt64 $census 8
            target_calls = $targetCalls
            target_same = $targetSame
            target_elided = $targetElided
            elision_enabled = $elisionEnabled
            elision_applied = Get-UInt64 $elision 4
            elision_voided = Get-UInt64 $elision 5
            elision_invalidations = Get-UInt64 $elision 6
        }
    }
    return [pscustomobject]@{
        runs = $runRows
        setters = $setterRows
    }
}

# Phase B: a short pass with back-buffer sampling and frame dumps enabled in both
# configurations. Kept out of the performance phase because glReadPixels would
# perturb the very number phase A measures.
function Invoke-VisualPass
{
    param(
        [string]$Directory,
        [string]$ElideValue
    )

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $eeprom = Join-Path $Directory "eeprom.dat"
    Copy-Item -LiteralPath $EepromSeed -Destination $eeprom
    $saved = @{}
    $names = @(
        "REPIU_EXECUTION_BACKEND", "REPIU_EXECUTION_TIMEOUT_MS",
        "REPIU_EEPROM_PATH", "REPIU_GLIDE_SETTER_ELIDE",
        "REPIU_GLIDE_PIXEL_DIAG", "REPIU_GLIDE_SETTER_CENSUS",
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", "REPIU_EXECUTION_TIME_PROFILE")
    foreach ($name in $names)
    {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }
    try
    {
        [Environment]::SetEnvironmentVariable(
            "REPIU_EXECUTION_BACKEND", "dynamic", "Process")
        [Environment]::SetEnvironmentVariable(
            "REPIU_EXECUTION_TIMEOUT_MS",
            ([string]($VisualDurationSeconds * 1000)), "Process")
        [Environment]::SetEnvironmentVariable(
            "REPIU_EEPROM_PATH", $eeprom, "Process")
        [Environment]::SetEnvironmentVariable(
            "REPIU_GLIDE_SETTER_ELIDE", $ElideValue, "Process")
        [Environment]::SetEnvironmentVariable(
            "REPIU_GLIDE_PIXEL_DIAG", "1", "Process")
        foreach ($name in @("REPIU_GLIDE_SETTER_CENSUS",
                            "REPIU_GLIDE_ORDINAL_TIME_PROFILE",
                            "REPIU_EXECUTION_TIME_PROFILE"))
        {
            [Environment]::SetEnvironmentVariable($name, $null, "Process")
        }
        $process = Start-Process `
            -FilePath (Join-Path $ReleaseDirectory "repiu.exe") `
            -ArgumentList @($Target) `
            -WorkingDirectory $repositoryRoot `
            -RedirectStandardOutput (Join-Path $Directory "stdout.log") `
            -RedirectStandardError (Join-Path $Directory "stderr.log") `
            -Wait -PassThru
        if ($process.ExitCode -ne 0)
        {
            throw "Visual pass failed with exit code $($process.ExitCode)"
        }
    }
    finally
    {
        foreach ($name in $names)
        {
            [Environment]::SetEnvironmentVariable(
                $name, $saved[$name], "Process")
        }
    }

    $text = Get-Content -Raw -LiteralPath (Join-Path $Directory "stderr.log")
    $samples = Get-AllMetricMatches $text `
        "Glide swap #(\d+) non-black pixels=(\d+)/(\d+) avg-rgb=(\d+),(\d+),(\d+)"
    if ($samples.Count -eq 0)
    {
        throw "Visual pass produced no back-buffer samples"
    }
    $nonBlack = @()
    $red = @()
    $green = @()
    $blue = @()
    foreach ($sample in $samples)
    {
        $total = [double](Get-UInt64 $sample 3)
        if ($total -le 0.0) { continue }
        $nonBlack += 100.0 * [double](Get-UInt64 $sample 2) / $total
        $red += [double](Get-UInt64 $sample 4)
        $green += [double](Get-UInt64 $sample 5)
        $blue += [double](Get-UInt64 $sample 6)
    }
    # Keyed by swap number so the two configurations can be matched under a phase
    # offset, which is a far stronger check than comparing overall means.
    $bySwap = @{}
    foreach ($sample in $samples)
    {
        $bySwap[[int](Get-UInt64 $sample 1)] =
            "$($sample.Groups[2].Value)|$($sample.Groups[4].Value)," +
            "$($sample.Groups[5].Value),$($sample.Groups[6].Value)"
    }
    return [pscustomobject]@{
        sample_count = $samples.Count
        mean_non_black_percent = ($nonBlack | Measure-Object -Average).Average
        mean_red = ($red | Measure-Object -Average).Average
        mean_green = ($green | Measure-Object -Average).Average
        mean_blue = ($blue | Measure-Object -Average).Average
        by_swap = $bySwap
    }
}

# Gate E7. If the elision is correct the same frames are drawn, just reached
# sooner, so matching the two swap sequences under a small phase offset should
# produce a sharp peak in exact statistic matches. A broken mask or blend matches
# at no offset. Byte-diffing whole back buffers across runs is impossible here
# because run progress differs, and there is no back-buffer screenshot facility.
function Measure-SequenceIdentity
{
    param(
        [hashtable]$Off,
        [hashtable]$On
    )

    $rows = @()
    foreach ($shift in -3..3)
    {
        $matched = 0
        $compared = 0
        foreach ($swap in $On.Keys)
        {
            if (-not $Off.ContainsKey($swap + $shift)) { continue }
            ++$compared
            if ($On[$swap] -eq $Off[$swap + $shift]) { ++$matched }
        }
        $percent = 0.0
        if ($compared -gt 0)
        {
            $percent = 100.0 * [double]$matched / [double]$compared
        }
        $rows += [pscustomobject]@{
            shift = $shift
            compared = $compared
            matched = $matched
            identical_percent = $percent
        }
    }
    return @($rows)
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
$offRoot = Join-Path $sessionDirectory "elide-off"
$onRoot = Join-Path $sessionDirectory "elide-on"
New-Item -ItemType Directory -Path $offRoot -Force | Out-Null
New-Item -ItemType Directory -Path $onRoot -Force | Out-Null

$environmentNames = @(
    "REPIU_GLIDE_SETTER_ELIDE",
    "REPIU_GLIDE_SETTER_CENSUS",
    "REPIU_GLIDE_ORDINAL_TIME_PROFILE")
$savedEnvironment = @{}
foreach ($name in $environmentNames)
{
    $savedEnvironment[$name] =
        [Environment]::GetEnvironmentVariable($name, "Process")
}
try
{
    $offResult = Invoke-Configuration $offRoot "0"
    $onResult = Invoke-Configuration $onRoot "1"
}
finally
{
    foreach ($name in $environmentNames)
    {
        [Environment]::SetEnvironmentVariable(
            $name, $savedEnvironment[$name], "Process")
    }
}

$off = Read-Runs $offResult "elide-off" $false
$on = Read-Runs $onResult "elide-on" $true
$offRuns = @($off.runs)
$onRuns = @($on.runs)
$setterRuns = @($off.setters) + @($on.setters)

$offMedianFrames = Get-Median @($offRuns | ForEach-Object { [double]$_.frames })
$onMedianFrames = Get-Median @($onRuns | ForEach-Object { [double]$_.frames })
$frameDeltaPercent =
    100.0 * ($onMedianFrames - $offMedianFrames) / $offMedianFrames
$offMedianGlide = Get-Median @(
    $offRuns | ForEach-Object { $_.glide_share_percent })
$onMedianGlide = Get-Median @(
    $onRuns | ForEach-Object { $_.glide_share_percent })
$offMedianTargetCalls = Get-Median @(
    $offRuns | ForEach-Object { [double]$_.target_calls })
$onMedianTargetCalls = Get-Median @(
    $onRuns | ForEach-Object { [double]$_.target_calls })
$medianElided = Get-Median @(
    $onRuns | ForEach-Object { [double]$_.target_elided })

# E2 call preservation: eliding must not change how often the guest calls these
# setters per frame. A raw total would move simply because a faster run runs
# longer in game time, so the per-frame rate is the invariant.
$offCallsPerFrame = Get-Median @(
    $offRuns | ForEach-Object {
        [double]$_.target_calls / [double]$_.frames })
$onCallsPerFrame = Get-Median @(
    $onRuns | ForEach-Object {
        [double]$_.target_calls / [double]$_.frames })
$callsPerFrameDeltaPercent =
    100.0 * ($onCallsPerFrame - $offCallsPerFrame) / $offCallsPerFrame
if ([Math]::Abs($callsPerFrameDeltaPercent) -gt 5.0)
{
    throw ("Gate E2: target setter calls per frame moved " +
           "$callsPerFrameDeltaPercent%")
}

$visualDirectory = Join-Path $sessionDirectory "visual"
$visualOff = Invoke-VisualPass (Join-Path $visualDirectory "elide-off") "0"
$visualOn = Invoke-VisualPass (Join-Path $visualDirectory "elide-on") "1"
# E6: a broken mask or blend moves these aggregates sharply, which is what this
# harness can detect. Byte-exact cross-run diffing is impossible because run
# progress differs, so E7's human comparison of frame dumps backs it up.
$visualNonBlackDelta =
    $visualOn.mean_non_black_percent - $visualOff.mean_non_black_percent
$visualRedDelta = $visualOn.mean_red - $visualOff.mean_red
$visualGreenDelta = $visualOn.mean_green - $visualOff.mean_green
$visualBlueDelta = $visualOn.mean_blue - $visualOff.mean_blue

$sequence = Measure-SequenceIdentity $visualOff.by_swap $visualOn.by_swap
$sequence | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "sequence-identity.csv") `
    -NoTypeInformation -Encoding utf8
$bestShift = ($sequence | Sort-Object identical_percent -Descending |
    Select-Object -First 1)
$zeroShift = ($sequence | Where-Object { $_.shift -eq 0 })
if ($bestShift.identical_percent -lt 50.0)
{
    throw ("Gate E7: no phase offset reproduces the frame sequence " +
           "(best {0:N1}% at shift {1})" -f `
           $bestShift.identical_percent, $bestShift.shift)
}

$offRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "elide-off-runs.csv") `
    -NoTypeInformation -Encoding utf8
$onRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "elide-on-runs.csv") `
    -NoTypeInformation -Encoding utf8
$setterRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "setter-runs.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    elide_off_median_frames = $offMedianFrames
    elide_on_median_frames = $onMedianFrames
    frame_delta_percent = $frameDeltaPercent
    elide_off_median_glide_share_percent = $offMedianGlide
    elide_on_median_glide_share_percent = $onMedianGlide
    glide_share_delta_points = $onMedianGlide - $offMedianGlide
    elide_off_median_target_calls = $offMedianTargetCalls
    elide_on_median_target_calls = $onMedianTargetCalls
    median_elided_calls = $medianElided
    elide_off_target_calls_per_frame = $offCallsPerFrame
    elide_on_target_calls_per_frame = $onCallsPerFrame
    calls_per_frame_delta_percent = $callsPerFrameDeltaPercent
    gate_e1_elided_equals_observed = $true
    visual_duration_seconds = $VisualDurationSeconds
    visual_off_samples = $visualOff.sample_count
    visual_on_samples = $visualOn.sample_count
    visual_off_mean_non_black_percent = $visualOff.mean_non_black_percent
    visual_on_mean_non_black_percent = $visualOn.mean_non_black_percent
    visual_non_black_delta_points = $visualNonBlackDelta
    visual_off_mean_rgb =
        "$($visualOff.mean_red),$($visualOff.mean_green),$($visualOff.mean_blue)"
    visual_on_mean_rgb =
        "$($visualOn.mean_red),$($visualOn.mean_green),$($visualOn.mean_blue)"
    visual_rgb_delta =
        "$visualRedDelta,$visualGreenDelta,$visualBlueDelta"
    sequence_best_shift = $bestShift.shift
    sequence_best_identical_percent = $bestShift.identical_percent
    sequence_zero_shift_identical_percent = $zeroShift.identical_percent
    elide_off_result = $offResult.FullName
    elide_on_result = $onResult.FullName
    visual_result = $visualDirectory
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 365 Glide setter state elision A/B complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames off/on/delta: {0:N0}/{1:N0}/{2:N2}%" -f `
    $offMedianFrames, $onMedianFrames, $frameDeltaPercent)
Write-Host ("Glide share off/on: {0:N2}%/{1:N2}% ({2:N2} points)" -f `
    $offMedianGlide, $onMedianGlide, ($onMedianGlide - $offMedianGlide))
Write-Host ("Elided calls (median): {0:N0}; target calls per frame off/on: {1:N2}/{2:N2}" -f `
    $medianElided, $offCallsPerFrame, $onCallsPerFrame)
Write-Host ("Gate E1 (elided == observed duplicates): pass")
Write-Host ("Visual non-black off/on: {0:N2}%/{1:N2}% (delta {2:N2} points)" -f `
    $visualOff.mean_non_black_percent, $visualOn.mean_non_black_percent, `
    $visualNonBlackDelta)
Write-Host ("Visual mean RGB off/on: {0}/{1}" -f `
    $summary.visual_off_mean_rgb, $summary.visual_on_mean_rgb)
Write-Host ("Gate E7 frame sequence: {0:N1}% identical at shift {1} (vs {2:N1}% at shift 0)" -f `
    $bestShift.identical_percent, $bestShift.shift, `
    $zeroShift.identical_percent)
