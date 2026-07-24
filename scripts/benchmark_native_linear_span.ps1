param(
    [ValidateSet("aot-dynamic", "aot-dbt")]
    [string]$Backend = "aot-dynamic",

    [ValidateRange(1000, 3600000)]
    [int]$DurationMilliseconds = 60000,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 1
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$supervisor = Join-Path $repoRoot `
    "build\win32_x86_debug\Debug\repiu_supervisor_win32.exe"
if (-not (Test-Path -LiteralPath $supervisor -PathType Leaf)) {
    throw "Win32 supervisor was not found: $supervisor"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$benchmarkRoot = Join-Path $repoRoot `
    "build\benchmarks\native-linear-span\$Backend"
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
$previousEeprom = $env:REPIU_EEPROM_PATH
$previousDbtIndirect = $env:REPIU_AOT_DBT_INDIRECT
$previousCallTrace = $env:REPIU_AOT_DBT_CALL_TRACE
$previousCallStep = $env:REPIU_AOT_DBT_CALL_STEP
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

        Write-Host (`
            "native-span run {0}/{1}: backend={2}, span={3}, duration_ms={4}" -f `
            $runNumber, $sequence.Count, $Backend, $mode, `
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

        $region = Read-SlashValues $lastLive `
            'region=([0-9/]+)' 5
        $span = Read-SlashValues $lastLive `
            'span=([0-9/]+)' 5
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
            duration_ms = $DurationMilliseconds
            supervisor_exit = $process.ExitCode
            child_exit = Read-Metric $childExitLine 'child_exit=([0-9]+)'
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
    $env:REPIU_EEPROM_PATH = $previousEeprom
    $env:REPIU_AOT_DBT_INDIRECT = $previousDbtIndirect
    $env:REPIU_AOT_DBT_CALL_TRACE = $previousCallTrace
    $env:REPIU_AOT_DBT_CALL_STEP = $previousCallStep
}

$csvPath = Join-Path $resultRoot "results.csv"
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation
$results | Format-Table run, pair, backend, span_enabled, window_open_ms, `
    texture_ms, swap_ms, guest_instruction_proxy, span_entry, `
    boundary_total, fatal_count, eeprom_matches_fixture -AutoSize
Write-Host "native-span results: $csvPath"
