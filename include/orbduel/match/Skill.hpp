#pragma once

// 技能运行时：冷却、触发条件、修饰器生效。
// 技能只改 Body 的公开字段或发事件，不画东西（ARCHITECTURE §8）。
//
// 注意：TriggerKind / ModifierOp 的权威定义在 content/DataModel.hpp，
// 这里只使用、不重复定义（否则两个头文件一起 include 会冲突）。

#include <string>
#include <vector>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Types.hpp"

namespace orbduel {

struct Body;
class Match;
class World;
class EventBus;
class Rng;
struct SkillDef;

struct SkillState {
    const SkillDef *def = nullptr;
    float cooldownLeft = 0.0f;
    float activeLeft = 0.0f;
    bool active = false;
    bool lowTriggered = false;
    float activeAge = 0.0f;

    // 蓄力冲刺（def->charge.enabled 时才使用）
    bool charging = false;
    float chargeLeft = 0.0f;
    Vec2 aimDir{1.0f, 0.0f}; // 当前锁定方向，已含抖动，虚线与冲出都用它
    float ramLeft = 0.0f;    // > 0 表示「冲出后还能造成一次重击」的窗口
    float aimNoise = 0.0f;   // 当前抖动角（度）：插值出来的，不是每帧随机
    float aimNoiseTarget = 0.0f;
    float aimNoiseTimer = 0.0f;
    int casts = 0; // 本局已经放出去几次，索敌精度随它提升

    // 万向天引（def->omnipull.enabled 时才使用）
    float pullLeft = 0.0f;   // > 0 = 正在吸；倒数到 0 就炸
    float pullRadius = 0.0f; // 本次吸力半径（施放瞬间按时间成长锁定）
    float pullAccel = 0.0f;
    float pullDpsEdge = 0.0f;
    float pullDpsCenter = 0.0f;
    float blastRadius = 0.0f;
    float blastDamage = 0.0f;
    bool blastPending = false;
};

// 吸力场的运行时参数：Match 拿它施加向心加速度 + 场内持续伤害。
struct PullField {
    float radius = 0.0f;
    float accel = 0.0f;     // 已含「质量放大」的话由施加方决定，这里只是数据里的值
    float dpsEdge = 0.0f;   // 边缘每秒伤害
    float dpsCenter = 0.0f; // 中心每秒伤害（按距离线性插值）
};

class SkillSystem {
  public:
    SkillSystem();
    ~SkillSystem() = default;

    void reset();

    // 绑定到一个 body 上，skills 指向内容包里已经校验过的 SkillDef。
    void attach(int bodyId, const std::vector<const SkillDef *> &skills, Match *match = nullptr,
                World *world = nullptr, Rng *rng = nullptr);

    // 每步调用：递减冷却、推进持续时间、检查自动触发。
    // enemy 只用于蓄力时锁定方向；rng 只用于锁定抖动（铁律 6：不许 rand()）。
    void update(float dt, float hpRatio, Body &self, const Body &enemy, Rng &rng, EventBus &events);

    // 事件驱动的触发（on_hit / on_hit_by / on_wall_bounce）。
    void onEvent(const Event &event, Body &self);

    // 被撞击：正在蓄力的技能立即作废并进 CD（不看这一下掉没掉血）。
    void interruptCharge();

    // 冲刺窗口内的第一次撞击：消费掉，并通过 outScale 返回额外伤害系数。
    bool consumeRam(float &outScale);

    // 万向天引正在吸时填充本次的半径/加速度/场内 DPS。
    bool pullField(PullField &out) const;

    // 万向天引进行中：自己无敌 / 被动失效 / 整对不吃撞击伤害。
    bool invulnerable() const;
    bool suppressesPassive() const;
    bool omnipullActive() const;

    // 吸力结束的那一步取走爆炸参数；只返回一次。
    bool consumeBlast(float &radius, float &damage);

    const std::vector<SkillState> &states() const;
    int bodyId() const;
    float damageMultiplier() const;

  private:
    void cast(int slot, Body &self, EventBus &events);
    void updateSpecial(int slot, float dt, Body &self, const Body &enemy, Rng &rng, EventBus &events);
    void release(int slot, Body &self, EventBus &events);
    Vec2 aimAt(const Body &self, const Body &enemy, float aimLag) const;
    static float learnProgress(int casts, int steps);

    std::vector<SkillState> states_;
    int bodyId_ = -1;
    Match *match_ = nullptr;
    World *world_ = nullptr;
    Rng *rng_ = nullptr;
    EventBus *events_ = nullptr;
    float baseRadius_ = 0.0f;
    float elapsed_ = 0.0f; // 本局技能时钟，成长曲线按它算（和 Match 的 elapsed 同步）
};

} // namespace orbduel
