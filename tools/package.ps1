# 打包可运行目录。BuildDirectory 可以指定已验证的构建目录。
[CmdletBinding()]
param([string]$BuildDirectory='build',[string]$OutputDirectory='dist\OrbDuel')
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
$Bin=Join-Path (Join-Path $ProjectRoot $BuildDirectory) 'bin'
$Dest=Join-Path $ProjectRoot $OutputDirectory
if(-not (Test-Path -LiteralPath (Join-Path $Bin 'orbduel.exe'))){throw '请先构建游戏'}
if(Test-Path -LiteralPath $Dest){throw '输出目录已存在，请另选一个新的 OutputDirectory；不会把旧包、存档或缓存混入新发布包。'}
New-Item -ItemType Directory -Path $Dest -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $Bin 'orbduel.exe') -Destination $Dest -Force
Copy-Item -Path (Join-Path $Bin '*.dll') -Destination $Dest -Force
foreach($folder in @('content','docs')){Copy-Item -LiteralPath (Join-Path $ProjectRoot $folder) -Destination $Dest -Recurse -Force}
# SDK 只打包公开源文件，不收模板里可能产生的安装位置、构建缓存或日志。
$sdkOut=Join-Path $Dest 'mod-sdk'
$templateOut=Join-Path $sdkOut 'template'
New-Item -ItemType Directory -Path (Join-Path $sdkOut 'include'),$templateOut -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $sdkOut 'examples') -Force | Out-Null
foreach($file in @('portal.cpp','gravity.cpp','launch.cpp','lava.cpp')){
    Copy-Item -LiteralPath (Join-Path (Join-Path $ProjectRoot 'mod-sdk\examples') $file) -Destination (Join-Path $sdkOut 'examples') -Force
}
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'mod-sdk\include\orbduel_mod.hpp') -Destination (Join-Path $sdkOut 'include') -Force
foreach($file in @('new-mod.ps1','LICENSE.txt')){Copy-Item -LiteralPath (Join-Path (Join-Path $ProjectRoot 'mod-sdk') $file) -Destination $sdkOut -Force}
foreach($file in @('CMakeLists.txt','mod.json','my_mod.cpp','mod_config.hpp.in','build-install.ps1','build-install.cmd','README.md','.gitignore')){
    Copy-Item -LiteralPath (Join-Path (Join-Path $ProjectRoot 'mod-sdk\template') $file) -Destination $templateOut -Force
}
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'README.md'),(Join-Path $ProjectRoot 'LICENSE') -Destination $Dest -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\QUICK_START.md') -Destination (Join-Path $Dest '开始使用.md') -Force
$modsOut=Join-Path $Dest 'mods'
New-Item -ItemType Directory -Path $modsOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'mods') -Directory | Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'mod.json')} | ForEach-Object {Copy-Item -LiteralPath (Join-Path (Join-Path $Bin 'mods') $_.Name) -Destination $modsOut -Recurse -Force}
$licenses=Join-Path $Dest 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $licenses 'Dear-ImGui-MIT.txt') -Force
Copy-Item -Path (Join-Path $ProjectRoot 'third_party\sdl2\msvc\LICENSE*') -Destination $licenses -Force
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(Test-Path -LiteralPath $vswhere){$vsPath=& $vswhere -latest -products '*' -property installationPath
    $redist=Join-Path $vsPath 'VC\Redist\MSVC'
    if(Test-Path -LiteralPath $redist){$version=Get-ChildItem -LiteralPath $redist -Directory | Where-Object Name -Match '^\d' | Sort-Object Name -Descending | Select-Object -First 1
        if($version){$crt=Join-Path $version.FullName 'x64\Microsoft.VC143.CRT';if(Test-Path -LiteralPath $crt){Copy-Item -Path (Join-Path $crt '*.dll') -Destination $Dest -Force}}}
}
Write-Output "可运行目录：$Dest"
