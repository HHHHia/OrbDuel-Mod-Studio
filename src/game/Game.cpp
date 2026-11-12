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
#include "orbduel/game/Menu.hpp"
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

// 结算后多久自动回主菜单（秒）
constexpr float kReturnToMenuSeconds = 5.0f;

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

// 技能名飘字：施放瞬间弹在球上方，上浮 + 淡出
struct CastText {
    int bodyId = -1;   // 跟着球走，否则冲刺一冲出去字就留在原地了
    Vec2 pos{};
    std::string text;
    float age = 0.0f;
};

constexpr float kCastTextSeconds = 0.9f;

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

    // 技能名飘字：Match 只发事件，文字内容在这里查（技能名在 SkillDef 里）
    std::vector<CastText> castTexts;
    events.subscribe(EventType::SkillCast, [&match, &castTexts](const Event& event) {
        const int slot = static_cast<int>(event.value);
        for (int i = 0; i < 2; ++i) {
            if (match.fighter(i).bodyId != event.a) {
                continue;
            }
            const std::vector<SkillState>& states = match.skills(i).states();
            if (slot < 0 || slot >= static_cast<int>(states.size()) || states[static_cast<std::size_t>(slot)].def == nullptr) {
                return;
            }
            castTexts.push_back(CastText{event.a, event.pos, states[static_cast<std::size_t>(slot)].def->name.en, 0.0f});
            return;
        }
    });

    auto setupMatch = [&]() {
        match.setup(*left, *right, *map, pack, rng);
        match.start();
        fx.clear();
    };
    setupMatch();

    std::string leftTexture = loader.root() + "/" + left->sourceDir + "/" + left->skin.texture;
    std::string rightTexture = loader.root() + "/" + right->sourceDir + "/" + right->skin.texture;
    assets.get(leftTexture);
    assets.get(rightTexture);

    Menu menu;
    menu.open(pack, left->id, right->id);
    // headless 没有任何输入来源，停在菜单等于什么都不做，所以直接开局。
    bool inMenu = options.startInMenu && !options.headless;

    // 菜单点了「开始游戏」：换角色 → 换贴图 → 重新开局。
    auto startFromMenu = [&]() -> bool {
        const CharacterDef* chosenLeft = pack.findCharacter(menu.leftId());
        const CharacterDef* chosenRight = pack.findCharacter(menu.rightId());
        if (chosenLeft == nullptr || chosenRight == nullptr) {
            return false;
        }
        left = chosenLeft;
        right = chosenRight;
        leftTexture = loader.root() + "/" + left->sourceDir + "/" + left->skin.texture;
        rightTexture = loader.root() + "/" + right->sourceDir + "/" + right->skin.texture;
        assets.get(leftTexture);
        assets.get(rightTexture);
        setupMatch();
        return true;
    };

    Clock clock(1.0f / 120.0f, 8);
    std::uint32_t lastTicks = SDL_GetTicks();
    float fpsTimer = 0.0f;
    float fpsValue = 0.0f;
    int fpsFrames = 0;
    float totalSeconds = 0.0f;
    float finishedSeconds = 0.0f;

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

        // ESC 不再退出进程：对局里 ESC 回菜单，菜单里 ESC 只关确认框。
        if (!inMenu && renderer.wasKeyPressed(SDL_SCANCODE_ESCAPE)) {
            inMenu = true;
            menu.open(pack, left->id, right->id);
        }

        if (inMenu) {
            menu.update(renderer);
            if (menu.wantsQuit()) {
                renderer.requestClose();
                break;
            }
            if (menu.wantsStart()) {
                menu.clearRequests();
                if (startFromMenu()) {
                    finishedSeconds = 0.0f;
                    inMenu = false;
                }
            }

            renderer.beginFrame(menu.background());
            menu.draw(renderer, assets, loader.root());
            renderer.endFrame();
        } else {
            if (renderer.wasKeyPressed(SDL_SCANCODE_R)) {
                setupMatch();
                finishedSeconds = 0.0f;
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

            for (CastText& cast : castTexts) {
                cast.age += frameSeconds;
            }
            castTexts.erase(std::remove_if(castTexts.begin(), castTexts.end(),
                                           [](const CastText& cast) {
                                               return cast.age >= kCastTextSeconds;
                                           }),
                            castTexts.end());

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

            // 蓄力虚线：指向延迟后的锁定点，抖动让落点没法完全预判
            for (int i = 0; i < 2; ++i) {
                const Body* body = world.body(match.fighter(i).bodyId);
                if (body == nullptr) {
                    continue;
                }
                for (const SkillState& state : match.skills(i).states()) {
                    if (!state.charging) {
                        continue;
                    }
                    for (float d = 0.0f; d < 460.0f; d += 22.0f) {
                        renderer.drawLine(body->pos + state.aimDir * d,
                                          body->pos + state.aimDir * (d + 12.0f), 2.0f,
                                          Rgba{255, 215, 120, 170});
                    }
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

                // 同角色互打时右侧整体偏蓝，否则两个一模一样的球分不清
                Rgba tint = fighter.def->skin.tint;
                if (i == 1 && left->id == right->id) {
                    tint.r = static_cast<unsigned char>(tint.r * 140 / 255);
                    tint.g = static_cast<unsigned char>(tint.g * 190 / 255);
                }

                const float angle = radToDeg(std::atan2(body->vel.y, body->vel.x));
                renderer.drawSprite(texture, body->pos, angle, scale, tint);
            }

            fx.draw(renderer);

            for (const CastText& cast : castTexts) {
                const float k = clampf(cast.age / kCastTextSeconds, 0.0f, 1.0f);
                const Rgba color{255, 235, 150, static_cast<unsigned char>((1.0f - k) * 255.0f)};
                const Body* body = world.body(cast.bodyId);
                const Vec2 anchor = body != nullptr ? body->pos : cast.pos;
                const int width = renderer.measureText(cast.text, 8);
                renderer.drawText(cast.text,
                                  Vec2{anchor.x - static_cast<float>(width) * 0.5f,
                                       anchor.y - 56.0f - k * 20.0f},
                                  8, color);
            }

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
                finishedSeconds += frameSeconds;
                if (finishedSeconds >= kReturnToMenuSeconds) {
                    inMenu = true;
                    menu.open(pack, left->id, right->id);
                } else {
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

                    const int remaining = static_cast<int>(kReturnToMenuSeconds - finishedSeconds);
                    const std::string hint = "RETURNING IN " + std::to_string(remaining) +
                                             "   R = REMATCH";
                    const int hintWidth = renderer.measureText(hint, 8);
                    renderer.drawText(hint,
                                      Vec2{(map->size.x - static_cast<float>(hintWidth)) * 0.5f,
                                           map->size.y * 0.42f + 40.0f},
                                      8, kHudDim);
                }
            } else {
                finishedSeconds = 0.0f;
            }

            renderer.endFrame();
        }

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
