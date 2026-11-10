#pragma once
// ============== DemoStrokeFont ==============
// 从 First Flight demo 吸收的程序化 stroke font（6x10 字形框、固定字距）。
// 用于章节旁白与角色对白正文；角色名仍使用原有 5x7 像素字体。
// 该实现不读取外部字体文件，也不修改任何游戏状态。
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace demo_font_detail {

struct Vec2f { float x, y; };
typedef std::vector<Vec2f> Stroke;
typedef std::vector<Stroke> Glyph;

static const char* const GLYPH_SPECS[95] = {
    /* 32  ' '  */ "",
    /* 33  '!'  */ "3,1 3,5.5|3,7.0 3,8.4",
    /* 34  '"'  */ "1.8,1 1.6,2.4|3.8,1 3.6,2.4",
    /* 35  '#'  */ "1.8,2.5 1.8,7|4.2,2.5 4.2,7|0.8,4 5.2,4|0.8,5.7 5.2,5.7",
    /* 36  '$'  */ "5.6,2 4.2,1 2,1 0.6,2.4 2,4.4 4.2,4.6 5.6,6 4.2,8 2,8 0.6,7|3,0.4 3,8.6",
    /* 37  '%'  */ "5,1 1,8.6|1.4,1.4 2.1,2.1 1.4,2.8 0.7,2.1 1.4,1.4|4.6,6.2 5.3,6.9 4.6,7.6 3.9,6.9 4.6,6.2",
    /* 38  '&'  */ "5,8 1.8,4 1.8,2.8 2.6,1.8 3.6,2.2 3.4,3.4 1.2,5.2 1.2,6.8 2.2,8 3.8,7.6 5,6",
    /* 39  '\'' */ "3,1 2.8,2.4",
    /* 40  '('  */ "4.2,0.8 2.4,2.6 1.8,5 2.4,7.4 4.2,9.2",
    /* 41  ')'  */ "1.8,0.8 3.6,2.6 4.2,5 3.6,7.4 1.8,9.2",
    /* 42  '*'  */ "3,2 3,6.5|1.2,3.2 4.8,5.4|4.8,3.2 1.2,5.4",
    /* 43  '+'  */ "1.2,4.6 5,4.6|3.1,2.7 3.1,6.5",
    /* 44  ','  */ "3,7.2 3,8|3,8 2.4,9.2",
    /* 45  '-'  */ "1.2,4.6 5,4.6",
    /* 46  '.'  */ "3,7.2 3,8",
    /* 47  '/'  */ "5.2,1 1,8.6",
    /* 48  '0'  */ "1.8,1 4.2,1 5.6,2.6 5.6,6.4 4.2,8 1.8,8 0.4,6.4 0.4,2.6 1.8,1|1.8,7 4.2,2",
    /* 49  '1'  */ "1.4,2.6 3,1 3,8|1.2,8 4.8,8",
    /* 50  '2'  */ "0.8,2 2.2,1 3.8,1 5.2,2.4 5.2,3.6 0.6,8 5.6,8",
    /* 51  '3'  */ "0.8,1.2 2,1 4.2,1 5.2,2.2 5.2,3.2 3.6,4.4 2,4.4|3.6,4.4 5.2,5.6 5.2,6.8 4,8 2,8 0.8,7.4",
    /* 52  '4'  */ "4.2,8 4.2,1 0.4,6 5.6,6",
    /* 53  '5'  */ "5.2,1 1,1 0.8,4.2 3.8,4.2 5.2,5.4 5.2,6.8 4,8 2,8 0.8,7.4",
    /* 54  '6'  */ "4.8,1.2 3.2,1 1.8,2.2 1,4.4 1,6.4 2.4,8 3.8,8 5.2,6.8 5.2,5.6 3.8,4.4 2,4.4 1,5.4",
    /* 55  '7'  */ "0.8,1 5.4,1 2.8,8",
    /* 56  '8'  */ "2,1 4,1 5,2.2 5,3.2 3,4.4 1.2,5.4 1.2,6.8 2.2,8 3.8,8 5,6.8 5,5.4 3,4.4 1,3.2 1,2.2 2,1",
    /* 57  '9'  */ "1.2,2.2 2.2,1 3.8,1 4.8,2.2 4.8,3.6 3.8,4.8 1.8,4.8 1.2,3.6 1.2,2.2|4.8,4.8 4.8,8",
    /* 58  ':'  */ "3,4.2 3,5|3,7.2 3,8",
    /* 59  ';'  */ "3,4.2 3,5|3,7.2 3,8|3,8 2.4,9.2",
    /* 60  '<'  */ "5,2 1.2,5 5,8",
    /* 61  '='  */ "1.2,3.5 5,3.5|1.2,5.6 5,5.6",
    /* 62  '>'  */ "1,2 4.8,5 1,8",
    /* 63  '?'  */ "1.2,2 2.2,1 3.8,1 5,2 5,3 3,4.4 3,5.4|3,7.2 3,8",
    /* 64  '@'  */ "5,5.2 4.6,4.2 3.4,4.2 2.4,5 2.4,6 3.2,7 4.4,6.8|4.8,4.2 4.8,6.8 5.6,7.2|5.6,7.2 5.6,3 4.4,1.6 2.6,1.4 1.2,2.6 1,4.6 1.4,6.8 2.6,8 4.6,8.2 6,7.4",
    /* 65  'A'  */ "0,8 3,1 6,8|1.2,5.2 4.9,5.2",
    /* 66  'B'  */ "0,8 0,1 4,1 5.4,2.2 5.4,3.4 4,4.6 0,4.6|4,4.6 5.6,5.8 5.6,7 4.2,8 0,8",
    /* 67  'C'  */ "5.8,2.2 4.4,1 2.2,1 0.6,2.8 0.6,6.2 2.2,8 4.4,8 5.8,6.8",
    /* 68  'D'  */ "0,8 0,1 3.6,1 5.8,2.8 5.8,6.2 3.6,8 0,8",
    /* 69  'E'  */ "5.8,1 0,1 0,8 5.8,8|0,4.6 4.4,4.6",
    /* 70  'F'  */ "5.8,1 0,1 0,8|0,4.6 4.4,4.6",
    /* 71  'G'  */ "5.8,2.2 4.4,1 2.2,1 0.6,2.8 0.6,6.2 2.2,8 4.4,8 5.8,6.6 5.8,4.8 3.4,4.8",
    /* 72  'H'  */ "0,1 0,8|5.8,1 5.8,8|0,4.6 5.8,4.6",
    /* 73  'I'  */ "1.2,1 4.8,1|3,1 3,8|1.2,8 4.8,8",
    /* 74  'J'  */ "5,1 5,6.4 3.4,8 2,8 0.8,7.2",
    /* 75  'K'  */ "0,1 0,8|5.6,1 0,5.4|1.8,4.2 5.8,8",
    /* 76  'L'  */ "0,1 0,8 5.8,8",
    /* 77  'M'  */ "0,8 0,1 3,5.2 6,1 6,8",
    /* 78  'N'  */ "0,8 0,1 6,8 6,1",
    /* 79  'O'  */ "2,1 4,1 5.8,2.8 5.8,6.2 4,8 2,8 0.2,6.2 0.2,2.8 2,1",
    /* 80  'P'  */ "0,8 0,1 4.2,1 5.6,2.4 5.6,3.8 4.2,5.2 0,5.2",
    /* 81  'Q'  */ "2,1 4,1 5.8,2.8 5.8,6.2 4,8 2,8 0.2,6.2 0.2,2.8 2,1|3.6,6.2 6,8.6",
    /* 82  'R'  */ "0,8 0,1 4.2,1 5.6,2.4 5.6,3.8 4.2,5.2 0,5.2|3,5.2 5.8,8",
    /* 83  'S'  */ "5.8,2 4.4,1 2,1 0.4,2.4 0.4,3.4 2,4.6 4.2,4.6 5.8,5.8 5.8,6.8 4.2,8 2,8 0.4,7",
    /* 84  'T'  */ "0,1 6,1|3,1 3,8",
    /* 85  'U'  */ "0,1 0,6 1.8,8 4,8 5.8,6 5.8,1",
    /* 86  'V'  */ "0,1 3,8 6,1",
    /* 87  'W'  */ "0.2,1 1.4,8 3,3.8 4.6,8 5.8,1",
    /* 88  'X'  */ "0,1 5.8,8|5.8,1 0,8",
    /* 89  'Y'  */ "0,1 3,4.6 6,1|3,4.6 3,8",
    /* 90  'Z'  */ "0,1 6,1 0,8 6,8",
    /* 91  '['  */ "4.2,0.6 2,0.6 2,9.6 4.2,9.6",
    /* 92  '\\' */ "1,1 5.2,8.6",
    /* 93  ']'  */ "1.8,0.6 4,0.6 4,9.6 1.8,9.6",
    /* 94  '^'  */ "1.2,2.6 3,1 4.8,2.6",
    /* 95  '_'  */ "0.6,9.4 5.4,9.4",
    /* 96  '`'  */ "2,1 3,2.2",
    /* 97  'a'  */ "5,4 2,4 0.5,5.4 0.5,6.6 2,8 5,8|5,4 5,8",
    /* 98  'b'  */ "0,1 0,8|0,6.4 2,4.2 3.8,4.2 5.2,5.6 5.2,6.4 3.8,8 2,8 0,6.4",
    /* 99  'c'  */ "5.2,4.8 4,4 2,4 0.5,5.5 0.5,6.5 2,8 4,8 5.2,7.2",
    /* 100 'd'  */ "5.2,1 5.2,8|5.2,6.4 3.8,4.2 2,4.2 0.5,5.6 0.5,6.4 2,8 3.8,8 5.2,6.4",
    /* 101 'e'  */ "0.5,6 5,6 5,5 3.8,4 2,4 0.5,5.4 0.5,6.6 2,8 3.8,8 5,7.4",
    /* 102 'f'  */ "4.8,1.4 3.8,1 2.6,1.6 2.6,8|0.8,4.2 4.4,4.2",
    /* 103 'g'  */ "5.2,4 5.2,9.2 4.2,10 2.2,10 1.2,9.4|5.2,6.2 3.8,4.2 2,4.2 0.5,5.6 0.5,6.4 2,8 3.8,8 5.2,6.6",
    /* 104 'h'  */ "0,1 0,8|0,5.6 1.8,4 3.4,4 5,5.4 5,8",
    /* 105 'i'  */ "2.6,4 2.6,8|2.6,2.2 2.6,2.9",
    /* 106 'j'  */ "3.4,4 3.4,9.2 2.4,10 1.2,9.6|3.4,2.2 3.4,2.9",
    /* 107 'k'  */ "0,1 0,8|4.8,4 0,6.2|1.8,5.4 5,8",
    /* 108 'l'  */ "1.6,1 1.6,7 2.8,8",
    /* 109 'm'  */ "0,8 0,4|0,5 1.2,4 2.2,4 3,5 3,8|3,5 4.2,4 5.2,4 6,5 6,8",
    /* 110 'n'  */ "0,8 0,4|0,5.4 1.8,4 3.4,4 5,5.4 5,8",
    /* 111 'o'  */ "2,4 3.8,4 5.2,5.4 5.2,6.6 3.8,8 2,8 0.6,6.6 0.6,5.4 2,4",
    /* 112 'p'  */ "0,4 0,10|0,5.4 1.8,4 3.4,4 5,5.4 5,6.6 3.4,8 1.8,8 0,6.6",
    /* 113 'q'  */ "5,4 5,10|5,5.4 3.4,4 1.8,4 0.2,5.4 0.2,6.6 1.8,8 3.4,8 5,6.6",
    /* 114 'r'  */ "0,8 0,4|0,5.4 1.8,4 4,4",
    /* 115 's'  */ "5,4.8 3.8,4 2,4 0.6,5 1.2,5.9 3.2,6.2 5,6.8 4.4,7.9 2.8,8 1.2,7.6",
    /* 116 't'  */ "2,2 2,7 3.2,8 4.6,7.6|0.6,4.4 4.2,4.4",
    /* 117 'u'  */ "0,4 0,6.5 1.6,8 3.4,8 5,6.6|5,4 5,8",
    /* 118 'v'  */ "0,4 2.5,8 5,4",
    /* 119 'w'  */ "0,4 1,8 3,5 5,8 6,4",
    /* 120 'x'  */ "0.4,4 5.2,8|5.2,4 0.4,8",
    /* 121 'y'  */ "0.2,4 2.6,8|5.2,4 2.6,8 1.6,10 0.6,10",
    /* 122 'z'  */ "0.4,4 5.2,4 0.4,8 5.2,8",
    /* 123 '{'  */ "4.6,0.6 3.2,1.6 3.2,4 1.8,5 3.2,6 3.2,8.4 4.6,9.4",
    /* 124 '|'  */ "3,0.4 3,9.6",
    /* 125 '}'  */ "1.4,0.6 2.8,1.6 2.8,4 4.2,5 2.8,6 2.8,8.4 1.4,9.4",
    /* 126 '~'  */ "0.6,5.2 1.6,4.2 3,5.2 4.4,6.2 5.4,5.2",
};

} // namespace demo_font_detail

class DemoStrokeFont {
    std::vector<demo_font_detail::Glyph> glyphs;

    void parse(int index, const char* spec) {
        using namespace demo_font_detail;
        Glyph& g = glyphs[index];
        if (!spec || !spec[0]) return;
        std::string text(spec);
        size_t start = 0;
        while (start <= text.size()) {
            size_t bar = text.find('|', start);
            std::string segment = text.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
            Stroke pts;
            size_t p = 0;
            while (p < segment.size()) {
                size_t next = segment.find(' ', p);
                std::string pair = segment.substr(p, next == std::string::npos ? std::string::npos : next - p);
                float x = 0.0f, y = 0.0f;
                if (std::sscanf(pair.c_str(), "%f,%f", &x, &y) == 2)
                    pts.push_back({x, y});
                if (next == std::string::npos) break;
                p = next + 1;
            }
            if (pts.size() >= 2) g.push_back(pts);
            if (bar == std::string::npos) break;
            start = bar + 1;
        }
    }

    static unsigned decodeUtf8(const std::string& s, size_t& i) {
        const unsigned char* p = (const unsigned char*)s.data();
        size_t n = s.size();
        unsigned c = p[i];
        if (c < 0x80) { i += 1; return c; }
        if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            unsigned cp = ((c & 0x1F) << 6) | (p[i+1] & 0x3F);
            i += 2; return cp;
        }
        if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            unsigned cp = ((c & 0x0F) << 12) | ((p[i+1] & 0x3F) << 6) | (p[i+2] & 0x3F);
            i += 3; return cp;
        }
        if ((c & 0xF8) == 0xF0 && i + 3 < n) {
            unsigned cp = ((c & 0x07) << 18) | ((p[i+1] & 0x3F) << 12) |
                          ((p[i+2] & 0x3F) << 6) | (p[i+3] & 0x3F);
            i += 4; return cp;
        }
        i += 1; return '?';
    }

    static char asciiFor(unsigned cp) {
        if (cp >= 32 && cp <= 126) return (char)cp;
        switch (cp) {
            case 0x00A0: return ' ';
            case 0x2013: case 0x2014: return '-';
            case 0x2018: case 0x2019: return '\'';
            case 0x201C: case 0x201D: return '"';
            case 0x2026: return '.';
            default: return '?';
        }
    }

    void drawGlyph(SDL_Renderer* r, char c, float x, float y, float px,
                   int cr, int cg, int cb, int ca, int thickness) const {
        unsigned char u = (unsigned char)c;
        if (u < 32 || u > 126) u = '?';
        const demo_font_detail::Glyph& g = glyphs[u - 32];
        float scale = px / 10.0f;
        int n = thickness < 1 ? 1 : thickness;
        int off = -(n - 1) / 2;
        for (const auto& stroke : g) {
            for (size_t k = 1; k < stroke.size(); ++k) {
                int x0 = (int)std::round(x + stroke[k-1].x * scale) + off;
                int y0 = (int)std::round(y + stroke[k-1].y * scale) + off;
                int x1 = (int)std::round(x + stroke[k].x * scale) + off;
                int y1 = (int)std::round(y + stroke[k].y * scale) + off;
                SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
                for (int oy = 0; oy < n; ++oy)
                    for (int ox = 0; ox < n; ++ox)
                        SDL_RenderDrawLine(r, x0 + ox, y0 + oy, x1 + ox, y1 + oy);
            }
        }
    }

public:
    DemoStrokeFont() : glyphs(95) {
        using namespace demo_font_detail;
        for (int i = 0; i < 95; ++i) parse(i, GLYPH_SPECS[i]);
    }

    static DemoStrokeFont& get() {
        static DemoStrokeFont instance;
        return instance;
    }

    static float advance(float px, float tracking = 0.0f) { return px * 0.70f + tracking; }
    static float lineHeight(float px) { return px * 1.35f; }

    void drawString(SDL_Renderer* r, const std::string& text, int x, int y,
                    float px, int cr, int cg, int cb, int ca = 255,
                    float tracking = 0.0f, int thickness = 1) const {
        if (!r || text.empty() || px <= 0.0f) return;
        float pen = (float)x;
        size_t i = 0;
        while (i < text.size()) {
            unsigned cp = decodeUtf8(text, i);
            if (cp == 0x2013 || cp == 0x2014) {
                SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
                int x0 = (int)std::round(pen + px * 0.08f);
                int x1 = (int)std::round(pen + px * 0.62f);
                int yy = (int)std::round(y + px * 0.48f);
                for (int oy = 0; oy < (thickness < 1 ? 1 : thickness); ++oy)
                    SDL_RenderDrawLine(r, x0, yy + oy, x1, yy + oy);
                pen += advance(px, tracking);
                continue;
            }
            char c = asciiFor(cp);
            if (c != ' ') drawGlyph(r, c, pen, (float)y, px, cr, cg, cb, ca, thickness);
            pen += advance(px, tracking);
        }
    }

    // 无文本框对白需要阴影来压住背景线条；阴影本身不是框，不改变排版。
    void drawStringShadow(SDL_Renderer* r, const std::string& text, int x, int y,
                          float px, int cr, int cg, int cb, int ca = 255,
                          int thickness = 1) const {
        int shadowA = ca * 70 / 100;
        drawString(r, text, x + 1, y + 2, px, 2, 7, 5, shadowA, 0.0f, thickness);
        drawString(r, text, x, y, px, cr, cg, cb, ca, 0.0f, thickness);
    }
};
