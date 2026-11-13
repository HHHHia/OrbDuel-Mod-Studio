#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/match/Simulation.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
using namespace orbduel;
namespace fs = std::filesystem;
using nlohmann::json;
struct Score {
    int games = 0, wins = 0, draws = 0, timeouts = 0;
    double seconds = 0, hp = 0, damage = 0, absorbed = 0;
};
int main(int argc, char **argv) {
    try {
        if (argc < 6) {
            std::cerr << "content mods output.json seeds firstSeed [tuning.json]\n";
            return 2;
        }
        ContentLoader loader(argv[1]);
        if (!loader.loadAll())
            return 2;
        auto pack = loader.pack();
        PluginManager plugins;
        plugins.load(argv[2]);
        log::setConsole(false);
        const int seeds = std::max(1, std::atoi(argv[4])), firstSeed = std::atoi(argv[5]);
        if (argc > 6) {
            std::ifstream f(fs::u8path(argv[6]));
            json overrides;
            f >> overrides;
            for (auto &c : pack.characters)
                if (overrides.contains(c.id)) {
                    auto j = overrides[c.id];
                    c.stats.attack = j.value("attack", c.stats.attack);
                    c.stats.maxHp = j.value("hp", c.stats.maxHp);
                    c.stats.armor = j.value("armor", c.stats.armor);
                }
        }
        const int n = int(pack.characters.size());
        std::vector<Score> totals(n);
        std::vector<std::vector<Score>> pair(n, std::vector<Score>(n));
        const char *maps[] = {"open", "obstacles", "mechanisms", "shrinking"};
        std::vector<std::vector<Score>> byMap(4, std::vector<Score>(n));
        json roster = json::array();
        for (auto &c : pack.characters)
            roster.push_back({{"id", c.id},
                              {"name", c.name.zh},
                              {"hp", c.stats.maxHp},
                              {"attack", c.stats.attack},
                              {"armor", c.stats.armor},
                              {"speed", c.stats.startSpeed},
                              {"mass", c.stats.mass},
                              {"radius", c.hitbox.radius},
                              {"restitution", c.stats.restitution},
                              {"skills", c.skills}});
        auto begin = std::chrono::steady_clock::now();
        int totalGames = 0, leftWins = 0, draws = 0;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b) {
                for (int map = 0; map < 4; ++map)
                    for (int seed = firstSeed; seed < firstSeed + seeds; ++seed)
                        for (int side = 0; side < 2; ++side) {
                            auto scene = SceneDocument::fromPack(pack);
                            scene.timeLimit = 60;
                            scene.shrink = map == 3;
                            const auto mapSeed = std::uint32_t(seed + map * 100003);
                            generateScene(scene, mapSeed, map == 0 ? 0 : 3, map >= 2 ? 3 : 0,
                                          map >= 2 ? plugins.generationTypes()
                                                   : std::vector<GenerationType>{});
                            scene.fighters[0] = pack.characters[side ? b : a];
                            scene.fighters[1] = pack.characters[side ? a : b];
                            Simulation sim(scene, pack, plugins);
                            for (int step = 0; step < 7202 && sim.match.phase() == MatchPhase::Fighting;
                                 ++step)
                                sim.step();
                            if (sim.match.phase() != MatchPhase::Finished)
                                throw std::runtime_error("match did not finish");
                            for (int i = 0; i < 2; ++i) {
                                int me = i == 0 ? (side ? b : a) : (side ? a : b), other = me == a ? b : a;
                                auto &f = sim.match.fighter(i);
                                for (Score *s : {&totals[me], &pair[me][other], &byMap[map][me]}) {
                                    ++s->games;
                                    s->wins += sim.match.result().winnerId == f.bodyId;
                                    s->draws += sim.match.result().winnerId < 0;
                                    s->timeouts += sim.match.result().reason == "time_limit";
                                    s->seconds += sim.match.elapsed();
                                    s->hp += f.hp / f.maxHp;
                                    s->damage += f.damageDealt;
                                    s->absorbed += f.damageAbsorbed;
                                }
                            }
                            ++totalGames;
                            leftWins += sim.match.result().winnerId == 0;
                            draws += sim.match.result().winnerId < 0;
                        }
                std::cout << pack.characters[a].id << " vs " << pack.characters[b].id << ": " << std::fixed
                          << std::setprecision(1)
                          << 100. * (pair[a][b].wins + .5 * pair[a][b].draws) / pair[a][b].games << "%\n"
                          << std::flush;
            }
        auto toJson = [](const Score &s) {
            return json{{"games", s.games},
                        {"wins", s.wins},
                        {"draws", s.draws},
                        {"winRate", s.games ? 100. * (s.wins + .5 * s.draws) / s.games : 0},
                        {"timeoutRate", s.games ? 100. * s.timeouts / s.games : 0},
                        {"avgSeconds", s.games ? s.seconds / s.games : 0},
                        {"avgRemainingHp", s.games ? s.hp / s.games : 0},
                        {"avgDamage", s.games ? s.damage / s.games : 0},
                        {"avgAbsorbed", s.games ? s.absorbed / s.games : 0}};
        };
        json result = {
            {"seedsPerMap", seeds},       {"firstSeed", firstSeed},
            {"swappedSides", true},       {"maps", maps},
            {"games", totalGames},        {"leftSideScore", 100. * (leftWins + .5 * draws) / totalGames},
            {"roster", roster},           {"characters", json::object()},
            {"matchups", json::object()}, {"byMap", json::object()}};
        for (int i = 0; i < n; ++i) {
            const auto &id = pack.characters[i].id;
            result["characters"][id] = toJson(totals[i]);
            std::cout << id << ": " << result["characters"][id]["winRate"] << "%\n";
            for (int j = 0; j < n; ++j)
                if (i != j)
                    result["matchups"][id][pack.characters[j].id] = toJson(pair[i][j]);
            for (int m = 0; m < 4; ++m)
                result["byMap"][maps[m]][id] = toJson(byMap[m][i]);
        }
        result["wallSeconds"] =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        result["skills"] = json::array();
        for (const auto &s : pack.skills) {
            json mods = json::array();
            for (const auto &m : s.modifiers)
                mods.push_back({{"op", int(m.op)},
                                {"value", m.value},
                                {"duration", m.duration},
                                {"radius", m.radius},
                                {"factor", m.factor}});
            result["skills"].push_back({{"id", s.id},
                                        {"trigger", int(s.trigger.kind)},
                                        {"cooldown", s.trigger.cooldown},
                                        {"hpBelow", s.trigger.hpBelow},
                                        {"modifiers", mods}});
        }
        fs::path out = fs::u8path(argv[3]);
        if (out.has_parent_path())
            fs::create_directories(out.parent_path());
        std::ofstream f(out);
        f << result.dump(2);
        return f ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
