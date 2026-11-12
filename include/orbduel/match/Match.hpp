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

// 贯穿（被动技能）：撞上不弹开、直接穿过并刮一次伤害；速度不够时退回普通碰撞。
struct PierceMod {
    float damage = 0.0f;    // 穿过一次掉多少血
    float minSpeed = 0.0f;  // 自身速度 ≥ 它才贯穿
    float push = 0.0f;      // 穿过时给对手加的速度（沿碰撞法线）
};

// 引力井（被动技能）的生效参数：数据在 SkillDef::gravity，这里只放运行时要用的副本。
struct GravityMod {
    float range = 0.0f;         // 0 = 没有引力井
    float accel = 0.0f;
    float coreFactor = 0.0f;
    float damage = 0.0f;
    float damageMax = 0.0f;
    float rampTime = 0.0f;
    float tickInterval = 0.0f;
    float teleportGuard = 0.0f;
    float teleportMinDist = 0.0f;
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
    PierceMod pierce{};
    GravityMod gravity{};
    float coreTick = 0.0f;    // 自己作为黑洞，距离下次核心伤害还要多久
    float coreGuard = 0.0f;   // 自己作为受害者，距离下次能被核心伤害/传送还要多久
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

    // 把某个 fighter 随机丢到一个「站得住」的位置：在场地内、不压障碍、离 awayFrom 至少
    // minDist。找不到就原地不动（宁可没传送，也不能塞进墙里）。
    void teleportRandom(int victimIndex, Vec2 awayFrom, float minDist);

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
