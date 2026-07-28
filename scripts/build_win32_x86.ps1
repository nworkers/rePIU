# Task 331: the configuration is a parameter so the Debug and the Release loader
# are produced by the same procedure. Correctness work stays on Debug; every
# performance measurement is taken on Release, because Task 330 measured an
# 11.34x Debug factor for plan building that also inverts the stage ranking.
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    [string[]]$Target = @()
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
# One multi-config Visual Studio tree holds every configuration, so the
# directory name is historical: `Debug\` and `Release\` are subdirectories of it.
$BuildDir = Join-Path $Root "build\win32_x86_debug"
$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

function Add-PathDirectory
{
    param([string]$Directory)

    if (!(Test-Path $Directory))
    {
        return
    }

    $resolved = (Resolve-Path $Directory).Path
    $pathParts = $env:PATH -split [System.IO.Path]::PathSeparator
    if ($pathParts -notcontains $resolved)
    {
        $env:PATH = $resolved + [System.IO.Path]::PathSeparator + $env:PATH
    }
}

function Resolve-CMake
{
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake -ne $null)
    {
        return $cmake.Source
    }

    if (Test-Path $VsWhere)
    {
        $vsRoot = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and $vsRoot)
        {
            $vsCmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $vsCmake)
            {
                Add-PathDirectory (Split-Path $vsCmake -Parent)
                return $vsCmake
            }
        }
    }

    throw "CMake was not found. Install CMake or Visual Studio with the C++ desktop workload."
}

function Resolve-VisualStudioGenerator
{
    $generatorMap = @{
        "18" = "Visual Studio 18 2026"
        "17" = "Visual Studio 17 2022"
        "16" = "Visual Studio 16 2019"
    }

    if (Test-Path $VsWhere)
    {
        $installationsJson = & $VsWhere `
            -all `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -format json
        if ($LASTEXITCODE -eq 0 -and $installationsJson)
        {
            $installations = $installationsJson | ConvertFrom-Json
            $installation = $installations |
                Sort-Object {[version]$_.installationVersion} -Descending |
                Select-Object -First 1
            if ($installation -ne $null)
            {
                $major = ([version]$installation.installationVersion).Major.ToString()
                if ($generatorMap.ContainsKey($major))
                {
                    return $generatorMap[$major]
                }
            }
        }
    }

    $helpText = & (Resolve-CMake) --help
    foreach ($major in @("18", "17", "16"))
    {
        $generator = $generatorMap[$major]
        if (($helpText | Out-String) -match [regex]::Escape($generator))
        {
            return $generator
        }
    }

    throw "No supported Visual Studio C++ generator was found."
}

Push-Location $Root
try
{
    $cmake = Resolve-CMake
    $generator = Resolve-VisualStudioGenerator

    Write-Host "CMake: $cmake"
    Write-Host "Generator: $generator"
    Write-Host "Build directory: $BuildDir"
    Write-Host "Configuration: $Configuration"

    & $cmake -S . -B $BuildDir -G $generator -A Win32
    if ($LASTEXITCODE -ne 0)
    {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    $buildArguments = @("--build", $BuildDir, "--config", $Configuration)
    foreach ($name in $Target)
    {
        $buildArguments += @("--target", $name)
    }

    & $cmake @buildArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }

    Write-Host ""
    Write-Host "Output directory: $(Join-Path $BuildDir $Configuration)"
}
finally
{
    Pop-Location
}
