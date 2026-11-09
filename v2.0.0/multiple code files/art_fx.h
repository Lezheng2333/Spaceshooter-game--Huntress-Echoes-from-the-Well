#pragma once
// ============== ArtFX ==============
// 纯绘制的复用工具：统一 alpha 混合、实心多边形、光晕线、HUD 框体。
// 所有函数只读参数、不改游戏状态；不使用随机数，避免读档帧出现漂移。
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ArtFX {

static inline int clampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// RAII：临时开启 alpha 混合，离开作用域还原，避免某个绘制分支污染后续 HUD。
class BlendGuard {
    SDL_Renderer* r;
    SDL_BlendMode prev;
public:
    explicit BlendGuard(SDL_Renderer* renderer) : r(renderer) {
        prev = SDL_BLENDMODE_NONE;
        if (r) SDL_GetRenderDrawBlendMode(r, &prev);
        if (r) SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }
    ~BlendGuard() { if (r) SDL_SetRenderDrawBlendMode(r, prev); }
};

static inline unsigned hashU(unsigned x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline double hash01(int x) { return (hashU((unsigned)x) % 4096) / 4096.0; }

// 单帧最多 32 点的简单多边形填充。扫描线实现不依赖 SDL_RenderGeometry，
// 对旧版 SDL2 也保持兼容；小尺寸 UI/飞机/菱形等用法足够快。
static inline void fillPoly(SDL_Renderer* r, const SDL_Point* pts, int n,
                            int cr, int cg, int cb, int ca = 255) {
    if (!r || n < 3 || ca <= 0) return;
    int minY = pts[0].y, maxY = pts[0].y;
    for (int i = 1; i < n; ++i) {
        minY = std::min(minY, pts[i].y);
        maxY = std::max(maxY, pts[i].y);
    }
    if (minY < -16) minY = -16;
    if (maxY > 616) maxY = 616;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
    double xs[40];
    for (int y = minY; y <= maxY; ++y) {
        int count = 0;
        for (int i = 0; i < n; ++i) {
            const SDL_Point& a = pts[i];
            const SDL_Point& b = pts[(i + 1) % n];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                if (count < 40)
                    xs[count++] = (double)a.x + (double)(y - a.y) * (b.x - a.x) / (double)(b.y - a.y);
            }
        }
        if (count < 2) continue;
        std::sort(xs, xs + count);
        for (int i = 0; i + 1 < count; i += 2) {
            int x0 = (int)std::ceil(xs[i]);
            int x1 = (int)std::floor(xs[i + 1]);
            if (x1 >= x0) SDL_RenderDrawLine(r, x0, y, x1, y);
        }
    }
    SDL_SetRenderDrawBlendMode(r, prev);
}

static inline void glowLine(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                            int cr, int cg, int cb, int alpha, int width = 1) {
    if (!r) return;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // 外层柔光：用低 alpha 的多条偏移线模拟，比单纯加粗更不刺眼。
    for (int pass = 2; pass >= 0; --pass) {
        int a = alpha / (pass == 2 ? 5 : (pass == 1 ? 2 : 1));
        int off = pass * width;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)clampI(a, 0, 255));
        SDL_RenderDrawLine(r, x0, y0 + off, x1, y1 + off);
        if (off) SDL_RenderDrawLine(r, x0, y0 - off, x1, y1 - off);
    }
    SDL_SetRenderDrawBlendMode(r, prev);
}

static inline void starGlint(SDL_Renderer* r, int x, int y, int radius,
                             int cr, int cg, int cb, int alpha) {
    if (!r || radius < 1) return;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)clampI(alpha / 3, 0, 255));
    SDL_RenderDrawLine(r, x - radius * 2, y, x + radius * 2, y);
    SDL_RenderDrawLine(r, x, y - radius * 2, x, y + radius * 2);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)clampI(alpha, 0, 255));
    SDL_RenderDrawLine(r, x - radius, y, x + radius, y);
    SDL_RenderDrawLine(r, x, y - radius, x, y + radius);
    SDL_RenderDrawPoint(r, x, y);
    SDL_SetRenderDrawBlendMode(r, prev);
}

// 带切角的深色信息面板。只作为 HUD/菜单底层，不改变原本文字坐标。
static inline void techPanel(SDL_Renderer* r, int x, int y, int w, int h,
                             int cr, int cg, int cb, int accentR, int accentG, int accentB) {
    if (!r || w <= 2 || h <= 2) return;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, 168);
    SDL_Rect body = {x, y, w, h}; SDL_RenderFillRect(r, &body);
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB, 150);
    SDL_RenderDrawLine(r, x + 4, y, x + w - 5, y);
    SDL_RenderDrawLine(r, x + 4, y + h - 1, x + w - 5, y + h - 1);
    SDL_RenderDrawLine(r, x, y + 4, x, y + h - 5);
    SDL_RenderDrawLine(r, x + w - 1, y + 4, x + w - 1, y + h - 5);
    int tick = std::min(14, std::min(w / 3, h / 2));
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB, 230);
    SDL_RenderDrawLine(r, x, y, x + tick, y);
    SDL_RenderDrawLine(r, x + w - tick, y + h - 1, x + w - 1, y + h - 1);
    SDL_SetRenderDrawBlendMode(r, prev);
}

static inline void bracket(SDL_Renderer* r, int x, int y, int w, int h, int len,
                           int cr, int cg, int cb, int alpha) {
    if (!r) return;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)alpha);
    SDL_RenderDrawLine(r, x, y, x + len, y);
    SDL_RenderDrawLine(r, x, y, x, y + len);
    SDL_RenderDrawLine(r, x + w - len, y, x + w, y);
    SDL_RenderDrawLine(r, x + w, y, x + w, y + len);
    SDL_RenderDrawLine(r, x, y + h - len, x, y + h);
    SDL_RenderDrawLine(r, x, y + h, x + len, y + h);
    SDL_RenderDrawLine(r, x + w - len, y + h, x + w, y + h);
    SDL_RenderDrawLine(r, x + w, y + h - len, x + w, y + h);
    SDL_SetRenderDrawBlendMode(r, prev);
}

} // namespace ArtFX
