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
    if ($piuOutput -notmatch "Runtime memory arena reserve size: 0x005E7000" -or
        $piuOutput -notmatch "Win32 relocated image placed size: 0x005E7000" -or
        $piuOutput -notmatch "Win32 last handled DOS interrupt AH: 0x4A" -or
        $piuOutput -notmatch "Win32 handled DOS chdir count: 1" -or
        $piuOutput -notmatch "Win32 last DOS chdir guest path: \\datas\\bga" -or
        $piuOutput -notmatch "Win32 last DOS chdir virtual path: \\DATAS\\BGA" -or
        $piuOutput -notmatch "Win32 last DOS chdir result: success" -or
        $piuOutput -notmatch "Win32 handled DOS open count: 2" -or
        $piuOutput -notmatch "Win32 last DOS open guest path: stage.cfg" -or
        $piuOutput -notmatch "Win32 last DOS open virtual path: \\DATAS\\BGA\\STAGE.CFG" -or
        $piuOutput -notmatch "Win32 last DOS open result: failure" -or
        $piuOutput -notmatch "Win32 last DOS open error: 0x0002" -or
        $piuOutput -notmatch "Win32 handled DOS IOCTL count: 2" -or
        $piuOutput -notmatch "Win32 last DOS IOCTL subfunction: 0x00" -or
        $piuOutput -notmatch "Win32 last DOS IOCTL handle: 0x0001" -or
        $piuOutput -notmatch "Win32 last DOS IOCTL result: success" -or
        $piuOutput -notmatch "Win32 last DOS IOCTL device info: 0x0080" -or
        $piuOutput -notmatch "Win32 handled DOS resize count: 40" -or
        $piuOutput -notmatch "Win32 last DOS resize selector: 0x0024" -or
        $piuOutput -notmatch "Win32 last DOS resize paragraphs: 0x4AE1" -or
        $piuOutput -notmatch "Win32 last DOS resize result: success" -or
        $piuOutput -notmatch "Win32 HLE console output bytes: 10" -or
        $piuOutput -notmatch "Win32 minimal execution exception address: 0x020F7340" -or
        $piuOutput -notmatch "Privileged instruction opcode: 0xC7" -or
        $piuOutput -notmatch "Privileged instruction classification: unknown" -or
        $piuOutput -notmatch "Privileged instruction classification message: opcode is not recognized by the initial privileged instruction classifier")
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
