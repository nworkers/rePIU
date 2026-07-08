param(
    [string]$InstallDir = "tools\openwatcom",
    [string]$DownloadDir = "tools\downloads",
    [string]$AssetName = "open-watcom-2_0-c-win-x64.exe"
)

$ErrorActionPreference = "Stop"

$assetUrl = "https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/$AssetName"
$expectedSha256 = "1433db0241710b0f75b53b78408fc7350c4b4b9025061aa94570c9de86a5cfb1"

New-Item -ItemType Directory -Force $DownloadDir | Out-Null
New-Item -ItemType Directory -Force $InstallDir | Out-Null

$assetPath = Join-Path $DownloadDir $AssetName
if (!(Test-Path $assetPath)) {
    Write-Host "Downloading $AssetName"
    Invoke-WebRequest -Uri $assetUrl -OutFile $assetPath
}

$actualSha256 = (Get-FileHash $assetPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "OpenWatcom installer hash mismatch: $actualSha256"
}

$marker = Join-Path $InstallDir "binnt\wcl386.exe"
if (Test-Path $marker) {
    Write-Host "OpenWatcom already installed at $InstallDir"
    exit 0
}

Write-Host "Extracting OpenWatcom to $InstallDir"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory(
    (Resolve-Path $assetPath),
    (Resolve-Path $InstallDir))

if (!(Test-Path $marker)) {
    throw "OpenWatcom extraction did not produce $marker"
}

Write-Host "OpenWatcom installed at $InstallDir"
