#include "orbduel/mods/PluginManager.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace orbduel {
namespace fs = std::filesystem;
PluginManager::~PluginManager() {
    clear();
}
void PluginManager::clear() {
    types.clear();
    for (auto &m : mods)
        if (m.library)
            FreeLibrary(static_cast<HMODULE>(m.library));
    mods.clear();
}
void PluginManager::load(const std::string &root, const std::vector<std::string> &disabled) {
    clear();
    issues.clear();
    std::error_code ec;
    auto path = fs::u8path(root);
    fs::create_directories(path, ec);
    std::vector<fs::path> dirs;
    for (const auto &e : fs::directory_iterator(path, ec))
        if (e.is_directory())
            dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto &dir : dirs) {
        auto file = dir / "mod.json";
        if (!fs::exists(file))
            continue;
        const auto first = types.size();
        HMODULE handle = nullptr;
        try {
            std::ifstream stream(file);
            nlohmann::json j;
            stream >> j;
            LoadedMod m;
            m.id = j.at("id").get<std::string>();
            m.name = j.value("name", m.id);
            m.path = dir.u8string();
            m.enabled = std::find(disabled.begin(), disabled.end(), m.id) == disabled.end();
            if (!m.enabled) {
                mods.push_back(m);
                continue;
            }
            auto dll = fs::absolute(dir / fs::u8path(j.at("library").get<std::string>()));
            handle = LoadLibraryW(dll.c_str());
            if (!handle)
                throw std::runtime_error("DLL 无法加载，请重新构建模组或确认 x64 运行库");
            auto entry = reinterpret_cast<orbmod::Entry>(GetProcAddress(handle, "orbduel_register"));
            if (!entry)
                throw std::runtime_error("找不到 orbduel_register 入口");
            registeringId_ = m.id;
            registeringRoot_ = dir.u8string();
            orbmod::Registrar reg{this, [](void *p, const orbmod::Type *t) {
                                      auto &self = *static_cast<PluginManager *>(p);
                                      if (!t || !t->id || !t->create || !t->destroy)
                                          throw std::runtime_error("模组类型缺少名称或创建/销毁回调");
                                      if (self.find(t->id))
                                          throw std::runtime_error(std::string("类型名重复: ") + t->id);
                                      self.types.push_back({*t, self.registeringId_, self.registeringRoot_});
                                  }};
            entry(&reg);
            m.library = handle;
            mods.push_back(m);
        } catch (const std::exception &e) {
            types.resize(first);
            if (handle)
                FreeLibrary(handle);
            issues.push_back(dir.filename().u8string() + ": " + e.what());
        }
    }
}
const RegisteredType *PluginManager::find(const std::string &id) const {
    for (const auto &t : types)
        if (id == t.api.id)
            return &t;
    return nullptr;
}
nlohmann::json PluginManager::defaults(const RegisteredType &t) const {
    nlohmann::json p = nlohmann::json::object();
    for (int i = 0; i < t.api.paramCount; ++i) {
        const auto &a = t.api.params[i];
        if (a.kind == orbmod::ParamKind::Text)
            p[a.key] = a.choices ? a.choices : "";
        else if (a.kind == orbmod::ParamKind::Toggle)
            p[a.key] = a.initial != 0;
        else if (a.kind == orbmod::ParamKind::Color)
            p[a.key] = {t.api.color.r / 255.f, t.api.color.g / 255.f, t.api.color.b / 255.f,
                        t.api.color.a / 255.f};
        else
            p[a.key] = a.initial;
    }
    return p;
}
std::vector<GenerationType> PluginManager::generationTypes() const {
    std::vector<GenerationType> v;
    for (const auto &t : types)
        if (t.api.placement)
            v.push_back({t.api.id, t.api.placement, defaults(t)});
    return v;
}
bool PluginManager::available(const SceneDocument &s, std::string &error) const {
    for (const auto &id : s.mods) {
        bool found = false;
        for (const auto &m : mods)
            if (m.id == id && m.enabled)
                found = true;
        if (!found) {
            error = "缺少模组: " + id;
            return false;
        }
    }
    for (const auto &o : s.objects) {
        auto *t = find(o.type);
        if (!t) {
            error = "缺少机关类型: " + o.type;
            return false;
        }
        for (int i = 0; i < t->api.paramCount; ++i) {
            const auto &p = t->api.params[i];
            auto it = o.params.find(p.key);
            if (it == o.params.end())
                continue;
            bool ok = true;
            if (p.kind == orbmod::ParamKind::Text)
                ok = it->is_string();
            else if (p.kind == orbmod::ParamKind::Toggle)
                ok = it->is_boolean() || it->is_number();
            else if (p.kind == orbmod::ParamKind::Color) {
                ok = it->is_array() && it->size() == 4;
                if (ok)
                    for (const auto &n : *it)
                        if (!n.is_number())
                            ok = false;
            } else
                ok = it->is_number();
            if (!ok) {
                error = o.type + " / " + p.key + ": 参数类型错误";
                return false;
            }
        }
    }
    error.clear();
    return true;
}
} // namespace orbduel
