#pragma once
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/match/Match.hpp"
#include "orbduel/mods/PluginManager.hpp"
#include "orbduel/physics/Arena.hpp"
#include "orbduel/physics/World.hpp"
#include <memory>
#include <set>

namespace orbduel {
// 编辑器预览与真正对局共享的无窗口运行时；持有全部内容快照。
class Simulation {
  public:
    Simulation(const SceneDocument &, const ContentPack &, const PluginManager &, bool preview = false);
    ~Simulation();
    void step(float dt = 1.f / 120);
    void draw(const orbmod::Draw &);
    SceneDocument scene;
    ContentPack pack;
    EventBus events;
    Arena arena;
    World world;
    Match match;
    Rng rng;
    float maxLull = 0;
    std::vector<std::string> log;
    int modActivations = 0;
    std::vector<std::pair<Vec2, float>> flashes;
    std::string drawingAssetRoot;

  private:
    struct Instance {
        int id;
        const RegisteredType *type;
        void *state;
    };
    struct OwnedEvent {
        std::string name, cause;
        int a, b, object;
        Vec2 pos;
        float value;
    };
    const PluginManager &plugins_;
    orbmod::Host host_{};
    std::vector<Instance> instances_;
    std::vector<SceneObject> pendingSpawn_;
    std::vector<int> pendingRemove_;
    std::vector<OwnedEvent> pendingEvents_;
    std::set<std::pair<int, int>> inside_;
    int nextId_ = 1;
    bool preview_;
    void instantiate(const SceneObject &);
    void forward(const orbmod::Event &);
    void onEvent(const Event &);
    SceneObject *object(int);
};
} // namespace orbduel
