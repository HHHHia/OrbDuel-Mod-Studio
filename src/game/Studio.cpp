#include "orbduel/game/Studio.hpp"
#include "BodyShape.hpp"
#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/Scene.hpp"
#include "orbduel/core/Time.hpp"
#include "orbduel/match/Simulation.hpp"
#include "orbduel/render/AssetCache.hpp"
#include "orbduel/render/Renderer.hpp"
#include "orbduel/render/Themes.hpp"
#include <SDL.h>
#include <algorithm>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <memory>
namespace orbduel {
namespace fs = std::filesystem;
namespace {
const char *themes[] = {"cosmos", "dungeon", "neon", "lava", "ice"};
const char *labels[] = {"星空", "地下城", "霓虹都市", "熔岩遗迹", "冰晶圣殿"};
ImU32 ink(Rgba c) {
    return IM_COL32(c.r, c.g, c.b, c.a);
}
ImVec4 color(float r, float g, float b, float a = 1) {
    return {r, g, b, a};
}
bool chineseNames = false;
const char *displayName(const LocalizedText &name) {
    return chineseNames && !name.zh.empty() ? name.zh.c_str() : name.en.c_str();
}
// 内容指定的通用徽记，不按角色 ID 判断外观。
void emblem(ImDrawList *d, ImVec2 center, float radius, const std::string &pattern, ImU32 tint) {
    if (pattern.empty())
        return;
    const float r = radius * .65f;
    auto p = [&](float x, float y) { return ImVec2(center.x + x * r, center.y + y * r); };
    if (pattern == "hex" || pattern == "crystal" || pattern == "spark") {
        int n = pattern == "hex" ? 6 : (pattern == "spark" ? 12 : 4);
        std::vector<ImVec2> points;
        for (int i = 0; i < n; ++i) {
            float a = -kPi * .5f + 2 * kPi * i / n;
            float scale = pattern == "spark" && i % 2 ? .38f : 1;
            points.push_back(p(std::cos(a) * scale, std::sin(a) * scale));
        }
        d->AddPolyline(points.data(), int(points.size()), tint, ImDrawFlags_Closed, 2);
        if (pattern == "crystal") {
            d->AddLine(p(0, -1), p(0, 1), tint, 1.5f);
            d->AddLine(p(-1, 0), p(1, 0), tint, 1.5f);
        }
    } else if (pattern == "chevron") {
        for (int i = 0; i < 3; ++i) {
            float x = (i - 1) * .55f;
            d->AddLine(p(x - .3f, -.65f), p(x + .25f, 0), tint, 2);
            d->AddLine(p(x + .25f, 0), p(x - .3f, .65f), tint, 2);
        }
    } else if (pattern == "leaf") {
        d->AddBezierCubic(p(-.7f, .7f), p(-1, -.8f), p(.8f, -1), p(.7f, -.7f), tint, 2);
        d->AddBezierCubic(p(.7f, -.7f), p(1, .8f), p(-.8f, 1), p(-.7f, .7f), tint, 2);
        d->AddLine(p(-.8f, .8f), p(.65f, -.65f), tint, 1.5f);
    } else {
        ImVec2 points[] = {p(0, -1),     p(.2f, -.2f), p(.65f, -.55f), p(.8f, .3f), p(.4f, .9f),
                           p(-.4f, .9f), p(-.8f, .3f), p(-.35f, -.3f), p(-.2f, .3f)};
        d->AddPolyline(points, 9, tint, ImDrawFlags_Closed, 2);
    }
}
struct View {
    ImDrawList *d;
    float scale;
    Vec2 origin;
    AssetCache *assets;
    Simulation *sim;
    ImVec2 at(Vec2 p) const {
        return {origin.x + p.x * scale, origin.y + p.y * scale};
    }
    Vec2 world(ImVec2 p) const {
        return {(p.x - origin.x) / scale, (p.y - origin.y) / scale};
    }
    orbmod::Draw api() {
        return {this,
                [](void *p, orbmod::Vec c, float r, orbmod::Color k, int fill) {
                    auto &v = *static_cast<View *>(p);
                    auto a = v.at({c.x, c.y});
                    r = std::max(.1f, r * v.scale);
                    if (fill)
                        v.d->AddCircleFilled(a, r, IM_COL32(k.r, k.g, k.b, k.a), 40);
                    else
                        v.d->AddCircle(a, r, IM_COL32(k.r, k.g, k.b, k.a), 40, 1.5f);
                },
                [](void *p, orbmod::Vec a, orbmod::Vec b, orbmod::Color k, float w) {
                    auto &v = *static_cast<View *>(p);
                    v.d->AddLine(v.at({a.x, a.y}), v.at({b.x, b.y}), IM_COL32(k.r, k.g, k.b, k.a),
                                 std::max(1.f, w * v.scale));
                },
                [](void *p, orbmod::Vec c, orbmod::Vec s, orbmod::Color k, int fill) {
                    auto &v = *static_cast<View *>(p);
                    auto a = v.at({c.x - s.x / 2, c.y - s.y / 2}), b = v.at({c.x + s.x / 2, c.y + s.y / 2});
                    if (fill)
                        v.d->AddRectFilled(a, b, IM_COL32(k.r, k.g, k.b, k.a), 4);
                    else
                        v.d->AddRect(a, b, IM_COL32(k.r, k.g, k.b, k.a), 4);
                },
                [](void *p, const char *path, orbmod::Vec c, orbmod::Vec s, float angle, orbmod::Color k) {
                    auto &v = *static_cast<View *>(p);
                    auto *tex = v.assets->get(v.sim->drawingAssetRoot + "/" + path);
                    if (!tex)
                        return;
                    float a = degToRad(angle);
                    Vec2 dx{std::cos(a) * s.x / 2, std::sin(a) * s.x / 2},
                        dy{-std::sin(a) * s.y / 2, std::cos(a) * s.y / 2}, o{c.x, c.y};
                    v.d->AddImageQuad((ImTextureID)(intptr_t)tex, v.at(o - dx - dy), v.at(o + dx - dy),
                                      v.at(o + dx + dy), v.at(o - dx + dy), {0, 0}, {1, 0}, {1, 1}, {0, 1},
                                      IM_COL32(k.r, k.g, k.b, k.a));
                }};
    }
};
class Studio {
  public:
    Options options;
    ContentLoader loader;
    PluginManager plugins;
    SceneDocument doc, before;
    std::unique_ptr<Simulation> preview, game;
    Renderer renderer;
    std::unique_ptr<AssetCache> assets;
    std::string root, modRoot, userRoot, message, tool = "select";
    std::vector<SceneDocument> undoStack, redoStack;
    std::vector<std::string> disabled;
    int page = 0, kind = -1, selected = -1, pairs = 3, groups = 3, returnPage = 0;
    bool dragRecorded = false, modified = false, dirty = true, dragging = false, transaction = false,
         paused = false, grid = true;
    Vec2 offset{};
    float elapsed = 0, speed = 1;
    char file[1024]{};
    std::vector<char> code = std::vector<char>(262144, 0);
    bool share = false, files = false, quitConfirm = false, quitRequested = false;
    explicit Studio(const Options &o) : options(o), loader(o.contentRoot) {
        root = fs::u8path(o.contentRoot).parent_path().u8string();
        userRoot = root + "/user";
        modRoot = o.modsRoot;
        if (modRoot.empty()) {
            char *b = SDL_GetBasePath();
            if (b) {
                modRoot = (fs::u8path(b) / "mods").u8string();
                SDL_free(b);
            } else
                modRoot = root + "/build/bin/mods";
        }
        std::snprintf(file, sizeof(file), "%s", (userRoot + "/arena.json").c_str());
    }
    void syncMods() {
        doc.mods.clear();
        for (auto &o : doc.objects)
            if (auto *t = plugins.find(o.type))
                if (std::find(doc.mods.begin(), doc.mods.end(), t->modId) == doc.mods.end())
                    doc.mods.push_back(t->modId);
    }
    bool init() {
        if (!loader.loadAll()) {
            for (auto &i : loader.issues())
                message += i.source + ": " + i.message + "\n";
            return false;
        }
        {
            std::ifstream settings(fs::u8path(userRoot + "/settings.json"));
            if (settings) {
                auto j = nlohmann::json::parse(settings, nullptr, false);
                if (j.is_object() && j.contains("disabledMods") && j["disabledMods"].is_array())
                    for (const auto &id : j["disabledMods"])
                        if (id.is_string())
                            disabled.push_back(id.get<std::string>());
            }
        }
        plugins.load(modRoot, disabled);
        doc = SceneDocument::fromPack(loader.pack());
        if (!options.mapId.empty()) {
            if (auto *m = loader.pack().findMap(options.mapId)) {
                doc.map = *m;
                doc.shrink = true;
                doc.timeLimit = 0;
            } else {
                message = "找不到地图: " + options.mapId;
                return false;
            }
        } else if (options.sceneFile.empty()) {
            generateScene(doc, options.seed ? options.seed : doc.map.seed, pairs, groups,
                          plugins.generationTypes());
        }
        syncMods();
        std::string error;
        if (!options.sceneFile.empty()) {
            if (!openScene(options.sceneFile, doc, error)) {
                message = error;
                return false;
            }
        } else if (fs::exists(fs::u8path(userRoot + "/last-scene.json")) && !options.headless &&
                   options.screenshotPath.empty())
            openScene(userRoot + "/last-scene.json", doc, error);
        if (options.seed)
            doc.map.seed = options.seed;
        if (!options.theme.empty())
            doc.theme = options.theme;
        if (!options.characterId.empty())
            if (auto *c = loader.pack().findCharacter(options.characterId))
                doc.fighters[0] = *c;
        if (!options.enemyId.empty())
            if (auto *c = loader.pack().findCharacter(options.enemyId))
                doc.fighters[1] = *c;
        if (options.view == "editor")
            page = 1;
        else if (options.view == "mods")
            page = 2;
        return true;
    }
    void change(bool c = true) {
        if (!c)
            return;
        if (!transaction) {
            undoStack.push_back(before);
            if (undoStack.size() > 50)
                undoStack.erase(undoStack.begin());
            redoStack.clear();
            transaction = true;
        }
        modified = dirty = true;
    }
    void checkpoint() {
        undoStack.push_back(doc);
        if (undoStack.size() > 50)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
        modified = dirty = true;
    }
    void restore(bool redo) {
        auto &from = redo ? redoStack : undoStack;
        auto &to = redo ? undoStack : redoStack;
        if (from.empty())
            return;
        to.push_back(doc);
        doc = from.back();
        from.pop_back();
        dirty = modified = true;
        kind = -1;
    }
    void start() {
        std::string e;
        if (!plugins.available(doc, e)) {
            message = e;
            return;
        }
        game = std::make_unique<Simulation>(doc, loader.pack(), plugins);
        paused = false;
        if (page != 3)
            returnPage = page;
        page = 3;
        message.clear();
    }
    void stop() {
        game.reset();
        page = returnPage;
        dirty = true;
    }
    void save() {
        std::string e;
        if (saveScene(file, doc, e)) {
            modified = false;
            message = "地图已保存";
        } else
            message = e;
    }
    void reload() {
        game.reset();
        preview.reset();
        plugins.load(modRoot, disabled);
        loader.loadAll();
        assets->clear();
        dirty = true;
        message = plugins.issues.empty() ? "模组与内容已重新加载" : plugins.issues.front();
    }
    int headless() {
        std::string e;
        if (!plugins.available(doc, e)) {
            std::fprintf(stderr, "%s\n", e.c_str());
            return 3;
        }
        float seconds =
            options.autotestSeconds > 0
                ? options.autotestSeconds
                : (options.runSeconds > 0 ? options.runSeconds : (doc.timeLimit > 0 ? doc.timeLimit : 120.f));
        int rounds = 0, hits = 0, activations = 0;
        float lull = 0;
        game = std::make_unique<Simulation>(doc, loader.pack(), plugins);
        for (int step = 0; step < int(seconds * 120); ++step) {
            game->step();
            if (game->match.phase() == MatchPhase::Finished) {
                ++rounds;
                if (options.autotestSeconds <= 0)
                    break;
                hits += game->match.hitCount();
                activations += game->modActivations;
                lull = std::max(lull, game->maxLull);
                auto next = doc;
                next.map.seed += rounds;
                game = std::make_unique<Simulation>(next, loader.pack(), plugins);
            }
        }
        std::printf("{\"rounds\":%d,\"phase\":%d,\"winner\":%d,\"seconds\":%.3f,\"hits\":%d,"
                    "\"modActivations\":%d,\"maxLull\":%.3f,\"hp\":[%.2f,%.2f]}\n",
                    rounds, int(game->match.phase()), game->match.result().winnerId, game->match.elapsed(),
                    hits + game->match.hitCount(), activations + game->modActivations,
                    std::max(lull, game->maxLull), game->match.fighter(0).hp, game->match.fighter(1).hp);
        return 0;
    }
    void section(const char *t) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", t);
        ImGui::Separator();
        ImGui::Spacing();
    }
    void characterPicker(int i) {
        ImGui::PushID(i);
        auto &f = doc.fighters[i];
        ImGui::SetNextItemWidth(180);
        if (ImGui::BeginCombo(i == 0 ? "左方角色" : "右方角色", displayName(f.name))) {
            for (const auto &c : loader.pack().characters) {
                if (ImGui::Selectable(displayName(c.name), f.id == c.id)) {
                    f = c;
                    change();
                }
                if (ImGui::IsItemHovered() && !c.description.zh.empty())
                    ImGui::SetTooltip("%s", c.description.zh.c_str());
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("应用预设"))
            if (auto *c = loader.pack().findCharacter(f.id)) {
                f = *c;
                change();
            }
        if (!f.description.zh.empty())
            ImGui::TextWrapped("%s", f.description.zh.c_str());
        ImGui::TextDisabled("生命 %.0f  攻击 %.1f  护甲 %.1f", f.stats.maxHp, f.stats.attack, f.stats.armor);
        if (!f.competitive)
            ImGui::TextDisabled("训练角色：不参与竞技平衡统计");
        for (const auto &id : f.skills)
            if (auto *s = loader.pack().findSkill(id)) {
                ImGui::Text("%s", displayName(s->name));
                ImGui::SameLine();
                if (s->trigger.kind == TriggerKind::Passive)
                    ImGui::TextDisabled("被动");
                else
                    ImGui::TextDisabled("%.1f 秒冷却", s->trigger.cooldown);
                if (ImGui::IsItemHovered() && !s->description.zh.empty())
                    ImGui::SetTooltip("%s", s->description.zh.c_str());
            }
        ImGui::PopID();
    }
    bool primary(const char *t, ImVec2 size = {0, 0}) {
        ImGui::PushStyleColor(ImGuiCol_Button, color(.38f, .44f, .73f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color(.47f, .54f, .86f));
        bool b = ImGui::Button(t, size);
        ImGui::PopStyleColor(2);
        return b;
    }
    bool danger(const char *t, ImVec2 size = {0, 0}) {
        ImGui::PushStyleColor(ImGuiCol_Button, color(.62f, .18f, .18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color(.78f, .24f, .24f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color(.52f, .13f, .13f));
        bool b = ImGui::Button(t, size);
        ImGui::PopStyleColor(3);
        return b;
    }
    void style() {
        ImGui::StyleColorsDark();
        auto &s = ImGui::GetStyle();
        s.WindowPadding = {18, 16};
        s.FramePadding = {10, 7};
        s.ItemSpacing = {10, 9};
        s.WindowRounding = 12;
        s.ChildRounding = 12;
        s.FrameRounding = 7;
        s.GrabRounding = 5;
        s.PopupRounding = 10;
        s.ScrollbarSize = 9;
        s.WindowBorderSize = 0;
        s.ChildBorderSize = 1;
        auto *c = s.Colors;
        c[ImGuiCol_WindowBg] = color(.045f, .055f, .085f);
        c[ImGuiCol_ChildBg] = color(.06f, .075f, .11f);
        c[ImGuiCol_Border] = color(.16f, .18f, .25f, .7f);
        c[ImGuiCol_FrameBg] = color(.1f, .12f, .18f);
        c[ImGuiCol_FrameBgHovered] = color(.16f, .18f, .27f);
        c[ImGuiCol_FrameBgActive] = color(.2f, .23f, .34f);
        c[ImGuiCol_Button] = color(.13f, .15f, .23f);
        c[ImGuiCol_ButtonHovered] = color(.23f, .27f, .39f);
        c[ImGuiCol_ButtonActive] = color(.28f, .32f, .47f);
        c[ImGuiCol_Header] = color(.19f, .23f, .36f);
        c[ImGuiCol_HeaderHovered] = color(.24f, .29f, .43f);
        c[ImGuiCol_CheckMark] = color(.65f, .73f, 1);
        c[ImGuiCol_SliderGrab] = color(.56f, .66f, .94f);
        c[ImGuiCol_Text] = color(.89f, .91f, .96f);
        c[ImGuiCol_TextDisabled] = color(.47f, .52f, .63f);
    }
    void nav() {
        ImGui::BeginChild("navigation", {184, 0}, true);
        ImGui::TextDisabled("WORKSPACE");
        ImGui::Spacing();
        if (ImGui::Selectable("概览", page == 0, 0, {0, 38})) {
            game.reset();
            page = 0;
        }
        if (ImGui::Selectable("地图编辑器", page == 1, 0, {0, 38})) {
            game.reset();
            page = 1;
        }
        if (ImGui::Selectable("模组工坊", page == 2, 0, {0, 38})) {
            game.reset();
            page = 2;
        }
        section("场景风格");
        for (int i = 0; i < 5; ++i) {
            auto p = themePalette(themes[i]);
            ImGui::PushID(i);
            ImGui::ColorButton("##color", {p.accent.r / 255.f, p.accent.g / 255.f, p.accent.b / 255.f, 1},
                               ImGuiColorEditFlags_NoTooltip, {12, 12});
            ImGui::SameLine();
            if (ImGui::Selectable(labels[i], doc.theme == themes[i])) {
                doc.theme = themes[i];
                change();
            }
            ImGui::PopID();
        }
        if (page == 1) {
            section("放置对象");
            if (ImGui::Selectable("选择 / 拖动", tool == "select"))
                tool = "select";
            if (ImGui::Selectable("矩形障碍", tool == "rect"))
                tool = "rect";
            if (ImGui::Selectable("圆形障碍", tool == "circle"))
                tool = "circle";
            for (auto &t : plugins.types)
                if (ImGui::Selectable(t.api.name, tool == t.api.id))
                    tool = t.api.id;
            ImGui::Checkbox("网格吸附", &grid);
            ImGui::TextDisabled("点击画布放置\n拖动调整位置\nDelete 删除\nCtrl+Z 撤销");
        }
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 20, ImGui::GetWindowHeight() - 125));
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, color(.35f, .12f, .12f, .75f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, color(.45f, .15f, .15f, .75f));
        ImGui::PushStyleColor(ImGuiCol_Text, color(.95f, .42f, .42f));
        if (ImGui::Selectable("退出游戏", false, 0, {0, 32}))
            quitConfirm = true;
        ImGui::PopStyleColor(3);
        ImGui::TextDisabled("ORBDUEL / STUDIO");
        ImGui::TextDisabled("C++17 / 离线创作");
        ImGui::EndChild();
    }
    void properties() {
        ImGui::BeginChild("properties", {290, 0}, true);
        ImGui::PushItemWidth(120);
        ImGui::Text("属性");
        ImGui::TextDisabled("修改后可直接开始测试");
        if (kind == 0 && selected >= 0 && selected < int(doc.objects.size())) {
            auto &o = doc.objects[selected];
            auto *t = plugins.find(o.type);
            section(t ? t->api.name : o.type.c_str());
            ImGui::TextDisabled("对象 #%d", o.id);
            float pos[2] = {o.pos.x, o.pos.y};
            if (ImGui::DragFloat2("位置", pos, 1)) {
                o.pos = {pos[0], pos[1]};
                change();
            }
            if (ImGui::DragFloat("半径", &o.radius, 1, 4, 600))
                change();
            if (ImGui::SliderFloat("朝向", &o.angle, -180, 180, "%.0f°"))
                change();
            const char *owners[] = {"固定在地图", "跟随左方角色", "跟随右方角色"};
            int own = o.owner + 1;
            if (ImGui::Combo("附着", &own, owners, 3)) {
                o.owner = own - 1;
                change();
            }
            if (t) {
                ImGui::TextWrapped("%s", t->api.description);
                section("模组参数");
                for (int i = 0; i < t->api.paramCount; ++i) {
                    auto &p = t->api.params[i];
                    ImGui::PushID(p.key);
                    auto &j = o.params;
                    if (!j.contains(p.key))
                        j[p.key] = plugins.defaults(*t)[p.key];
                    bool edit = false;
                    if (p.kind == orbmod::ParamKind::Number) {
                        float x = j[p.key].get<float>();
                        if (ImGui::DragFloat(p.label, &x, .5f, p.minimum, p.maximum)) {
                            j[p.key] = x;
                            edit = true;
                        }
                    } else if (p.kind == orbmod::ParamKind::Integer) {
                        int x = int(j[p.key].get<float>());
                        if (ImGui::DragInt(p.label, &x, 1, int(p.minimum), int(p.maximum))) {
                            j[p.key] = x;
                            edit = true;
                        }
                    } else if (p.kind == orbmod::ParamKind::Toggle) {
                        bool x = j[p.key].is_boolean() ? j[p.key].get<bool>() : j[p.key].get<float>() != 0;
                        if (ImGui::Checkbox(p.label, &x)) {
                            j[p.key] = x;
                            edit = true;
                        }
                    } else if (p.kind == orbmod::ParamKind::Choice) {
                        int x = int(j[p.key].get<float>());
                        std::vector<std::string> names;
                        std::string all = p.choices ? p.choices : "";
                        std::size_t start = 0;
                        for (std::size_t k = 0; k <= all.size(); ++k)
                            if (k == all.size() || all[k] == '|') {
                                names.push_back(all.substr(start, k - start));
                                start = k + 1;
                            }
                        if (ImGui::BeginCombo(p.label,
                                              x >= 0 && x < int(names.size()) ? names[x].c_str() : "选择")) {
                            for (int k = 0; k < int(names.size()); ++k)
                                if (ImGui::Selectable(names[k].c_str(), k == x)) {
                                    j[p.key] = k;
                                    edit = true;
                                }
                            ImGui::EndCombo();
                        }
                    } else if (p.kind == orbmod::ParamKind::ObjectRef) {
                        int target = int(j[p.key].get<float>());
                        std::string label = "未关联";
                        for (auto &other : doc.objects)
                            if (other.id == target)
                                label = "#" + std::to_string(target) + " " + other.type;
                        if (ImGui::BeginCombo(p.label, label.c_str())) {
                            if (ImGui::Selectable("未关联", target < 0)) {
                                j[p.key] = -1;
                                edit = true;
                            }
                            for (auto &other : doc.objects)
                                if (other.id != o.id) {
                                    auto name = "#" + std::to_string(other.id) + " " + other.type;
                                    if (ImGui::Selectable(name.c_str(), target == other.id)) {
                                        j[p.key] = other.id;
                                        edit = true;
                                    }
                                }
                            ImGui::EndCombo();
                        }
                    } else if (p.kind == orbmod::ParamKind::Color) {
                        auto x = j[p.key];
                        float a[4] = {x.at(0), x.at(1), x.at(2), x.at(3)};
                        if (ImGui::ColorEdit4(p.label, a)) {
                            j[p.key] = {a[0], a[1], a[2], a[3]};
                            edit = true;
                        }
                    } else {
                        char b[512];
                        std::snprintf(b, sizeof(b), "%s", j[p.key].get<std::string>().c_str());
                        if (ImGui::InputText(p.label, b, sizeof(b))) {
                            j[p.key] = b;
                            edit = true;
                        }
                    }
                    if (p.help && *p.help && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", p.help);
                    change(edit);
                    ImGui::PopID();
                }
                for (int i = 0; i < t->api.paramCount; ++i)
                    if (t->api.params[i].kind == orbmod::ParamKind::ObjectRef) {
                        int target = int(o.params[t->api.params[i].key].get<float>());
                        bool found = false;
                        for (auto &other : doc.objects)
                            if (other.id == target && target != o.id)
                                found = true;
                        if (!found)
                            ImGui::TextColored({1, .66f, .4f, 1}, "请选择有效的关联对象");
                    }
            }
            if (ImGui::Button("复制对象")) {
                checkpoint();
                auto copy = o;
                copy.id = doc.nextId();
                copy.pos += Vec2{30, 30};
                doc.objects.push_back(copy);
                selected = int(doc.objects.size()) - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("删除")) {
                checkpoint();
                doc.objects.erase(doc.objects.begin() + selected);
                kind = -1;
                syncMods();
            }
        } else if (kind == 1 && selected >= 0 && selected < int(doc.map.obstacles.size())) {
            auto &o = doc.map.obstacles[selected];
            section("障碍");
            float a[2] = {o.pos.x, o.pos.y};
            if (ImGui::DragFloat2("位置", a, 1)) {
                o.pos = {a[0], a[1]};
                change();
            }
            if (o.shape == "circle") {
                if (ImGui::DragFloat("半径", &o.radius, 1, 5, 300))
                    change();
            } else {
                float b[2] = {o.size.x, o.size.y};
                if (ImGui::DragFloat2("大小", b, 1, 5, 600)) {
                    o.size = {b[0], b[1]};
                    change();
                }
            }
            if (ImGui::SliderFloat("弹性", &o.restitution, 0, 1))
                change();
            if (ImGui::Button("删除障碍")) {
                checkpoint();
                doc.map.obstacles.erase(doc.map.obstacles.begin() + selected);
                kind = -1;
            }
        } else if (kind == 2 && selected >= 0 && selected < 2) {
            section(selected == 0 ? "左方出生点" : "右方出生点");
            auto &s = doc.map.spawns[selected];
            float a[2] = {s.pos.x, s.pos.y};
            if (ImGui::DragFloat2("位置", a, 1)) {
                s.pos = {a[0], a[1]};
                change();
            }
            float angle = radToDeg(std::atan2(s.dir.y, s.dir.x));
            if (ImGui::SliderFloat("方向", &angle, -180, 180)) {
                s.dir = {std::cos(degToRad(angle)), std::sin(degToRad(angle))};
                change();
            }
        } else {
            section("地图设置");
            char name[512];
            std::snprintf(name, sizeof(name), "%s", doc.name.c_str());
            if (ImGui::InputText("名称", name, sizeof(name))) {
                doc.name = name;
                change();
            }
            int size[2] = {int(doc.map.size.x), int(doc.map.size.y)};
            if (ImGui::InputInt2("尺寸", size)) {
                doc.map.size = {float(std::max(640, size[0])), float(std::max(480, size[1]))};
                change();
            }
            ImGui::TextDisabled("点击对象查看专属参数");
        }
        section("场景表现");
        if (ImGui::SliderFloat("装饰密度", &doc.decoration, 0, 1))
            change();
        if (ImGui::Checkbox("场景动画", &doc.animate))
            change();
        if (ImGui::Checkbox("自定义配色", &doc.customColors))
            change();
        if (doc.customColors) {
            if (ImGui::ColorEdit3("背景色", doc.background.data()))
                change();
            if (ImGui::ColorEdit3("强调色", doc.accent.data()))
                change();
        }
        section("对战规则");
        if (ImGui::DragFloat("时间上限", &doc.timeLimit, 1, 1, 600, "%.0f 秒"))
            change();
        if (ImGui::SliderFloat("普通碰撞倍率", &doc.map.physics.damageScale, 0, 5))
            change();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("只影响普通碰撞；技能和模组主动伤害另行结算。");
        if (ImGui::Checkbox("缩圈", &doc.shrink))
            change();
        if (doc.shrink) {
            if (ImGui::SliderFloat("最大缩圈比例", &doc.map.shrink.minInset, 0, 1))
                change();
            if (ImGui::DragFloat("首次缩圈", &doc.map.shrink.firstAt, 1, 1, 300, "%.0f 秒"))
                change();
            if (ImGui::DragFloat("挤压伤害/秒", &doc.map.shrink.damagePerSec, .2f, 0, 50))
                change();
        }
        if (ImGui::Checkbox("防僵局吸引", &doc.stall))
            change();
        if (ImGui::CollapsingHeader("角色与技能"))
            for (int i = 0; i < 2; ++i) {
                ImGui::PushID(i);
                auto &f = doc.fighters[i];
                ImGui::Text("%s", i == 0 ? "左方" : "右方");
                if (ImGui::BeginCombo("角色", displayName(f.name))) {
                    for (auto &c : loader.pack().characters)
                        if (ImGui::Selectable(displayName(c.name), f.id == c.id)) {
                            f = c;
                            change();
                        }
                    ImGui::EndCombo();
                }
                if (ImGui::SmallButton("应用当前角色预设"))
                    if (auto *c = loader.pack().findCharacter(f.id)) {
                        f = *c;
                        change();
                    }
                ImGui::TextWrapped("%s", f.description.zh.c_str());
                if (ImGui::DragFloat("生命", &f.stats.maxHp, 1, 1, 10000))
                    change();
                if (ImGui::DragFloat("攻击", &f.stats.attack, .2f, 0, 200))
                    change();
                if (ImGui::DragFloat("护甲", &f.stats.armor, .2f, 0, 100))
                    change();
                if (ImGui::DragFloat("初速", &f.stats.startSpeed, 5, 0, 1800))
                    change();
                if (ImGui::DragFloat("质量", &f.stats.mass, .05f, .1f, 20))
                    change();
                if (ImGui::TreeNode("身体与成长")) {
                    if (ImGui::Checkbox("可穿入身体", &f.hitbox.ghost))
                        change();
                    if (ImGui::DragFloat("挨撞系数", &f.hitbox.contact, .02f, 0, 10))
                        change();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("与地图伤害倍率相乘；1 为正常受伤，0 为显式免疫普通碰撞。");
                    if (ImGui::DragFloat("质量上限", &f.stats.massMax, .05f, 0, 20))
                        change();
                    if (ImGui::DragFloat("成长秒数", &f.stats.massRamp, 1, .1f, 600))
                        change();
                    ImGui::TreePop();
                }
                for (int slot = 0; slot < 2; ++slot) {
                    ImGui::PushID(slot);
                    std::string id = slot < int(f.skills.size()) ? f.skills[slot] : "";
                    const auto *chosen = loader.pack().findSkill(id);
                    if (ImGui::BeginCombo("技能", chosen ? displayName(chosen->name) : "无")) {
                        if (ImGui::Selectable("无", id.empty())) {
                            if (slot < int(f.skills.size()))
                                f.skills.erase(f.skills.begin() + slot);
                            change();
                        }
                        for (auto &s : loader.pack().skills)
                            if (ImGui::Selectable(displayName(s.name), s.id == id)) {
                                if (slot < int(f.skills.size()))
                                    f.skills[slot] = s.id;
                                else
                                    f.skills.push_back(s.id);
                                change();
                            }
                        ImGui::EndCombo();
                    }
                    if (chosen && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s\n冷却 %.1f 秒", chosen->description.zh.c_str(),
                                          chosen->trigger.cooldown);
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::Separator();
            }
        ImGui::PopItemWidth();
        ImGui::EndChild();
    }
    void canvas(bool editable, float height) {
        ImVec2 size{std::max(20.f, ImGui::GetContentRegionAvail().x), std::max(20.f, height)},
            pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("arena-canvas", size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        auto *d = ImGui::GetWindowDrawList();
        float scale = std::min((size.x - 20) / doc.map.size.x, (size.y - 20) / doc.map.size.y);
        Rect r{pos.x + (size.x - doc.map.size.x * scale) / 2, pos.y + (size.y - doc.map.size.y * scale) / 2,
               doc.map.size.x * scale, doc.map.size.y * scale};
        auto &scene = game ? game->scene : doc;
        auto pal = themePalette(scene.theme);
        if (scene.customColors) {
            pal.top = {static_cast<unsigned char>(scene.background[0] * 255),
                       static_cast<unsigned char>(scene.background[1] * 255),
                       static_cast<unsigned char>(scene.background[2] * 255), 255};
            pal.bottom = pal.top;
            pal.accent = {static_cast<unsigned char>(scene.accent[0] * 255),
                          static_cast<unsigned char>(scene.accent[1] * 255),
                          static_cast<unsigned char>(scene.accent[2] * 255), 255};
        }
        drawTheme(d, r, scene.theme, scene.animate ? elapsed : 0, scene.decoration, &pal);
        d->PushClipRect({r.x, r.y}, {r.x + r.w, r.y + r.h}, true);
        Simulation *sim = game.get();
        if (!sim) {
            if (dirty || !preview) {
                preview = std::make_unique<Simulation>(doc, loader.pack(), plugins, true);
                dirty = false;
            }
            sim = preview.get();
        }
        View view{d, scale, {r.x, r.y}, assets.get(), sim};
        if (editable && grid) {
            for (int x = 0; x < int(scene.map.size.x); x += 40)
                d->AddLine(view.at({float(x), 0}), view.at({float(x), scene.map.size.y}),
                           IM_COL32(180, 195, 235, 12));
            for (int y = 0; y < int(scene.map.size.y); y += 40)
                d->AddLine(view.at({0, float(y)}), view.at({scene.map.size.x, float(y)}),
                           IM_COL32(180, 195, 235, 12));
        }
        for (auto &o : sim->arena.obstacles()) {
            auto a = view.at(o.pos);
            if (o.shape == ObstacleShape::Circle) {
                d->AddCircleFilled(a, o.radius * scale, ink(pal.stone), 40);
                d->AddCircle(a, o.radius * scale, ink(pal.accent), 40, 1.4f);
            } else {
                auto min = view.at(o.pos - o.size * .5f), max = view.at(o.pos + o.size * .5f);
                d->AddRectFilled(min, max, ink(pal.stone), 4);
                d->AddRect(min, max, IM_COL32(pal.accent.r, pal.accent.g, pal.accent.b, 150), 4, 0, 1.4f);
            }
        }
        auto api = view.api();
        sim->draw(api);
        if (editable)
            for (auto &o : doc.objects) {
                if (!plugins.find(o.type))
                    d->AddCircle(view.at(o.pos), o.radius * scale, IM_COL32(255, 100, 100, 255));
                if (o.params.contains("target") && o.params["target"].is_number()) {
                    int target = int(o.params["target"].get<float>());
                    for (auto &other : doc.objects)
                        if (other.id == target && o.id < target)
                            d->AddLine(view.at(o.pos), view.at(other.pos), IM_COL32(150, 150, 235, 75), 1.2f);
                }
            }
        for (int i = 0; i < 2; ++i) {
            auto &f = sim->match.fighter(i);
            auto *b = sim->world.body(f.bodyId);
            Vec2 p = game && b ? b->pos : doc.map.spawns[i].pos;
            float radius = game && b ? b->radius : scene.fighters[i].hitbox.radius;
            auto center = view.at(p);
            if (game && !f.alive)
                continue;
            Rgba c = scene.fighters[i].skin.pattern.empty()
                         ? (i == 0 ? Rgba{255, 174, 104, 255} : Rgba{119, 198, 255, 255})
                         : scene.fighters[i].skin.tint;
            for (int k = 3; k > 0; --k)
                d->AddCircleFilled(center, (radius + 5 * k) * scale, IM_COL32(c.r, c.g, c.b, 6), 40);
            auto &ch = scene.fighters[i];
            auto *texture = assets->get(root + "/content/" + ch.sourceDir + "/" + ch.skin.texture);
            d->AddCircleFilled(center, radius * scale, IM_COL32(c.r, c.g, c.b, 60), 40);
            float facing = b && game ? angleOf(b->vel) : angleOf(scene.map.spawns[i].dir);
            if (game) {
                for (const auto &state : game->match.skillStates(i)) {
                    if (state.charging) {
                        facing = angleOf(state.aimDir);
                        const float tip = skinTipDistance(ch.skin, radius);
                        for (float dash = 0; dash < 460; dash += 22)
                            d->AddLine(view.at(p + state.aimDir * (tip + dash)),
                                       view.at(p + state.aimDir * (tip + dash + 12)),
                                       IM_COL32(255, 215, 120, 170), 2);
                    }
                    if (state.pullLeft > 0)
                        d->AddCircle(center, state.pullRadius * scale, IM_COL32(190, 140, 255, 150), 64, 2);
                }
                if (f.gravity.range > 0 && !game->match.skills(i).suppressesPassive()) {
                    float t = f.gravity.growthSeconds > 0
                                  ? clampf(game->match.elapsed() / f.gravity.growthSeconds, 0, 1)
                                  : 1;
                    float range = f.gravity.range + (f.gravity.rangeMax - f.gravity.range) * t;
                    d->AddCircle(center, range * scale, IM_COL32(160, 120, 235, 70), 64, 1);
                }
            }
            if (ch.skin.shape == "triangle") {
                Vec2 dir = fromAngle(facing), side{-dir.y, dir.x};
                auto tip = view.at(p + dir * (radius * kDartTip));
                auto tail = p + dir * (radius * kDartTail);
                d->AddTriangleFilled(tip, view.at(tail + side * (radius * kDartHalfWidth)),
                                     view.at(tail - side * (radius * kDartHalfWidth)), ink(ch.skin.tint));
                d->AddTriangle(tip, view.at(tail + side * (radius * kDartHalfWidth)),
                               view.at(tail - side * (radius * kDartHalfWidth)), IM_COL32(255, 225, 170, 255),
                               2);
            } else if (texture && ch.skin.pattern.empty()) {
                float extent = radius * scale * ch.skin.scale;
                d->AddImage((ImTextureID)(intptr_t)texture, {center.x - extent, center.y - extent},
                            {center.x + extent, center.y + extent}, {0, 0}, {1, 1}, ink(ch.skin.tint));
            }
            emblem(d, center, radius * scale, ch.skin.pattern, ink(c));
            if (game)
                for (const auto &state : game->match.skillStates(i))
                    if (state.active && state.activeLeft > 0) {
                        d->AddCircle(center, (radius + 12) * scale, IM_COL32(c.r, c.g, c.b, 100), 24, 1.2f);
                        break;
                    }
            d->AddCircle(center, radius * scale, ink(c), 40, 1.8f);
            if (game && f.shield > 0)
                d->AddCircle(center, (radius + 7) * scale, IM_COL32(130, 230, 255, 210), 40, 2.5f);
            if (editable) {
                auto dir = normalized(doc.map.spawns[i].dir);
                d->AddLine(center, view.at(p + dir * (radius + 35)), ink(c), 2);
                d->AddText({center.x - 10, center.y + radius * scale + 7}, ink(c), i == 0 ? "P1" : "P2");
            }
        }
        for (auto &f : sim->flashes)
            d->AddCircle(view.at(f.first), (20 + (1 - f.second / .5f) * 60) * scale,
                         IM_COL32(pal.accent.r, pal.accent.g, pal.accent.b, int(f.second * 250)), 40, 2);
        Rect bounds = game ? game->arena.bounds() : Rect{6, 6, scene.map.size.x - 12, scene.map.size.y - 12};
        d->AddRect(view.at({bounds.x, bounds.y}), view.at({bounds.x + bounds.w, bounds.y + bounds.h}),
                   ink(pal.accent), 6, 0, 2);
        Vec2 selectedPos{};
        float radius = 0;
        if (editable) {
            if (kind == 0 && selected >= 0 && selected < int(doc.objects.size())) {
                selectedPos = doc.objects[selected].pos;
                radius = doc.objects[selected].radius;
            }
            if (kind == 1 && selected >= 0 && selected < int(doc.map.obstacles.size())) {
                auto &o = doc.map.obstacles[selected];
                selectedPos = o.pos;
                radius = o.shape == "circle" ? o.radius : lengthOf(o.size) * .5f;
            }
            if (kind == 2 && selected >= 0 && selected < 2) {
                selectedPos = doc.map.spawns[selected].pos;
                radius = doc.fighters[selected].hitbox.radius;
            }
            if (radius)
                d->AddCircle(view.at(selectedPos), (radius + 10) * scale, IM_COL32(255, 239, 190, 220), 40,
                             2);
        }
        d->PopClipRect();
        if (game && game->match.phase() == MatchPhase::Finished) {
            int winner = game->match.result().winnerId;
            std::string text =
                winner < 0 ? "平局" : std::string(displayName(scene.fighters[winner].name)) + " 获胜";
            d->AddRectFilled({r.x, r.y}, {r.x + r.w, r.y + r.h}, IM_COL32(7, 10, 20, 180), 8);
            auto s = ImGui::CalcTextSize(text.c_str());
            d->AddText({r.x + (r.w - s.x) / 2, r.y + r.h * .43f}, IM_COL32(245, 235, 207, 255), text.c_str());
            const char *hint = "重新测试，或返回编辑器调整你的组合";
            s = ImGui::CalcTextSize(hint);
            d->AddText({r.x + (r.w - s.x) / 2, r.y + r.h * .54f}, IM_COL32(170, 185, 215, 255), hint);
        }
        if (!editable)
            return;
        Vec2 world = view.world(ImGui::GetIO().MousePos);
        bool in = world.x >= 0 && world.y >= 0 && world.x <= doc.map.size.x && world.y <= doc.map.size.y;
        if (ImGui::IsItemHovered() && in && ImGui::IsMouseClicked(0)) {
            if (tool != "select") {
                checkpoint();
                if (tool == "rect" || tool == "circle") {
                    ObstacleDef o;
                    o.shape = tool;
                    o.pos = world;
                    o.radius = 30;
                    o.size = {120, 28};
                    o.restitution = .96f;
                    doc.map.obstacles.push_back(o);
                    kind = 1;
                    selected = int(doc.map.obstacles.size()) - 1;
                } else if (auto *t = plugins.find(tool)) {
                    SceneObject o;
                    o.id = doc.nextId();
                    o.type = tool;
                    o.pos = world;
                    o.params = plugins.defaults(*t);
                    doc.objects.push_back(o);
                    kind = 0;
                    selected = int(doc.objects.size()) - 1;
                    syncMods();
                }
                tool = "select";
            } else {
                kind = -1;
                float nearest = 1e9f;
                for (int i = 0; i < int(doc.objects.size()); ++i) {
                    float dist = lengthOf(world - doc.objects[i].pos);
                    if (dist < doc.objects[i].radius + 12 && dist < nearest) {
                        kind = 0;
                        selected = i;
                        nearest = dist;
                        selectedPos = doc.objects[i].pos;
                    }
                }
                for (int i = 0; i < int(doc.map.obstacles.size()); ++i) {
                    auto &o = doc.map.obstacles[i];
                    Vec2 delta = world - o.pos;
                    bool hit = o.shape == "circle" ? lengthOf(delta) < o.radius + 8
                                                   : (std::abs(delta.x) < o.size.x * .5f + 8 &&
                                                      std::abs(delta.y) < o.size.y * .5f + 8);
                    if (hit && lengthOf(delta) < nearest) {
                        kind = 1;
                        selected = i;
                        nearest = lengthOf(delta);
                        selectedPos = o.pos;
                    }
                }
                for (int i = 0; i < 2; ++i)
                    if (lengthOf(world - doc.map.spawns[i].pos) < doc.fighters[i].hitbox.radius + 8) {
                        kind = 2;
                        selected = i;
                        selectedPos = doc.map.spawns[i].pos;
                    }
                if (kind >= 0) {
                    dragging = true;
                    dragRecorded = false;
                    offset = selectedPos - world;
                }
            }
        }
        if (dragging && ImGui::IsMouseDragging(0, 3)) {
            if (!dragRecorded) {
                checkpoint();
                dragRecorded = true;
            }
            Vec2 p = world + offset;
            if (grid) {
                p.x = std::round(p.x / 20) * 20;
                p.y = std::round(p.y / 20) * 20;
            }
            p.x = clampf(p.x, 15, doc.map.size.x - 15);
            p.y = clampf(p.y, 15, doc.map.size.y - 15);
            if (kind == 0 && selected < int(doc.objects.size()))
                doc.objects[selected].pos = p;
            else if (kind == 1 && selected < int(doc.map.obstacles.size()))
                doc.map.obstacles[selected].pos = p;
            else if (kind == 2 && selected < 2)
                doc.map.spawns[selected].pos = p;
            dirty = true;
        }
        if (!ImGui::IsMouseDown(0))
            dragging = false;
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
            tool = "select";
            kind = -1;
        }
    }
    void overview() {
        ImGui::TextDisabled("CREATE / SIMULATE / SHARE");
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("让规则，成为你的作品。");
        ImGui::SetWindowFontScale(1);
        ImGui::TextDisabled("用 C++ 模组创造新能力，在地图里组合它们，再离线分享你的竞技场。");
        ImGui::Spacing();
        if (primary("开始自动对战", {170, 40}))
            start();
        ImGui::SameLine();
        if (ImGui::Button("进入地图编辑器", {170, 40}))
            page = 1;
        ImGui::SameLine();
        if (ImGui::Button("导入分享码", {140, 40}))
            share = true;
        ImGui::Spacing();
        if (ImGui::BeginTable("roster-pickers", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            characterPicker(0);
            ImGui::TableNextColumn();
            characterPicker(1);
            ImGui::EndTable();
        }
        ImGui::Spacing();
        canvas(false, std::max(220.f, ImGui::GetContentRegionAvail().y - 115));
        ImGui::Spacing();
        ImGui::Text("%s / %s", doc.name.c_str(), themeName(doc.theme));
        ImGui::TextDisabled("%zu 个机关 · %zu 个障碍 · %zu 个已加载类型", doc.objects.size(),
                            doc.map.obstacles.size(), plugins.types.size());
        ImGui::TextWrapped("同一个种子生成同一个场地；拖动、调整参数，或者自己写一个新的模组。");
    }
    void editor() {
        ImGui::Text("%s%s / %s", doc.name.c_str(), modified ? " *" : "", themeName(doc.theme));
        if (ImGui::Button("保存"))
            save();
        ImGui::SameLine();
        if (ImGui::Button("打开 / 另存"))
            files = true;
        ImGui::SameLine();
        if (ImGui::Button("分享码"))
            share = true;
        ImGui::SameLine();
        if (ImGui::Button("撤销"))
            restore(false);
        ImGui::SameLine();
        if (ImGui::Button("重做"))
            restore(true);
        ImGui::SameLine();
        if (primary("测试地图"))
            start();
        ImGui::Spacing();
        ImGui::SetNextItemWidth(155);
        int seed = int(doc.map.seed);
        if (ImGui::InputInt("种子", &seed, 0, 0)) {
            doc.map.seed = std::uint32_t(seed);
            change();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(65);
        ImGui::SliderInt("障碍组", &pairs, 0, 5);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(65);
        ImGui::SliderInt("机关组", &groups, 0, 6);
        if (ImGui::Button("按种子生成")) {
            checkpoint();
            generateScene(doc, doc.map.seed, pairs, groups, plugins.generationTypes());
            syncMods();
            kind = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("换个种子")) {
            checkpoint();
            Rng r{std::uint64_t(SDL_GetPerformanceCounter())};
            generateScene(doc, r.nextU32(), pairs, groups, plugins.generationTypes());
            syncMods();
            kind = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("清空机关")) {
            checkpoint();
            doc.objects.clear();
            syncMods();
            kind = -1;
        }
        canvas(true, std::max(180.f, ImGui::GetContentRegionAvail().y - 35));
        ImGui::TextDisabled("工具：%s | %d × %d | 点击对象查看属性", tool == "select" ? "选择" : tool.c_str(),
                            int(doc.map.size.x), int(doc.map.size.y));
    }
    void modPage() {
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("模组工坊");
        ImGui::SetWindowFontScale(1);
        ImGui::TextWrapped("模组可独立增加新能力。导入文件夹后重新加载，参数会自动出现在编辑器中。");
        if (primary("重新加载模组和内容"))
            reload();
        ImGui::TextDisabled("%s", modRoot.c_str());
        section("已安装");
        for (auto &m : plugins.mods) {
            ImGui::PushID(m.id.c_str());
            bool enabled = std::find(disabled.begin(), disabled.end(), m.id) == disabled.end();
            if (ImGui::Checkbox(m.name.c_str(), &enabled)) {
                if (enabled)
                    disabled.erase(std::remove(disabled.begin(), disabled.end(), m.id), disabled.end());
                else
                    disabled.push_back(m.id);
                message = "点击重新加载以应用启用状态";
            }
            ImGui::TextDisabled("%s", m.id.c_str());
            ImGui::PopID();
        }
        section("可用能力");
        for (auto &t : plugins.types) {
            ImGui::BulletText("%s", t.api.name);
            ImGui::TextWrapped("%s", t.api.description);
            ImGui::TextDisabled("%s", t.api.id);
            ImGui::Spacing();
        }
        for (auto &e : plugins.issues)
            ImGui::TextColored({1, .65f, .45f, 1}, "%s", e.c_str());
        section("自己制作模组");
        ImGui::TextWrapped("从 mod-sdk/template 复制模板，修改 C++ "
                           "行为并构建；将生成的整个文件夹放进此模组目录。参数标签、默认值和说明会自动显示，"
                           "无需另外编写界面。");
        ImGui::TextWrapped("传送门、引力场和弹射板使用相同接口；模板还提供了第四种机制：旋风。");
    }
    void battle() {
        if (!game)
            return;
        ImGui::Text("自动对战 / %s", game->scene.name.c_str());
        ImGui::SameLine();
        if (game->scene.timeLimit > 0)
            ImGui::TextDisabled("%.1f / %.0f 秒", game->match.elapsed(), game->scene.timeLimit);
        else
            ImGui::TextDisabled("%.1f 秒 / 无时限", game->match.elapsed());
        if (ImGui::Button(paused ? "继续" : "暂停"))
            paused = !paused;
        ImGui::SameLine();
        if (ImGui::Button("重新测试"))
            start();
        ImGui::SameLine();
        const char *backTo = "返回概览";
        if (returnPage == 1)
            backTo = "返回编辑器";
        if (ImGui::Button(backTo)) {
            stop();
            return;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("速度", &speed, .25f, 4, "%.2fx");
        for (int i = 0; i < 2; ++i) {
            auto &f = game->match.fighter(i);
            ImGui::PushID(i);
            ImGui::Text("%s %.0f / %.0f HP 护盾 %.0f", displayName(game->scene.fighters[i].name), f.hp,
                        f.maxHp, f.shield);
            auto tint = game->scene.fighters[i].skin.tint;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                  color(tint.r / 255.f, tint.g / 255.f, tint.b / 255.f));
            ImGui::ProgressBar(f.hp / f.maxHp, {-1, 7}, "");
            ImGui::PopStyleColor();
            for (const auto &state : game->match.skillStates(i)) {
                const bool passive = state.def->trigger.kind == TriggerKind::Passive;
                const char *status =
                    passive
                        ? "被动"
                        : (state.charging
                               ? "蓄力"
                               : (state.pullLeft > 0
                                      ? "牵引"
                                      : (state.active ? "生效"
                                                      : (state.lowTriggered
                                                             ? "已使用"
                                                             : (state.cooldownLeft > 0 ? "冷却" : "待命")))));
                const float left =
                    state.charging
                        ? state.chargeLeft
                        : (state.pullLeft > 0 ? state.pullLeft
                                              : (state.active ? state.activeLeft : state.cooldownLeft));
                if (passive)
                    ImGui::TextDisabled("%s  %s", displayName(state.def->name), status);
                else
                    ImGui::TextDisabled("%s  %s %.1fs", displayName(state.def->name), status, left);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", state.def->description.zh.c_str());
            }
            ImGui::PopID();
        }
        canvas(false, std::max(170.f, ImGui::GetContentRegionAvail().y - 150));
        ImGui::BeginChild("battle-log", {0, 125}, true);
        ImGui::TextDisabled("对战记录 / 碰撞 %d · 机关触发 %d", game->match.hitCount(), game->modActivations);
        for (int i = 0; i < 2; ++i) {
            auto &f = game->match.fighter(i);
            ImGui::Text("%s 伤害 %.1f 护盾吸收 %.1f 治疗 %.1f", displayName(game->scene.fighters[i].name),
                        f.damageDealt, f.damageAbsorbed, f.healingDone);
        }
        for (auto i = game->log.rbegin(); i != game->log.rend(); ++i)
            ImGui::TextDisabled("%s", i->c_str());
        ImGui::EndChild();
    }
    void dialogs() {
        if (quitConfirm) {
            ImGui::OpenPopup("退出游戏");
            quitConfirm = false;
        }
        if (ImGui::BeginPopupModal("退出游戏", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("确定退出 OrbDuel？");
            ImGui::TextDisabled("当前草稿会写入 user/last-scene.json，下次启动可恢复。");
            ImGui::Spacing();
            if (danger("退出", {100, 32})) {
                quitRequested = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("取消", {100, 32}))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (share) {
            ImGui::OpenPopup("离线分享");
            share = false;
        }
        ImGui::SetNextWindowSize({720, 490}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("离线分享", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("分享码保存地图、规则和风格。接收者需要安装相同的模组文件夹，无需联网。");
            if (primary("生成并复制分享码")) {
                auto s = shareCode(doc);
                if (s.size() >= code.size())
                    code.resize(s.size() + 4096);
                std::memcpy(code.data(), s.c_str(), s.size() + 1);
                SDL_SetClipboardText(s.c_str());
                message = "分享码已复制；自定义模组文件夹请另行分享";
            }
            ImGui::SameLine();
            if (ImGui::Button("从剪贴板粘贴")) {
                char *t = SDL_GetClipboardText();
                if (t) {
                    auto n = std::strlen(t);
                    if (n >= code.size())
                        code.resize(n + 4096);
                    std::memcpy(code.data(), t, n + 1);
                    SDL_free(t);
                }
            }
            ImGui::InputTextMultiline("##code", code.data(), code.size(), {-1, 250});
            if (primary("导入到编辑器")) {
                SceneDocument candidate;
                std::string e;
                if (importCode(code.data(), candidate, e) && plugins.available(candidate, e)) {
                    checkpoint();
                    doc = std::move(candidate);
                    game.reset();
                    page = 1;
                    kind = -1;
                    dirty = true;
                    message = "分享地图已还原";
                    ImGui::CloseCurrentPopup();
                } else
                    message = e;
            }
            ImGui::SameLine();
            if (ImGui::Button("保存分享码文本")) {
                std::error_code ec;
                fs::create_directories(fs::u8path(userRoot), ec);
                std::ofstream f(fs::u8path(userRoot + "/share-code.txt"));
                f << code.data();
                message = f ? "已保存到 user/share-code.txt" : "无法保存分享码";
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭"))
                ImGui::CloseCurrentPopup();
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::EndPopup();
        }
        if (files) {
            ImGui::OpenPopup("地图文件");
            files = false;
        }
        ImGui::SetNextWindowSize({700, 400}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("地图文件", nullptr, 0)) {
            ImGui::InputText("文件路径", file, sizeof(file));
            if (primary("保存到此文件")) {
                save();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("打开此文件")) {
                SceneDocument candidate;
                std::string e;
                if (openScene(file, candidate, e) && plugins.available(candidate, e)) {
                    checkpoint();
                    doc = std::move(candidate);
                    dirty = true;
                    kind = -1;
                    message = "地图已打开";
                    ImGui::CloseCurrentPopup();
                } else
                    message = e;
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭"))
                ImGui::CloseCurrentPopup();
            section("已保存的地图");
            std::error_code ec;
            fs::create_directories(fs::u8path(userRoot), ec);
            for (auto &e : fs::directory_iterator(fs::u8path(userRoot), ec))
                if (e.path().extension() == ".json")
                    if (ImGui::Selectable(e.path().filename().u8string().c_str()))
                        std::snprintf(file, sizeof(file), "%s", e.path().u8string().c_str());
            section("内置示例地图");
            for (auto &e : fs::directory_iterator(fs::u8path(root + "/content/studio"), ec))
                if (e.path().extension() == ".json")
                    if (ImGui::Selectable(e.path().filename().u8string().c_str()))
                        std::snprintf(file, sizeof(file), "%s", e.path().u8string().c_str());
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::EndPopup();
        }
    }
    int run() {
        chineseNames = options.language == "zh";
        if (options.headless)
            return headless();
        if (!renderer.init(1480, 900, "OrbDuel | Mod Studio", options.hidden))
            return 2;
        assets = std::make_unique<AssetCache>(renderer.backend());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        if (fs::exists("C:/Windows/Fonts/msyh.ttc")) {
            ImFontConfig cfg;
            cfg.FontNo = 0;
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18, &cfg,
                                         io.Fonts->GetGlyphRangesChineseFull());
        } else
            io.Fonts->AddFontDefault();
        style();
        ImGui_ImplSDL2_InitForSDLRenderer(SDL_GetWindowFromID(1), renderer.backend());
        ImGui_ImplSDLRenderer2_Init(renderer.backend());
        if (options.play || !options.startInMenu)
            start();
        Clock clock(1.f / 120, 24);
        auto previous = SDL_GetPerformanceCounter();
        bool running = true;
        float wall = 0;
        while (running) {
            auto now = SDL_GetPerformanceCounter();
            float dt = std::min(.1f, float(double(now - previous) / SDL_GetPerformanceFrequency()));
            previous = now;
            wall += dt;
            elapsed += dt;
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT)
                    quitConfirm = true;
                if (e.type == SDL_KEYDOWN && !io.WantTextInput && !ImGui::IsPopupOpen("退出游戏")) {
                    bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
                    if (ctrl && e.key.keysym.sym == SDLK_s)
                        save();
                    if (ctrl && e.key.keysym.sym == SDLK_z)
                        restore(false);
                    if (ctrl && e.key.keysym.sym == SDLK_y)
                        restore(true);
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        if (game)
                            stop();
                        else if (page == 1) {
                            tool = "select";
                            kind = -1;
                        } else
                            page = 0;
                    }
                    if (e.key.keysym.sym == SDLK_DELETE && page == 1) {
                        if (kind == 0 && selected >= 0 && selected < int(doc.objects.size())) {
                            checkpoint();
                            doc.objects.erase(doc.objects.begin() + selected);
                            syncMods();
                            kind = -1;
                        } else if (kind == 1 && selected >= 0 && selected < int(doc.map.obstacles.size())) {
                            checkpoint();
                            doc.map.obstacles.erase(doc.map.obstacles.begin() + selected);
                            kind = -1;
                        }
                    }
                }
            }
            if (game && !paused) {
                clock.tick(dt * speed);
                for (int i = 0, n = clock.consumeSteps(); i < n; ++i)
                    game->step();
            } else
                clock.reset();
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            before = doc;
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("OrbDuel", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::PushStyleColor(ImGuiCol_Text, color(.76f, .8f, 1));
            ImGui::Text("O R B D U E L");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("/ MOD STUDIO");
            ImGui::SameLine(io.DisplaySize.x - 300);
            ImGui::TextDisabled("%zu 个能力已加载 · 离线就绪", plugins.types.size());
            ImGui::Separator();
            ImGui::BeginChild("workspace", {0, message.empty() ? -12.f : -40.f}, false);
            nav();
            ImGui::SameLine();
            float right = page == 1 ? 300.f : 0;
            ImGui::BeginChild("main-content", {-right, 0}, false);
            if (page == 0)
                overview();
            if (page == 1)
                editor();
            if (page == 2)
                modPage();
            if (page == 3)
                battle();
            ImGui::EndChild();
            if (page == 1) {
                ImGui::SameLine();
                properties();
            }
            ImGui::EndChild();
            if (!message.empty())
                ImGui::TextColored({.73f, .79f, .94f, 1}, "%s", message.c_str());
            dialogs();
            ImGui::End();
            if (!ImGui::IsAnyItemActive())
                transaction = false;
            ImGui::Render();
            renderer.beginFrame({11, 14, 22, 255});
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.backend());
            bool shot = !options.screenshotPath.empty() && wall >= options.shotAtSeconds;
            if (shot)
                renderer.saveScreenshot(options.screenshotPath);
            renderer.endFrame();
            if (shot)
                running = false;
            if (options.runSeconds > 0 && wall >= options.runSeconds)
                running = false;
            if (quitRequested)
                running = false;
        }
        if (options.screenshotPath.empty()) {
            std::string e;
            saveScene(userRoot + "/last-scene.json", doc, e);
            std::ofstream settings(fs::u8path(userRoot + "/settings.json"));
            settings << nlohmann::json{{"disabledMods", disabled}}.dump(2);
        }
        game.reset();
        preview.reset();
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        assets.reset();
        return 0;
    }
};
} // namespace
int runStudio(const Options &o) {
    try {
        Studio s(o);
        if (!s.init()) {
            std::fprintf(stderr, "%s\n", s.message.c_str());
            return 3;
        }
        return s.run();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "OrbDuel: %s\n", e.what());
        return 3;
    }
}
} // namespace orbduel
