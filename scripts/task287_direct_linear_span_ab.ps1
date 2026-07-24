param(
    [ValidateRange(1000, 3600000)]
    [int]$DurationMilliseconds = 240000,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 3
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$loader = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_loader_win32.exe"
$fixture = Join-Path $repoRoot "eeprom.dat"
if (-not (Test-Path -LiteralPath $loader -PathType Leaf)) {
    throw "Win32 loader was not found: $loader"
}
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    throw "EEPROM fixture was not found: $fixture"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultRoot = Join-Path $repoRoot `
    "build\benchmarks\native-linear-span\aot-dbt-direct\$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null
$fixtureSha256 =
    (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash

$sequence = New-Object System.Collections.Generic.List[int]
for ($index = 0; $index -lt $Repetitions; ++$index) {
    if (($index % 2) -eq 0) {
        $sequence.Add(0)
        $sequence.Add(1)
    }
    else {
        $sequence.Add(1)
        $sequence.Add(0)
    }
}

function Read-Metric {
    param(
        [string]$Line,
        [string]$Pattern
    )
    if ($Line -match $Pattern) {
        return $matches[1]
    }
    return $null
}

function Find-LastLine {
    param(
        [string[]]$Lines,
        [string]$Pattern
    )
    return $Lines |
        Where-Object { $_ -match $Pattern } |
        Select-Object -Last 1
}

function Read-SlashValues {
    param(
        [string]$Line,
        [string]$Pattern,
        [int]$Count
    )
    $values = New-Object long[] $Count
    $text = Read-Metric $Line $Pattern
    if ($null -ne $text) {
        $parts = $text.Split('/')
        if ($parts.Count -eq $Count) {
            for ($index = 0; $index -lt $Count; ++$index) {
                $values[$index] = [long]$parts[$index]
            }
        }
    }
    return $values
}

function Read-GlideCallCount {
    param(
        [string[]]$Lines,
        [int]$Ordinal
    )
    $line = Find-LastLine $Lines `
        ("Glide call trace: ordinal={0} .* count=" -f $Ordinal)
    $count = Read-Metric $line ' count=([0-9]+)'
    if ($null -eq $count) {
        return 0
    }
    return [long]$count
}

$environmentNames = @(
    "REPIU_EXECUTION_BACKEND",
    "REPIU_EXECUTION_TIMEOUT_MS",
    "REPIU_AOT_INDIRECT_CACHE_SLOTS",
    "REPIU_NATIVE_REGION",
    "REPIU_NATIVE_LINEAR_SPAN",
    "REPIU_EEPROM_PATH",
    "REPIU_AOT_DBT_INDIRECT",
    "REPIU_AOT_DBT_CALL_TRACE",
    "REPIU_AOT_DBT_CALL_STEP"
)
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] =
        [Environment]::GetEnvironmentVariable($name)
}
$results = New-Object System.Collections.Generic.List[object]

Push-Location $repoRoot
try {
    for ($runIndex = 0; $runIndex -lt $sequence.Count; ++$runIndex) {
        $spanEnabled = $sequence[$runIndex]
        $runNumber = $runIndex + 1
        $mode = if ($spanEnabled -eq 1) { "on" } else { "off" }
        $runName = "run-{0:D2}-span-{1}" -f $runNumber, $mode
        $stdoutLog = Join-Path $resultRoot "$runName-stdout.log"
        $stderrLog = Join-Path $resultRoot "$runName-stderr.log"
        $runEeprom = Join-Path $resultRoot "$runName-eeprom.dat"
        Copy-Item -LiteralPath $fixture -Destination $runEeprom

        $env:REPIU_EXECUTION_BACKEND = "aot-dbt"
        $env:REPIU_EXECUTION_TIMEOUT_MS =
            $DurationMilliseconds.ToString()
        $env:REPIU_AOT_INDIRECT_CACHE_SLOTS = "4"
        $env:REPIU_EEPROM_PATH = $runEeprom
        Remove-Item Env:REPIU_NATIVE_REGION -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_INDIRECT -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_CALL_TRACE -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_CALL_STEP -ErrorAction SilentlyContinue
        if ($spanEnabled -eq 1) {
            $env:REPIU_NATIVE_LINEAR_SPAN = "1"
        }
        else {
            $env:REPIU_NATIVE_LINEAR_SPAN = "0"
        }

        Write-Host (`
            "direct native-span run {0}/{1}: span={2}, duration_ms={3}" -f `
            $runNumber, $sequence.Count, $mode, $DurationMilliseconds)
        $process = Start-Process -FilePath $loader `
            -ArgumentList @("pumpit1") `
            -RedirectStandardOutput $stdoutLog `
            -RedirectStandardError $stderrLog `
            -WindowStyle Hidden -Wait -PassThru

        $lines = @(
            Get-Content -LiteralPath $stdoutLog
            Get-Content -LiteralPath $stderrLog
        )
        $aot = Read-SlashValues `
            (Find-LastLine $lines 'AOT entry/boundary/reentry/fallback:') `
            'fallback: ([0-9/]+)' 4
        $boundary = Read-SlashValues `
            (Find-LastLine $lines 'AOT boundary reason ret/indir') `
            'other: ([0-9/]+)' 5
        $dbtHle = Read-SlashValues `
            (Find-LastLine $lines 'AOT-DBT HLE reentry attempt/success:') `
            'success: ([0-9/]+)' 2
        $dbtReturn = Read-SlashValues `
            (Find-LastLine $lines `
                'AOT-DBT return entry/attempt/success/fallback:') `
            'fallback: ([0-9/]+)' 4
        $drawCount = 0
        foreach ($ordinal in 71..76) {
            $drawCount += Read-GlideCallCount $lines $ordinal
        }
        $runEepromSha256 =
            (Get-FileHash -LiteralPath $runEeprom -Algorithm SHA256).Hash

        $results.Add([pscustomobject]@{
            run = $runNumber
            pair = [math]::Floor($runIndex / 2) + 1
            span_enabled = $spanEnabled
            duration_ms = $DurationMilliseconds
            loader_exit = $process.ExitCode
            exception_caught = Read-Metric `
                (Find-LastLine $lines 'minimal execution exception caught:') `
                'caught: (true|false)'
            timed_out = Read-Metric `
                (Find-LastLine $lines 'minimal execution timed out:') `
                'out: (true|false)'
            progress = Read-Metric `
                (Find-LastLine $lines 'diagnostic progress count:') `
                'count: ([0-9]+)'
            single_step = Read-Metric `
                (Find-LastLine $lines 'single-step trace count:') `
                'count: ([0-9]+)'
            aot_entry = $aot[0]
            aot_boundary = $aot[1]
            aot_reentry = $aot[2]
            legacy_fallback = $aot[3]
            boundary_return = $boundary[0]
            boundary_indirect = $boundary[1]
            boundary_other = $boundary[4]
            dbt_hle_attempt = $dbtHle[0]
            dbt_hle_success = $dbtHle[1]
            dbt_return_entry = $dbtReturn[0]
            dbt_return_attempt = $dbtReturn[1]
            dbt_return_success = $dbtReturn[2]
            dbt_return_fallback = $dbtReturn[3]
            texture_download_count = Read-GlideCallCount $lines 49
            draw_count = $drawCount
            buffer_swap_count = Read-GlideCallCount $lines 85
            fatal_count = Read-Metric `
                (Find-LastLine $lines `
                    'handled original fatal breakpoint count:') `
                'count: ([0-9]+)'
            eeprom_sha256 = $runEepromSha256
            fixture_sha256 = $fixtureSha256
            eeprom_matches_fixture =
                $runEepromSha256 -eq $fixtureSha256
            stdout_log = $stdoutLog
            stderr_log = $stderrLog
            eeprom_copy = $runEeprom
        })
    }
}
finally {
    Pop-Location
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable(
            $name, $previousEnvironment[$name], "Process")
    }
}

$csvPath = Join-Path $resultRoot "results.csv"
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation
$results | Format-Table run, pair, span_enabled, progress, single_step, `
    texture_download_count, draw_count, buffer_swap_count, `
    exception_caught, fatal_count, eeprom_matches_fixture -AutoSize
Write-Host "direct native-span results: $csvPath"
