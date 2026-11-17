#pragma once

// 从内容目录读取随包字体；用 filesystem 路径打开，兼容包含中文的 Windows 安装路径。
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string>
#include <vector>

namespace orbduel {
inline bool loadBundledUiFont(ImFontAtlas &fonts, const std::filesystem::path &contentRoot, float size,
                              std::string &error) {
    const auto path = contentRoot / "fonts" / "NotoSansSC-wght.ttf";
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const std::streamsize length = input ? static_cast<std::streamsize>(input.tellg()) : 0;
    if (length <= 0 || length > 64 * 1024 * 1024) {
        error = "Could not read bundled Chinese font: " + path.u8string();
        return false;
    }
    std::vector<char> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!input.read(bytes.data(), length)) {
        error = "Could not read bundled Chinese font: " + path.u8string();
        return false;
    }
    ImFontConfig cfg;
    // ImGui 会复制这份内存，局部字节缓冲区随后可安全释放。
    cfg.FontDataOwnedByAtlas = false;
    if (!fonts.AddFontFromMemoryTTF(bytes.data(), static_cast<int>(length), size, &cfg,
                                     fonts.GetGlyphRangesChineseFull())) {
        error = "Could not load bundled Chinese font: " + path.u8string();
        return false;
    }
    error.clear();
    return true;
}
} // namespace orbduel
