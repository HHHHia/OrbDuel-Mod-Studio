#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/Scene.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace orbduel;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        if (argc < 4)
            return 2;
        log::setConsole(false);
        ContentLoader loader(argv[1]);
        check(loader.loadAll(), "content failed");
        const auto &pack = loader.pack();
        PluginManager plugins;
        plugins.load(argv[2]);
        check(plugins.issues.empty(), "initial plugin load failed");
        auto *type = plugins.find("rookie-vortex.vortex");
        check(type && plugins.defaults(*type).at("strength") == 300, "initial configured default missing");
        check(std::string(type->api.name) == u8"逆向旋风", "configured Unicode label not registered");
        check(std::filesystem::equivalent(std::filesystem::u8path(type->assetRoot),
                                          std::filesystem::u8path(argv[2]) / "rookie-vortex"),
              "assets redirected into a DLL cache");
        std::ifstream asset(std::filesystem::u8path(type->assetRoot) / "assets" / "sample [data]" /
                            "example.txt");
        std::string sample;
        std::getline(asset, sample);
        check(sample == "public sample", "public assets were not installed at their original root");
        SceneDocument scene = SceneDocument::fromPack(pack);
        scene.map.obstacles.clear();
        scene.map.physics.damageScale = 0;
        scene.stall = false;
        scene.shrink = false;
        scene.map.spawns[0].pos = {400, 360};
        scene.map.spawns[1].pos = {900, 360};
        for (auto &f : scene.fighters) {
            f.skills.clear();
            f.stats.startSpeed = 0;
        }
        SceneObject vortex;
        vortex.type = type->api.id;
        vortex.pos = {350, 360};
        vortex.radius = 200;
        vortex.params = plugins.defaults(*type);
        scene.objects = {vortex};
        scene.mods = {"rookie-vortex"};
        auto live = std::make_unique<Simulation>(scene, pack, plugins);
        for (int i = 0; i < 12; ++i)
            live->step();
        check(live->world.body(0)->vel.y > 20, "initial vortex has no positive effect");
        std::cout << "READY" << std::endl;
        std::string command;
        check(bool(std::getline(std::cin, command)) && command == "reload", "reload command missing");
        // 与工作台相同：先销毁实例，再卸载并重新发现 DLL。
        live.reset();
        plugins.load(argv[2]);
        check(plugins.issues.empty(), "reload failed");
        type = plugins.find("rookie-vortex.vortex");
        check(type && plugins.defaults(*type).at("strength") == -600, "updated DLL default not loaded");
        check(plugins.find("other-vortex.vortex") != nullptr, "second project collides with first");
        check(scene.objects[0].params.at("strength") == 300, "reload changed an edited scene");
        {
            Simulation old(scene, pack, plugins);
            for (int i = 0; i < 12; ++i)
                old.step();
            check(old.world.body(0)->vel.y > 20, "saved object lost its explicit parameter");
        }
        scene.objects[0].params = plugins.defaults(*type);
        live = std::make_unique<Simulation>(scene, pack, plugins);
        for (int i = 0; i < 12; ++i)
            live->step();
        check(live->world.body(0)->vel.y < -40, "new object did not reverse direction");
        std::string error;
        check(saveScene(argv[3], scene, error), "cannot save installed-mod scene");
        std::cout << "RELOADED" << std::endl;
        check(bool(std::getline(std::cin, command)) && command == "verify", "verify command missing");
        live.reset();
        plugins.load(argv[2]);
        check(plugins.issues.empty(), "failed build damaged installed plugin");
        type = plugins.find("rookie-vortex.vortex");
        check(type && plugins.defaults(*type).at("strength") == -600,
              "failed build replaced the working mod");
        live = std::make_unique<Simulation>(scene, pack, plugins);
        for (int i = 0; i < 12; ++i)
            live->step();
        check(live->world.body(0)->vel.y < -40, "installed behavior was damaged");
        std::cout << "DONE" << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
