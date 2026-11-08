#include "orbduel/match/MatchRule.hpp"

namespace orbduel {

void HpRule::reset() {
    totalDamage_ = 0.0f;
    environmentDamage_ = 0.0f;
    hitCount_ = 0;
}

void HpRule::onHit(int attackerId, int victimId, float damage) {
    (void)attackerId;
    (void)victimId;
    totalDamage_ += damage;
    ++hitCount_;
}

void HpRule::onEnvironmentDamage(int victimId, float amount) {
    (void)victimId;
    environmentDamage_ += amount;
}

MatchResult HpRule::evaluate(const FightStatus& a, const FightStatus& b,
                             float elapsedSeconds) const {
    (void)elapsedSeconds;

    MatchResult result;
    const bool aDead = !a.alive || a.hp <= 0.0f;
    const bool bDead = !b.alive || b.hp <= 0.0f;

    if (!aDead && !bDead) {
        return result;
    }

    result.finished = true;
    if (aDead && bDead) {
        result.winnerId = -1;
        result.reason = "draw";
    } else {
        result.winnerId = aDead ? b.id : a.id;
        result.reason = "hp_zero";
    }
    return result;
}

} // namespace orbduel
