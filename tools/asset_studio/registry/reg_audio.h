#pragma once

// ============================================================
// BGM / 音效注册
//
// 只登记元数据；真正的 WAV 由 renderer/audiopreview.cpp 直接调用
// 游戏 audio.h 的合成函数离线导出，保证试听与游戏内声音同源。
// ============================================================

#include "reg_common.h"
#include "audio_manifest.h"

inline void registerAllAudio() {
    // BGM：category=audio，tag 标出 BGM 与分组。
#define REG_AUDIO_BGM_ENTRY(id, name, chapter, key, seconds, group, desc, source) \
    reg(id, name, chapter, "audio") \
        .describe(desc) \
        .from(source) \
        .tag("音频").tag("BGM").tag(group) \
        .audioClip("bgm", seconds);
    AUDIO_BGM_ASSETS(REG_AUDIO_BGM_ENTRY)
#undef REG_AUDIO_BGM_ENTRY

    // SFX：source 里的方法名只用于定位源码，不参与绘制覆盖率扫描。
#define REG_AUDIO_SFX_ENTRY(id, name, chapter, key, seconds, group, desc, source) \
    reg(id, name, chapter, "audio") \
        .describe(desc) \
        .from(source) \
        .tag("音频").tag("音效").tag(group) \
        .audioClip("sfx", seconds);
    AUDIO_SFX_ASSETS(REG_AUDIO_SFX_ENTRY)
#undef REG_AUDIO_SFX_ENTRY
}
