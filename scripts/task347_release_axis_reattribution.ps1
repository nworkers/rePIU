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
if ([string]::IsNullOrWhiteSpace($OutputRoot))
{
    $OutputRoot = Join-Path $repositoryRoot "build\benchmarks\release-axis"
}

$loaderPath = Join-Path $ReleaseDirectory "repiu_loader_win32.exe"
$probePath = Join-Path $ReleaseDirectory "repiu_aot_probe.exe"
foreach ($requiredPath in @($loaderPath, $probePath, $EepromSeed, $ProbeInput))
{
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf))
    {
        throw "Required file is missing: $requiredPath"
    }
}

function Get-LastMetricMatch
{
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Name
    )

    $metricMatches = [regex]::Matches(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if ($metricMatches.Count -eq 0)
    {
        throw "Required metric is missing: $Name"
    }
    return $metricMatches[$metricMatches.Count - 1]
}

function Get-UInt64Group
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

    if ($Values.Count -eq 0)
    {
        return 0.0
    }
    $ordered = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if (($ordered.Count % 2) -eq 1)
    {
        return [double]$ordered[$middle]
    }
    return ([double]$ordered[$middle - 1] +
            [double]$ordered[$middle]) / 2.0
}

function Invoke-CapturedProcess
{
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$WorkingDirectory,
        [string]$StdoutPath,
        [string]$StderrPath
    )

    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -WorkingDirectory $WorkingDirectory `
        -RedirectStandardOutput $StdoutPath `
        -RedirectStandardError $StderrPath `
        -Wait `
        -PassThru
    return $process.ExitCode
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultDirectory = Join-Path $OutputRoot $timestamp
New-Item -ItemType Directory -Path $resultDirectory -Force | Out-Null

$probeStdout = Join-Path $resultDirectory "probe.stdout.log"
$probeStderr = Join-Path $resultDirectory "probe.stderr.log"
$probeExitCode = Invoke-CapturedProcess `
    -FilePath $probePath `
    -ArgumentList @($ProbeInput) `
    -WorkingDirectory $repositoryRoot `
    -StdoutPath $probeStdout `
    -StderrPath $probeStderr
if ($probeExitCode -ne 0)
{
    throw "Release AOT probe failed with exit code $probeExitCode"
}
$probeText =
    (Get-Content -Raw -LiteralPath $probeStdout) + "`n" +
    (Get-Content -Raw -LiteralPath $probeStderr)
$calibrationMatch = Get-LastMetricMatch `
    -Text $probeText `
    -Pattern "exception_transition_int3_cycles_per_transition=(\d+)[\s\S]*?exception_transition_single_step_cycles_per_transition=(\d+)[\s\S]*?exception_transition_calibration_all=true" `
    -Name "exception transition calibration"
$int3CyclesPerTransition = Get-UInt64Group $calibrationMatch 1
$singleStepCyclesPerTransition = Get-UInt64Group $calibrationMatch 2

$environmentNames = @(
    "REPIU_EXECUTION_BACKEND",
    "REPIU_EXECUTION_TIMEOUT_MS",
    "REPIU_EXECUTION_TIME_PROFILE",
    "REPIU_EEPROM_PATH",
    "REPIU_SINGLE_STEP_HOTSPOT_PROFILE",
    "REPIU_TIMER_INJECT_LOG",
    "REPIU_AOT_DBT_SUPERBLOCK",
    "REPIU_AOT_DBT_POST_HLE_TRANSLATE",
    "REPIU_NATIVE_LINEAR_SPAN",
    "REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE"
)
$savedEnvironment = @{}
foreach ($name in $environmentNames)
{
    $savedEnvironment[$name] =
        [Environment]::GetEnvironmentVariable($name, "Process")
}

$runResults = @()
try
{
    [Environment]::SetEnvironmentVariable(
        "REPIU_EXECUTION_BACKEND", "aot-dbt", "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_EXECUTION_TIMEOUT_MS",
        ([string]($DurationSeconds * 1000)),
        "Process")
    [Environment]::SetEnvironmentVariable(
        "REPIU_EXECUTION_TIME_PROFILE", "1", "Process")
    foreach ($name in @(
        "REPIU_SINGLE_STEP_HOTSPOT_PROFILE",
        "REPIU_TIMER_INJECT_LOG",
        "REPIU_AOT_DBT_SUPERBLOCK",
        "REPIU_AOT_DBT_POST_HLE_TRANSLATE",
        "REPIU_NATIVE_LINEAR_SPAN",
        "REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE"))
    {
        [Environment]::SetEnvironmentVariable($name, $null, "Process")
    }

    for ($run = 1; $run -le $Runs; ++$run)
    {
        $runName = "run-{0:D2}" -f $run
        $runDirectory = Join-Path $resultDirectory $runName
        New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
        $runEeprom = Join-Path $runDirectory "eeprom.dat"
        Copy-Item -LiteralPath $EepromSeed -Destination $runEeprom
        [Environment]::SetEnvironmentVariable(
            "REPIU_EEPROM_PATH", $runEeprom, "Process")

        $stdoutPath = Join-Path $runDirectory "stdout.log"
        $stderrPath = Join-Path $runDirectory "stderr.log"
        Write-Host ("Task 347 Release run {0}/{1}: {2}s" -f
                    $run, $Runs, $DurationSeconds)
        $exitCode = Invoke-CapturedProcess `
            -FilePath $loaderPath `
            -ArgumentList @($Target) `
            -WorkingDirectory $repositoryRoot `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath
        if ($exitCode -ne 0)
        {
            throw "$runName failed with exit code $exitCode"
        }

        $text =
            (Get-Content -Raw -LiteralPath $stdoutPath) + "`n" +
            (Get-Content -Raw -LiteralPath $stderrPath)
        $combinedPath = Join-Path $runDirectory "combined.log"
        Set-Content -LiteralPath $combinedPath -Value $text -Encoding UTF8

        $timeoutMatch = Get-LastMetricMatch `
            $text "Win32 minimal execution timed out: (true|false)" "timeout"
        if ($timeoutMatch.Groups[1].Value -ne "true")
        {
            throw "$runName did not reach the planned timeout"
        }

        $profileMatch = Get-LastMetricMatch `
            $text "Win32 execution time profile enabled: (true|false)" `
            "execution time profile"
        if ($profileMatch.Groups[1].Value -ne "true")
        {
            throw "$runName did not enable the execution-time profile"
        }

        $cyclesMatch = Get-LastMetricMatch `
            $text `
            "Win32 execution time cycles guest-run/veh/glide-gate/port-io/dos: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "execution time cycles"
        $timeCountMatch = Get-LastMetricMatch `
            $text `
            "Win32 execution time count guest-run/veh/glide-gate/port-io/dos: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "execution time counts"
        $insideMatch = Get-LastMetricMatch `
            $text `
            "Win32 execution time inside-veh cycles glide-gate/port-io/dos: (\d+)/(\d+)/(\d+)" `
            "inside-VEH service cycles"
        $derivedMatch = Get-LastMetricMatch `
            $text `
            "Win32 execution time derived veh-exclusive/unaccounted: (\d+)/(\d+)" `
            "derived execution time"
        $censusMatch = Get-LastMetricMatch `
            $text `
            "Win32 exception census single-step/breakpoint/access-violation/other/total: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "exception census"
        $dispatchMatch = Get-LastMetricMatch `
            $text "Win32 exception dispatch entry count: (\d+)" `
            "VEH entry count"
        $runBucketMatch = Get-LastMetricMatch `
            $text `
            "Win32 single-step run buckets 1/2/3/4/5-8/9-16/17-32/33\+: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "single-step run buckets"
        $runSummaryMatch = Get-LastMetricMatch `
            $text "Win32 single-step run count/max/mean: (\d+)/(\d+)/(\d+)" `
            "single-step run summary"
        $funnelMatch = Get-LastMetricMatch `
            $text `
            "Win32 hle reentry funnel not-pending/backend/segment-write/outside-arena/quarantined/span-unsafe/success/total: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "HLE reentry funnel"
        $timerMatch = Get-LastMetricMatch `
            $text `
            "Win32 AOT timer safe-point trap/injected/deferred: (\d+)/(\d+)/(\d+)" `
            "timer safe-point counters"
        $pitMatch = Get-LastMetricMatch `
            $text `
            "\[repiu-pit\] channel=0 divisor=(\d+) frequency=([0-9.]+)Hz generation=(\d+)" `
            "PIT channel 0 configuration"
        $frameMatch = Get-LastMetricMatch `
            $text `
            "Win32 Glide call trace: ordinal=\d+ name=_?GRBUFFERSWAP@4 count=(\d+)" `
            "grBufferSwap count"
        $gateMatch = Get-LastMetricMatch `
            $text "Win32 Glide gate entries/handled/ESP: (\d+)/(\d+)/" `
            "Glide gate count"
        $getProcMatch = Get-LastMetricMatch `
            $text "Win32 LINEXE get-proc count/name/result: (\d+)/" `
            "LINEXE get-proc count"
        $malformedMatch = Get-LastMetricMatch `
            $text "Win32 exception dispatch malformed count: (\d+)" `
            "malformed dispatch count"
        $fatalCountMatch = Get-LastMetricMatch `
            $text "Win32 handled original fatal breakpoint count: (\d+)" `
            "fatal breakpoint count"
        $fatalHaltMatch = Get-LastMetricMatch `
            $text "Win32 original fatal halt reached: (true|false)" `
            "fatal halt state"
        $glideIssuesMatch = Get-LastMetricMatch `
            $text `
            "Win32 Glide implementation issues unimplemented/unsupported/backend/abi/unique/overflow: (\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)" `
            "Glide implementation issues"

        $guestRun = Get-UInt64Group $cyclesMatch 1
        $veh = Get-UInt64Group $cyclesMatch 2
        $glide = Get-UInt64Group $cyclesMatch 3
        $portIo = Get-UInt64Group $cyclesMatch 4
        $dos = Get-UInt64Group $cyclesMatch 5
        $vehExclusive = Get-UInt64Group $derivedMatch 1
        $unaccounted = Get-UInt64Group $derivedMatch 2
        $singleStep = Get-UInt64Group $censusMatch 1
        $breakpoint = Get-UInt64Group $censusMatch 2
        $accessViolation = Get-UInt64Group $censusMatch 3
        $other = Get-UInt64Group $censusMatch 4
        $censusTotal = Get-UInt64Group $censusMatch 5
        $profileVehEntries = Get-UInt64Group $timeCountMatch 2
        $lateDispatchEntries = Get-UInt64Group $dispatchMatch 1
        $timerTraps = Get-UInt64Group $timerMatch 1
        $frames = Get-UInt64Group $frameMatch 1
        $gateEntries = Get-UInt64Group $gateMatch 1
        $getProc = Get-UInt64Group $getProcMatch 1
        $malformed = Get-UInt64Group $malformedMatch 1
        $fatalCount = Get-UInt64Group $fatalCountMatch 1

        if ($guestRun -eq 0)
        {
            throw "$runName has a zero guest-run denominator"
        }
        if ($censusTotal -lt $profileVehEntries -or
            ($censusTotal - $profileVehEntries) -gt 1)
        {
            throw "$runName census/profiled-VEH delta exceeds the one open timeout scope"
        }
        if ($lateDispatchEntries -gt $censusTotal)
        {
            throw "$runName late dispatch entries exceed the census total"
        }
        if ($timerTraps -gt $breakpoint)
        {
            throw "$runName timer traps exceed the breakpoint census"
        }
        if ($malformed -ne 0 -or $fatalCount -ne 0 -or
            $fatalHaltMatch.Groups[1].Value -ne "false")
        {
            throw "$runName violated malformed/fatal equivalence"
        }
        for ($group = 1; $group -le 6; ++$group)
        {
            if ((Get-UInt64Group $glideIssuesMatch $group) -ne 0)
            {
                throw "$runName reported a Glide implementation issue"
            }
        }
        if ($frames -eq 0 -or $gateEntries -eq 0 -or $getProc -eq 0)
        {
            throw "$runName did not preserve frame/gate/get-proc liveness"
        }
        if ((Get-UInt64Group $pitMatch 1) -ne 4972 -or
            [Math]::Abs(
                [double]::Parse(
                    $pitMatch.Groups[2].Value,
                    [System.Globalization.CultureInfo]::InvariantCulture) -
                240.0) -gt 0.01)
        {
            throw "$runName did not use the expected 240Hz PIT path"
        }

        $kernelTransitionCycles =
            [double]$singleStep * [double]$singleStepCyclesPerTransition +
            [double]($breakpoint + $accessViolation + $other) *
                [double]$int3CyclesPerTransition
        if ($kernelTransitionCycles -gt [double]$unaccounted)
        {
            throw "$runName kernel transition estimate exceeds unaccounted"
        }
        $guestExecutionCycles =
            [double]$unaccounted - $kernelTransitionCycles
        $spanUnsafe = Get-UInt64Group $funnelMatch 6
        $reentrySuccess = Get-UInt64Group $funnelMatch 7
        $reentryTotal = Get-UInt64Group $funnelMatch 8

        $result = [pscustomobject][ordered]@{
            run = $run
            duration_seconds = $DurationSeconds
            exit_code = $exitCode
            guest_run_cycles = $guestRun
            veh_cycles = $veh
            glide_cycles = $glide
            port_io_cycles = $portIo
            dos_cycles = $dos
            glide_inside_veh_cycles = Get-UInt64Group $insideMatch 1
            port_io_inside_veh_cycles = Get-UInt64Group $insideMatch 2
            dos_inside_veh_cycles = Get-UInt64Group $insideMatch 3
            veh_exclusive_cycles = $vehExclusive
            unaccounted_cycles = $unaccounted
            kernel_transition_cycles_estimate = $kernelTransitionCycles
            guest_execution_cycles_estimate = $guestExecutionCycles
            veh_share_percent = 100.0 * [double]$veh / [double]$guestRun
            glide_share_percent = 100.0 * [double]$glide / [double]$guestRun
            veh_exclusive_share_percent =
                100.0 * [double]$vehExclusive / [double]$guestRun
            unaccounted_share_percent =
                100.0 * [double]$unaccounted / [double]$guestRun
            kernel_transition_share_percent =
                100.0 * $kernelTransitionCycles / [double]$guestRun
            guest_execution_share_percent =
                100.0 * $guestExecutionCycles / [double]$guestRun
            single_step_count = $singleStep
            breakpoint_count = $breakpoint
            access_violation_count = $accessViolation
            other_exception_count = $other
            exception_total = $censusTotal
            profiled_veh_entry_count = $profileVehEntries
            open_veh_scope_at_timeout =
                $censusTotal - $profileVehEntries
            late_dispatch_entry_count = $lateDispatchEntries
            pre_dispatch_early_return_count =
                $censusTotal - $lateDispatchEntries
            timer_safe_point_traps = $timerTraps
            timer_safe_point_injected = Get-UInt64Group $timerMatch 2
            timer_safe_point_deferred = Get-UInt64Group $timerMatch 3
            timer_trap_breakpoint_share_percent =
                if ($breakpoint -ne 0)
                {
                    100.0 * [double]$timerTraps / [double]$breakpoint
                }
                else
                {
                    0.0
                }
            run_bucket_1 = Get-UInt64Group $runBucketMatch 1
            run_bucket_2 = Get-UInt64Group $runBucketMatch 2
            run_bucket_3 = Get-UInt64Group $runBucketMatch 3
            run_bucket_4 = Get-UInt64Group $runBucketMatch 4
            run_bucket_5_8 = Get-UInt64Group $runBucketMatch 5
            run_bucket_9_16 = Get-UInt64Group $runBucketMatch 6
            run_bucket_17_32 = Get-UInt64Group $runBucketMatch 7
            run_bucket_33_plus = Get-UInt64Group $runBucketMatch 8
            single_step_run_count = Get-UInt64Group $runSummaryMatch 1
            single_step_run_max = Get-UInt64Group $runSummaryMatch 2
            single_step_run_mean = Get-UInt64Group $runSummaryMatch 3
            reentry_span_unsafe = $spanUnsafe
            reentry_success = $reentrySuccess
            reentry_total = $reentryTotal
            reentry_span_unsafe_share_percent =
                if ($reentryTotal -ne 0)
                {
                    100.0 * [double]$spanUnsafe / [double]$reentryTotal
                }
                else
                {
                    0.0
                }
            reentry_success_share_percent =
                if ($reentryTotal -ne 0)
                {
                    100.0 * [double]$reentrySuccess / [double]$reentryTotal
                }
                else
                {
                    0.0
                }
            frames = $frames
            glide_gate_entries = $gateEntries
            linexe_get_proc_count = $getProc
            pit_divisor = Get-UInt64Group $pitMatch 1
            pit_frequency_hz = [double]::Parse(
                $pitMatch.Groups[2].Value,
                [System.Globalization.CultureInfo]::InvariantCulture)
            log_path = $combinedPath
        }
        $result | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $runDirectory "metrics.json") `
                -Encoding UTF8
        $runResults += $result
    }
}
finally
{
    foreach ($name in $environmentNames)
    {
        [Environment]::SetEnvironmentVariable(
            $name, $savedEnvironment[$name], "Process")
    }
}

$summary = [pscustomobject][ordered]@{
    generated_at = (Get-Date).ToString("o")
    target = $Target
    runs = $Runs
    duration_seconds = $DurationSeconds
    int3_cycles_per_transition = $int3CyclesPerTransition
    single_step_cycles_per_transition = $singleStepCyclesPerTransition
    frames_median = Get-Median @(
        $runResults | ForEach-Object { [double]$_.frames })
    frames_min = ($runResults | Measure-Object -Property frames -Minimum).Minimum
    frames_max = ($runResults | Measure-Object -Property frames -Maximum).Maximum
    veh_share_percent_median = Get-Median @(
        $runResults | ForEach-Object { [double]$_.veh_share_percent })
    glide_share_percent_median = Get-Median @(
        $runResults | ForEach-Object { [double]$_.glide_share_percent })
    veh_exclusive_share_percent_median = Get-Median @(
        $runResults |
            ForEach-Object { [double]$_.veh_exclusive_share_percent })
    kernel_transition_share_percent_median = Get-Median @(
        $runResults |
            ForEach-Object { [double]$_.kernel_transition_share_percent })
    guest_execution_share_percent_median = Get-Median @(
        $runResults |
            ForEach-Object { [double]$_.guest_execution_share_percent })
    span_unsafe_share_percent_median = Get-Median @(
        $runResults |
            ForEach-Object {
                [double]$_.reentry_span_unsafe_share_percent
            })
    reentry_success_share_percent_median = Get-Median @(
        $runResults |
            ForEach-Object { [double]$_.reentry_success_share_percent })
    result_directory = $resultDirectory
}

$runResults | Export-Csv `
    -LiteralPath (Join-Path $resultDirectory "runs.csv") `
    -NoTypeInformation `
    -Encoding UTF8
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $resultDirectory "summary.json") `
        -Encoding UTF8

Write-Host ""
Write-Host "Task 347 Release-axis measurement complete"
Write-Host "Result directory: $resultDirectory"
Write-Host ("Frames median/range: {0} [{1}, {2}]" -f
            $summary.frames_median,
            $summary.frames_min,
            $summary.frames_max)
Write-Host ("Median shares: kernel={0:N2}% guest={1:N2}% glide={2:N2}% veh-exclusive={3:N2}%" -f
            $summary.kernel_transition_share_percent_median,
            $summary.guest_execution_share_percent_median,
            $summary.glide_share_percent_median,
            $summary.veh_exclusive_share_percent_median)
