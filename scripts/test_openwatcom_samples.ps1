param(
    # Task 434: the loader under test is a parameter so CI can exercise the same
    # Release binary it ships. Debug stays the default, because that is the
    # configuration every existing baseline and local procedure was recorded on.
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    # Task 434: wall-clock bound per sample. The loader's own budget governs
    # guest execution only, so a sample blocked outside it -- reading standard
    # input, for instance -- never returns. A killed sample counts as a failure.
    [ValidateRange(1, 600)]
    [int]$SampleTimeoutSeconds = 10,
    # Task 435: the loader's execution policy is pinned here rather than
    # inherited. The product default is now the `dynamic` backend with no time
    # limit, but this suite's baseline was recorded on `legacy`, and its pass
    # criterion counts a timeout as a failure -- with no guest budget a stalled
    # sample would end on the harness kill above (ten seconds) instead of the
    # loader's own budget, changing both the verdict basis and the suite's
    # running time. Re-recording the baseline on other values is a deliberate
    # act, not a side effect of a default changing underneath it.
    [ValidateSet("legacy", "dynamic")]
    [string]$Backend = "legacy",
    [ValidateRange(0, 600000)]
    [int]$GuestTimeoutMilliseconds = 1000,
    [string]$ManifestPath = "build\openwatcom_samples\manifest.json",
    [string]$ReportPath = "build\openwatcom_sample_report\index.html",
    [string]$SummaryPath = "build\openwatcom_sample_report\summary.json",
    [string]$RegressionPath = "build\openwatcom_sample_report\regressions.json",
    [string]$BaselinePath = "tests\baselines\openwatcom_samples.json",
    [string]$HistoryPath = "tests\history\openwatcom_samples",
    [switch]$CompareBaseline,
    [switch]$UpdateBaseline
)

$ErrorActionPreference = "Stop"

# Task 429: bump this whenever the run-pass criterion changes, so a comparison
# against a baseline recorded under a different rule is flagged rather than read
# as a code regression.
$script:RunCriterionId = "exit0+no-exception+returned+no-timeout+no-harness-timeout"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
# The tree is multi-config, so the directory name is historical: every
# configuration is a subdirectory of it.
$LoaderRelative = "build\win32_x86_debug\$Configuration\repiu.exe"
$Loader = Join-Path $Root $LoaderRelative
$ResolvedManifestPath = Join-Path $Root $ManifestPath
$ResolvedReportPath = Join-Path $Root $ReportPath
$ResolvedSummaryPath = Join-Path $Root $SummaryPath
$ResolvedRegressionPath = Join-Path $Root $RegressionPath
$ResolvedBaselinePath = Join-Path $Root $BaselinePath
$ResolvedHistoryPath = Join-Path $Root $HistoryPath
$VersionFile = Join-Path $Root "VERSION"

function ConvertTo-HtmlText
{
    param([string]$Value)

    if ($null -eq $Value)
    {
        return ""
    }

    return [System.Net.WebUtility]::HtmlEncode($Value)
}

function Invoke-Capture
{
    # Task 434: bounded, and with stdin closed.
    #
    # `& $FilePath` inherits the console and waits forever. A sample that reads
    # standard input -- cplbexam\iostream\istream\get.cpp is one -- then parks
    # the whole suite: one such sample was observed blocked for 39 minutes on
    # 1.0 second of CPU. Under CI that consumes the job's entire time limit and
    # produces nothing.
    #
    # Redirecting from an empty file gives a reading sample EOF instead of a
    # blocking console, and the wall-clock bound catches every other way a
    # sample can fail to return.
    param(
        [string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $Root,
        [int]$TimeoutSeconds = 0
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    $stdinPath = [System.IO.Path]::GetTempFileName()
    $timedOut = $false

    try
    {
        $startArguments = @{
            FilePath = $FilePath
            WorkingDirectory = $WorkingDirectory
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError = $stderrPath
            RedirectStandardInput = $stdinPath
            NoNewWindow = $true
            PassThru = $true
        }
        if ($Arguments.Count -gt 0)
        {
            $startArguments.ArgumentList = $Arguments
        }

        $process = Start-Process @startArguments

        # Touching Handle caches it, which is what makes ExitCode readable after
        # the process ends. Without this, Start-Process -PassThru leaves
        # ExitCode null and every sample scores as a failure.
        [void]$process.Handle

        if ($TimeoutSeconds -gt 0)
        {
            if (!$process.WaitForExit($TimeoutSeconds * 1000))
            {
                $timedOut = $true
                try
                {
                    $process.Kill()
                }
                catch
                {
                    # It can exit between the wait expiring and the kill.
                }
                # Give the pipes a moment to flush into the redirect files.
                [void]$process.WaitForExit(5000)
            }
        }
        else
        {
            $process.WaitForExit()
        }

        $exitCode = $process.ExitCode
    }
    finally
    {
        $stdout = if (Test-Path $stdoutPath) { Get-Content $stdoutPath -Raw } else { "" }
        $stderr = if (Test-Path $stderrPath) { Get-Content $stderrPath -Raw } else { "" }
        foreach ($temporary in @($stdoutPath, $stderrPath, $stdinPath))
        {
            Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
        }
    }

    $output = "$stdout$stderr"
    if ($timedOut)
    {
        # The marker the run criterion looks for. Deliberately distinct from the
        # loader's own "minimal execution attempt timed out", which is a guest
        # execution budget rather than the sample failing to return at all.
        $output += "`nharness: sample timed out after $TimeoutSeconds seconds and was terminated`n"
    }

    [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
        TimedOut = $timedOut
    }
}

function Invoke-GitValue
{
    param([string[]]$Arguments)

    try
    {
        $output = & git @Arguments 2>$null
        if ($LASTEXITCODE -eq 0)
        {
            return (($output | Out-String).Trim())
        }
    }
    catch
    {
    }

    return ""
}

function Get-ProjectVersion
{
    if (!(Test-Path $VersionFile))
    {
        throw "Project VERSION file was not found."
    }

    $version = (Get-Content $VersionFile -Raw -Encoding UTF8).Trim()
    if ($version -notmatch "^\d+\.\d+\.\d+$")
    {
        throw "Project VERSION must use major.minor.patch format."
    }

    return $version
}

function Get-ResultStatus
{
    param([object]$Result)

    if ($Result.BuildSkipped)
    {
        return "build_skip"
    }
    if (!$Result.BuildPassed)
    {
        return "build_fail"
    }
    if ($Result.RunPassed)
    {
        return "pass"
    }
    if ($Result.RunStatus -eq "not run")
    {
        return "not_run"
    }
    return "run_fail"
}

function Get-Summary
{
    param(
        [object[]]$Results,
        [object]$Manifest
    )

    $total = $Results.Count
    $buildSkip = @($Results | Where-Object { $_.BuildSkipped }).Count
    $buildPass = @($Results | Where-Object { $_.BuildPassed }).Count
    $runEligible = @($Results | Where-Object { $_.BuildPassed -and (Test-Path $_.Executable) }).Count
    $runPass = @($Results | Where-Object { $_.RunPassed }).Count
    $overallPass = @($Results | Where-Object { $_.BuildPassed -and $_.RunPassed }).Count

    [pscustomobject]@{
        GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        # Task 429: records which pass criterion produced these numbers. A
        # baseline without this field predates the completion requirement, so its
        # counts are not comparable -- see the -CompareBaseline warning.
        RunCriterion = $script:RunCriterionId
        # Task 434: Debug and Release do not share a timing profile, and the
        # criterion counts a timeout as a failure, so a baseline recorded on one
        # configuration is not comparable with a run on the other.
        Configuration = $Configuration
        Version = Get-ProjectVersion
        GitCommit = Invoke-GitValue @("rev-parse", "HEAD")
        GitBranch = Invoke-GitValue @("branch", "--show-current")
        ManifestPath = $ManifestPath
        ManifestGeneratedAt = $Manifest.GeneratedAt
        Suites = @($Manifest.Suites)
        Total = $total
        BuildPassed = $buildPass
        BuildSkipped = $buildSkip
        RunEligible = $runEligible
        RunPassed = $runPass
        OverallPassed = $overallPass
        BuildPassRate = if ($total -eq 0) { 0.0 } else { [Math]::Round(($buildPass * 100.0) / $total, 1) }
        RunPassRate = if ($runEligible -eq 0) { 0.0 } else { [Math]::Round(($runPass * 100.0) / $runEligible, 1) }
        OverallPassRate = if ($total -eq 0) { 0.0 } else { [Math]::Round(($overallPass * 100.0) / $total, 1) }
    }
}

function Get-BaselineSamples
{
    param([object[]]$Results)

    foreach ($result in $Results)
    {
        [pscustomobject]@{
            Suite = $result.Suite
            Relative = $result.Relative
            Status = Get-ResultStatus $result
            BuildStatus = $result.BuildStatus
            RunStatus = $result.RunStatus
            BuildSkipReason = $result.BuildSkipReason
        }
    }
}

function Write-JsonFile
{
    param(
        [object]$Value,
        [string]$Path
    )

    $directory = Split-Path $Path -Parent
    New-Item -ItemType Directory -Force $directory | Out-Null
    $Value | ConvertTo-Json -Depth 8 | Set-Content -Path $Path -Encoding UTF8
}

function Compare-Baseline
{
    param(
        [object]$Baseline,
        [object[]]$CurrentSamples
    )

    $baselineMap = @{}
    foreach ($sample in @($Baseline.Samples))
    {
        $baselineMap["$($sample.Suite)|$($sample.Relative)"] = $sample
    }

    $currentMap = @{}
    foreach ($sample in $CurrentSamples)
    {
        $currentMap["$($sample.Suite)|$($sample.Relative)"] = $sample
    }

    $newPass = New-Object System.Collections.Generic.List[object]
    $regressions = New-Object System.Collections.Generic.List[object]
    $unchangedFail = New-Object System.Collections.Generic.List[object]
    $newSamples = New-Object System.Collections.Generic.List[object]
    $missingSamples = New-Object System.Collections.Generic.List[object]

    foreach ($sample in $CurrentSamples)
    {
        $key = "$($sample.Suite)|$($sample.Relative)"
        if (!$baselineMap.ContainsKey($key))
        {
            $newSamples.Add($sample)
            continue
        }

        $previous = $baselineMap[$key]
        if ($previous.Status -eq "pass" -and $sample.Status -ne "pass")
        {
            $regressions.Add([pscustomobject]@{
                Suite = $sample.Suite
                Relative = $sample.Relative
                PreviousStatus = $previous.Status
                CurrentStatus = $sample.Status
            })
        }
        elseif ($previous.Status -ne "pass" -and $sample.Status -eq "pass")
        {
            $newPass.Add([pscustomobject]@{
                Suite = $sample.Suite
                Relative = $sample.Relative
                PreviousStatus = $previous.Status
                CurrentStatus = $sample.Status
            })
        }
        elseif ($sample.Status -ne "pass")
        {
            $unchangedFail.Add([pscustomobject]@{
                Suite = $sample.Suite
                Relative = $sample.Relative
                PreviousStatus = $previous.Status
                CurrentStatus = $sample.Status
            })
        }
    }

    foreach ($sample in @($Baseline.Samples))
    {
        $key = "$($sample.Suite)|$($sample.Relative)"
        if (!$currentMap.ContainsKey($key))
        {
            $missingSamples.Add($sample)
        }
    }

    [pscustomobject]@{
        BaselineVersion = $Baseline.Version
        BaselineGeneratedAt = $Baseline.GeneratedAt
        BaselineGitCommit = $Baseline.GitCommit
        NewPass = $newPass.ToArray()
        Regressions = $regressions.ToArray()
        UnchangedFail = $unchangedFail.ToArray()
        NewSamples = $newSamples.ToArray()
        MissingSamples = $missingSamples.ToArray()
        NewPassCount = $newPass.Count
        RegressionCount = $regressions.Count
        UnchangedFailCount = $unchangedFail.Count
        NewSampleCount = $newSamples.Count
        MissingSampleCount = $missingSamples.Count
    }
}

function Write-Report
{
    param(
        [object[]]$Results,
        [string]$Path,
        [object]$Comparison = $null
    )

    $total = $Results.Count
    $buildSkip = @($Results | Where-Object { $_.BuildSkipped }).Count
    $buildPass = @($Results | Where-Object { $_.BuildPassed }).Count
    $runEligible = @($Results | Where-Object { $_.BuildPassed -and (Test-Path $_.Executable) }).Count
    $runPass = @($Results | Where-Object { $_.RunPassed }).Count
    $overallPass = @($Results | Where-Object { $_.BuildPassed -and $_.RunPassed }).Count

    function Rate([int]$Value, [int]$Denominator)
    {
        if ($Denominator -eq 0)
        {
            return "0.0%"
        }
        return "{0:N1}%" -f (($Value * 100.0) / $Denominator)
    }

    $rows = foreach ($result in $Results)
    {
        $class = if ($result.BuildPassed -and $result.RunPassed) {
            "pass"
        } elseif ($result.BuildSkipped) {
            "build-skip"
        } elseif (!$result.BuildPassed) {
            "build-fail"
        } else {
            "run-fail"
        }
        "<tr class='$class'><td>$(ConvertTo-HtmlText $result.Suite)</td><td>$(ConvertTo-HtmlText $result.Relative)</td><td>$(ConvertTo-HtmlText $result.BuildStatus)</td><td>$(ConvertTo-HtmlText $result.RunStatus)</td><td><pre>$(ConvertTo-HtmlText $result.Detail)</pre></td></tr>"
    }

    $comparisonHtml = ""
    if ($null -ne $Comparison)
    {
        $comparisonHtml = @"
<div class="summary">
<div class="metric"><span>New pass</span><strong>$($Comparison.NewPassCount)</strong></div>
<div class="metric"><span>Regressions</span><strong>$($Comparison.RegressionCount)</strong></div>
<div class="metric"><span>New samples</span><strong>$($Comparison.NewSampleCount)</strong></div>
<div class="metric"><span>Missing samples</span><strong>$($Comparison.MissingSampleCount)</strong></div>
</div>
"@
    }

    $generatedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $html = @"
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenWatcom Sample Loader Report</title>
<style>
body { font-family: Segoe UI, Arial, sans-serif; margin: 24px; color: #1f2937; }
h1 { font-size: 22px; margin-bottom: 4px; }
.summary { display: flex; gap: 12px; margin: 20px 0; flex-wrap: wrap; }
.metric { border: 1px solid #d1d5db; border-radius: 4px; padding: 10px 12px; min-width: 150px; }
.metric strong { display: block; font-size: 20px; }
table { border-collapse: collapse; width: 100%; font-size: 13px; }
th, td { border: 1px solid #d1d5db; padding: 6px 8px; vertical-align: top; }
th { background: #f3f4f6; text-align: left; }
tr.pass { background: #ecfdf5; }
tr.build-skip { background: #eff6ff; }
tr.build-fail { background: #fff7ed; }
tr.run-fail { background: #fef2f2; }
pre { white-space: pre-wrap; margin: 0; max-height: 140px; overflow: auto; }
</style>
</head>
<body>
<h1>OpenWatcom Sample Loader Report</h1>
<div>Generated at $(ConvertTo-HtmlText $generatedAt)</div>
<div>Version $(ConvertTo-HtmlText (Get-ProjectVersion))</div>
<div class="summary">
<div class="metric"><span>Total</span><strong>$total</strong></div>
<div class="metric"><span>Overall pass</span><strong>$(Rate $overallPass $total)</strong><small>$overallPass / $total</small></div>
<div class="metric"><span>Build pass</span><strong>$(Rate $buildPass $total)</strong><small>$buildPass / $total</small></div>
<div class="metric"><span>Build skip</span><strong>$buildSkip</strong><small>explicitly not built</small></div>
<div class="metric"><span>Run pass</span><strong>$(Rate $runPass $runEligible)</strong><small>$runPass / $runEligible runnable</small></div>
</div>
$comparisonHtml
<table>
<thead><tr><th>Suite</th><th>Sample</th><th>Build</th><th>Run</th><th>Detail</th></tr></thead>
<tbody>
$($rows -join "`n")
</tbody>
</table>
</body>
</html>
"@

    $reportDirectory = Split-Path $Path -Parent
    New-Item -ItemType Directory -Force $reportDirectory | Out-Null
    Set-Content -Path $Path -Value $html -Encoding UTF8
}

$PreviousBackend = $env:REPIU_EXECUTION_BACKEND
$PreviousGuestTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS

Push-Location $Root
try
{
    $env:REPIU_EXECUTION_BACKEND = $Backend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $GuestTimeoutMilliseconds.ToString()
    Write-Host ("Loader execution policy: backend=$Backend" +
        " timeout=$GuestTimeoutMilliseconds ms")

    if ($CompareBaseline -and $UpdateBaseline)
    {
        throw "-CompareBaseline and -UpdateBaseline cannot be used together."
    }

    if (!(Test-Path $ResolvedManifestPath))
    {
        throw "OpenWatcom sample build manifest was not found. Run scripts\build_openwatcom_samples.ps1 first."
    }
    if (!(Test-Path $Loader))
    {
        throw "Loader executable was not found at $LoaderRelative. Run scripts\build_openwatcom_samples.ps1 -Configuration $Configuration first."
    }

    $manifest = Get-Content $ResolvedManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $samples = @($manifest.Samples)
    if ($samples.Count -eq 0)
    {
        throw "OpenWatcom sample build manifest contains no samples."
    }

    $results = New-Object System.Collections.Generic.List[object]
    $index = 0
    foreach ($sample in $samples)
    {
        ++$index
        Write-Host "[$index/$($samples.Count)] test $($sample.Suite) $($sample.Relative)"

        $buildPassed = [bool]$sample.BuildPassed
        $buildSkipped = $false
        if ($sample.PSObject.Properties["BuildSkipped"])
        {
            $buildSkipped = [bool]$sample.BuildSkipped
        }
        $runPassed = $false
        $runStatus = "not run"
        $detail = $sample.BuildOutput
        $buildStatus = if ($buildPassed) { "pass" } elseif ($buildSkipped) { "skip" } else { "fail" }
        $buildSkipReason = ""
        if ($sample.PSObject.Properties["BuildSkipReason"])
        {
            $buildSkipReason = [string]$sample.BuildSkipReason
        }

        if ($buildPassed -and (Test-Path $sample.Executable))
        {
            $run = Invoke-Capture `
                -FilePath $Loader `
                -Arguments @($sample.Executable) `
                -WorkingDirectory $sample.BuildDirectory `
                -TimeoutSeconds $SampleTimeoutSeconds
            # Task 429: the old criterion was exit code plus "no exception", which
            # a timeout also satisfies -- the guest stalls, nothing is caught, and
            # the loader exits 0. Four of eight sampled dynamic-only "passes" were
            # timeouts scored as passes. Completion is now required explicitly:
            # a genuine pass reports "returned: true", a timeout "returned: false"
            # with "minimal execution attempt timed out".
            #
            # Task 434: a sample the harness had to kill is a failure too. It is
            # kept as its own term rather than folded into the exit-code test,
            # because a killed process's exit code is not meaningful.
            $runPassed =
                -not $run.TimedOut -and
                $run.ExitCode -eq 0 -and
                $run.Output -match "Win32 minimal execution exception caught: false" -and
                $run.Output -match "Win32 minimal execution returned: true" -and
                $run.Output -notmatch "minimal execution attempt timed out"
            $runStatus = if ($runPassed)
                         { "pass" }
                         elseif ($run.TimedOut)
                         { "fail (harness timeout)" }
                         else
                         { "fail" }
            $detail = $run.Output.Trim()
        }

        $results.Add([pscustomobject]@{
            Suite = $sample.Suite
            Relative = $sample.Relative
            Executable = $sample.Executable
            BuildPassed = $buildPassed
            BuildSkipped = $buildSkipped
            BuildStatus = $buildStatus
            BuildSkipReason = $buildSkipReason
            RunPassed = $runPassed
            RunStatus = $runStatus
            Detail = $detail
        })
    }

    $resultArray = $results.ToArray()
    $summary = Get-Summary -Results $resultArray -Manifest $manifest
    $baselineSamples = @(Get-BaselineSamples -Results $resultArray)

    Write-JsonFile -Value ([pscustomobject]@{
        Summary = $summary
        Samples = $baselineSamples
    }) -Path ([string]$ResolvedSummaryPath)

    $comparison = $null
    if ($CompareBaseline)
    {
        if (!(Test-Path $ResolvedBaselinePath))
        {
            throw "OpenWatcom sample baseline was not found at $BaselinePath. Run with -UpdateBaseline to create it."
        }

        $baseline = Get-Content $ResolvedBaselinePath -Raw -Encoding UTF8 | ConvertFrom-Json

        # Task 429: a baseline recorded under a different pass criterion is not
        # comparable. Say so loudly, because the first run after tightening the
        # criterion will report samples that were only ever passing as timeouts,
        # and those are measurement corrections rather than code regressions.
        $baselineCriterion = $null
        if ($baseline.PSObject.Properties["RunCriterion"])
        {
            $baselineCriterion = [string]$baseline.RunCriterion
        }
        elseif ($baseline.PSObject.Properties["Summary"] -and
                $baseline.Summary.PSObject.Properties["RunCriterion"])
        {
            # Task 434: Task 429 recorded the criterion inside Summary but never
            # at the top level, so a baseline re-recorded under the new rule
            # still looked like it predated it. Read either position.
            $baselineCriterion = [string]$baseline.Summary.RunCriterion
        }
        if ($baselineCriterion -ne $script:RunCriterionId)
        {
            $shown = if ([string]::IsNullOrEmpty($baselineCriterion))
                     { "(none - predates Task 429)" } else { $baselineCriterion }
            Write-Warning ("Baseline pass criterion differs from the current one." +
                " baseline=$shown current=$($script:RunCriterionId)." +
                " Reported regressions may be measurement corrections, not code" +
                " regressions. Re-record the baseline with -UpdateBaseline once" +
                " the difference has been reviewed.")
        }

        # Task 434: the same argument applies to the build configuration. A
        # Debug-recorded baseline compared against a Release run mixes two
        # timing profiles, and the criterion counts a timeout as a failure.
        $baselineConfiguration = $null
        if ($baseline.PSObject.Properties["Configuration"])
        {
            $baselineConfiguration = [string]$baseline.Configuration
        }
        elseif ($baseline.PSObject.Properties["Summary"] -and
                $baseline.Summary.PSObject.Properties["Configuration"])
        {
            $baselineConfiguration = [string]$baseline.Summary.Configuration
        }
        if ($baselineConfiguration -ne $Configuration)
        {
            $shownConfiguration = if ([string]::IsNullOrEmpty($baselineConfiguration))
                                  { "(none - predates Task 434, recorded on Debug)" }
                                  else { $baselineConfiguration }
            Write-Warning ("Baseline build configuration differs from this run." +
                " baseline=$shownConfiguration current=$Configuration." +
                " Debug and Release do not share a timing profile, so reported" +
                " regressions may be timeouts rather than code regressions.")
        }

        $comparison = Compare-Baseline -Baseline $baseline -CurrentSamples $baselineSamples
        Write-JsonFile -Value $comparison -Path ([string]$ResolvedRegressionPath)
    }

    Write-Report -Results $resultArray -Path ([string]$ResolvedReportPath) -Comparison $comparison

    if ($UpdateBaseline)
    {
        $baselineRecord = [pscustomobject]@{
            GeneratedAt = $summary.GeneratedAt
            # Task 434: both guards are read from the top level by
            # -CompareBaseline, so record them there and not only inside Summary.
            RunCriterion = $summary.RunCriterion
            Configuration = $summary.Configuration
            Version = $summary.Version
            GitCommit = $summary.GitCommit
            GitBranch = $summary.GitBranch
            ManifestGeneratedAt = $summary.ManifestGeneratedAt
            Suites = $summary.Suites
            Summary = $summary
            Samples = $baselineSamples
        }

        Write-JsonFile -Value $baselineRecord -Path ([string]$ResolvedBaselinePath)

        # Task 434: the history keeps JSON only.
        #
        # Task 049 also snapshotted the HTML report here, when this file was the
        # only place a past run could be read from. Those snapshots grew with the
        # detail they carry -- 5.6 MB at 0.0.5 against 28 MB at 0.0.135, 127 MB
        # across the directory -- and every one of them is in the repository
        # permanently. The release workflow now uploads the same report as a
        # build artifact, so the readable copy has another home while the JSON
        # keeps the structured data this file compares against.
        $historyBaseName = "{0}-{1}" -f (Get-Date -Format "yyyyMMdd-HHmmss"), $summary.Version
        $resolvedHistoryFile = Join-Path $ResolvedHistoryPath "$historyBaseName.json"
        Write-JsonFile -Value $baselineRecord -Path ([string]$resolvedHistoryFile)

        Write-Host "OpenWatcom sample baseline: $ResolvedBaselinePath"
        Write-Host "OpenWatcom sample history: $resolvedHistoryFile"
    }

    Write-Host ""
    Write-Host "OpenWatcom sample report: $ResolvedReportPath"
    Write-Host "OpenWatcom sample summary: $ResolvedSummaryPath"
    if ($CompareBaseline)
    {
        Write-Host "OpenWatcom sample baseline comparison: $ResolvedRegressionPath"
        if ($comparison.RegressionCount -gt 0 -or $comparison.MissingSampleCount -gt 0)
        {
            throw "OpenWatcom sample baseline comparison failed: $($comparison.RegressionCount) regressions, $($comparison.MissingSampleCount) missing samples."
        }
    }
}
finally
{
    $env:REPIU_EXECUTION_BACKEND = $PreviousBackend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $PreviousGuestTimeout
    Pop-Location
}
