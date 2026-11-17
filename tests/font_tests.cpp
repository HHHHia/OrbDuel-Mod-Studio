#include "UiFont.hpp"
#include <imgui.h>
#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const auto path = std::filesystem::u8path(argv[1]);
    if (!std::filesystem::is_regular_file(path))
        return 3;
    ImGui::CreateContext();
    auto &fonts = *ImGui::GetIO().Fonts;
    std::string error;
    if (orbduel::loadBundledUiFont(fonts, path.parent_path() / "missing", 18.f, error) ||
        error.empty() || !fonts.Fonts.empty())
        return 4;
    if (!orbduel::loadBundledUiFont(fonts, path.parent_path().parent_path(), 18.f, error))
        return 5;
    ImFont *font = fonts.Fonts.back();
    unsigned char *pixels = nullptr;
    int width = 0, height = 0;
    fonts.GetTexDataAsAlpha8(&pixels, &width, &height);
    if (!pixels || !font->FindGlyphNoFallback('A') || !font->FindGlyphNoFallback(0x4e2d) ||
        !font->FindGlyphNoFallback(0x6587) || !font->FindGlyphNoFallback(0x9f99))
        return 6;
    std::printf("Bundled font OK; atlas %d x %d\n", width, height);
    ImGui::DestroyContext();
    return 0;
}
