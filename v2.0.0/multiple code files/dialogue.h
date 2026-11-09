#pragma once

#include "art_fx.h"

// Forward declaration
class Font;
static void drawTextLine(SDL_Renderer* r, const Font& font, const std::string& text, int tx, int ty, int scale, int mul, int charW);


// ============== DialogueHistory ==============
class DialogueHistory {
public:
    struct Entry {
        std::string speaker;
        std::vector<std::string> lines;
        int numLines;
        template <class Ar> void visit(Ar& ar) {
            ar.ioStr(speaker);
            ar.ioVecStr(lines);
            ar.ioNum(numLines);
        }
    };

    void add(const std::string& speaker, const std::vector<std::string>& lines, int numLines) {
        entries.push_back({speaker, lines, numLines});
    }
    const Entry& get(int i) const { return entries[i]; }
    const Entry& operator[](int i) const { return entries[i]; }
    int size() const { return (int)entries.size(); }
    void clear() { entries.clear(); scroll = 0; focusSlot = 2; }

    // 存档：历史记录 + 暂停菜单里的滚动/焦点位置
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(entries);
        ar.ioNum(scroll); ar.ioNum(focusSlot);
    }

    int scroll = 0;
    int focusSlot = 2;  // 0=top, 1=mid, 2=bottom

    void resetView() {
        scroll = 0;
        int total = size();
        focusSlot = (total >= 3) ? 2 : (total > 0 ? total - 1 : 2);
    }

    void moveUp() {
        int total = size();
        if (total == 0) return;
        int maxSlot = (total >= 3) ? 2 : total - 1;
        if (focusSlot > maxSlot) focusSlot = maxSlot;
        int bottomIdx = total - 1 - scroll;
        bool canScrollOlder = (bottomIdx > maxSlot);
        if (focusSlot > 0) focusSlot--;
        else if (canScrollOlder) scroll++;
    }
    void moveDown() {
        int total = size();
        if (total == 0) return;
        int maxSlot = (total >= 3) ? 2 : total - 1;
        if (focusSlot > maxSlot) focusSlot = maxSlot;
        if (focusSlot < maxSlot) focusSlot++;
        else if (scroll > 0) scroll--;
    }

private:
    std::vector<Entry> entries;
};


// ============== DialogueSystem ==============
class DialogueSystem {
    struct Line {
        std::string text;
        std::string speaker;
        bool sameSpeaker;
        bool historyRecorded;
        std::vector<std::string> vlines;
        int numLines;
        int state;       // 0=idle, 1=popin, 2=typewriter, 3=display, 4=fadeout
        int timer;
        int revealed;
        int typeTimer;
        float y;
        float fadeStartY;
        // 存档：对话进度（含打字机已显示字数/弹出动画计时/淡出位置）
        template <class Ar> void visit(Ar& ar) {
            ar.ioStr(text); ar.ioStr(speaker);
            ar.ioBool(sameSpeaker); ar.ioBool(historyRecorded);
            ar.ioVecStr(vlines); ar.ioNum(numLines);
            ar.ioNum(state); ar.ioNum(timer); ar.ioNum(revealed); ar.ioNum(typeTimer);
            ar.ioNum(y); ar.ioNum(fadeStartY);
        }
    };

    static const int POPUP_FRAMES = 15;
    static const int FADE_FRAMES = 40;
    static const int WRAP_CHARS = 20;
    static const int TYPE_SPEED = 2;

    static double marioEase(double t) {
        if (t >= 1.0) return 1.0;
        if (t <= 0.0) return 0.0;
        const double c1 = 1.70158;
        const double c3 = c1 + 1.0;
        return 1.0 + c3 * std::pow(t - 1.0, 3) + c1 * std::pow(t - 1.0, 2);
    }

    std::vector<Line> queue;
    int idx = 0;
    bool active = false;
    bool enterWas = false;
    int ticks = 0;

public:
    DialogueHistory history;

    // 存档：当前队列 + 进度 + 边沿检测标记（enterWas 保持 true，读档后不会误跳过第一行）
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(queue);
        ar.ioNum(idx); ar.ioBool(active); ar.ioBool(enterWas); ar.ioNum(ticks);
        ar.ioObj(history);
    }

    void queueDialogue(const char* speaker, const char* text) {
        Line l;
        l.text = text;
        l.speaker = speaker ? speaker : "";
        l.sameSpeaker = false;
        l.historyRecorded = false;
        l.state = 0; l.timer = 0; l.revealed = 0; l.typeTimer = 0;
        l.y = 200.0f; l.fadeStartY = 0;
        std::string s(text);
        while ((int)s.length() > WRAP_CHARS) {
            int brk = WRAP_CHARS;
            while (brk > 0 && s[brk] != ' ') brk--;
            if (brk == 0) brk = WRAP_CHARS;
            l.vlines.push_back(s.substr(0, brk));
            s = s.substr(brk + 1);
        }
        if (!s.empty()) l.vlines.push_back(s);
        l.numLines = (int)l.vlines.size();
        queue.push_back(l);
    }

    void start() {
        if (idx > 0) { queue.erase(queue.begin(), queue.begin() + idx); idx = 0; }
        if (queue.empty()) return;
        for (int i = 0; i < (int)queue.size(); ++i)
            queue[i].sameSpeaker = (i + 1 < (int)queue.size() && queue[i].speaker == queue[i+1].speaker);
        queue[idx].state = 1; queue[idx].timer = 0;
        queue[idx].revealed = 0; queue[idx].typeTimer = 0;
        queue[idx].y = 200.0f;
        active = true;
    }

    bool isActive() const { return active; }
    int popTicks() { int n = ticks; ticks = 0; return n; }
    // 读档后调用：把 ENTER 边沿状态置为"已按下"，避免读档瞬间跳过当前行
    void suppressEnter() { enterWas = true; }
    const std::string& currentSpeaker() const {
        static std::string none;
        if (!active || idx >= (int)queue.size()) return none;
        return queue[idx].speaker;
    }
    const std::string& currentText() const {
        static std::string none;
        if (!active || idx >= (int)queue.size()) return none;
        return queue[idx].text;
    }

    void reset() {
        queue.clear(); idx = 0; active = false; enterWas = false; ticks = 0;
        history.clear();
    }

    void update(bool enterPressed) {
        if (!active || idx >= (int)queue.size()) { active = false; return; }
        auto& l = queue[idx];
        bool skip = enterPressed && !enterWas;
        int totalChars = 0;
        for (auto& vl : l.vlines) totalChars += (int)vl.length();

        switch (l.state) {
            case 1: // Pop-in
                l.timer++;
                if (skip || l.timer >= POPUP_FRAMES) {
                    l.state = 2; l.timer = 0;
                    if (!l.historyRecorded) {
                        l.historyRecorded = true;
                        history.add(l.speaker, l.vlines, l.numLines);
                    }
                }
                break;
            case 2: // Typewriter
                if (l.revealed < totalChars) {
                    if (skip) l.revealed = totalChars;
                    else { l.typeTimer++; if (l.typeTimer >= TYPE_SPEED) { l.typeTimer = 0; l.revealed++; ticks++; } }
                } else { l.state = 3; l.timer = 0; }
                break;
            case 3: // Display
                l.timer++;
                if (skip || l.timer >= 120) { l.state = 4; l.timer = 0; l.fadeStartY = l.y; }
                break;
            case 4: { // Fade out
                l.timer++;
                double t = (double)l.timer / FADE_FRAMES;
                l.y = l.fadeStartY - (float)(50.0 * (1.0 - std::pow(1.0 - t, 3.0)));
                if (skip || l.timer >= FADE_FRAMES) {
                    idx++;
                    if (idx >= (int)queue.size()) { active = false; enterWas = enterPressed; return; }
                    auto& next = queue[idx];
                    next.state = 1; next.timer = 0; next.revealed = 0; next.typeTimer = 0;
                    next.y = 200.0f;
                }
                break;
            }
        }
        enterWas = enterPressed;
    }

    void draw(SDL_Renderer* r, const Font& font) {
        if (!active || idx >= (int)queue.size()) return;
        auto& l = queue[idx];
        if (l.vlines.empty()) return;

        const int CH_W = 12, CH_H = 14, LINE_GAP = 4;
        double bright = 1.0;
        int floatOff = 0;
        if (l.state == 4) {
            double t = (double)l.timer / FADE_FRAMES;
            bright = 1.0 - t; if (bright < 0.0) bright = 0.0;
            floatOff = (int)(50.0 * (1.0 - std::pow(1.0 - t, 3.0)));
        }
        int drawY = (int)l.y - floatOff;
        if (l.state == 1) return;

        int maxChars = 0;
        for (auto& vl : l.vlines)
            if ((int)vl.length() > maxChars) maxChars = (int)vl.length();
        int speakerH = l.speaker.empty() ? 0 : CH_H + 5;
        int boxW = maxChars * CH_W + 34;
        int boxH = l.numLines * (CH_H + LINE_GAP) + speakerH + 18;
        int boxX = 8, boxY = drawY - 9;
        if (boxX + boxW > 390) boxW = 382;

        int bgA = (int)(215 * bright);
        ArtFX::BlendGuard guard(r);
        SDL_SetRenderDrawColor(r, 3, 12, 10, (Uint8)bgA);
        SDL_Rect bg = {boxX, boxY, boxW, boxH};
        SDL_RenderFillRect(r, &bg);
        SDL_SetRenderDrawColor(r, 42, 163, 96, (Uint8)(190 * bright));
        SDL_RenderDrawLine(r, boxX, boxY, boxX + boxW, boxY);
        SDL_RenderDrawLine(r, boxX, boxY + boxH - 1, boxX + boxW, boxY + boxH - 1);
        SDL_SetRenderDrawColor(r, 41, 102, 72, (Uint8)(150 * bright));
        SDL_RenderDrawLine(r, boxX + 1, boxY + 1, boxX + 1, boxY + boxH - 2);
        SDL_SetRenderDrawColor(r, 121, 255, 166, (Uint8)(190 * bright));
        SDL_Rect accent = {boxX, boxY, 3, boxH};
        SDL_RenderFillRect(r, &accent);
        ArtFX::bracket(r, boxX + 5, boxY + 5, boxW - 10, boxH - 10, 9,
                       78, 205, 128, (int)(115 * bright));
        // 小型通讯标识，不占用任何游戏逻辑字段。
        font.drawStringColor(r, "COMMS // INCOMING", boxX + 12, boxY + 6, 1,
                             91, 188, 126, (int)(180 * bright));

        const int TR = 66, TG = 210, TB = 126;
        int rr = (int)(TR * bright), gg = (int)(TG * bright), bb = (int)(TB * bright);
        int sr = (int)(166 * bright), sg = (int)(228 * bright), sb = (int)(196 * bright);
        int textX = boxX + 10;
        int textY = boxY + 18;

        // Speaker
        if (!l.speaker.empty()) {
            SDL_SetRenderDrawColor(r, (Uint8)sr, (Uint8)sg, (Uint8)sb, 255);
            drawTextLine(r, font, l.speaker, textX, textY, 1, 2, CH_W);
            textY += speakerH;
        }
        // Content
        int charsLeft = l.revealed;
        SDL_SetRenderDrawColor(r, (Uint8)rr, (Uint8)gg, (Uint8)bb, 255);
        for (int li = 0; li < l.numLines && charsLeft > 0; ++li) {
            int show = charsLeft;
            if (show > (int)l.vlines[li].length()) show = (int)l.vlines[li].length();
            drawTextLine(r, font, l.vlines[li].substr(0, show),
                         textX, textY + li * (CH_H + LINE_GAP), 1, 2, CH_W);
            charsLeft -= show;
        }
        // 打字机光标：只依赖已显示字符数，读档帧保持确定性。
        if (l.state == 2 && (l.revealed / 4) % 2 == 0) {
            int visible = l.revealed;
            int cx = textX, cy = textY;
            int curLine = 0;
            for (; curLine < l.numLines; ++curLine) {
                int len = (int)l.vlines[curLine].length();
                if (visible <= len) break;
                visible -= len;
                cy += CH_H + LINE_GAP;
            }
            if (curLine >= l.numLines) { curLine = l.numLines - 1; visible = (int)l.vlines[curLine].length(); cy = textY + curLine * (CH_H + LINE_GAP); }
            cx = textX + visible * CH_W;
            SDL_SetRenderDrawColor(r, 150, 255, 188, (Uint8)(210 * bright));
            SDL_Rect caret = {cx, cy + 2, 6, 9};
            SDL_RenderFillRect(r, &caret);
        }
    }
};

// Night Elf dark green color scheme
// Center narration: chapter intro/outro, blocking (gameplay paused)
// Left dialogue: in-game teammate hints, non-blocking (future use)
// Shared text drawing helper
static void drawTextLine(SDL_Renderer* r, const Font& font, const std::string& text,
                         int tx, int ty, int scale, int mul, int charW) {
    for (char c : text) {
        if (c == ' ') { tx += charW; continue; }
        font.drawChar(r, c, tx, ty, scale, mul);
        tx += charW;
    }
}


