#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/Scene.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace orbduel;
void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
}
bool near(float a, float b) {
    return std::abs(a - b) < .001f;
}

Event contact(float left = 400, float right = 400) {
    Event e;
    e.type = EventType::Hit;
    e.a = 0;
    e.b = 1;
    e.attackSpeedA = left;
    e.attackSpeedB = right;
    e.value = left + right;
    return e;
}
int main(int argc, char **argv) {
    try {
        if (argc < 2)
            return 2;
        log::setConsole(false);
        ContentLoader loader(argv[1]);
        check(loader.loadAll(), "load balance content");
        const auto &pack = loader.pack();
        check(pack.findCharacter("lancer")->hitbox.contact == 1 &&
                  pack.findCharacter("wooden-dummy")->hitbox.contact == 1,
              "omitted contact loads as neutral rather than immunity");
        check(!pack.findCharacter("wooden-dummy")->competitive,
              "training dummy stays selectable but is not competitive");
        auto scene = SceneDocument::fromPack(pack);
        scene.fighters = {CharacterDef{}, CharacterDef{}};
        scene.fighters[0].id = "test-left";
        scene.fighters[1].id = "test-right";
        scene.map.physics.damageScale = 1;
        scene.map.physics.refSpeed = 400;
        scene.map.obstacles.clear();
        scene.objects.clear();
        scene.stall = false;
        scene.shrink = false;
        PluginManager none;
        {
            Simulation s(scene, pack, none);
            s.match.onEvent(contact());
            check(near(s.match.fighter(0).hp, 82) && near(s.match.fighter(1).hp, 82),
                  "neutral bodies exchange ordinary contact damage");
        }
        {
            auto a = scene;
            a.map.physics.damageScale = .5f;
            a.fighters[1].hitbox.contact = .6f;
            Simulation s(a, pack, none);
            s.match.onEvent(contact());
            check(near(s.match.fighter(0).hp, 91) && near(s.match.fighter(1).hp, 94.6f),
                  "map and victim multipliers compose multiplicatively");
        }
        {
            auto a = scene;
            a.map.physics.damageScale = 0;
            a.fighters[1].hitbox.contact = 3;
            a.fighters[1].stats.armor = -5;
            Simulation s(a, pack, none);
            s.match.onEvent(contact());
            check(s.match.fighter(0).hp == 100 && s.match.fighter(1).hp == 100,
                  "zero map multiplier wins even over high vulnerability");
            s.match.damage(0, 1, 7, "mod-test");
            check(s.match.fighter(1).hp == 93, "map contact switch does not disable explicit mod damage");
        }
        {
            auto a = scene;
            a.fighters[0].hitbox.contact = 0;
            Simulation s(a, pack, none);
            s.match.onEvent(contact());
            check(s.match.fighter(0).hp == 100 && near(s.match.fighter(1).hp, 82),
                  "explicit custom contact immunity only protects its owner");
        }
        {
            auto a = scene;
            a.fighters[1].hitbox.ghost = true;
            Simulation s(a, pack, none);
            s.match.onEvent(contact());
            check(s.world.body(1)->ghost && near(s.match.fighter(1).hp, 82),
                  "ghost collision response does not imply damage immunity");
        }
        {
            Simulation s(scene, pack, none);
            s.match.onEvent(contact(800, 0));
            check(near(s.match.fighter(0).hp, 94) && near(s.match.fighter(1).hp, 70),
                  "each attack uses its own incoming speed captured before collision");
        }
        {
            Simulation s(scene, pack, none);
            s.match.onEvent(contact(100000, 100000));
            check(near(s.match.fighter(0).hp, 70) && near(s.match.fighter(1).hp, 70),
                  "ordinary impact multiplier has a bounded high-speed gain");
        }
        {
            auto a = scene;
            a.fighters[0].stats.attack = 20;
            a.fighters[1].stats.armor = 4;
            Simulation s(a, pack, none);
            s.match.giveShield(1, 10, 1);
            s.match.onEvent(contact());
            check(near(s.match.fighter(1).hp, 84) && near(s.match.fighter(1).damageAbsorbed, 10),
                  "attack then armor then shield produce actual damage statistics");
        }
        {
            auto a = scene;
            for (auto &f : a.fighters)
                f.stats.maxHp = 10;
            Simulation s(a, pack, none);
            s.match.onEvent(contact());
            s.match.finishStep();
            check(!s.match.fighter(0).alive && !s.match.fighter(1).alive &&
                      s.match.phase() == MatchPhase::Finished && s.match.result().winnerId == -1,
                  "simultaneous ordinary hits permit a fair mutual knockout");
        }
        {
            auto j = sceneJson(scene);
            j["fighters"][0].erase("contact");
            j["fighters"][0].erase("competitive");
            SceneDocument restored;
            std::string error;
            check(readScene(j, restored, error) && restored.fighters[0].hitbox.contact == 1 &&
                      restored.fighters[0].competitive,
                  "legacy missing fields use neutral eligible defaults");
            j["fighters"][0]["contact"] = 0;
            j["fighters"][0]["competitive"] = false;
            check(readScene(j, restored, error) && restored.fighters[0].hitbox.contact == 0 &&
                      !restored.fighters[0].competitive,
                  "explicit custom settings survive scene import");
        }
        std::cout << "All contact checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
