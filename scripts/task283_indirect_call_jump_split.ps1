param(
    [int]$DurationMilliseconds = 30000
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$supervisor = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_supervisor_win32.exe"
if (-not (Test-Path -LiteralPath $supervisor -PathType Leaf)) {
    throw "supervisor not found: $supervisor"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultRoot = Join-Path $repoRoot "build\task283-indirect-split-$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null

$fixture = Join-Path $repoRoot "eeprom.dat"
$conditions = @("0", "call", "jump")

$prevBackend = $env:REPIU_EXECUTION_BACKEND
$prevTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS
$prevEeprom = $env:REPIU_EEPROM_PATH
$prevIndirect = $env:REPIU_AOT_DBT_INDIRECT

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
        $env:REPIU_EXECUTION_TIMEOUT_MS = "0"
        $env:REPIU_EEPROM_PATH = $runEeprom
        $env:REPIU_AOT_DBT_INDIRECT = $mode

        Write-Host ("=== REPIU_AOT_DBT_INDIRECT=$mode  ({0} ms) ===" -f $DurationMilliseconds)
        $process = Start-Process -FilePath $supervisor `
            -ArgumentList @("pumpit1", "$DurationMilliseconds") `
            -RedirectStandardOutput $stdoutLog `
            -RedirectStandardError $stderrLog `
            -WindowStyle Hidden -Wait -PassThru
        $supervisorExit = $process.ExitCode

        $childExit = $null
        $lastSnapshot = ""
        foreach ($line in Get-Content -LiteralPath $stdoutLog) {
            if ($line -match '^\[repiu-supervisor\] elapsed_ms=([0-9]+)') {
                $lastSnapshot = $line
            }
            if ($line -match '\[repiu-supervisor\] child_exit=([0-9]+)') {
                $childExit = [int]$matches[1]
            }
        }
        $sha = (Get-FileHash -LiteralPath $runEeprom -Algorithm SHA256).Hash
        Write-Host ("  supervisor_exit={0}  child_exit={1}" -f $supervisorExit, $childExit)
        Write-Host ("  eeprom_sha256={0}" -f $sha)
        Write-Host ("  last: {0}" -f $lastSnapshot)
    }
}
finally {
    Pop-Location
    $env:REPIU_EXECUTION_BACKEND = $prevBackend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $prevTimeout
    $env:REPIU_EEPROM_PATH = $prevEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $prevIndirect
}
$fixtureSha = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
Write-Host ("fixture eeprom_sha256={0}" -f $fixtureSha)
Write-Host ("results: {0}" -f $resultRoot)
