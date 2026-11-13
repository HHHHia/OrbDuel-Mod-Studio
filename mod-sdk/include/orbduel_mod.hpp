#pragma once
// 模组唯一需要的公共头。所有位置、速度使用世界坐标；回调在主线程执行。
#include <cmath>
#include <cstdint>

#if defined(_WIN32)
#define ORBDUEL_MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define ORBDUEL_MOD_EXPORT extern "C"
#endif

namespace orbmod {
using Id = int;
struct Vec {
    float x, y;
};
struct Color {
    unsigned char r, g, b, a;
};
struct Entity {
    Id id;
    Vec pos, velocity;
    float radius, hp, maxHp, shield;
    int alive;
};
struct Object {
    Id id;
    Vec pos, size;
    float radius, angle;
    int owner;
};
// name 为内置事件名或作者自定义名称；所有指针仅在本次回调期间有效。
struct Event {
    const char *name;
    Id a, b, object;
    Vec pos;
    float value;
    const char *cause;
};
struct Draw {
    void *user;
    void (*circle)(void *, Vec, float, Color, int filled);
    void (*line)(void *, Vec, Vec, Color, float thickness);
    void (*rect)(void *, Vec, Vec, Color, int filled);
    void (*sprite)(void *, const char *relativePath, Vec, Vec, float angle, Color);
};
struct Host {
    void *user;
    int (*entityCount)(void *);
    int (*entityAt)(void *, int index, Entity *);
    int (*getObject)(void *, Id, Object *);
    float (*number)(void *, Id, const char *key, float fallback);
    int (*text)(void *, Id, const char *key, char *out, int capacity);
    void (*setPosition)(void *, Id entity, Vec);
    void (*setVelocity)(void *, Id entity, Vec);
    void (*accelerate)(void *, Id entity, Vec acceleration);
    void (*damage)(void *, Id source, Id target, float amount, const char *cause);
    void (*heal)(void *, Id entity, float amount);
    void (*shield)(void *, Id entity, float amount, float seconds);
    Id (*spawn)(void *, const char *type, Vec pos, const char *paramsJson);
    void (*remove)(void *, Id object);
    void (*emit)(void *, const Event *);
    float (*random)(void *, float low, float high);
    float (*time)(void *);
    Color (*color)(void *, Id, const char *key, Color fallback);
};
enum class ParamKind { Number, Integer, Toggle, Choice, Color, ObjectRef, Text };
struct Param {
    const char *key;
    const char *label;
    ParamKind kind;
    float initial, minimum, maximum;
    const char *choices; // Choice 的选项以 | 分隔；Text 的默认内容。
    const char *help;
};
struct Type {
    const char *id;
    const char *name;
    const char *description;
    const Param *params;
    int paramCount;
    int placement; // 0=仅手工，1=单个生成，2=成对生成并关联 ObjectRef 参数。
    Color color;
    void *(*create)(const Host *, Id);
    void (*destroy)(void *);
    void (*step)(void *, float dt);
    void (*event)(void *, const Event *);
    void (*draw)(void *, const Draw *);
};
struct Registrar {
    void *user;
    void (*add)(void *, const Type *);
};
using Entry = void (*)(Registrar *);

// 便捷包装：作者保留逻辑，宿主负责实体查询、参数和事件生命周期。
struct Behavior {
    const Host *host;
    Id id;
    Behavior(const Host *h, Id i) : host(h), id(i) {}
    Object object() const {
        Object o{};
        host->getObject(host->user, id, &o);
        return o;
    }
    float number(const char *key, float fallback = 0) const {
        return host->number(host->user, id, key, fallback);
    }
    Color color(const char *key, Color fallback) const {
        return host->color(host->user, id, key, fallback);
    }
    Entity entity(int index) const {
        Entity e{};
        host->entityAt(host->user, index, &e);
        return e;
    }
    int count() const {
        return host->entityCount(host->user);
    }
    void step(float) {}
    void event(const Event &) {}
    void draw(const Draw &) {}
};
template <class T>
Type behavior(const char *id, const char *name, const char *description, const Param *params, int count,
              int placement, Color color) {
    return {id,
            name,
            description,
            params,
            count,
            placement,
            color,
            [](const Host *h, Id i) -> void * { return new T(h, i); },
            [](void *p) { delete static_cast<T *>(p); },
            [](void *p, float dt) { static_cast<T *>(p)->step(dt); },
            [](void *p, const Event *e) { static_cast<T *>(p)->event(*e); },
            [](void *p, const Draw *d) { static_cast<T *>(p)->draw(*d); }};
}
inline Vec direction(float degrees) {
    float a = degrees * 0.017453292519943295f;
    return {std::cos(a), std::sin(a)};
}
inline float distance(Vec a, Vec b) {
    float x = a.x - b.x, y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}
} // namespace orbmod
