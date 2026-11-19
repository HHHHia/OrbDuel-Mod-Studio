// OrbDuel 入口：解析命令行 → 自检 / 列内容 / 开始游戏。

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Audio.hpp"
#include "orbduel/game/Options.hpp"
#include "orbduel/game/Studio.hpp"
#include "orbduel/version.hpp"

namespace fs = std::filesystem;

namespace {

std::vector<std::string> listDirs(const fs::path &dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return names;
    }
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec)) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> listJsonFiles(const fs::path &dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return names;
    }
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void printIds(const char *label, const std::vector<std::string> &ids) {
    std::printf("%-11s(%zu):", label, ids.size());
    for (const auto &id : ids) {
        std::printf(" %s", id.c_str());
    }
    std::printf("\n");
}

// 先看命令行给的路径，再从 exe 所在目录向上找 4 层。
fs::path resolveContentRoot(const std::string &requested, const char *argv0) {
    std::error_code ec;

    fs::path candidate = fs::absolute(requested, ec);
    if (!ec && fs::is_directory(candidate / "characters", ec)) {
        return candidate;
    }

#if defined(__APPLE__)
    if (char *base = SDL_GetBasePath()) {
        const auto resources = fs::u8path(base) / "content";
        SDL_free(base);
        if (fs::is_directory(resources / "characters", ec))
            return resources;
    }
#endif
    if (argv0 != nullptr) {
        fs::path dir = fs::absolute(fs::path(argv0), ec).parent_path();
        for (int depth = 0; depth < 4; ++depth) {
            fs::path guess = dir / "content";
            if (fs::is_directory(guess / "characters", ec)) {
                return guess;
            }
            if (!dir.has_parent_path()) {
                break;
            }
            dir = dir.parent_path();
        }
    }

    return fs::absolute(requested, ec);
}

int probeWindow() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("window      FAILED: SDL_Init: %s\n", SDL_GetError());
        return 2;
    }

    SDL_Window *window = SDL_CreateWindow("orbduel selftest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          320, 200, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::printf("window      FAILED: CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RendererInfo info{};
    const char *rendererName = "none";
    if (renderer != nullptr && SDL_GetRendererInfo(renderer, &info) == 0) {
        rendererName = info.name;
    }

    std::printf("window      OK (hidden 320x200, video=%s, renderer=%s)\n", SDL_GetCurrentVideoDriver(),
                rendererName);

    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    orbduel::Options options;
    options.contentRoot = "content";

    bool selftest = false;
    bool wantWindow = false;
    bool listOnly = false;
    bool sfxTest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            selftest = true;
        } else if (arg == "--sfx-test") {
            sfxTest = true;
        } else if (arg == "--window") {
            wantWindow = true;
        } else if (arg == "--list") {
            listOnly = true;
        } else if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--hidden") {
            options.hidden = true;
        } else if (arg == "--no-menu") {
            options.startInMenu = false;
            options.play = true;
        } else if (arg == "--lang" && i + 1 < argc) {
            options.language = argv[++i];
        } else if (arg == "--shot-at" && i + 1 < argc) {
            options.shotAtSeconds = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--play") {
            options.play = true;
        } else if (arg == "--scene" && i + 1 < argc) {
            options.sceneFile = argv[++i];
        } else if (arg == "--mods" && i + 1 < argc) {
            options.modsRoot = argv[++i];
        } else if (arg == "--theme" && i + 1 < argc) {
            options.theme = argv[++i];
        } else if (arg == "--ui-skin" && i + 1 < argc) {
            options.uiSkin = argv[++i];
        } else if (arg == "--view" && i + 1 < argc) {
            options.view = argv[++i];
        } else if (arg == "--autotest" && i + 1 < argc) {
            options.autotestSeconds = static_cast<float>(std::atof(argv[++i]));
            options.headless = true;
        } else if (arg == "--content" && i + 1 < argc) {
            options.contentRoot = argv[++i];
        } else if (arg == "--map" && i + 1 < argc) {
            options.mapId = argv[++i];
        } else if (arg == "--char" && i + 1 < argc) {
            options.characterId = argv[++i];
        } else if (arg == "--enemy" && i + 1 < argc) {
            options.enemyId = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            options.seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--seconds" && i + 1 < argc) {
            options.runSeconds = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshotPath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::printf("OrbDuel %s\n", orbduel::versionString());
            std::printf("用法:\n");
            std::printf("  orbduel                         直接开一局\n");
            std::printf("  orbduel --char blaze --enemy bulwark --map duel-basic\n");
            std::printf("  orbduel --seed 1234             固定随机种子（可复现）\n");
            std::printf("  orbduel --selftest [--window]   环境自检\n");
            std::printf("  orbduel --sfx-test              依次播放全部音效（试听用）\n");
            std::printf("  orbduel --list                  只列内容包\n");
            std::printf("  orbduel --headless              不开窗口（只做逻辑）\n");
            std::printf("  orbduel --seconds 5             跑 5 秒后自动退出\n");
            std::printf("  orbduel --scene user/arena.json  读取工作台地图\n");
            std::printf("  orbduel --autotest 600          快速无窗口物理回归\n");
            std::printf("  orbduel --no-menu               Alias for --play\n");
            std::printf("  orbduel --lang en|zh            UI, fighter and skill language (default en)\n");
            std::printf("  orbduel --ui-skin aurora        Use a UI skin from content/ui/skins\n");
            std::printf("  orbduel --shot-at 11            Screenshot at 11 seconds\n");
            std::printf("  orbduel --play                  直接开始自动对战\n");
            return 0;
        }
    }

    const fs::path root = resolveContentRoot(options.contentRoot, argc > 0 ? argv[0] : nullptr);
    options.contentRoot = root.string();

    if (sfxTest) {
        // 试听：把音效目录里的 WAV 按文件名各放一遍，用来确认设备和素材都正常。
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(root / "audio" / "orbduel", ec))
            if (entry.is_regular_file(ec) && entry.path().extension() == ".wav")
                names.push_back(entry.path().stem().u8string());
        std::sort(names.begin(), names.end());
        orbduel::Audio audio;
        if (!audio.init((root / "audio" / "orbduel").u8string())) {
            std::printf("audio       FAILED\n");
            return 4;
        }
        std::printf("audio       OK (%zu 个音效)\n", names.size());
        for (const auto &name : names) {
            std::printf("  %s\n", name.c_str());
            audio.play(name.c_str(), .8f);
            SDL_Delay(450);
        }
        SDL_Delay(600);
        audio.shutdown();
        return 0;
    }

    if (listOnly || selftest) {
        std::printf("OrbDuel %s\n", orbduel::versionString());
        std::printf("SDL         %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
        std::printf("content     %s\n", root.string().c_str());
        printIds("characters", listDirs(root / "characters"));
        printIds("skills", listJsonFiles(root / "skills"));
        printIds("maps", listDirs(root / "maps"));

        int exitCode = 0;
        if (listDirs(root / "characters").empty() || listDirs(root / "maps").empty()) {
            std::printf("status      FAILED: content/characters 或 content/maps 为空\n");
            exitCode = 3;
        } else {
            std::printf("status      OK\n");
        }

        if (selftest && wantWindow) {
            exitCode = probeWindow();
        }
        return exitCode;
    }

    return orbduel::runStudio(options);
}
