// 这个文件不产生任何逻辑，只把所有公开头文件 include 一遍。
// 作用：阶段一之前就能发现头文件语法错误、缺 include、命名冲突。
// 新增头文件时记得加到这里（否则 CI 不会替你检查）。

#include "orbduel/version.hpp"

#include "orbduel/core/Event.hpp"
#include "orbduel/core/Log.hpp"
#include "orbduel/core/Rng.hpp"
#include "orbduel/core/Time.hpp"
#include "orbduel/core/Types.hpp"

#include "orbduel/physics/Arena.hpp"
#include "orbduel/physics/Body.hpp"
#include "orbduel/physics/Collision.hpp"
#include "orbduel/physics/World.hpp"

#include "orbduel/render/AssetCache.hpp"
#include "orbduel/render/DebugDraw.hpp"
#include "orbduel/render/Fx.hpp"
#include "orbduel/render/Renderer.hpp"

#include "orbduel/content/ContentLoader.hpp"
#include "orbduel/content/DataModel.hpp"

#include "orbduel/match/Match.hpp"
#include "orbduel/match/MatchRule.hpp"
#include "orbduel/match/Skill.hpp"
#include "orbduel/match/StallGuard.hpp"
#include "orbduel/match/Zone.hpp"

#include "orbduel/game/Game.hpp"
#include "orbduel/game/Input.hpp"

namespace orbduel {

// 避免编译器抱怨空翻译单元。
int compileCheckAnchor() {
    return static_cast<int>(EventType::Count);
}

} // namespace orbduel
