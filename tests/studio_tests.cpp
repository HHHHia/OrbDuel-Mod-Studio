#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/Scene.hpp"
#include "orbduel/match/Simulation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace orbduel;
namespace fs = std::filesystem;
void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << "\n";
}
int main(int argc, char **argv) {
    try {
        if (argc < 4)
            return 2;
        fs::path content = fs::u8path(argv[1]), scratch = fs::u8path(argv[3]);
        fs::create_directories(scratch);
        ContentLoader loader(content.u8string());
        check(loader.loadAll(), "load bundled content");
        PluginManager plugins;
        plugins.load(argv[2]);
        for (const auto &e : plugins.issues)
            std::cout << e << "\n";
        check(plugins.types.size() >= 3 && plugins.issues.empty(), "load independent C++ plugins");
        auto scene = SceneDocument::fromPack(loader.pack());
        generateScene(scene, 42, 3, 3, plugins.generationTypes());
        auto same = SceneDocument::fromPack(loader.pack());
        generateScene(same, 42, 3, 3, plugins.generationTypes());
        check(sceneJson(scene) == sceneJson(same), "seed reproduces map");
        scene.name = "手工地图 / ★";
        scene.theme = "lava";
        scene.objects[0].pos = {333.5f, 222};
        scene.objects[0].params["custom"] = 18;
        std::string error;
        SceneDocument decoded;
        check(importCode(shareCode(scene), decoded, error) && sceneJson(decoded) == sceneJson(scene),
              "share restores edited geometry, Unicode, rules and mod parameters");
        auto before = sceneJson(decoded);
        check(!importCode("OD1:broken!", decoded, error) && sceneJson(decoded) == before,
              "bad import retains previous map");
        check(saveScene((scratch / fs::u8path("地图.json")).u8string(), scene, error) &&
                  openScene((scratch / fs::u8path("地图.json")).u8string(), decoded, error) &&
                  sceneJson(decoded) == sceneJson(scene),
              "save and open Unicode file");
        auto idCase = scene;
        int oldId = idCase.nextId();
        SceneObject removed;
        removed.id = oldId;
        idCase.objects.push_back(removed);
        idCase.objects.pop_back();
        check(idCase.nextId() > oldId, "deleted object IDs are not reassigned");
        auto absent = scene;
        absent.mods = {"not-installed"};
        check(!plugins.available(absent, error), "missing mod has an actionable name");
        scene.map.obstacles.clear();
        scene.objects.clear();
        scene.fighters[0].skills.clear();
        scene.fighters[1].skills.clear();
        scene.stall = false;
        // 通用效果测试显式打开碰撞伤害；基础地图保留主分支的 damageScale=0。
        scene.map.physics.damageScale = 1;
        auto draftBefore = sceneJson(scene);
        Simulation first(scene, loader.pack(), plugins), second(scene, loader.pack(), plugins);
        for (int i = 0; i < 5000; ++i) {
            first.step();
            second.step();
        }
        check(first.match.fighter(0).hp == second.match.fighter(0).hp &&
                  first.world.body(0)->pos.x == second.world.body(0)->pos.x,
              "fixed-step simulation repeats with same seed");
        {
            Simulation s(scene, loader.pack(), plugins);
            Event e;
            e.type = EventType::Hit;
            e.a = 0;
            e.b = 1;
            e.attackSpeedA = 800;
            e.attackSpeedB = 100;
            e.value = 900;
            s.match.onEvent(e);
            float left = scene.fighters[0].stats.maxHp - s.match.fighter(0).hp,
                  right = scene.fighters[1].stats.maxHp - s.match.fighter(1).hp;
            check(right > left, "contact damage respects attacker stats and victim armor");
            s.match.giveShield(0, 20, 1);
            float hp = s.match.fighter(0).hp;
            s.match.damage(1, 0, 12, "test");
            check(s.match.fighter(0).hp == hp && s.match.fighter(0).shield == 8,
                  "shield absorbs actual damage");
            s.match.damage(1, 0, 20, "test");
            s.match.heal(0, 5);
            check(s.match.fighter(0).hp == hp - 7, "heal respects actual hit points");
        }
        {
            auto a = scene;
            a.timeLimit = .1f;
            Simulation s(a, loader.pack(), plugins);
            for (int i = 0; i < 20; ++i)
                s.step();
            check(s.match.phase() == MatchPhase::Finished, "time limit ends a stalled match");
        }
        {
            auto a = scene;
            auto *t = plugins.find("portal.portal");
            check(t != nullptr, "portal is registered by DLL");
            SceneObject entry;
            entry.id = 1;
            entry.type = t->api.id;
            entry.pos = a.map.spawns[0].pos;
            entry.params = plugins.defaults(*t);
            entry.params["target"] = 2;
            auto exit = entry;
            exit.id = 2;
            exit.pos = {800, 250};
            exit.params["target"] = 1;
            exit.angle = 90;
            a.objects = {entry, exit};
            Simulation s(a, loader.pack(), plugins);
            s.step();
            check(s.world.body(0)->pos.y > 300 && s.world.body(0)->vel.y > 0,
                  "portal teleports and redirects velocity");
            for (int i = 0; i < 4; ++i)
                s.step();
            check(s.modActivations == 1, "portal exit does not immediately loop");
        }
        {
            auto a = scene;
            a.fighters[0].stats.startSpeed = 0;
            auto *t = plugins.find("gravity.field");
            SceneObject field;
            field.type = t->api.id;
            field.pos = a.map.spawns[0].pos + Vec2{50, 0};
            field.radius = 200;
            field.params = plugins.defaults(*t);
            a.objects = {field};
            Simulation s(a, loader.pack(), plugins);
            for (int i = 0; i < 30; ++i)
                s.step();
            check(s.world.body(0)->vel.x > 10, "continuous plugin affects physics");
        }
        {
            auto a = scene;
            auto *t = plugins.find("launch.pad");
            SceneObject pad;
            pad.type = t->api.id;
            pad.pos = a.map.spawns[0].pos;
            pad.params = plugins.defaults(*t);
            pad.params["speed"] = 900;
            pad.params["shield"] = 15;
            a.objects = {pad};
            Simulation s(a, loader.pack(), plugins);
            s.step();
            check(s.world.body(0)->vel.x == 900 && s.match.fighter(0).shield == 15,
                  "launch parameters alter speed and shield");
        }
        auto bad = scratch / "bad-content";
        {
            auto a = scene;
            auto p = loader.pack();
            SkillDef boost;
            boost.id = "test-boost";
            boost.trigger.kind = TriggerKind::OnWallBounce;
            boost.trigger.cooldown = 10;
            ModifierDef attack;
            attack.op = ModifierOp::DamageBoost;
            attack.value = 2;
            attack.duration = .1f;
            ModifierDef guard;
            guard.op = ModifierOp::Shield;
            guard.value = 1;
            guard.duration = 1;
            boost.modifiers = {attack, guard};
            p.skills.push_back(boost);
            a.fighters[0].skills = {boost.id};
            a.fighters[1].skills.clear();
            a.fighters[0].stats.maxHp = a.fighters[1].stats.maxHp = 1000;
            Simulation s(a, p, plugins);
            s.step();
            Event wall;
            wall.type = EventType::WallBounce;
            wall.a = 0;
            s.match.onEvent(wall);
            check(s.match.skillStates(0).front().casts == 1, "reactive skills are ready on the first event");
            Event hit;
            hit.type = EventType::Hit;
            hit.a = 0;
            hit.b = 1;
            hit.value = 400;
            hit.attackSpeedA = 400;
            float before = s.match.fighter(1).hp;
            s.match.onEvent(hit);
            float boosted = before - s.match.fighter(1).hp;
            s.match.update(.2f);
            before = s.match.fighter(1).hp;
            s.match.onEvent(hit);
            float normal = before - s.match.fighter(1).hp;
            check(boosted > normal * 1.7f && s.match.skillStates(0).front().active,
                  "damage boost expires independently of a longer shield");
        }
        {
            auto a = scene;
            auto p = loader.pack();
            SkillDef thorns;
            thorns.id = "test-thorns";
            thorns.trigger.kind = TriggerKind::OnHitBy;
            thorns.trigger.cooldown = 10;
            ModifierDef reflect;
            reflect.op = ModifierOp::Thorns;
            reflect.value = .5f;
            reflect.duration = 1;
            thorns.modifiers = {reflect};
            p.skills.push_back(thorns);
            for (auto &f : a.fighters) {
                f.skills = {thorns.id};
                f.stats.maxHp = 1000;
            }
            Simulation s(a, p, plugins);
            s.step();
            Event hit;
            hit.type = EventType::Hit;
            hit.a = 0;
            hit.b = 1;
            hit.value = 800;
            hit.attackSpeedA = hit.attackSpeedB = 400;
            s.match.onEvent(hit);
            float hp = s.match.fighter(0).hp;
            s.events.dispatch();
            check(s.match.fighter(0).hp < hp, "thorns reflect contact damage");
            s.events.dispatch();
            check(s.events.queuedCount() == 0, "two thorns skills cannot reflect recursively");
        }
        {
            auto a = scene;
            auto p = loader.pack();
            SkillDef wind;
            wind.id = "test-low-hp";
            wind.trigger.kind = TriggerKind::OnLowHp;
            wind.trigger.cooldown = 1;
            wind.trigger.hpBelow = .5f;
            ModifierDef heal;
            heal.op = ModifierOp::Heal;
            heal.value = 20;
            wind.modifiers = {heal};
            p.skills.push_back(wind);
            a.fighters[0].skills = {wind.id};
            Simulation s(a, p, plugins);
            s.match.damage(1, 0, a.fighters[0].stats.maxHp * .6f, "test");
            s.step();
            check(s.match.skillStates(0).front().casts == 1 && s.match.fighter(0).healingDone == 20,
                  "low-health recovery triggers immediately and records actual healing");
            s.match.damage(1, 0, 30, "test");
            for (int i = 0; i < 240; ++i)
                s.step();
            check(s.match.skillStates(0).front().casts == 1, "low-health recovery is once per round");
        }
        fs::create_directories(bad);
        fs::copy(content, bad, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        ContentLoader reload(bad.u8string());
        check(reload.loadAll(), "initial reload snapshot");
        auto charFile = bad / loader.pack().characters.front().sourceDir / "character.json";
        nlohmann::json j;
        {
            std::ifstream f(charFile);
            f >> j;
        }
        j["hitbox"]["offset"] = {"bad", 0};
        {
            std::ofstream f(charFile);
            f << j.dump();
        }
        check(!reload.loadAll() && reload.pack().characters.size() == loader.pack().characters.size(),
              "invalid vector fails without losing previous content");
        check(sceneJson(scene) == draftBefore, "simulation owns its draft snapshot");
        if (auto *t = plugins.find("my-mod.vortex")) {
            auto a = scene;
            a.fighters[0].stats.startSpeed = 0;
            SceneObject field;
            field.type = t->api.id;
            field.pos = a.map.spawns[0].pos + Vec2{50, 0};
            field.radius = 200;
            field.params = plugins.defaults(*t);
            a.objects = {field};
            Simulation s(a, loader.pack(), plugins);
            for (int i = 0; i < 30; ++i)
                s.step();
            check(std::abs(s.world.body(0)->vel.y) > 10,
                  "separately built fourth ability runs without changing the host");
        }
        std::cout << "All studio scenarios passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
