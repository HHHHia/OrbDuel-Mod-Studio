#include "Audio.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace orbduel {
namespace fs = std::filesystem;
namespace {
constexpr int kVoices = 12;
constexpr float kMasterGain = 0.8f;
constexpr int kWantFreq = 44100;
constexpr int kWantFrames = 1024;
} // namespace

Audio::~Audio() {
    shutdown();
}

void Audio::shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    if (plays_ > 0)
        std::fprintf(stderr, "audio: 本次共播放 %u 个音效\n", plays_);
    plays_ = 0;
    for (auto &entry : sounds_)
        if (entry.second.data != nullptr)
            SDL_FreeWAV(entry.second.data);
    sounds_.clear();
    voices_.clear();
    lastPlayMs_.clear();
    mix_.clear();
    if (audioSubsystem_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystem_ = false;
    }
}

bool Audio::init(const std::string &dir) {
    dir_ = dir;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(dir_), ec)) {
        std::fprintf(stderr, "audio: 找不到音效目录 %s，静音运行\n", dir_.c_str());
        return false;
    }
    if (!audioSubsystem_) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::fprintf(stderr, "audio: SDL 音频初始化失败: %s\n", SDL_GetError());
            return false;
        }
        audioSubsystem_ = true;
    }

    SDL_AudioSpec want{};
    want.freq = kWantFreq;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = kWantFrames;
    want.callback = &Audio::callback;
    want.userdata = this;
    // 只允许改采样率：格式固定成 S16 单声道，回调就不用再分支。
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (device_ == 0) {
        std::fprintf(stderr, "audio: 没有可用的音频输出设备: %s\n", SDL_GetError());
        return false;
    }
    mix_.assign(std::size_t(spec_.samples) + 32, 0.0f);
    voices_.assign(kVoices, Voice{});
    SDL_PauseAudioDevice(device_, 0);
    std::fprintf(stderr, "audio: %s 已就绪（%d Hz 单声道 S16）\n", dir_.c_str(), int(spec_.freq));
    return true;
}

void Audio::callback(void *user, Uint8 *stream, int len) {
    auto &self = *static_cast<Audio *>(user);
    auto *out = reinterpret_cast<Sint16 *>(stream);
    const int frames = len / int(sizeof(Sint16)); // 设备是单声道
    if (frames <= 0)
        return;
    if (frames > int(self.mix_.size())) {
        SDL_memset(stream, 0, len);
        return;
    }

    std::fill(self.mix_.begin(), self.mix_.begin() + frames, 0.0f);
    for (Voice &voice : self.voices_) {
        if (voice.sound == nullptr)
            continue;
        const auto *src = reinterpret_cast<const Sint16 *>(voice.sound->data);
        const Uint32 total = voice.sound->len / sizeof(Sint16);
        const Uint32 start = std::min<Uint32>(voice.pos / sizeof(Sint16), total);
        const int count = std::min<int>(frames, int(total - start));
        for (int i = 0; i < count; ++i)
            self.mix_[i] += float(src[start + i]) * voice.gain / 32768.0f;
        voice.pos += Uint32(count) * sizeof(Sint16);
        if (voice.pos >= voice.sound->len)
            voice.sound = nullptr;
    }

    for (int i = 0; i < frames; ++i) {
        const float sample = std::max(-1.0f, std::min(1.0f, self.mix_[i] * kMasterGain));
        out[i] = Sint16(sample * 32767.0f);
    }
}

const Audio::Sound *Audio::load(const std::string &id) {
    auto known = sounds_.find(id);
    if (known != sounds_.end())
        return known->second.data != nullptr ? &known->second : nullptr;

    const std::string path = dir_ + "/" + id + ".wav";
    SDL_AudioSpec wav{};
    Uint8 *data = nullptr;
    Uint32 len = 0;
    if (SDL_LoadWAV(path.c_str(), &wav, &data, &len) == nullptr) {
        std::fprintf(stderr, "audio: 读不到 %s（%s），跳过\n", path.c_str(), SDL_GetError());
        sounds_.emplace(id, Sound{});
        return nullptr;
    }
    if (wav.format != AUDIO_S16SYS || wav.channels != 1 || len < sizeof(Sint16)) {
        std::fprintf(stderr, "audio: %s 不是 S16 单声道，跳过\n", path.c_str());
        SDL_FreeWAV(data);
        sounds_.emplace(id, Sound{});
        return nullptr;
    }

    // 设备采样率和文件不一致时转一次，之后就按原样混音。
    if (wav.freq != spec_.freq) {
        SDL_AudioStream *stream =
            SDL_NewAudioStream(wav.format, wav.channels, wav.freq, AUDIO_S16SYS, 1, spec_.freq);
        if (stream != nullptr) {
            SDL_AudioStreamPut(stream, data, int(len));
            SDL_AudioStreamFlush(stream);
            const int available = SDL_AudioStreamAvailable(stream);
            auto *converted = static_cast<Uint8 *>(SDL_malloc(std::size_t(available)));
            if (converted != nullptr) {
                const int got = SDL_AudioStreamGet(stream, converted, available);
                if (got > 0) {
                    SDL_FreeWAV(data);
                    data = converted;
                    len = Uint32(got);
                } else {
                    SDL_free(converted);
                }
            }
            SDL_FreeAudioStream(stream);
        }
    }

    auto inserted = sounds_.emplace(id, Sound{data, len});
    return &inserted.first->second;
}

void Audio::play(const char *id, float gain, int gapMs) {
    if (device_ == 0 || id == nullptr || *id == '\0')
        return;
    const Uint32 now = SDL_GetTicks();
    if (gapMs > 0) {
        auto last = lastPlayMs_.find(id);
        if (last != lastPlayMs_.end() && now - last->second < Uint32(gapMs))
            return;
    }
    const Sound *sound = load(id);
    if (sound == nullptr)
        return;
    lastPlayMs_[id] = now;
    ++plays_;

    SDL_LockAudioDevice(device_);
    Voice *slot = nullptr;
    for (Voice &voice : voices_) {
        if (voice.sound == nullptr) {
            slot = &voice;
            break;
        }
    }
    if (slot == nullptr) {
        slot = &voices_[steal_ % voices_.size()];
        steal_ = (steal_ + 1) % voices_.size();
    }
    slot->sound = sound;
    slot->pos = 0;
    slot->gain = std::max(0.0f, gain);
    SDL_UnlockAudioDevice(device_);
}

} // namespace orbduel
