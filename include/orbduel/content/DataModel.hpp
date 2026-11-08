#pragma once

// 内容数据模型：纯数据，不依赖 SDL2，也不依赖 physics。
// 字段与 content/schema/*.schema.json 一一对应；改动必须同步
// docs/CONTENT_FORMAT.md 与 schema（AGENTS 铁律 1）。

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
    std::string texture;          // 相对角色目录
    float scale = 1.0f;           // 只影响绘制
    Vec2 anchor{0.5f, 0.5f};      // 归一化锚点
    Rgba tint{255, 255, 255, 255};
};

struct HitboxDef {
    float radius = 18.0f;
    Vec2 offset{};                // 贴图中心 → 碰撞中心的偏移
};

struct StatsDef {
    float mass = 1.0f;
    float restitution = 0.95f;
    float maxHp = 100.0f;
    float startSpeed = 420.0f;
};

struct CharacterDef {
    int format = 1;
    std::string id;
    LocalizedText name;
    SkinDef skin;
    HitboxDef hitbox;
    StatsDef stats;
    std::vector<std::string> skills;   // 0~2 个技能 id
    std::string sourceDir;             // 例如 "characters/blaze"
};

enum class TriggerKind {
    Cooldown = 0,
    OnHit,
    OnHitBy,
    OnLowHp,
    OnWallBounce,
    Manual            // 预留给玩家输入
};

enum class ModifierOp {
    AddSpeed = 0,
    Shield,
    Lifesteal,
    SlowField,
    Teleport,
    Resize,
    BounceBoost,
    Heal
};

enum class Curve {
    Linear = 0,
    EaseOut,
    EaseInOut
};

enum class VfxType {
    Trail = 0,
    Burst,
    Ring,
    Flash,
    ScreenShake,
    HitStop
};

enum class VfxOn {
    Cast = 0,
    Hit,
    Expire
};

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

struct SkillDef {
    int format = 1;
    std::string id;
    LocalizedText name;
    TriggerDef trigger;
    std::vector<ModifierDef> modifiers;
    std::vector<VfxDef> vfx;
    bool stackRefresh = true;
    int priority = 10;
    std::string sourceFile;      // 例如 "skills/dash.json"
};

struct ObstacleDef {
    std::string shape;           // "rect" | "circle"
    Vec2 pos{};
    Vec2 size{};
    float radius = 0.0f;
    float restitution = 1.0f;
};

struct SpawnDef {
    Vec2 pos{};
    Vec2 dir{1.0f, 0.0f};
    std::string character;       // 可选：指定该出生点固定角色
};

struct ThemeDef {
    std::string bgTexture;       // 可为空
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
    PhysicsDef physics;
    ShrinkDef shrink;
    std::uint32_t seed = 0;
    std::string sourceDir;       // 例如 "maps/duel-basic"
};

struct ContentPack {
    std::vector<CharacterDef> characters;
    std::vector<SkillDef> skills;
    std::vector<MapDef> maps;

    const CharacterDef* findCharacter(const std::string& id) const;
    const SkillDef* findSkill(const std::string& id) const;
    const MapDef* findMap(const std::string& id) const;
};

} // namespace orbduel
