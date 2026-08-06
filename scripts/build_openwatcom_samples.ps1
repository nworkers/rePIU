param(
    [string[]]$Suites = @("clibexam", "cplbexam"),
    # Task 434: which loader configuration to build, so the harness can test the
    # same Release binary that ships. Only the host build is affected -- the
    # samples themselves are compiled by wcl386 and are configuration-neutral.
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    [string]$ManifestPath = "build\openwatcom_samples\manifest.json",
    [switch]$SkipSetup,
    [switch]$SkipHostBuild
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Watcom = Join-Path $Root "tools\openwatcom"
$Compiler = Join-Path $Watcom "binnt\wcl386.exe"
$BuildRoot = Join-Path $Root "build\openwatcom_samples"
$ResolvedManifestPath = Join-Path $Root $ManifestPath

function Invoke-Step
{
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments = @()
    )

    Write-Host ""
    Write-Host "== $Name =="
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
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

function Get-RelativeName
{
    param(
        [string]$BasePath,
        [string]$Path
    )

    $baseUri = [System.Uri]((Resolve-Path $BasePath).Path + [System.IO.Path]::DirectorySeparatorChar)
    $pathUri = [System.Uri]((Resolve-Path $Path).Path)
    return [System.Uri]::UnescapeDataString(
        $baseUri.MakeRelativeUri($pathUri).ToString()).Replace("/", "\")
}

function Get-SafeName
{
    param([string]$Name)

    $safe = $Name -replace "[:\\\/\.\s]+", "_"
    $safe = $safe -replace "[^A-Za-z0-9_\-]", "_"
    return $safe.Trim("_")
}

function New-BuildPlan
{
    param(
        [bool]$Skip = $false,
        [string]$SkipReason = "",
        [string[]]$Options = @()
    )

    [pscustomobject]@{
        Skip = $Skip
        SkipReason = $SkipReason
        Options = $Options
    }
}

function Get-SampleBuildPlan
{
    param([object]$Sample)

    $key = "$($Sample.Suite)|$($Sample.Relative)"

    $skipReasons = @{
        "clibexam|_bfreese.c" = "based heap APIs are not available in the DOS/4GW flat 32-bit target"
        "clibexam|_bheapse.c" = "based heap APIs are not available in the DOS/4GW flat 32-bit target"
        "clibexam|_dwdelcl.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_dwshutd.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_dwstabo.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_dwstapt.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_dwstcnt.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_dwyield.c" = "default windowing sample is not a DOS/4GW console target"
        "clibexam|_bthread.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_clear87.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_ethread.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_expand.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_fpreset.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_pclose.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_pipe.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_popen.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|_status8.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|cwait.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|halloc.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|hfree.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|int86x.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|wait.c" = "sample requires runtime symbols unavailable for the current DOS/4GW target"
        "clibexam|lseek.c" = "installed file is documentation-style sample text, not standalone C source"
        "clibexam|strncoll.c" = "installed sample signature does not match the current header declaration"
        "clibexam|strnicol.c" = "installed sample signature does not match the current header declaration"
        "clibexam|strninc.c" = "installed file contains invalid #ninclude directives"
    }

    if ($skipReasons.ContainsKey($key))
    {
        return New-BuildPlan -Skip $true -SkipReason $skipReasons[$key]
    }

    $optionOverrides = @{
        "clibexam|setnew.c" = @("-cc++")
        "cplbexam|contain\wcldintr.cpp" = @("-xs")
        "cplbexam|contain\wcldptr.cpp" = @("-xs")
        "cplbexam|contain\wcldval.cpp" = @("-xs")
        "cplbexam|ios\except.cpp" = @("-xs")
    }

    if ($optionOverrides.ContainsKey($key))
    {
        return New-BuildPlan -Options $optionOverrides[$key]
    }

    return New-BuildPlan
}

function Get-Samples
{
    $clibRoot = Join-Path $Watcom "samples\clibexam"
    $cplbRoot = Join-Path $Watcom "samples\cplbexam"
    $samples = New-Object System.Collections.Generic.List[object]
    $includeClib = $Suites -contains "clibexam"
    $includeCplb = $Suites -contains "cplbexam"

    if ($includeClib -and (Test-Path $clibRoot))
    {
        Get-ChildItem $clibRoot -Recurse -File -Filter *.c |
            Sort-Object FullName |
            ForEach-Object {
                $relative = Get-RelativeName $clibRoot $_.FullName
                $samples.Add([pscustomobject]@{
                    Suite = "clibexam"
                    Source = $_.FullName
                    Relative = $relative
                    Language = "c"
                })
            }
    }

    if ($includeCplb -and (Test-Path $cplbRoot))
    {
        Get-ChildItem $cplbRoot -Recurse -File -Include *.c,*.cc,*.cpp,*.cxx |
            Sort-Object FullName |
            ForEach-Object {
                $relative = Get-RelativeName $cplbRoot $_.FullName
                $samples.Add([pscustomobject]@{
                    Suite = "cplbexam"
                    Source = $_.FullName
                    Relative = $relative
                    Language = "cpp"
                })
            }
    }

    return $samples
}

Push-Location $Root
try
{
    if (!$SkipSetup)
    {
        & (Join-Path $PSScriptRoot "setup_test_environment.ps1")
        if ($LASTEXITCODE -ne 0)
        {
            throw "setup_test_environment.ps1 failed with exit code $LASTEXITCODE"
        }
    }

    if (!$SkipHostBuild)
    {
        # Task 434: call the script directly rather than through the .bat, which
        # has no way to pass a configuration through.
        Invoke-Step `
            -Name "Build Win32 x86 host ($Configuration)" `
            -FilePath "powershell" `
            -Arguments @(
                "-ExecutionPolicy", "Bypass",
                "-File", (Join-Path $PSScriptRoot "build_win32_x86.ps1"),
                "-Configuration", $Configuration)
    }

    if (!(Test-Path $Compiler))
    {
        throw "OpenWatcom compiler was not found at tools\openwatcom\binnt\wcl386.exe"
    }

    $env:WATCOM = $Watcom
    $env:PATH = (Join-Path $Watcom "binnt") + [System.IO.Path]::PathSeparator + $env:PATH
    $env:INCLUDE = (Join-Path $Watcom "h") + [System.IO.Path]::PathSeparator + (Join-Path $Watcom "h\nt")
    $env:LIB = (Join-Path $Watcom "lib386") + [System.IO.Path]::PathSeparator + (Join-Path $Watcom "lib386\dos")

    $samples = @(Get-Samples)
    if ($samples.Count -eq 0)
    {
        throw "No OpenWatcom samples were found."
    }

    New-Item -ItemType Directory -Force $BuildRoot | Out-Null
    $results = New-Object System.Collections.Generic.List[object]
    $index = 0
    foreach ($sample in $samples)
    {
        ++$index
        Write-Host "[$index/$($samples.Count)] build $($sample.Suite) $($sample.Relative)"
        $safeName = Get-SafeName "$($sample.Suite)_$($sample.Relative)"
        $sampleBuildDir = Join-Path $BuildRoot $safeName
        New-Item -ItemType Directory -Force $sampleBuildDir | Out-Null
        $exePath = Join-Path $sampleBuildDir "sample.exe"
        $plan = Get-SampleBuildPlan $sample

        if ($plan.Skip)
        {
            $build = [pscustomobject]@{
                ExitCode = 0
                Output = $plan.SkipReason
            }
            $buildPassed = $false
            $buildStatus = "skip"
        }
        else
        {
            $arguments = @("-q") + @($plan.Options) + @("-bt=dos", "-l=dos4g", "-fe=$exePath", $sample.Source)
            $build = Invoke-Capture `
                -FilePath $Compiler `
                -Arguments $arguments `
                -WorkingDirectory $sampleBuildDir

            $buildPassed = $build.ExitCode -eq 0 -and (Test-Path $exePath)
            $buildStatus = if ($buildPassed) { "pass" } else { "fail" }
        }
        $results.Add([pscustomobject]@{
            Suite = $sample.Suite
            Relative = $sample.Relative
            Source = $sample.Source
            Executable = $exePath
            BuildDirectory = $sampleBuildDir
            BuildPassed = $buildPassed
            BuildSkipped = [bool]$plan.Skip
            BuildStatus = $buildStatus
            BuildSkipReason = $plan.SkipReason
            BuildOptions = @($plan.Options)
            BuildExitCode = $build.ExitCode
            BuildOutput = $build.Output.Trim()
        })
    }

    $manifestDirectory = Split-Path $ResolvedManifestPath -Parent
    New-Item -ItemType Directory -Force $manifestDirectory | Out-Null
    [pscustomobject]@{
        GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Suites = $Suites
        Total = $results.Count
        BuildPassed = @($results | Where-Object { $_.BuildPassed }).Count
        BuildSkipped = @($results | Where-Object { $_.BuildSkipped }).Count
        Samples = $results.ToArray()
    } | ConvertTo-Json -Depth 8 | Set-Content -Path $ResolvedManifestPath -Encoding UTF8

    Write-Host ""
    Write-Host "OpenWatcom sample build manifest: $ResolvedManifestPath"
}
finally
{
    Pop-Location
}
