param(
    [ValidateRange(1000, 3600000)]
    [int]$DurationMilliseconds = 240000,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 3,

    [switch]$CompareCache,

    [switch]$CompareRejectCache,

    [switch]$CompareWrites,

    [switch]$CompareJumps,

    [ValidateRange(0, 20)]
    [int]$StartupRetries = 8
)

$ErrorActionPreference = "Stop"
$comparisonCount = [int]$CompareCache.IsPresent +
    [int]$CompareRejectCache.IsPresent +
    [int]$CompareWrites.IsPresent + [int]$CompareJumps.IsPresent
if ($comparisonCount -gt 1) {
    throw "CompareCache, CompareRejectCache, CompareWrites, and CompareJumps are mutually exclusive"
}

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
$benchmarkKind = if ($CompareCache) {
    "dynamic-direct-cache"
}
elseif ($CompareRejectCache) {
    "dynamic-direct-reject-cache"
}
elseif ($CompareWrites) {
    "dynamic-direct-writes"
}
elseif ($CompareJumps) {
    "dynamic-direct-jumps"
}
else {
    "dynamic-direct"
}
$resultRoot = Join-Path $repoRoot `
    "build\benchmarks\native-linear-span\$benchmarkKind\$timestamp"
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
    "REPIU_NATIVE_LINEAR_SPAN_CACHE",
    "REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE",
    "REPIU_NATIVE_LINEAR_SPAN_WRITES",
    "REPIU_NATIVE_LINEAR_SPAN_JUMPS",
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
        $featureEnabled = $sequence[$runIndex]
        $compareExtension = $CompareCache -or $CompareWrites -or
            $CompareJumps -or $CompareRejectCache
        $spanEnabled = if ($compareExtension) { 1 } else { $featureEnabled }
        $cacheEnabled = if ($CompareCache) { $featureEnabled } else { 0 }
        $rejectCacheEnabled = if ($CompareRejectCache) {
            $featureEnabled
        }
        else {
            0
        }
        $writesEnabled = if ($CompareWrites) { $featureEnabled } else { 0 }
        $jumpsEnabled = if ($CompareJumps) { $featureEnabled } else { 0 }
        $runNumber = $runIndex + 1
        $mode = if ($featureEnabled -eq 1) { "on" } else { "off" }
        $featureName = if ($CompareCache) {
            "cache"
        }
        elseif ($CompareRejectCache) {
            "reject-cache"
        }
        elseif ($CompareWrites) {
            "writes"
        }
        elseif ($CompareJumps) {
            "jumps"
        }
        else {
            "span"
        }
        $runName = "run-{0:D2}-{1}-{2}" -f `
            $runNumber, $featureName, $mode
        $stdoutLog = Join-Path $resultRoot "$runName-stdout.log"
        $stderrLog = Join-Path $resultRoot "$runName-stderr.log"
        $runEeprom = Join-Path $resultRoot "$runName-eeprom.dat"
        $env:REPIU_EXECUTION_BACKEND = "dynamic"
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
        $env:REPIU_NATIVE_LINEAR_SPAN_CACHE =
            $cacheEnabled.ToString()
        $env:REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE =
            $rejectCacheEnabled.ToString()
        $env:REPIU_NATIVE_LINEAR_SPAN_WRITES =
            $writesEnabled.ToString()
        $env:REPIU_NATIVE_LINEAR_SPAN_JUMPS =
            $jumpsEnabled.ToString()

        $startupAttempt = 0
        do {
            ++$startupAttempt
            Copy-Item -LiteralPath $fixture -Destination $runEeprom -Force
            Write-Host (`
                "direct native-span run {0}/{1} attempt={2}: span={3}, cache={4}, reject_cache={5}, writes={6}, jumps={7}, duration_ms={8}" -f `
                $runNumber, $sequence.Count, $startupAttempt, $spanEnabled, `
                $cacheEnabled, $rejectCacheEnabled, $writesEnabled, `
                $jumpsEnabled, `
                $DurationMilliseconds)
            $process = Start-Process -FilePath $loader `
                -ArgumentList @("pumpit1") `
                -RedirectStandardOutput $stdoutLog `
                -RedirectStandardError $stderrLog `
                -WindowStyle Hidden -Wait -PassThru
            if ($process.ExitCode -ne 0 -and
                $startupAttempt -le $StartupRetries) {
                Write-Warning (`
                    "startup failed for run {0}; retrying ({1}/{2})" -f `
                    $runNumber, $startupAttempt, $StartupRetries)
            }
        } while ($process.ExitCode -ne 0 -and
                 $startupAttempt -le $StartupRetries)
        if ($process.ExitCode -ne 0) {
            throw "direct native-span run $runNumber exhausted startup retries"
        }

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
            (Find-LastLine $lines 'dynamic HLE reentry attempt/success:') `
            'success: ([0-9/]+)' 2
        $dbtReturn = Read-SlashValues `
            (Find-LastLine $lines `
                'dynamic return entry/attempt/success/fallback:') `
            'fallback: ([0-9/]+)' 4
        $span = Read-SlashValues `
            (Find-LastLine $lines `
                'Win32 native linear span entry/boundary/cancel/instructions/reject:') `
            'reject: ([0-9/]+)' 5
        $spanCache = Read-SlashValues `
            (Find-LastLine $lines `
                'Win32 native linear span cache hit/miss:') `
            'miss: ([0-9/]+)' 2
        $spanRejectCache = Read-SlashValues `
            (Find-LastLine $lines `
                'Win32 native linear span reject cache hit/miss/stale/store/capacity-skip:') `
            'capacity-skip: ([0-9/]+)' 5
        $spanWrite = Read-SlashValues `
            (Find-LastLine $lines `
                'Win32 native linear span write cross/uncovered/fault-cancel:') `
            'fault-cancel: ([0-9/]+)' 3
        $spanJump = Read-SlashValues `
            (Find-LastLine $lines `
                'Win32 native linear span jump chain/backward-stop:') `
            'backward-stop: ([0-9/]+)' 2
        $spanCancelLine = Find-LastLine $lines `
            'Win32 native linear span last cancel code/eip:'
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
            span_cache_enabled = $cacheEnabled
            span_reject_cache_enabled = $rejectCacheEnabled
            span_writes_enabled = $writesEnabled
            span_jumps_enabled = $jumpsEnabled
            duration_ms = $DurationMilliseconds
            startup_attempts = $startupAttempt
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
            span_entry = $span[0]
            span_boundary = $span[1]
            span_cancel = $span[2]
            span_native_instructions = $span[3]
            span_reject = $span[4]
            span_cache_hit = $spanCache[0]
            span_cache_miss = $spanCache[1]
            span_reject_cache_hit = $spanRejectCache[0]
            span_reject_cache_miss = $spanRejectCache[1]
            span_reject_cache_stale = $spanRejectCache[2]
            span_reject_cache_store = $spanRejectCache[3]
            span_reject_cache_capacity_skip = $spanRejectCache[4]
            span_write_cross = $spanWrite[0]
            span_write_guard_uncovered = $spanWrite[1]
            span_write_fault_cancel = $spanWrite[2]
            span_jump_chain = $spanJump[0]
            span_backward_jump_stop = $spanJump[1]
            span_last_cancel_code = Read-Metric $spanCancelLine `
                'code/eip: (0x[0-9A-Fa-f]+)'
            span_last_cancel_eip = Read-Metric $spanCancelLine `
                'code/eip: 0x[0-9A-Fa-f]+/(0x[0-9A-Fa-f]+)'
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
$results | Format-Table run, pair, span_enabled, span_cache_enabled, `
    span_reject_cache_enabled, span_writes_enabled, progress, `
    single_step, span_entry, `
    span_jumps_enabled, span_jump_chain, span_backward_jump_stop, `
    span_cache_hit, span_cache_miss, span_write_cross, `
    span_reject_cache_hit, span_reject_cache_miss, `
    span_write_guard_uncovered, span_write_fault_cancel, `
    texture_download_count, draw_count, buffer_swap_count, `
    exception_caught, fatal_count, eeprom_matches_fixture -AutoSize
Write-Host "direct native-span results: $csvPath"
