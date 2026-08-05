param(
    [ValidateRange(1000, 3600000)]
    [int]$DurationMilliseconds = 240000,

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
$resultRoot = Join-Path $repoRoot `
    "build\benchmarks\aot-inline-cache\$timestamp"
New-Item -ItemType Directory -Path $resultRoot -Force | Out-Null

$fixture = Join-Path $repoRoot "eeprom.dat"
$sequence = New-Object System.Collections.Generic.List[int]
for ($index = 0; $index -lt $Repetitions; ++$index) {
    if (($index % 2) -eq 0) {
        $sequence.Add(1)
        $sequence.Add(4)
    }
    else {
        $sequence.Add(4)
        $sequence.Add(1)
    }
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

$previousBackend = $env:REPIU_EXECUTION_BACKEND
$previousTimeout = $env:REPIU_EXECUTION_TIMEOUT_MS
$previousSlots = $env:REPIU_AOT_INDIRECT_CACHE_SLOTS
$previousEeprom = $env:REPIU_EEPROM_PATH
$results = New-Object System.Collections.Generic.List[object]

Push-Location $repoRoot
try {
    for ($runIndex = 0; $runIndex -lt $sequence.Count; ++$runIndex) {
        $slots = $sequence[$runIndex]
        $runNumber = $runIndex + 1
        $runName = "run-{0:D2}-slots-{1}" -f $runNumber, $slots
        $stdoutLog = Join-Path $resultRoot "$runName-stdout.log"
        $stderrLog = Join-Path $resultRoot "$runName-stderr.log"
        $runEeprom = Join-Path $resultRoot "$runName-eeprom.dat"
        if (Test-Path -LiteralPath $fixture -PathType Leaf) {
            Copy-Item -LiteralPath $fixture -Destination $runEeprom
        }

        $env:REPIU_EXECUTION_BACKEND = "dynamic"
        $env:REPIU_EXECUTION_TIMEOUT_MS = "0"
        $env:REPIU_AOT_INDIRECT_CACHE_SLOTS = "$slots"
        $env:REPIU_EEPROM_PATH = $runEeprom

        $milestones = @{
            window_gate = $null
            window_open = $null
            texture = $null
            draw = $null
            swap = $null
        }
        $currentElapsed = 0
        $lastSnapshot = ""
        $childExit = $null

        Write-Host ("A/B run {0}/{1}: slots={2}, duration_ms={3}" -f `
            $runNumber, $sequence.Count, $slots, $DurationMilliseconds)
        $process = Start-Process -FilePath $supervisor `
            -ArgumentList @("pumpit1", "$DurationMilliseconds") `
            -RedirectStandardOutput $stdoutLog `
            -RedirectStandardError $stderrLog `
            -WindowStyle Hidden -Wait -PassThru
        $supervisorExit = $process.ExitCode

        foreach ($line in Get-Content -LiteralPath $stdoutLog) {
            if ($line -match '^\[repiu-supervisor\] elapsed_ms=([0-9]+)') {
                $currentElapsed = [int]$matches[1]
                $lastSnapshot = $line
                if ($line -match `
                    'glide_milestone\(gate/open/tex/draw/swap\)=([0-9/]+)') {
                    $milestoneParts = $matches[1].Split('/')
                    if ($milestoneParts.Count -eq 5) {
                        if ([int]$milestoneParts[0] -ne 0) {
                            Set-FirstMilestone $milestones "window_gate" `
                                $currentElapsed
                        }
                        if ([int]$milestoneParts[1] -ne 0) {
                            Set-FirstMilestone $milestones "window_open" `
                                $currentElapsed
                        }
                        if ([int]$milestoneParts[2] -ne 0) {
                            Set-FirstMilestone $milestones "texture" `
                                $currentElapsed
                        }
                        if ([int]$milestoneParts[3] -ne 0) {
                            Set-FirstMilestone $milestones "draw" `
                                $currentElapsed
                        }
                        if ([int]$milestoneParts[4] -ne 0) {
                            Set-FirstMilestone $milestones "swap" `
                                $currentElapsed
                        }
                    }
                }
            }
            if ($line -match '\[repiu-supervisor\] child_exit=([0-9]+)') {
                $childExit = [int]$matches[1]
            }
        }

        $boundary = Read-Metric $lastSnapshot `
            'boundary_reason\(ret/indir/direct/cond/other\)=([0-9/]+)'
        $boundaryValues = @(0, 0, 0, 0, 0)
        if ($null -ne $boundary) {
            $parts = $boundary.Split('/')
            if ($parts.Count -eq 5) {
                for ($partIndex = 0; $partIndex -lt 5; ++$partIndex) {
                    $boundaryValues[$partIndex] = [long]$parts[$partIndex]
                }
            }
        }
        $boundaryReentry = Read-Metric $lastSnapshot `
            'aot_boundary/reentry=([0-9/]+)'
        $boundaryTotal = 0
        $reentryTotal = 0
        if ($null -ne $boundaryReentry) {
            $parts = $boundaryReentry.Split('/')
            if ($parts.Count -eq 2) {
                $boundaryTotal = [long]$parts[0]
                $reentryTotal = [long]$parts[1]
            }
        }

        $results.Add([pscustomobject]@{
            run = $runNumber
            slots = $slots
            duration_ms = $DurationMilliseconds
            supervisor_exit = $supervisorExit
            child_exit = $childExit
            window_gate_ms = $milestones.window_gate
            window_open_ms = $milestones.window_open
            texture_ms = $milestones.texture
            draw_ms = $milestones.draw
            swap_ms = $milestones.swap
            boundary_total = $boundaryTotal
            reentry_total = $reentryTotal
            boundary_return = $boundaryValues[0]
            boundary_indirect = $boundaryValues[1]
            boundary_direct = $boundaryValues[2]
            boundary_conditional = $boundaryValues[3]
            boundary_other = $boundaryValues[4]
            heartbeat = Read-Metric $lastSnapshot 'heartbeat=([0-9]+)'
            fatal_count = Read-Metric $lastSnapshot 'fatal_count/msg=([0-9]+)'
            legacy_fallback = Read-Metric $lastSnapshot `
                'legacy_fallback_count/addr=([0-9]+)'
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
    $env:REPIU_EEPROM_PATH = $previousEeprom
}

$csvPath = Join-Path $resultRoot "results.csv"
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation
$results | Format-Table run, slots, window_open_ms, texture_ms, draw_ms, `
    swap_ms, boundary_indirect, boundary_total, fatal_count -AutoSize
Write-Host "A/B results: $csvPath"
