#pragma once

// 一局比赛：把 CharacterDef 翻译成 Body + Fighter，结算伤害/连击/胜负，
// 调度 StallGuard 与 Zone。
//
// 不变量（match 契约）：
//   1. HP 只存在 Fighter 里，physics::Body 没有 hp 字段
//   2. 扣血唯一入口是 applyDamage()
//   3. 环境伤害不计连击
//   4. 对局结束后不再结算伤害

#include <memory>
#include <vector>

#include "orbduel/core/Types.hpp"
#include "orbduel/match/MatchRule.hpp"
#include "orbduel/match/Skill.hpp"
#include "orbduel/match/StallGuard.hpp"
#include "orbduel/match/Zone.hpp"

namespace orbduel {

class EventBus;
class World;
class Arena;
class Rng;
struct CharacterDef;
struct MapDef;
struct ContentPack;

enum class MatchPhase {
    Idle = 0,
    Fighting,
    Finished
};

struct Fighter {
    int bodyId = -1;
    const CharacterDef* def = nullptr;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float shield = 0.0f;
    int combo = 0;
    float comboTimer = 0.0f;
    bool alive = true;
};

class Match {
public:
    Match(EventBus& events, World& world, Arena& arena);
    ~Match() = default;

    Match(const Match&) = delete;
    Match& operator=(const Match&) = delete;

    void setRule(std::unique_ptr<IMatchRule> rule);

    // 建两个 Body、挂技能、重置 HP。rng 用于初始偏角抖动（防僵局）。
    void setup(const CharacterDef& left,
               const CharacterDef& right,
               const MapDef& map,
               const ContentPack& pack,
               Rng& rng);

    void start();
    void reset();

    // 每个固定步调用一次。
    void update(float dt);

    // 只处理本步派发的事件。
    void onEvent(const Event& event);

    // 唯一扣血入口。countsForCombo=false 时用于缩圈等环境伤害。
    void applyDamage(int victimBodyId, float amount, bool countsForCombo);

    MatchPhase phase() const;
    int hitCount() const;
    const Fighter& fighter(int index) const;
    const MatchResult& result() const;
    float elapsed() const;

    StallGuard& stallGuard();
    ZoneController& zone();
    const StallGuard& stallGuard() const;
    const SkillSystem& skills(int index) const;

private:
    // setup() 一定先于 update() 调用；这里只兜住「没给 rng」的情况，避免解空指针。
    Rng& rngRef();

    // 场地缩到装不下两个球时的结算：HP 高者胜，同 HP 平局。
    void finishByHp();

    EventBus& events_;
    World& world_;
    Arena& arena_;

    std::unique_ptr<IMatchRule> rule_;
    Fighter fighters_[2];
    SkillSystem skills_[2];
    StallGuard stall_;
    ZoneController zone_;
    Rng* rng_ = nullptr;

    MatchPhase phase_ = MatchPhase::Idle;
    MatchResult result_{};
    float elapsed_ = 0.0f;
    int hitCount_ = 0;
    const MapDef* map_ = nullptr;
};

} // namespace orbduel
