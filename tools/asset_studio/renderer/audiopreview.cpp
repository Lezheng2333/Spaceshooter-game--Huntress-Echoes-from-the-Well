// ============================================================
// Asset Studio — BGM / SFX 离线试听渲染器
//
// 为什么需要独立二进制：
//   · 游戏声音不是 WAV 文件，而是 AudioEngine 内的实时合成；
//   · 本程序直接 include 游戏真实 audio.h，并调用同一套乐谱 / snd* 函数，
//     导出 44.1kHz 单声道 16-bit WAV 给网页 <audio> 播放。
//   · 主游戏代码零改动；只把编解码结果写入 asset_studio/cache/。
//
// 用法：
//   audiopreview --render audio.bgm.title --out /tmp/title.wav
//   audiopreview --verify-all [--json]
//
// 说明：编译时使用 -fno-access-control，让试听器可以操作 AudioEngine
// 的私有合成状态；该编译选项只作用于本工具，不改变游戏本体。
// ============================================================

#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "audio.h"
#include "audio_manifest.h"

struct BgmDef {
    const char* id;
    const char* mode;
    double seconds;
};

struct SfxDef {
    const char* id;
    void (AudioEngine::*fn)();
    double seconds;
};

static const BgmDef BGM_TABLE[] = {
#define X(id, name, chapter, key, seconds, group, desc, source) { id, key, seconds },
    AUDIO_BGM_ASSETS(X)
#undef X
};

static const SfxDef SFX_TABLE[] = {
#define X(id, name, chapter, key, seconds, group, desc, source) { id, &AudioEngine::key, seconds },
    AUDIO_SFX_ASSETS(X)
#undef X
};

static const int SAMPLE_RATE = 44100;

static void prepareEngine(AudioEngine& eng) {
    // 构造时会打开 dummy 音频设备；立即暂停，避免后台回调和手工离线渲染抢状态。
    if (eng.audioDev) SDL_PauseAudioDevice(eng.audioDev, 1);

    eng.bgmVolume = 7;
    eng.sfxVolume = 9;
    eng.eqLow = 0;
    eng.eqMid = 0;
    eng.eqHigh = 0;
    eng.musicIntensity = 2;
    eng.voiceGain = 0.9f;
    eng.voiceClips.clear();
    eng.activeSnd.clear();
}

static void setMusicFlags(AudioEngine& eng, const std::string& mode) {
    eng.bossMusicOn = false;
    eng.ch2Bgm = false;
    eng.bgmOff = false;
    eng.startBgm = false;
    eng.upgradeMusicOn = false;

    int modeEnum = AudioEngine::MUSIC_CH1;
    if (mode == "title") {
        eng.startBgm = true;
        modeEnum = AudioEngine::MUSIC_TITLE;
    } else if (mode == "ch2") {
        eng.ch2Bgm = true;
        modeEnum = AudioEngine::MUSIC_CH2;
    } else if (mode == "boss") {
        eng.bossMusicOn = true;
        modeEnum = AudioEngine::MUSIC_BOSS;
    } else if (mode == "upgrade") {
        eng.upgradeMusicOn = true;
        modeEnum = AudioEngine::MUSIC_UPGRADE;
    }
    eng.resetMusicState(modeEnum);
    eng.musicGain = 1.0f;
}

static bool renderBgm(AudioEngine& eng, const std::string& mode, double seconds,
                      std::vector<float>& out) {
    setMusicFlags(eng, mode);
    int total = (int)std::ceil(seconds * SAMPLE_RATE);
    out.clear();
    out.reserve(total);
    std::vector<float> block(1024, 0.0f);
    while ((int)out.size() < total) {
        int n = std::min((int)block.size(), total - (int)out.size());
        std::fill(block.begin(), block.begin() + n, 0.0f);
        eng.audioCB((Uint8*)block.data(), n * (int)sizeof(float));
        out.insert(out.end(), block.begin(), block.begin() + n);
    }
    return true;
}

static bool renderSfx(AudioEngine& eng, void (AudioEngine::*fn)(), double seconds,
                      std::vector<float>& out) {
    // 试听 SFX 时保持 BGM 静音；activeSnd 是游戏真实音效总线。
    setMusicFlags(eng, "ch1");
    eng.bgmOff = true;
    eng.musicGain = 0.0f;
    eng.activeSnd.clear();
    (eng.*fn)();

    int total = (int)std::ceil(seconds * SAMPLE_RATE);
    out.clear();
    out.reserve(total);
    std::vector<float> block(1024, 0.0f);
    while ((int)out.size() < total) {
        int n = std::min((int)block.size(), total - (int)out.size());
        std::fill(block.begin(), block.begin() + n, 0.0f);
        eng.audioCB((Uint8*)block.data(), n * (int)sizeof(float));
        out.insert(out.end(), block.begin(), block.begin() + n);
    }
    return true;
}

static bool renderById(const std::string& id, std::vector<float>& out, double* secondsOut) {
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 0);
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[ERR] SDL 音频初始化失败: %s\n", SDL_GetError());
        return false;
    }

    AudioEngine eng;
    if (!eng.audioDev) {
        std::fprintf(stderr, "[ERR] 无法打开 dummy 音频设备: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    prepareEngine(eng);

    bool found = false;
    for (size_t i = 0; i < sizeof(BGM_TABLE) / sizeof(BGM_TABLE[0]); ++i) {
        if (id == BGM_TABLE[i].id) {
            renderBgm(eng, BGM_TABLE[i].mode, BGM_TABLE[i].seconds, out);
            if (secondsOut) *secondsOut = BGM_TABLE[i].seconds;
            found = true;
            break;
        }
    }
    if (!found) {
        for (size_t i = 0; i < sizeof(SFX_TABLE) / sizeof(SFX_TABLE[0]); ++i) {
            if (id == SFX_TABLE[i].id) {
                renderSfx(eng, SFX_TABLE[i].fn, SFX_TABLE[i].seconds, out);
                if (secondsOut) *secondsOut = SFX_TABLE[i].seconds;
                found = true;
                break;
            }
        }
    }

    SDL_Quit();
    if (!found) std::fprintf(stderr, "[ERR] 找不到音频素材: %s\n", id.c_str());
    return found;
}

static void writeU16(std::FILE* f, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff)};
    std::fwrite(b, 1, 2, f);
}
static void writeU32(std::FILE* f, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff),
                          (unsigned char)((v >> 16) & 0xff), (unsigned char)((v >> 24) & 0xff)};
    std::fwrite(b, 1, 4, f);
}

static bool writeWav(const char* path, const std::vector<float>& samples) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "[ERR] 无法写出 WAV: %s\n", path);
        return false;
    }
    uint32_t dataBytes = (uint32_t)(samples.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    writeU32(f, 36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    writeU32(f, 16);
    writeU16(f, 1);                 // PCM
    writeU16(f, 1);                 // mono
    writeU32(f, SAMPLE_RATE);
    writeU32(f, SAMPLE_RATE * 2);   // byte rate
    writeU16(f, 2);                 // block align
    writeU16(f, 16);                // bits
    std::fwrite("data", 1, 4, f);
    writeU32(f, dataBytes);
    for (size_t i = 0; i < samples.size(); ++i) {
        float s = samples[i];
        if (!std::isfinite(s)) s = 0.0f;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t pcm = (int16_t)std::lround(s * 32767.0f);
        writeU16(f, (uint16_t)pcm);
    }
    std::fclose(f);
    return true;
}

static double rmsOf(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    double e = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) e += (double)samples[i] * samples[i];
    return std::sqrt(e / (double)samples.size());
}

static int verifyAll(bool json) {
    const size_t bgmCount = sizeof(BGM_TABLE) / sizeof(BGM_TABLE[0]);
    const size_t sfxCount = sizeof(SFX_TABLE) / sizeof(SFX_TABLE[0]);
    int ok = 0, failed = 0;
    std::string failedIds;
    if (json) std::printf("{\"results\":[");
    bool first = true;

    for (size_t i = 0; i < bgmCount + sfxCount; ++i) {
        const char* id = i < bgmCount ? BGM_TABLE[i].id : SFX_TABLE[i - bgmCount].id;
        std::vector<float> clip;
        double seconds = 0.0;
        bool rendered = renderById(id, clip, &seconds);
        double rms = rendered ? rmsOf(clip) : 0.0;
        bool good = rendered && clip.size() > 0 && std::isfinite(rms) && rms > 0.0005;
        if (good) ++ok; else { ++failed; if (!failedIds.empty()) failedIds += ","; failedIds += id; }
        if (json) {
            if (!first) std::printf(",");
            first = false;
            std::printf("{\"id\":\"%s\",\"seconds\":%.3f,\"samples\":%d,\"rms\":%.6f,\"ok\":%s}",
                        id, seconds, (int)clip.size(), rms, good ? "true" : "false");
        } else {
            std::printf("%s  %-30s %6.2fs  rms=%.5f\n", good ? "  OK  " : " FAIL ",
                        id, seconds, rms);
        }
    }
    if (json) {
        std::printf("],\"ok\":%d,\"failed\":%d,\"total\":%d,\"failedIds\":[", ok, failed, ok + failed);
        size_t start = 0;
        std::string one;
        for (size_t i = 0; i <= failedIds.size(); ++i) {
            if (i == failedIds.size() || failedIds[i] == ',') {
                if (i > start) {
                    if (start) std::printf(",");
                    std::printf("\"%s\"", failedIds.substr(start, i - start).c_str());
                }
                start = i + 1;
            }
        }
        std::printf("]}\n");
    } else {
        std::printf("\n音频自检：通过 %d，失败 %d，共 %d\n", ok, failed, ok + failed);
    }
    return failed == 0 ? 0 : 1;
}

static void usage() {
    std::printf("Asset Studio — BGM / SFX 离线试听渲染器\n\n");
    std::printf("  audiopreview --render <音频id> --out <path.wav>\n");
    std::printf("  audiopreview --verify-all [--json]\n\n");
}

int main(int argc, char** argv) {
    std::string cmd, id, out;
    bool json = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--render" && i + 1 < argc) { cmd = "render"; id = argv[++i]; }
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--verify-all") cmd = "verify";
        else if (a == "--json") json = true;
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { std::fprintf(stderr, "[ERR] 未知参数: %s\n", a.c_str()); usage(); return 2; }
    }

    if (cmd == "verify") return verifyAll(json);
    if (cmd != "render" || id.empty() || out.empty()) { usage(); return 2; }

    std::vector<float> clip;
    double seconds = 0.0;
    if (!renderById(id, clip, &seconds)) return 1;
    if (!writeWav(out.c_str(), clip)) return 1;
    std::printf("{\"ok\":true,\"id\":\"%s\",\"seconds\":%.3f,\"samples\":%d,\"out\":\"%s\"}\n",
                id.c_str(), seconds, (int)clip.size(), out.c_str());
    return 0;
}
