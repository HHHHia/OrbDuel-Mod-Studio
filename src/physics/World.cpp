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
    copy.id = static_cast<int>(bodies_.size());
    copy.invMass = copy.mass > 0.0f ? 1.0f / copy.mass : 0.0f;
    bodies_.push_back(copy);
    return copy.id;
}

void World::clear() {
    bodies_.clear();
}

Body *World::body(int id) {
    if (id < 0 || id >= static_cast<int>(bodies_.size())) {
        return nullptr;
    }
    return &bodies_[static_cast<std::size_t>(id)];
}

const Body *World::body(int id) const {
    if (id < 0 || id >= static_cast<int>(bodies_.size())) {
        return nullptr;
    }
    return &bodies_[static_cast<std::size_t>(id)];
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
            const float speedA = std::max(0.0f, dot(a.vel, contact.normal));
            const float speedB = std::max(0.0f, -dot(b.vel, contact.normal));
            resolveCircles(a, b, contact);

            // 分离中的接触不再次命中，避免贴合时刷伤害和触发器。
            if (closing <= 0.001f) {
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
        const float restitution = clampf(wallRestitution_ + b.wallBoost, 0, 1);
        Vec2 normal{0.0f, 0.0f};

        if (b.pos.x - b.radius < bounds.left()) {
            b.pos.x = bounds.left() + b.radius;
            if (b.vel.x < 0.0f) {
                b.vel.x = -b.vel.x * restitution;
                bounced = true;
                normal = Vec2{1.0f, 0.0f};
            }
        }
        if (b.pos.x + b.radius > bounds.right()) {
            b.pos.x = bounds.right() - b.radius;
            if (b.vel.x > 0.0f) {
                b.vel.x = -b.vel.x * restitution;
                bounced = true;
                normal = Vec2{-1.0f, 0.0f};
            }
        }
        if (b.pos.y - b.radius < bounds.top()) {
            b.pos.y = bounds.top() + b.radius;
            if (b.vel.y < 0.0f) {
                b.vel.y = -b.vel.y * restitution;
                bounced = true;
                normal = Vec2{0.0f, 1.0f};
            }
        }
        if (b.pos.y + b.radius > bounds.bottom()) {
            b.pos.y = bounds.bottom() - b.radius;
            if (b.vel.y > 0.0f) {
                b.vel.y = -b.vel.y * restitution;
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
