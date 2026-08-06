# Task 434: builds the two release archives from an existing build tree.
#
# This lives in a script rather than inline in the workflow so the same
# procedure produces the same package locally. It builds nothing -- run
# scripts\build_win32_x86.ps1 and, for the sample report,
# scripts\test_openwatcom_samples.ps1 first.
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Version = "",
    [string]$OutputDir = "build\package",
    [string]$SampleReportDir = "build\openwatcom_sample_report",
    # The sample report is required by default, because CI must never publish a
    # release that silently lacks its test evidence. Local packaging that has
    # not run the suite passes this switch.
    [switch]$AllowMissingSampleReport
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BinaryDir = Join-Path $Root "build\win32_x86_debug\$Configuration"
$ResolvedOutputDir = Join-Path $Root $OutputDir
$ResolvedSampleReportDir = Join-Path $Root $SampleReportDir

function Get-ProjectVersion
{
    $versionFile = Join-Path $Root "VERSION"
    if (!(Test-Path $versionFile))
    {
        throw "Project VERSION file was not found."
    }

    $value = (Get-Content $versionFile -Raw -Encoding UTF8).Trim()
    if ($value -notmatch "^\d+\.\d+\.\d+$")
    {
        throw "Project VERSION must use major.minor.patch format."
    }

    return $value
}

function New-Archive
{
    param(
        [string]$ArchivePath,
        [string]$StagingDir
    )

    if (Test-Path $ArchivePath)
    {
        Remove-Item -LiteralPath $ArchivePath -Force
    }

    Compress-Archive -Path (Join-Path $StagingDir "*") -DestinationPath $ArchivePath
    $size = (Get-Item -LiteralPath $ArchivePath).Length
    Write-Host ("  {0} ({1:N0} bytes)" -f (Split-Path $ArchivePath -Leaf), $size)
}

if ([string]::IsNullOrWhiteSpace($Version))
{
    $Version = Get-ProjectVersion
}
$Version = $Version.TrimStart("v")

Write-Host "Packaging rePIU v$Version from $Configuration"

# The binaries. SDL3 is linked statically (SDL_SHARED OFF / SDL_STATIC ON), so
# no runtime DLL travels with these.
$binaries = @(
    "repiu_loader_win32.exe",
    "repiu_supervisor_win32.exe",
    "repiu_exe_analyzer.exe",
    "repiu_chd_cd_probe.exe",
    "repiu_aot_probe.exe",
    "repiu_glide_issue_probe.exe"
)
$documents = @("VERSION", "README.md", "THIRD_PARTY_NOTICES.md")

$missing = @()
foreach ($binary in $binaries)
{
    if (!(Test-Path (Join-Path $BinaryDir $binary)))
    {
        $missing += $binary
    }
}
if ($missing.Count -gt 0)
{
    throw ("Missing $Configuration binaries: " + ($missing -join ", ") +
        ". Run scripts\build_win32_x86.ps1 -Configuration $Configuration first.")
}

New-Item -ItemType Directory -Force $ResolvedOutputDir | Out-Null
$stagingRoot = Join-Path $ResolvedOutputDir "staging"
if (Test-Path $stagingRoot)
{
    Remove-Item -LiteralPath $stagingRoot -Recurse -Force
}

$binaryStaging = Join-Path $stagingRoot "rePIU-v$Version-win32"
New-Item -ItemType Directory -Force $binaryStaging | Out-Null
foreach ($binary in $binaries)
{
    Copy-Item -LiteralPath (Join-Path $BinaryDir $binary) -Destination $binaryStaging
}
foreach ($document in $documents)
{
    Copy-Item -LiteralPath (Join-Path $Root $document) -Destination $binaryStaging
}

$binaryArchive = Join-Path $ResolvedOutputDir "rePIU-v$Version-win32.zip"
New-Archive -ArchivePath $binaryArchive -StagingDir $binaryStaging

# The sample test evidence. Uploaded alongside the binaries so a release can be
# read without re-running the suite.
$sampleArchive = Join-Path $ResolvedOutputDir "openwatcom-samples-v$Version.zip"
$reportFiles = @("index.html", "summary.json", "regressions.json")
$presentReportFiles = @()
foreach ($reportFile in $reportFiles)
{
    $candidate = Join-Path $ResolvedSampleReportDir $reportFile
    if (Test-Path $candidate)
    {
        $presentReportFiles += $candidate
    }
}

# regressions.json exists only after -CompareBaseline, so require the two that
# every run produces rather than all three.
$requiredPresent = @("index.html", "summary.json") |
    Where-Object { Test-Path (Join-Path $ResolvedSampleReportDir $_) }

if ($requiredPresent.Count -lt 2)
{
    if (!$AllowMissingSampleReport)
    {
        throw ("Sample report was not found in $SampleReportDir. Run " +
            "scripts\test_openwatcom_samples.ps1 -Configuration $Configuration " +
            "-CompareBaseline first, or pass -AllowMissingSampleReport.")
    }

    Write-Warning "Sample report is missing; skipping $([System.IO.Path]::GetFileName($sampleArchive))."
}
else
{
    $sampleStaging = Join-Path $stagingRoot "openwatcom-samples-v$Version"
    New-Item -ItemType Directory -Force $sampleStaging | Out-Null
    foreach ($reportFile in $presentReportFiles)
    {
        Copy-Item -LiteralPath $reportFile -Destination $sampleStaging
    }

    New-Archive -ArchivePath $sampleArchive -StagingDir $sampleStaging
}

Remove-Item -LiteralPath $stagingRoot -Recurse -Force

Write-Host ""
Write-Host "Package directory: $ResolvedOutputDir"
