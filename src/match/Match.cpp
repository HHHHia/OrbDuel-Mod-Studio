#include "orbduel/match/Match.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

Match::Match(EventBus &events, World &world, Arena &arena)
    : events_(events), world_(world), arena_(arena), rule_(std::make_unique<HpRule>()),
      zone_(arena, ShrinkDef{}) {}

Rng &Match::rngRef() {
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

void Match::setup(const CharacterDef &left, const CharacterDef &right, const MapDef &map,
                  const ContentPack &pack, Rng &rng) {
    world_.clear();
    fighters_.assign(2, Fighter{});
    skills_.assign(2, SkillSystem{});
    ownedDefs_.clear();
    pack_ = &pack;
    stall_.reset();
    zone_.configure(map.shrink);

    map_ = &map;
    rng_ = &rng;

    const float wall = map.theme.wallThickness;
    Rect bounds;
    bounds.x = wall;
    bounds.y = wall;
    bounds.w = map.size.x - wall * 2.0f;
    bounds.h = map.size.y - wall * 2.0f;
    if (bounds.w < 64.0f) {
        bounds.w = 64.0f;
    }
    if (bounds.h < 64.0f) {
        bounds.h = 64.0f;
    }

    arena_.configure(bounds, map.physics.maxSpeed);
    arena_.clearObstacles();

    int obstacleId = 0;
    for (const ObstacleDef &def : map.obstacles) {
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

    const CharacterDef *defs[2] = {&left, &right};

    for (int i = 0; i < 2; ++i) {
        const CharacterDef &def = *defs[i];
        const SpawnDef &spawn = map.spawns[static_cast<std::size_t>(i)];

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

        Fighter &fighter = fighters_[i];
        fighter = Fighter{};
        fighter.bodyId = bodyId;
        fighter.team = i;
        fighter.primary = true;
        fighter.def = &def;
        fighter.hp = def.stats.maxHp;
        fighter.maxHp = def.stats.maxHp;
        fighter.alive = true;

        attachSkills(i);
    }

    elapsed_ = 0.0f;
    hitCount_ = 0;
    zone_.configure(map.shrink);
    phase_ = MatchPhase::Idle;
    result_ = MatchResult{};
    if (rule_) {
        rule_->reset();
    }
}

void Match::attachSkills(int i) {
    Fighter &fighter = fighters_[i];
    const CharacterDef &def = *fighter.def;
    std::vector<const SkillDef *> skillDefs;
    for (const std::string &skillId : def.skills) {
        const SkillDef *skill = pack_->findSkill(skillId);
        if (skill != nullptr) {
            skillDefs.push_back(skill);
        } else {
            log::warn("角色 '" + def.id + "' 引用了不存在的技能: " + skillId);
        }
    }
    skills_[i].attach(fighter.bodyId, skillDefs, this, &world_, rng_);

    // 被动技能在开局读一次：贯穿、引力井都靠这几个数，之后每步只看状态
    for (const SkillDef *skill : skillDefs) {
        if (skill == nullptr || skill->trigger.kind != TriggerKind::Passive) {
            continue;
        }
        for (const ModifierDef &modifier : skill->modifiers) {
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

int Match::fighterCount() const {
    return static_cast<int>(fighters_.size());
}
const Fighter *Match::fighterById(int id) const {
    for (const auto &f : fighters_)
        if (f.bodyId == id)
            return &f;
    return nullptr;
}
bool Match::isEnemy(int a, int b) const {
    const auto *left = fighterById(a), *right = fighterById(b);
    return left && right && a != b && (left->team < 0 || right->team < 0 || left->team != right->team);
}
int Match::addCombatant(int id, const CharacterDef &def, Vec2 pos, Vec2 velocity, int team, int owner,
                        float lifetime) {
    if (phase_ != MatchPhase::Fighting || fighterById(id))
        return -1;
    auto &stored = ownedDefs_.emplace(id, def).first->second;
    Body body;
    body.id = id;
    body.pos = pos;
    body.vel = velocity;
    body.radius = stored.hitbox.radius;
    body.mass = stored.stats.mass;
    body.restitution = stored.stats.restitution;
    body.ghost = stored.hitbox.ghost;
    if (world_.addBody(body) < 0) {
        ownedDefs_.erase(id);
        return -1;
    }
    arena_.resolveOverlaps(*world_.body(id), 1.f);
    Fighter f;
    f.bodyId = id;
    f.def = &stored;
    f.hp = f.maxHp = stored.stats.maxHp;
    f.team = team;
    f.owner = owner;
    f.lifetime = lifetime;
    f.expires = lifetime > 0;
    fighters_.push_back(f);
    skills_.emplace_back();
    attachSkills(fighterCount() - 1);
    return id;
}
void Match::removeCombatant(int id) {
    for (int i = 2; i < fighterCount(); ++i)
        if (fighters_[i].bodyId == id) {
            skills_.erase(skills_.begin() + i);
            fighters_.erase(fighters_.begin() + i);
            world_.removeBody(id);
            ownedDefs_.erase(id);
            return;
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
    skills_.assign(2, SkillSystem{});
    fighters_.assign(2, Fighter{});
    ownedDefs_.clear();
    stall_.reset();
    elapsed_ = 0.0f;
    phase_ = MatchPhase::Idle;
    result_ = MatchResult{};
    for (Fighter &fighter : fighters_) {
        fighter = Fighter{};
    }
    if (rule_) {
        rule_->reset();
    }
}

void Match::update(float dt) {
    if (phase_ != MatchPhase::Fighting)
        return;
    elapsed_ += dt;
    Body *left = world_.body(fighters_[0].bodyId), *right = world_.body(fighters_[1].bodyId);
    if (!left || !right)
        return;
    stall_.update(dt);
    if (stallEnabled_ && stall_.lullSeconds() > stall_.params().lullStart) {
        left->force += stall_.attraction(*left, *right);
        right->force += stall_.attraction(*right, *left);
    }
    // 所有战斗实体共享质量成长、护盾、连击与技能计时。
    for (int i = 0; i < fighterCount(); ++i) {
        auto &f = fighters_[i];
        auto *body = world_.body(f.bodyId);
        if (!body || !f.alive || !f.def)
            continue;
        if (f.expires)
            f.lifetime = std::max(0.f, f.lifetime - dt);
        const auto &stats = f.def->stats;
        body->mass = stats.mass;
        if (stats.massMax > stats.mass)
            body->mass += (stats.massMax - stats.mass) * growthOf(elapsed_, stats.massRamp);
        body->invMass = body->mass > 0 ? 1.f / body->mass : 0;
        f.coreTick = std::max(0.f, f.coreTick - dt);
        f.coreGuard = std::max(0.f, f.coreGuard - dt);
        if (f.shieldTime > 0 && (f.shieldTime -= dt) <= 0)
            f.shield = 0;
        if (f.comboTimer > 0 && (f.comboTimer -= dt) <= 0)
            f.combo = 0;
        // 单目标技能选择最近的存活敌人；范围技能在下方遍历所有敌人。
        Body *enemy = nullptr;
        float nearest = std::numeric_limits<float>::infinity();
        for (const auto &other : fighters_) {
            const auto *b = world_.body(other.bodyId);
            if (other.alive && b && isEnemy(f.bodyId, other.bodyId)) {
                float d = lengthSq(b->pos - body->pos);
                if (d < nearest) {
                    nearest = d;
                    enemy = world_.body(other.bodyId);
                }
            }
        }
        skills_[i].update(dt, f.maxHp > 0 ? f.hp / f.maxHp : 0, *body, enemy ? *enemy : *body, rngRef(),
                          events_);
    }
    // 万向天引与引力井保留原有成长、免伤、核心传送和冷却规则。
    for (int i = 0; i < fighterCount(); ++i) {
        auto &me = fighters_[i];
        auto *self = world_.body(me.bodyId);
        if (!me.alive || !self)
            continue;
        PullField field;
        bool pulling = skills_[i].pullField(field);
        float blastRadius = 0, blastDamage = 0;
        bool blast = skills_[i].consumeBlast(blastRadius, blastDamage);
        for (int j = 0; j < fighterCount(); ++j) {
            auto &foe = fighters_[j];
            auto *enemy = world_.body(foe.bodyId);
            if (!foe.alive || !enemy || !isEnemy(me.bodyId, foe.bodyId))
                continue;
            Vec2 delta = self->pos - enemy->pos;
            float dist = lengthOf(delta);
            float massScale = me.def->stats.mass > 0 ? self->mass / me.def->stats.mass : 1;
            if (pulling && dist > 1e-3f && dist <= field.radius) {
                enemy->force += delta * (field.accel * massScale / dist);
                float dps = field.dpsEdge + (field.dpsCenter - field.dpsEdge) * (1 - dist / field.radius);
                if (dps > 0)
                    damage(me.bodyId, foe.bodyId, dps * dt, "omnipull");
            }
            if (blast && dist <= blastRadius)
                damage(me.bodyId, foe.bodyId, blastDamage, "omnipull");
            if (!foe.alive || me.gravity.range <= 0 || skills_[i].suppressesPassive() || dist <= 1e-3f)
                continue;
            float growth = growthOf(elapsed_, me.gravity.growthSeconds);
            float range = me.gravity.range + (me.gravity.rangeMax - me.gravity.range) * growth;
            if (dist > range)
                continue;
            enemy->force += delta * (me.gravity.accel * massScale / dist);
            float dps = me.gravity.dpsEdge + (me.gravity.dpsCenter - me.gravity.dpsEdge) * (1 - dist / range);
            if (dps > 0)
                damage(me.bodyId, foe.bodyId, dps * dt, "gravity");
            if (!foe.alive || dist > me.gravity.coreFactor * self->radius || me.coreTick > 0 ||
                foe.coreGuard > 0)
                continue;
            me.coreTick = me.gravity.tickInterval;
            foe.coreGuard = me.gravity.teleportGuard;
            Vec2 victimPos = enemy->pos;
            damage(me.bodyId, foe.bodyId,
                   me.gravity.damage + (me.gravity.damageMax - me.gravity.damage) * growth, "gravity-core");
            if (foe.alive) {
                teleportRandom(j, self->pos, me.gravity.teleportMinDist);
                Event event;
                event.type = EventType::SkillExpire;
                event.a = me.bodyId;
                event.pos = victimPos;
                event.value = 44;
                events_.publish(event);
            }
        }
        if (blast) {
            Event event;
            event.type = EventType::SkillExpire;
            event.a = me.bodyId;
            event.pos = self->pos;
            event.value = blastRadius;
            events_.publish(event);
        }
    }
    for (const auto &f : fighters_)
        if (auto *b = world_.body(f.bodyId))
            b->phasePush =
                f.alive && f.pierce.push > 0 && lengthOf(b->vel) >= f.pierce.minSpeed ? f.pierce.push : 0;
    if (shrinkEnabled_)
        zone_.update(dt, stall_.lullSeconds());
}

void Match::configure(float seconds, bool shrink, bool stall) {
    timeLimit_ = seconds;
    shrinkEnabled_ = shrink;
    stallEnabled_ = stall;
}

void Match::finishStep() {
    if (phase_ != MatchPhase::Fighting)
        return;
    const Body *a = world_.body(fighters_[0].bodyId);
    const Body *b = world_.body(fighters_[1].bodyId);
    const Rect field = arena_.bounds();
    if (shrinkEnabled_ && a && b && (field.w < a->radius + b->radius || field.h < a->radius + b->radius)) {
        finishByHp();
        return;
    }
    evaluate();
}

void Match::evaluate() {

    FightStatus status[2];
    for (int i = 0; i < 2; ++i) {
        status[i].id = fighters_[i].bodyId;
        status[i].hp = fighters_[i].hp;
        status[i].maxHp = fighters_[i].maxHp;
        status[i].alive = fighters_[i].alive;
    }

    if (rule_) {
        MatchResult result = rule_->evaluate(status[0], status[1], elapsed_);
        if (!result.finished && timeLimit_ > 0 && elapsed_ >= timeLimit_) {
            result.finished = true;
            result.reason = "time_limit";
            const float a = fighters_[0].hp / fighters_[0].maxHp;
            const float b = fighters_[1].hp / fighters_[1].maxHp;
            result.winnerId = std::abs(a - b) < 0.00001f ? -1 : fighters_[a > b ? 0 : 1].bodyId;
        }
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

void Match::onEvent(const Event &event) {
    if (phase_ != MatchPhase::Fighting)
        return;
    // 友方仍产生物理接触，但不消耗重击、打断蓄力或触发命中技能。
    if (event.type == EventType::Hit && !isEnemy(event.a, event.b))
        return;
    for (int i = 0; i < fighterCount(); ++i)
        if (fighters_[i].alive)
            if (auto *b = world_.body(fighters_[i].bodyId))
                skills_[i].onEvent(event, *b);
    if (event.type != EventType::Hit)
        return;
    int pair[2] = {-1, -1};
    for (int i = 0; i < fighterCount(); ++i) {
        if (fighters_[i].bodyId == event.a)
            pair[0] = i;
        if (fighters_[i].bodyId == event.b)
            pair[1] = i;
    }
    if (pair[0] < 0 || pair[1] < 0 || !fighters_[pair[0]].alive || !fighters_[pair[1]].alive)
        return;
    stall_.onHit();
    ++hitCount_;
    for (int k = 0; k < 2; ++k) {
        int i = pair[k], j = pair[1 - k];
        float ramScale = 0;
        if (skills_[i].consumeRam(ramScale))
            damage(fighters_[i].bodyId, fighters_[j].bodyId, ramScale * event.value, "ram");
        const auto *b = world_.body(fighters_[i].bodyId);
        if (b && b->phasePush > 0 && fighters_[i].pierce.damage > 0)
            damage(fighters_[i].bodyId, fighters_[j].bodyId, fighters_[i].pierce.damage, "pierce");
        skills_[i].interruptCharge();
    }
    const float refSpeed = map_ ? std::max(1.f, map_->physics.refSpeed) : 400.f;
    bool ultRunning = skills_[pair[0]].omnipullActive() || skills_[pair[1]].omnipullActive();
    float amounts[2]{};
    // 仅计算本次接触的两名实体；双方数额先算好再结算。
    for (int k = 0; k < 2; ++k) {
        auto &victim = fighters_[pair[k]];
        if (!victim.alive)
            continue;
        victim.combo = victim.comboTimer > 0 ? victim.combo + 1 : 1;
        victim.comboTimer = 3;
        float combo = std::min(2.5f, 1.f + .15f * (victim.combo - 1));
        float scale =
            ultRunning ? 0 : std::max(map_ ? map_->physics.damageScale : 1.f, victim.def->hitbox.contact);
        float raw = fighters_[pair[1 - k]].def->stats.attack * scale * event.value / refSpeed * combo *
                    skills_[pair[1 - k]].damageMultiplier();
        amounts[k] = std::max(0.f, raw - victim.def->stats.armor);
    }
    for (int k = 0; k < 2; ++k)
        damage(fighters_[pair[1 - k]].bodyId, fighters_[pair[k]].bodyId, amounts[k], "contact");
}

void Match::damage(int source, int target, float amount, const std::string &cause) {
    if (phase_ != MatchPhase::Fighting || !std::isfinite(amount) || amount <= 0)
        return;
    float before = 0, shieldBefore = 0;
    for (auto &f : fighters_)
        if (f.bodyId == target) {
            before = f.hp;
            shieldBefore = f.shield;
        }
    applyDamage(target, amount, cause != "environment");
    float dealt = 0, absorbed = 0;
    for (auto &f : fighters_)
        if (f.bodyId == target) {
            dealt = before - f.hp;
            absorbed = shieldBefore - f.shield;
            f.damageAbsorbed += absorbed;
        }
    for (auto &f : fighters_)
        if (f.bodyId == source)
            f.damageDealt += dealt;
    if (rule_ && cause != "environment")
        rule_->onHit(source, target, dealt);
    Event e;
    e.type = EventType::Damage;
    e.a = source;
    e.b = target;
    e.value = dealt;
    e.value2 = absorbed;
    e.tag = cause;
    events_.publish(e);
}
void Match::heal(int id, float amount) {
    if (phase_ != MatchPhase::Fighting || !std::isfinite(amount))
        return;
    for (auto &f : fighters_)
        if (f.bodyId == id && f.alive) {
            const float before = f.hp;
            f.hp = std::min(f.maxHp, f.hp + std::max(0.f, amount));
            f.healingDone += f.hp - before;
        }
}
void Match::giveShield(int id, float amount, float duration) {
    if (!std::isfinite(amount) || !std::isfinite(duration))
        return;
    for (auto &f : fighters_)
        if (f.bodyId == id && f.alive) {
            f.shield = std::max(f.shield, std::max(0.f, amount));
            f.shieldTime = std::max(0.f, duration);
        }
}

void Match::applyDamage(int victimBodyId, float amount, bool countsForCombo) {
    if (phase_ != MatchPhase::Fighting || amount <= 0.0f) {
        return;
    }

    for (int i = 0; i < fighterCount(); ++i) {
        Fighter &fighter = fighters_[i];
        if (fighter.bodyId != victimBodyId || !fighter.alive) {
            continue;
        }

        if (skills_[i].invulnerable())
            return;
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
            if (Body *body = world_.body(fighter.bodyId)) {
                body->active = false;
            }

            Event event;
            event.type = EventType::Death;
            event.a = fighter.bodyId;
            event.value = elapsed_;
            events_.publish(event);

            log::info("fighter " + std::to_string(fighter.bodyId) + " died at " +
                      std::to_string(static_cast<int>(elapsed_)) + "s");
        }
        return;
    }
}

void Match::teleportRandom(int victimIndex, Vec2 awayFrom, float minDist) {
    if (victimIndex < 0 || victimIndex >= fighterCount()) {
        return;
    }
    Fighter &victim = fighters_[victimIndex];
    Body *body = world_.body(victim.bodyId);
    if (body == nullptr || !body->active) {
        return;
    }

    const Rect field = arena_.bounds();
    const std::vector<Obstacle> &obstacles = arena_.obstacles();
    Rng &rng = rngRef();

    // 拒绝采样：随机撒点，撞墙/压障碍/离黑洞太近就重撒。64 次都不行就原地不动。
    for (int attempt = 0; attempt < 64; ++attempt) {
        const Vec2 point{rng.range(field.left(), field.right()), rng.range(field.top(), field.bottom())};
        if (!arena_.contains(point, body->radius)) {
            continue;
        }
        if (lengthOf(point - awayFrom) < minDist) {
            continue;
        }

        bool blocked = false;
        for (const Obstacle &obstacle : obstacles) {
            if (obstacle.shape == ObstacleShape::Circle) {
                blocked = lengthOf(point - obstacle.pos) < body->radius + obstacle.radius;
            } else {
                const Rect rect{obstacle.pos.x - obstacle.size.x * 0.5f,
                                obstacle.pos.y - obstacle.size.y * 0.5f, obstacle.size.x, obstacle.size.y};
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

MatchPhase Match::phase() const {
    return phase_;
}

int Match::hitCount() const {
    return hitCount_;
}

const Fighter &Match::fighter(int index) const {
    static const Fighter kEmpty{};
    if (index < 0 || index >= fighterCount()) {
        return kEmpty;
    }
    return fighters_[index];
}

const MatchResult &Match::result() const {
    return result_;
}

float Match::elapsed() const {
    return elapsed_;
}
const std::vector<SkillState> &Match::skillStates(int index) const {
    static const std::vector<SkillState> empty;
    return index >= 0 && index < fighterCount() ? skills_[index].states() : empty;
}

StallGuard &Match::stallGuard() {
    return stall_;
}
const StallGuard &Match::stallGuard() const {
    return stall_;
}
ZoneController &Match::zone() {
    return zone_;
}

const SkillSystem &Match::skills(int index) const {
    static const SkillSystem kEmpty{};
    if (index < 0 || index >= fighterCount()) {
        return kEmpty;
    }
    return skills_[index];
}

} // namespace orbduel
