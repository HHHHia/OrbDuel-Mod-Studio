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

Match::Match(EventBus &events, World &world, Arena &arena)
    : events_(events), world_(world), arena_(arena), rule_(std::make_unique<HpRule>()),
      zone_(arena, ShrinkDef{}) {}

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

        const int bodyId = world_.addBody(body);

        Fighter &fighter = fighters_[i];
        fighter = Fighter{};
        fighter.bodyId = bodyId;
        fighter.def = &def;
        fighter.hp = def.stats.maxHp;
        fighter.maxHp = def.stats.maxHp;
        fighter.alive = true;

        std::vector<const SkillDef *> skillDefs;
        for (const std::string &skillId : def.skills) {
            const SkillDef *skill = pack.findSkill(skillId);
            if (skill != nullptr) {
                skillDefs.push_back(skill);
            } else {
                log::warn("角色 '" + def.id + "' 引用了不存在的技能: " + skillId);
            }
        }
        skills_[i].attach(bodyId, skillDefs, this, &world_, &rng);
    }

    elapsed_ = 0.0f;
    hitCount_ = 0;
    ShrinkDef shrink = map.shrink;
    shrink.minInset = std::min(shrink.minInset, zone_.floorInset(left.hitbox.radius, right.hitbox.radius));
    zone_.configure(shrink);
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
    for (Fighter &fighter : fighters_) {
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

    Body *bodies[2] = {world_.body(fighters_[0].bodyId), world_.body(fighters_[1].bodyId)};
    if (bodies[0] == nullptr || bodies[1] == nullptr) {
        return;
    }

    stall_.update(dt);

    if (stallEnabled_ && stall_.lullSeconds() > stall_.params().lullStart) {
        bodies[0]->force += stall_.attraction(*bodies[0], *bodies[1]);
        bodies[1]->force += stall_.attraction(*bodies[1], *bodies[0]);
    }

    for (int i = 0; i < 2; ++i) {
        Fighter &fighter = fighters_[i];
        if (fighter.shieldTime > 0) {
            fighter.shieldTime -= dt;
            if (fighter.shieldTime <= 0)
                fighter.shield = 0;
        }
        if (fighter.alive) {
            const float ratio = fighter.maxHp > 0.0f ? fighter.hp / fighter.maxHp : 0.0f;
            skills_[i].update(dt, ratio, *bodies[i], events_);
        }
        if (fighter.comboTimer > 0.0f) {
            fighter.comboTimer -= dt;
            if (fighter.comboTimer <= 0.0f) {
                fighter.comboTimer = 0.0f;
                fighter.combo = 0;
            }
        }
    }

    if (shrinkEnabled_)
        zone_.update(dt, stall_.lullSeconds());
}

void Match::configure(float seconds, bool shrink, bool stall) {
    timeLimit_ = seconds;
    shrinkEnabled_ = shrink;
    stallEnabled_ = stall;
}

void Match::finishStep() {
    if (phase_ == MatchPhase::Fighting)
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
    // 内置技能也接收命中、受击和撞墙事件，参数均来自内容。
    for (int i = 0; i < 2; ++i) {
        if (auto *b = world_.body(fighters_[i].bodyId))
            skills_[i].onEvent(event, *b);
    }
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

    Fighter &victim = fighters_[victimIndex];
    if (!victim.alive) {
        return;
    }

    stall_.onHit();
    hitCount_ += 1;

    const float refSpeed = map_ != nullptr ? std::max(1.0f, map_->physics.refSpeed) : 400.0f;
    const float damageScale = map_ != nullptr ? map_->physics.damageScale : 1.0f;

    // 同一份碰撞前状态分别计算，之后统一结算；不存在同步的全局连击。
    float amounts[2]{};
    for (int i = 0; i < 2; ++i) {
        const float speed = fighters_[i].bodyId == event.a ? event.attackSpeedA : event.attackSpeedB;
        amounts[i] =
            std::max(0.0f, fighters_[i].def->stats.attack * damageScale * skills_[i].damageMultiplier() *
                                   clampf(0.5f + speed / refSpeed, 0.5f, 2.5f) -
                               fighters_[1 - i].def->stats.armor);
    }
    damage(fighters_[0].bodyId, fighters_[1].bodyId, amounts[0], "contact");
    damage(fighters_[1].bodyId, fighters_[0].bodyId, amounts[1], "contact");
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

    for (int i = 0; i < 2; ++i) {
        Fighter &fighter = fighters_[i];
        if (fighter.bodyId != victimBodyId || !fighter.alive) {
            continue;
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
            if (Body *body = world_.body(fighter.bodyId)) {
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

MatchPhase Match::phase() const {
    return phase_;
}

int Match::hitCount() const {
    return hitCount_;
}

const Fighter &Match::fighter(int index) const {
    static const Fighter kEmpty{};
    if (index < 0 || index > 1) {
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
    return index >= 0 && index < 2 ? skills_[index].states() : empty;
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

} // namespace orbduel
