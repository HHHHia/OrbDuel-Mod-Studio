#include "orbduel/mods/PluginManager.hpp"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>

namespace orbduel {
namespace fs = std::filesystem;
namespace {
#if defined(_WIN32)
using Library = HMODULE;
void closeLibrary(Library h) {
    FreeLibrary(h);
}
auto symbol(Library h, const char *name) {
    return GetProcAddress(h, name);
}
unsigned long processId() {
    return GetCurrentProcessId();
}
const wchar_t *librarySuffix = L".dll";
#else
using Library = void *;
void closeLibrary(Library h) {
    dlclose(h);
}
auto symbol(Library h, const char *name) {
    return dlsym(h, name);
}
unsigned long processId() {
    return static_cast<unsigned long>(getpid());
}
const wchar_t *librarySuffix = L".dylib";
#endif
} // namespace
PluginManager::~PluginManager() {
    clear();
}
void PluginManager::clear() {
    types.clear();
    for (auto &m : mods)
        if (m.library)
            closeLibrary(static_cast<Library>(m.library));
    mods.clear();
    // 仅删除本实例创建并记录的文件和空目录，不递归清理用户模组目录。
    std::error_code ec;
    for (const auto &file : cacheFiles_)
        fs::remove(fs::u8path(file), ec);
    for (auto it = cacheDirectories_.rbegin(); it != cacheDirectories_.rend(); ++it)
        fs::remove(fs::u8path(*it), ec);
    cacheFiles_.clear();
    cacheDirectories_.clear();
}
void PluginManager::load(const std::string &root, const std::vector<std::string> &disabled,
                         const std::vector<std::string> &extraRoots) {
    clear();
    issues.clear();
    std::error_code ec;

    std::vector<fs::path> dirs;
    std::vector<std::string> roots{root};
    roots.insert(roots.end(), extraRoots.begin(), extraRoots.end());
    std::map<std::string, fs::path> selected;
    for (const auto &source : roots) {
        std::vector<fs::path> entries;
        fs::create_directories(fs::u8path(source), ec);
        for (const auto &e : fs::directory_iterator(fs::u8path(source), ec))
            if (e.is_directory() && e.path().filename() != ".orbduel-cache")
                entries.push_back(e.path());
        std::sort(entries.begin(), entries.end());
        for (const auto &entry : entries) {
            dirs.push_back(entry);
            if (extraRoots.empty())
                continue;
            // 只做目录优先级选择；错误仍由下方加载阶段统一报告。
            try {
                std::ifstream in(entry / "mod.json");
                auto manifest = nlohmann::json::parse(in, nullptr, false);
                if (manifest.is_object() && manifest.contains("id") && manifest["id"].is_string())
                    selected[manifest["id"].get<std::string>()] = entry;
            } catch (...) {
            }
        }
    }
    fs::path session;
    static unsigned long serial = 0;
    int ordinal = 0;
    for (const auto &dir : dirs) {
        if (dir.filename() == ".orbduel-cache")
            continue;
        ++ordinal;
        auto file = dir / "mod.json";
        if (!fs::exists(file))
            continue;
        const auto first = types.size();
        Library handle = nullptr;
        try {
            std::ifstream stream(file);
            nlohmann::json j;
            stream >> j;
            LoadedMod m;
            m.id = j.at("id").get<std::string>();
            if (selected.count(m.id) && selected[m.id] != dir)
                continue;
            m.name = j.value("name", m.id);
            m.path = dir.u8string();
            m.enabled = std::find(disabled.begin(), disabled.end(), m.id) == disabled.end();
            if (!m.enabled) {
                mods.push_back(m);
                continue;
            }
            auto dll = fs::absolute(dir / fs::u8path(j.at("library").get<std::string>()));
            // 加载副本，原安装目录保持可写；作者可在游戏运行时重新构建安装。
            if (session.empty()) {
                const auto cacheRoot = extraRoots.empty() ? root : extraRoots.back();
                const auto base = fs::absolute(fs::u8path(cacheRoot) / ".orbduel-cache");
                fs::create_directories(base);
                do {
                    session = base / (std::to_string(processId()) + "-" + std::to_string(++serial));
                } while (!fs::create_directory(session));
                cacheDirectories_.push_back(session.u8string());
            }
            const auto cache = session / std::to_string(ordinal);
            fs::create_directory(cache);
            cacheDirectories_.push_back(cache.u8string());
            for (const auto &peer : fs::directory_iterator(dll.parent_path())) {
                auto ext = peer.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if (!peer.is_regular_file() || ext != librarySuffix)
                    continue;
                const auto copy = cache / peer.path().filename();
                cacheFiles_.push_back(copy.u8string());
                fs::copy_file(peer.path(), copy, fs::copy_options::overwrite_existing);
            }
            const auto loaded = cache / dll.filename();
#if defined(_WIN32)
            handle = LoadLibraryExW(loaded.c_str(), nullptr,
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
            handle = dlopen(loaded.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
            if (!handle)
                throw std::runtime_error("原生模组无法加载，请确认与游戏使用相同系统和 CPU 架构并重新构建");
            auto entry = reinterpret_cast<orbmod::Entry>(symbol(handle, "orbduel_register"));
            if (!entry)
                throw std::runtime_error("找不到 orbduel_register 入口");
            registeringId_ = m.id;
            if (auto receive =
                    reinterpret_cast<orbmod::EntityApiReceiver>(symbol(handle, "orbduel_set_entity_api")))
                receive(entityApi_);
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
                closeLibrary(handle);
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
        if (t.api.placement > 0)
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
