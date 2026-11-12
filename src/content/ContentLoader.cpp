#include "orbduel/content/ContentLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "orbduel/core/Log.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace orbduel {

namespace {

// ---------- JSON 小工具（缺字段/类型不对时退回默认值，不让解析炸掉） ----------

float readFloat(const json& object, const char* key, float fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return fallback;
    }
    return it->get<float>();
}

int readInt(const json& object, const char* key, int fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return fallback;
    }
    return it->get<int>();
}


std::string readString(const json& object, const char* key, const std::string& fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return fallback;
    }
    return it->get<std::string>();
}

Vec2 readVec2(const json& object, const char* key, Vec2 fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 2) {
        return fallback;
    }
    return Vec2{(*it)[0].get<float>(), (*it)[1].get<float>()};
}

bool readBool(const json& object, const char* key, bool fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
}

Rgba readRgba(const json& object, const char* key, Rgba fallback) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_array() || it->size() != 4) {
        return fallback;
    }
    Rgba color;
    color.r = static_cast<unsigned char>((*it)[0].get<int>());
    color.g = static_cast<unsigned char>((*it)[1].get<int>());
    color.b = static_cast<unsigned char>((*it)[2].get<int>());
    color.a = static_cast<unsigned char>((*it)[3].get<int>());
    return color;
}

LocalizedText readName(const json& object) {
    LocalizedText name;
    auto it = object.find("name");
    if (it != object.end() && it->is_object()) {
        name.en = readString(*it, "en", "");
        name.zh = readString(*it, "zh", "");
    }
    return name;
}

TriggerKind parseTriggerKind(const std::string& text) {
    if (text == "on_hit") { return TriggerKind::OnHit; }
    if (text == "on_hit_by") { return TriggerKind::OnHitBy; }
    if (text == "on_low_hp") { return TriggerKind::OnLowHp; }
    if (text == "on_wall_bounce") { return TriggerKind::OnWallBounce; }
    if (text == "manual") { return TriggerKind::Manual; }
    if (text == "passive") { return TriggerKind::Passive; }
    return TriggerKind::Cooldown;
}

ModifierOp parseModifierOp(const std::string& text) {
    if (text == "shield") { return ModifierOp::Shield; }
    if (text == "lifesteal") { return ModifierOp::Lifesteal; }
    if (text == "slow_field") { return ModifierOp::SlowField; }
    if (text == "teleport") { return ModifierOp::Teleport; }
    if (text == "resize") { return ModifierOp::Resize; }
    if (text == "bounce_boost") { return ModifierOp::BounceBoost; }
    if (text == "heal") { return ModifierOp::Heal; }
    if (text == "pierce") { return ModifierOp::Pierce; }
    return ModifierOp::AddSpeed;
}

Curve parseCurve(const std::string& text) {
    if (text == "ease_out") { return Curve::EaseOut; }
    if (text == "ease_in_out") { return Curve::EaseInOut; }
    return Curve::Linear;
}

VfxType parseVfxType(const std::string& text) {
    if (text == "trail") { return VfxType::Trail; }
    if (text == "ring") { return VfxType::Ring; }
    if (text == "flash") { return VfxType::Flash; }
    if (text == "screen_shake") { return VfxType::ScreenShake; }
    if (text == "hit_stop") { return VfxType::HitStop; }
    return VfxType::Burst;
}

VfxOn parseVfxOn(const std::string& text) {
    if (text == "hit") { return VfxOn::Hit; }
    if (text == "expire") { return VfxOn::Expire; }
    return VfxOn::Cast;
}

void addIssue(std::vector<LoadIssue>& issues, IssueSeverity severity,
              const std::string& source, const std::string& message) {
    issues.push_back(LoadIssue{severity, source, message});
    if (severity == IssueSeverity::Error) {
        log::error(source + ": " + message);
    } else {
        log::warn(source + ": " + message);
    }
}

// ---------- 各类内容 ----------

void loadSkills(const fs::path& dir, ContentPack& pack, std::vector<LoadIssue>& issues) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        addIssue(issues, IssueSeverity::Error, "skills", "目录不存在");
        return;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const fs::path& file : files) {
        std::ifstream stream(file);
        if (!stream) {
            continue;
        }

        json root;
        try {
            stream >> root;
        } catch (const std::exception& error) {
            addIssue(issues, IssueSeverity::Error, "skills/" + file.stem().string(),
                     std::string("JSON 解析失败: ") + error.what());
            continue;
        }

        SkillDef def;
        def.format = readInt(root, "format", 1);
        def.id = readString(root, "id", file.stem().string());
        def.name = readName(root);
        def.sourceFile = "skills/" + file.filename().string();

        if (root.contains("trigger") && root["trigger"].is_object()) {
            const json& trigger = root["trigger"];
            def.trigger.kind = parseTriggerKind(readString(trigger, "kind", "cooldown"));
            def.trigger.cooldown = readFloat(trigger, "cooldown", 5.0f);
            def.trigger.hpBelow = readFloat(trigger, "hpBelow", 0.3f);
        }

        if (root.contains("charge") && root["charge"].is_object()) {
            const json& charge = root["charge"];
            def.charge.enabled = true;
            def.charge.duration = readFloat(charge, "duration", 3.0f);
            def.charge.durationMin = readFloat(charge, "durationMin", 0.0f);
            def.charge.durationLearningCasts = readInt(charge, "durationLearningCasts", 10);
            def.charge.releaseSpeed = readFloat(charge, "releaseSpeed", 500.0f);
            def.charge.aimLag = readFloat(charge, "aimLag", 0.3f);
            def.charge.aimJitterDeg = readFloat(charge, "aimJitterDeg", 6.0f);
            def.charge.aimLagMin = readFloat(charge, "aimLagMin", 0.05f);
            def.charge.aimJitterMinDeg = readFloat(charge, "aimJitterMinDeg", 0.5f);
            def.charge.aimLearningCasts = readInt(charge, "aimLearningCasts", 5);
            def.charge.interruptImmuneCasts = readInt(charge, "interruptImmuneCasts", 15);
            def.charge.ramDamageScale = readFloat(charge, "ramDamageScale", 0.12f);
        }

        if (root.contains("gravity") && root["gravity"].is_object()) {
            const json& gravity = root["gravity"];
            def.gravity.enabled = true;
            def.gravity.range = readFloat(gravity, "range", 260.0f);
            def.gravity.rangeMax = readFloat(gravity, "rangeMax", def.gravity.range);
            def.gravity.accel = readFloat(gravity, "accel", 420.0f);
            def.gravity.coreFactor = readFloat(gravity, "coreFactor", 0.7f);
            def.gravity.damage = readFloat(gravity, "damage", 6.0f);
            def.gravity.damageMax = readFloat(gravity, "damageMax", 18.0f);
            def.gravity.growthSeconds = readFloat(gravity, "growthSeconds", 90.0f);
            def.gravity.dpsEdge = readFloat(gravity, "dpsEdge", 0.0f);
            def.gravity.dpsCenter = readFloat(gravity, "dpsCenter", 0.0f);
            def.gravity.tickInterval = readFloat(gravity, "tickInterval", 0.5f);
            def.gravity.teleportGuard = readFloat(gravity, "teleportGuard", 1.5f);
            def.gravity.teleportMinDist = readFloat(gravity, "teleportMinDist", 240.0f);
        }

        if (root.contains("omnipull") && root["omnipull"].is_object()) {
            const json& pull = root["omnipull"];
            def.omnipull.enabled = true;
            def.omnipull.rangeFrom = readFloat(pull, "rangeFrom", 200.0f);
            def.omnipull.rangeTo = readFloat(pull, "rangeTo", 340.0f);
            def.omnipull.accel = readFloat(pull, "accel", 1200.0f);
            def.omnipull.durationFrom = readFloat(pull, "durationFrom", 2.0f);
            def.omnipull.durationTo = readFloat(pull, "durationTo", 5.0f);
            def.omnipull.damageFrom = readFloat(pull, "damageFrom", 20.0f);
            def.omnipull.damageTo = readFloat(pull, "damageTo", 48.0f);
            def.omnipull.growthSeconds = readFloat(pull, "growthSeconds", 90.0f);
            def.omnipull.dpsEdge = readFloat(pull, "dpsEdge", 0.0f);
            def.omnipull.dpsCenter = readFloat(pull, "dpsCenter", 0.0f);
            def.omnipull.invincible = readBool(pull, "invincible", true);
            def.omnipull.suppressPassive = readBool(pull, "suppressPassive", true);
        }

        if (root.contains("modifiers") && root["modifiers"].is_array()) {
            for (const json& item : root["modifiers"]) {
                ModifierDef modifier;
                modifier.op = parseModifierOp(readString(item, "op", "add_speed"));
                modifier.value = readFloat(item, "value", 0.0f);
                modifier.duration = readFloat(item, "duration", 0.0f);
                modifier.radius = readFloat(item, "radius", 0.0f);
                modifier.factor = readFloat(item, "factor", 1.0f);
                modifier.minSpeed = readFloat(item, "minSpeed", 0.0f);
                modifier.push = readFloat(item, "push", 0.0f);
                modifier.curve = parseCurve(readString(item, "curve", "linear"));
                def.modifiers.push_back(modifier);
            }
        }

        if (root.contains("vfx") && root["vfx"].is_array()) {
            for (const json& item : root["vfx"]) {
                VfxDef vfx;
                vfx.on = parseVfxOn(readString(item, "on", "cast"));
                vfx.type = parseVfxType(readString(item, "type", "burst"));
                vfx.color = readRgba(item, "color", Rgba{255, 255, 255, 255});
                vfx.duration = readFloat(item, "duration", 0.5f);
                vfx.value = readFloat(item, "value", 0.0f);
                vfx.count = readInt(item, "count", 12);
                def.vfx.push_back(vfx);
            }
        }

        def.stackRefresh = readString(root, "stack", "refresh") == "refresh";
        def.priority = readInt(root, "priority", 10);

        if (def.modifiers.empty() && !def.charge.enabled && !def.gravity.enabled &&
            !def.omnipull.enabled) {
            addIssue(issues, IssueSeverity::Warning, def.sourceFile, "modifiers 为空，这个技能不会有效果");
        }

        if (def.gravity.enabled) {
            if (def.gravity.range <= 0.0f) {
                addIssue(issues, IssueSeverity::Error, def.sourceFile, "gravity.range 必须大于 0");
            }
            if (def.gravity.coreFactor <= 0.0f || def.gravity.coreFactor > 1.0f) {
                addIssue(issues, IssueSeverity::Error, def.sourceFile,
                         "gravity.coreFactor 必须落在 (0, 1]");
            }
            if (def.gravity.damageMax < def.gravity.damage) {
                addIssue(issues, IssueSeverity::Warning, def.sourceFile,
                         "gravity.damageMax 小于 damage，伤害不会成长");
            }
            if (def.gravity.growthSeconds <= 0.0f) {
                addIssue(issues, IssueSeverity::Error, def.sourceFile,
                         "gravity.growthSeconds 必须大于 0");
            }
            if (def.gravity.tickInterval <= 0.0f) {
                addIssue(issues, IssueSeverity::Error, def.sourceFile, "gravity.tickInterval 必须大于 0");
            }
        }

        if (def.omnipull.enabled) {
            if (def.omnipull.growthSeconds <= 0.0f) {
                addIssue(issues, IssueSeverity::Error, def.sourceFile,
                         "omnipull.growthSeconds 必须大于 0");
            }
            if (def.omnipull.rangeTo < def.omnipull.rangeFrom ||
                def.omnipull.damageTo < def.omnipull.damageFrom ||
                def.omnipull.durationTo < def.omnipull.durationFrom) {
                addIssue(issues, IssueSeverity::Warning, def.sourceFile,
                         "omnipull 的 *To 小于 *From，越放越弱");
            }
        }

        pack.skills.push_back(def);
    }
}

void loadCharacters(const fs::path& dir, ContentPack& pack, std::vector<LoadIssue>& issues) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        addIssue(issues, IssueSeverity::Error, "characters", "目录不存在");
        return;
    }

    std::vector<fs::path> folders;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec)) {
            folders.push_back(entry.path());
        }
    }
    std::sort(folders.begin(), folders.end());

    for (const fs::path& folder : folders) {
        const fs::path file = folder / "character.json";
        const std::string source = "characters/" + folder.filename().string();
        if (!fs::is_regular_file(file, ec)) {
            addIssue(issues, IssueSeverity::Warning, source, "缺少 character.json");
            continue;
        }

        std::ifstream stream(file);
        if (!stream) {
            continue;
        }

        json root;
        try {
            stream >> root;
        } catch (const std::exception& error) {
            addIssue(issues, IssueSeverity::Error, source,
                     std::string("JSON 解析失败: ") + error.what());
            continue;
        }

        CharacterDef def;
        def.format = readInt(root, "format", 1);
        def.id = readString(root, "id", folder.filename().string());
        def.name = readName(root);
        def.sourceDir = source;

        if (root.contains("skin") && root["skin"].is_object()) {
            const json& skin = root["skin"];
            def.skin.texture = readString(skin, "texture", "skin.png");
            def.skin.shape = readString(skin, "shape", "sprite");
            def.skin.scale = readFloat(skin, "scale", 1.0f);
            def.skin.anchor = readVec2(skin, "anchor", Vec2{0.5f, 0.5f});
            def.skin.tint = readRgba(skin, "tint", Rgba{255, 255, 255, 255});
        }

        if (root.contains("hitbox") && root["hitbox"].is_object()) {
            const json& hitbox = root["hitbox"];
            def.hitbox.radius = readFloat(hitbox, "radius", 18.0f);
            def.hitbox.offset = readVec2(hitbox, "offset", Vec2{0.0f, 0.0f});
            def.hitbox.ghost = readBool(hitbox, "ghost", false);
            def.hitbox.contact = readFloat(hitbox, "contact", 0.0f);
        }

        if (root.contains("stats") && root["stats"].is_object()) {
            const json& stats = root["stats"];
            def.stats.mass = readFloat(stats, "mass", 1.0f);
            def.stats.massMax = readFloat(stats, "massMax", 0.0f);
            def.stats.massRamp = readFloat(stats, "massRamp", 90.0f);
            def.stats.restitution = readFloat(stats, "restitution", 0.95f);
            def.stats.maxHp = readFloat(stats, "maxHp", 100.0f);
            def.stats.startSpeed = readFloat(stats, "startSpeed", 400.0f);
        }

        if (root.contains("skills") && root["skills"].is_array()) {
            for (const json& item : root["skills"]) {
                if (item.is_string()) {
                    def.skills.push_back(item.get<std::string>());
                }
            }
        }

        if (def.hitbox.radius < 6.0f) {
            addIssue(issues, IssueSeverity::Error, source, "hitbox.radius 太小");
            continue;
        }
        if (def.skin.shape != "sprite" && def.skin.shape != "triangle") {
            addIssue(issues, IssueSeverity::Error, source,
                     "skin.shape 只能是 sprite 或 triangle");
            continue;
        }
        if (!fs::is_regular_file(folder / def.skin.texture, ec)) {
            addIssue(issues, IssueSeverity::Warning, source, "贴图不存在: " + def.skin.texture);
        }

        pack.characters.push_back(def);
    }
}

void loadMaps(const fs::path& dir, ContentPack& pack, std::vector<LoadIssue>& issues) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        addIssue(issues, IssueSeverity::Error, "maps", "目录不存在");
        return;
    }

    std::vector<fs::path> folders;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec)) {
            folders.push_back(entry.path());
        }
    }
    std::sort(folders.begin(), folders.end());

    for (const fs::path& folder : folders) {
        const fs::path file = folder / "map.json";
        const std::string source = "maps/" + folder.filename().string();
        if (!fs::is_regular_file(file, ec)) {
            addIssue(issues, IssueSeverity::Warning, source, "缺少 map.json");
            continue;
        }

        std::ifstream stream(file);
        if (!stream) {
            continue;
        }

        json root;
        try {
            stream >> root;
        } catch (const std::exception& error) {
            addIssue(issues, IssueSeverity::Error, source,
                     std::string("JSON 解析失败: ") + error.what());
            continue;
        }

        MapDef def;
        def.format = readInt(root, "format", 1);
        def.id = readString(root, "id", folder.filename().string());
        def.name = readName(root);
        def.sourceDir = source;
        def.size = readVec2(root, "size", Vec2{1280.0f, 720.0f});

        if (root.contains("theme") && root["theme"].is_object()) {
            const json& theme = root["theme"];
            def.theme.bgTexture = readString(theme, "bgTexture", "");
            def.theme.bgColor = readRgba(theme, "bgColor", Rgba{16, 17, 24, 255});
            def.theme.wallColor = readRgba(theme, "wallColor", Rgba{90, 120, 255, 255});
            def.theme.wallThickness = readFloat(theme, "wallThickness", 6.0f);
        }

        if (root.contains("spawns") && root["spawns"].is_array()) {
            for (const json& item : root["spawns"]) {
                SpawnDef spawn;
                spawn.pos = readVec2(item, "pos", Vec2{0.0f, 0.0f});
                spawn.dir = readVec2(item, "dir", Vec2{1.0f, 0.0f});
                spawn.character = readString(item, "character", "");
                def.spawns.push_back(spawn);
            }
        }

        if (root.contains("obstacles") && root["obstacles"].is_array()) {
            for (const json& item : root["obstacles"]) {
                ObstacleDef obstacle;
                obstacle.shape = readString(item, "shape", "rect");
                obstacle.pos = readVec2(item, "pos", Vec2{0.0f, 0.0f});
                obstacle.size = readVec2(item, "size", Vec2{0.0f, 0.0f});
                obstacle.radius = readFloat(item, "radius", 0.0f);
                obstacle.restitution = readFloat(item, "restitution", 1.0f);
                def.obstacles.push_back(obstacle);
            }
        }

        if (root.contains("physics") && root["physics"].is_object()) {
            const json& physics = root["physics"];
            def.physics.wallRestitution = readFloat(physics, "wallRestitution", 0.98f);
            def.physics.bounceJitterDeg = readFloat(physics, "bounceJitterDeg", 1.5f);
            def.physics.maxSpeed = readFloat(physics, "maxSpeed", 1800.0f);
            def.physics.damageScale = readFloat(physics, "damageScale", 1.0f);
            def.physics.refSpeed = readFloat(physics, "refSpeed", 400.0f);
        }

        if (root.contains("shrink") && root["shrink"].is_object()) {
            const json& shrink = root["shrink"];
            def.shrink.firstAt = readFloat(shrink, "firstAt", 25.0f);
            def.shrink.interval = readFloat(shrink, "interval", 12.0f);
            def.shrink.step = readFloat(shrink, "step", 0.08f);
            def.shrink.minInset = readFloat(shrink, "minInset", 0.40f);
            def.shrink.warnTime = readFloat(shrink, "warnTime", 1.0f);
            def.shrink.animTime = readFloat(shrink, "animTime", 1.5f);
            def.shrink.damagePerSec = readFloat(shrink, "damagePerSec", 4.0f);
            def.shrink.lullTrigger = readFloat(shrink, "lullTrigger", 8.0f);
        }

        def.seed = static_cast<std::uint32_t>(readInt(root, "seed", 0));

        if (def.spawns.size() != 2) {
            addIssue(issues, IssueSeverity::Error, source, "spawns 必须恰好 2 个");
            continue;
        }

        pack.maps.push_back(def);
    }
}

} // namespace

ContentLoader::ContentLoader(std::string contentRoot) : root_(std::move(contentRoot)) {}

bool ContentLoader::loadAll() {
    pack_ = ContentPack{};
    issues_.clear();
    stamps_.clear();

    const fs::path root(root_);
    loadSkills(root / "skills", pack_, issues_);
    loadCharacters(root / "characters", pack_, issues_);
    loadMaps(root / "maps", pack_, issues_);

    return !pack_.characters.empty() && !pack_.maps.empty();
}

void ContentLoader::poll() {
    // MVP：先不做热重载，阶段三再接（见 docs/ROADMAP.md）
    pollTimer_ += 1.0f;
}

const ContentPack& ContentLoader::pack() const { return pack_; }

std::vector<std::string> ContentLoader::characterIds() const {
    std::vector<std::string> ids;
    for (const CharacterDef& def : pack_.characters) {
        ids.push_back(def.id);
    }
    return ids;
}

std::vector<std::string> ContentLoader::skillIds() const {
    std::vector<std::string> ids;
    for (const SkillDef& def : pack_.skills) {
        ids.push_back(def.id);
    }
    return ids;
}

std::vector<std::string> ContentLoader::mapIds() const {
    std::vector<std::string> ids;
    for (const MapDef& def : pack_.maps) {
        ids.push_back(def.id);
    }
    return ids;
}

const std::vector<LoadIssue>& ContentLoader::issues() const { return issues_; }

bool ContentLoader::hasFatal() const {
    for (const LoadIssue& issue : issues_) {
        if (issue.severity == IssueSeverity::Error) {
            return true;
        }
    }
    return false;
}

void ContentLoader::clearIssues() { issues_.clear(); }

const std::string& ContentLoader::root() const { return root_; }

const CharacterDef* ContentPack::findCharacter(const std::string& id) const {
    for (const CharacterDef& def : characters) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

const SkillDef* ContentPack::findSkill(const std::string& id) const {
    for (const SkillDef& def : skills) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

const MapDef* ContentPack::findMap(const std::string& id) const {
    for (const MapDef& def : maps) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace orbduel
