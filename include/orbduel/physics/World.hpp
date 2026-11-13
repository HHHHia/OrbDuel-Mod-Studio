#pragma once

// 物理世界：只负责「推进一个固定步」。
// 事件只入队不派发，派发由调用方在步末统一做（ADR-004）。

#include <cstdint>
#include <set>
#include <vector>

#include "orbduel/core/Types.hpp"
#include "orbduel/physics/Body.hpp"

namespace orbduel {

class EventBus;
class Arena;
class Rng;

class World {
  public:
    World(EventBus &events, Arena &arena);
    ~World() = default;

    World(const World &) = delete;
    World &operator=(const World &) = delete;

    // 墙反弹抖动用它；不设置则不加抖动（不推荐）。
    void setRng(Rng &rng);
    void setWallParams(float restitution, float jitterDeg);

    // 返回新 body 的 id（当前实现里 id == 下标，不支持中途移除）。
    int addBody(const Body &body);

    // 清空所有 body（重开一局用）。
    void clear();

    Body *body(int id);
    const Body *body(int id) const;

    std::vector<Body> &bodies();
    const std::vector<Body> &bodies() const;

    // 顺序：force → 积分 → 球球 → 障碍 → 墙 → 清 force。
    // dt 由 Clock 提供，永远是固定值。
    void step(float dt);

  private:
    EventBus &events_;
    Arena &arena_;
    std::vector<Body> bodies_;
    Rng *rng_ = nullptr;
    float wallRestitution_ = 0.98f;
    float jitterDeg_ = 1.5f;
    // 上一步仍在重叠的贯穿配对：一次穿过只算一次（分开一步就清掉）
    std::set<std::uint64_t> phaseContacts_;
};

} // namespace orbduel
