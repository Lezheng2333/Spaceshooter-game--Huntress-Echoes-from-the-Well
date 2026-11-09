#pragma once

#include <SDL.h>
#include <cmath>
#include <vector>

// ============== Renderer ==============
// 薄封装之外还负责统一的"CRT 终端"后处理。overlay 是启动时生成一次的静态
// texture：每三行一条极淡扫描线 + 四角暗角。它只影响观感，不参与任何游戏
// 状态，也不改变任何绘制坐标，因此不会影响碰撞、HUD 对齐或存档帧一致性。
class Renderer {
    SDL_Renderer* sdlRenderer;
    SDL_Texture* crtOverlay;
    bool crtEnabled;

    void buildCRTOverlay() {
        if (crtOverlay) { SDL_DestroyTexture(crtOverlay); crtOverlay = nullptr; }
        if (!sdlRenderer) return;
        crtOverlay = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STATIC, 800, 600);
        if (!crtOverlay) return;
        std::vector<Uint8> px((size_t)800 * 600 * 4, 0);
        const float cx = 400.0f, cy = 300.0f;
        for (int y = 0; y < 600; ++y) {
            for (int x = 0; x < 800; ++x) {
                // 低强度扫描线：只压暗每三行中的一行，保留 HUD 与弹幕可读性。
                int rowAlpha = (y % 3 == 2) ? 20 : 0;
                float dx = (x - cx) / cx;
                float dy = (y - cy) / cy;
                float d = std::sqrt(dx * dx * 0.88f + dy * dy);
                float v = (d - 0.58f) / 0.56f;
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                v = v * v * (3.0f - 2.0f * v);
                int alpha = rowAlpha + (int)(v * 84.0f);
                if (alpha > 100) alpha = 100;
                size_t i = ((size_t)y * 800 + x) * 4;
                // ARGB8888 小端内存序为 B,G,R,A；RGB 全 0，只写 alpha。
                px[i + 3] = (Uint8)alpha;
            }
        }
        SDL_UpdateTexture(crtOverlay, nullptr, px.data(), 800 * 4);
        SDL_SetTextureBlendMode(crtOverlay, SDL_BLENDMODE_BLEND);
    }

public:
    Renderer(SDL_Window* window) : sdlRenderer(nullptr), crtOverlay(nullptr), crtEnabled(true) {
        sdlRenderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdlRenderer)
            sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (sdlRenderer) SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        buildCRTOverlay();
    }
    ~Renderer() {
        if (crtOverlay) SDL_DestroyTexture(crtOverlay);
        if (sdlRenderer) SDL_DestroyRenderer(sdlRenderer);
    }
    SDL_Renderer* get() const { return sdlRenderer; }

    void setColor(int r, int g, int b, int a = 255) {
        SDL_SetRenderDrawColor(sdlRenderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    }
    void clear() { SDL_RenderClear(sdlRenderer); }
    void present() { SDL_RenderPresent(sdlRenderer); }
    void drawPoint(int x, int y) { SDL_RenderDrawPoint(sdlRenderer, x, y); }
    void drawLine(int x1, int y1, int x2, int y2) {
        SDL_RenderDrawLine(sdlRenderer, x1, y1, x2, y2);
    }
    void drawRect(const SDL_Rect* r) { SDL_RenderFillRect(sdlRenderer, r); }
    void drawRectBorder(const SDL_Rect* r) { SDL_RenderDrawRect(sdlRenderer, r); }
    void setTarget(SDL_Texture* tex) { SDL_SetRenderTarget(sdlRenderer, tex); }
    void resetTarget() { SDL_SetRenderTarget(sdlRenderer, NULL); }
    SDL_Texture* createTexture(Uint32 fmt, int access, int w, int h) {
        return SDL_CreateTexture(sdlRenderer, fmt, access, w, h);
    }
    void copyTexture(SDL_Texture* tex, const SDL_Rect* src, const SDL_Rect* dst) {
        SDL_RenderCopy(sdlRenderer, tex, src, dst);
    }
    void destroyTexture(SDL_Texture* tex) { SDL_DestroyTexture(tex); }

    // 统一后处理。放在所有界面绘制之后、ReadPixels/Present 之前调用；
    // 截图与游戏内呈现因此会得到完全相同的最终画面。
    void composite() {
        if (!crtEnabled || !crtOverlay || !sdlRenderer) return;
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(sdlRenderer, &prev);
        SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(sdlRenderer, crtOverlay, NULL, NULL);
        SDL_SetRenderDrawBlendMode(sdlRenderer, prev);
    }
    void toggleCRT() { crtEnabled = !crtEnabled; }
    bool isCRTEnabled() const { return crtEnabled; }
};
