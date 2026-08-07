param(
    [string]$InstallDir = "tools\openwatcom",
    [string]$DownloadDir = "tools\downloads",
    [string]$AssetName = "open-watcom-2_0-c-win-x64.exe",
    # Task 447: a dated snapshot, never `Current-build`. Upstream rebuilds that
    # rolling tag in place, so a hash pinned against it expires without anything
    # here changing: the download cache hides it until the cache is evicted, and
    # then CI fails on a mismatch. Dated tags are not rebuilt, so the pin below
    # stays true. Moving to a newer toolchain is a deliberate edit of these two
    # lines together -- and the sample baseline may need re-recording, because
    # 877 of the archive's 4,038 entries differ between two adjacent snapshots.
    [string]$ReleaseTag = "2026-08-01-Build"
)

$ErrorActionPreference = "Stop"

$assetUrl = "https://github.com/open-watcom/open-watcom-v2/releases/download/$ReleaseTag/$AssetName"
$expectedSha256 = "089b9693df89d52b793156afa102ffd93e7cf75dd6377badafae6391b59df2c6"

New-Item -ItemType Directory -Force $DownloadDir | Out-Null
New-Item -ItemType Directory -Force $InstallDir | Out-Null

# The cached download carries its release tag in the name. Two snapshots share
# one asset name upstream, so a single cache path would hand the previous
# toolchain's file to the hash check and fail on it.
$assetLeaf = [System.IO.Path]::GetFileNameWithoutExtension($AssetName)
$assetExtension = [System.IO.Path]::GetExtension($AssetName)
$assetPath = Join-Path $DownloadDir "$assetLeaf-$ReleaseTag$assetExtension"
if (!(Test-Path $assetPath)) {
    Write-Host "Downloading $AssetName from $ReleaseTag"
    Invoke-WebRequest -Uri $assetUrl -OutFile $assetPath
}

$actualSha256 = (Get-FileHash $assetPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    # Say what to do, not just that it broke. Reaching here means either the
    # local download is damaged or the tag's asset was replaced upstream; delete
    # $assetPath and retry to tell them apart.
    throw @"
OpenWatcom installer hash mismatch for release tag '$ReleaseTag'.
  expected : $expectedSha256
  actual   : $actualSha256
  file     : $assetPath
Delete that file and rerun. If the hash is still different, upstream replaced
the asset: verify the new download and update `$expectedSha256 in this script.
"@
}

$marker = Join-Path $InstallDir "binnt\wcl386.exe"
# Which snapshot the install directory holds. Without it, an existing install
# from another tag satisfies the marker and the script reports success while
# leaving the old compiler in place -- a local toolchain silently diverging from
# CI's, which is the same class of stale-artifact bug this task is fixing.
$stamp = Join-Path $InstallDir ".openwatcom-release"
$installedTag = if (Test-Path $stamp) { (Get-Content $stamp -Raw).Trim() } else { $null }
if ((Test-Path $marker) -and $installedTag -eq $ReleaseTag) {
    Write-Host "OpenWatcom $ReleaseTag already installed at $InstallDir"
    exit 0
}
if (Test-Path $marker) {
    $describe = if ($installedTag) { $installedTag } else { "an unstamped build" }
    Write-Host "Replacing OpenWatcom $describe with $ReleaseTag at $InstallDir"
    Remove-Item -Recurse -Force $InstallDir -Confirm:$false
    New-Item -ItemType Directory -Force $InstallDir | Out-Null
}

Write-Host "Extracting OpenWatcom to $InstallDir"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory(
    (Resolve-Path $assetPath),
    (Resolve-Path $InstallDir))

if (!(Test-Path $marker)) {
    throw "OpenWatcom extraction did not produce $marker"
}

Set-Content -Path $stamp -Value $ReleaseTag -Encoding utf8
Write-Host "OpenWatcom $ReleaseTag installed at $InstallDir"
