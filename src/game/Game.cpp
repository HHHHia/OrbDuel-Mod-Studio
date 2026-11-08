#include "orbduel/game/Game.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/core/Event.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/core/Time.hpp"
#include "orbduel/match/Match.hpp"
#include "orbduel/physics/Arena.hpp"
#include "orbduel/physics/World.hpp"
#include "orbduel/render/AssetCache.hpp"
#include "orbduel/render/Fx.hpp"
#include "orbduel/render/Renderer.hpp"

namespace orbduel {

Game::Game() = default;
Game::~Game() = default;

namespace {

constexpr Rgba kHudBack{18, 20, 30, 220};
constexpr Rgba kHudBorder{90, 105, 150, 255};
constexpr Rgba kHudText{235, 240, 255, 255};
constexpr Rgba kHudDim{150, 160, 190, 255};

void drawHpBar(Renderer& renderer, Rect area, float ratio, Rgba fillColor, bool mirror) {
    renderer.drawRect(area, kHudBack);

    const float clamped = clampf(ratio, 0.0f, 1.0f);
    Rect fill = area;
    fill.w = area.w * clamped;
    if (mirror) {
        fill.x = area.x + area.w - fill.w;
    }
    renderer.drawRect(fill, fillColor);
    renderer.drawRectLines(area, 1.0f, kHudBorder);
}

std::string hpText(const Fighter& fighter) {
    const int hp = static_cast<int>(std::ceil(fighter.hp));
    const int maxHp = static_cast<int>(fighter.maxHp);
    return std::to_string(hp) + "/" + std::to_string(maxHp);
}

} // namespace

int Game::run(const Options& options) {
    ContentLoader loader(options.contentRoot);
    if (!loader.loadAll()) {
        log::error("内容加载失败，检查 " + options.contentRoot + " 目录");
        return 3;
    }

    const ContentPack& pack = loader.pack();

    const MapDef* map = nullptr;
    if (options.mapId.empty()) {
        map = &pack.maps.front();
    } else {
        map = pack.findMap(options.mapId);
    }
    if (map == nullptr) {
        log::error("找不到地图: " + options.mapId);
        return 3;
    }

    const CharacterDef* left = nullptr;
    if (options.characterId.empty()) {
        left = &pack.characters.front();
    } else {
        left = pack.findCharacter(options.characterId);
    }
    if (left == nullptr) {
        log::error("找不到角色: " + options.characterId);
        return 3;
    }

    const CharacterDef* right = nullptr;
    if (options.enemyId.empty()) {
        right = pack.characters.size() > 1 ? &pack.characters[1] : left;
    } else {
        right = pack.findCharacter(options.enemyId);
    }
    if (right == nullptr) {
        log::error("找不到对手角色: " + options.enemyId);
        return 3;
    }

    std::uint32_t seed = options.seed;
    if (seed == 0) {
        seed = map->seed != 0 ? map->seed : 20261124u;
    }
    Rng rng(seed);
    log::info("seed = " + std::to_string(seed));

    Renderer renderer;
    if (!renderer.init(static_cast<int>(map->size.x), static_cast<int>(map->size.y),
                       "OrbDuel", options.headless)) {
        log::error(std::string("SDL 初始化失败: ") + SDL_GetError());
        return 2;
    }

    const std::string fontPath = loader.root() + "/fonts/PressStart2P-Regular.ttf";
    if (!renderer.loadFont(fontPath, 10)) {
        log::warn("字体加载失败，HUD 文字不会显示: " + fontPath);
    }

    AssetCache assets(renderer.backend());

    EventBus events;
    Arena arena;
    World world(events, arena);
    Match match(events, world, arena);
    Fx fx;
    fx.bind(events);

    // 命中事件 → 结算伤害（Match 不是事件源，必须显式订阅）
    events.subscribe(EventType::Hit, [&match](const Event& event) { match.onEvent(event); });

    auto setupMatch = [&]() {
        match.setup(*left, *right, *map, pack, rng);
        match.start();
        fx.clear();
    };
    setupMatch();

    const std::string leftTexture = loader.root() + "/" + left->sourceDir + "/" + left->skin.texture;
    const std::string rightTexture = loader.root() + "/" + right->sourceDir + "/" + right->skin.texture;
    assets.get(leftTexture);
    assets.get(rightTexture);

    Clock clock(1.0f / 120.0f, 8);
    std::uint32_t lastTicks = SDL_GetTicks();
    float fpsTimer = 0.0f;
    float fpsValue = 0.0f;
    int fpsFrames = 0;
    float totalSeconds = 0.0f;

    while (!renderer.shouldClose()) {
        const std::uint32_t now = SDL_GetTicks();
        float frameSeconds = static_cast<float>(now - lastTicks) / 1000.0f;
        lastTicks = now;
        if (frameSeconds > 0.05f) {
            frameSeconds = 0.05f;
        }
        totalSeconds += frameSeconds;

        if (options.runSeconds > 0.0f && totalSeconds >= options.runSeconds) {
            renderer.requestClose();
            break;
        }

        renderer.pumpEvents();
        if (renderer.wasKeyPressed(SDL_SCANCODE_ESCAPE)) {
            renderer.requestClose();
            break;
        }
        if (renderer.wasKeyPressed(SDL_SCANCODE_R)) {
            setupMatch();
        }

        clock.tick(frameSeconds);

        if (fx.hitStopActive()) {
            fx.consumeHitStop();
            clock.tick(-frameSeconds * 0.85);
        }

        const float stepDt = clock.fixedDt();
        const int steps = clock.consumeSteps();
        for (int step = 0; step < steps; ++step) {
            match.update(stepDt);
            world.step(stepDt);

            const std::vector<int> crushed = arena.update(stepDt);
            for (int obstacleId : crushed) {
                Event event;
                event.type = EventType::ObstacleCrushed;
                event.a = obstacleId;
                event.pos = arena.bounds().center();
                events.publish(event);
            }

            events.dispatch();
            fx.update(stepDt);

            if (step % 6 == 0) {
                for (int i = 0; i < 2; ++i) {
                    const Body* body = world.body(match.fighter(i).bodyId);
                    if (body == nullptr || !body->active) {
                        continue;
                    }
                    if (lengthOf(body->vel) < 260.0f) {
                        continue;
                    }
                    const Rgba trailColor = i == 0 ? Rgba{255, 150, 80, 70} : Rgba{110, 180, 255, 70};
                    fx.spawnTrail(body->pos, body->radius * 0.55f, trailColor);
                }
            }
        }

        // ---- 绘制 ----
        renderer.beginFrame(map->theme.bgColor);

        const Rect bounds = arena.bounds();
        renderer.drawRectLines(bounds, map->theme.wallThickness, map->theme.wallColor);

        for (const Obstacle& obstacle : arena.obstacles()) {
            const Rgba color{40, 44, 60, 255};
            if (obstacle.shape == ObstacleShape::Rect) {
                Rect rect;
                rect.x = obstacle.pos.x - obstacle.size.x * 0.5f;
                rect.y = obstacle.pos.y - obstacle.size.y * 0.5f;
                rect.w = obstacle.size.x;
                rect.h = obstacle.size.y;
                renderer.drawRect(rect, color);
                renderer.drawRectLines(rect, 1.0f, Rgba{80, 90, 130, 255});
            } else {
                renderer.drawCircle(obstacle.pos, obstacle.radius, color);
                renderer.drawCircleLines(obstacle.pos, obstacle.radius, Rgba{80, 90, 130, 255});
            }
        }

        for (int i = 0; i < 2; ++i) {
            const Fighter& fighter = match.fighter(i);
            const Body* body = world.body(fighter.bodyId);
            if (body == nullptr || !body->active || fighter.def == nullptr) {
                continue;
            }

            const std::string& path = i == 0 ? leftTexture : rightTexture;
            SDL_Texture* texture = assets.get(path);
            if (texture == nullptr) {
                const Rgba fallback = i == 0 ? Rgba{255, 140, 60, 255} : Rgba{90, 160, 255, 255};
                renderer.drawCircle(body->pos, body->radius, fallback);
                continue;
            }

            int texWidth = 1;
            int texHeight = 1;
            SDL_QueryTexture(texture, nullptr, nullptr, &texWidth, &texHeight);
            const float scale = (body->radius * 2.0f * fighter.def->skin.scale) /
                                static_cast<float>(std::max(1, texWidth));

            const float angle = radToDeg(std::atan2(body->vel.y, body->vel.x));
            renderer.drawSprite(texture, body->pos, angle, scale, fighter.def->skin.tint);
        }

        fx.draw(renderer);

        // ---- HUD ----
        const float barWidth = 340.0f;
        const float barHeight = 16.0f;
        const float margin = 20.0f;

        Rect leftBar{margin, margin, barWidth, barHeight};
        Rect rightBar{map->size.x - margin - barWidth, margin, barWidth, barHeight};

        drawHpBar(renderer, leftBar, match.fighter(0).hp / match.fighter(0).maxHp,
                  Rgba{255, 130, 60, 255}, false);
        drawHpBar(renderer, rightBar, match.fighter(1).hp / match.fighter(1).maxHp,
                  Rgba{90, 160, 255, 255}, true);

        renderer.drawText(left->name.en.empty() ? left->id : left->name.en,
                          Vec2{leftBar.x, leftBar.y + barHeight + 6.0f}, 8, kHudText);
        renderer.drawText(right->name.en.empty() ? right->id : right->name.en,
                          Vec2{rightBar.x, rightBar.y + barHeight + 6.0f}, 8, kHudText);

        renderer.drawText(hpText(match.fighter(0)), Vec2{leftBar.x + 6.0f, leftBar.y + 3.0f}, 8, kHudText);
        renderer.drawText(hpText(match.fighter(1)),
                          Vec2{rightBar.x + rightBar.w - 6.0f - 60.0f, rightBar.y + 3.0f}, 8, kHudText);

        renderer.drawText("T " + std::to_string(static_cast<int>(match.elapsed())),
                          Vec2{map->size.x * 0.5f - 18.0f, margin}, 8, kHudDim);

        if (match.zone().warningActive()) {
            const Rgba warn{255, 70, 70, 255};
            renderer.drawRectLines(bounds, 3.0f, warn);
            renderer.drawText("ZONE CLOSING",
                              Vec2{map->size.x * 0.5f - 110.0f, margin + 22.0f}, 8, warn);
        }

        if (match.phase() == MatchPhase::Finished) {
            renderer.drawRect(Rect{0.0f, 0.0f, map->size.x, map->size.y}, Rgba{8, 9, 14, 190});

            const MatchResult& result = match.result();
            std::string headline = "DRAW";
            if (result.winnerId >= 0) {
                headline = (result.winnerId == match.fighter(0).bodyId)
                               ? (left->name.en.empty() ? left->id : left->name.en) + " WINS"
                               : (right->name.en.empty() ? right->id : right->name.en) + " WINS";
            }

            const int headlineWidth = renderer.measureText(headline, 16);
            renderer.drawText(headline,
                              Vec2{(map->size.x - static_cast<float>(headlineWidth)) * 0.5f,
                                   map->size.y * 0.42f},
                              16, kHudText);

            const std::string hint = "PRESS R TO RESTART";
            const int hintWidth = renderer.measureText(hint, 8);
            renderer.drawText(hint,
                              Vec2{(map->size.x - static_cast<float>(hintWidth)) * 0.5f,
                                   map->size.y * 0.42f + 40.0f},
                              8, kHudDim);
        }

        renderer.endFrame();

        if (!options.screenshotPath.empty() && totalSeconds >= 2.0f) {
            if (renderer.saveScreenshot(options.screenshotPath)) {
                log::info("screenshot saved: " + options.screenshotPath);
            } else {
                log::warn("screenshot failed: " + options.screenshotPath);
            }
            break;
        }

        // 帧率统计
        fpsFrames += 1;
        fpsTimer += frameSeconds;
        if (fpsTimer >= 0.5f) {
            fpsValue = static_cast<float>(fpsFrames) / fpsTimer;
            fpsFrames = 0;
            fpsTimer = 0.0f;
        }
        (void)fpsValue;
    }

    log::info("run finished: phase=" + std::to_string(static_cast<int>(match.phase())) +
              " elapsed=" + std::to_string(static_cast<int>(match.elapsed())) +
              "s hits=" + std::to_string(match.hitCount()) +
              " hp0=" + std::to_string(static_cast<int>(std::ceil(match.fighter(0).hp))) +
              " hp1=" + std::to_string(static_cast<int>(std::ceil(match.fighter(1).hp))));
    return 0;
}

} // namespace orbduel
