#pragma once

// ============================================================
// 预览宿主 —— 素材注册表的数据结构 + 渲染上下文
//
// 设计要点：draw 回调是「适配 lambda」，负责构造游戏实例、摆好状态、
// 调用游戏真实的 draw()。这样即使某些类的成员是 private
// （Ch2SphereBoss / Ch2SkillOrb / Ch1Boss 都有 private 段）也能接入。
// ============================================================

#include <SDL.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <cstdio>

// ---------- 可调参数（网页据此自动生成滑块） ----------
struct ParamSpec {
    std::string key;
    std::string label;
    double mn = 0, mx = 1, def = 0, step = 0.1;
};

// ---------- 可切换状态（单炮/三炮、Boss 阶段、入场进度…） ----------
struct StateSpec {
    std::string key;
    std::string label;
};

// ---------- 绘制上下文 ----------
struct DrawCtx {
    SDL_Renderer* r = nullptr;
    std::map<std::string, double> params;
    std::string state;
    double t = 0.0;        // 动画进度 0..1
    int    frame = 0;
    int    totalFrames = 1;
    int    W = 800, H = 600;

    // 跨帧持久状态（动画需要：背景要一直滚动，而不是每帧重建）
    void* priv = nullptr;
    void* scenePriv = nullptr;

    double P(const std::string& k, double def) const {
        std::map<std::string, double>::const_iterator it = params.find(k);
        return it == params.end() ? def : it->second;
    }
    bool S(const std::string& k) const { return state == k; }
};

// ---------- 素材条目 ----------
struct AssetEntry {
    std::string id;        // "ch2.player.nightelf"
    std::string name;      // "暗夜精灵号"
    std::string chapter;   // "ch1" / "ch2" / "shared"
    std::string category;  // "player" / "enemy" / "boss" / "bullet" / "fx" / "hud" / "bg"
    std::string desc;
    std::vector<std::string> tags;
    std::vector<ParamSpec>   params;
    std::vector<StateSpec>   states;

    bool        crop = true;      // 缩略图自动裁剪到内容（小素材必备）
    int         frames = 1;       // 默认动画帧数
    std::string scene;            // 可选的场景上下文素材 id
    std::string source;           // 对应游戏源码位置（供"打开源码"用）

    // 媒体类型：image（默认绘制素材）或 audio（BGM / 音效试听）
    std::string media = "image";
    std::string audioType;        // bgm / sfx
    double      duration = 0.0;   // 音频试听长度（秒）

    void (*draw)(DrawCtx&) = nullptr;
    // 叠加层回调：在「内容分析之后」绘制，所以不会污染包围盒/对称度/裁剪
    void (*overlay)(DrawCtx&) = nullptr;

    // ---- 链式配置 ----
    AssetEntry& tag(const std::string& t) { tags.push_back(t); return *this; }
    AssetEntry& describe(const std::string& d) { desc = d; return *this; }
    AssetEntry& from(const std::string& s) { source = s; return *this; }
    AssetEntry& withScene(const std::string& s) { scene = s; return *this; }
    AssetEntry& noCrop() { crop = false; return *this; }
    AssetEntry& framesN(int n) { frames = n; return *this; }

    AssetEntry& param(const std::string& k, const std::string& label,
                      double mn, double mx, double def, double step) {
        ParamSpec p; p.key = k; p.label = label; p.mn = mn; p.mx = mx; p.def = def; p.step = step;
        params.push_back(p); return *this;
    }
    AssetEntry& state_(const std::string& k, const std::string& label) {
        StateSpec s; s.key = k; s.label = label; states.push_back(s); return *this;
    }
    AssetEntry& drawFn(void (*f)(DrawCtx&)) { draw = f; return *this; }
    AssetEntry& overlayFn(void (*f)(DrawCtx&)) { overlay = f; return *this; }
    AssetEntry& audioClip(const std::string& type, double seconds) {
        media = "audio"; audioType = type; duration = seconds;
        return *this;
    }
};

// ---------- 全局注册表 ----------
// 用 deque 保证 push_back 后已有引用不失效
inline std::deque<AssetEntry>& assetRegistry() { static std::deque<AssetEntry> v; return v; }

inline AssetEntry& reg(const std::string& id, const std::string& name,
                       const std::string& chapter, const std::string& category) {
    AssetEntry e;
    e.id = id; e.name = name; e.chapter = chapter; e.category = category;
    assetRegistry().push_back(e);
    return assetRegistry().back();
}

inline AssetEntry* findAsset(const std::string& id) {
    std::deque<AssetEntry>& v = assetRegistry();
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].id == id) return &v[i];
    return nullptr;
}
