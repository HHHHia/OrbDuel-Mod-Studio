#include "orbduel/match/Skill.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/match/Match.hpp"
#include "orbduel/physics/World.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace orbduel {
namespace {

// 冲出后还能造成重击的窗口：够穿过一次场地，撞一次就结束
constexpr float kRamWindow = 1.2f;

// 虚线抖动是低频噪声：每 0.5 s 换一个目标角度，再用 0.35 s 插值过去，
// 这样线是「慢慢飘」而不是每帧乱跳。
constexpr float kAimNoiseSeconds = 0.5f;
constexpr float kAimSmoothSeconds = 0.35f;

// 成长曲线：对局时间 / 时长，线性 0 → 1。黑洞的伤害/范围/时长都按它长。
float growthByTime(float elapsed, float seconds) {
    if (seconds <= 0.01f) {
        return 1.0f;
    }
    return clampf(elapsed / seconds, 0.0f, 1.0f);
}

} // namespace

SkillSystem::SkillSystem() = default;
void SkillSystem::reset() {
    states_.clear();
    bodyId_ = -1;
    events_ = nullptr;
    elapsed_ = 0.f;
}
void SkillSystem::attach(int id, const std::vector<const SkillDef *> &defs, Match *m, World *w, Rng *r) {
    bodyId_ = id;
    match_ = m;
    world_ = w;
    rng_ = r;
    if (w)
        if (auto *b = w->body(id)) {
            baseRadius_ = b->radius;
        }
    elapsed_ = 0.f;
    states_.clear();
    for (auto *d : defs)
        if (d) {
            SkillState s;
            s.def = d;
            // 反应技能开局即可触发；定时技能等第一个冷却结束。
            s.cooldownLeft =
                d->trigger.kind == TriggerKind::Cooldown ? std::max(0.f, d->trigger.cooldown) : 0.f;
            if (d->trigger.kind == TriggerKind::Passive) {
                s.active = true;
                s.activeLeft = std::numeric_limits<float>::infinity();
            }
            states_.push_back(s);
        }
    std::stable_sort(states_.begin(), states_.end(), [](const SkillState &a, const SkillState &b) {
        return a.def->priority < b.def->priority;
    });
}
void SkillSystem::update(float dt, float ratio, Body &self, const Body &enemy, Rng &rng, EventBus &bus) {
    events_ = &bus;
    self.radius = baseRadius_ > 0 ? baseRadius_ : self.radius;
    elapsed_ += dt;
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
        if (s.def->charge.enabled || s.def->omnipull.enabled) {
            updateSpecial(i, dt, self, enemy, rng, bus);
            continue;
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
void SkillSystem::updateSpecial(int i, float dt, Body &self, const Body &enemy, Rng &rng, EventBus &events) {
    SkillState &state = states_[i];
    if (state.ramLeft > 0.0f) {
        state.ramLeft = std::max(0.0f, state.ramLeft - dt);
    }

    // 万向天引：正在吸就只推进倒计时；吸完的那一步挂上爆炸，再从那一刻开始算冷却
    if (state.pullLeft > 0.0f) {
        state.pullLeft -= dt;
        if (state.pullLeft <= 0.0f) {
            state.pullLeft = 0.0f;
            state.blastPending = true;
            // 冷却固定用 trigger.cooldown，爆炸之后才开始算
            state.cooldownLeft = state.def->trigger.cooldown;
            log::info("omnipull blast: " + state.def->id);
        }
        return;
    }

    if (state.charging) {
        // 蓄力中：方向每帧重算，但抖动走低频噪声 + 插值，线是「飘」不是「抖」
        const ChargeDef &charge = state.def->charge;
        const float progress = learnProgress(state.casts, charge.aimLearningCasts);
        const float lag = charge.aimLag + (charge.aimLagMin - charge.aimLag) * progress;
        const float jitter = charge.aimJitterDeg + (charge.aimJitterMinDeg - charge.aimJitterDeg) * progress;

        state.aimNoiseTimer -= dt;
        if (state.aimNoiseTimer <= 0.0f) {
            state.aimNoiseTarget = jitter > 0.0f ? rng.range(-jitter, jitter) : 0.0f;
            state.aimNoiseTimer = kAimNoiseSeconds;
        }
        const float blend = clampf(dt / kAimSmoothSeconds, 0.0f, 1.0f);
        state.aimNoise += (state.aimNoiseTarget - state.aimNoise) * blend;

        state.aimDir = rotated(aimAt(self, enemy, lag), degToRad(state.aimNoise));
        state.chargeLeft -= dt;
        if (state.chargeLeft <= 0.0f) {
            release(static_cast<int>(i), self, events);
        }
        return;
    }

    if (state.cooldownLeft > 0.0f || state.def->trigger.kind != TriggerKind::Cooldown) {
        return;
    }

    if (state.def->omnipull.enabled) {
        // 万向天引：按对局时间锁定本次的半径/时长/伤害，冷却等炸完再进
        const OmnipullDef &pull = state.def->omnipull;
        const float progress = growthByTime(elapsed_, pull.growthSeconds);
        state.pullRadius = pull.rangeFrom + (pull.rangeTo - pull.rangeFrom) * progress;
        state.pullAccel = pull.accel;
        state.pullDpsEdge = pull.dpsEdge;
        state.pullDpsCenter = pull.dpsCenter;
        state.pullLeft = pull.durationFrom + (pull.durationTo - pull.durationFrom) * progress;
        state.blastRadius = state.pullRadius;
        state.blastDamage = pull.damageFrom + (pull.damageTo - pull.damageFrom) * progress;
        ++state.casts;
        state.activeAge = 0.f;
        state.active = true;
        state.activeLeft = state.pullLeft;

        Event event;
        event.type = EventType::SkillCast;
        event.a = bodyId_;
        event.pos = self.pos;
        event.value = static_cast<float>(i);
        event.tag = state.def->id;
        events.publish(event);
        log::info("omnipull start: " + state.def->id);
        return;
    }

    if (!state.def->charge.enabled) {
        cast(static_cast<int>(i), self, events);
        return;
    }

    // 原地蓄力：只清一次速度，StallGuard 的吸力照样能把它推走（按设计保留）
    const ChargeDef &charge = state.def->charge;
    state.charging = true;
    state.chargeLeft = charge.duration + (charge.durationMin - charge.duration) *
                                             learnProgress(state.casts, charge.durationLearningCasts);
    state.aimNoise = 0.0f;
    state.aimNoiseTarget = 0.0f;
    state.aimNoiseTimer = 0.0f;
    state.aimDir = aimAt(self, enemy, charge.aimLag);
    self.vel = Vec2{0.0f, 0.0f};
    log::info("charge start: " + state.def->id);

    // 蓄力时间已经缩到 0：当帧就冲出去，不再等
    if (state.chargeLeft <= 0.0f) {
        release(static_cast<int>(i), self, events);
    }
}

// 0 = 第一次放，1 = 已经放够 steps 次
float SkillSystem::learnProgress(int casts, int steps) {
    const int need = steps > 0 ? steps : 1;
    return clampf(static_cast<float>(casts) / static_cast<float>(need), 0.0f, 1.0f);
}

Vec2 SkillSystem::aimAt(const Body &self, const Body &enemy, float aimLag) const {
    // shortcut: 用「敌人速度 × 延迟」回推锁定点，不存历史轨迹；敌人撞墙反弹那一下会偏，
    // 观感上正好是「不会百发百中」。要精确复刻轨迹再改成环形缓冲。
    const Vec2 locked = enemy.pos - enemy.vel * aimLag;

    Vec2 dir = normalized(locked - self.pos);
    if (lengthSq(dir) < 0.5f) {
        dir = normalized(self.vel);
    }
    if (lengthSq(dir) < 0.5f) {
        dir = Vec2{1.0f, 0.0f};
    }
    return dir;
}

void SkillSystem::release(int slot, Body &self, EventBus &events) {
    SkillState &state = states_[static_cast<std::size_t>(slot)];
    state.charging = false;
    state.ramLeft = kRamWindow;
    self.vel = state.aimDir * state.def->charge.releaseSpeed;
    // cast 统一记录成功释放次数，避免学习计数翻倍。
    log::info("charge release: " + state.def->id);
    // 冲出也算一次施放：冷却、事件、vfx 都走同一条路
    cast(slot, self, events);
}

void SkillSystem::interruptCharge() {
    for (SkillState &state : states_) {
        if (!state.charging || state.def == nullptr) {
            continue;
        }
        // 放够次数之后就打不断了
        if (state.casts >= state.def->charge.interruptImmuneCasts) {
            continue;
        }
        state.charging = false;
        state.cooldownLeft = std::max(0.25f, state.def->trigger.cooldown);
        log::info("charge interrupted: " + state.def->id);
    }
}

bool SkillSystem::consumeRam(float &outScale) {
    for (SkillState &state : states_) {
        if (state.ramLeft <= 0.0f || state.def == nullptr) {
            continue;
        }
        state.ramLeft = 0.0f;
        outScale = state.def->charge.ramDamageScale;
        return true;
    }
    return false;
}

bool SkillSystem::pullField(PullField &out) const {
    for (const SkillState &state : states_) {
        if (state.pullLeft <= 0.0f || state.def == nullptr) {
            continue;
        }
        out.radius = state.pullRadius;
        out.accel = state.pullAccel;
        out.dpsEdge = state.pullDpsEdge;
        out.dpsCenter = state.pullDpsCenter;
        return true;
    }
    return false;
}

bool SkillSystem::invulnerable() const {
    for (const SkillState &state : states_) {
        if (state.pullLeft > 0.0f && state.def != nullptr && state.def->omnipull.enabled &&
            state.def->omnipull.invincible) {
            return true;
        }
    }
    return false;
}

bool SkillSystem::suppressesPassive() const {
    for (const SkillState &state : states_) {
        if (state.pullLeft > 0.0f && state.def != nullptr && state.def->omnipull.enabled &&
            state.def->omnipull.suppressPassive) {
            return true;
        }
    }
    return false;
}

bool SkillSystem::omnipullActive() const {
    for (const SkillState &state : states_) {
        if (state.pullLeft > 0.0f && state.def != nullptr && state.def->omnipull.enabled) {
            return true;
        }
    }
    return false;
}

bool SkillSystem::consumeBlast(float &radius, float &damage) {
    for (SkillState &state : states_) {
        if (!state.blastPending) {
            continue;
        }
        state.blastPending = false;
        radius = state.blastRadius;
        damage = state.blastDamage;
        return true;
    }
    return false;
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
