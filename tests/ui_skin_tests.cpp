#include "UiSkin.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    const fs::path content = fs::u8path(argv[1]);
    const fs::path scratch = fs::u8path(argv[2]);
    const auto ids = orbduel::listUiSkins(content);
    if (std::find(ids.begin(), ids.end(), "midnight") == ids.end() ||
        std::find(ids.begin(), ids.end(), "aurora") == ids.end())
        return 3;

    orbduel::UiSkin selected;
    std::string error;
    if (!orbduel::loadUiSkin(content, "midnight", selected, error) || selected.fontSize != 18 ||
        selected.color("primaryButton")[0] != 0.38f)
        return 4;
    if (!orbduel::loadUiSkin(content, "aurora", selected, error) || selected.fontSize != 19 ||
        selected.color("primaryButton")[0] >= 0.38f)
        return 5;

    const auto previous = selected.nameEn;
    if (orbduel::loadUiSkin(content, "../midnight", selected, error) || selected.nameEn != previous)
        return 6;
    if (orbduel::loadUiSkin(content, "missing", selected, error) || selected.nameEn != previous)
        return 7;

    fs::create_directories(scratch / "ui" / "skins");
    auto broken = nlohmann::json::parse(std::ifstream(content / "ui" / "skins" / "midnight.json"));
    broken["colors"]["text"] = {1.5, 0.5, 0.5};
    std::ofstream(scratch / "ui" / "skins" / "broken.json") << broken.dump(2);
    if (orbduel::loadUiSkin(scratch, "broken", selected, error) || selected.nameEn != previous || error.empty())
        return 8;

    std::cout << "UI skin loading and invalid-config fallback passed\n";
    return 0;
}
