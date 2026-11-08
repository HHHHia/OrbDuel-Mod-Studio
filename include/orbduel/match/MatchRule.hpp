#pragma once

// 对局规则接口（ADR-011）。默认 HpRule；以后加「限时得分」「三局两胜」
// 只需要新增一个实现，不改 Match。

#include <string>

namespace orbduel {

struct FightStatus {
    int id = -1;
    float hp = 1.0f;
    float maxHp = 1.0f;
    bool alive = true;
};

struct MatchResult {
    bool finished = false;
    int winnerId = -1;         // -1 表示平局
    std::string reason;        // 例如 "hp_zero" / "draw"
};

class IMatchRule {
public:
    virtual ~IMatchRule() = default;

    virtual void reset() = 0;

    // 只用于统计/计分；伤害本身已由 Match 扣过。
    virtual void onHit(int attackerId, int victimId, float damage) = 0;

    // 环境伤害（缩圈）单独走，不计连击。
    virtual void onEnvironmentDamage(int victimId, float amount) = 0;

    // 每个固定步调用一次。
    virtual MatchResult evaluate(const FightStatus& a,
                                 const FightStatus& b,
                                 float elapsedSeconds) const = 0;
};

// 单局 HP 制：任一球 HP 归零即结束；同一帧双双归零算平局。
class HpRule : public IMatchRule {
public:
    HpRule() = default;

    void reset() override;
    void onHit(int attackerId, int victimId, float damage) override;
    void onEnvironmentDamage(int victimId, float amount) override;
    MatchResult evaluate(const FightStatus& a,
                         const FightStatus& b,
                         float elapsedSeconds) const override;

private:
    float totalDamage_ = 0.0f;
    float environmentDamage_ = 0.0f;
    int hitCount_ = 0;
};

} // namespace orbduel
