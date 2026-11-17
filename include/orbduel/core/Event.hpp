#pragma once

// 队列式事件总线（ADR-004、ADR-008）。
//
// 规则：
//   - publish() 只入队，不回调；
//   - dispatch() 在每个固定步末尾统一分发；
//   - 分发过程中再 publish 的事件留到下一次 dispatch，不递归。

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "orbduel/core/Types.hpp"

namespace orbduel {

enum class EventType {
    Hit = 0,         // a 打中 b；pos 接触点，normal 法线，value 相对速度，value2 伤害
    WallBounce,      // a 撞墙；pos 接触点，normal 法线
    ObstacleCrushed, // a 障碍 id；pos 位置
    SkillCast,       // a 施法者；value 技能在角色 skills[] 里的槽位
    ZoneShrink,      // value 新档位；pos 新边界中心
    SkillExpire,     // a 施法者；pos 效果中心，value 半径（爆炸/传送这类「到点」效果）
    Death,           // a 死亡者 id
    RoundEnd,        // a 胜者 id（-1 平局）
    Damage,          // a 来源；b 目标；value 实际伤害；tag 伤害原因
    EntitySpawned,   // a 实体；b 所有者；pos 出生位置
    EntityRemoved,   // a 实体；b 所有者；tag 清理原因
    Count
};

struct Event {
    EventType type = EventType::Hit;
    int a = -1;
    int b = -1;
    Vec2 pos{};
    Vec2 normal{};
    float value = 0.0f;
    float value2 = 0.0f;
    float attackSpeedA = 0.0f;
    float attackSpeedB = 0.0f;
    std::string tag;
};

using EventHandler = std::function<void(const Event &)>;

class EventBus {
  public:
    EventBus();
    ~EventBus();

    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

    void subscribe(EventType type, EventHandler handler);

    // 清空订阅者（不清理队列）。场景切换时用。
    void clearSubscribers();

    void publish(const Event &event);

    // 分发队列里的全部事件。可分发的数量有上限（1024/步），超出丢弃并记警告。
    void dispatch();

    std::size_t queuedCount() const;

  private:
    static constexpr std::size_t kTypeCount = static_cast<std::size_t>(EventType::Count);

    std::vector<EventHandler> handlers_[kTypeCount];
    std::vector<Event> queue_;
    std::vector<Event> dispatching_;
    bool dispatchingNow_ = false;
};

} // namespace orbduel
