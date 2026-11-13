# 内容校验器（纯 PowerShell，不依赖 Python / Node）
#
#   .\tools\validate_content.ps1
#   .\tools\validate_content.ps1 -ContentRoot .\content
#
# 退出码：0 全部通过（可以有 warning），1 有 error，2 路径不对。

[CmdletBinding()]
param([string]$ContentRoot = '')

$ErrorActionPreference = 'Stop'

if (-not $ContentRoot) { $ContentRoot = Join-Path $PSScriptRoot '..\content' }
if (-not (Test-Path $ContentRoot)) {
    Write-Host "[fatal] content root not found: $ContentRoot" -ForegroundColor Red
    exit 2
}
$Root = (Resolve-Path $ContentRoot).Path

$script:Errors = 0
$script:Warns = 0

function Fail([string]$src, [string]$msg) {
    $script:Errors++
    Write-Host ("[error] {0}: {1}" -f $src, $msg) -ForegroundColor Red
}
function Warn([string]$src, [string]$msg) {
    $script:Warns++
    Write-Host ("[warn ] {0}: {1}" -f $src, $msg) -ForegroundColor Yellow
}
function Pass([string]$msg) {
    Write-Host ("[ok   ] {0}" -f $msg) -ForegroundColor Green
}

function Read-Json([string]$path, [string]$src) {
    try {
        return (Get-Content -Raw -Encoding UTF8 $path | ConvertFrom-Json)
    } catch {
        Fail $src ("JSON 解析失败: " + $_.Exception.Message)
        return $null
    }
}

function Prop($obj, [string]$name) {
    if ($null -eq $obj) { return $null }
    $p = $obj.PSObject.Properties[$name]
    if ($null -eq $p) { return $null }
    return $p.Value
}

function ArrOf($obj, [string]$name) {
    $v = Prop $obj $name
    if ($null -eq $v) { return @() }
    return @($v)
}

function Need($obj, [string]$name, [string]$src) {
    $v = Prop $obj $name
    if ($null -eq $v) {
        Fail $src ("缺少必填字段 '{0}'" -f $name)
        return $false
    }
    return $true
}

function InRange($obj, [string]$name, [double]$lo, [double]$hi, [string]$src) {
    $v = Prop $obj $name
    if ($null -eq $v) { return }
    if ([double]$v -lt $lo -or [double]$v -gt $hi) {
        Fail $src ("{0}={1} 超出范围 [{2}, {3}]" -f $name, $v, $lo, $hi)
    }
}

function CheckColor($obj, [string]$name, [string]$src) {
    $v = Prop $obj $name
    if ($null -eq $v) { return }
    if (@($v).Count -ne 4) {
        Fail $src ("{0} 必须是 4 个 0-255 的整数" -f $name)
        return
    }
    foreach ($c in $v) {
        if ([int]$c -lt 0 -or [int]$c -gt 255) {
            Fail $src ("{0} 分量 {1} 超出 0-255" -f $name, $c)
            return
        }
    }
}

function CheckVector($obj, [string]$name, [string]$src) {
    $v = Prop $obj $name
    if ($null -eq $v) { return }
    if (@($v).Count -ne 2) { Fail $src ("{0} 必须是两个数值" -f $name); return }
    foreach ($n in $v) {
        if ($n -isnot [ValueType] -or $n -is [bool]) { Fail $src ("{0} 的分量必须是数值" -f $name); return }
    }
}

# ---------------------------------------------------------------------------
$SkillKinds = @('cooldown', 'on_hit', 'on_hit_by', 'on_low_hp', 'on_wall_bounce', 'manual')
$SkillOps = @('add_speed', 'shield', 'lifesteal', 'slow_field', 'teleport', 'resize', 'bounce_boost', 'heal', 'damage_boost', 'thorns')
$VfxTypes = @('trail', 'burst', 'ring', 'flash', 'screen_shake', 'hit_stop')
$VfxOns = @('cast', 'hit', 'expire')

$SkillDir = Join-Path $Root 'skills'
$CharDir = Join-Path $Root 'characters'
$MapDir = Join-Path $Root 'maps'

$SkillIds = @{}
$SkillCount = 0
$CharCount = 0
$MapCount = 0

# ---- skills --------------------------------------------------------------
if (Test-Path $SkillDir) {
    Get-ChildItem $SkillDir -Filter *.json | Sort-Object Name | ForEach-Object {
        $src = "skills/" + $_.BaseName
        $d = Read-Json $_.FullName $src
        if ($null -eq $d) { return }

        $id = Prop $d 'id'
        if ($null -eq $id) { Fail $src "缺少 id"; return }
        if ($id -ne $_.BaseName) { Fail $src ("id='{0}' 与文件名不一致" -f $id) }
        if ($SkillIds.ContainsKey($id)) { Fail $src ("技能 id 重复: " + $id) }
        $SkillIds[$id] = $true

        if ((Prop $d 'format') -ne 1) { Fail $src "format 必须为 1" }
        if (-not (Need $d 'name' $src)) { }
        $name = Prop $d 'name'
        if ($null -ne $name -and -not (Prop $name 'en')) { Fail $src "name.en 不能为空" }

        $trigger = Prop $d 'trigger'
        if (-not (Need $d 'trigger' $src)) { }
        if ($null -ne $trigger) {
            $kind = Prop $trigger 'kind'
            if ($SkillKinds -notcontains $kind) { Fail $src ("trigger.kind='{0}' 不合法" -f $kind) }
            InRange $trigger 'cooldown' 0.5 60 $src
            InRange $trigger 'hpBelow' 0.0 1.0 $src
        }

        if (-not (Need $d 'modifiers' $src)) { }
        $mods = ArrOf $d 'modifiers'
        if ($mods.Count -eq 0) { Fail $src "modifiers 不能为空" }
        foreach ($m in $mods) {
            if ($null -eq $m) { continue }
            $op = Prop $m 'op'
            if ($SkillOps -notcontains $op) { Fail $src ("modifier.op='{0}' 不合法" -f $op) }
            InRange $m 'duration' 0.0 120 $src
            InRange $m 'factor' 0.0 1.0 $src
            InRange $m 'value' -100000 100000 $src
        }

        $vfxs = ArrOf $d 'vfx'
        foreach ($v in $vfxs) {
            if ($null -eq $v) { continue }
            if ($VfxTypes -notcontains (Prop $v 'type')) { Fail $src ("vfx.type='{0}' 不合法" -f (Prop $v 'type')) }
            if ($VfxOns -notcontains (Prop $v 'on')) { Fail $src ("vfx.on='{0}' 不合法" -f (Prop $v 'on')) }
            CheckColor $v 'color' $src
        }

        $SkillCount++
    }
} else {
    Fail 'skills/' '目录不存在'
}

# ---- characters ----------------------------------------------------------
if (Test-Path $CharDir) {
    Get-ChildItem $CharDir -Directory | Sort-Object Name | ForEach-Object {
        $dirName = $_.Name
        $src = "characters/" + $dirName
        $jsonPath = Join-Path $_.FullName 'character.json'
        if (-not (Test-Path $jsonPath)) { Fail $src "缺少 character.json"; return }

        $d = Read-Json $jsonPath $src
        if ($null -eq $d) { return }

        $id = Prop $d 'id'
        if ($null -eq $id) { Fail $src "缺少 id" }
        elseif ($id -ne $dirName) { Fail $src ("id='{0}' 与目录名不一致" -f $id) }

        if ((Prop $d 'format') -ne 1) { Fail $src "format 必须为 1" }

        $skin = Prop $d 'skin'
        if (-not (Need $d 'skin' $src)) { } 
        if ($null -ne $skin) {
            $tex = Prop $skin 'texture'
            if (-not $tex) { Fail $src "skin.texture 不能为空" }
            elseif (-not (Test-Path (Join-Path $_.FullName $tex))) { Fail $src ("贴图不存在: " + $tex) }
            InRange $skin 'scale' 0.1 8 $src
            CheckColor $skin 'tint' $src
        }

        $hitbox = Prop $d 'hitbox'
        if (-not (Need $d 'hitbox' $src)) { }
        if ($null -ne $hitbox) {
            CheckVector $hitbox 'offset' $src
            if ((Prop $hitbox 'shape') -ne 'circle') { Fail $src "hitbox.shape 目前只支持 'circle'" }
            InRange $hitbox 'radius' 6 120 $src
        }

        $stats = Prop $d 'stats'
        if (-not (Need $d 'stats' $src)) { }
        if ($null -ne $stats) {
            InRange $stats 'mass' 0.2 20 $src
            InRange $stats 'restitution' 0.0 1.0 $src
            InRange $stats 'maxHp' 10 10000 $src
            InRange $stats 'startSpeed' 0 1800 $src
        }

        if (-not (Need $d 'skills' $src)) { }
        $skills = ArrOf $d 'skills'
        if ($skills.Count -gt 2) { Fail $src ("skills 最多 2 个，当前 " + $skills.Count) }
        foreach ($s in $skills) {
            if (-not $SkillIds.ContainsKey($s)) { Fail $src ("引用了不存在的技能: " + $s) }
        }

        $CharCount++
    }
} else {
    Fail 'characters/' '目录不存在'
}

# ---- maps ----------------------------------------------------------------
if (Test-Path $MapDir) {
    Get-ChildItem $MapDir -Directory | Sort-Object Name | ForEach-Object {
        $dirName = $_.Name
        $src = "maps/" + $dirName
        $jsonPath = Join-Path $_.FullName 'map.json'
        if (-not (Test-Path $jsonPath)) { Fail $src "缺少 map.json"; return }

        $d = Read-Json $jsonPath $src
        if ($null -eq $d) { return }

        if ((Prop $d 'id') -ne $dirName) { Fail $src "id 与目录名不一致" }
        if ((Prop $d 'format') -ne 1) { Fail $src "format 必须为 1" }

        $size = ArrOf $d 'size'
        if ($size.Count -ne 2) { Fail $src "size 必须是 [w, h]" }
        else {
            if ([double]$size[0] -lt 640 -or [double]$size[0] -gt 3840) { Fail $src ("size[0]={0} 超出 640-3840" -f $size[0]) }
            if ([double]$size[1] -lt 480 -or [double]$size[1] -gt 2160) { Fail $src ("size[1]={0} 超出 480-2160" -f $size[1]) }
        }

        $spawns = ArrOf $d 'spawns'
        if ($spawns.Count -ne 2) { Fail $src ("spawns 必须恰好 2 个，当前 " + $spawns.Count) }
        foreach ($s in $spawns) {
            if ($null -eq $s) { continue }
            CheckVector $s 'pos' $src
            CheckVector $s 'dir' $src
            $dir = ArrOf $s 'dir'
            if ($dir.Count -ne 2 -or ([double]$dir[0] -eq 0 -and [double]$dir[1] -eq 0)) {
                Fail $src "spawns[].dir 必须是非零二维向量"
            }
        }

        $phys = Prop $d 'physics'
        if ($null -ne $phys) {
            InRange $phys 'wallRestitution' 0.0 1.0 $src
            InRange $phys 'bounceJitterDeg' 0.0 5.0 $src
            InRange $phys 'maxSpeed' 100 5000 $src
            InRange $phys 'damageScale' 0.1 10 $src
        }

        $shrink = Prop $d 'shrink'
        if ($null -ne $shrink) {
            InRange $shrink 'firstAt' 5 300 $src
            InRange $shrink 'interval' 3 120 $src
            InRange $shrink 'step' 0.01 0.2 $src
            InRange $shrink 'minInset' 0.0 0.9 $src
            InRange $shrink 'damagePerSec' 0.0 50 $src
            InRange $shrink 'lullTrigger' 0.0 60 $src
        }

        $obstacles = ArrOf $d 'obstacles'
        if ($obstacles.Count -gt 12) { Fail $src "obstacles 最多 12 个" }

        $MapCount++
    }
} else {
    Fail 'maps/' '目录不存在'
}

# ---- summary -------------------------------------------------------------
Write-Host ''
if ($script:Errors -gt 0) {
    Write-Host ("FAILED: {0} error(s), {1} warning(s)" -f $script:Errors, $script:Warns) -ForegroundColor Red
    exit 1
}
Pass ("{0} characters, {1} skills, {2} maps" -f $CharCount, $SkillCount, $MapCount)
if ($script:Warns -gt 0) { Write-Host ("{0} warning(s)" -f $script:Warns) -ForegroundColor Yellow }
exit 0
