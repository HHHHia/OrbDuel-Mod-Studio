# 一键配置 + 构建 + 冒烟测试
#
#   .\build.ps1              释放版构建，然后跑 --selftest
#   .\build.ps1 -SkipRun     只构建
#   .\build.ps1 -DebugBuild  Debug 构建
#   .\build.ps1 -Clean       删掉 build/ 重新配置（会先确认）
#
# 依赖：cmake。编译器用 PATH 里的 g++（w64devkit）或 MSVC。
# SDL2 已离线在 third_party/，不需要联网。

[CmdletBinding()]
param(
    [switch]$SkipRun,
    [switch]$DebugBuild,
    [switch]$Clean,
    [string]$Generator = ''
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Build = Join-Path $Root 'build'
$BuildType = if ($DebugBuild) { 'Debug' } else { 'Release' }

function Find-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

$CMake = Find-Tool 'cmake'
if (-not $CMake) {
    Write-Host 'cmake 不在 PATH。' -ForegroundColor Red
    Write-Host 'cmake 不在 PATH。装 CMake，或把 w64devkit 的 bin 目录加进 PATH。'
    exit 2
}

if ($Clean -and (Test-Path $Build)) {
    Write-Host "即将删除构建目录: $Build" -ForegroundColor Yellow
    $answer = Read-Host '确认删除？(y/N)'
    if ($answer -ne 'y') { Write-Host '已取消'; exit 1 }
    Remove-Item -Recurse -Force $Build
}

if (-not $Generator) {
    if (Find-Tool 'ninja') { $Generator = 'Ninja' }
}

$cache = Join-Path $Build 'CMakeCache.txt'
if (-not (Test-Path $cache)) {
    Write-Host "配置构建目录: $Build" -ForegroundColor Cyan
    $cmakeArgs = @('-S', $Root, '-B', $Build, "-DCMAKE_BUILD_TYPE=$BuildType")
    if ($Generator) { $cmakeArgs += @('-G', $Generator) }
    & $CMake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { Write-Host 'cmake configure 失败' -ForegroundColor Red; exit 2 }
}

Write-Host "构建 ($BuildType)" -ForegroundColor Cyan
& $CMake --build $Build --config $BuildType
if ($LASTEXITCODE -ne 0) { Write-Host '构建失败' -ForegroundColor Red; exit 2 }

$Exe = Join-Path $Build 'bin\orbduel.exe'
if (-not (Test-Path $Exe)) {
    Write-Host "找不到产物: $Exe" -ForegroundColor Red
    exit 2
}
Write-Host "构建成功: $Exe" -ForegroundColor Green

if (-not $SkipRun) {
    Write-Host ''
    Push-Location $Root
    try {
        & $Exe --selftest
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    exit $code
}
exit 0
