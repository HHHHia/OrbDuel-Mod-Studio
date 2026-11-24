#pragma once
// C++ 作者辅助层：不增加宿主函数表字段，不改变已有二进制接口。
#include "orbduel_entities.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>
namespace orbmod {
struct Cooldown {
    float remaining = 0;
    void tick(float dt) {
        remaining = std::max(0.f, remaining - std::max(0.f, dt));
    }
    bool ready() const {
        return remaining <= 0;
    }
    void start(float seconds) {
        remaining = std::max(0.f, seconds);
    }
};
struct Interval {
    float elapsed = 0;
    bool tick(float dt, float period) {
        if (period <= 0)
            return false;
        elapsed += std::max(0.f, dt);
        if (elapsed < period)
            return false;
        elapsed = std::fmod(elapsed, period);
        return true;
    }
};
enum class Relation { Any, Allies, Enemies };
class Ability : public Behavior {
    struct ManagedResource {
        Id id;
        bool firstStep = true;
    };
    std::vector<ManagedResource> entities_, objects_, bindings_;

    template <class Exists> static void prune(std::vector<ManagedResource> &resources, Exists exists) {
        auto next = resources.begin();
        for (auto resource : resources) {
            if (resource.firstStep || exists(resource.id)) {
                resource.firstStep = false;
                *next++ = resource;
            }
        }
        resources.erase(next, resources.end());
    }

  public:
    const EntityApi *api;
    Ability(const Host *h, Id id, const EntityApi *extension) : Behavior(h, id), api(extension) {}
    Ability(const Ability &) = delete;
    Ability &operator=(const Ability &) = delete;
    ~Ability() {
        cleanup();
    }
    bool get(Id entityId, Entity &out) const {
        if (api)
            return api->get(host->user, entityId, &out) != 0;
        for (int i = 0; i < count(); ++i) {
            auto e = entity(i);
            if (e.id == entityId) {
                out = e;
                return true;
            }
        }
        return false;
    }
    Id ownerId() const {
        Object o{};
        return host->getObject(host->user, id, &o) ? o.owner : -1;
    }
    bool owner(Entity &out) const {
        return get(ownerId(), out) && out.alive;
    }
    bool info(Id entityId, CombatInfo &out) const {
        return api && api->info(host->user, entityId, &out);
    }
    bool related(Id source, Id target, Relation relation, bool includeSelf = false) const {
        if (source == target)
            return includeSelf && relation != Relation::Enemies;
        if (relation == Relation::Any)
            return true;
        CombatInfo a{}, b{};
        if (!info(source, a) || !info(target, b))
            return false;
        bool enemies = a.team < 0 || b.team < 0 || a.team != b.team;
        return relation == Relation::Enemies ? enemies : !enemies;
    }
    std::vector<Entity> within(Id source, Vec center, float radius, Relation relation = Relation::Any,
                               bool includeSelf = false) const {
        std::vector<Entity> result;
        if (radius < 0)
            return result;
        for (int i = 0; i < count(); ++i) {
            auto e = entity(i);
            if (e.alive && related(source, e.id, relation, includeSelf) && distance(center, e.pos) <= radius)
                result.push_back(e);
        }
        return result;
    }
    bool nearestEnemy(Id source, Entity &out, float radius = std::numeric_limits<float>::infinity()) const {
        Entity self{};
        if (!get(source, self) || !self.alive)
            return false;
        bool found = false;
        float best = radius;
        for (const auto &e : within(source, self.pos, radius, Relation::Enemies)) {
            float d = distance(self.pos, e.pos);
            if (!found || d < best || (d == best && e.id < out.id)) {
                out = e;
                best = d;
                found = true;
            }
        }
        return found;
    }
    void moveTowards(Id entityId, Vec point, float speed, float stopDistance = 0) const {
        Entity e{};
        if (!get(entityId, e) || !e.alive)
            return;
        float dist = distance(e.pos, point);
        Vec velocity{};
        if (dist > std::max(.001f, stopDistance)) {
            float factor = std::max(0.f, speed) / dist;
            velocity = {(point.x - e.pos.x) * factor, (point.y - e.pos.y) * factor};
        }
        host->setVelocity(host->user, entityId, velocity);
    }
    bool hurt(const Event &e) const {
        return ownerId() >= 0 && !std::strcmp(e.name, "damage") && e.b == ownerId() && e.value > 0;
    }
    bool hit(const Event &e) const {
        return ownerId() >= 0 && !std::strcmp(e.name, "hit") && (e.a == ownerId() || e.b == ownerId());
    }
    bool died(const Event &e) const {
        return ownerId() >= 0 && !std::strcmp(e.name, "death") && e.a == ownerId();
    }
    Id create(const EntitySpec &spec) {
        Id value = api ? api->create(host->user, &spec) : -1;
        if (value >= 0)
            entities_.push_back({value});
        return value;
    }
    Id bind(Id target, const char *type, const char *params = nullptr) {
        Id value = api ? api->bind(host->user, target, type, params) : -1;
        if (value >= 0)
            bindings_.push_back({value});
        return value;
    }
    Id spawn(const char *type, Vec pos, const char *params = nullptr) {
        Id value = host->spawn(host->user, type, pos, params);
        if (value >= 0)
            objects_.push_back({value});
        return value;
    }
    // 每次 step 开头调用一次。新请求保留到下一步，等待宿主提交。
    void beginStep() {
        prune(entities_, [&](Id value) {
            Entity e{};
            return get(value, e);
        });
        auto objectExists = [&](Id value) {
            Object o{};
            return host->getObject(host->user, value, &o) != 0;
        };
        prune(objects_, objectExists);
        prune(bindings_, objectExists);
    }
    // 仅清理由本实例创建/绑定的资源；宿主继续在安全边界提交增删。
    void cleanup() {
        if (api)
            for (const auto &resource : bindings_)
                api->unbind(host->user, resource.id);
        for (const auto &resource : objects_)
            host->remove(host->user, resource.id);
        if (api)
            for (const auto &resource : entities_)
                api->destroy(host->user, resource.id);
        bindings_.clear();
        objects_.clear();
        entities_.clear();
    }
};
} // namespace orbmod
