#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace orbduel;
namespace {
const orbmod::Host *host = nullptr;
std::map<int, int> deaths, removals, destroyed;
bool deathSnapshot = false;
int watched = -1;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
struct Probe : orbmod::Behavior {
    Probe(const orbmod::Host *h, int id) : Behavior(h, id) {
        ::host = h;
    }
    ~Probe() {
        ++destroyed[id];
    }
    void step(float) {
        auto o = object();
        if (o.owner >= 0 && number("move"))
            host->setVelocity(host->user, o.owner, {120, 0});
    }
    void event(const orbmod::Event &e) {
        // 根观察者仅计数一次；绑定能力另行验证销毁。
        if (object().owner >= 0)
            return;
        if (!std::strcmp(e.name, "death")) {
            ++deaths[e.a];
            orbmod::Entity entity{};
            if (e.a == watched)
                deathSnapshot = Simulation::entityApi()->get(host->user, e.a, &entity) && !entity.alive;
        }
        if (!std::strcmp(e.name, "entity_removed"))
            ++removals[e.a];
    }
};
orbmod::EntitySpec clone(int owner = 0) {
    orbmod::EntitySpec spec;
    spec.owner = owner;
    spec.pos = {550, 200};
    spec.hpScale = .3f;
    spec.radiusScale = .6f;
    return spec;
}
void resetTrace() {
    deaths.clear();
    removals.clear();
    destroyed.clear();
    deathSnapshot = false;
    watched = -1;
}
SceneDocument fixture(const ContentPack &pack) {
    auto scene = SceneDocument::fromPack(pack);
    scene.mods.clear();
    scene.objects.clear();
    scene.map.obstacles.clear();
    scene.stall = scene.shrink = false;
    scene.timeLimit = 0;
    scene.map.physics.damageScale = 1;
    scene.map.physics.bounceJitterDeg = 0;
    scene.map.spawns[0].pos = {180, 360};
    scene.map.spawns[1].pos = {1000, 360};
    for (auto &f : scene.fighters) {
        f.skills.clear();
        f.stats.startSpeed = 0;
        f.stats.armor = 0;
    }
    SceneObject observer;
    observer.id = 1;
    observer.type = "test.probe";
    scene.objects.push_back(observer);
    return scene;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 3)
            return 2;
        log::setConsole(false);
        ContentLoader loader(argv[1]);
        check(loader.loadAll(), "load content");
        const auto &pack = loader.pack();
        PluginManager plugins;
        plugins.setEntityApi(Simulation::entityApi());
        plugins.load(argv[2]);
        check(plugins.issues.empty(), "load entity-aware native plugins");
        auto probe = orbmod::behavior<Probe>("test.probe", "probe", "", nullptr, 0, 0, {0, 0, 0, 0});
        plugins.types.push_back({probe, "test", ""});
        auto scene = fixture(pack);
        const auto *api = Simulation::entityApi();
        {
            resetTrace();
            Simulation s(scene, pack, plugins);
            auto spec = clone();
            int a = api->create(host->user, &spec);
            spec.pos = {750, 200};
            int b = api->create(host->user, &spec);
            int ability = api->bind(host->user, a, "test.probe", "{\"move\":1}");
            orbmod::Entity e{};
            check(a >= 2 && b > a && ability >= 0 && !api->get(host->user, a, &e),
                  "creation and binding are deferred");
            s.step();
            check(api->get(host->user, a, &e) &&
                      std::abs(e.maxHp - scene.fighters[0].stats.maxHp * .3f) < .01f,
                  "clone owns independently scaled HP");
            float start = e.pos.x;
            s.step();
            check(api->get(host->user, a, &e) && e.pos.x > start,
                  "bound ability moves its owner through physics");
            orbmod::CombatInfo info{};
            check(api->info(host->user, a, &info) && info.owner == 0 && info.team == 0 && !info.primary,
                  "clone inherits owner team and stays outside primary win condition");
            api->destroy(host->user, a);
            api->destroy(host->user, a);
            api->unbind(host->user, ability);
            api->unbind(host->user, ability);
            s.step();
            check(!api->get(host->user, a, &e) && api->get(host->user, b, &e) &&
                      host->entityAt(host->user, 2, &e) && e.id == b && destroyed[ability] == 1 &&
                      removals[a] == 1,
                  "repeated removal destroys once and enumeration survives ID holes");
            api->destroy(host->user, b);
            s.step();
            int previous = b;
            for (int i = 0; i < 2000; ++i) {
                int id = api->create(host->user, &spec);
                s.step();
                if (id <= previous || s.match.fighterCount() != 3)
                    throw std::runtime_error("unstable lifecycle");
                previous = id;
                api->destroy(host->user, id);
                s.step();
                if (s.world.bodies().size() != 2 || s.match.fighterCount() != 2 ||
                    s.scene.objects.size() != 1)
                    throw std::runtime_error("lifecycle storage leak");
            }
            check(true, "2000 create/destroy cycles reclaim actors and never reuse IDs");
        }
        {
            resetTrace();
            Simulation s(scene, pack, plugins);
            auto spec = clone();
            int id = api->create(host->user, &spec);
            int ability = api->bind(host->user, id, "test.probe", nullptr);
            s.step();
            watched = id;
            host->shield(host->user, id, 10, 1);
            host->damage(host->user, 1, id, 15, "test");
            orbmod::Entity e{};
            api->get(host->user, id, &e);
            check(e.shield == 0 && e.hp == e.maxHp - 5, "summon damage uses ordinary shield pipeline");
            host->heal(host->user, id, 3);
            api->get(host->user, id, &e);
            check(e.hp == e.maxHp - 2, "summon can heal");
            host->damage(host->user, 1, id, 100000, "test");
            host->damage(host->user, 1, id, 100000, "test");
            s.step();
            check(deaths[id] == 1 && removals[id] == 1 && deathSnapshot && destroyed[ability] == 1 &&
                      !api->get(host->user, id, &e) && s.match.phase() == MatchPhase::Fighting,
                  "death delivers dead snapshot, cleans bound ability once and leaves duel running");
        }
        {
            resetTrace();
            Simulation s(scene, pack, plugins);
            auto spec = clone();
            spec.lifetime = .025f;
            int id = api->create(host->user, &spec);
            api->bind(host->user, id, "test.probe", nullptr);
            for (int i = 0; i < 5; ++i)
                s.step();
            check(!s.world.body(id) && deaths[id] == 0 && removals[id] == 1,
                  "lifetime removes without inventing an HP death");
            spec.lifetime = 0;
            id = api->create(host->user, &spec);
            auto childSpec = clone(id);
            childSpec.pos = {700, 200};
            int child = api->create(host->user, &childSpec);
            int ability = api->bind(host->user, child, "test.probe", nullptr);
            s.step();
            api->destroy(host->user, id);
            s.step();
            check(!s.world.body(child) && destroyed[ability] == 1,
                  "removing owner recursively cleans descendants");
            int pending = api->create(host->user, &spec);
            api->destroy(host->user, pending);
            s.step();
            check(!s.world.body(pending), "destroy before creation cancels pending entity");
            id = api->create(host->user, &spec);
            s.step();
            host->damage(host->user, 1, 0, 100000, "test");
            check(api->create(host->user, &spec) < 0, "dead owner cannot create replacements");
            s.step();
            check(s.match.phase() == MatchPhase::Finished && s.world.bodies().size() == 2 &&
                      !s.world.body(id),
                  "primary death determines winner and clears summons");
        }
        {
            Simulation s(scene, pack, plugins);
            auto spec = clone();
            int id = api->create(host->user, &spec);
            s.step();
            Event hit;
            hit.type = EventType::Hit;
            hit.a = 0;
            hit.b = id;
            hit.value = 400;
            float primary = s.match.fighter(0).hp;
            auto hp = s.match.fighterById(id)->hp;
            s.match.onEvent(hit);
            check(s.match.fighter(0).hp == primary && s.match.fighterById(id)->hp == hp,
                  "friendly collision causes no combat damage");
            hit.a = 1;
            s.match.onEvent(hit);
            check(s.match.fighter(0).hp == primary && s.match.fighterById(id)->hp < hp,
                  "enemy contact damages the involved summon without hitting unrelated primary");
        }
        {
            auto a = scene;
            a.fighters[0] = *pack.findCharacter("singularity");
            a.map.spawns[0].pos = {400, 360};
            a.map.spawns[1].pos = {1000, 360};
            Simulation s(a, pack, plugins);
            auto spec = clone(1);
            spec.pos = {500, 360};
            int enemy = api->create(host->user, &spec);
            spec.owner = 0;
            spec.pos = {500, 300};
            int ally = api->create(host->user, &spec);
            s.step();
            float hp = s.match.fighterById(enemy)->hp, allied = s.match.fighterById(ally)->hp;
            s.match.update(1.f / 120);
            check(s.world.body(enemy)->force.x < 0 && s.match.fighterById(enemy)->hp < hp &&
                      s.world.body(ally)->force.x == 0 && s.match.fighterById(ally)->hp == allied,
                  "teammate gravity affects extra enemies and preserves allies");
        }
        {
            auto a = scene;
            a.fighters[0] = *pack.findCharacter("lancer");
            Simulation s(a, pack, plugins);
            auto spec = clone();
            spec.copySkills = 1;
            spec.velocity = {700, 0};
            int id = api->create(host->user, &spec);
            s.step();
            check(s.match.fighterById(id)->pierce.damage > 0 && s.world.body(id)->phasePush > 0,
                  "opt-in cloned built-in skills preserve teammate piercing");
        }
        {
            Simulation s(scene, pack, plugins);
            auto spec = clone();
            spec.hpScale = std::numeric_limits<float>::quiet_NaN();
            check(api->create(host->user, &spec) < 0 &&
                      api->bind(host->user, 50000, "test.probe", nullptr) < 0,
                  "invalid numeric state and missing binding target fail without crashing");
        }
        {
            Simulation s(scene, pack, plugins);
            orbmod::EntitySpec spec;
            spec.character = pack.characters.front().id.c_str();
            spec.pos = {500, 200};
            int neutral = api->create(host->user, &spec);
            s.step();
            check(neutral >= 2 && s.match.isEnemy(neutral, 0) && s.match.isEnemy(neutral, 1),
                  "ownerless archetype can create a neutral combatant");
            api->destroy(host->user, 0);
            s.step();
            check(s.match.fighter(0).alive && s.world.body(neutral),
                  "destroy rejects primary without cleaning its unrelated allies");
        }
        {
            resetTrace();
            auto a = scene;
            SceneObject root;
            root.id = 2;
            root.type = "summon.echo";
            root.params = {{"maxAlive", 1}, {"interval", .05}, {"speed", 280}, {"hpScale", .01}};
            a.objects.push_back(root);
            a.map.spawns[0].pos = {300, 360};
            a.map.spawns[1].pos = {460, 360};
            Simulation s(a, pack, plugins);
            s.step();
            check(s.match.fighterCount() == 3, "native summon DLL creates a real combat entity");
            int id = s.match.fighter(2).bodyId;
            auto before = s.world.body(id)->pos;
            s.step();
            check(lengthOf(s.world.body(id)->pos - before) > 1,
                  "native summon bound controller chases a target");
            float hp = s.match.fighterById(id)->hp;
            for (int i = 0; i < 240 && s.world.body(id); ++i)
                s.step();
            check(!s.world.body(id) || s.match.fighterById(id)->hp < hp,
                  "real physics collisions injure summon");
            check(!s.world.body(id) && deaths[id] == 1 && s.match.phase() == MatchPhase::Fighting,
                  "real physics contact kills low-HP summon and delivers one death");
            // 两个独立对局使用同一函数表和 DLL，不共享实体状态。
            Simulation twin(a, pack, plugins);
            for (int i = 0; i < 20; ++i)
                twin.step();
            check(twin.match.fighterCount() == 3, "optional API uses per-simulation context");
            Simulation first(a, pack, plugins), second(a, pack, plugins);
            for (int i = 0; i < 1200; ++i) {
                first.step();
                second.step();
            }
            check(first.world.bodies().size() == second.world.bodies().size(),
                  "seed reproduces dynamic creation and removal counts");
            for (const auto &b : first.world.bodies()) {
                const auto *other = second.world.body(b.id);
                if (!other || other->pos.x != b.pos.x || other->pos.y != b.pos.y ||
                    first.match.fighterById(b.id)->hp != second.match.fighterById(b.id)->hp)
                    throw std::runtime_error("dynamic match diverged for identical seed");
            }
            check(true, "seed reproduces summon movement, damage and death");
        }
        if (argc > 3) {
            PluginManager legacy;
            legacy.setEntityApi(api);
            legacy.load(argv[3]);
            check(legacy.issues.empty() && legacy.find("portal.portal") && legacy.find("gravity.field"),
                  "previously built original-SDK DLLs still load");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
