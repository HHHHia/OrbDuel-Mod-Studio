#include "orbduel/physics/Collision.hpp"

#include <algorithm>
#include <cmath>

namespace orbduel {

Contact collideCircles(const Body& a, const Body& b) {
    Contact contact;

    const Vec2 delta = b.pos - a.pos;
    const float radiusSum = a.radius + b.radius;
    const float distSq = lengthSq(delta);
    if (distSq > radiusSum * radiusSum) {
        return contact;
    }

    const float dist = std::sqrt(distSq);
    Vec2 normal{1.0f, 0.0f};
    if (dist > 1e-5f) {
        normal = delta / dist;
    }

    contact.hit = true;
    contact.normal = normal;
    contact.depth = radiusSum - dist;
    contact.point = a.pos + normal * (a.radius - contact.depth * 0.5f);

    // 沿法线的接近速度：正数表示正在靠近
    contact.relSpeed = -dot(b.vel - a.vel, normal);
    return contact;
}

bool collideCircleRect(const Body& body, const Rect& rect, Contact& out) {
    out = Contact{};

    const Vec2 closest{
        clampf(body.pos.x, rect.left(), rect.right()),
        clampf(body.pos.y, rect.top(), rect.bottom())};

    const Vec2 delta = body.pos - closest;
    const float distSq = lengthSq(delta);

    if (distSq > body.radius * body.radius) {
        return false;
    }

    const float dist = std::sqrt(distSq);
    Vec2 normal{0.0f, 0.0f};
    float depth = 0.0f;

    if (dist > 1e-5f) {
        normal = delta / dist;
        depth = body.radius - dist;
    } else {
        // 圆心落在矩形内部：沿最浅的一条边推出去
        const float toLeft = body.pos.x - rect.left();
        const float toRight = rect.right() - body.pos.x;
        const float toTop = body.pos.y - rect.top();
        const float toBottom = rect.bottom() - body.pos.y;
        float best = toLeft;
        normal = Vec2{-1.0f, 0.0f};
        if (toRight < best) { best = toRight; normal = Vec2{1.0f, 0.0f}; }
        if (toTop < best) { best = toTop; normal = Vec2{0.0f, -1.0f}; }
        if (toBottom < best) { best = toBottom; normal = Vec2{0.0f, 1.0f}; }
        depth = body.radius + best;
    }

    out.hit = true;
    out.normal = normal;
    out.depth = depth;
    out.point = closest;
    return true;
}

bool collideCircleStatic(const Body& body, Vec2 center, float radius, Contact& out) {
    out = Contact{};

    const Vec2 delta = body.pos - center;
    const float radiusSum = body.radius + radius;
    const float distSq = lengthSq(delta);
    if (distSq > radiusSum * radiusSum) {
        return false;
    }

    const float dist = std::sqrt(distSq);
    Vec2 normal{1.0f, 0.0f};
    if (dist > 1e-5f) {
        normal = delta / dist;
    }

    out.hit = true;
    out.normal = normal;
    out.depth = radiusSum - dist;
    out.point = center + normal * radius;
    return true;
}

void resolveCircles(Body& a, Body& b, const Contact& contact) {
    if (!contact.hit) {
        return;
    }

    const float invSum = a.invMass + b.invMass;
    if (invSum <= 0.0f) {
        return;
    }

    // 1) 位置分离：按质量比例各退一半
    const Vec2 correction = contact.normal * (contact.depth / invSum);
    a.pos -= correction * a.invMass;
    b.pos += correction * b.invMass;

    // 2) 速度响应：只处理正在靠近的情况
    const Vec2 relativeVelocity = b.vel - a.vel;
    const float velocityAlongNormal = dot(relativeVelocity, contact.normal);
    if (velocityAlongNormal > 0.0f) {
        return;
    }

    const float restitution = std::min(a.restitution, b.restitution);
    const float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal / invSum;
    const Vec2 impulse = contact.normal * impulseMagnitude;

    a.vel -= impulse * a.invMass;
    b.vel += impulse * b.invMass;
}

void resolveCircleRect(Body& body, const Rect& rect, const Contact& contact, float restitution) {
    (void)rect;
    if (!contact.hit) {
        return;
    }

    body.pos += contact.normal * contact.depth;

    const float velocityAlongNormal = dot(body.vel, contact.normal);
    if (velocityAlongNormal < 0.0f) {
        body.vel -= contact.normal * ((1.0f + restitution) * velocityAlongNormal);
    }
}

void resolveCircleStatic(Body& body, Vec2 center, float radius, const Contact& contact, float restitution) {
    (void)center;
    (void)radius;
    if (!contact.hit) {
        return;
    }

    body.pos += contact.normal * contact.depth;

    const float velocityAlongNormal = dot(body.vel, contact.normal);
    if (velocityAlongNormal < 0.0f) {
        body.vel -= contact.normal * ((1.0f + restitution) * velocityAlongNormal);
    }
}

} // namespace orbduel
