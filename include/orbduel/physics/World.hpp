#pragma once

// 物理世界：只负责「推进一个固定步」。
// 事件只入队不派发，派发由调用方在步末统一做（ADR-004）。

#include <cstdint>
#include <set>
#include <unordered_map>
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

    // ID 在本局内不复用；指定已预留 ID 或使用默认 -1 自动分配。
    int addBody(const Body &body);
    int reserveBodyId();
    // 仅在固定步边界调用；指针/枚举下标在增删后失效，ID 保持稳定。
    void removeBody(int id);

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
    std::unordered_map<int, std::size_t> slots_;
    int nextId_ = 0;
    Rng *rng_ = nullptr;
    float wallRestitution_ = 0.98f;
    float jitterDeg_ = 1.5f;
    // 上一步仍在重叠的贯穿配对：一次穿过只算一次（分开一步就清掉）
    std::set<std::uint64_t> phaseContacts_;
};

} // namespace orbduel
