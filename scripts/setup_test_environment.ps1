param(
    [switch]$SkipOpenWatcomInstall,
    # Original MAME-format game assets are proprietary and never in the repository,
    # so a clean checkout -- CI's, or a contributor's -- cannot satisfy the check
    # below. The OpenWatcom sample suite compiles its own DOS
    # programs and runs them through the loader; it never touches PIU.EXE. This
    # switch lets that caller reuse the rest of this script (toolchain checks and
    # the self-healing OpenWatcom install) without demanding assets it does not
    # use. `pumpit1` callers must leave it off.
    [switch]$SkipGameAssets
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$OpenWatcomCompiler = Join-Path $Root "tools\openwatcom\binnt\wcl386.exe"
$Pumpit1RomZip = Join-Path $Root "roms\pumpit1.zip"
$Pumpit1ChdDirectory = Join-Path $Root "roms\pumpit1"

function Write-CheckOk
{
    param([string]$Message)
    Write-Host "[ok] $Message"
}

function Write-CheckWarn
{
    param([string]$Message)
    Write-Host "[warn] $Message"
}

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

    return $null
}

function Resolve-VisualStudio
{
    if (!(Test-Path $VsWhere))
    {
        return $null
    }

    $vsRoot = & $VsWhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or !$vsRoot)
    {
        return $null
    }

    return $vsRoot
}

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

Push-Location $Root
try
{
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git -eq $null)
    {
        throw "Git was not found on PATH. Install Git for Windows before running tests."
    }
    Write-CheckOk "Git: $($git.Source)"

    $cmake = Resolve-CMake
    if ($cmake -eq $null)
    {
        throw "CMake was not found. Install CMake or Visual Studio with the C++ desktop workload."
    }
    Write-CheckOk "CMake: $cmake"

    $vsRoot = Resolve-VisualStudio
    if ($vsRoot -eq $null)
    {
        throw "Visual Studio C++ x86/x64 build tools were not found. Install the Desktop development with C++ workload."
    }
    Write-CheckOk "Visual Studio C++ tools: $vsRoot"

    $pumpit1Chd = Get-ChildItem `
        -Path $Pumpit1ChdDirectory `
        -Filter "*.chd" `
        -File `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!(Test-Path $Pumpit1RomZip) -or $null -eq $pumpit1Chd)
    {
        if (!$SkipGameAssets)
        {
            throw "pumpit1 assets were not found. Provide roms\pumpit1.zip and a CHD under roms\pumpit1 before running game tests."
        }
        Write-CheckWarn "pumpit1 assets are absent; skipping the game asset check. pumpit1 tests will not run."
    }
    else
    {
        Write-CheckOk "pumpit1 assets: roms\pumpit1.zip and $($pumpit1Chd.Name)"
    }

    if (!(Test-Path $OpenWatcomCompiler))
    {
        if ($SkipOpenWatcomInstall)
        {
            throw "OpenWatcom was not found at tools\openwatcom. Re-run without -SkipOpenWatcomInstall to download and install it."
        }

        Write-CheckWarn "OpenWatcom was not found. Installing local copy."
        Invoke-Step `
            -Name "Install OpenWatcom" `
            -FilePath "powershell" `
            -Arguments @(
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                "scripts\install_openwatcom.ps1"
            )
    }
    else
    {
        Write-CheckOk "OpenWatcom: tools\openwatcom\binnt\wcl386.exe"
    }

    Invoke-Step `
        -Name "Build DOS/4GW hello sample" `
        -FilePath "cmd" `
        -Arguments @("/c", "scripts\build_dos4gw_hello.bat")

    Write-Host ""
    Write-Host "Test environment is ready."
}
finally
{
    Pop-Location
}
