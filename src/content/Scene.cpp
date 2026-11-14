#include "orbduel/content/Scene.hpp"
#include "orbduel/core/Rng.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

namespace orbduel {
using nlohmann::json;
namespace {
json vec(Vec2 p) {
    return json::array({p.x, p.y});
}
Vec2 v(const json &j) {
    return {j.at(0).get<float>(), j.at(1).get<float>()};
}
json fighterJson(const CharacterDef &c) {
    return {{"id", c.id},
            {"competitive", c.competitive},
            {"name", {{"en", c.name.en}, {"zh", c.name.zh}}},
            {"description", {{"en", c.description.en}, {"zh", c.description.zh}}},
            {"pattern", c.skin.pattern},
            {"shape", c.skin.shape},
            {"ghost", c.hitbox.ghost},
            {"contact", c.hitbox.contact},
            {"offset", vec(c.hitbox.offset)},
            {"anchor", vec(c.skin.anchor)},
            {"massMax", c.stats.massMax},
            {"massRamp", c.stats.massRamp},
            {"sourceDir", c.sourceDir},
            {"texture", c.skin.texture},
            {"radius", c.hitbox.radius},
            {"mass", c.stats.mass},
            {"hp", c.stats.maxHp},
            {"speed", c.stats.startSpeed},
            {"restitution", c.stats.restitution},
            {"attack", c.stats.attack},
            {"armor", c.stats.armor},
            {"skills", c.skills},
            {"scale", c.skin.scale},
            {"tint", {c.skin.tint.r, c.skin.tint.g, c.skin.tint.b, c.skin.tint.a}}};
}
CharacterDef fighterRead(const json &j) {
    CharacterDef c;
    c.id = j.at("id").get<std::string>();
    c.competitive = j.value("competitive", true);
    c.name.en = j.at("name").value("en", c.id);
    c.name.zh = j.at("name").value("zh", "");
    if (j.contains("description")) {
        c.description.en = j["description"].value("en", "");
        c.description.zh = j["description"].value("zh", "");
    }
    c.skin.pattern = j.value("pattern", "");
    c.skin.shape = j.value("shape", "sprite");
    c.hitbox.ghost = j.value("ghost", false);
    c.hitbox.contact = j.value("contact", 1.f);
    if (j.contains("offset"))
        c.hitbox.offset = v(j.at("offset"));
    if (j.contains("anchor"))
        c.skin.anchor = v(j.at("anchor"));
    c.stats.massMax = j.value("massMax", 0.f);
    c.stats.massRamp = j.value("massRamp", 90.f);
    c.sourceDir = j.value("sourceDir", "");
    c.skin.texture = j.value("texture", "");
    c.skin.scale = j.value("scale", 1.f);
    if (j.contains("tint")) {
        auto a = j.at("tint");
        c.skin.tint = {a.at(0).get<unsigned char>(), a.at(1).get<unsigned char>(),
                       a.at(2).get<unsigned char>(), a.at(3).get<unsigned char>()};
    }
    c.hitbox.radius = j.at("radius").get<float>();
    c.stats.mass = j.at("mass").get<float>();
    c.stats.maxHp = j.at("hp").get<float>();
    c.stats.startSpeed = j.at("speed").get<float>();
    c.stats.restitution = j.value("restitution", 0.95f);
    c.stats.attack = j.value("attack", 12.f);
    c.stats.armor = j.value("armor", 0.f);
    c.skills = j.value("skills", std::vector<std::string>{});
    if (!std::isfinite(c.stats.maxHp) || c.stats.maxHp <= 0 || !std::isfinite(c.stats.mass) ||
        c.stats.mass <= 0 || !std::isfinite(c.hitbox.radius) || c.hitbox.radius <= 0)
        throw std::runtime_error("角色血量、质量和半径必须为正数");
    return c;
}
const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
std::string encode(const std::string &input) {
    std::string s = "OD1:";
    unsigned value = 0;
    int bits = 0;
    for (unsigned char c : input) {
        value = (value << 8) | c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            s += alphabet[(value >> bits) & 63];
        }
    }
    if (bits)
        s += alphabet[(value << (6 - bits)) & 63];
    return s;
}
} // namespace
int SceneDocument::nextId() {
    int n = nextObjectId;
    for (const auto &o : objects)
        n = std::max(n, o.id + 1);
    nextObjectId = n + 1;
    return n;
}
SceneDocument SceneDocument::fromPack(const ContentPack &p) {
    SceneDocument s;
    s.map = p.maps.at(0);
    s.name = s.map.name.en;
    s.fighters[0] = p.characters.at(0);
    s.fighters[1] = p.characters.at(p.characters.size() > 1 ? 1 : 0);
    return s;
}
json sceneJson(const SceneDocument &s) {
    const auto &m = s.map;
    json j = {{"name", s.name},
              {"nextObjectId", s.nextObjectId},
              {"seed", m.seed},
              {"size", vec(m.size)},
              {"style",
               {{"preset", s.theme},
                {"decoration", s.decoration},
                {"animate", s.animate},
                {"customColors", s.customColors},
                {"background", s.background},
                {"accent", s.accent}}},
              {"rules",
               {{"timeLimit", s.timeLimit},
                {"shrink", s.shrink},
                {"stall", s.stall},
                {"damageScale", m.physics.damageScale},
                {"refSpeed", m.physics.refSpeed},
                {"maxSpeed", m.physics.maxSpeed},
                {"wallRestitution", m.physics.wallRestitution},
                {"jitter", m.physics.bounceJitterDeg},
                {"firstAt", m.shrink.firstAt},
                {"interval", m.shrink.interval},
                {"step", m.shrink.step},
                {"minInset", m.shrink.minInset},
                {"warnTime", m.shrink.warnTime},
                {"animTime", m.shrink.animTime},
                {"damagePerSec", m.shrink.damagePerSec},
                {"lullTrigger", m.shrink.lullTrigger}}},
              {"fighters", json::array({fighterJson(s.fighters[0]), fighterJson(s.fighters[1])})},
              {"spawns", json::array()},
              {"obstacles", json::array()},
              {"objects", json::array()},
              {"mods", s.mods}};
    for (const auto &p : m.spawns)
        j["spawns"].push_back({{"pos", vec(p.pos)}, {"dir", vec(p.dir)}});
    for (const auto &o : m.obstacles)
        j["obstacles"].push_back({{"shape", o.shape},
                                  {"pos", vec(o.pos)},
                                  {"size", vec(o.size)},
                                  {"radius", o.radius},
                                  {"restitution", o.restitution}});
    for (const auto &o : s.objects)
        j["objects"].push_back({{"id", o.id},
                                {"type", o.type},
                                {"pos", vec(o.pos)},
                                {"size", vec(o.size)},
                                {"radius", o.radius},
                                {"angle", o.angle},
                                {"owner", o.owner},
                                {"params", o.params}});
    return j;
}
bool readScene(const json &j, SceneDocument &out, std::string &error) {
    try {
        SceneDocument s;
        s.nextObjectId = j.value("nextObjectId", 1);
        s.name = j.value("name", "Untitled arena");
        s.map.id = "studio";
        s.map.name.en = s.name;
        s.map.seed = j.value("seed", 1u);
        s.map.size = v(j.at("size"));
        if (!std::isfinite(s.map.size.x) || !std::isfinite(s.map.size.y) || s.map.size.x < 320 ||
            s.map.size.y < 240)
            throw std::runtime_error("地图尺寸过小或无法使用");
        auto style = j.value("style", json::object());
        s.theme = style.value("preset", "cosmos");
        s.decoration = style.value("decoration", 0.7f);
        s.animate = style.value("animate", true);
        s.customColors = style.value("customColors", false);
        if (style.contains("background"))
            s.background = style.at("background").get<std::array<float, 3>>();
        if (style.contains("accent"))
            s.accent = style.at("accent").get<std::array<float, 3>>();
        auto r = j.value("rules", json::object());
        s.timeLimit = r.value("timeLimit", 60.f);
        s.shrink = r.value("shrink", false);
        s.stall = r.value("stall", true);
        auto &p = s.map.physics;
        p.damageScale = r.value("damageScale", 1.f);
        p.refSpeed = r.value("refSpeed", 400.f);
        p.maxSpeed = r.value("maxSpeed", 1800.f);
        p.wallRestitution = r.value("wallRestitution", 0.98f);
        p.bounceJitterDeg = r.value("jitter", 1.5f);
        auto &sh = s.map.shrink;
        sh.firstAt = r.value("firstAt", 25.f);
        sh.interval = r.value("interval", 12.f);
        sh.step = r.value("step", .08f);
        sh.minInset = r.value("minInset", .4f);
        sh.warnTime = r.value("warnTime", 1.f);
        sh.animTime = r.value("animTime", 1.5f);
        sh.damagePerSec = r.value("damagePerSec", 4.f);
        sh.lullTrigger = r.value("lullTrigger", 8.f);
        s.fighters[0] = fighterRead(j.at("fighters").at(0));
        s.fighters[1] = fighterRead(j.at("fighters").at(1));
        for (const auto &a : j.at("spawns"))
            s.map.spawns.push_back({v(a.at("pos")), v(a.at("dir")), ""});
        if (s.map.spawns.size() != 2)
            throw std::runtime_error("需要两个出生点");
        for (const auto &a : j.at("obstacles")) {
            ObstacleDef o;
            o.shape = a.at("shape").get<std::string>();
            o.pos = v(a.at("pos"));
            o.size = v(a.at("size"));
            o.radius = a.at("radius").get<float>();
            o.restitution = a.value("restitution", .9f);
            s.map.obstacles.push_back(o);
        }
        std::set<int> ids;
        for (const auto &a : j.at("objects")) {
            SceneObject o;
            o.id = a.at("id").get<int>();
            o.type = a.at("type").get<std::string>();
            o.pos = v(a.at("pos"));
            o.size = v(a.at("size"));
            o.radius = a.at("radius").get<float>();
            o.angle = a.value("angle", 0.f);
            o.owner = a.value("owner", -1);
            o.params = a.value("params", json::object());
            if (!o.params.is_object() || !ids.insert(o.id).second)
                throw std::runtime_error("机关参数错误或对象编号重复");
            s.objects.push_back(o);
            s.nextObjectId = std::max(s.nextObjectId, o.id + 1);
        }
        s.mods = j.value("mods", std::vector<std::string>{});
        out = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool openScene(const std::string &path, SceneDocument &out, std::string &error) {
    try {
        std::ifstream f(std::filesystem::u8path(path));
        if (!f)
            throw std::runtime_error("无法打开地图文件");
        json j;
        f >> j;
        return readScene(j, out, error);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool saveScene(const std::string &path, const SceneDocument &s, std::string &error) {
    try {
        auto p = std::filesystem::u8path(path);
        if (p.has_parent_path())
            std::filesystem::create_directories(p.parent_path());
        std::ofstream f(p);
        f << sceneJson(s).dump(2);
        if (!f)
            throw std::runtime_error("无法保存地图文件");
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
std::string shareCode(const SceneDocument &s) {
    return encode(sceneJson(s).dump());
}
bool importCode(const std::string &text, SceneDocument &out, std::string &error) {
    try {
        std::string s;
        for (unsigned char c : text)
            if (!std::isspace(c))
                s += char(c);
        if (s.compare(0, 4, "OD1:") != 0)
            throw std::runtime_error("分享码应以 OD1: 开头");
        std::string decoded;
        unsigned value = 0;
        int bits = 0;
        for (std::size_t i = 4; i < s.size(); ++i) {
            const char *p = std::find(alphabet, alphabet + 64, s[i]);
            if (p == alphabet + 64)
                throw std::runtime_error("分享码含有无法读取的字符");
            value = (value << 6) | unsigned(p - alphabet);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                decoded += char((value >> bits) & 255);
            }
        }
        return readScene(json::parse(decoded), out, error);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
void generateScene(SceneDocument &s, std::uint32_t seed, int pairs, int groups,
                   const std::vector<GenerationType> &types) {
    Rng rng(seed);
    s.map.seed = seed;
    s.map.obstacles.clear();
    s.objects.clear();
    s.nextObjectId = 1;
    const float w = s.map.size.x, h = s.map.size.y;
    s.map.spawns = {{{w * .15f, h * .5f}, {1, 0}, ""}, {{w * .85f, h * .5f}, {-1, 0}, ""}};
    for (int i = 0; i < pairs; ++i) {
        ObstacleDef o;
        o.shape = i % 2 ? "circle" : "rect";
        o.pos = {rng.range(w * .24f, w * .43f), rng.range(h * .17f, h * .34f)};
        if (i % 2)
            o.pos.y = h - o.pos.y;
        o.size = {rng.range(60, 120), 24};
        o.radius = rng.range(20, 34);
        o.restitution = .96f;
        s.map.obstacles.push_back(o);
        o.pos.x = w - o.pos.x;
        s.map.obstacles.push_back(o);
    }
    if (types.empty())
        return;
    for (int i = 0; i < groups; ++i) {
        const auto &t = types[std::size_t(i) % types.size()];
        if (!t.placement)
            continue;
        SceneObject a;
        a.id = s.nextId();
        a.type = t.id;
        a.params = t.defaults;
        a.pos = {w * (.28f + .12f * (i % 3)), h * (i % 2 ? .72f : .28f)};
        s.objects.push_back(a);
        if (t.placement == 2) {
            SceneObject b = a;
            b.id = s.nextId();
            b.pos = {w - a.pos.x, h - a.pos.y};
            b.angle = 180;
            for (auto it = a.params.begin(); it != a.params.end(); ++it)
                if (it.key() == "target") {
                    s.objects.back().params[it.key()] = b.id;
                    b.params[it.key()] = a.id;
                }
            s.objects.push_back(b);
        }
    }
}
} // namespace orbduel
