#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
using namespace orbduel;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int run(int argc, char **argv) {
    try {
        if (argc != 4)
            return 2;
        log::setConsole(false);
        ContentLoader content(argv[1]);
        require(content.loadAll(), "content load failed");
        PluginManager mods;
        mods.setEntityApi(Simulation::entityApi());
        mods.load(argv[2], {}, {argv[3]});
        require(mods.issues.empty(), "overlay load failed");
        const auto *type = mods.find("gravity.vortex");
        require(type && !mods.find("gravity.field") && mods.find("portal.portal") && mods.find("lava.pool"),
                "user override removed unrelated built-in mods or failed to replace matching ID");
        require(mods.defaults(*type)["strength"] == -777, "portable configuration was not compiled");
        auto scene = SceneDocument::fromPack(content.pack());
        scene.map.obstacles.clear();
        scene.objects.clear();
        scene.stall = scene.shrink = false;
        scene.map.physics.damageScale = 0;
        for (auto &f : scene.fighters) {
            f.skills.clear();
            f.stats.startSpeed = 0;
        }
        scene.map.spawns[0].pos = {400, 360};
        scene.map.spawns[1].pos = {900, 360};
        SceneObject object;
        object.id = 1;
        object.type = type->api.id;
        object.pos = {350, 360};
        object.radius = 200;
        object.params = mods.defaults(*type);
        scene.objects = {object};
        {
            Simulation simulation(scene, content.pack(), mods);
            for (int i = 0; i < 12; ++i)
                simulation.step();
            require(simulation.world.body(0)->vel.y < -60, "portable vortex does not affect real physics");
        }
        type = mods.find("portable-summons.echo");
        require(type, "portable summon template missing");
        scene.objects[0].type = type->api.id;
        scene.objects[0].params = mods.defaults(*type);
        {
            Simulation simulation(scene, content.pack(), mods);
            simulation.step();
            simulation.step();
            require(simulation.match.fighterCount() == 3, "portable summon cannot create combat entities");
        }
        type = mods.find("portable-ability.ability");
        require(type && mods.defaults(*type)["shieldAmount"] == 33, "author template metadata not compiled");
        scene.objects[0].type = type->api.id;
        scene.objects[0].owner = 0;
        scene.objects[0].params = mods.defaults(*type);
        {
            Simulation simulation(scene, content.pack(), mods);
            simulation.step();
            simulation.match.damage(1, 0, 10, "test");
            simulation.step();
            require(simulation.match.fighter(0).shield == 33,
                    "standalone authored ability does not react to owner damage");
        }
        for (const char *id : {"portable-rewind.rewind", "portable-guardian.guardian"}) {
            type = mods.find(id);
            require(type, "independent ability example project not registered");
            scene.objects[0].type = type->api.id;
            scene.objects[0].params = mods.defaults(*type);
            Simulation simulation(scene, content.pack(), mods);
            simulation.step();
            simulation.step();
            if (std::string(id).find("guardian") != std::string::npos)
                require(simulation.match.fighterCount() == 3,
                        "standalone guardian cannot create its C++ controller");
            else {
                simulation.match.damage(1, 0, 70, "test");
                simulation.step();
                require(simulation.match.fighter(0).hp == simulation.match.fighter(0).maxHp,
                        "standalone rewind logic did not restore historical HP");
            }
        }
        mods.load(argv[2], {"gravity"}, {argv[3]});
        require(mods.issues.empty() && !mods.find("gravity.vortex") && !mods.find("gravity.field"),
                "disabled mod was restored by built-in fallback");
        mods.load(argv[2]);
        require(mods.issues.empty() && mods.find("gravity.field") && !mods.find("gravity.vortex"),
                "normal Windows mod loading changed");
        std::cout << "PASS portable C++ mod generation, installation, physics, entities and user overrides\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
#if defined(_WIN32)
// 控制台测试没有 SDL_main 的 UTF-8 参数转换，显式保留 Unicode 安装目录。
int wmain(int argc, wchar_t **wide) {
    std::vector<std::string> values;
    std::vector<char *> args;
    for (int i = 0; i < argc; ++i) {
        int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, value.data(), size, nullptr, nullptr);
        values.push_back(std::move(value));
    }
    for (auto &value : values)
        args.push_back(value.data());
    return run(argc, args.data());
}
#else
int main(int argc, char **argv) {
    return run(argc, argv);
}
#endif
