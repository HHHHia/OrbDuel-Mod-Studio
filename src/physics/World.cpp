#include "orbduel/physics/World.hpp"

#include <algorithm>
#include <cmath>

#include "orbduel/core/Event.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/physics/Arena.hpp"
#include "orbduel/physics/Collision.hpp"

namespace orbduel {

namespace {

Vec2 applyJitter(Vec2 velocity, float degrees, Rng *rng) {
    if (rng == nullptr || degrees <= 0.0f) {
        return velocity;
    }
    const float radians = degToRad(rng->range(-degrees, degrees));
    return rotated(velocity, radians);
}

} // namespace

World::World(EventBus &events, Arena &arena) : events_(events), arena_(arena) {}

void World::setRng(Rng &rng) {
    rng_ = &rng;
}

void World::setWallParams(float restitution, float jitterDeg) {
    wallRestitution_ = restitution;
    jitterDeg_ = jitterDeg;
}

int World::addBody(const Body &body) {
    Body copy = body;
    copy.id = body.id >= 0 ? body.id : reserveBodyId();
    nextId_ = std::max(nextId_, copy.id + 1);
    if (slots_.count(copy.id))
        return -1;
    copy.invMass = copy.mass > 0.0f ? 1.0f / copy.mass : 0.0f;
    slots_[copy.id] = bodies_.size();
    bodies_.push_back(copy);
    return copy.id;
}

int World::reserveBodyId() {
    return nextId_++;
}

void World::removeBody(int id) {
    auto it = slots_.find(id);
    if (it == slots_.end())
        return;
    bodies_.erase(bodies_.begin() + it->second);
    slots_.erase(it);
    for (std::size_t i = 0; i < bodies_.size(); ++i)
        slots_[bodies_[i].id] = i;
    for (auto contact = phaseContacts_.begin(); contact != phaseContacts_.end();)
        if (int(*contact >> 32) == id || int(*contact & 0xffffffffu) == id)
            contact = phaseContacts_.erase(contact);
        else
            ++contact;
}

void World::clear() {
    bodies_.clear();
    slots_.clear();
    nextId_ = 0;
    phaseContacts_.clear();
}

Body *World::body(int id) {
    auto it = slots_.find(id);
    return it == slots_.end() ? nullptr : &bodies_[it->second];
}

const Body *World::body(int id) const {
    auto it = slots_.find(id);
    return it == slots_.end() ? nullptr : &bodies_[it->second];
}

std::vector<Body> &World::bodies() {
    return bodies_;
}
const std::vector<Body> &World::bodies() const {
    return bodies_;
}

void World::step(float dt) {
    const Rect bounds = arena_.bounds();
    const float maxSpeed = arena_.maxSpeed();

    // 1) 外力（加速度）→ 速度 → 位置
    for (Body &b : bodies_) {
        if (!b.active) {
            continue;
        }
        b.vel += b.force * dt;
        b.force = Vec2{};
        const float speed = lengthOf(b.vel);
        if (speed > maxSpeed && speed > 0.0f) {
            b.vel *= maxSpeed / speed;
        }
        b.pos += b.vel * dt;
    }

    // 2) 球-球
    std::set<std::uint64_t> phaseContacts;
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            Body &a = bodies_[i];
            Body &b = bodies_[j];
            if (!a.active || !b.active) {
                continue;
            }
            const Contact contact = collideCircles(a, b);
            if (!contact.hit) {
                continue;
            }
            const float closing = contact.relSpeed;
            const float speedA = std::max(0.f, dot(a.vel, contact.normal));
            const float speedB = std::max(0.f, -dot(b.vel, contact.normal));

            // 贯穿体：不分离、不换速度；对方只在「刚撞上」的那一步被推开一次。
            // 重叠期间会连续命中同一个配对，所以用 phaseContacts_ 记住「这一趟已经算过」。
            const bool aPhases = a.ghost || a.phasePush > 0.0f;
            const bool bPhases = b.ghost || b.phasePush > 0.0f;
            if (aPhases || bPhases) {
                const int low = std::min(a.id, b.id);
                const int high = std::max(a.id, b.id);
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(low) << 32) | static_cast<std::uint32_t>(high);
                const bool firstTouch = phaseContacts_.count(key) == 0;
                phaseContacts.insert(key);
                if (!firstTouch || closing <= 0.0f) {
                    continue;
                }
                // 黑洞（ghost）：谁都不弹、谁都不推，只是穿过去
                if (a.ghost || b.ghost) {
                    // 不推
                } else if (aPhases && !bPhases) {
                    b.vel += contact.normal * a.phasePush;
                } else if (bPhases && !aPhases) {
                    a.vel -= contact.normal * b.phasePush;
                }
            } else {
                resolveCircles(a, b, contact);
                if (closing <= 0.f)
                    continue;
            }

            Event event;
            event.type = EventType::Hit;
            event.a = a.id;
            event.b = b.id;
            event.pos = contact.point;
            event.normal = contact.normal;
            event.value = closing > 0.0f ? closing : 0.0f;
            event.attackSpeedA = speedA;
            event.attackSpeedB = speedB;
            events_.publish(event);
        }
    }
    phaseContacts_.swap(phaseContacts);

    // 3) 障碍
    for (Body &b : bodies_) {
        if (!b.active) {
            continue;
        }
        for (const Obstacle &obstacle : arena_.obstacles()) {
            Contact contact;
            bool hit = false;

            if (obstacle.shape == ObstacleShape::Rect) {
                Rect rect;
                rect.x = obstacle.pos.x - obstacle.size.x * 0.5f;
                rect.y = obstacle.pos.y - obstacle.size.y * 0.5f;
                rect.w = obstacle.size.x;
                rect.h = obstacle.size.y;
                hit = collideCircleRect(b, rect, contact);
                if (hit) {
                    resolveCircleRect(b, rect, contact, obstacle.restitution);
                }
            } else {
                hit = collideCircleStatic(b, obstacle.pos, obstacle.radius, contact);
                if (hit) {
                    resolveCircleStatic(b, obstacle.pos, obstacle.radius, contact, obstacle.restitution);
                }
            }

            if (hit) {
                Event event;
                event.type = EventType::WallBounce;
                event.a = b.id;
                event.pos = contact.point;
                event.normal = contact.normal;
                event.value = lengthOf(b.vel);
                events_.publish(event);
            }
        }
    }

    // 4) 场地边界
    for (Body &b : bodies_) {
        if (!b.active) {
            continue;
        }

        bool bounced = false;
        Vec2 normal{0.0f, 0.0f};

        if (b.pos.x - b.radius < bounds.left()) {
            b.pos.x = bounds.left() + b.radius;
            if (b.vel.x < 0.0f) {
                b.vel.x = -b.vel.x * std::min(1.f, wallRestitution_ + b.wallBoost);
                bounced = true;
                normal = Vec2{1.0f, 0.0f};
            }
        }
        if (b.pos.x + b.radius > bounds.right()) {
            b.pos.x = bounds.right() - b.radius;
            if (b.vel.x > 0.0f) {
                b.vel.x = -b.vel.x * std::min(1.f, wallRestitution_ + b.wallBoost);
                bounced = true;
                normal = Vec2{-1.0f, 0.0f};
            }
        }
        if (b.pos.y - b.radius < bounds.top()) {
            b.pos.y = bounds.top() + b.radius;
            if (b.vel.y < 0.0f) {
                b.vel.y = -b.vel.y * std::min(1.f, wallRestitution_ + b.wallBoost);
                bounced = true;
                normal = Vec2{0.0f, 1.0f};
            }
        }
        if (b.pos.y + b.radius > bounds.bottom()) {
            b.pos.y = bounds.bottom() - b.radius;
            if (b.vel.y > 0.0f) {
                b.vel.y = -b.vel.y * std::min(1.f, wallRestitution_ + b.wallBoost);
                bounced = true;
                normal = Vec2{0.0f, -1.0f};
            }
        }

        if (bounced) {
            b.vel = applyJitter(b.vel, jitterDeg_, rng_);
            Event event;
            event.type = EventType::WallBounce;
            event.a = b.id;
            event.pos = b.pos;
            event.normal = normal;
            event.value = lengthOf(b.vel);
            events_.publish(event);
        }
    }
}

} // namespace orbduel
