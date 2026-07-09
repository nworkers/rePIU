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
        [string[]]$Arguments = @(),
        [int]$TimeoutSeconds = 30
    )

    Write-Host ""
    Write-Host "== $Name =="
    $stdoutPath = Join-Path $env:TEMP ("repiu-test-stdout-{0}.log" -f ([guid]::NewGuid()))
    $stderrPath = Join-Path $env:TEMP ("repiu-test-stderr-{0}.log" -f ([guid]::NewGuid()))
    try
    {
        $startInfo = New-Object System.Diagnostics.ProcessStartInfo
        $startInfo.FileName = $FilePath
        foreach ($argument in $Arguments)
        {
            [void]$startInfo.ArgumentList.Add($argument)
        }
        $startInfo.WorkingDirectory = (Get-Location).Path
        $startInfo.UseShellExecute = $false
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true

        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $startInfo
        [void]$process.Start()

        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit($TimeoutSeconds * 1000))
        {
            try
            {
                $process.Kill()
                $process.WaitForExit(5000) | Out-Null
            }
            catch
            {
                throw "$Name timed out after $TimeoutSeconds seconds and the process could not be killed: $($_.Exception.Message)"
            }
            throw "$Name timed out after $TimeoutSeconds seconds"
        }

        $stdoutTask.Wait()
        $stderrTask.Wait()
        $stdout = $stdoutTask.Result
        $stderr = $stderrTask.Result
        Set-Content -Path $stdoutPath -Value $stdout
        Set-Content -Path $stderrPath -Value $stderr
        $output = @()
        if ($stdout.Length -gt 0)
        {
            $output += ($stdout -split "`r?`n")
        }
        if ($stderr.Length -gt 0)
        {
            $output += ($stderr -split "`r?`n")
        }
        $exitCode = $process.ExitCode
    }
    finally
    {
        Remove-Item -LiteralPath $stdoutPath -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stderrPath -ErrorAction SilentlyContinue
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
        $piuOutput -notmatch "Win32 minimal execution returned: false" -or
        $piuOutput -notmatch "Win32 minimal execution exception caught: false" -or
        $piuOutput -notmatch "Win32 minimal execution timed out: true" -or
        $piuOutput -notmatch "Win32 handled HLE trap count: 0" -or
        $piuOutput -notmatch "Win32 handled DOS interrupt count: 0" -or
        $piuOutput -notmatch "Win32 handled memory store count: 0" -or
        $piuOutput -notmatch "Win32 minimal execution thread exit code: 3" -or
        $piuOutput -notmatch "Win32 minimal execution message: minimal execution attempt timed out")
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
