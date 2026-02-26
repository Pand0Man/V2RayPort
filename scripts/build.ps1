param(
    [string]$BuildDir = ".build",
    [string]$Config = "Release",
    [string]$Generator = "auto",
    [string]$Arch = "x64",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [string]$Exe,
        [string[]]$Args
    )

    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $Exe $($Args -join ' ')"
    }
}

function Resolve-CMakePath {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $bundledCmake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $bundledCmake) {
                return $bundledCmake
            }
        }
    }

    return $null
}

function Resolve-VisualStudioPath {
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        return $null
    }

    $vsPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($vsPath -and (Test-Path $vsPath)) {
        return $vsPath
    }

    return $null
}

function Select-Generator {
    param([string]$Requested)

    if ($Requested -ne "auto") {
        return @{ Name = $Requested; MultiConfig = ($Requested -like "Visual Studio*") }
    }

    $vsPath = Resolve-VisualStudioPath
    if ($vsPath) {
        return @{ Name = "Visual Studio 17 2022"; MultiConfig = $true }
    }

    $ninja = Get-Command ninja -ErrorAction SilentlyContinue
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    $clang = Get-Command clang++.exe -ErrorAction SilentlyContinue
    $gxx = Get-Command g++.exe -ErrorAction SilentlyContinue

    if ($ninja -and ($cl -or $clang -or $gxx)) {
        return @{ Name = "Ninja"; MultiConfig = $false }
    }

    return $null
}


if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "ERROR: CMakeLists.txt не найден. Запускай скрипт из корня репозитория V2RayPort." -ForegroundColor Red
    exit 1
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Очистка старой папки сборки: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

$cmakePath = Resolve-CMakePath
if (-not $cmakePath) {
    Write-Host "ERROR: cmake не найден в PATH и не найден внутри Visual Studio." -ForegroundColor Red
    Write-Host "Установи CMake и перезапусти PowerShell:" -ForegroundColor Yellow
    Write-Host "  winget install Kitware.CMake"
    exit 1
}

$selected = Select-Generator -Requested $Generator
if (-not $selected) {
    Write-Host "ERROR: Не найден рабочий генератор CMake." -ForegroundColor Red
    Write-Host "Вариант 1 (рекомендуется): установи Visual Studio Build Tools 2022 + workload C++." -ForegroundColor Yellow
    Write-Host "Вариант 2: установи Ninja + компилятор (clang++/g++/cl)." -ForegroundColor Yellow
    exit 1
}

$generatorName = $selected.Name
$isMultiConfig = $selected.MultiConfig

Write-Host "Используется CMake: $cmakePath" -ForegroundColor Cyan
Write-Host "Генератор: $generatorName" -ForegroundColor Cyan

$configureArgs = @('-S', '.', '-B', $BuildDir, '-G', $generatorName)
if ($generatorName -like 'Visual Studio*') {
    $configureArgs += @('-A', $Arch)
}
if (-not $isMultiConfig) {
    $configureArgs += @("-DCMAKE_BUILD_TYPE=$Config")
}

Invoke-Checked -Exe $cmakePath -Args $configureArgs

$buildArgs = @('--build', $BuildDir)
if ($isMultiConfig) {
    $buildArgs += @('--config', $Config)
}
Invoke-Checked -Exe $cmakePath -Args $buildArgs

if ($isMultiConfig) {
    $exePath = Join-Path $BuildDir "$Config\V2RayPort.exe"
}
else {
    $exePath = Join-Path $BuildDir "V2RayPort.exe"
}

Write-Host "Сборка успешна: $exePath" -ForegroundColor Green
