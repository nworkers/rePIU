# Task 331: the Release entry point for the performance baseline.
#
# Debug stays the correctness configuration (assertions, iterator debugging).
# Release is the only configuration whose timings may be quoted as performance
# evidence, because Task 330 measured a 11.34x Debug factor for plan building
# whose per-stage factors range from 2.67x to 28.7x and therefore invert the
# stage ranking.
#
# The loader lands in build\win32_x86_debug\Release\repiu_loader_win32.exe --
# the tree is multi-config, so the directory name is historical.
param(
    [string[]]$Target = @()
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "build_win32_x86.ps1") -Configuration Release -Target $Target
if ($LASTEXITCODE -ne 0)
{
    throw "Release build failed with exit code $LASTEXITCODE"
}
