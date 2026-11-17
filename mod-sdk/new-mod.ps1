# 复制独立模组工程，并自动带上 SDK；不覆盖已有工程。
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-z][a-z0-9_-]*$')][string]$Id,
    [string]$Name='我的旋风',
    [string]$Destination
)
$ErrorActionPreference='Stop'
if(-not $Destination){$Destination=Join-Path (Split-Path -Parent $PSScriptRoot) ('user\mod-projects\'+$Id)}
$Destination=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $Destination){throw '目标工程已经存在，请使用另一个 Id 或 Destination；不会覆盖已有文件。'}
$template=Join-Path $PSScriptRoot 'template'
$sdkHeader=Join-Path $PSScriptRoot 'include\orbduel_mod.hpp'
if(-not (Test-Path -LiteralPath $sdkHeader)){throw '未找到 SDK 头文件，请保留 mod-sdk 的完整目录。'}
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
foreach($file in @('CMakeLists.txt','mod.json','my_mod.cpp','mod_config.hpp.in','build-install.ps1','build-install.cmd','README.md','.gitignore')){
    Copy-Item -LiteralPath (Join-Path $template $file) -Destination $Destination
}
$include=Join-Path $Destination 'include'
New-Item -ItemType Directory -Path $include | Out-Null
Copy-Item -LiteralPath $sdkHeader -Destination $include
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE.txt') -Destination (Join-Path $Destination 'SDK_LICENSE.txt')
$manifestPath=Join-Path $Destination 'mod.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$manifest.id=$Id
$manifest.name=$Name
$manifest.typeName=$Name
$manifest.library=$Id+'.dll'
[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 10)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host '工程已复制，SDK 已包含。修改 mod.json 后，双击 build-install.cmd 即可构建安装。'
Write-Output $Destination
