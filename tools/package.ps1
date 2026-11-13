# 打包可运行目录。BuildDirectory 可以指定已验证的构建目录。
[CmdletBinding()]
param([string]$BuildDirectory='build',[string]$OutputDirectory='dist\OrbDuel')
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
$Bin=Join-Path (Join-Path $ProjectRoot $BuildDirectory) 'bin'
$Dest=Join-Path $ProjectRoot $OutputDirectory
if(-not (Test-Path -LiteralPath (Join-Path $Bin 'orbduel.exe'))){throw '请先构建游戏'}
New-Item -ItemType Directory -Path $Dest -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $Bin 'orbduel.exe') -Destination $Dest -Force
Copy-Item -Path (Join-Path $Bin '*.dll') -Destination $Dest -Force
foreach($folder in @('content','mod-sdk','docs')){Copy-Item -LiteralPath (Join-Path $ProjectRoot $folder) -Destination $Dest -Recurse -Force}
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'README.md'),(Join-Path $ProjectRoot 'LICENSE') -Destination $Dest -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\QUICK_START.md') -Destination (Join-Path $Dest '开始使用.md') -Force
$modsOut=Join-Path $Dest 'mods'
New-Item -ItemType Directory -Path $modsOut -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'mods') -Directory | ForEach-Object {Copy-Item -LiteralPath (Join-Path (Join-Path $Bin 'mods') $_.Name) -Destination $modsOut -Recurse -Force}
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
