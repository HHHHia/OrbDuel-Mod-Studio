#include "orbduel/match/Skill.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/match/Match.hpp"
#include "orbduel/physics/World.hpp"
#include <algorithm>
#include <cmath>
namespace orbduel {
SkillSystem::SkillSystem() = default;
void SkillSystem::reset() {
    states_.clear();
    bodyId_ = -1;
    events_ = nullptr;
}
void SkillSystem::attach(int id, const std::vector<const SkillDef *> &defs, Match *m, World *w, Rng *r) {
    bodyId_ = id;
    match_ = m;
    world_ = w;
    rng_ = r;
    if (w)
        if (auto *b = w->body(id)) {
            baseRadius_ = b->radius;
            baseMass_ = b->mass;
        }
    states_.clear();
    for (auto *d : defs)
        if (d) {
            SkillState s;
            s.def = d;
            // 反应技能开局即可触发；定时技能等第一个冷却结束。
            s.cooldownLeft =
                d->trigger.kind == TriggerKind::Cooldown ? std::max(0.f, d->trigger.cooldown) : 0.f;
            states_.push_back(s);
        }
    std::stable_sort(states_.begin(), states_.end(), [](const SkillState &a, const SkillState &b) {
        return a.def->priority < b.def->priority;
    });
}
void SkillSystem::update(float dt, float ratio, Body &self, EventBus &bus) {
    events_ = &bus;
    self.radius = baseRadius_ > 0 ? baseRadius_ : self.radius;
    self.mass = baseMass_;
    self.wallBoost = 0;
    for (int i = 0; i < int(states_.size()); ++i) {
        auto &s = states_[i];
        s.cooldownLeft = std::max(0.f, s.cooldownLeft - dt);
        if (s.active) {
            s.activeAge += dt;
            s.activeLeft -= dt;
            if (s.activeLeft <= 0)
                s.active = false;
        }
        if (s.active && world_)
            for (const auto &m : s.def->modifiers) {
                if (s.activeAge >= m.duration)
                    continue;
                if (m.op == ModifierOp::Resize) {
                    const float factor = std::max(.1f, m.value);
                    self.radius *= factor;
                    self.mass *= factor * factor;
                }
                if (m.op == ModifierOp::BounceBoost)
                    self.wallBoost += m.value;
                if (m.op == ModifierOp::SlowField) {
                    for (auto &other : world_->bodies())
                        if (other.id != bodyId_ && lengthOf(other.pos - self.pos) < m.radius)
                            other.vel *= std::pow(clampf(m.factor, .001f, 1.f), dt);
                }
            }
        bool low =
            s.def->trigger.kind == TriggerKind::OnLowHp && ratio <= s.def->trigger.hpBelow && !s.lowTriggered;
        if (s.cooldownLeft <= 0 && (s.def->trigger.kind == TriggerKind::Cooldown || low)) {
            if (low)
                s.lowTriggered = true;
            cast(i, self, bus);
        }
    }
    self.invMass = self.mass > 0 ? 1.f / self.mass : 0;
}
void SkillSystem::cast(int slot, Body &self, EventBus &bus) {
    if (slot < 0 || slot >= int(states_.size()))
        return;
    auto &s = states_[slot];
    s.active = true;
    s.activeAge = 0;
    ++s.casts;
    s.activeLeft = 0;
    for (const auto &m : s.def->modifiers) {
        s.activeLeft = std::max(s.activeLeft, m.duration);
        switch (m.op) {
        case ModifierOp::AddSpeed: {
            auto dir = normalized(self.vel);
            if (lengthSq(dir) < .001f)
                dir = {1, 0};
            self.vel += dir * m.value;
            break;
        }
        case ModifierOp::Shield:
            if (match_)
                match_->giveShield(bodyId_, m.value, m.duration);
            break;
        case ModifierOp::Heal:
            if (match_)
                match_->heal(bodyId_, m.value);
            break;
        case ModifierOp::Teleport:
            if (rng_) {
                self.pos += rng_->unitVector() * m.radius;
            }
            break;
        default:
            break;
        }
    }
    s.cooldownLeft = std::max(.05f, s.def->trigger.cooldown);
    Event e;
    e.type = EventType::SkillCast;
    e.a = bodyId_;
    e.pos = self.pos;
    e.value = float(slot);
    e.tag = s.def->id;
    bus.publish(e);
}
void SkillSystem::onEvent(const Event &e, Body &self) {
    if (!events_)
        return;
    for (int i = 0; i < int(states_.size()); ++i) {
        auto &s = states_[i];
        if (s.active && e.type == EventType::Damage && e.tag == "contact" && match_)
            for (const auto &m : s.def->modifiers) {
                if (s.activeAge >= m.duration)
                    continue;
                if (m.op == ModifierOp::Lifesteal && e.a == bodyId_)
                    match_->heal(bodyId_, e.value * m.value);
                if (m.op == ModifierOp::Thorns && e.b == bodyId_ && e.a >= 0)
                    match_->damage(bodyId_, e.a, (e.value + e.value2) * m.value, "thorns");
            }
        if (s.cooldownLeft > 0)
            continue;
        const auto k = s.def->trigger.kind;
        if ((e.type == EventType::Hit && (e.a == bodyId_ || e.b == bodyId_) &&
             (k == TriggerKind::OnHit || k == TriggerKind::OnHitBy)) ||
            (e.type == EventType::WallBounce && e.a == bodyId_ && k == TriggerKind::OnWallBounce))
            cast(i, self, *events_);
    }
}
const std::vector<SkillState> &SkillSystem::states() const {
    return states_;
}
int SkillSystem::bodyId() const {
    return bodyId_;
}
float SkillSystem::damageMultiplier() const {
    float factor = 1;
    for (const auto &s : states_)
        if (s.active)
            for (const auto &m : s.def->modifiers)
                if (m.op == ModifierOp::DamageBoost && s.activeAge < m.duration)
                    factor *= m.value;
    return factor;
}
} // namespace orbduel
