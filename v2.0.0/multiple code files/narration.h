#pragma once

#include "constants.h"
#include "font.h"
#include "renderer.h"

// ============== NarrationSystem (center narration only) ==============
class NarrationSystem {
    struct Line {
        std::string raw;        // original queued text (for voice clip lookup)
        std::vector<std::string> vlines;
        int numLines;
        int revealed;
        int popupTimer;
        int typeTimer;
        template <class Ar> void visit(Ar& ar) {
            ar.ioStr(raw);
            ar.ioVecStr(vlines);
            ar.ioNum(numLines); ar.ioNum(revealed); ar.ioNum(popupTimer); ar.ioNum(typeTimer);
        }
    };

    static const int POPUP_FRAMES = 18;
    static const int TYPE_SPEED = 2;

    static double marioEase(double t) {
        if (t >= 1.0) return 1.0;
        if (t <= 0.0) return 0.0;
        const double c1 = 1.70158;
        const double c3 = c1 + 1.0;
        return 1.0 + c3 * std::pow(t - 1.0, 3) + c1 * std::pow(t - 1.0, 2);
    }

    std::vector<Line> lines;
    int curLine = 0;
    bool active = false;
    int bgAlpha = 0;
    int bgTimer = 0;
    bool enterWas = false;
    int ticks = 0;

public:
    void queue(const char* text) {
        Line l;
        l.raw = text ? text : "";
        l.revealed = 0; l.popupTimer = 0; l.typeTimer = 0;
        std::string s(text);
        const int maxChars = 36;
        std::vector<std::string> segs;
        size_t pos = 0;
        while (pos < s.length()) {
            size_t nl = s.find('\n', pos);
            if (nl == std::string::npos) nl = s.length();
            segs.push_back(s.substr(pos, nl - pos));
            pos = nl + 1;
        }
        for (auto& seg : segs) {
            while ((int)seg.length() > maxChars) {
                int brk = maxChars;
                while (brk > 0 && seg[brk] != ' ') brk--;
                if (brk == 0) brk = maxChars;
                l.vlines.push_back(seg.substr(0, brk));
                seg = seg.substr(brk + 1);
            }
            if (!seg.empty()) l.vlines.push_back(seg);
        }
        l.numLines = (int)l.vlines.size();
        lines.push_back(l);
    }

    void start() {
        if (lines.empty()) return;
        curLine = 0; active = true; bgAlpha = 0; bgTimer = 0; enterWas = true;
        for (auto& l : lines) { l.revealed = 0; l.popupTimer = 0; l.typeTimer = 0; }
    }

    bool isActive() const { return active; }
    int popTicks() { int n = ticks; ticks = 0; return n; }
    // 读档后调用：避免读档瞬间跳过当前旁白页
    void suppressEnter() { enterWas = true; }

    // 存档：旁白队列 + 当前页 + 逐字进度（enterWas 保持 true 防止读档误跳过）
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(lines);
        ar.ioNum(curLine); ar.ioBool(active); ar.ioNum(bgAlpha); ar.ioNum(bgTimer);
        ar.ioBool(enterWas); ar.ioNum(ticks);
    }
    int getCurLine() const { return active ? curLine : -1; }
    const std::string& currentRawText() const {
        static std::string none;
        if (!active || curLine < 0 || curLine >= (int)lines.size()) return none;
        return lines[curLine].raw;
    }

    void reset() {
        lines.clear(); curLine = 0; active = false;
        bgAlpha = 0; bgTimer = 0; enterWas = false; ticks = 0;
    }

    void update(bool enterPressed) {
        if (!active || lines.empty()) {
            if (bgAlpha > 0) { bgTimer++; if (bgTimer >= 3) { bgAlpha -= 4; bgTimer = 0; } }
            return;
        }
        if (bgAlpha < 180) { bgTimer++; if (bgTimer >= 2) { bgAlpha += 4; bgTimer = 0; } }
        auto& cur = lines[curLine];
        if (cur.popupTimer < POPUP_FRAMES) { cur.popupTimer++; return; }
        int fullLen = 0;
        for (auto& vl : cur.vlines) fullLen += (int)vl.length();
        if (cur.revealed < fullLen) {
            cur.typeTimer++;
            if (cur.typeTimer >= TYPE_SPEED) { cur.typeTimer = 0; cur.revealed++; ticks++; }
            if (enterPressed && !enterWas) cur.revealed = fullLen;
            enterWas = enterPressed;
            return;
        }
        if (enterPressed && !enterWas) { curLine++; if (curLine >= (int)lines.size()) active = false; }
        enterWas = enterPressed;
    }

    void draw(SDL_Renderer* r, const Font& font) {
        if (!active && bgAlpha <= 0) return;
        if (lines.empty() || curLine >= (int)lines.size()) return;
        auto& cur = lines[curLine];
        int charW = 18, charH = 21, lineGap = 6;
        int maxLineLen = 0;
        for (auto& vl : cur.vlines)
            if ((int)vl.length() > maxLineLen) maxLineLen = (int)vl.length();

        double popScale = marioEase((double)cur.popupTimer / POPUP_FRAMES);
        int boxW = maxLineLen * charW + 50;
        int boxH = cur.numLines * charH + (cur.numLines - 1) * lineGap + 40;
        int boxX = CENTER_X - boxW / 2, boxY = WIN_HEIGHT / 2 - boxH / 2;
        int drawW = (int)(boxW * popScale), drawH = (int)(boxH * popScale);
        int drawX = boxX + (boxW - drawW) / 2, drawY = boxY + (boxH - drawH) / 2;
        if (drawW < 10 || drawH < 10) return;

        const int TR = 50, TG = 155, TB = 70;
        Uint8 ba = (Uint8)(bgAlpha);
        SDL_SetRenderDrawColor(r, 10, 25, 15, (Uint8)(ba * 0.85));
        SDL_Rect bgRect = {drawX, drawY, drawW, drawH};
        SDL_RenderFillRect(r, &bgRect);
        SDL_SetRenderDrawColor(r, TR, TG, TB, (Uint8)(ba * 0.7));
        SDL_RenderDrawRect(r, &bgRect);
        if (cur.popupTimer < POPUP_FRAMES) return;

        SDL_SetRenderDrawColor(r, TR, TG, TB, 255);
        int charsLeft = cur.revealed;
        for (int li = 0; li < cur.numLines && charsLeft > 0; ++li) {
            int show = charsLeft;
            if (show > (int)cur.vlines[li].length()) show = (int)cur.vlines[li].length();
            drawTextLine(r, font, cur.vlines[li].substr(0, show),
                         drawX + 25, drawY + 22 + li * (charH + lineGap), 3, 1, charW);
            charsLeft -= show;
        }
    }
};
