#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/Scene.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace orbduel;
void require(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
}
bool near(float a, float b) {
    return std::abs(a - b) < .02f;
}

int main(int argc, char **argv) {
    try {
        if (argc < 3)
            return 2;
        log::setConsole(false);
        ContentLoader loader(argv[1]);
        require(loader.loadAll(), "load combined content");
        const auto &pack = loader.pack();
        PluginManager plugins;
        plugins.load(argv[2]);
        require(plugins.issues.empty(), "load native mods with combined content");
        const auto *lancer = pack.findCharacter("lancer");
        const auto *singularity = pack.findCharacter("singularity");
        const auto *dummy = pack.findCharacter("wooden-dummy");
        require(lancer && singularity && dummy && pack.characters.size() == 9,
                "preserve teammate characters alongside the studio roster");
        require(lancer->stats.maxHp == 150 && lancer->skin.shape == "triangle" &&
                    singularity->stats.maxHp == 250 && singularity->hitbox.ghost &&
                    near(singularity->hitbox.contact, .6f) && dummy->stats.maxHp == 1000,
                "preserve teammate character values and body types");
        const auto *dash = pack.findSkill("dash");
        require(dash && dash->charge.enabled && pack.findSkill("burst-dash") &&
                    !pack.findSkill("burst-dash")->charge.enabled && pack.skills.size() == 16,
                "charged dash and instant burst use distinct IDs");
        auto scene = SceneDocument::fromPack(pack);
        scene.fighters = {*lancer, *singularity};
        scene.objects.clear();
        scene.map.obstacles.clear();
        scene.stall = false;
        scene.shrink = false;
        scene.timeLimit = 0;
        scene.map.spawns[0].pos = {400, 360};
        scene.map.spawns[1].pos = {700, 360};
        SceneDocument restored;
        std::string error;
        require(importCode(shareCode(scene), restored, error) && sceneJson(restored) == sceneJson(scene),
                "offline share preserves triangle, ghost, contact and mass growth");
        Simulation roundtrip(restored, pack, plugins);
        require(roundtrip.world.body(1)->ghost && roundtrip.match.fighter(0).maxHp == 150,
                "shared teammate roles retain runtime properties");
        {
            auto a = scene;
            for (auto &f : a.fighters)
                f.skills.clear();
            Simulation s(a, pack, plugins);
            Event hit;
            hit.type = EventType::Hit;
            hit.a = 0;
            hit.b = 1;
            hit.value = 400;
            s.match.onEvent(hit);
            require(near(s.match.fighter(0).hp, 150) && near(s.match.fighter(1).hp, 242.8f),
                    "contact coefficient belongs to the victim on zero-damage maps");
        }
        {
            auto a = scene;
            a.fighters[1] = *dummy;
            Simulation s(a, pack, plugins);
            SkillSystem skill;
            skill.attach(0, {dash}, nullptr, &s.world, &s.rng);
            auto &self = *s.world.body(0);
            auto &enemy = *s.world.body(1);
            EventBus events;
            skill.update(6.01f, 1, self, enemy, s.rng, events);
            require(skill.states()[0].charging && near(skill.states()[0].chargeLeft, 3) &&
                        lengthOf(self.vel) == 0,
                    "dash charges in place after cooldown");
            skill.interruptCharge();
            require(!skill.states()[0].charging && skill.states()[0].casts == 0,
                    "early collision interrupts charge without advancing learning");
            skill.update(6.01f, 1, self, enemy, s.rng, events);
            skill.update(3.01f, 1, self, enemy, s.rng, events);
            require(skill.states()[0].casts == 1 && near(lengthOf(self.vel), 500),
                    "charge releases at configured speed and counts once");
            float scale = 0;
            require(skill.consumeRam(scale) && near(scale, .12f) && !skill.consumeRam(scale),
                    "charged ram damage is consumed only once");
            for (int cast = 1; cast < 15; ++cast) {
                skill.update(6.01f, 1, self, enemy, s.rng, events);
                const auto state = skill.states()[0];
                if (cast < 10) {
                    require(state.charging && near(state.chargeLeft, 3.f * (1.f - cast / 10.f)),
                            "successful casts progressively shorten charge");
                }
                if (state.charging)
                    skill.update(state.chargeLeft + .01f, 1, self, enemy, s.rng, events);
            }
            require(skill.states()[0].casts == 15, "instant learned releases still count once");
            auto learned = *dash;
            learned.charge.durationMin = 1;
            SkillSystem immune;
            immune.attach(0, {&learned}, nullptr, &s.world, &s.rng);
            for (int cast = 0; cast < 15; ++cast) {
                immune.update(6.01f, 1, self, enemy, s.rng, events);
                immune.update(immune.states()[0].chargeLeft + .01f, 1, self, enemy, s.rng, events);
            }
            immune.update(6.01f, 1, self, enemy, s.rng, events);
            immune.interruptCharge();
            require(immune.states()[0].charging, "experienced charges resist interruption");
        }
        for (bool ghost : {false, true}) {
            EventBus events;
            Arena arena;
            arena.configure({0, 0, 1000, 720}, 1800);
            World world(events, arena);
            int hits = 0;
            events.subscribe(EventType::Hit, [&](const Event &) { ++hits; });
            Body a, b;
            a.pos = {400, 360};
            b.pos = {440, 360};
            a.vel = {100, 0};
            b.vel = {-100, 0};
            a.radius = b.radius = 30;
            a.ghost = ghost;
            a.phasePush = ghost ? 0 : 200;
            world.addBody(a);
            world.addBody(b);
            for (int i = 0; i < 4; ++i) {
                world.step(1.f / 120);
                events.dispatch();
            }
            require(hits == 1, "phase or ghost overlap emits one hit per passage");
            require(near(world.body(0)->vel.x, 100) && near(world.body(1)->vel.x, ghost ? -100 : 100),
                    "ghost does not push; pierce pushes only its opponent");
        }
        {
            auto a = scene;
            a.fighters[1] = *dummy;
            Simulation s(a, pack, plugins);
            s.world.body(0)->vel = {0, 0};
            s.match.update(.01f);
            require(s.world.body(0)->phasePush == 0, "stationary lancer does not pierce");
            s.world.body(0)->vel = {700, 0};
            s.match.update(.01f);
            require(s.world.body(0)->phasePush > 0, "lancer pierces above speed threshold");
        }
        {
            auto a = scene;
            a.fighters = {*singularity, *dummy};
            a.fighters[0].skills = {"gravity-well"};
            for (auto &f : a.fighters)
                f.stats.startSpeed = 0;
            const auto &g = pack.findSkill("gravity-well")->gravity;
            a.map.spawns[1].pos = {400 + g.range + (g.rangeMax - g.range) * .25f, 360};
            Simulation s(a, pack, plugins);
            s.match.update(.01f);
            require(lengthOf(s.world.body(1)->force) == 0, "outside initial gravity range is unaffected");
            s.match.update(44.99f);
            require(near(s.world.body(0)->mass, 3.8f) && s.world.body(1)->force.x < 0,
                    "gravity range and body mass grow with match time");
            s.world.body(1)->pos = s.world.body(0)->pos + Vec2{5, 0};
            float before = s.match.fighter(1).hp;
            s.match.update(.01f);
            const auto teleported = s.world.body(1)->pos;
            require(s.match.fighter(1).hp < before &&
                        lengthOf(teleported - s.world.body(0)->pos) >= g.teleportMinDist &&
                        s.match.fighter(1).coreGuard > 0,
                    "gravity core damages and teleports to a safe distance");
            s.match.update(.01f);
            require(s.world.body(1)->pos.x == teleported.x && s.world.body(1)->pos.y == teleported.y,
                    "gravity core guard prevents repeated teleportation");
        }
        {
            auto a = scene;
            a.fighters = {*singularity, *dummy};
            a.fighters[0].skills = {"omnipull"};
            for (auto &f : a.fighters)
                f.stats.startSpeed = 0;
            a.map.spawns[1].pos = {450, 360};
            Simulation s(a, pack, plugins);
            s.match.update(pack.findSkill("omnipull")->trigger.cooldown + .01f);
            auto state = s.match.skillStates(0)[0];
            require(state.pullLeft > 0 && state.casts == 1 && s.match.skills(0).invulnerable() &&
                        s.match.skills(0).suppressesPassive(),
                    "omnipull activates invulnerability and passive suppression");
            float self = s.match.fighter(0).hp, foe = s.match.fighter(1).hp;
            s.match.damage(1, 0, 50, "test");
            s.match.damage(-1, 0, 50, "environment");
            Event hit;
            hit.type = EventType::Hit;
            hit.a = 0;
            hit.b = 1;
            hit.value = 400;
            s.match.onEvent(hit);
            require(s.match.fighter(0).hp == self && s.match.fighter(1).hp == foe,
                    "omnipull blocks own damage and both sides' contact damage");
            s.match.update(state.pullLeft + .01f);
            require(near(foe - s.match.fighter(1).hp, state.blastDamage) &&
                        !s.match.skills(0).invulnerable() &&
                        near(s.match.skillStates(0)[0].cooldownLeft,
                             pack.findSkill("omnipull")->trigger.cooldown),
                    "omnipull blasts once then starts its cooldown");
            foe = s.match.fighter(1).hp;
            s.match.update(.01f);
            require(s.match.fighter(1).hp == foe, "omnipull blast is not applied twice");
        }
        {
            auto a = scene;
            a.fighters[1] = *dummy;
            for (auto &f : a.fighters) {
                f.skills.clear();
                f.stats.startSpeed = 0;
            }
            a.shrink = true;
            auto &sh = a.map.shrink;
            sh.firstAt = 0;
            sh.interval = .01f;
            sh.step = 1;
            sh.minInset = 1;
            sh.warnTime = 0;
            sh.animTime = .01f;
            Simulation s(a, pack, plugins);
            s.match.damage(0, 1, 900, "test");
            int ends = 0;
            s.events.subscribe(EventType::RoundEnd, [&](const Event &) { ++ends; });
            for (int step = 0; step < 1000 && s.match.phase() == MatchPhase::Fighting; ++step)
                s.step();
            require(s.match.result().reason == "zone_zero" && s.match.result().winnerId == 0 && ends == 1,
                    "zero-size zone ends once by raw HP, preserving teammate rule");
            const auto pos = s.world.body(0)->pos;
            for (int i = 0; i < 100; ++i)
                s.step();
            require(s.world.body(0)->pos.x == pos.x && s.world.body(0)->pos.y == pos.y,
                    "finished shrinking arena does not run degenerate physics");
        }
        if (argc > 3) {
            PluginManager external;
            external.load(argv[3]);
            require(external.issues.empty() && external.find("my-mod.vortex"),
                    "load independently compiled SDK template");
            auto a = scene;
            a.mods = {"my-mod"};
            for (auto &f : a.fighters) {
                f.skills.clear();
                f.stats.startSpeed = 0;
            }
            SceneObject vortex;
            vortex.type = "my-mod.vortex";
            vortex.pos = {350, 360};
            vortex.radius = 200;
            vortex.params = external.defaults(*external.find(vortex.type));
            a.objects = {vortex};
            Simulation s(a, pack, external);
            for (int i = 0; i < 12; ++i)
                s.step();
            require(s.world.body(0)->vel.y > 20,
                    "external template behavior changes host physics through SDK");
        }
        std::cout << "All integration checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
