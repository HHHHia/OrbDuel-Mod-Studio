#pragma once

// 名字显示：有中文字体就优先 name.zh，否则退回 name.en / id。
// 菜单、HUD、技能飘字共用这一份判断，避免三处各写一遍。

#include <string>

#include "orbduel/content/DataModel.hpp"

namespace orbduel {

inline std::string displayName(const LocalizedText &name, const std::string &id, bool cjk) {
    if (cjk && !name.zh.empty()) {
        return name.zh;
    }
    return name.en.empty() ? id : name.en;
}

inline std::string characterLabel(const CharacterDef &def, bool cjk) {
    return displayName(def.name, def.id, cjk);
}

inline std::string skillLabel(const SkillDef &def, bool cjk) {
    return displayName(def.name, def.id, cjk);
}

} // namespace orbduel
