param(
    # Task 425: backend는 legacy / dynamic 둘뿐이고, 이 벤치마크는 AOT cache가
    # 필요하므로 dynamic만 유효합니다. span A/B 축은 backend가 아니라 아래에서
    # 명시적으로 설정하는 REPIU_NATIVE_LINEAR_SPAN입니다.
    [ValidateSet("dynamic")]
    [string]$Backend = "dynamic",

    [ValidateRange(1000, 3600000)]
    [int]$DurationMilliseconds = 60000,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 1,

    [switch]$CompareCache,

    [switch]$CompareRejectCache,

    [switch]$CompareRetiredSpan,

    [switch]$CompareWrites,

    [switch]$CompareJumps,

    [switch]$ComparePostHle,

    [ValidateRange(0, 20)]
    [int]$StartupRetries = 8
)

$ErrorActionPreference = "Stop"
$comparisonCount = [int]$CompareCache.IsPresent +
    [int]$CompareRejectCache.IsPresent +
    [int]$CompareRetiredSpan.IsPresent +
    [int]$CompareWrites.IsPresent + [int]$CompareJumps.IsPresent +
    [int]$ComparePostHle.IsPresent
if ($comparisonCount -gt 1) {
    throw "CompareCache, CompareRejectCache, CompareRetiredSpan, CompareWrites, CompareJumps, and ComparePostHle are mutually exclusive"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$supervisor = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_supervisor_win32.exe"
if (-not (Test-Path -LiteralPath $supervisor -PathType Leaf)) {
    throw "Win32 supervisor was not found: $supervisor"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$benchmarkKind = if ($CompareCache) {
    "native-linear-span-cache"
}
elseif ($CompareRejectCache) {
    "native-linear-span-reject-cache"
}
elseif ($CompareRetiredSpan) {
    "aot-retired-span-reentry"
}
elseif ($CompareWrites) {
    "native-linear-span-writes"
}
elseif ($CompareJumps) {
    "native-linear-span-jumps"
}
elseif ($ComparePostHle) {
    "dynamic-post-hle"
}
else {
    "native-linear-span"
}
$benchmarkRoot = Join-Path $repoRoot `
    "build\benchmarks\$benchmarkKind\$Backend"
$resultRoot = Join-Path $benchmarkRoot $timestamp
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null

$fixture = Join-Path $repoRoot "eeprom.dat"
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    throw "EEPROM fixture was not found: $fixture"
}
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

function Set-FirstMilestone {
    param(
        [hashtable]$Milestones,
        [string]$Name,
        [int]$ElapsedMilliseconds
    )
    if ($null -eq $Milestones[$Name]) {
        $Milestones[$Name] = $ElapsedMilliseconds
    }
}

$previousBackend = $env:REPIU_EXECUTION_BACKEND
$previousTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS
$previousSlots = $env:REPIU_AOT_INDIRECT_CACHE_SLOTS
$previousRegion = $env:REPIU_NATIVE_REGION
$previousSpan = $env:REPIU_NATIVE_LINEAR_SPAN
$previousSpanCache = $env:REPIU_NATIVE_LINEAR_SPAN_CACHE
$previousSpanRejectCache = $env:REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE
$previousRetiredSpan = $env:REPIU_AOT_RETIRED_SPAN_REENTRY
$previousSpanWrites = $env:REPIU_NATIVE_LINEAR_SPAN_WRITES
$previousSpanJumps = $env:REPIU_NATIVE_LINEAR_SPAN_JUMPS
$previousPostHle = $env:REPIU_AOT_DBT_POST_HLE_TRANSLATE
$previousEeprom = $env:REPIU_EEPROM_PATH
$previousDbtIndirect = $env:REPIU_AOT_DBT_INDIRECT
$previousCallTrace = $env:REPIU_AOT_DBT_CALL_TRACE
$previousCallStep = $env:REPIU_AOT_DBT_CALL_STEP
$results = New-Object System.Collections.Generic.List[object]

Push-Location $repoRoot
try {
    for ($runIndex = 0; $runIndex -lt $sequence.Count; ++$runIndex) {
        $featureEnabled = $sequence[$runIndex]
        $compareExtension = $CompareCache -or $CompareWrites -or
            $CompareJumps -or $ComparePostHle -or $CompareRejectCache -or
            $CompareRetiredSpan
        $spanEnabled = if ($compareExtension) { 1 } else { $featureEnabled }
        $cacheEnabled = if ($CompareCache) { $featureEnabled } else { 0 }
        $rejectCacheEnabled = if ($CompareRejectCache) {
            $featureEnabled
        }
        elseif ($CompareRetiredSpan) {
            1
        }
        else {
            0
        }
        $retiredSpanEnabled = if ($CompareRetiredSpan) {
            $featureEnabled
        }
        else {
            0
        }
        $writesEnabled = if ($CompareWrites) { $featureEnabled } else { 0 }
        $jumpsEnabled = if ($CompareJumps) { $featureEnabled } else { 0 }
        $postHleEnabled = if ($ComparePostHle) { $featureEnabled } else { 0 }
        $runNumber = $runIndex + 1
        $mode = if ($featureEnabled -eq 1) { "on" } else { "off" }
        $featureName = if ($CompareCache) {
            "cache"
        }
        elseif ($CompareRejectCache) {
            "reject-cache"
        }
        elseif ($CompareRetiredSpan) {
            "retired-span"
        }
        elseif ($CompareWrites) {
            "writes"
        }
        elseif ($CompareJumps) {
            "jumps"
        }
        elseif ($ComparePostHle) {
            "post-hle"
        }
        else {
            "span"
        }
        $runName = "run-{0:D2}-{1}-{2}" -f `
            $runNumber, $featureName, $mode
        $stdoutLog = Join-Path $resultRoot "$runName-stdout.log"
        $stderrLog = Join-Path $resultRoot "$runName-stderr.log"
        $runEeprom = Join-Path $resultRoot "$runName-eeprom.dat"
        $env:REPIU_EXECUTION_BACKEND = $Backend
        $env:REPIU_EXECUTION_TIMEOUT_MS = "0"
        $env:REPIU_AOT_INDIRECT_CACHE_SLOTS = "4"
        Remove-Item Env:REPIU_NATIVE_REGION -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_INDIRECT -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_CALL_TRACE -ErrorAction SilentlyContinue
        Remove-Item Env:REPIU_AOT_DBT_CALL_STEP -ErrorAction SilentlyContinue
        $env:REPIU_EEPROM_PATH = $runEeprom
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
        $env:REPIU_AOT_RETIRED_SPAN_REENTRY =
            $retiredSpanEnabled.ToString()
        $env:REPIU_NATIVE_LINEAR_SPAN_WRITES =
            $writesEnabled.ToString()
        $env:REPIU_NATIVE_LINEAR_SPAN_JUMPS =
            $jumpsEnabled.ToString()
        $env:REPIU_AOT_DBT_POST_HLE_TRANSLATE =
            $postHleEnabled.ToString()

        $startupAttempt = 0
        $validRun = $false
        do {
            ++$startupAttempt
            Copy-Item -LiteralPath $fixture -Destination $runEeprom -Force
            Write-Host (`
                "native-span run {0}/{1} attempt={2}: backend={3}, span={4}, cache={5}, reject_cache={6}, retired_span={7}, writes={8}, jumps={9}, post_hle={10}, duration_ms={11}" -f `
                $runNumber, $sequence.Count, $startupAttempt, $Backend, `
                $spanEnabled, $cacheEnabled, $rejectCacheEnabled, `
                $retiredSpanEnabled, $writesEnabled, $jumpsEnabled, `
                $postHleEnabled, `
                $DurationMilliseconds)
            $process = Start-Process -FilePath $supervisor `
                -ArgumentList @("pumpit1", "$DurationMilliseconds") `
                -RedirectStandardOutput $stdoutLog `
                -RedirectStandardError $stderrLog `
                -WindowStyle Hidden -Wait -PassThru

            $snapshotLines = Get-Content -LiteralPath $stdoutLog |
                Where-Object { $_ -match '^\[repiu-supervisor\] elapsed_ms=' }
            $lastSnapshot = $snapshotLines | Select-Object -Last 1
            $liveLines = Get-Content -LiteralPath $stderrLog |
                Where-Object { $_ -match '^\[repiu-live\] elapsed_ms=' }
            $lastLive = $liveLines | Select-Object -Last 1
            $childExitLine = Get-Content -LiteralPath $stdoutLog |
                Where-Object { $_ -match '\[repiu-supervisor\] child_exit=' } |
                Select-Object -Last 1
            $childExit = Read-Metric $childExitLine 'child_exit=([0-9]+)'
            $lastElapsed = Read-Metric $lastSnapshot 'elapsed_ms=([0-9]+)'
            $minimumElapsed = [math]::Max(
                0, $DurationMilliseconds - 2000)
            $validRun = $null -ne $lastLive -and
                $childExit -in @("0", "124") -and
                $null -ne $lastElapsed -and
                [long]$lastElapsed -ge $minimumElapsed
            if (-not $validRun -and $startupAttempt -le $StartupRetries) {
                Write-Warning (`
                    "startup failed for run {0}; retrying ({1}/{2})" -f `
                    $runNumber, $startupAttempt, $StartupRetries)
            }
        } while (-not $validRun -and $startupAttempt -le $StartupRetries)
        if (-not $validRun) {
            throw "native-span run $runNumber exhausted startup retries"
        }

        $region = Read-SlashValues $lastLive `
            'region=([0-9/]+)' 5
        $span = Read-SlashValues $lastLive `
            'span=([0-9/]+)' 5
        $spanCache = Read-SlashValues $lastLive `
            'span_cache=([0-9/]+)' 2
        $spanRejectCache = Read-SlashValues $lastLive `
            'span_reject_cache=([0-9/]+)' 5
        $spanWrite = Read-SlashValues $lastLive `
            'span_write=([0-9/]+)' 3
        $spanJump = Read-SlashValues $lastLive `
            'span_jump=([0-9/]+)' 2
        $retiredSpan = Read-SlashValues $lastLive `
            'retired_span=([0-9/]+)' 2
        $postHle = Read-SlashValues $lastLive `
            'posthle=([0-9/]+)' 2
        $boundary = Read-SlashValues $lastSnapshot `
            'boundary_reason\(ret/indir/direct/cond/other\)=([0-9/]+)' 5
        $aot = Read-SlashValues $lastSnapshot `
            'aot_boundary/reentry=([0-9/]+)' 2
        $milestones = @{
            window_open = $null
            texture = $null
            draw = $null
            swap = $null
        }
        foreach ($line in $snapshotLines) {
            if ($line -match `
                'elapsed_ms=([0-9]+).*glide_milestone\(gate/open/tex/draw/swap\)=([0-9/]+)') {
                $elapsed = [int]$matches[1]
                $parts = $matches[2].Split('/')
                if ($parts.Count -eq 5) {
                    if ([int]$parts[1] -ne 0) {
                        Set-FirstMilestone $milestones "window_open" $elapsed
                    }
                    if ([int]$parts[2] -ne 0) {
                        Set-FirstMilestone $milestones "texture" $elapsed
                    }
                    if ([int]$parts[3] -ne 0) {
                        Set-FirstMilestone $milestones "draw" $elapsed
                    }
                    if ([int]$parts[4] -ne 0) {
                        Set-FirstMilestone $milestones "swap" $elapsed
                    }
                }
            }
        }
        $singleStep = Read-Metric $lastLive 'single_step=([0-9]+)'
        $guestInstructionProxy =
            [long]$singleStep + [long]$span[3]
        $runEepromSha256 =
            (Get-FileHash -LiteralPath $runEeprom -Algorithm SHA256).Hash

        $results.Add([pscustomobject]@{
            run = $runNumber
            pair = [math]::Floor($runIndex / 2) + 1
            backend = $Backend
            span_enabled = $spanEnabled
            span_cache_enabled = $cacheEnabled
            span_reject_cache_enabled = $rejectCacheEnabled
            retired_span_enabled = $retiredSpanEnabled
            span_writes_enabled = $writesEnabled
            span_jumps_enabled = $jumpsEnabled
            post_hle_enabled = $postHleEnabled
            duration_ms = $DurationMilliseconds
            observed_elapsed_ms = $lastElapsed
            startup_attempts = $startupAttempt
            supervisor_exit = $process.ExitCode
            child_exit = $childExit
            window_open_ms = $milestones.window_open
            texture_ms = $milestones.texture
            draw_ms = $milestones.draw
            swap_ms = $milestones.swap
            progress = Read-Metric $lastLive 'progress=([0-9]+)'
            single_step = $singleStep
            guest_instruction_proxy = $guestInstructionProxy
            region_entry = $region[0]
            region_sensitive = $region[1]
            region_return = $region[2]
            region_reject = $region[3]
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
            retired_span_attempt = $retiredSpan[0]
            retired_span_success = $retiredSpan[1]
            post_hle_translation_attempt = $postHle[0]
            post_hle_translation_success = $postHle[1]
            span_last_cancel_code = Read-Metric $lastLive `
                'span_cancel_last=(0x[0-9A-Fa-f]+)'
            span_last_cancel_eip = Read-Metric $lastLive `
                'span_cancel_last=0x[0-9A-Fa-f]+/(0x[0-9A-Fa-f]+)'
            boundary_total = $aot[0]
            reentry_total = $aot[1]
            boundary_return = $boundary[0]
            boundary_indirect = $boundary[1]
            boundary_other = $boundary[4]
            heartbeat = Read-Metric $lastSnapshot 'heartbeat=([0-9]+)'
            fatal_count = Read-Metric $lastSnapshot 'fatal_count/msg=([0-9]+)'
            legacy_fallback = Read-Metric $lastSnapshot `
                'legacy_fallback_count/addr=([0-9]+)'
            eeprom_sha256 = $runEepromSha256
            fixture_sha256 = $fixtureSha256
            eeprom_matches_fixture =
                $runEepromSha256 -eq $fixtureSha256
            supervisor_log = $stdoutLog
            child_log = $stderrLog
            eeprom_copy = $runEeprom
        })
    }
}
finally {
    Pop-Location
    $env:REPIU_EXECUTION_BACKEND = $previousBackend
    $env:REPIU_EXECUTION_TIMEOUT_MS = $previousTimeout
    $env:REPIU_AOT_INDIRECT_CACHE_SLOTS = $previousSlots
    $env:REPIU_NATIVE_REGION = $previousRegion
    $env:REPIU_NATIVE_LINEAR_SPAN = $previousSpan
    $env:REPIU_NATIVE_LINEAR_SPAN_CACHE = $previousSpanCache
    $env:REPIU_NATIVE_LINEAR_SPAN_REJECT_CACHE =
        $previousSpanRejectCache
    $env:REPIU_AOT_RETIRED_SPAN_REENTRY = $previousRetiredSpan
    $env:REPIU_NATIVE_LINEAR_SPAN_WRITES = $previousSpanWrites
    $env:REPIU_NATIVE_LINEAR_SPAN_JUMPS = $previousSpanJumps
    $env:REPIU_AOT_DBT_POST_HLE_TRANSLATE = $previousPostHle
    $env:REPIU_EEPROM_PATH = $previousEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $previousDbtIndirect
    $env:REPIU_AOT_DBT_CALL_TRACE = $previousCallTrace
    $env:REPIU_AOT_DBT_CALL_STEP = $previousCallStep
}

$csvPath = Join-Path $resultRoot "results.csv"
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation
$results | Format-Table run, pair, backend, span_enabled, `
    span_cache_enabled, span_reject_cache_enabled, retired_span_enabled, `
    window_open_ms, `
    texture_ms, swap_ms, `
    span_writes_enabled, guest_instruction_proxy, span_entry, `
    span_jumps_enabled, span_jump_chain, span_backward_jump_stop, `
    post_hle_enabled, post_hle_translation_attempt, `
    post_hle_translation_success, `
    span_cache_hit, span_cache_miss, span_write_cross, `
    span_reject_cache_hit, span_reject_cache_miss, `
    retired_span_attempt, retired_span_success, `
    span_write_guard_uncovered, span_write_fault_cancel, `
    boundary_total, fatal_count, eeprom_matches_fixture -AutoSize
Write-Host "native-span results: $csvPath"
