param(
    [switch]$SkipSetup
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Loader = Join-Path $Root "build\win32_x86_debug\Debug\repiu_loader_win32.exe"

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

function Invoke-CaptureStep
{
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments = @()
    )

    Write-Host ""
    Write-Host "== $Name =="
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

    $output | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0)
    {
        throw "$Name failed with exit code $exitCode"
    }

    return ($output | Out-String)
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

    Invoke-Step `
        -Name "Build Win32 x86 host" `
        -FilePath "cmd" `
        -Arguments @("/c", "scripts\build_win32_x86.bat")

    Invoke-Step `
        -Name "Build DOS/4GW hello sample" `
        -FilePath "cmd" `
        -Arguments @("/c", "scripts\build_dos4gw_hello.bat")

    if (!(Test-Path $Loader))
    {
        throw "Loader executable was not found at build\win32_x86_debug\Debug\repiu_loader_win32.exe"
    }

    $helloOutput = Invoke-CaptureStep `
        -Name "Run dos4gw_hello target" `
        -FilePath $Loader `
        -Arguments @("dos4gw_hello")
    if ($helloOutput -notmatch "Hello, world!")
    {
        throw "dos4gw_hello did not print the expected Hello, world! output."
    }

    $piuOutput = Invoke-CaptureStep `
        -Name "Run piu_1st target" `
        -FilePath $Loader `
        -Arguments @("piu_1st")
    if ($piuOutput -notmatch "Win32 handled segment load count: 3")
    {
        throw "piu_1st did not reach the expected current HLE observation point."
    }

    Write-Host ""
    Write-Host "All current tests passed."
}
finally
{
    Pop-Location
}
