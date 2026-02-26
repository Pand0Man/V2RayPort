param(
    [string]$OutputDir = "build",
    [string]$Arch = "64",
    [switch]$KeepTemp
)

$ErrorActionPreference = "Stop"

function Require-Command($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Required command '$name' not found in PATH"
    }
}

function Remove-IfExists($path) {
    if ($path -and (Test-Path $path)) {
        Remove-Item -Recurse -Force $path
    }
}

function Resolve-OutputDir([string]$requested) {
    if ($requested -and $requested -ne "") {
        return $requested
    }

    if (Test-Path "build/Release") {
        return "build/Release"
    }

    return "build"
}

Require-Command "Expand-Archive"

$resolvedOutputDir = Resolve-OutputDir $OutputDir
New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null
$absOut = (Resolve-Path $resolvedOutputDir).Path

$xrayZip = $null
$xrayExtract = $null
$wintunZip = $null
$wintunExtract = $null

try {
    Write-Host "[1/5] Resolve latest Xray-core release..."
    $xrayRelease = Invoke-RestMethod "https://api.github.com/repos/XTLS/Xray-core/releases/latest"
    $xrayAssetName = "Xray-windows-$Arch.zip"
    $xrayAsset = $xrayRelease.assets | Where-Object { $_.name -eq $xrayAssetName } | Select-Object -First 1
    if (-not $xrayAsset) {
        throw "Asset '$xrayAssetName' not found in latest Xray-core release"
    }

    $xrayZip = Join-Path $absOut $xrayAsset.name
    $xrayExtract = Join-Path $absOut "tmp-xray"

    Write-Host "[2/5] Download Xray-core: $($xrayAsset.browser_download_url)"
    Invoke-WebRequest -Uri $xrayAsset.browser_download_url -OutFile $xrayZip

    Remove-IfExists $xrayExtract
    Expand-Archive -Path $xrayZip -DestinationPath $xrayExtract -Force

    $xrayExe = Get-ChildItem -Path $xrayExtract -Recurse -Filter "xray.exe" | Select-Object -First 1
    if (-not $xrayExe) {
        throw "xray.exe not found after extracting Xray archive"
    }
    Copy-Item -Force $xrayExe.FullName (Join-Path $absOut "xray.exe")

    Write-Host "[3/5] Download wintun package from official source..."
    $wintunZip = Join-Path $absOut "wintun.zip"
    $wintunExtract = Join-Path $absOut "tmp-wintun"
    Invoke-WebRequest -Uri "https://www.wintun.net/builds/wintun-0.14.1.zip" -OutFile $wintunZip

    Remove-IfExists $wintunExtract
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

    Write-Host "[4/5] Runtime files are ready:"
    Write-Host " - $(Join-Path $absOut 'xray.exe')"
    Write-Host " - $(Join-Path $absOut 'wintun.dll')"
}
finally {
    if (-not $KeepTemp) {
        Remove-IfExists $xrayExtract
        Remove-IfExists $wintunExtract
        if ($xrayZip -and (Test-Path $xrayZip)) { Remove-Item -Force $xrayZip }
        if ($wintunZip -and (Test-Path $wintunZip)) { Remove-Item -Force $wintunZip }
        Write-Host "[5/5] Temporary archives and extracted folders removed"
    }
    else {
        Write-Host "[5/5] KeepTemp enabled: temp files preserved"
    }
}
