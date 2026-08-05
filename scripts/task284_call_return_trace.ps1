param(
    [int]$DurationMilliseconds = 240000
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$loader = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_loader_win32.exe"
if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) {
    throw "loader not found: $loader"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultRoot = Join-Path $repoRoot "build\task284-call-return-trace-$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null

$fixture = Join-Path $repoRoot "eeprom.dat"
$conditions = @("0", "call")

$prevBackend = $env:REPIU_EXECUTION_BACKEND
$prevTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS
$prevEeprom = $env:REPIU_EEPROM_PATH
$prevIndirect = $env:REPIU_AOT_DBT_INDIRECT
$prevCallTrace = $env:REPIU_AOT_DBT_CALL_TRACE

Push-Location $repoRoot
try {
    foreach ($mode in $conditions) {
        $runName = "indirect-$mode"
        $stdoutLog = Join-Path $resultRoot "$runName-stdout.log"
        $stderrLog = Join-Path $resultRoot "$runName-stderr.log"
        $runEeprom = Join-Path $resultRoot "$runName-eeprom.dat"
        if (Test-Path -LiteralPath $fixture -PathType Leaf) {
            Copy-Item -LiteralPath $fixture -Destination $runEeprom
        }

        $env:REPIU_EXECUTION_BACKEND = "dynamic"
        $env:REPIU_EXECUTION_TIMEOUT_MS =
            $DurationMilliseconds.ToString()
        $env:REPIU_EEPROM_PATH = $runEeprom
        $env:REPIU_AOT_DBT_INDIRECT = $mode
        $env:REPIU_AOT_DBT_CALL_TRACE = "1"

        Write-Host (
            "=== REPIU_AOT_DBT_INDIRECT=$mode CALL_TRACE=1 ({0} ms) ===" `
                -f $DurationMilliseconds)
        $process = Start-Process -FilePath $loader `
            -ArgumentList @("pumpit1") `
            -RedirectStandardOutput $stdoutLog `
            -RedirectStandardError $stderrLog `
            -WindowStyle Hidden -Wait -PassThru
        $loaderExit = $process.ExitCode
        $combinedLog = @(
            Get-Content -LiteralPath $stdoutLog
            Get-Content -LiteralPath $stderrLog
        )
        $traceSummary = $combinedLog |
            Where-Object {
                $_ -match 'dynamic CALL/RET trace stored-events/calls/returns'
            } |
            Select-Object -Last 1
        $firstDivergence = $combinedLog |
            Where-Object {
                $_ -match 'dynamic CALL/RET first divergence'
            } |
            Select-Object -First 1
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
        Write-Host ("  loader_exit={0}" -f $loaderExit)
        Write-Host ("  eeprom_sha256={0}" -f $sha)
        Write-Host ("  exception: {0}" -f $exceptionState)
        Write-Host ("  timeout: {0}" -f $timeoutState)
        Write-Host ("  trace: {0}" -f $traceSummary)
        Write-Host ("  first_divergence: {0}" -f $firstDivergence)
    }
}
finally {
    Pop-Location
    $env:REPIU_EXECUTION_BACKEND = $prevBackend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $prevTimeout
    $env:REPIU_EEPROM_PATH = $prevEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $prevIndirect
    $env:REPIU_AOT_DBT_CALL_TRACE = $prevCallTrace
}
$fixtureSha = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
Write-Host ("fixture eeprom_sha256={0}" -f $fixtureSha)
Write-Host ("results: {0}" -f $resultRoot)
