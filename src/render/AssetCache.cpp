#include "orbduel/render/AssetCache.hpp"

#include <SDL_image.h>

namespace orbduel {

namespace {
constexpr std::size_t kMaxErrors = 8;
}

AssetCache::AssetCache(SDL_Renderer *renderer) : renderer_(renderer) {
    IMG_Init(IMG_INIT_PNG);
}

AssetCache::~AssetCache() {
    clear();
}

SDL_Texture *AssetCache::get(const std::string &path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second;
    }
    if (renderer_ == nullptr) {
        return nullptr;
    }

    SDL_Texture *texture = IMG_LoadTexture(renderer_, path.c_str());
    if (texture == nullptr) {
        if (errors_.size() < kMaxErrors) {
            errors_.push_back(path + ": " + IMG_GetError());
        }
        return nullptr;
    }

    textures_[path] = texture;
    return texture;
}

bool AssetCache::reload(const std::string &path) {
    SDL_Texture *fresh = renderer_ ? IMG_LoadTexture(renderer_, path.c_str()) : nullptr;
    if (!fresh) {
        if (errors_.size() < kMaxErrors)
            errors_.push_back(path + ": " + IMG_GetError());
        return false;
    }
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        if (it->second != nullptr) {
            SDL_DestroyTexture(it->second);
        }
        textures_.erase(it);
    }
    textures_[path] = fresh;
    return true;
}

void AssetCache::reloadAll() {
    std::unordered_map<std::string, SDL_Texture *> fresh;
    for (auto &entry : textures_) {
        if (entry.second != nullptr) {
            SDL_DestroyTexture(entry.second);
        }
    }
    textures_.clear();
    errors_.clear();
}

void AssetCache::clear() {
    for (auto &entry : textures_) {
        if (entry.second != nullptr) {
            SDL_DestroyTexture(entry.second);
        }
    }
    textures_.clear();
}

const std::vector<std::string> &AssetCache::errors() const {
    return errors_;
}

} // namespace orbduel
