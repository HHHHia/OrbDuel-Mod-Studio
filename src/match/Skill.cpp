#include "orbduel/match/Skill.hpp"

#include <algorithm>
#include <set>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

namespace {

// 冲出后还能造成重击的窗口：够穿过一次场地，撞一次就结束
constexpr float kRamWindow = 1.2f;

} // namespace

SkillSystem::SkillSystem() = default;

void SkillSystem::reset() {
    states_.clear();
    bodyId_ = -1;
}

void SkillSystem::attach(int bodyId, const std::vector<const SkillDef*>& skills) {
    bodyId_ = bodyId;
    states_.clear();
    for (const SkillDef* def : skills) {
        if (def == nullptr) {
            continue;
        }
        SkillState state;
        state.def = def;
        state.cooldownLeft = def->trigger.cooldown > 0.0f ? def->trigger.cooldown : 1.0f;
        states_.push_back(state);
    }
}

void SkillSystem::update(float dt,
                         float hpRatio,
                         Body& self,
                         const Body& enemy,
                         Rng& rng,
                         EventBus& events) {
    (void)hpRatio;

    for (std::size_t i = 0; i < states_.size(); ++i) {
        SkillState& state = states_[i];
        if (state.def == nullptr) {
            continue;
        }

        state.cooldownLeft = std::max(0.0f, state.cooldownLeft - dt);
        if (state.active) {
            state.activeLeft -= dt;
            if (state.activeLeft <= 0.0f) {
                state.active = false;
            }
        }
        if (state.ramLeft > 0.0f) {
            state.ramLeft = std::max(0.0f, state.ramLeft - dt);
        }

        if (state.charging) {
            // 蓄力中：方向每帧重算（所以虚线一直在追），时间到就冲出去
            state.aimDir = aimAt(self, enemy, state.def->charge, rng);
            state.chargeLeft -= dt;
            if (state.chargeLeft <= 0.0f) {
                release(static_cast<int>(i), self, events);
            }
            continue;
        }

        if (state.cooldownLeft > 0.0f || state.def->trigger.kind != TriggerKind::Cooldown) {
            continue;
        }

        if (!state.def->charge.enabled) {
            cast(static_cast<int>(i), self, events);
            continue;
        }

        // 原地蓄力：只清一次速度，StallGuard 的吸力照样能把它推走（按设计保留）
        state.charging = true;
        state.chargeLeft = std::max(0.1f, state.def->charge.duration);
        state.aimDir = aimAt(self, enemy, state.def->charge, rng);
        self.vel = Vec2{0.0f, 0.0f};
        log::info("charge start: " + state.def->id);
    }
}

Vec2 SkillSystem::aimAt(const Body& self, const Body& enemy, const ChargeDef& charge, Rng& rng) const {
    // shortcut: 用「敌人速度 × 延迟」回推锁定点，不存历史轨迹；敌人撞墙反弹那一下会偏，
    // 观感上正好是「不会百发百中」。要精确复刻轨迹再改成环形缓冲。
    const Vec2 locked = enemy.pos - enemy.vel * charge.aimLag;

    Vec2 dir = normalized(locked - self.pos);
    if (lengthSq(dir) < 0.5f) {
        dir = normalized(self.vel);
    }
    if (lengthSq(dir) < 0.5f) {
        dir = Vec2{1.0f, 0.0f};
    }

    if (charge.aimJitterDeg > 0.0f) {
        dir = rotated(dir, degToRad(rng.range(-charge.aimJitterDeg, charge.aimJitterDeg)));
    }
    return dir;
}

void SkillSystem::release(int slot, Body& self, EventBus& events) {
    SkillState& state = states_[static_cast<std::size_t>(slot)];
    state.charging = false;
    state.ramLeft = kRamWindow;
    self.vel = state.aimDir * state.def->charge.releaseSpeed;
    log::info("charge release: " + state.def->id);
    // 冲出也算一次施放：冷却、事件、vfx 都走同一条路
    cast(slot, self, events);
}

void SkillSystem::interruptCharge() {
    for (SkillState& state : states_) {
        if (!state.charging || state.def == nullptr) {
            continue;
        }
        state.charging = false;
        state.cooldownLeft = std::max(0.25f, state.def->trigger.cooldown);
        log::info("charge interrupted: " + state.def->id);
    }
}

bool SkillSystem::consumeRam(float& outScale) {
    for (SkillState& state : states_) {
        if (state.ramLeft <= 0.0f || state.def == nullptr) {
            continue;
        }
        state.ramLeft = 0.0f;
        outScale = state.def->charge.ramDamageScale;
        return true;
    }
    return false;
}

void SkillSystem::cast(int slot, Body& self, EventBus& events) {
    if (slot < 0 || slot >= static_cast<int>(states_.size())) {
        return;
    }
    SkillState& state = states_[slot];
    if (state.def == nullptr) {
        return;
    }

    bool appliedAny = false;
    for (const ModifierDef& modifier : state.def->modifiers) {
        if (modifier.op == ModifierOp::AddSpeed) {
            Vec2 direction = normalized(self.vel);
            if (lengthSq(direction) < 1e-4f) {
                direction = Vec2{1.0f, 0.0f};
            }
            self.vel += direction * modifier.value;
            appliedAny = true;
        }
        // 其余原语（shield / lifesteal / slow_field / teleport / resize / bounce_boost）
        // 需要 Match 的上下文，MVP 先留空，下一步接（见 docs/STATUS.md）。
    }

    state.cooldownLeft = std::max(0.25f, state.def->trigger.cooldown);
    state.active = true;
    state.activeLeft = 0.25f;

    Event event;
    event.type = EventType::SkillCast;
    event.a = bodyId_;
    event.pos = self.pos;
    event.value = static_cast<float>(slot);
    events.publish(event);

    if (!appliedAny && !state.def->charge.enabled) {
        static std::set<std::string> warned;
        if (warned.insert(state.def->id).second) {
            log::warn("skill '" + state.def->id + "' 的原语在 MVP 里还没实现");
        }
    }
}

void SkillSystem::onEvent(const Event& event, Body& self) {
    (void)event;
    (void)self;
    // on_hit / on_hit_by 等事件触发留给下一步；当前只有 Cooldown 生效。
}

const std::vector<SkillState>& SkillSystem::states() const { return states_; }
int SkillSystem::bodyId() const { return bodyId_; }

} // namespace orbduel
