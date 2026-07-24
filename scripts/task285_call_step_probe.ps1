param(
    [Parameter(Mandatory = $true)]
    [int]$Sequence,
    [int]$DurationMilliseconds = 90000
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$loader = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_loader_win32.exe"
if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) {
    throw "loader not found: $loader"
}

$fixture = Join-Path $repoRoot "eeprom.dat"
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    throw "EEPROM fixture not found: $fixture"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultRoot = Join-Path $repoRoot `
    "build\task285-call-step-seq$Sequence-$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null
$stdoutLog = Join-Path $resultRoot "stdout.log"
$stderrLog = Join-Path $resultRoot "stderr.log"
$runEeprom = Join-Path $resultRoot "eeprom.dat"
Copy-Item -LiteralPath $fixture -Destination $runEeprom

$prevBackend = $env:REPIU_EXECUTION_BACKEND
$prevTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS
$prevEeprom = $env:REPIU_EEPROM_PATH
$prevIndirect = $env:REPIU_AOT_DBT_INDIRECT
$prevCallTrace = $env:REPIU_AOT_DBT_CALL_TRACE
$prevCallStep = $env:REPIU_AOT_DBT_CALL_STEP

Push-Location $repoRoot
try {
    $env:REPIU_EXECUTION_BACKEND = "aot-dbt"
    $env:REPIU_EXECUTION_TIMEOUT_MS =
        $DurationMilliseconds.ToString()
    $env:REPIU_EEPROM_PATH = $runEeprom
    $env:REPIU_AOT_DBT_INDIRECT = "call"
    $env:REPIU_AOT_DBT_CALL_TRACE = "1"
    $env:REPIU_AOT_DBT_CALL_STEP = $Sequence.ToString()

    $process = Start-Process -FilePath $loader `
        -ArgumentList @("pumpit1") `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -WindowStyle Hidden -Wait -PassThru
    $combinedLog = @(
        Get-Content -LiteralPath $stdoutLog
        Get-Content -LiteralPath $stderrLog
    )
    $stepSummary = $combinedLog |
        Where-Object {
            $_ -match 'AOT-DBT CALL step probe targets/events'
        } |
        Select-Object -Last 1
    $exceptionState = $combinedLog |
        Where-Object {
            $_ -match 'minimal execution exception caught'
        } |
        Select-Object -Last 1
    $timeoutState = $combinedLog |
        Where-Object {
            $_ -match 'minimal execution timed out'
        } |
        Select-Object -Last 1
    $sha = (Get-FileHash -LiteralPath $runEeprom -Algorithm SHA256).Hash
    Write-Host ("loader_exit={0}" -f $process.ExitCode)
    Write-Host ("eeprom_sha256={0}" -f $sha)
    Write-Host ("exception: {0}" -f $exceptionState)
    Write-Host ("timeout: {0}" -f $timeoutState)
    Write-Host ("step: {0}" -f $stepSummary)
}
finally {
    Pop-Location
    $env:REPIU_EXECUTION_BACKEND = $prevBackend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $prevTimeout
    $env:REPIU_EEPROM_PATH = $prevEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $prevIndirect
    $env:REPIU_AOT_DBT_CALL_TRACE = $prevCallTrace
    $env:REPIU_AOT_DBT_CALL_STEP = $prevCallStep
}

$fixtureSha = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
Write-Host ("fixture_eeprom_sha256={0}" -f $fixtureSha)
Write-Host ("results={0}" -f $resultRoot)
