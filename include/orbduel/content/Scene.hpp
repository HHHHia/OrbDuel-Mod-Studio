#pragma once
#include "orbduel/content/DataModel.hpp"
#include <array>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace orbduel {
struct SceneObject {
    int id = 1;
    std::string type;
    Vec2 pos{}, size{80, 80};
    float radius = 42, angle = 0;
    int owner = -1;
    nlohmann::json params = nlohmann::json::object();
};
struct SceneDocument {
    std::string name = "Untitled arena";
    MapDef map;
    std::array<CharacterDef, 2> fighters;
    std::vector<SceneObject> objects;
    std::vector<std::string> mods;
    std::string theme = "cosmos";
    float decoration = 0.7f;
    bool customColors = false;
    std::array<float, 3> background{0.05f, 0.07f, 0.15f}, accent{0.63f, 0.66f, 1.0f};
    bool animate = true, shrink = false, stall = true;
    float timeLimit = 60;
    int nextObjectId = 1;
    int nextId();
    static SceneDocument fromPack(const ContentPack &);
};
// 相同文档供编辑、保存、分享与对局快照使用。失败时不修改 out。
nlohmann::json sceneJson(const SceneDocument &);
bool readScene(const nlohmann::json &, SceneDocument &out, std::string &error);
bool openScene(const std::string &path, SceneDocument &out, std::string &error);
bool saveScene(const std::string &path, const SceneDocument &, std::string &error);
std::string shareCode(const SceneDocument &);
bool importCode(const std::string &, SceneDocument &out, std::string &error);
struct GenerationType {
    std::string id;
    int placement;
    nlohmann::json defaults;
};
void generateScene(SceneDocument &, std::uint32_t seed, int obstaclePairs, int mechanismGroups,
                   const std::vector<GenerationType> &);
} // namespace orbduel
