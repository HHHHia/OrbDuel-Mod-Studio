#include "orbduel/core/Event.hpp"

namespace orbduel {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;

void EventBus::subscribe(EventType type, EventHandler handler) {
    const std::size_t index = static_cast<std::size_t>(type);
    if (index >= kTypeCount || !handler) {
        return;
    }
    handlers_[index].push_back(std::move(handler));
}

void EventBus::clearSubscribers() {
    for (auto& list : handlers_) {
        list.clear();
    }
}

void EventBus::publish(const Event& event) {
    // 队列上限，防止事件风暴把内存吃光
    constexpr std::size_t kMaxQueued = 1024;
    if (queue_.size() >= kMaxQueued) {
        return;
    }
    queue_.push_back(event);
}

void EventBus::dispatch() {
    if (dispatchingNow_) {
        return; // 回调里不要再 dispatch
    }
    dispatchingNow_ = true;
    dispatching_.swap(queue_);
    queue_.clear();

    for (const Event& event : dispatching_) {
        const std::size_t index = static_cast<std::size_t>(event.type);
        if (index >= kTypeCount) {
            continue;
        }
        for (const EventHandler& handler : handlers_[index]) {
            if (handler) {
                handler(event);
            }
        }
    }

    dispatching_.clear();
    dispatchingNow_ = false;
}

std::size_t EventBus::queuedCount() const { return queue_.size(); }

} // namespace orbduel
