// OrbDuel 入口：解析命令行 → 自检 / 列内容 / 开始游戏。

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "orbduel/game/Game.hpp"
#include "orbduel/version.hpp"

namespace fs = std::filesystem;

namespace {

std::vector<std::string> listDirs(const fs::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return names;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec)) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> listJsonFiles(const fs::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return names;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void printIds(const char* label, const std::vector<std::string>& ids) {
    std::printf("%-11s(%zu):", label, ids.size());
    for (const auto& id : ids) {
        std::printf(" %s", id.c_str());
    }
    std::printf("\n");
}

// 先看命令行给的路径，再从 exe 所在目录向上找 4 层。
fs::path resolveContentRoot(const std::string& requested, const char* argv0) {
    std::error_code ec;

    fs::path candidate = fs::absolute(requested, ec);
    if (!ec && fs::is_directory(candidate / "characters", ec)) {
        return candidate;
    }

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

    SDL_Window* window = SDL_CreateWindow("orbduel selftest",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          320, 200, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::printf("window      FAILED: CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RendererInfo info{};
    const char* rendererName = "none";
    if (renderer != nullptr && SDL_GetRendererInfo(renderer, &info) == 0) {
        rendererName = info.name;
    }

    std::printf("window      OK (hidden 320x200, video=%s, renderer=%s)\n",
                SDL_GetCurrentVideoDriver(), rendererName);

    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    orbduel::Options options;
    options.contentRoot = "content";

    bool selftest = false;
    bool wantWindow = false;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            selftest = true;
        } else if (arg == "--window") {
            wantWindow = true;
        } else if (arg == "--list") {
            listOnly = true;
        } else if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--no-menu") {
            options.startInMenu = false;
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
            std::printf("  orbduel --char lancer --enemy bulwark --map duel-basic\n");
            std::printf("  orbduel --seed 1234             固定随机种子（可复现）\n");
            std::printf("  orbduel --selftest [--window]   环境自检\n");
            std::printf("  orbduel --list                  只列内容包\n");
            std::printf("  orbduel --headless              不开窗口（只做逻辑）\n");
            std::printf("  orbduel --no-menu               跳过主菜单直接开局\n");
            std::printf("  orbduel --seconds 5             跑 5 秒后自动退出\n");
            return 0;
        }
    }

    const fs::path root = resolveContentRoot(options.contentRoot, argc > 0 ? argv[0] : nullptr);
    options.contentRoot = root.string();

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

    return orbduel::Game().run(options);
}
