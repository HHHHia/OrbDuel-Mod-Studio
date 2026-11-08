#include "orbduel/match/Skill.hpp"

#include <algorithm>
#include <set>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

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

void SkillSystem::update(float dt, float hpRatio, Body& self, EventBus& events) {
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

        if (state.cooldownLeft <= 0.0f && state.def->trigger.kind == TriggerKind::Cooldown) {
            cast(static_cast<int>(i), self, events);
        }
    }
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

    if (!appliedAny) {
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
