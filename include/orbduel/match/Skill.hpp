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
class EventBus;
class Rng;
class Match;
class World;
struct SkillDef;

struct SkillState {
    const SkillDef *def = nullptr;
    float cooldownLeft = 0.0f;
    float activeLeft = 0.0f;
    bool active = false;
    bool lowTriggered = false;
    float activeAge = 0.0f;
    int casts = 0;
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
    void update(float dt, float hpRatio, Body &self, EventBus &events);

    // 事件驱动的触发（on_hit / on_hit_by / on_wall_bounce）。
    void onEvent(const Event &event, Body &self);

    const std::vector<SkillState> &states() const;
    int bodyId() const;
    // 当前有效的碰撞攻击倍率；同一技能的各修饰器分别按 duration 到期。
    float damageMultiplier() const;

  private:
    void cast(int slot, Body &self, EventBus &events);

    std::vector<SkillState> states_;
    int bodyId_ = -1;
    Match *match_ = nullptr;
    World *world_ = nullptr;
    Rng *rng_ = nullptr;
    EventBus *events_ = nullptr;
    float baseRadius_ = 0, baseMass_ = 1;
};

} // namespace orbduel
