#include "orbduel/match/Match.hpp"

#include <algorithm>
#include <cmath>

#include "orbduel/content/DataModel.hpp"
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/physics/Arena.hpp"
#include "orbduel/physics/World.hpp"

namespace orbduel {

namespace {

// 成长曲线：对局时间 / 时长，线性 0 → 1（和 SkillSystem 里的成长用同一套口径）
float growthOf(float elapsed, float seconds) {
    if (seconds <= 0.01f) {
        return 1.0f;
    }
    return clampf(elapsed / seconds, 0.0f, 1.0f);
}

} // namespace

Match::Match(EventBus& events, World& world, Arena& arena)
    : events_(events),
      world_(world),
      arena_(arena),
      rule_(std::make_unique<HpRule>()),
      zone_(arena, ShrinkDef{}) {}

Rng& Match::rngRef() {
    static Rng fallback;
    return rng_ != nullptr ? *rng_ : fallback;
}

void Match::setRule(std::unique_ptr<IMatchRule> rule) {
    if (rule) {
        rule_ = std::move(rule);
    }
    if (rule_) {
        rule_->reset();
    }
}

void Match::setup(const CharacterDef& left,
                  const CharacterDef& right,
                  const MapDef& map,
                  const ContentPack& pack,
                  Rng& rng) {
    world_.clear();
    skills_[0].reset();
    skills_[1].reset();
    stall_.reset();
    zone_.configure(map.shrink);

    map_ = &map;

    const float wall = map.theme.wallThickness;
    Rect bounds;
    bounds.x = wall;
    bounds.y = wall;
    bounds.w = map.size.x - wall * 2.0f;
    bounds.h = map.size.y - wall * 2.0f;
    if (bounds.w < 64.0f) { bounds.w = 64.0f; }
    if (bounds.h < 64.0f) { bounds.h = 64.0f; }

    arena_.configure(bounds, map.physics.maxSpeed);
    arena_.clearObstacles();

    int obstacleId = 0;
    for (const ObstacleDef& def : map.obstacles) {
        Obstacle obstacle;
        obstacle.id = obstacleId;
        ++obstacleId;
        obstacle.pos = def.pos;
        obstacle.restitution = def.restitution;
        if (def.shape == "circle") {
            obstacle.shape = ObstacleShape::Circle;
            obstacle.radius = def.radius;
        } else {
            obstacle.shape = ObstacleShape::Rect;
            obstacle.size = def.size;
        }
        arena_.addObstacle(obstacle);
    }

    world_.setRng(rng);
    world_.setWallParams(map.physics.wallRestitution, map.physics.bounceJitterDeg);
    rng_ = &rng;

    const CharacterDef* defs[2] = {&left, &right};

    for (int i = 0; i < 2; ++i) {
        const CharacterDef& def = *defs[i];
        const SpawnDef& spawn = map.spawns[static_cast<std::size_t>(i)];

        Vec2 direction = normalized(spawn.dir);
        if (lengthSq(direction) < 0.5f) {
            direction = Vec2{1.0f, 0.0f};
        }
        // 防僵局 L0：初始方向抖 ±4°，避免两球走平行线
        direction = rotated(direction, degToRad(rng.range(-4.0f, 4.0f)));

        Body body;
        body.pos = spawn.pos;
        body.vel = direction * def.stats.startSpeed;
        body.radius = def.hitbox.radius;
        body.mass = def.stats.mass;
        body.restitution = def.stats.restitution;
        body.ghost = def.hitbox.ghost;

        const int bodyId = world_.addBody(body);

        Fighter& fighter = fighters_[i];
        fighter = Fighter{};
        fighter.bodyId = bodyId;
        fighter.def = &def;
        fighter.hp = def.stats.maxHp;
        fighter.maxHp = def.stats.maxHp;
        fighter.alive = true;

        std::vector<const SkillDef*> skillDefs;
        for (const std::string& skillId : def.skills) {
            const SkillDef* skill = pack.findSkill(skillId);
            if (skill != nullptr) {
                skillDefs.push_back(skill);
            } else {
                log::warn("角色 '" + def.id + "' 引用了不存在的技能: " + skillId);
            }
        }
        skills_[i].attach(bodyId, skillDefs);

        // 被动技能在开局读一次：贯穿、引力井都靠这几个数，之后每步只看状态
        for (const SkillDef* skill : skillDefs) {
            if (skill == nullptr || skill->trigger.kind != TriggerKind::Passive) {
                continue;
            }
            for (const ModifierDef& modifier : skill->modifiers) {
                if (modifier.op != ModifierOp::Pierce) {
                    continue;
                }
                fighter.pierce.damage = modifier.value;
                fighter.pierce.minSpeed = modifier.minSpeed;
                fighter.pierce.push = modifier.push;
            }
            if (skill->gravity.enabled) {
                fighter.gravity.range = skill->gravity.range;
                fighter.gravity.rangeMax = skill->gravity.rangeMax;
                fighter.gravity.growthSeconds = skill->gravity.growthSeconds;
                fighter.gravity.accel = skill->gravity.accel;
                fighter.gravity.coreFactor = skill->gravity.coreFactor;
                fighter.gravity.damage = skill->gravity.damage;
                fighter.gravity.damageMax = skill->gravity.damageMax;
                fighter.gravity.dpsEdge = skill->gravity.dpsEdge;
                fighter.gravity.dpsCenter = skill->gravity.dpsCenter;
                fighter.gravity.tickInterval = skill->gravity.tickInterval;
                fighter.gravity.teleportGuard = skill->gravity.teleportGuard;
                fighter.gravity.teleportMinDist = skill->gravity.teleportMinDist;
            }
        }
    }

    elapsed_ = 0.0f;
    hitCount_ = 0;
    phase_ = MatchPhase::Idle;
    result_ = MatchResult{};
    if (rule_) {
        rule_->reset();
    }
}

void Match::start() {
    if (phase_ != MatchPhase::Fighting) {
        phase_ = MatchPhase::Fighting;
        elapsed_ = 0.0f;
    }
}

void Match::reset() {
    world_.clear();
    skills_[0].reset();
    skills_[1].reset();
    stall_.reset();
    elapsed_ = 0.0f;
    phase_ = MatchPhase::Idle;
    result_ = MatchResult{};
    for (Fighter& fighter : fighters_) {
        fighter = Fighter{};
    }
    if (rule_) {
        rule_->reset();
    }
}

void Match::update(float dt) {
    if (phase_ != MatchPhase::Fighting) {
        return;
    }

    elapsed_ += dt;

    Body* bodies[2] = {world_.body(fighters_[0].bodyId), world_.body(fighters_[1].bodyId)};
    if (bodies[0] == nullptr || bodies[1] == nullptr) {
        return;
    }

    stall_.update(dt);

    if (stall_.lullSeconds() > stall_.params().lullStart) {
        bodies[0]->force += stall_.attraction(*bodies[0], *bodies[1]);
        bodies[1]->force += stall_.attraction(*bodies[1], *bodies[0]);
    }

    // 质量随时间成长：黑洞会越吃越重，吸力也按 mass / 起始质量 放大
    for (int i = 0; i < 2; ++i) {
        Fighter& fighter = fighters_[i];
        if (!fighter.alive || fighter.def == nullptr) {
            continue;
        }
        const StatsDef& stats = fighter.def->stats;
        if (stats.massMax <= stats.mass) {
            continue;
        }
        const float t = stats.massRamp > 0.01f ? clampf(elapsed_ / stats.massRamp, 0.0f, 1.0f) : 1.0f;
        Body& body = *bodies[i];
        body.mass = stats.mass + (stats.massMax - stats.mass) * t;
        body.invMass = body.mass > 0.0f ? 1.0f / body.mass : 0.0f;
    }

    for (int i = 0; i < 2; ++i) {
        Fighter& fighter = fighters_[i];
        if (fighter.alive) {
            const float ratio = fighter.maxHp > 0.0f ? fighter.hp / fighter.maxHp : 0.0f;
            skills_[i].update(dt, ratio, *bodies[i], *bodies[1 - i], rngRef(), events_);
        }
        if (fighter.comboTimer > 0.0f) {
            fighter.comboTimer -= dt;
            if (fighter.comboTimer <= 0.0f) {
                fighter.comboTimer = 0.0f;
                fighter.combo = 0;
            }
        }
        if (fighter.coreTick > 0.0f) {
            fighter.coreTick = std::max(0.0f, fighter.coreTick - dt);
        }
        if (fighter.coreGuard > 0.0f) {
            fighter.coreGuard = std::max(0.0f, fighter.coreGuard - dt);
        }
    }

    // 万向天引：范围内持续吸力 + 场内掉血 + 到点爆炸。爆炸之后才进冷却（冷却在 SkillSystem 里设）。
    for (int i = 0; i < 2; ++i) {
        Body& self = *bodies[i];
        Body& enemy = *bodies[1 - i];

        PullField field;
        if (fighters_[i].alive && skills_[i].pullField(field)) {
            const Vec2 delta = self.pos - enemy.pos;
            const float dist = lengthOf(delta);
            if (dist > 1e-3f && dist <= field.radius) {
                // 质量越大吸得越狠
                float massScale = 1.0f;
                if (fighters_[i].def != nullptr && fighters_[i].def->stats.mass > 0.0f) {
                    massScale = self.mass / fighters_[i].def->stats.mass;
                }
                enemy.force += delta * (field.accel * massScale / dist);

                const float ratio = 1.0f - dist / field.radius;
                const float dps = field.dpsEdge + (field.dpsCenter - field.dpsEdge) * ratio;
                if (dps > 0.0f) {
                    applyDamage(fighters_[1 - i].bodyId, dps * dt, true);
                }
            }
        }

        float blastRadius = 0.0f;
        float blastDamage = 0.0f;
        if (skills_[i].consumeBlast(blastRadius, blastDamage)) {
            if (fighters_[1 - i].alive && lengthOf(enemy.pos - self.pos) <= blastRadius) {
                applyDamage(fighters_[1 - i].bodyId, blastDamage, true);
            }
            Event event;
            event.type = EventType::SkillExpire;
            event.a = fighters_[i].bodyId;
            event.pos = self.pos;
            event.value = blastRadius;
            events_.publish(event);
        }
    }

    // 引力井（被动）：把范围内的敌人往中心拉（保留切向速度 → 会绕着转），
    // 进到核心圈就掉血并把对方随机传走。大招期间被动失效。
    for (int i = 0; i < 2; ++i) {
        Fighter& me = fighters_[i];
        Fighter& foe = fighters_[1 - i];
        if (!me.alive || !foe.alive || me.gravity.range <= 0.0f || skills_[i].suppressesPassive()) {
            continue;
        }

        Body& self = *bodies[i];
        Body& enemy = *bodies[1 - i];
        const Vec2 delta = self.pos - enemy.pos;
        const float dist = lengthOf(delta);
        if (dist <= 1e-3f) {
            continue;
        }

        // 范围和伤害都按对局时间成长（同一个 growthSeconds）
        const float growth = growthOf(elapsed_, me.gravity.growthSeconds);
        const float range = me.gravity.range + (me.gravity.rangeMax - me.gravity.range) * growth;
        if (dist > range) {
            continue;
        }

        // 质量越大吸得越狠（body.mass 每步已经按时间更新过）
        float massScale = 1.0f;
        if (me.def != nullptr && me.def->stats.mass > 0.0f) {
            massScale = self.mass / me.def->stats.mass;
        }
        enemy.force += delta * (me.gravity.accel * massScale / dist);

        // 吸力场持续伤害：边缘少、中心多，按距离线性插值
        const float fieldDps =
            me.gravity.dpsEdge + (me.gravity.dpsCenter - me.gravity.dpsEdge) * (1.0f - dist / range);
        if (fieldDps > 0.0f) {
            applyDamage(foe.bodyId, fieldDps * dt, true);
        }

        const float core = me.gravity.coreFactor * self.radius;
        if (dist > core || me.coreTick > 0.0f || foe.coreGuard > 0.0f) {
            continue;
        }

        const float damage = me.gravity.damage + (me.gravity.damageMax - me.gravity.damage) * growth;
        me.coreTick = me.gravity.tickInterval;
        foe.coreGuard = me.gravity.teleportGuard;

        const Vec2 victimPos = enemy.pos;
        applyDamage(foe.bodyId, damage, true);
        if (foe.alive) {
            teleportRandom(1 - i, self.pos, me.gravity.teleportMinDist);
            Event event;
            event.type = EventType::SkillExpire;
            event.a = me.bodyId;
            event.pos = victimPos;
            event.value = 44.0f;
            events_.publish(event);
        }
    }

    // 贯穿是「被动」：速度够快才生效。原地蓄力时速度是 0，照常被弹开、照常被打断。
    for (int i = 0; i < 2; ++i) {
        const Fighter& fighter = fighters_[i];
        const bool phase = fighter.alive && fighter.pierce.push > 0.0f &&
                           lengthOf(bodies[i]->vel) >= fighter.pierce.minSpeed;
        bodies[i]->phasePush = phase ? fighter.pierce.push : 0.0f;
    }

    zone_.update(dt, stall_.lullSeconds());
    // 场地缩到装不下两个球：直接结算，HP 高者胜。到此为止不会再跑物理，
    // 所以不会出现「两个球被挤在同一条线上反复弹」的退化状态。
    const Rect field = arena_.bounds();
    const float need = bodies[0]->radius + bodies[1]->radius;
    if (field.w < need || field.h < need) {
        finishByHp();
        return;
    }

    FightStatus status[2];
    for (int i = 0; i < 2; ++i) {
        status[i].id = fighters_[i].bodyId;
        status[i].hp = fighters_[i].hp;
        status[i].maxHp = fighters_[i].maxHp;
        status[i].alive = fighters_[i].alive;
    }

    if (rule_) {
        const MatchResult result = rule_->evaluate(status[0], status[1], elapsed_);
        if (result.finished) {
            result_ = result;
            phase_ = MatchPhase::Finished;

            Event event;
            event.type = EventType::RoundEnd;
            event.a = result.winnerId;
            event.value = elapsed_;
            events_.publish(event);
        }
    }
}

void Match::onEvent(const Event& event) {
    if (event.type != EventType::Hit || phase_ != MatchPhase::Fighting) {
        return;
    }

    int attackerIndex = -1;
    int victimIndex = -1;
    for (int i = 0; i < 2; ++i) {
        if (fighters_[i].bodyId == event.a) {
            attackerIndex = i;
        }
        if (fighters_[i].bodyId == event.b) {
            victimIndex = i;
        }
    }
    if (attackerIndex < 0 || victimIndex < 0 || attackerIndex == victimIndex) {
        return;
    }

    Fighter& victim = fighters_[victimIndex];
    if (!victim.alive) {
        return;
    }

    stall_.onHit();
    hitCount_ += 1;

    // 冲刺撞击：冲出去的那一方只让对手吃一次额外伤害（与碰撞伤害的倍率无关）
    for (int i = 0; i < 2; ++i) {
        float ramScale = 0.0f;
        if (skills_[i].consumeRam(ramScale)) {
            applyDamage(fighters_[1 - i].bodyId, ramScale * event.value, true);
        }
    }

    // 贯穿伤害：物理层保证「刚撞上」那一步只发一次 Hit，所以一次穿过只刮一次
    for (int i = 0; i < 2; ++i) {
        const Body* body = world_.body(fighters_[i].bodyId);
        if (body == nullptr || body->phasePush <= 0.0f || fighters_[i].pierce.damage <= 0.0f) {
            continue;
        }
        applyDamage(fighters_[1 - i].bodyId, fighters_[i].pierce.damage, true);
    }

    // 撞上就算打断蓄力：不看这一下掉了多少血（碰撞伤害可能被数据关成 0）
    skills_[0].interruptCharge();
    skills_[1].interruptCharge();

    const float refSpeed = map_ != nullptr ? std::max(1.0f, map_->physics.refSpeed) : 400.0f;
    // 万向天引期间整对不吃撞击伤害（黑洞自己还有无敌兜底）
    const bool ultRunning = skills_[0].omnipullActive() || skills_[1].omnipullActive();

    // 撞一次各自结算：系数按**挨撞的人**算 —— hitbox.contact 是「我这具身体挨撞有多疼」，
    // 0 表示撞它不疼（也打不出反伤），地图的 damageScale 是全局底数。
    for (int i = 0; i < 2; ++i) {
        Fighter& fighter = fighters_[i];
        if (!fighter.alive) {
            continue;
        }
        if (fighter.comboTimer > 0.0f) {
            fighter.combo += 1;
        } else {
            fighter.combo = 1;
        }
        fighter.comboTimer = 3.0f;

        float multiplier = 1.0f + 0.15f * static_cast<float>(fighter.combo - 1);
        if (multiplier > 2.5f) {
            multiplier = 2.5f;
        }

        float contactScale = 0.0f;
        if (!ultRunning) {
            contactScale = map_ != nullptr ? map_->physics.damageScale : 1.0f;
            if (fighter.def != nullptr && fighter.def->hitbox.contact > contactScale) {
                contactScale = fighter.def->hitbox.contact;
            }
        }
        const float damage = 12.0f * contactScale * (event.value / refSpeed) * multiplier;

        if (rule_) {
            rule_->onHit(fighter.bodyId, fighters_[1 - i].bodyId, damage);
        }
        applyDamage(fighter.bodyId, damage, true);
    }
}

void Match::applyDamage(int victimBodyId, float amount, bool countsForCombo) {
    if (phase_ != MatchPhase::Fighting || amount <= 0.0f) {
        return;
    }

    for (int i = 0; i < 2; ++i) {
        Fighter& fighter = fighters_[i];
        if (fighter.bodyId != victimBodyId || !fighter.alive) {
            continue;
        }
        // 万向天引期间自己无敌：连缩圈伤害也不吃
        if (skills_[i].invulnerable()) {
            return;
        }

        float remaining = amount;
        if (fighter.shield > 0.0f) {
            const float absorbed = std::min(fighter.shield, remaining);
            fighter.shield -= absorbed;
            remaining -= absorbed;
        }
        if (remaining <= 0.0f) {
            return;
        }

        fighter.hp = std::max(0.0f, fighter.hp - remaining);
        if (!countsForCombo && rule_) {
            rule_->onEnvironmentDamage(victimBodyId, remaining);
        }

        if (fighter.hp <= 0.0f) {
            fighter.alive = false;
            if (Body* body = world_.body(fighter.bodyId)) {
                body->active = false;
            }

            Event event;
            event.type = EventType::Death;
            event.a = fighter.bodyId;
            event.value = elapsed_;
            events_.publish(event);

            log::info("fighter " + std::to_string(i) + " died at " +
                      std::to_string(static_cast<int>(elapsed_)) + "s");
        }
        return;
    }
}

void Match::teleportRandom(int victimIndex, Vec2 awayFrom, float minDist) {
    if (victimIndex < 0 || victimIndex > 1) {
        return;
    }
    Fighter& victim = fighters_[victimIndex];
    Body* body = world_.body(victim.bodyId);
    if (body == nullptr || !body->active) {
        return;
    }

    const Rect field = arena_.bounds();
    const std::vector<Obstacle>& obstacles = arena_.obstacles();
    Rng& rng = rngRef();

    // 拒绝采样：随机撒点，撞墙/压障碍/离黑洞太近就重撒。64 次都不行就原地不动。
    for (int attempt = 0; attempt < 64; ++attempt) {
        const Vec2 point{rng.range(field.left(), field.right()),
                         rng.range(field.top(), field.bottom())};
        if (!arena_.contains(point, body->radius)) {
            continue;
        }
        if (lengthOf(point - awayFrom) < minDist) {
            continue;
        }

        bool blocked = false;
        for (const Obstacle& obstacle : obstacles) {
            if (obstacle.shape == ObstacleShape::Circle) {
                blocked = lengthOf(point - obstacle.pos) < body->radius + obstacle.radius;
            } else {
                const Rect rect{obstacle.pos.x - obstacle.size.x * 0.5f,
                                obstacle.pos.y - obstacle.size.y * 0.5f,
                                obstacle.size.x,
                                obstacle.size.y};
                const Vec2 closest{clampf(point.x, rect.left(), rect.right()),
                                   clampf(point.y, rect.top(), rect.bottom())};
                blocked = lengthOf(point - closest) < body->radius;
            }
            if (blocked) {
                break;
            }
        }
        if (blocked) {
            continue;
        }

        body->pos = point;
        body->force = Vec2{0.0f, 0.0f};
        return;
    }
}

void Match::finishByHp() {
    if (phase_ != MatchPhase::Fighting) {
        return;
    }

    MatchResult result;
    result.finished = true;
    result.reason = "zone_zero";
    if (fighters_[0].hp > fighters_[1].hp) {
        result.winnerId = fighters_[0].bodyId;
    } else if (fighters_[1].hp > fighters_[0].hp) {
        result.winnerId = fighters_[1].bodyId;
    }

    result_ = result;
    phase_ = MatchPhase::Finished;

    Event event;
    event.type = EventType::RoundEnd;
    event.a = result.winnerId;
    event.value = elapsed_;
    events_.publish(event);
}

MatchPhase Match::phase() const { return phase_; }
int Match::hitCount() const { return hitCount_; }

const Fighter& Match::fighter(int index) const {
    static const Fighter kEmpty{};
    if (index < 0 || index > 1) {
        return kEmpty;
    }
    return fighters_[index];
}

const MatchResult& Match::result() const { return result_; }

float Match::elapsed() const { return elapsed_; }

StallGuard& Match::stallGuard() { return stall_; }
const StallGuard& Match::stallGuard() const { return stall_; }
ZoneController& Match::zone() { return zone_; }

const SkillSystem& Match::skills(int index) const {
    static const SkillSystem kEmpty{};
    if (index < 0 || index > 1) {
        return kEmpty;
    }
    return skills_[index];
}

} // namespace orbduel
