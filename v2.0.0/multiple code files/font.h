#pragma once

#include "renderer.h"
#include "types.h"



// ============== Font 类 ==============
class Font {
    FontChar chars[128];
public:
    Font() {
        auto setChar = [this](char c, const char* bits) {
            for (int r = 0; r < 7; ++r)
                chars[(int)c].rows[r] = (unsigned char)bits[r];
        };
        setChar('0', "\x0E\x11\x13\x15\x19\x11\x0E");
        setChar('1', "\x04\x0C\x04\x04\x04\x04\x0E");
        setChar('2', "\x0E\x11\x01\x06\x08\x10\x1F");
        setChar('3', "\x0E\x11\x01\x0E\x01\x11\x0E");
        setChar('4', "\x02\x06\x0A\x12\x1F\x02\x02");
        setChar('5', "\x1F\x10\x1E\x01\x01\x11\x0E");
        setChar('6', "\x0E\x10\x10\x1E\x11\x11\x0E");
        setChar('7', "\x1F\x01\x02\x04\x04\x08\x08");
        setChar('8', "\x0E\x11\x11\x0E\x11\x11\x0E");
        setChar('9', "\x0E\x11\x11\x0F\x01\x11\x0E");
        setChar('A', "\x0E\x11\x11\x1F\x11\x11\x11");
        setChar('B', "\x1E\x11\x11\x1E\x11\x11\x1E");
        setChar('C', "\x0E\x11\x10\x10\x10\x11\x0E");
        setChar('D', "\x1E\x11\x11\x11\x11\x11\x1E");
        setChar('E', "\x1F\x10\x10\x1E\x10\x10\x1F");
        setChar('F', "\x1F\x10\x10\x1E\x10\x10\x10");
        setChar('G', "\x0E\x11\x10\x17\x11\x11\x0E");
        setChar('H', "\x11\x11\x11\x1F\x11\x11\x11");
        setChar('I', "\x0E\x04\x04\x04\x04\x04\x0E");
        setChar('J', "\x07\x02\x02\x02\x02\x12\x0C");
        setChar('K', "\x11\x12\x14\x18\x14\x12\x11");
        setChar('L', "\x10\x10\x10\x10\x10\x10\x1F");
        setChar('M', "\x11\x1B\x15\x15\x11\x11\x11");
        setChar('N', "\x11\x19\x15\x13\x11\x11\x11");
        setChar('O', "\x0E\x11\x11\x11\x11\x11\x0E");
        setChar('P', "\x1E\x11\x11\x1E\x10\x10\x10");
        setChar('Q', "\x0E\x11\x11\x11\x15\x12\x0D");
        setChar('R', "\x1E\x11\x11\x1E\x14\x12\x11");
        setChar('S', "\x0E\x11\x10\x0E\x01\x11\x0E");
        setChar('T', "\x1F\x04\x04\x04\x04\x04\x04");
        setChar('U', "\x11\x11\x11\x11\x11\x11\x0E");
        setChar('V', "\x11\x11\x11\x11\x0A\x0A\x04");
        setChar('W', "\x11\x11\x11\x15\x15\x1B\x11");
        setChar('X', "\x11\x11\x0A\x04\x0A\x11\x11");
        setChar('Y', "\x11\x11\x0A\x04\x04\x04\x04");
        setChar('Z', "\x1F\x01\x02\x04\x08\x10\x1F");
        setChar('a', "\x00\x00\x0E\x01\x0F\x11\x0F");
        setChar('b', "\x10\x10\x1E\x11\x11\x11\x0E");
        setChar('c', "\x00\x00\x0E\x10\x10\x11\x0E");
        setChar('d', "\x01\x01\x0F\x11\x11\x11\x0F");
        setChar('e', "\x00\x00\x0E\x11\x1F\x10\x0E");
        setChar('f', "\x02\x04\x0E\x04\x04\x04\x0E");
        setChar('g', "\x00\x0F\x11\x0F\x01\x19\x0F");
        setChar('h', "\x10\x10\x16\x19\x11\x11\x11");
        setChar('i', "\x04\x00\x0C\x04\x04\x04\x0E");
        setChar('j', "\x02\x00\x06\x02\x02\x12\x0C");
        setChar('k', "\x10\x10\x12\x14\x18\x14\x12");
        setChar('l', "\x0C\x04\x04\x04\x04\x04\x0E");
        setChar('m', "\x00\x00\x1B\x15\x15\x11\x11");
        setChar('n', "\x00\x00\x1E\x11\x11\x11\x11");
        setChar('o', "\x00\x00\x0E\x11\x11\x11\x0E");
        setChar('p', "\x00\x00\x1E\x11\x11\x1E\x10");
        setChar('q', "\x00\x00\x0F\x11\x11\x0F\x01");
        setChar('r', "\x00\x00\x16\x19\x10\x10\x10");
        setChar('s', "\x00\x00\x0E\x10\x0E\x01\x1E");
        setChar('t', "\x08\x08\x1C\x08\x08\x09\x06");
        setChar('u', "\x00\x00\x11\x11\x11\x13\x0D");
        setChar('v', "\x00\x00\x11\x11\x0A\x0A\x04");
        setChar('w', "\x00\x00\x11\x15\x15\x1B\x11");
        setChar('x', "\x00\x00\x11\x0A\x04\x0A\x11");
        setChar('y', "\x00\x00\x11\x11\x0F\x01\x0E");
        setChar('z', "\x00\x00\x1F\x02\x04\x08\x1F");
        setChar(' ', "\x00\x00\x00\x00\x00\x00\x00");
        setChar('!', "\x04\x04\x04\x04\x00\x00\x04");
        setChar('"', "\x0A\x0A\x00\x00\x00\x00\x00");
        setChar('#', "\x0A\x0A\x1F\x0A\x1F\x0A\x0A");
        setChar('$', "\x04\x1E\x14\x0E\x05\x1E\x04");
        setChar('%', "\x11\x01\x02\x04\x08\x10\x11");
        setChar('&', "\x0C\x12\x14\x0C\x15\x12\x0D");
        setChar('\'', "\x04\x04\x00\x00\x00\x00\x00");
        setChar('(', "\x04\x08\x08\x08\x08\x08\x04");
        setChar(')', "\x04\x02\x02\x02\x02\x02\x04");
        setChar('*', "\x00\x0A\x1F\x1F\x0E\x04\x00");
        setChar('+', "\x00\x00\x04\x0E\x04\x00\x00");
        setChar(',', "\x00\x00\x00\x00\x00\x06\x0C");
        setChar('-', "\x00\x00\x00\x1F\x00\x00\x00");
        setChar('.', "\x00\x00\x00\x00\x00\x00\x04");
        setChar('/', "\x01\x02\x02\x04\x08\x08\x10");
        setChar(':', "\x00\x00\x04\x00\x00\x04\x00");
        setChar(';', "\x04\x00\x00\x00\x00\x06\x0C");
        setChar('<', "\x00\x02\x04\x08\x04\x02\x00");
        setChar('=', "\x00\x1F\x00\x1F\x00\x00\x00");
        setChar('>', "\x00\x08\x04\x02\x04\x08\x00");
        setChar('?', "\x0E\x11\x01\x02\x04\x00\x04");
        setChar('@', "\x0E\x11\x17\x15\x16\x10\x0E");
        setChar('[', "\x0E\x08\x08\x08\x08\x08\x0E");
        setChar('\\',"\x10\x08\x04\x02\x01\x00\x00");
        setChar(']', "\x0E\x02\x02\x02\x02\x02\x0E");
        setChar('^', "\x04\x0A\x11\x00\x00\x00\x00");
        setChar('_', "\x00\x00\x00\x00\x00\x00\x1F");
        setChar('`', "\x08\x04\x00\x00\x00\x00\x00");
        setChar('{', "\x03\x04\x04\x0C\x04\x04\x03");
        setChar('|', "\x04\x04\x04\x04\x04\x04\x04");
        setChar('}', "\x18\x04\x04\x06\x04\x04\x18");
        setChar('~', "\x00\x0A\x15\x00\x00\x00\x00");
    }

    void drawChar(SDL_Renderer* renderer, char c, int x, int y, int scale, int mul=1) const {
        const FontChar& fc = chars[(int)c];
        int s = scale * mul;
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = fc.rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_Rect r2 = {x + col * s, y + row * s, s, s};
                    SDL_RenderFillRect(renderer, &r2);
                }
            }
        }
    }

    void drawString(SDL_Renderer* renderer, const char* str, int x, int y, int scale, int mul=1) const {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int cx = x;
        int step = 6 * scale * mul;
        for (const char* p = str; *p; ++p) {
            if (*p == ' ') { cx += step; continue; }
            drawChar(renderer, *p, cx, y, scale, mul);
            cx += step;
        }
    }

    // 保留 drawString 的白色默认行为，另提供可指定颜色的版本，供标题/HUD/
    // 菜单强调色使用。逐字符调用 drawChar，避免改动现有调用者的默认颜色。
    void drawStringColor(SDL_Renderer* renderer, const char* str, int x, int y,
                         int scale, int r, int g, int b, int a = 255, int mul = 1) const {
        SDL_SetRenderDrawColor(renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        int cx = x;
        int step = 6 * scale * mul;
        for (const char* p = str; *p; ++p) {
            if (*p == ' ') { cx += step; continue; }
            drawChar(renderer, *p, cx, y, scale, mul);
            cx += step;
        }
    }

    void drawCharFloat(SDL_Renderer* r, char c, float x, float y, float scale) const {
        const FontChar& fc = chars[(int)c];
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = fc.rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_FRect fr = {x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRectF(r, &fr);
                }
            }
        }
    }

    const FontChar& getChar(char c) const { return chars[(int)c]; }
};
