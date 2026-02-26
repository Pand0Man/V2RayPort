param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

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

$cmakePath = Resolve-CMakePath
if (-not $cmakePath) {
    Write-Host "ERROR: cmake не найден в PATH и не найден внутри Visual Studio." -ForegroundColor Red
    Write-Host "\nУстанови CMake одним из способов:" -ForegroundColor Yellow
    Write-Host "  winget install Kitware.CMake"
    Write-Host "или установи Visual Studio 2022 Build Tools с workload 'Desktop development with C++'."
    Write-Host "\nПосле установки перезапусти PowerShell и запусти скрипт снова:" -ForegroundColor Yellow
    Write-Host "  powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1"
    exit 1
}


$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    Write-Host "WARNING: cl.exe не найден в текущем shell." -ForegroundColor Yellow
    Write-Host "Открой 'x64 Native Tools Command Prompt for VS 2022' или 'Developer PowerShell for VS 2022'." -ForegroundColor Yellow
}

Write-Host "Используется CMake: $cmakePath" -ForegroundColor Cyan
& $cmakePath -S . -B $BuildDir -G $Generator -A $Arch
& $cmakePath --build $BuildDir --config $Config

Write-Host "Сборка завершена: $BuildDir\\$Config\\V2RayPort.exe" -ForegroundColor Green
