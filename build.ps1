# 一键构建 C++ 游戏、编辑器和示例模组；所有产物位于仓库的 build/。
[CmdletBinding()]
param([switch]$SkipRun,[switch]$DebugBuild,[switch]$Clean,[string]$Generator='Visual Studio 17 2022')
$ErrorActionPreference='Stop'
$ProjectRoot=$PSScriptRoot
$Build=Join-Path $ProjectRoot 'build'
$BuildType=if($DebugBuild){'Debug'}else{'Release'}
$CMake=(Get-Command cmake -ErrorAction SilentlyContinue).Source
if(-not $CMake){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(Test-Path -LiteralPath $vswhere){$vsPath=& $vswhere -latest -products '*' -property installationPath; if($vsPath){$candidate=Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe';if(Test-Path -LiteralPath $candidate){$CMake=$candidate}}}
}
if(-not $CMake){Write-Error '请安装 Visual Studio 2022 的 C++ 桌面开发与 CMake 工具。';exit 2}
if($Clean -and (Test-Path -LiteralPath $Build)){
    $resolved=[IO.Path]::GetFullPath($Build)
    $expected=[IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build'))
    if($resolved -ne $expected){throw '构建目录不在预期位置'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
& $CMake -S $ProjectRoot -B $Build -G $Generator -A x64
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
& $CMake --build $Build --config $BuildType --parallel
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
Write-Host "构建成功：$Build\bin\orbduel.exe"
if(-not $SkipRun){& (Join-Path $Build 'bin\orbduel.exe') --selftest;exit $LASTEXITCODE}
