# 只构建当前模组并安装公开运行文件；不会重新编译游戏。
[CmdletBinding()]
param([string]$GameDirectory,[switch]$NonInteractive)
$ErrorActionPreference='Stop'
$ProjectRoot=$PSScriptRoot
$settingsPath=Join-Path $ProjectRoot '.orbduel-user.json'
$explicitGame=[bool]$GameDirectory
if(-not $GameDirectory -and (Test-Path -LiteralPath $settingsPath)){
    $settings=Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $GameDirectory=$settings.gameDirectory
}
if(-not $explicitGame -and $GameDirectory -and -not (Test-Path -LiteralPath (Join-Path $GameDirectory 'orbduel.exe'))){$GameDirectory=''}
if(-not $GameDirectory){
    if($NonInteractive){throw '请使用 -GameDirectory 指向 orbduel.exe 所在文件夹。'}
    Add-Type -AssemblyName System.Windows.Forms
    $dialog=New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title='选择要安装模组的 OrbDuel 游戏'
    $dialog.Filter='OrbDuel 游戏|orbduel.exe'
    try {
        if($dialog.ShowDialog() -ne [Windows.Forms.DialogResult]::OK){Write-Host '已取消，未安装。';return}
        $GameDirectory=Split-Path -Parent $dialog.FileName
    } finally {$dialog.Dispose()}
}
$GameDirectory=[IO.Path]::GetFullPath($GameDirectory)
if(-not (Test-Path -LiteralPath (Join-Path $GameDirectory 'orbduel.exe'))){throw '所选文件夹没有 orbduel.exe，请选择游戏程序所在文件夹。'}
$manifest=Get-Content -LiteralPath (Join-Path $ProjectRoot 'mod.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($manifest.id -notmatch '^[a-z][a-z0-9_-]*$'){throw 'mod.json 的 id 请使用小写字母开头，后接字母、数字、- 或 _。'}
if($manifest.library -notmatch '^[A-Za-z0-9_.-]+\.dll$'){throw 'library 应是 DLL 文件名，不要填写路径。'}
# 保留已构建工程的生成器。显式设置 CMAKE_GENERATOR 时沿用原有优先级。
$build=Join-Path $ProjectRoot 'build'
$cache=Join-Path $build 'CMakeCache.txt'
$generator=$env:CMAKE_GENERATOR
if(-not $generator -and (Test-Path -LiteralPath $cache)){
    $entry=Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$'
    if($entry){$generator=$entry.Matches[0].Groups[1].Value}
}

function Find-ModToolchain([string]$RequestedGenerator){
    $versions=[ordered]@{
        'Visual Studio 17 2022'='[17.0,18.0)'
        'Visual Studio 18 2026'='[18.0,19.0)'
    }
    if($RequestedGenerator -and -not $versions.Contains($RequestedGenerator)){
        throw "模组使用 Visual Studio 2022 或 2026 x64。请检查 CMAKE_GENERATOR（当前为 $RequestedGenerator）。"
    }
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(-not (Test-Path -LiteralPath $vswhere)){return}
    $pathCmake=(Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1).Source
    $generators=if($RequestedGenerator){$RequestedGenerator}else{$versions.Keys}
    foreach($name in $generators){
        $instances=& $vswhere -products '*' -version $versions[$name] -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        foreach($instance in $instances){
            $bundled=Join-Path $instance 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            foreach($exe in @($bundled,$pathCmake)){
                if(-not $exe -or -not (Test-Path -LiteralPath $exe -PathType Leaf)){continue}
                $capabilities=& $exe -E capabilities
                if($LASTEXITCODE -ne 0){continue}
                if(($capabilities | ConvertFrom-Json).generators.name -contains $name){
                    return [pscustomobject]@{CMake=$exe; Generator=$name; Instance=$instance}
                }
            }
        }
    }
}
<<<<<<< fix/ability-cleanup-mod-installer

$toolchain=Find-ModToolchain $generator
if(-not $toolchain){
    throw '请安装有 C++ 桌面开发与 CMake 的 Visual Studio 2022 / 2026（或 Build Tools）。VS 2026 需要 CMake 4.2 或更新。若此前切换过工具链，请检查 CMAKE_GENERATOR 和该模组工程的 build 目录。'
}
Write-Host ("模组编译工具："+$toolchain.Generator)
$configureArgs=@('-S',$ProjectRoot,'-B',$build,'-G',$toolchain.Generator,'-A','x64')
if(-not (Test-Path -LiteralPath $cache)){
    $configureArgs+=("-DCMAKE_GENERATOR_INSTANCE="+$toolchain.Instance)
}
& $toolchain.CMake @configureArgs
=======
if(-not $cmake){throw '请安装 Visual Studio 2022 / Build Tools，勾选 C++ 桌面开发与 CMake 工具。'}
$build=Join-Path $ProjectRoot 'build'
$generator = if ($env:CMAKE_GENERATOR) { $env:CMAKE_GENERATOR } else { 'Visual Studio 17 2022' }
& $cmake -S $ProjectRoot -B $build -G $generator -A x64
>>>>>>> main
if($LASTEXITCODE -ne 0){throw '模组配置失败，请查看上方提示；已有安装未改动。'}
& $toolchain.CMake --build $build --config Release --parallel
if($LASTEXITCODE -ne 0){throw '模组编译失败，请修正上方错误后重试；已有安装未改动。'}
$package=Join-Path (Join-Path $build 'package') $manifest.id
$library=Join-Path $package $manifest.library
if(-not (Test-Path -LiteralPath $library)){throw '未找到生成的 DLL，请核对 mod.json 的 library。'}
$install=Join-Path (Join-Path $GameDirectory 'mods') $manifest.id
New-Item -ItemType Directory -Path $install -Force | Out-Null
try {
    Copy-Item -LiteralPath $library -Destination (Join-Path $install $manifest.library) -Force
    Copy-Item -LiteralPath (Join-Path $package 'mod.json') -Destination (Join-Path $install 'mod.json') -Force
    Copy-Item -LiteralPath (Join-Path $package 'SDK_LICENSE.txt') -Destination $install -Force
    $assets=Join-Path $package 'assets'
    if(Test-Path -LiteralPath $assets){
        Get-ChildItem -LiteralPath $assets -Recurse -File | ForEach-Object {
            $relative=$_.FullName.Substring($assets.Length).TrimStart('\','/')
            $target=Join-Path (Join-Path $install 'assets') $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
        }
    }
} catch {
    throw '安装失败：请确认游戏目录可写。若旧版游戏锁住 DLL，请先更新游戏，或退出旧版游戏再重试。'
}
# 安装位置仅保存在本地，不放进 mod.json 或可分享的 package。
[IO.File]::WriteAllText($settingsPath,(@{gameDirectory=$GameDirectory}|ConvertTo-Json)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
Write-Host ('安装成功：'+$manifest.name) -ForegroundColor Green
Write-Host '回到游戏的“模组工坊”，点击“重新加载模组和内容”；新放置的对象使用新的默认参数。'
Write-Host ('可分享文件夹：'+$package)
