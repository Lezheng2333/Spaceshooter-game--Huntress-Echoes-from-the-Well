#pragma once

#include "../art_fx.h"
#include "../font.h"
#include "../renderer.h"

// ============== HUDBase (shared HUD drawing) ==============
class HUDBase {
public:
    // 统一的状态区底板。文字坐标仍沿用旧版 rightEdge 对齐，避免任何 HUD 位移。
    static void drawStatusPanel(SDL_Renderer* r, int rightEdge, int y, int h) {
        int x = rightEdge - 162;
        int w = 162;
        ArtFX::techPanel(r, x, y, w, h, 4, 10, 11, 54, 168, 105);
        // 右侧的竖排信号灯，为右上角增加仪器感。
        for (int i = 0; i < 3; ++i) {
            int gy = y + 8 + i * 7;
            SDL_SetRenderDrawColor(r, i == 0 ? 82 : 38, i == 0 ? 210 : 70,
                                   i == 0 ? 128 : 74, i == 0 ? 210 : 110);
            SDL_Rect rr = {x + 5, gy, 2, 3};
            SDL_RenderFillRect(r, &rr);
        }
    }

    // rightEdge = rightmost pixel of the HUD column (all elements aligned)
    static void drawScore(SDL_Renderer* r, const Font& font, int score, int rightEdge, int y) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE:%-4d", score);
        font.drawStringColor(r, buf, rightEdge - 120, y, 2, 224, 241, 232, 255);
        // 10 chars * 12px = 120px
        SDL_SetRenderDrawColor(r, 67, 206, 125, 210);
        SDL_Rect accent = {rightEdge - 126, y + 2, 2, 11};
        SDL_RenderFillRect(r, &accent);
    }

    static void drawHPHearts(SDL_Renderer* r, const Font& font, int hp, int maxHp, int rightEdge, int y) {
        (void)font;
        if (maxHp > 3) {
            // 第一章基地完整度：绿色分段条，比红色星号更接近"基地护盾"。
            const int segW = 9, segH = 5, gap = 4;
            for (int i = 0; i < maxHp; ++i) {
                int hx = rightEdge - 10 - i * (segW + gap);
                SDL_SetRenderDrawColor(r, i < hp ? 46 : 28, i < hp ? 209 : 52,
                                       i < hp ? 116 : 48, 255);
                SDL_Rect rr = {hx - segW, y, segW, segH};
                SDL_RenderFillRect(r, &rr);
                if (i < hp) {
                    SDL_SetRenderDrawColor(r, 150, 255, 190, 42);
                    SDL_Rect glow = {hx - segW - 1, y - 1, segW + 2, segH + 2};
                    SDL_RenderDrawRect(r, &glow);
                }
            }
        } else {
            // 第二章玩家生命：红色菱形，与右侧状态板形成清晰的危险色。
            for (int i = 0; i < maxHp; ++i) {
                int hx = rightEdge - 11 - i * 14;
                SDL_Point p[4] = {{hx, y}, {hx + 5, y + 4}, {hx, y + 8}, {hx - 5, y + 4}};
                ArtFX::fillPoly(r, p, 4, i < hp ? 255 : 52, i < hp ? 48 : 32,
                                i < hp ? 54 : 38, 255);
                SDL_SetRenderDrawColor(r, i < hp ? 255 : 80, i < hp ? 130 : 62,
                                       i < hp ? 120 : 62, 210);
                SDL_RenderDrawLine(r, p[0].x, p[0].y, p[1].x, p[1].y);
                SDL_RenderDrawLine(r, p[1].x, p[1].y, p[2].x, p[2].y);
                SDL_RenderDrawLine(r, p[2].x, p[2].y, p[3].x, p[3].y);
                SDL_RenderDrawLine(r, p[3].x, p[3].y, p[0].x, p[0].y);
            }
        }
    }

    static void drawEnergyBar(SDL_Renderer* r, int rightEdge, int y, int w, int h, float fill, bool breathing = false) {
        int x = rightEdge - w;
        SDL_SetRenderDrawColor(r, 8, 22, 18, 235);
        SDL_Rect bg = {x - 1, y - 1, w + 2, h + 2}; SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 33, 78, 51, 220);
        SDL_Rect border = {x - 2, y - 2, w + 4, h + 4}; SDL_RenderDrawRect(r, &border);
        int fw = (int)(fill * w); if (fw > w) fw = w; if (fw < 0) fw = 0;
        if (fw > 0) {
            int green;
            if (breathing) {
                // 不使用 wall-clock：满能量条保持稳定的高亮，避免读档帧像素漂移。
                green = 242;
            } else {
                green = (fill > 0.9f) ? 255 : (fill > 0.3f ? 198 : 124);
            }
            // 两段亮度渐变：底部深绿，顶部带荧光亮边。
            SDL_SetRenderDrawColor(r, 28, (Uint8)(green * 2 / 3), 54, 255);
            SDL_Rect fr = {x, y, fw, h}; SDL_RenderFillRect(r, &fr);
            SDL_SetRenderDrawColor(r, 128, (Uint8)green, 154, 200);
            SDL_Rect hi = {x, y, fw, 1}; SDL_RenderFillRect(r, &hi);
            SDL_SetRenderDrawColor(r, 150, 255, 188, breathing ? 220 : 130);
            SDL_Rect tip = {x + fw - 3, y - 1, 3, h + 2}; SDL_RenderFillRect(r, &tip);
        }
        // 10 格刻度。与旧版 10*14 宽的能量条完全兼容。
        SDL_SetRenderDrawColor(r, 38, 72, 49, 180);
        for (int i = 1; i < 10; ++i) {
            int sx = x + (int)((double)i * w / 10.0);
            SDL_RenderDrawLine(r, sx, y, sx, y + h - 1);
        }
    }

    // NightElf white energy bar (drawn under the green pulse bar)
    // mode: 0 = charging (white), 1 = decaying (dim), 2 = triple fire (pulsing cyan)
    static void drawEnergyBarWhite(SDL_Renderer* r, int rightEdge, int y, int w, int h, float fill, int mode) {
        int x = rightEdge - w;
        SDL_SetRenderDrawColor(r, 10, 17, 25, 235);
        SDL_Rect bg = {x - 1, y - 1, w + 2, h + 2}; SDL_RenderFillRect(r, &bg);
        int fw = (int)(fill * w); if (fw > w) fw = w; if (fw < 0) fw = 0;
        if (fw > 0) {
            int R, G, B;
            if (mode == 2) {
                R = 236; G = 246; B = 255;
            } else if (mode == 0) {
                R = 220; G = 230; B = 246;
            } else {
                R = 116; G = 128; B = 148;
            }
            SDL_SetRenderDrawColor(r, (Uint8)R, (Uint8)G, (Uint8)B, 255);
            SDL_Rect fr = {x, y, fw, h}; SDL_RenderFillRect(r, &fr);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 130);
            SDL_Rect hi = {x, y, fw, 1}; SDL_RenderFillRect(r, &hi);
        }
        SDL_SetRenderDrawColor(r, 67, 89, 111, 160);
        for (int i = 1; i < 10; ++i) {
            int sx = x + (int)((double)i * w / 10.0);
            SDL_RenderDrawLine(r, sx, y, sx, y + h - 1);
        }
    }

    // Generic boss HP bar (name rendered left of the bar, scale 2)
    static void drawBossBar(SDL_Renderer* r, const Font& font, const char* name,
                            int hp, int maxHp, int x, int y, int w, int h) {
        // Boss 名称放在血条左上，配合一条红色警示线；不改变血条 x/y。
        SDL_SetRenderDrawColor(r, 12, 7, 9, 225);
        SDL_Rect bg = {x - 2, y - 2, w + 4, h + 4}; SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 58, 15, 19, 220);
        SDL_Rect border = {x - 1, y - 1, w + 2, h + 2}; SDL_RenderDrawRect(r, &border);
        int fw = (int)((double)hp / (maxHp > 0 ? maxHp : 1) * w);
        if (fw > w) fw = w; if (fw < 0) fw = 0;
        if (fw > 0) {
            SDL_SetRenderDrawColor(r, 188, 24, 33, 255);
            SDL_Rect fr = {x, y, fw, h}; SDL_RenderFillRect(r, &fr);
            SDL_SetRenderDrawColor(r, 255, 88, 88, 190);
            SDL_Rect hi = {x, y, fw, 1}; SDL_RenderFillRect(r, &hi);
            SDL_SetRenderDrawColor(r, 255, 190, 160, 125);
            SDL_Rect tip = {x + fw - 2, y, 2, h}; SDL_RenderFillRect(r, &tip);
            // 血条内部的 10% 刻度，方便读伤害。
            SDL_SetRenderDrawColor(r, 70, 12, 18, 150);
            for (int i = 1; i < 10; ++i) {
                int sx = x + w * i / 10;
                SDL_RenderDrawLine(r, sx, y, sx, y + h - 1);
            }
        }
        if (name && name[0]) {
            int nameLen = (int)strlen(name);
            int nameX = x - nameLen * 12 - 8;
            if (nameX < 4) nameX = 4;
            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), "%s", name);
            font.drawStringColor(r, nameBuf, nameX, y + 1, 2, 230, 202, 194, 255);
            SDL_SetRenderDrawColor(r, 255, 88, 88, 125);
            SDL_RenderDrawLine(r, nameX, y + h + 1, nameX + nameLen * 12 - 4, y + h + 1);
        }
    }
};
