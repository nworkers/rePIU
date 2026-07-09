param(
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

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Loader = Join-Path $Root "build\win32_x86_debug\Debug\repiu_loader_win32.exe"
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
    param(
        [string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $Root
    )

    Push-Location $WorkingDirectory
    try
    {
        $oldErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try
        {
            $output = & $FilePath @Arguments 2>&1 |
                ForEach-Object { $_.ToString() }
            $exitCode = $LASTEXITCODE
        }
        finally
        {
            $ErrorActionPreference = $oldErrorActionPreference
        }
    }
    finally
    {
        Pop-Location
    }

    [pscustomobject]@{
        ExitCode = $exitCode
        Output = ($output | Out-String)
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
    $buildPass = @($Results | Where-Object { $_.BuildPassed }).Count
    $runEligible = @($Results | Where-Object { $_.BuildPassed -and (Test-Path $_.Executable) }).Count
    $runPass = @($Results | Where-Object { $_.RunPassed }).Count
    $overallPass = @($Results | Where-Object { $_.BuildPassed -and $_.RunPassed }).Count

    [pscustomobject]@{
        GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Version = Get-ProjectVersion
        GitCommit = Invoke-GitValue @("rev-parse", "HEAD")
        GitBranch = Invoke-GitValue @("branch", "--show-current")
        ManifestPath = $ManifestPath
        ManifestGeneratedAt = $Manifest.GeneratedAt
        Suites = @($Manifest.Suites)
        Total = $total
        BuildPassed = $buildPass
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

Push-Location $Root
try
{
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
        throw "Loader executable was not found at build\win32_x86_debug\Debug\repiu_loader_win32.exe. Run scripts\build_openwatcom_samples.ps1 first."
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
        $runPassed = $false
        $runStatus = "not run"
        $detail = $sample.BuildOutput

        if ($buildPassed -and (Test-Path $sample.Executable))
        {
            $run = Invoke-Capture `
                -FilePath $Loader `
                -Arguments @($sample.Executable) `
                -WorkingDirectory $sample.BuildDirectory
            $runPassed =
                $run.ExitCode -eq 0 -and
                $run.Output -match "Win32 minimal execution exception caught: false"
            $runStatus = if ($runPassed) { "pass" } else { "fail" }
            $detail = $run.Output.Trim()
        }

        $results.Add([pscustomobject]@{
            Suite = $sample.Suite
            Relative = $sample.Relative
            Executable = $sample.Executable
            BuildPassed = $buildPassed
            BuildStatus = if ($buildPassed) { "pass" } else { "fail" }
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
        $comparison = Compare-Baseline -Baseline $baseline -CurrentSamples $baselineSamples
        Write-JsonFile -Value $comparison -Path ([string]$ResolvedRegressionPath)
    }

    Write-Report -Results $resultArray -Path ([string]$ResolvedReportPath) -Comparison $comparison

    if ($UpdateBaseline)
    {
        $baselineRecord = [pscustomobject]@{
            GeneratedAt = $summary.GeneratedAt
            Version = $summary.Version
            GitCommit = $summary.GitCommit
            GitBranch = $summary.GitBranch
            ManifestGeneratedAt = $summary.ManifestGeneratedAt
            Suites = $summary.Suites
            Summary = $summary
            Samples = $baselineSamples
        }

        Write-JsonFile -Value $baselineRecord -Path ([string]$ResolvedBaselinePath)

        $historyFileName = "{0}-{1}.json" -f (Get-Date -Format "yyyyMMdd-HHmmss"), $summary.Version
        $resolvedHistoryFile = Join-Path $ResolvedHistoryPath $historyFileName
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
    Pop-Location
}
