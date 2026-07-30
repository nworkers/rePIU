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
        "build\benchmarks\glide-setter-census"
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

function Invoke-ReleaseAxis
{
    param(
        [string]$ResultRoot,
        [string]$CensusValue,
        [string]$PhaseValue,
        [string]$OrdinalValue
    )

    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SETTER_CENSUS", $CensusValue, "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_SETTER_PHASE", $PhaseValue, "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_GLIDE_ORDINAL_TIME_PROFILE", $OrdinalValue, "Process")
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

    $runRows = @()
    $setterRows = @()
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
            "Win32 Glide setter census enabled/entries/calls/first/same/changed/failure/unsupported: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName census summary"
        $health = Get-LastMetricMatch $text `
            "Win32 Glide setter census key-overflow/distinct-overflow/ordinal-overflow/invalidations/frames/texture-generation: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName census health"
        $phaseState = Get-LastMetricMatch $text `
            "Win32 Glide setter phase enabled/clamped: (true|false)/(\d+)" `
            "$runName phase state"
        $depth = Get-LastMetricMatch $text `
            "Win32 Glide setter phase depth-mask calls/drain/apply/error/total/max-total/max-apply/max-error/drain-iterations/errors: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName depth-mask phases"
        $blend = Get-LastMetricMatch $text `
            "Win32 Glide setter phase alpha-blend calls/drain/apply/error/total/max-total/max-apply/max-error/drain-iterations/errors: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName alpha-blend phases"
        $ordinalSummary = Get-LastMetricMatch $text `
            "Win32 Glide ordinal timing enabled/entries/completed/overflow/clamped: (true|false)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "$runName ordinal summary"
        $gate = Get-LastMetricMatch $text `
            "Win32 Glide gate entries/handled/ESP: (\d+)/(\d+)/" `
            "$runName gate counts"

        # C3 stability: the instruments must not introduce a clamped sample or
        # a profiler overflow of any kind.
        if ($summary.Groups[1].Value -ne "true" -or
            $phaseState.Groups[1].Value -ne "true" -or
            $ordinalSummary.Groups[1].Value -ne "true")
        {
            throw "$runName did not enable every Task 364 instrument"
        }
        if ((Get-UInt64 $phaseState 2) -ne 0 -or
            (Get-UInt64 $ordinalSummary 4) -ne 0 -or
            (Get-UInt64 $ordinalSummary 5) -ne 0)
        {
            throw "$runName reported a clamped or overflowed sample"
        }
        # C6 key sanity: a wider-than-key setter in the target list would be
        # silently excluded, so any occurrence invalidates the target list.
        if ((Get-UInt64 $health 1) -ne 0 -or (Get-UInt64 $health 3) -ne 0)
        {
            throw "$runName reported a census key or ordinal overflow"
        }

        $calls = Get-UInt64 $summary 3
        $first = Get-UInt64 $summary 4
        $same = Get-UInt64 $summary 5
        $changed = Get-UInt64 $summary 6
        $failure = Get-UInt64 $summary 7
        $unsupported = Get-UInt64 $summary 8
        # C4: the outcome classes partition the calls exactly.
        if ($first + $same + $changed + $failure + $unsupported -ne $calls)
        {
            throw "$runName census outcome classes do not partition calls"
        }
        if ($calls -eq 0)
        {
            throw "$runName recorded no state-setter calls"
        }

        $depthTotal = Get-UInt64 $depth 5
        $blendTotal = Get-UInt64 $blend 5
        # C5: the four timestamps partition each OpenGL interval exactly.
        if ((Get-UInt64 $depth 2) + (Get-UInt64 $depth 3) +
            (Get-UInt64 $depth 4) -ne $depthTotal)
        {
            throw "$runName depth-mask phases do not sum to the total"
        }
        if ((Get-UInt64 $blend 2) + (Get-UInt64 $blend 3) +
            (Get-UInt64 $blend 4) -ne $blendTotal)
        {
            throw "$runName alpha-blend phases do not sum to the total"
        }
        if ((Get-UInt64 $depth 2) -ne 0)
        {
            throw "$runName recorded a nonzero depth-mask drain interval"
        }

        # C2: every completed ordinal is a handled gate.
        $completed = Get-UInt64 $ordinalSummary 3
        $handled = Get-UInt64 $gate 2
        if ($completed -gt $handled)
        {
            throw "$runName completed ordinals exceed handled gates"
        }

        $censusLines = Get-AllMetricMatches $text `
            "Win32 Glide setter census: ordinal=(\d+) name=(\S+) calls=(\d+) first=(\d+) same=(\d+) changed=(\d+) failure=(\d+) unsupported=(\d+) key_overflow=(\d+) distinct=(\d+) distinct_overflow=(\d+) max_run=(\d+) max_frame_calls=(\d+) max_frame_changes=(\d+)"
        $ordinalLines = Get-AllMetricMatches $text `
            "Win32 Glide ordinal timing: ordinal=(\d+) name=(\S+) count=(\d+) gate=(\d+) max=(\d+) rendezvous=(\d+) queue=(\d+) wake=(\d+) work=(\d+) complete=(\d+) residual=(\d+) backend_total=(\d+) direct=(\d+) direct_work=(\d+)"
        # The per-run log repeats each block once per emitted summary, so index
        # by ordinal and keep the last occurrence.
        $ordinalByKey = @{}
        $ordinalByName = @{}
        foreach ($line in $ordinalLines)
        {
            $ordinalByKey[[string](Get-UInt64 $line 1)] = $line
            $ordinalByName[$line.Groups[2].Value] = $line
        }
        $censusByKey = @{}
        foreach ($line in $censusLines)
        {
            $censusByKey[[string](Get-UInt64 $line 1)] = $line
        }

        $guestRun = [double]$metrics.guest_run_cycles
        $censusCallSum = [UInt64]0
        $ceilingCycles = 0.0
        foreach ($key in $censusByKey.Keys)
        {
            $line = $censusByKey[$key]
            $ordinal = Get-UInt64 $line 1
            $setterCalls = Get-UInt64 $line 3
            $setterSame = Get-UInt64 $line 5
            $censusCallSum += $setterCalls
            $rendezvous = [UInt64]0
            $backendTotal = [UInt64]0
            $gateCycles = [UInt64]0
            $workCycles = [UInt64]0
            $ordinalCount = [UInt64]0
            if ($ordinalByKey.ContainsKey($key))
            {
                $timing = $ordinalByKey[$key]
                $ordinalCount = Get-UInt64 $timing 3
                $gateCycles = Get-UInt64 $timing 4
                $rendezvous = Get-UInt64 $timing 6
                $workCycles = Get-UInt64 $timing 9
                $backendTotal = Get-UInt64 $timing 12
            }
            # C7: the census and the ordinal timing must have counted the same
            # calls; they are independent instruments on the same gate.
            if ($ordinalCount -ne 0 -and $ordinalCount -ne $setterCalls)
            {
                throw ("$runName ordinal $ordinal census/timing call counts " +
                       "differ: $setterCalls vs $ordinalCount")
            }
            # An elided repeat removes one whole backend rendezvous while the
            # gate entry itself stays, so the ceiling uses the mean rendezvous
            # cost rather than the whole gate interval.
            $meanRendezvous = 0.0
            if ($rendezvous -ne 0)
            {
                $meanRendezvous = [double]$backendTotal / [double]$rendezvous
            }
            $setterCeiling = [double]$setterSame * $meanRendezvous
            $ceilingCycles += $setterCeiling
            $setterRows += [pscustomobject][ordered]@{
                run = $run
                ordinal = $ordinal
                name = $line.Groups[2].Value
                calls = $setterCalls
                first = Get-UInt64 $line 4
                same = $setterSame
                changed = Get-UInt64 $line 6
                failure = Get-UInt64 $line 7
                unsupported = Get-UInt64 $line 8
                distinct = Get-UInt64 $line 10
                distinct_overflow = Get-UInt64 $line 11
                max_repeat_run = Get-UInt64 $line 12
                max_frame_calls = Get-UInt64 $line 13
                max_frame_changes = Get-UInt64 $line 14
                repetition_rate_percent =
                    100.0 * [double]$setterSame / [double]$setterCalls
                gate_cycles = $gateCycles
                work_cycles = $workCycles
                backend_total_cycles = $backendTotal
                rendezvous_count = $rendezvous
                mean_rendezvous_cycles = $meanRendezvous
                gate_share_percent =
                    100.0 * [double]$gateCycles / $guestRun
                elision_ceiling_cycles = $setterCeiling
                elision_ceiling_share_percent =
                    100.0 * $setterCeiling / $guestRun
            }
        }
        if ($censusCallSum -ne $calls)
        {
            throw "$runName per-ordinal census rows do not sum to the total"
        }

        $depthApply = [double](Get-UInt64 $depth 3)
        $depthError = [double](Get-UInt64 $depth 4)
        $blendDrain = [double](Get-UInt64 $blend 2)
        $blendApply = [double](Get-UInt64 $blend 3)
        $blendError = [double](Get-UInt64 $blend 4)
        # Looked up by export name: the gate-id constant and the export ordinal
        # are different numbering spaces (grDepthMask is gate id 34, ordinal 98).
        $depthWork = 0.0
        if ($ordinalByName.ContainsKey("_GRDEPTHMASK@4"))
        {
            $depthWork =
                [double](Get-UInt64 $ordinalByName["_GRDEPTHMASK@4"] 9)
        }

        $depthApplyShare = 0.0
        $depthErrorShare = 0.0
        if ($depthTotal -ne 0)
        {
            $depthApplyShare = 100.0 * $depthApply / [double]$depthTotal
            $depthErrorShare = 100.0 * $depthError / [double]$depthTotal
        }
        # How much of the ordinal's measured host work the OpenGL interval
        # covers. The remainder is the open check, message, and dispatch.
        $depthCoverage = 0.0
        if ($depthWork -ne 0.0)
        {
            $depthCoverage = 100.0 * [double]$depthTotal / $depthWork
        }
        $glideCycles = [double]$metrics.glide_cycles
        $ceilingGlideShare = 0.0
        if ($glideCycles -ne 0.0)
        {
            $ceilingGlideShare = 100.0 * $ceilingCycles / $glideCycles
        }
        $blendDrainShare = 0.0
        $blendApplyShare = 0.0
        $blendErrorShare = 0.0
        if ($blendTotal -ne 0)
        {
            $blendDrainShare = 100.0 * $blendDrain / [double]$blendTotal
            $blendApplyShare = 100.0 * $blendApply / [double]$blendTotal
            $blendErrorShare = 100.0 * $blendError / [double]$blendTotal
        }

        $runRows += [pscustomobject][ordered]@{
            run = $run
            frames = [UInt64]$metrics.frames
            guest_run_cycles = [UInt64]$metrics.guest_run_cycles
            glide_share_percent = [double]$metrics.glide_share_percent
            census_calls = $calls
            census_first = $first
            census_same = $same
            census_changed = $changed
            census_failure = $failure
            census_unsupported = $unsupported
            census_repetition_rate_percent =
                100.0 * [double]$same / [double]$calls
            census_distinct_overflow = Get-UInt64 $health 2
            census_invalidations = Get-UInt64 $health 4
            census_frames = Get-UInt64 $health 5
            census_texture_generation = Get-UInt64 $health 6
            elision_ceiling_cycles = $ceilingCycles
            elision_ceiling_share_percent = 100.0 * $ceilingCycles / $guestRun
            # Reported against the Glide gate as well: a scene whose gate is
            # dominated by something else (LFB readback, for instance) dilutes
            # the wall share without changing the setter opportunity itself.
            elision_ceiling_glide_share_percent = $ceilingGlideShare
            depth_mask_calls = Get-UInt64 $depth 1
            depth_mask_apply_cycles = $depthApply
            depth_mask_error_cycles = $depthError
            depth_mask_total_cycles = $depthTotal
            depth_mask_apply_share_percent = $depthApplyShare
            depth_mask_error_share_percent = $depthErrorShare
            depth_mask_gl_coverage_percent = $depthCoverage
            depth_mask_errors = Get-UInt64 $depth 10
            alpha_blend_calls = Get-UInt64 $blend 1
            alpha_blend_drain_cycles = $blendDrain
            alpha_blend_apply_cycles = $blendApply
            alpha_blend_error_cycles = $blendError
            alpha_blend_total_cycles = $blendTotal
            alpha_blend_drain_share_percent = $blendDrainShare
            alpha_blend_apply_share_percent = $blendApplyShare
            alpha_blend_error_share_percent = $blendErrorShare
            alpha_blend_drain_iterations = Get-UInt64 $blend 9
            alpha_blend_errors = Get-UInt64 $blend 10
        }
    }
    return [pscustomobject]@{
        runs = $runRows
        setters = $setterRows
    }
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $OutputRoot $timestamp
$controlRoot = Join-Path $sessionDirectory "control"
$profileRoot = Join-Path $sessionDirectory "profile"
New-Item -ItemType Directory -Path $controlRoot -Force | Out-Null
New-Item -ItemType Directory -Path $profileRoot -Force | Out-Null

$environmentNames = @(
    "REPIU_GLIDE_SETTER_CENSUS",
    "REPIU_GLIDE_SETTER_PHASE",
    "REPIU_GLIDE_ORDINAL_TIME_PROFILE")
$savedEnvironment = @{}
foreach ($name in $environmentNames)
{
    $savedEnvironment[$name] =
        [Environment]::GetEnvironmentVariable($name, "Process")
}
try
{
    $controlResult = Invoke-ReleaseAxis $controlRoot $null $null $null
    $profileResult = Invoke-ReleaseAxis $profileRoot "1" "1" "1"
}
finally
{
    foreach ($name in $environmentNames)
    {
        [Environment]::SetEnvironmentVariable(
            $name, $savedEnvironment[$name], "Process")
    }
}

$controlRuns = @(Read-ControlRuns $controlResult)
$profileData = Read-ProfileRuns $profileResult
$profileRuns = @($profileData.runs)
$setterRuns = @($profileData.setters)

$controlMedianFrames = Get-Median @(
    $controlRuns | ForEach-Object { [double]$_.frames })
$profileMedianFrames = Get-Median @(
    $profileRuns | ForEach-Object { [double]$_.frames })
$frameDeltaPercent =
    100.0 * ($profileMedianFrames - $controlMedianFrames) /
    $controlMedianFrames
# C1 observer gate. Applied only at the full duration, where run-to-run
# variance is characterised (Task 335: 18% between samples).
if ($DurationSeconds -ge 60 -and
    [Math]::Abs($frameDeltaPercent) -gt 5.0)
{
    throw "Profile observer impact exceeds 5%: $frameDeltaPercent"
}

$medianRepetition = Get-Median @(
    $profileRuns | ForEach-Object { $_.census_repetition_rate_percent })
$medianCeilingShare = Get-Median @(
    $profileRuns | ForEach-Object { $_.elision_ceiling_share_percent })
$medianCeilingGlideShare = Get-Median @(
    $profileRuns | ForEach-Object { $_.elision_ceiling_glide_share_percent })
$medianDepthErrorShare = Get-Median @(
    $profileRuns | ForEach-Object { $_.depth_mask_error_share_percent })
$medianDepthApplyShare = Get-Median @(
    $profileRuns | ForEach-Object { $_.depth_mask_apply_share_percent })
$medianDepthCoverage = Get-Median @(
    $profileRuns | ForEach-Object { $_.depth_mask_gl_coverage_percent })

# Per-setter medians across runs, ranked by call volume.
$setterSummary = @()
foreach ($group in ($setterRuns | Group-Object ordinal))
{
    $rows = @($group.Group)
    $setterSummary += [pscustomobject][ordered]@{
        ordinal = [UInt64]$group.Name
        name = $rows[0].name
        median_calls = Get-Median @($rows | ForEach-Object { [double]$_.calls })
        median_repetition_rate_percent =
            Get-Median @($rows | ForEach-Object { $_.repetition_rate_percent })
        median_max_repeat_run =
            Get-Median @($rows | ForEach-Object { [double]$_.max_repeat_run })
        median_distinct =
            Get-Median @($rows | ForEach-Object { [double]$_.distinct })
        median_gate_share_percent =
            Get-Median @($rows | ForEach-Object { $_.gate_share_percent })
        median_elision_ceiling_share_percent =
            Get-Median @(
                $rows | ForEach-Object { $_.elision_ceiling_share_percent })
    }
}
$setterSummary = @($setterSummary | Sort-Object median_calls -Descending)

# Pre-registered decisions from the design. Recorded either way rather than
# enforced: a failing gate redirects the next task, it does not fail the run.
$leadingSetter = $setterSummary[0]
$g1 = $leadingSetter.median_repetition_rate_percent -ge 50.0
$g2 = $medianDepthErrorShare -ge 50.0
$g3 = $medianCeilingShare -ge 5.0

$controlRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "control-runs.csv") `
    -NoTypeInformation -Encoding utf8
$profileRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "profile-runs.csv") `
    -NoTypeInformation -Encoding utf8
$setterRuns | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "setter-runs.csv") `
    -NoTypeInformation -Encoding utf8
$setterSummary | Export-Csv `
    -LiteralPath (Join-Path $sessionDirectory "setter-summary.csv") `
    -NoTypeInformation -Encoding utf8

$summary = [pscustomobject]@{
    runs = $Runs
    duration_seconds = $DurationSeconds
    control_median_frames = $controlMedianFrames
    profile_median_frames = $profileMedianFrames
    frame_delta_percent = $frameDeltaPercent
    median_census_repetition_rate_percent = $medianRepetition
    median_elision_ceiling_share_percent = $medianCeilingShare
    median_elision_ceiling_glide_share_percent = $medianCeilingGlideShare
    median_depth_mask_apply_share_percent = $medianDepthApplyShare
    median_depth_mask_error_share_percent = $medianDepthErrorShare
    median_depth_mask_gl_coverage_percent = $medianDepthCoverage
    leading_setter_ordinal = $leadingSetter.ordinal
    leading_setter_name = $leadingSetter.name
    leading_setter_repetition_rate_percent =
        $leadingSetter.median_repetition_rate_percent
    gate_g1_leading_setter_repeats_50 = $g1
    gate_g2_depth_mask_error_50 = $g2
    gate_g3_ceiling_5_percent_of_wall = $g3
    control_result = $controlResult.FullName
    profile_result = $profileResult.FullName
}
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $sessionDirectory "summary.json") `
        -Encoding utf8

Write-Host "Task 364 Glide setter census complete"
Write-Host "Result: $sessionDirectory"
Write-Host ("Frames control/profile/delta: {0:N0}/{1:N0}/{2:N2}%" -f `
    $controlMedianFrames, $profileMedianFrames, $frameDeltaPercent)
Write-Host ("Setter repetition rate (median): {0:N2}%" -f $medianRepetition)
Write-Host ("Elision ceiling wall/Glide (median): {0:N2}%/{1:N2}%" -f `
    $medianCeilingShare, $medianCeilingGlideShare)
Write-Host ("Depth mask GL apply/error: {0:N2}%/{1:N2}% (GL covers {2:N2}% of host work)" -f `
    $medianDepthApplyShare, $medianDepthErrorShare, $medianDepthCoverage)
Write-Host ("Gates G1/G2/G3: {0}/{1}/{2}" -f $g1, $g2, $g3)
