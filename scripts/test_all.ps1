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

function ConvertTo-ProcessArgument
{
    param([string]$Value)

    if ($Value -notmatch '[\s"]')
    {
        return $Value
    }

    return '"' + ($Value -replace '\\(?=\\*")', '$0$0' -replace '"', '\"') + '"'
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
        if ($null -ne $startInfo.ArgumentList)
        {
            foreach ($argument in $Arguments)
            {
                [void]$startInfo.ArgumentList.Add($argument)
            }
        }
        else
        {
            $startInfo.Arguments =
                (($Arguments | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join " ")
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
    $piuStoppedAtExpectedException =
        $piuOutput -match "Win32 minimal execution exception caught: true" -and
        $piuOutput -match "Win32 minimal execution exception code: 0xC0000005" -and
        $piuOutput -match "Win32 minimal execution exception address: 0x0[1-9](01E1[0-9A-F]{2}|0F5F[0-9A-F]{2}|0F7[AB][0-9A-F]{2})" -and
        $piuOutput -match "Win32 minimal execution exception context captured: true" -and
        $piuOutput -match "Win32 minimal execution exception EIP: 0x0[1-9](01E1[0-9A-F]{2}|0F5F[0-9A-F]{2}|0F7[AB][0-9A-F]{2})" -and
        $piuOutput -match "Win32 minimal execution thread exit code: 2" -and
        $piuOutput -match "Win32 minimal execution message: original entry raised a caught exception" -and
        $piuOutput -match "Privileged instruction opcode: 0x(03|38|83|8B|89|C7)"
    $piuTimedOutAfterProgress =
        $piuOutput -match "Win32 minimal execution exception caught: false" -and
        $piuOutput -match "Win32 minimal execution timed out: true" -and
        $piuOutput -match "Win32 minimal execution thread exit code: 3" -and
        $piuOutput -match "Win32 minimal execution message: minimal execution attempt timed out"
    if ($piuOutput -notmatch "Win32 loader executable: MASTER/PIU_1ST/PIU/PIU.EXE" -or
        $piuOutput -notmatch "DOS virtual filesystem root: .+MASTER\\PIU_1ST" -or
        $piuOutput -notmatch "DOS virtual filesystem current directory: \\PIU" -or
        $piuOutput -notmatch "Runtime memory arena reserve size: 0x015D7000" -or
        $piuOutput -notmatch "Win32 relocated image placed size: 0x015D7000" -or
        $piuOutput -notmatch "Win32 relocated selector binding count: 4" -or
        $piuOutput -notmatch "Win32 relocated selector binding: selector=0x0024 object=2" -or
        $piuOutput -notmatch "Win32 relocated selector binding: selector=0x002C object=3" -or
        $piuOutput -notmatch "Win32 minimal execution returned: false" -or
        -not ($piuStoppedAtExpectedException -or $piuTimedOutAfterProgress) -or
        $piuOutput -notmatch "Win32 last single-step context captured: true" -or
        $piuOutput -notmatch "Win32 diagnostic poll iterations: [1-9]" -or
        $piuOutput -notmatch "Win32 diagnostic progress count: [1-9]" -or
        $piuOutput -notmatch "Win32 exception dispatch entry count: [1-9]" -or
        $piuOutput -notmatch "Win32 exception dispatch exit count: [1-9]" -or
        $piuOutput -notmatch "Win32 exception dispatch outstanding count: [01]" -or
        $piuOutput -notmatch "Win32 exception dispatch last EIP: 0x0[1-9][0-9A-F]{6}" -or
        $piuOutput -notmatch "Win32 selector table valid: true" -or
        $piuOutput -notmatch "Win32 selector descriptor count: [1-9]" -or
        $piuOutput -notmatch "Win32 DOS low memory valid: true" -or
        $piuOutput -notmatch "Win32 DOS low memory bytes: 65536" -or
        $piuOutput -notmatch "Win32 DOS environment block bytes: [1-9]" -or
        $piuOutput -notmatch "Win32 DOS environment access observed: true" -or
        $piuOutput -notmatch "Win32 last DOS environment entry: .+=<redacted>" -or
        $piuOutput -notmatch "Win32 last DOS environment value bytes: [0-9]" -or
        $piuOutput -notmatch "Win32 handled HLE trap count: [1-9]" -or
        $piuOutput -notmatch "Win32 port I/O observation count: 0" -or
        $piuOutput -notmatch "Win32 DOS path trace stored count: [2-9]" -or
        $piuOutput -notmatch "Win32 DOS path trace limit reached: false" -or
        $piuOutput -notmatch "Win32 allocator probe observation count: [0-9]+" -or
        $piuOutput -notmatch "Win32 allocator probe trace stored count: ([0-9]|1[0-6])" -or
        $piuOutput -notmatch "Win32 allocator probe trace wrapped: (true|false)" -or
        $piuOutput -notmatch "Win32 allocator control-flow observation count: [0-9]+" -or
        $piuOutput -notmatch "Win32 allocator control-flow trace stored count: ([0-9]|[12][0-9]|3[0-2])" -or
        $piuOutput -notmatch "Win32 allocator control-flow trace wrapped: (true|false)" -or
        $piuOutput -notmatch "Win32 DOS path trace #1 service=chdir result=failure error=0x0003 drive=0x00 access=0x00 guest=\\datas\\bga virtual=\\DATAS\\BGA" -or
        $piuOutput -notmatch "Win32 DOS path trace #2 service=open result=success error=0x0000 drive=0x00 access=0x00 guest=intro.ani virtual=\\PIU\\INTRO.ANI" -or
        $piuOutput -notmatch "Win32 handled DOS interrupt count: [1-9]" -or
        $piuOutput -notmatch "Win32 last handled DOS interrupt vector: 0x21" -or
        $piuOutput -notmatch "Win32 last handled DOS interrupt AH: 0x4[4A]" -or
        $piuOutput -notmatch "Win32 last handled DOS interrupt AX: 0x(4400|4A2B)" -or
        $piuOutput -notmatch "Win32 handled DOS chdir count: [1-9]" -or
        $piuOutput -notmatch "Win32 last DOS chdir guest path: \\datas\\bga" -or
        $piuOutput -notmatch "Win32 last DOS chdir result: failure" -or
        $piuOutput -notmatch "Win32 handled DOS getcwd count: 0" -or
        $piuOutput -notmatch "Win32 handled DOS get drive count: 0" -or
        $piuOutput -notmatch "Win32 handled DOS open count: [1-9]" -or
        $piuOutput -notmatch "Win32 last DOS open guest path: (intro\.ani|stage\.cfg)" -or
        $piuOutput -notmatch "Win32 last DOS open virtual path: \\PIU\\(INTRO\.ANI|STAGE\.CFG)" -or
        $piuOutput -notmatch "Win32 last DOS open result: success" -or
        $piuOutput -notmatch "Win32 last DOS open handle: 0x000[56]" -or
        $piuOutput -notmatch "Win32 handled DOS read count: [1-9]" -or
        $piuOutput -notmatch "Win32 last DOS read handle: 0x0005" -or
        $piuOutput -notmatch "Win32 last DOS read requested bytes: 0" -or
        $piuOutput -notmatch "Win32 last DOS read actual bytes: 0" -or
        $piuOutput -notmatch "Win32 last DOS read buffer: 0x0[1-9][0-9A-F]{6}" -or
        $piuOutput -notmatch "Win32 last DOS read result: success" -or
        $piuOutput -notmatch "Win32 handled DOS seek count: [1-9]" -or
        $piuOutput -notmatch "Win32 last DOS seek handle: 0x0005" -or
        $piuOutput -notmatch "Win32 last DOS seek origin: 0x00" -or
        $piuOutput -notmatch "Win32 last DOS seek offset: 44544" -or
        $piuOutput -notmatch "Win32 last DOS seek position: 44544" -or
        $piuOutput -notmatch "Win32 last DOS seek result: success" -or
        $piuOutput -notmatch "Win32 handled DOS close count: [1-9]" -or
        $piuOutput -notmatch "Win32 last DOS close handle: 0x0005" -or
        $piuOutput -notmatch "Win32 last DOS close result: success" -or
        $piuOutput -notmatch "Win32 handled DOS resize count: [1-9]" -or
        $piuOutput -notmatch "Win32 handled low-memory access count: [1-9]" -or
        $piuOutput -notmatch "Win32 segment load trace stored count: [1-9]" -or
        $piuOutput -notmatch "Win32 segment load trace wrapped: false" -or
        $piuOutput -notmatch "Win32 handled segment memory load count: [1-9]" -or
        $piuOutput -notmatch "Win32 last handled segment memory load address: 0x0[1-9]0F4DD2" -or
        $piuOutput -notmatch "Win32 last handled segment memory load opcode: 0xA4" -or
        $piuOutput -notmatch "Win32 last segment memory load register: DS" -or
        $piuOutput -notmatch "Win32 last segment memory load selector: 0x002C" -or
        $piuOutput -notmatch "Win32 last segment memory load offset: 0x[0-9A-F]{8}" -or
        $piuOutput -notmatch "Win32 last segment memory load width: 1" -or
        $piuOutput -notmatch "Win32 last segment memory load value: 0x00" -or
        $piuOutput -notmatch "Win32 handled memory store count: [1-9][0-9]{4}" -or
        $piuOutput -notmatch "Win32 last handled memory store address: 0x0[1-9](01E1[0-9A-F]{2}|0F5F[0-9A-F]{2}|0F7AD4)" -or
        $piuOutput -notmatch "Win32 last memory store opcode: 0x(66C7|83|89|C7)" -or
        $piuOutput -notmatch "Win32 last memory store source kind: (mov-imm16|mov-imm32|mov-reg32|or-imm8)" -or
        $piuOutput -notmatch "Win32 last memory store applied: false" -or
        $piuOutput -notmatch "Win32 shadow memory write count: [1-9][0-9]{4}" -or
        $piuOutput -notmatch "Win32 shadow memory read hit count: [1-9][0-9]{3,}" -or
        $piuOutput -notmatch "Win32 shadow memory byte count: [1-9][0-9]{4}" -or
        $piuOutput -notmatch "Win32 shadow memory range valid: true" -or
        ($piuStoppedAtExpectedException -and
         $piuOutput -notmatch "Current execution blocker: unhandled or unclassified instruction/memory access at exception point"))
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
