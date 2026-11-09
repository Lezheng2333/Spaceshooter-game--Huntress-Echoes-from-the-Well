#pragma once

#include <SDL.h>
#include <cmath>
#include "art_fx.h"
#include "types.h"
// ============== UIRenderer ==============
class UIRenderer {
public:
    static void drawMenuCursor(SDL_Renderer* r, int x, int y, int size = 0) {
        int s = size > 0 ? size : 10;
        ArtFX::BlendGuard guard(r);
        // 双层箭头 + 小目标刻度：绿色荧光与游戏中的生命色一致。
        SDL_SetRenderDrawColor(r, 82, 255, 150, 210);
        SDL_RenderDrawLine(r, x, y, x + s, y + s/2);
        SDL_RenderDrawLine(r, x, y, x + s, y - s/2);
        SDL_RenderDrawLine(r, x + s, y - s/2, x + s, y + s/2);
        SDL_SetRenderDrawColor(r, 196, 255, 216, 115);
        SDL_RenderDrawLine(r, x + 2, y, x + s + 2, y + s/2);
        SDL_RenderDrawLine(r, x + 2, y, x + s + 2, y - s/2);
        SDL_SetRenderDrawColor(r, 52, 190, 112, 90);
        SDL_RenderDrawLine(r, x + s + 4, y, x + s + 9, y);
    }

    static void drawMenuUnderline(SDL_Renderer* r, int x, int y, int w) {
        ArtFX::BlendGuard guard(r);
        SDL_SetRenderDrawColor(r, 58, 205, 118, 180);
        SDL_RenderDrawLine(r, x, y, x + w, y);
        SDL_SetRenderDrawColor(r, 182, 255, 205, 95);
        SDL_RenderDrawLine(r, x + 3, y - 1, x + w - 3, y - 1);
        SDL_SetRenderDrawColor(r, 35, 132, 78, 130);
        SDL_RenderDrawLine(r, x + 8, y + 1, x + w - 8, y + 1);
    }

    static void drawSlider(SDL_Renderer* r, int x, int y, int w, int val, int lo, int hi, bool sym) {
        ArtFX::BlendGuard guard(r);
        int range = hi - lo;
        if (range <= 0) return;
        int fillW = (val - lo) * w / range;
        // 仪器槽：深底 + 两端刻度 + 16 格细分。
        SDL_SetRenderDrawColor(r, 7, 16, 18, 235);
        SDL_Rect bg = {x, y, w, 12};
        SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 48, 101, 79, 210);
        SDL_RenderDrawRect(r, &bg);
        for (int i = 1; i < 16; ++i) {
            int sx = x + w * i / 16;
            SDL_SetRenderDrawColor(r, 28, 55, 46, 130);
            SDL_RenderDrawLine(r, sx, y + 8, sx, y + 11);
        }
        if (sym) {
            int zeroX = x + w / 2;
            if (fillW >= w / 2) {
                SDL_SetRenderDrawColor(r, 76, 222, 132, 255);
                SDL_Rect fg = {zeroX, y + 2, fillW - w/2, 8}; SDL_RenderFillRect(r, &fg);
            } else {
                SDL_SetRenderDrawColor(r, 238, 112, 92, 255);
                SDL_Rect fg = {zeroX + fillW - w/2, y + 2, w/2 - fillW, 8}; SDL_RenderFillRect(r, &fg);
            }
            SDL_SetRenderDrawColor(r, 220, 245, 232, 220);
            SDL_RenderDrawLine(r, zeroX, y, zeroX, y + 12);
        } else {
            SDL_SetRenderDrawColor(r, 52, 210, 120, 255);
            SDL_Rect fg = {x + 1, y + 2, fillW - 2, 8};
            if (fg.w > 0) SDL_RenderFillRect(r, &fg);
            if (fillW > 3) {
                SDL_SetRenderDrawColor(r, 188, 255, 211, 210);
                SDL_Rect hi = {x + 1, y + 2, fillW - 2, 1};
                SDL_RenderFillRect(r, &hi);
            }
        }
        // 当前值小三角。
        int tx = x + fillW;
        SDL_SetRenderDrawColor(r, 230, 255, 238, 215);
        SDL_RenderDrawLine(r, tx, y - 3, tx - 3, y - 6);
        SDL_RenderDrawLine(r, tx, y - 3, tx + 3, y - 6);
    }

    static void drawHalfTransparentOverlay(SDL_Renderer* r, int alpha) {
        // 先整屏压暗，再叠一层稀疏扫描线。原版只画扫描线，长文本后面仍会
        // 透出高对比场景；整屏暗底可以让暂停/读档界面重新获得层次。
        ArtFX::BlendGuard guard(r);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)(alpha * 3 / 4));
        SDL_Rect full = {0, 0, WIN_WIDTH, WIN_HEIGHT};
        SDL_RenderFillRect(r, &full);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)alpha);
        for (int yy = 0; yy < WIN_HEIGHT; yy += 3) {
            SDL_Rect rect = {0, yy, WIN_WIDTH, 2};
            SDL_RenderFillRect(r, &rect);
        }
    }
};



// ============== MenuStateMachine ==============
class MenuStateMachine {
public:
    enum ScreenType { NONE, START, CHAPTER, TEST, OPTIONS, SOUND, PAUSE, GAMEOVER, MISSION_COMPLETE };
    enum Action { ACT_NONE, ACT_START_GAME, ACT_SELECT_CHAPTER, ACT_BACK_TO_MENU, ACT_QUIT,
                  ACT_RESUME, ACT_RESTART, ACT_TOGGLE_OPTION, ACT_OPTIONS, ACT_SOUND,
                  ACT_BACK_TO_MAIN, ACT_TEST_START, ACT_CHAPTER_START, ACT_EXIT_PAUSE_OPTIONS };

    struct StateSnapshot {
        ScreenType screen;
        int cursor;
        int extraData;
    };

private:
    ScreenType currentScreen;
    int cursor;
    bool justEntered;
    // Edge detection
    bool upWas, downWas, enterWas, escWas, leftWas, rightWas;
    // Extra
    int soundCursor;
    bool optionFromPause;

public:
    MenuStateMachine() : currentScreen(START), cursor(0), justEntered(true),
        upWas(false), downWas(false), enterWas(false), escWas(false),
        leftWas(false), rightWas(false), soundCursor(0), optionFromPause(false) {}

    void enter(ScreenType s) {
        currentScreen = s;
        cursor = 0;
        justEntered = true;
        upWas = downWas = enterWas = escWas = leftWas = rightWas = false;
    }

    void enterSound() { soundCursor = 0; justEntered = true; }

    ScreenType getScreen() const { return currentScreen; }
    int getCursor() const { return cursor; }
    int getSoundCursor() const { return soundCursor; }

    void update(const Uint8* keys) {
        bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        bool escNow   = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
        bool leftNow  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];

        if (justEntered) {
            upWas = upNow; downWas = downNow; enterWas = enterNow;
            escWas = escNow; leftWas = leftNow; rightWas = rightNow;
            justEntered = false;
            return;
        }

        int maxItems = 5;
        switch (currentScreen) {
            case START:    maxItems = 5; break;
            case CHAPTER:  maxItems = 5; break;
            case TEST:     maxItems = 9; break;
            case OPTIONS:  maxItems = 2; break;
            case SOUND:    maxItems = 6; break;
            case PAUSE:    maxItems = 5; break;
            case GAMEOVER: maxItems = 2; break;
            default: return;
        }

        if (upNow && !upWas) {
            if (currentScreen == SOUND && soundCursor > 0) soundCursor--;
            else cursor = (cursor - 1 + maxItems) % maxItems;
        }
        if (downNow && !downWas) {
            if (currentScreen == SOUND && soundCursor < maxItems - 1) soundCursor++;
            else cursor = (cursor + 1) % maxItems;
        }
        upWas = upNow; downWas = downNow; enterWas = enterNow;
        escWas = escNow; leftWas = leftNow; rightWas = rightNow;
    }

    bool isEnterPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        return enterNow && !enterWas;
    }

    bool isEscPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool escNow = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
        return escNow && !escWas;
    }

    bool isLeftPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool leftNow = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        return leftNow && !leftWas;
    }

    bool isRightPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        return rightNow && !rightWas;
    }

    void setOptionFromPause(bool v) { optionFromPause = v; }
    bool getOptionFromPause() const { return optionFromPause; }
};


// ============== Game 类 ==============

struct MenuKeys {
    bool up, down, left, right, enter, esc;
    bool upWas, downWas, enterWas, escWas;
    MenuKeys(const Uint8* keys) : up(false), down(false), left(false), right(false), enter(false), esc(false),
        upWas(false), downWas(false), enterWas(false), escWas(false) {
        up = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        down = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        enter = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER];
        esc = keys[SDL_SCANCODE_ESCAPE];
    }
};

