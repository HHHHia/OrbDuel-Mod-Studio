# 端到端验证复制模板、双击入口、安装、运行时重装、重新加载与失败保留。
[CmdletBinding()]
param([string]$HostExe,[string]$GameBin,[string]$SdkRoot,[string]$ContentRoot,[string]$ScratchRoot)
$ErrorActionPreference='Stop'
$run=Join-Path $ScratchRoot ('run-'+$PID)
$index=0
while(Test-Path -LiteralPath $run){$index++;$run=Join-Path $ScratchRoot ('run-'+$PID+'-'+$index)}
New-Item -ItemType Directory -Path $run -Force | Out-Null
$game=Join-Path $run 'Game [copy]'
New-Item -ItemType Directory -Path $game | Out-Null
Copy-Item -LiteralPath (Join-Path $GameBin 'orbduel.exe') -Destination $game
Get-ChildItem -LiteralPath $GameBin -Filter '*.dll' | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $game}
Copy-Item -LiteralPath $ContentRoot -Destination (Join-Path $game 'content') -Recurse
$project=Join-Path $run '作品 [first project]'
$other=Join-Path $run 'Other project'
$newScript=Join-Path $SdkRoot 'new-mod.ps1'
& $newScript -Id rookie-vortex -Name '逆向旋风' -Destination $project | Out-Null
$assetDir=Join-Path $project 'assets\sample [data]'
New-Item -ItemType Directory -Path $assetDir -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $assetDir 'example.txt'),'public sample',[Text.UTF8Encoding]::new($false))
if(-not (Test-Path -LiteralPath (Join-Path $project 'include\orbduel_mod.hpp'))){throw '复制的工程不独立'}
try { & $newScript -Id rookie-vortex -Destination $project | Out-Null; throw '覆盖检测失效' }
catch { if($_.Exception.Message -eq '覆盖检测失效'){throw} }

function Build-Install([string]$Source,[bool]$First=$false,[bool]$ExpectFailure=$false){
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$env:ComSpec
    $info.WorkingDirectory=$Source
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $info.EnvironmentVariables['ORBDUEL_NO_PAUSE']='1'
    $extra=if($First){' -GameDirectory "'+$game+'"'}else{''}
    $info.Arguments='/d /c build-install.cmd -NonInteractive'+$extra
    $proc=New-Object Diagnostics.Process
    $proc.StartInfo=$info
    [void]$proc.Start()
    $stdout=$proc.StandardOutput.ReadToEndAsync()
    $stderr=$proc.StandardError.ReadToEndAsync()
    $proc.WaitForExit()
    [IO.File]::WriteAllText((Join-Path $Source 'build-test.log'),$stdout.Result+$stderr.Result,[Text.UTF8Encoding]::new($false))
    $failed=$proc.ExitCode -ne 0
    $proc.Dispose()
    if($failed -ne $ExpectFailure){throw '一键入口结果不符，请查看测试工程 build-test.log'}
}
Build-Install $project $true
$summon=Join-Path $run '分身 [summon project]'
& $newScript -Id rookie-summon -Kind summon -Name '新手分身' -Destination $summon | Out-Null
$summonManifest=Join-Path $summon 'mod.json'
$summonJson=Get-Content -LiteralPath $summonManifest -Raw -Encoding UTF8 | ConvertFrom-Json
$summonJson.defaults.speed=345
$summonJson.defaults.maxAlive=1
[IO.File]::WriteAllText($summonManifest,($summonJson|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
Build-Install $summon $true
if(-not (Test-Path -LiteralPath (Join-Path $game 'mods\rookie-vortex\rookie-vortex.dll'))){throw '未安装 DLL'}
if(Test-Path -LiteralPath (Join-Path $game 'mods\rookie-vortex\.orbduel-user.json')){throw '本地安装路径混入分享文件'}
if(-not (Test-Path -LiteralPath (Join-Path $game 'mods\rookie-vortex\SDK_LICENSE.txt'))){throw '遗漏 SDK 许可证'}
$info=New-Object Diagnostics.ProcessStartInfo
$info.FileName=$HostExe
$scene=Join-Path $game 'test-arena.json'
$info.Arguments='"'+$ContentRoot+'" "'+(Join-Path $game 'mods')+'" "'+$scene+'"'
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardInput=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
$live=New-Object Diagnostics.Process
$live.StartInfo=$info
[void]$live.Start()
function Expect-Line([string]$Expected){
    $line=$live.StandardOutput.ReadLineAsync()
    if(-not $line.Wait(20000)){throw ('等待 '+$Expected+' 超时')}
    if($line.Result -ne $Expected){throw ('宿主验证失败：'+$line.Result)}
}
try {
    Expect-Line 'READY'
    $manifest=Join-Path $project 'mod.json'
    $j=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
    $j.defaults.strength=-600
    [IO.File]::WriteAllText($manifest,($j|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    Build-Install $project
    $summonJson.defaults.speed=690
    [IO.File]::WriteAllText($summonManifest,($summonJson|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    Build-Install $summon
    & $newScript -Id other-vortex -Name '另一份旋风' -Destination $other | Out-Null
    Build-Install $other $true
    $live.StandardInput.WriteLine('reload')
    Expect-Line 'RELOADED'
    $j.defaults.strength=-900
    [IO.File]::WriteAllText($manifest,($j|ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
    [IO.File]::AppendAllText((Join-Path $project 'my_mod.cpp'),"`nintentional compile error`n",[Text.UTF8Encoding]::new($false))
    Build-Install $project $false $true
    $installed=Get-Content -LiteralPath (Join-Path $game 'mods\rookie-vortex\mod.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($installed.defaults.strength -ne -600){throw '编译失败替换了已安装配置'}
    $live.StandardInput.WriteLine('verify')
    Expect-Line 'DONE'
    $live.WaitForExit()
    if($live.ExitCode -ne 0){throw '宿主运行失败'}
} finally {
    if(-not $live.HasExited){$live.Kill()}
    $live.Dispose()
}
$leftovers=@(Get-ChildItem -LiteralPath (Join-Path $game 'mods\.orbduel-cache') -Directory)
if($leftovers.Count){throw '正常卸载后遗留运行时 DLL 副本'}
& (Join-Path $game 'orbduel.exe') --headless --content (Join-Path $game 'content') --mods (Join-Path $game 'mods') --scene $scene --seconds 1
if($LASTEXITCODE -ne 0){throw '安装后的真实游戏不能运行模组地图'}
Write-Output 'PASS independent copy, one-click install, live reinstall/reload, coexistence, failed-build retention, saved parameters and real game'
