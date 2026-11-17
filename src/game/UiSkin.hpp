#pragma once

// 工作台皮肤只控制界面外观；场景风格和对战规则仍由地图文档决定。
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orbduel {
struct UiSkin {
    using Color = std::array<float, 4>;
    std::string nameEn, nameZh;
    float fontSize = 18, overviewTitleScale = 1, workshopTitleScale = 1;
    std::array<float, 2> windowPadding{}, framePadding{}, itemSpacing{};
    float windowRounding = 0, childRounding = 0, frameRounding = 0, grabRounding = 0, popupRounding = 0;
    float scrollbarSize = 0, windowBorderSize = 0, childBorderSize = 0;
    std::unordered_map<std::string, Color> colors;

    const Color &color(const char *key) const { return colors.at(key); }
};

inline bool validUiSkinId(const std::string &id) {
    if (id.empty())
        return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
              c == '_'))
            return false;
    return true;
}

inline std::vector<std::string> listUiSkins(const std::filesystem::path &contentRoot) {
    namespace fs = std::filesystem;
    std::vector<std::string> ids;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(contentRoot / "ui" / "skins", ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
            auto id = entry.path().stem().u8string();
            if (validUiSkinId(id))
                ids.push_back(std::move(id));
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

inline bool loadUiSkin(const std::filesystem::path &contentRoot, const std::string &id, UiSkin &result,
                       std::string &error) {
    error.clear();
    if (!validUiSkinId(id)) {
        error = "Invalid UI skin name: " + id;
        return false;
    }
    const auto path = contentRoot / "ui" / "skins" / (id + ".json");
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "UI skin file not found: " + path.u8string();
        return false;
    }
    auto json = nlohmann::json::parse(input, nullptr, false);
    if (!json.is_object()) {
        error = "Invalid UI skin JSON: " + path.u8string();
        return false;
    }
    try {
        auto number = [](const nlohmann::json &value, float min, float max) {
            if (!value.is_number())
                throw std::runtime_error("expected a number");
            float n = value.get<float>();
            if (!std::isfinite(n) || n < min || n > max)
                throw std::runtime_error("number outside allowed range");
            return n;
        };
        auto pair = [&](const nlohmann::json &value) {
            if (!value.is_array() || value.size() != 2)
                throw std::runtime_error("expected a two-number array");
            return std::array<float, 2>{number(value[0], 0, 64), number(value[1], 0, 64)};
        };
        auto rgba = [&](const nlohmann::json &value) {
            if (!value.is_array() || (value.size() != 3 && value.size() != 4))
                throw std::runtime_error("expected a three- or four-number color array");
            return UiSkin::Color{number(value[0], 0, 1), number(value[1], 0, 1), number(value[2], 0, 1),
                                 value.size() == 4 ? number(value[3], 0, 1) : 1.f};
        };
        UiSkin candidate;
        const auto &names = json.at("name");
        candidate.nameEn = names.at("en").get<std::string>();
        candidate.nameZh = names.at("zh").get<std::string>();
        if (candidate.nameEn.empty() || candidate.nameZh.empty())
            throw std::runtime_error("skin names cannot be empty");
        const auto &font = json.at("font");
        candidate.fontSize = number(font.at("size"), 14, 26);
        candidate.overviewTitleScale = number(font.at("overviewTitleScale"), 1, 3);
        candidate.workshopTitleScale = number(font.at("workshopTitleScale"), 1, 3);
        const auto &layout = json.at("layout");
        candidate.windowPadding = pair(layout.at("windowPadding"));
        candidate.framePadding = pair(layout.at("framePadding"));
        candidate.itemSpacing = pair(layout.at("itemSpacing"));
        candidate.scrollbarSize = number(layout.at("scrollbarSize"), 4, 40);
        candidate.windowBorderSize = number(layout.at("windowBorderSize"), 0, 4);
        candidate.childBorderSize = number(layout.at("childBorderSize"), 0, 4);
        const auto &rounding = json.at("rounding");
        candidate.windowRounding = number(rounding.at("window"), 0, 32);
        candidate.childRounding = number(rounding.at("child"), 0, 32);
        candidate.frameRounding = number(rounding.at("frame"), 0, 32);
        candidate.grabRounding = number(rounding.at("grab"), 0, 32);
        candidate.popupRounding = number(rounding.at("popup"), 0, 32);
        const auto &colors = json.at("colors");
        constexpr const char *keys[] = {
            "windowBg",      "childBg",       "border",       "frameBg",       "frameBgHovered",
            "frameBgActive", "button",        "buttonHovered", "buttonActive",  "header",
            "headerHovered", "checkMark",     "sliderGrab",   "text",          "textDisabled",
            "primaryButton", "primaryHovered", "dangerButton", "dangerHovered", "dangerActive",
            "exitHovered",   "exitActive",    "exitText",     "brandText",     "messageText",
            "clearBg"};
        for (const char *key : keys)
            candidate.colors.emplace(key, rgba(colors.at(key)));
        result = std::move(candidate);
        return true;
    } catch (const std::exception &e) {
        error = "Invalid UI skin " + path.u8string() + ": " + e.what();
        return false;
    }
}
} // namespace orbduel
