param(
    [string]$OutputDir = "runtime",
    [string]$Arch = "64"
)

$ErrorActionPreference = "Stop"

function Require-Command($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Required command '$name' not found in PATH"
    }
}

Require-Command "Expand-Archive"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$absOut = (Resolve-Path $OutputDir).Path

Write-Host "[1/4] Resolve latest Xray-core release..."
$xrayRelease = Invoke-RestMethod "https://api.github.com/repos/XTLS/Xray-core/releases/latest"
$xrayAssetName = "Xray-windows-$Arch.zip"
$xrayAsset = $xrayRelease.assets | Where-Object { $_.name -eq $xrayAssetName } | Select-Object -First 1
if (-not $xrayAsset) {
    throw "Asset '$xrayAssetName' not found in latest Xray-core release"
}

$xrayZip = Join-Path $absOut $xrayAsset.name
$xrayExtract = Join-Path $absOut "xray-extracted"

Write-Host "[2/4] Download Xray-core: $($xrayAsset.browser_download_url)"
Invoke-WebRequest -Uri $xrayAsset.browser_download_url -OutFile $xrayZip

if (Test-Path $xrayExtract) { Remove-Item -Recurse -Force $xrayExtract }
Expand-Archive -Path $xrayZip -DestinationPath $xrayExtract -Force

$xrayExe = Get-ChildItem -Path $xrayExtract -Recurse -Filter "xray.exe" | Select-Object -First 1
if (-not $xrayExe) {
    throw "xray.exe not found after extracting Xray archive"
}
Copy-Item -Force $xrayExe.FullName (Join-Path $absOut "xray.exe")

Write-Host "[3/4] Download wintun package from official source..."
$wintunZip = Join-Path $absOut "wintun.zip"
$wintunExtract = Join-Path $absOut "wintun-extracted"
Invoke-WebRequest -Uri "https://www.wintun.net/builds/wintun-0.14.1.zip" -OutFile $wintunZip

if (Test-Path $wintunExtract) { Remove-Item -Recurse -Force $wintunExtract }
Expand-Archive -Path $wintunZip -DestinationPath $wintunExtract -Force

$wintunDll = Get-ChildItem -Path $wintunExtract -Recurse -Filter "wintun.dll" |
    Where-Object { $_.FullName -match "amd64|x64" } |
    Select-Object -First 1
if (-not $wintunDll) {
    $wintunDll = Get-ChildItem -Path $wintunExtract -Recurse -Filter "wintun.dll" | Select-Object -First 1
}
if (-not $wintunDll) {
    throw "wintun.dll not found after extracting wintun archive"
}
Copy-Item -Force $wintunDll.FullName (Join-Path $absOut "wintun.dll")

Write-Host "[4/4] Done"
Write-Host "Saved runtime files:"
Write-Host " - $(Join-Path $absOut 'xray.exe')"
Write-Host " - $(Join-Path $absOut 'wintun.dll')"
