#pragma once
#include "orbduel/content/Scene.hpp"
#include "orbduel_entities.hpp"
#include "orbduel_mod.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace orbduel {
struct RegisteredType {
    orbmod::Type api{};
    std::string modId, assetRoot;
};
struct LoadedMod {
    std::string id, name, path;
    bool enabled = true;
    void *library = nullptr;
};
class PluginManager {
  public:
    ~PluginManager();
    PluginManager() = default;
    PluginManager(const PluginManager &) = delete;
    PluginManager &operator=(const PluginManager &) = delete;
    // 必须在 load 前设置；函数表生命周期覆盖所有模组实例。
    void setEntityApi(const orbmod::EntityApi *api) {
        entityApi_ = api;
    }
    // 只能在所有对局和预览实例销毁后重新加载。失败信息放 issues，不修改游戏源码。
    // extraRoots 为可写用户模组目录；同 ID 的后置目录覆盖内置版本。
    void load(const std::string &root, const std::vector<std::string> &disabled = {},
              const std::vector<std::string> &extraRoots = {});
    const RegisteredType *find(const std::string &) const;
    nlohmann::json defaults(const RegisteredType &) const;
    std::vector<GenerationType> generationTypes() const;
    bool available(const SceneDocument &, std::string &error) const;
    std::vector<LoadedMod> mods;
    std::vector<RegisteredType> types;
    std::vector<std::string> issues;

  private:
    void clear();
    const orbmod::EntityApi *entityApi_ = nullptr;
    std::string registeringId_, registeringRoot_;
    std::vector<std::string> cacheFiles_, cacheDirectories_;
};
} // namespace orbduel
