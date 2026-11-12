#pragma once

// 纹理缓存（SDL2 版）。热重载时用 reload() 换掉同一个 key 的纹理；
// 失败时保留旧纹理并记一条 error，不返回空指针（AGENTS 常见坑）。

#include <SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace orbduel {

class AssetCache {
public:
    explicit AssetCache(SDL_Renderer* renderer);
    ~AssetCache();

    AssetCache(const AssetCache&) = delete;
    AssetCache& operator=(const AssetCache&) = delete;

    // path 是相对项目根的路径，例如 "content/characters/lancer/skin.png"。
    // 第一次访问时懒加载（IMG_LoadTexture）；失败返回 nullptr 并记 error。
    SDL_Texture* get(const std::string& path);

    // 重新加载；返回 false 表示失败（旧纹理仍在）。
    bool reload(const std::string& path);

    // 重载所有已缓存的纹理（内容热重载后调用）。
    void reloadAll();

    void clear();

    const std::vector<std::string>& errors() const;

private:
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
    std::vector<std::string> errors_;
};

} // namespace orbduel
