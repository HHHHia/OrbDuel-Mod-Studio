#pragma once

// 内容数据模型：纯数据，不依赖 SDL2，也不依赖 physics。
// 字段以本结构、ContentLoader 与 tools/validate_content.ps1 为准；改动必须同步
// docs/CONTENT_FORMAT.md（AGENTS 铁律 1）。

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "orbduel/core/Types.hpp"

namespace orbduel {

struct LocalizedText {
    std::string en;
    std::string zh;
};

struct SkinDef {
    std::string pattern;
    std::string texture;          // 相对角色目录
    std::string shape = "sprite"; // "sprite" 画贴图 | "triangle" 程序化皮肤
    float scale = 1.0f;           // 只影响绘制
    Vec2 anchor{0.5f, 0.5f};      // 归一化锚点
    Rgba tint{255, 255, 255, 255};
};

struct HitboxDef {
    float radius = 18.0f;
    Vec2 offset{};        // 贴图中心 → 碰撞中心的偏移
    bool ghost = false;   // true：对方可以穿进身体（黑洞），不做分离也不推人
    float contact = 0.0f; // 接触伤害系数：0 = 撞它不掉血；1 = 常规撞击伤害（和地图 damageScale 取大）
};

struct StatsDef {
    float attack = 12.0f;
    float armor = 0.0f;
    float mass = 1.0f;
    float massMax = 0.0f;   // > mass 时质量随对局时间从 mass 长到 massMax
    float massRamp = 90.0f; // 多少秒长到 massMax
    float restitution = 0.95f;
    float maxHp = 100.0f;
    float startSpeed = 420.0f;
};

struct CharacterDef {
    int format = 1;
    std::string id;
    LocalizedText name;
    LocalizedText description;
    SkinDef skin;
    HitboxDef hitbox;
    StatsDef stats;
    std::vector<std::string> skills; // 0~2 个技能 id
    std::string sourceDir;           // 例如 "characters/lancer"
};

enum class TriggerKind {
    Cooldown = 0,
    OnHit,
    OnHitBy,
    OnLowHp,
    OnWallBounce,
    Manual, // 预留给玩家输入
    Passive // 被动：开局就生效，不进冷却循环
};

enum class ModifierOp {
    AddSpeed = 0,
    Shield,
    Lifesteal,
    SlowField,
    Teleport,
    Resize,
    BounceBoost,
    Heal,
    DamageBoost,
    Thorns,
    Pierce // 贯穿：撞到对手不弹开、直接穿过，并按 value/minSpeed/push 结算
};

enum class Curve { Linear = 0, EaseOut, EaseInOut };

enum class VfxType { Trail = 0, Burst, Ring, Flash, ScreenShake, HitStop };

enum class VfxOn { Cast = 0, Hit, Expire };

struct TriggerDef {
    TriggerKind kind = TriggerKind::Cooldown;
    float cooldown = 5.0f;
    float hpBelow = 0.3f;
};

struct ModifierDef {
    ModifierOp op = ModifierOp::AddSpeed;
    float value = 0.0f;
    float duration = 0.0f;
    float radius = 0.0f;
    float factor = 1.0f;
    float minSpeed = 0.0f; // pierce：自身速度 ≥ 它才贯穿
    float push = 0.0f;     // pierce：穿过时给对手加的速度（沿碰撞法线）
    Curve curve = Curve::Linear;
};

struct VfxDef {
    VfxOn on = VfxOn::Cast;
    VfxType type = VfxType::Burst;
    Rgba color{255, 255, 255, 255};
    float duration = 0.5f;
    float value = 0.0f;
    int count = 12;
};

// 蓄力冲刺：冷却到点后先原地蓄力，再沿「延迟锁定的敌人位置」冲出，撞到对手算一次重击。
struct ChargeDef {
    bool enabled = false;
    float duration = 3.0f;          // 蓄力时长（秒，第一次放的时候用）
    float durationMin = 0.0f;       // 放熟之后的蓄力时长下限（0 = 瞬发）
    int durationLearningCasts = 10; // 放几次把蓄力时间压到 durationMin
    float releaseSpeed = 500.0f;    // 释放瞬间的速度
    float aimLag = 0.3f;            // 锁定点落后敌人多少秒（第一次放的时候用）
    float aimJitterDeg = 6.0f;      // 锁定方向的随机抖动（度，第一次放的时候用）
    float aimLagMin = 0.05f;        // 放熟之后的下限
    float aimJitterMinDeg = 0.5f;   // 放熟之后的下限
    int aimLearningCasts = 5;       // 放几次达到最准
    int interruptImmuneCasts = 15;  // 放够几次之后蓄力打不断
    float ramDamageScale = 0.12f;   // 撞击额外伤害 = scale × 接触瞬间相对速度
};

// 引力井（被动）：把范围内的敌人往自己中心拉；敌人进到核心圈就掉血、并被随机传送走。
// 范围、核心伤害都按「本局已进行时间 / growthSeconds」线性长到上限。
struct GravityDef {
    bool enabled = false;
    float range = 260.0f;           // 起始吸力半径
    float rangeMax = 260.0f;        // 长满时的吸力半径
    float accel = 420.0f;           // 向心加速度（px/s²），保留切向速度 → 会绕圈
    float coreFactor = 0.7f;        // 核心半径 = 本值 × 自身碰撞半径
    float damage = 6.0f;            // 核心伤害起始值
    float damageMax = 18.0f;        // 核心伤害上限
    float growthSeconds = 90.0f;    // 多少秒长满（范围与核心伤害共用）
    float dpsEdge = 0.0f;           // 站在吸力场边缘每秒掉多少血
    float dpsCenter = 0.0f;         // 站在中心每秒掉多少血（按距离线性插值）
    float tickInterval = 0.5f;      // 核心内两次核心伤害的最小间隔
    float teleportGuard = 1.5f;     // 同一个人两次传送的最小间隔
    float teleportMinDist = 240.0f; // 落点离黑洞至少多远
};

// 万向天引（大招）：范围内持续吸力 → N 秒后原地爆炸；期间自己无敌、被动失效、不吃撞击伤害。
// 范围、时长、伤害按时间线性成长；冷却固定用 trigger.cooldown（爆炸之后才开始算）。
struct OmnipullDef {
    bool enabled = false;
    float rangeFrom = 200.0f;
    float rangeTo = 340.0f;
    float accel = 1200.0f;
    float durationFrom = 2.0f;
    float durationTo = 5.0f;
    float damageFrom = 20.0f;
    float damageTo = 48.0f;
    float growthSeconds = 90.0f;
    float dpsEdge = 0.0f;
    float dpsCenter = 0.0f;
    bool invincible = true;
    bool suppressPassive = true;
};

struct SkillDef {
    int format = 1;
    std::string id;
    LocalizedText name;
    LocalizedText description;
    TriggerDef trigger;
    ChargeDef charge;
    GravityDef gravity;
    OmnipullDef omnipull;
    std::vector<ModifierDef> modifiers;
    std::vector<VfxDef> vfx;
    bool stackRefresh = true;
    int priority = 10;
    std::string sourceFile; // 例如 "skills/dash.json"
};

struct ObstacleDef {
    std::string shape; // "rect" | "circle"
    Vec2 pos{};
    Vec2 size{};
    float radius = 0.0f;
    float restitution = 1.0f;
};

struct SpawnDef {
    Vec2 pos{};
    Vec2 dir{1.0f, 0.0f};
    std::string character; // 可选：指定该出生点固定角色
};

// 地图自带的范围效果：球在圆圈里会被持续加速、扣血或回血。
// 纯数据（写在地图 JSON 的 fields 里），不需要编译模组。
struct FieldDef {
    Vec2 pos{};
    float radius = 120.0f;
    float dps = 0.0f;   // 每秒环境伤害（走 environment：不吃连击、不触发反伤）
    float heal = 0.0f;  // 每秒回血
    Vec2 accel{};       // 持续加速度（px/s²），每一物理步都加
    int affect = 0;     // 0 所有球 | 1 左方球 | 2 右方球
    Rgba color{120, 200, 255, 90};
};

struct ThemeDef {
    std::string bgTexture; // 可为空
    Rgba bgColor{16, 17, 24, 255};
    Rgba wallColor{90, 120, 255, 255};
    float wallThickness = 6.0f;
};

struct PhysicsDef {
    float wallRestitution = 0.98f;
    float bounceJitterDeg = 1.5f;
    float maxSpeed = 1800.0f;
    float damageScale = 1.0f;
    float refSpeed = 400.0f;
};

struct ShrinkDef {
    float firstAt = 25.0f;
    float interval = 12.0f;
    float step = 0.08f;
    float minInset = 0.40f;
    float warnTime = 1.0f;
    float animTime = 1.5f;
    float damagePerSec = 4.0f;
    float lullTrigger = 8.0f;
};

struct MapDef {
    int format = 1;
    std::string id;
    LocalizedText name;
    Vec2 size{1280.0f, 720.0f};
    ThemeDef theme;
    std::vector<SpawnDef> spawns;
    std::vector<ObstacleDef> obstacles;
    std::vector<FieldDef> fields;
    PhysicsDef physics;
    ShrinkDef shrink;
    std::uint32_t seed = 0;
    std::string sourceDir; // 例如 "maps/duel-basic"
};

struct ContentPack {
    std::vector<CharacterDef> characters;
    std::vector<SkillDef> skills;
    std::vector<MapDef> maps;

    const CharacterDef *findCharacter(const std::string &id) const;
    const SkillDef *findSkill(const std::string &id) const;
    const MapDef *findMap(const std::string &id) const;
};

} // namespace orbduel
