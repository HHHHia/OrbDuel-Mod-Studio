#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include "orbduel_abilities.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace orbduel;
namespace {
const orbmod::Host *host = nullptr;
int recalls = 0;
struct Observer : orbmod::Behavior {
    Observer(const orbmod::Host *h, int id) : Behavior(h, id) {
        ::host = h;
    }
    void event(const orbmod::Event &e) {
        if (!std::strcmp(e.name, "rewind.used"))
            ++recalls;
    }
};
struct EventCreator : orbmod::Ability {
    bool sent = false;
    EventCreator(const orbmod::Host *h, int id) : Ability(h, id, Simulation::entityApi()) {}
    void step(float) {
        beginStep();
        if (!sent) {
            sent = true;
            orbmod::Event signal{"test.create_resources", ownerId(), -1, id, {}, 0, "test"};
            host->emit(host->user, &signal);
        }
    }
    void event(const orbmod::Event &e) {
        if (std::strcmp(e.name, "test.create_resources") || e.object != id)
            return;
        orbmod::EntitySpec spec;
        spec.owner = ownerId();
        spec.pos = {350, 360};
        create(spec);
        spawn("test.ability-observer", {400, 360});
        bind(spec.owner, "test.ability-observer");
        if (number("unbindImmediately", 0))
            api->unbind(host->user, id);
    }
};
void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
}
bool near(float a, float b) {
    return std::abs(a - b) < .02f;
}
SceneDocument fixture(const ContentPack &pack) {
    auto s = SceneDocument::fromPack(pack);
    s.mods.clear();
    s.objects.clear();
    s.map.fields.clear();
    s.map.obstacles.clear();
    s.stall = s.shrink = false;
    s.timeLimit = 0;
    s.map.physics.damageScale = 0;
    s.map.spawns[0].pos = {200, 360};
    s.map.spawns[1].pos = {1000, 360};
    for (auto &f : s.fighters) {
        f.skills.clear();
        f.stats.startSpeed = 0;
        f.stats.maxHp = 100;
        f.stats.armor = 0;
        f.hitbox.ghost = false;
    }
    SceneObject observe;
    observe.id = 1;
    observe.type = "test.ability-observer";
    s.objects.push_back(observe);
    return s;
}
void attach(SceneDocument &s, const char *type, int owner, int id, nlohmann::json params = {}) {
    SceneObject o;
    o.id = id;
    o.type = type;
    o.owner = owner;
    o.params = std::move(params);
    s.objects.push_back(o);
}
int guardId(const Simulation &s, int owner = 0) {
    for (int i = 2; i < s.match.fighterCount(); ++i)
        if (s.match.fighter(i).owner == owner)
            return s.match.fighter(i).bodyId;
    return -1;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 3)
            return 2;
        log::setConsole(false);
        ContentLoader content(argv[1]);
        check(content.loadAll(), "load author examples content");
        PluginManager plugins;
        plugins.setEntityApi(Simulation::entityApi());
        plugins.load(argv[2]);
        check(plugins.issues.empty() && plugins.find("rewind.recall") && plugins.find("guardian.guard"),
              "load independent C++ ability DLLs");
        auto observer =
            orbmod::behavior<Observer>("test.ability-observer", "observer", "", nullptr, 0, 0, {0, 0, 0, 0});
        plugins.types.push_back({observer, "test", ""});
        check(plugins.generationTypes().size() == 4 &&
                  plugins.find("guardian.guard.controller")->api.placement < 0,
              "runtime controller is hidden and does not change generated mechanism palette");
        auto scene = fixture(content.pack());
        auto *api = Simulation::entityApi();
        auto creator =
            orbmod::behavior<EventCreator>("test.event-creator", "creator", "", nullptr, 0, 0, {0, 0, 0, 0});
        plugins.types.push_back({creator, "test", ""});
        for (bool unbindImmediately : {false, true}) {
            auto a = scene;
            attach(a, "test.event-creator", 0, 2, {{"unbindImmediately", unbindImmediately ? 1 : 0}});
            Simulation s(a, content.pack(), plugins);
            s.step();
            s.step();
            if (!unbindImmediately) {
                check(guardId(s) >= 2 && s.scene.objects.size() == 4,
                      "custom event creates a combatant, object and bound ability before pruning");
                api->unbind(host->user, 2);
            } else {
                check(guardId(s) < 0, "removal at the same boundary cancels the pending combatant");
            }
            s.step();
            s.step();
            check(s.match.fighterCount() == 2 && s.scene.objects.size() == 1 && s.match.fighter(0).alive,
                  unbindImmediately ? "same-boundary removal cleans pending event resources"
                                    : "removing an event creator cleans all tracked resources");
        }
        {
            recalls = 0;
            auto a = scene;
            attach(a, "rewind.recall", 0, 2, {{"lookback", .25}, {"cooldown", 1}, {"threshold", .5}});
            Simulation s(a, content.pack(), plugins);
            for (int i = 0; i < 40; ++i)
                s.step();
            s.world.body(0)->pos = {600, 200};
            s.world.body(0)->vel = {200, 0};
            s.match.damage(1, 0, 60, "test");
            s.step();
            check(near(s.match.fighter(0).hp, 100) && near(s.world.body(0)->pos.x, 200) &&
                      near(s.world.body(0)->pos.y, 360) && near(s.world.body(0)->vel.x, 0),
                  "recall restores sampled position velocity and prior HP after owner damage");
            s.step();
            check(recalls == 1, "recall emits one deferred feedback event");
            s.world.body(0)->pos = {500, 200};
            s.match.damage(1, 0, 60, "test");
            s.step();
            check(near(s.match.fighter(0).hp, 40) && near(s.world.body(0)->pos.x, 500),
                  "recall cooldown blocks repeated damage triggers");
        }
        {
            recalls = 0;
            auto a = scene;
            attach(a, "rewind.recall", 0, 2);
            Simulation s(a, content.pack(), plugins);
            s.step();
            s.match.giveShield(0, 20, 1);
            s.match.damage(1, 0, 10, "test");
            s.step();
            s.step();
            check(recalls == 0 && s.match.fighter(0).hp == 100,
                  "shield-only absorption does not trigger rewind");
            s.match.damage(1, 0, 1000, "test");
            s.step();
            check(!s.match.fighter(0).alive && s.match.fighter(0).hp == 0 &&
                      s.match.phase() == MatchPhase::Finished,
                  "rewind does not resurrect an already dead primary");
        }
        {
            auto a = scene;
            attach(a, "rewind.recall", 0, 2);
            attach(a, "rewind.recall", 1, 3);
            Simulation s(a, content.pack(), plugins);
            for (int i = 0; i < 20; ++i)
                s.step();
            s.match.damage(1, 0, 60, "test");
            s.match.damage(0, 1, 60, "test");
            s.step();
            check(near(s.match.fighter(0).hp, 100) && near(s.match.fighter(1).hp, 100),
                  "independent owners keep separate rewind history and cooldown");
        }
        {
            auto a = scene;
            attach(a, "guardian.guard", 0, 2, {{"respawn", 1}});
            Simulation s(a, content.pack(), plugins);
            s.step();
            s.step();
            int id = guardId(s);
            check(id >= 2 && s.match.fighterCount() == 3 && s.match.fighter(0).shield == 8 &&
                      s.match.fighter(1).shield == 0,
                  "guardian is a real entity and shields only nearby allies");
            s.world.body(0)->pos = {500, 360};
            s.step();
            check(s.world.body(id)->vel.x > 0, "guardian follows its moving owner");
            s.world.body(1)->pos = {500, 450};
            s.step();
            check(s.world.body(id)->vel.y > 0, "guardian intercepts an enemy near its owner");
            s.match.damage(1, id, 1000, "test");
            s.step();
            check(!s.world.body(id) && s.match.fighterCount() == 2,
                  "guardian HP death cleans its controller without ending primary duel");
            for (int i = 0; i < 130; ++i)
                s.step();
            int replacement = guardId(s);
            check(replacement > id && s.match.fighterCount() == 3,
                  "guardian replenishes once after cooldown with fresh stable ID");
            api->unbind(host->user, 2);
            s.step();
            s.step();
            check(s.match.fighterCount() == 2 && s.scene.objects.size() == 1 && s.match.fighter(0).alive,
                  "unbinding creator ability reclaims managed guardian and controller");
        }
        {
            auto a = scene;
            attach(a, "guardian.guard", 0, 2);
            Simulation s(a, content.pack(), plugins);
            s.step();
            s.match.damage(1, 0, 1000, "test");
            s.step();
            check(s.match.fighterCount() == 2 && s.world.bodies().size() == 2,
                  "owner death cleans managed summon without resurrection");
        }
        {
            Simulation s(scene, content.pack(), plugins);
            orbmod::Ability helper(host, 1, api);
            orbmod::EntitySpec spec;
            spec.owner = 0;
            spec.pos = {260, 360};
            int ally = helper.create(spec);
            spec.owner = 1;
            spec.pos = {300, 360};
            int foe = helper.create(spec);
            s.step();
            orbmod::Entity target{};
            check(helper.nearestEnemy(0, target, 500) && target.id == foe,
                  "helper nearest enemy uses team metadata rather than enumeration index");
            auto friends = helper.within(0, {200, 360}, 500, orbmod::Relation::Allies, true);
            check(friends.size() == 2 && helper.within(0, {200, 360}, -1).empty(),
                  "helper radius query includes selected allies and handles empty radius");
            helper.cleanup();
            helper.cleanup();
            s.step();
            check(!s.world.body(ally) && !s.world.body(foe) && s.match.fighterCount() == 2,
                  "explicit managed cleanup is idempotent and boundary-safe");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
