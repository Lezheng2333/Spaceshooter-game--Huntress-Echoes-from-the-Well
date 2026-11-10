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

// 切角深色玻璃面板：低饱和底色 + 极淡纵向渐变 + 细扫描纹理 +
// 单层发光轮廓 + 内嵌细线。所有装饰都是几何线，不使用装饰性小字。
static inline void techPanel(SDL_Renderer* r, int x, int y, int w, int h,
                             int cr, int cg, int cb, int accentR, int accentG, int accentB,
                             int alpha = 255) {
    if (!r || w <= 4 || h <= 4) return;
    if (alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    int cut = std::min(12, std::min(w, h) / 4);
    if (cut < 3) cut = 3;
    SDL_Point p[8] = {
        {x + cut, y}, {x + w - cut, y}, {x + w, y + cut}, {x + w, y + h - cut},
        {x + w - cut, y + h}, {x + cut, y + h}, {x, y + h - cut}, {x, y + cut}
    };

    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // 1) 半透明熏黑玻璃底。
    fillPoly(r, p, 8, cr, cg, cb, 168 * alpha / 255);

    // 2) 极轻的冷色顶部渐变与 4px 扫描纹理，制造“玻璃内屏”质感。
    for (int yy = 0; yy < h; ++yy) {
        int inset = 0;
        if (yy < cut) inset = cut - yy;
        if (yy >= h - cut) inset = yy - (h - cut) + 1;
        int rx = x + inset;
        int rw = w - inset * 2;
        if (rw <= 0) continue;
        int tint = (int)((1.0f - (float)yy / (float)h) * 26.0f + 4.0f);
        SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB,
                               (Uint8)(tint * alpha / 255));
        SDL_RenderDrawLine(r, rx, y + yy, rx + rw - 1, y + yy);
        if ((yy & 3) == 0) {
            SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(10 * alpha / 255));
            SDL_RenderDrawLine(r, rx, y + yy, rx + rw - 1, y + yy);
        }
    }

    // 3) 轮廓与内侧细线。切角让矩形更像舰载/终端 HUD，而不是普通卡片。
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB,
                           (Uint8)(164 * alpha / 255));
    for (int i = 0; i < 8; ++i)
        SDL_RenderDrawLine(r, p[i].x, p[i].y, p[(i + 1) % 8].x, p[(i + 1) % 8].y);

    int ix = 3, iy = 3;
    int ic = cut > 5 ? cut - 2 : 2;
    SDL_Point inner[8] = {
        {x + ix + ic, y + iy}, {x + w - ix - ic, y + iy},
        {x + w - ix, y + iy + ic}, {x + w - ix, y + h - iy - ic},
        {x + w - ix - ic, y + h - iy}, {x + ix + ic, y + h - iy},
        {x + ix, y + h - iy - ic}, {x + ix, y + iy + ic}
    };
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB,
                           (Uint8)(34 * alpha / 255));
    for (int i = 0; i < 8; ++i)
        SDL_RenderDrawLine(r, inner[i].x, inner[i].y, inner[(i + 1) % 8].x, inner[(i + 1) % 8].y);

    // 4) 顶部一条极细高光，以及左缘一道短光轨，保持克制但强化层次。
    SDL_SetRenderDrawColor(r, (Uint8)std::min(255, accentR + 60),
                           (Uint8)std::min(255, accentG + 60),
                           (Uint8)std::min(255, accentB + 60),
                           (Uint8)(126 * alpha / 255));
    SDL_RenderDrawLine(r, x + cut + 12, y + 1, x + w - cut - 12, y + 1);
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB,
                           (Uint8)(72 * alpha / 255));
    SDL_RenderDrawLine(r, x + 5, y + cut + 10, x + 5, y + h - cut - 10);
    SDL_SetRenderDrawColor(r, (Uint8)accentR, (Uint8)accentG, (Uint8)accentB,
                           (Uint8)(55 * alpha / 255));
    SDL_RenderDrawLine(r, x + cut + 12, y + h - 2, x + w - cut - 12, y + h - 2);

    SDL_SetRenderDrawBlendMode(r, prev);
}

static inline void bracket(SDL_Renderer* r, int x, int y, int w, int h, int len,
                           int cr, int cg, int cb, int alpha) {
    if (!r || w <= 0 || h <= 0 || alpha <= 0) return;
    int L = std::min(len, std::min(w / 2, h / 2));
    if (L < 3) L = 3;
    SDL_BlendMode prev;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)clampI(alpha, 0, 255));
    SDL_RenderDrawLine(r, x, y, x + L, y);
    SDL_RenderDrawLine(r, x, y, x, y + L);
    SDL_RenderDrawLine(r, x + w - L, y, x + w, y);
    SDL_RenderDrawLine(r, x + w, y, x + w, y + L);
    SDL_RenderDrawLine(r, x, y + h - L, x, y + h);
    SDL_RenderDrawLine(r, x, y + h, x + L, y + h);
    SDL_RenderDrawLine(r, x + w - L, y + h, x + w, y + h);
    SDL_RenderDrawLine(r, x + w, y + h - L, x + w, y + h);
    // 内嵌的第二层短角线，做出“双层金属框”的厚度感。
    SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb,
                           (Uint8)(alpha * 42 / 100));
    int o = 3, m = L > 8 ? 5 : 3;
    SDL_RenderDrawLine(r, x + o, y + o, x + o + m, y + o);
    SDL_RenderDrawLine(r, x + o, y + o, x + o, y + o + m);
    SDL_RenderDrawLine(r, x + w - o - m, y + o, x + w - o, y + o);
    SDL_RenderDrawLine(r, x + w - o, y + o, x + w - o, y + o + m);
    SDL_RenderDrawLine(r, x + o, y + h - o - m, x + o, y + h - o);
    SDL_RenderDrawLine(r, x + o, y + h - o, x + o + m, y + h - o);
    SDL_RenderDrawLine(r, x + w - o - m, y + h - o, x + w - o, y + h - o);
    SDL_RenderDrawLine(r, x + w - o, y + h - o - m, x + w - o, y + h - o);
    SDL_SetRenderDrawBlendMode(r, prev);
}

} // namespace ArtFX
