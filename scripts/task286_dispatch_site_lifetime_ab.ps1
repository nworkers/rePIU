param(
    [ValidateSet("0", "call")]
    [string]$Mode = "call",
    [int]$DurationMilliseconds = 240000
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$loader = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_loader_win32.exe"
$fixture = Join-Path $repoRoot "eeprom.dat"
if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) {
    throw "loader not found: $loader"
}
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    throw "EEPROM fixture not found: $fixture"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultRoot = Join-Path $repoRoot `
    "build\task286-dispatch-lifetime-$Mode-$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null
$stdoutLog = Join-Path $resultRoot "stdout.log"
$stderrLog = Join-Path $resultRoot "stderr.log"
$runEeprom = Join-Path $resultRoot "eeprom.dat"
Copy-Item -LiteralPath $fixture -Destination $runEeprom

$names = @(
    "REPIU_EXECUTION_BACKEND",
    "REPIU_EXECUTION_TIMEOUT_MS",
    "REPIU_EEPROM_PATH",
    "REPIU_AOT_DBT_INDIRECT",
    "REPIU_AOT_DBT_CALL_TRACE",
    "REPIU_AOT_DBT_CALL_STEP"
)
$previous = @{}
foreach ($name in $names) {
    $previous[$name] = [Environment]::GetEnvironmentVariable($name)
}

Push-Location $repoRoot
try {
    $env:REPIU_EXECUTION_BACKEND = "dynamic"
    $env:REPIU_EXECUTION_TIMEOUT_MS =
        $DurationMilliseconds.ToString()
    $env:REPIU_EEPROM_PATH = $runEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $Mode
    Remove-Item Env:REPIU_AOT_DBT_CALL_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:REPIU_AOT_DBT_CALL_STEP -ErrorAction SilentlyContinue

    $process = Start-Process -FilePath $loader `
        -ArgumentList @("pumpit1") `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -WindowStyle Hidden -Wait -PassThru
    $combinedLog = @(
        Get-Content -LiteralPath $stdoutLog
        Get-Content -LiteralPath $stderrLog
    )
    $exception = $combinedLog |
        Where-Object { $_ -match 'minimal execution exception caught' } |
        Select-Object -Last 1
    $timeout = $combinedLog |
        Where-Object { $_ -match 'minimal execution timed out' } |
        Select-Object -Last 1
    $progress = $combinedLog |
        Where-Object { $_ -match 'diagnostic progress count' } |
        Select-Object -Last 1
    $indirect = $combinedLog |
        Where-Object { $_ -match 'dynamic indirect entry/attempt' } |
        Select-Object -Last 1
    $window = $combinedLog |
        Where-Object { $_ -match 'Glide window opens/logical size' } |
        Select-Object -Last 1
    $sha = (Get-FileHash -LiteralPath $runEeprom -Algorithm SHA256).Hash
    Write-Host ("loader_exit={0}" -f $process.ExitCode)
    Write-Host ("eeprom_sha256={0}" -f $sha)
    Write-Host ("exception={0}" -f $exception)
    Write-Host ("timeout={0}" -f $timeout)
    Write-Host ("progress={0}" -f $progress)
    Write-Host ("indirect={0}" -f $indirect)
    Write-Host ("window={0}" -f $window)
}
finally {
    Pop-Location
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable(
            $name, $previous[$name], "Process")
    }
}

$fixtureSha = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
Write-Host ("fixture_eeprom_sha256={0}" -f $fixtureSha)
Write-Host ("results={0}" -f $resultRoot)
