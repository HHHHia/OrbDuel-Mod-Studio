#pragma once

// 输入层：现阶段没有任何玩家操作（自动对战），所以 sample() 返回全零。
// 接玩家输入时只改这里，match / physics 不动。

namespace orbduel {

struct InputState {
    bool boostA = false;   // 预留：玩家 1 冲刺
    bool boostB = false;   // 预留：玩家 2 冲刺
    bool pause = false;
};

class Input {
public:
    Input() = default;
    ~Input() = default;

    InputState sample() const;
};

} // namespace orbduel
