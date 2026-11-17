#pragma once

// 最小音效播放器：SDL2 自带音频子系统 + content/audio/orbduel 下的 16-bit WAV。
//
// 为什么不用 SDL2_mixer：那是新增第三方依赖（AGENTS 铁律 5）。这几个短音效只需要
// SDL_LoadWAV + 一个音频回调就能放，Kenney 的 OGG 包因此只当素材库，暂不参与播放。
//
// 线程模型：play() 只在主线程调用，混音在 SDL 音频线程。voice 表由设备锁保护；
// 已加载的 Sound 在 Audio 生命周期内不释放，所以回调里不拿锁。
//
// 失败策略：设备打不开、目录缺失、WAV 读失败都只记一行日志并静音继续，不打断对局。

#include <SDL.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace orbduel {

class Audio {
  public:
    Audio() = default;
    ~Audio();
    Audio(const Audio &) = delete;
    Audio &operator=(const Audio &) = delete;

    // dir 是 WAV 目录（一般是 content/audio/orbduel）。返回 false 表示静音运行。
    bool init(const std::string &dir);
    void shutdown();
    // 音频设备是否可用；false 时所有 play() 都是空操作。
    bool ready() const { return device_ != 0; }

    // id 是不带扩展名的文件名；gain 是音量（1 = 原样）；gapMs > 0 时同一 id 在间隔内不重复播放。
    void play(const char *id, float gain = 1.0f, int gapMs = 0);

  private:
    struct Sound {
        Uint8 *data = nullptr; // 已转成设备采样率的 PCM（S16 单声道）
        Uint32 len = 0;
    };
    struct Voice {
        const Sound *sound = nullptr;
        Uint32 pos = 0;
        float gain = 1.0f;
    };

    static void callback(void *user, Uint8 *stream, int len);
    const Sound *load(const std::string &id);

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    bool audioSubsystem_ = false;
    std::string dir_;
    std::unordered_map<std::string, Sound> sounds_;
    std::unordered_map<std::string, Uint32> lastPlayMs_;
    std::vector<Voice> voices_;
    std::vector<float> mix_; // 回调里的浮点累加缓冲，长度 = 设备缓冲帧数
    std::size_t steal_ = 0;  // 所有 voice 都在响时轮流抢占的位置
    unsigned plays_ = 0;     // 本次运行实际播放次数，退出时报一行数字
};

} // namespace orbduel
