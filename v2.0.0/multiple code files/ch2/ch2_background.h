#pragma once

#include "../constants.h"
#include "../renderer.h"
#include "../types.h"
#include <cmath>
#include <vector>
#include <algorithm>

// ============== Ch2Background ==============
class Ch2Background {
private:
    struct Star2 { double wx, wy; float phase, twinkleSpeed; bool isCross;
        template <class Ar> void visit(Ar& ar) {
            ar.ioNum(wx); ar.ioNum(wy); ar.ioNum(phase); ar.ioNum(twinkleSpeed); ar.ioBool(isCross);
        }
    };
    struct Pillar { double wx; bool isMajor;
        template <class Ar> void visit(Ar& ar) { ar.ioNum(wx); ar.ioBool(isMajor); }
    };

    std::vector<Star2> stars;
    std::vector<Pillar> pillars;
    double scrollX, scrollSpeed, genNext;
    static constexpr double STAR_PARALLAX = 0.015;
    static constexpr double PILLAR_SPACING = 330.0; // ~1.5s between pillars
    static const int VP_X = CENTER_X;    // pillar/glass wall vanishing point x
    static const int VP_Y = -60;        // pillar/glass wall vanishing point y (above screen)
    static const int WALL_Y = 280;      // floor meets glass wall

    // Longitudinal floor seams (horizontal lines along corridor, at fixed depths)
    // Each track = a fixed y position on the floor, seams scroll left at same speed
    struct SeamTrack {
        double y;           // fixed screen y for this depth track
        double spacing;     // world spacing between seams at this depth (near=wider, far=narrower)
        double genNext;     // next world-x to generate
        std::vector<double> wxs; // world-x positions of seams
    };
    static const int N_SEAM_TRACKS = 5;
    SeamTrack seamTracks[N_SEAM_TRACKS];

    void genStars() {
        for (int i = 0; i < 400; ++i) {
            Star2 s;
            s.wx = rand() % 6000; s.wy = rand() % WIN_HEIGHT;
            s.phase = (rand() % 628) / 100.0f;
            s.twinkleSpeed = 0.008f + (rand() % 50) / 1000.0f;
            s.isCross = (rand() % 100 < 10);
            stars.push_back(s);
        }
    }

    void genAhead() {
        double ahead = scrollX + WIN_WIDTH + 600.0; // margin: generate well off-screen to avoid visible pop-in
        // Pillars
        while (genNext < ahead) {
            pillars.push_back({genNext, (int)(genNext / PILLAR_SPACING) % 5 == 0});
            genNext += PILLAR_SPACING;
        }
        // Floor seams per track
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            while (seamTracks[t].genNext < ahead) {
                seamTracks[t].wxs.push_back(seamTracks[t].genNext);
                seamTracks[t].genNext += seamTracks[t].spacing;
            }
        }
    }

    template<typename T>
    void pruneVec(std::vector<T>& v, double cutoff) {
        int w = 0;
        for (int i = 0; i < (int)v.size(); ++i)
            if (v[i].wx >= cutoff) v[w++] = v[i];
        v.resize(w);
    }
    template<typename T>
    void pruneDoubleVec(std::vector<T>& v, double cutoff) {
        int w = 0;
        for (int i = 0; i < (int)v.size(); ++i)
            if (v[i] >= cutoff) v[w++] = v[i];
        v.resize(w);
    }
    void pruneBehind() {
        double cutoff = scrollX - 500.0;
        pruneVec(pillars, cutoff);
        for (int t = 0; t < N_SEAM_TRACKS; ++t)
            pruneDoubleVec(seamTracks[t].wxs, cutoff);
    }

    void pruneStars() {
        double cutoff = scrollX * STAR_PARALLAX - 400.0;
        double wrap = scrollX * STAR_PARALLAX + WIN_WIDTH + 400.0;
        for (auto& s : stars) {
            if (s.wx < cutoff) { s.wx += 6000.0; s.phase = (rand() % 628) / 100.0f; }
            if (s.wx > wrap) { s.wx -= 6000.0; }
        }
    }

public:
    Ch2Background() : scrollX(0), scrollSpeed(3.5), genNext(-200.0) {
        genStars();
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            double frac = (double)(t + 1) / (N_SEAM_TRACKS + 1);
            seamTracks[t].y = WALL_Y + (WIN_HEIGHT - WALL_Y) * frac * frac;
            seamTracks[t].spacing = 45.0 + t * 22.0;
            seamTracks[t].genNext = -200.0;
        }
    }
    void reset() {
        scrollX = 0; scrollSpeed = 3.5; genNext = -200.0;
        pillars.clear(); stars.clear(); genStars();
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            seamTracks[t].wxs.clear();
            seamTracks[t].genNext = -200.0;
        }
    }
    void setSpeed(double s) { scrollSpeed = s; }
    double getScrollX() const { return scrollX; }

    // 存档：走廊滚动进度 + 星星/立柱/地板缝（读档后画面与存档帧完全一致）
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(stars);
        ar.ioVecObj(pillars);
        ar.ioNum(scrollX); ar.ioNum(scrollSpeed); ar.ioNum(genNext);
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            ar.ioNum(seamTracks[t].y);
            ar.ioNum(seamTracks[t].spacing);
            ar.ioNum(seamTracks[t].genNext);
            ar.ioVecNum(seamTracks[t].wxs);
        }
    }

    void update() {
        scrollX += scrollSpeed;
        for (auto& s : stars) {
            s.phase += s.twinkleSpeed;
            if (s.phase > 2.0f * M_PI) s.phase -= 2.0f * M_PI;
        }
        pruneStars();
        genAhead();
        pruneBehind();
    }

    void draw(SDL_Renderer* r) {
        // 整层打开 BLEND：玻璃、反射、灯光衰减都需要 alpha 才能真正半透明
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        drawSky(r);
        drawFarStructure(r);
        drawCeiling(r);
        drawFloor(r);
        drawGlassPanels(r);
        drawPillars(r);
        drawAmbient(r);
        drawForegroundRail(r);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

private:
    // ---------------- 绘制小工具 ----------------
    static void fillR(SDL_Renderer* r, int x, int y, int w, int h,
                      int cr, int cg, int cb, int ca = 255) {
        if (w <= 0 || h <= 0) return;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderFillRect(r, &rr);
    }
    static void lineR(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                      int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void boxR(SDL_Renderer* r, int x, int y, int w, int h,
                     int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderDrawRect(r, &rr);
    }
    static void arcR(SDL_Renderer* r, double cx, double cy, double rad,
                     double a0, double a1, int cr, int cg, int cb, int ca) {
        int steps = (int)(std::fabs(a1 - a0) * rad / 3.0);
        if (steps < 3) steps = 3;
        if (steps > 96) steps = 96;
        int px = (int)(cx + rad * std::cos(a0));
        int py = (int)(cy + rad * std::sin(a0));
        for (int i = 1; i <= steps; ++i) {
            double a = a0 + (a1 - a0) * i / steps;
            int nx = (int)(cx + rad * std::cos(a));
            int ny = (int)(cy + rad * std::sin(a));
            if (i < steps) lineR(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static void ellipseR(SDL_Renderer* r, double cx, double cy, double rx, double ry,
                         int cr, int cg, int cb, int ca) {
        const int SEG = 40;
        int px = (int)(cx + rx), py = (int)cy;
        for (int i = 1; i <= SEG; ++i) {
            double a = 2.0 * M_PI * i / SEG;
            int nx = (int)(cx + rx * std::cos(a));
            int ny = (int)(cy + ry * std::sin(a));
            lineR(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static unsigned hashU(unsigned x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
    static double hash01(int x) { return (hashU((unsigned)x) % 4096) / 4096.0; }

    // 灯带按世界坐标等距排列；廊桥滚动时灯随结构一起移动
    template <class F> void forEachLamp(F f) const {
        const double step = 240.0;
        double first = std::floor((scrollX - 80.0) / step) * step;
        for (double wx = first; wx < scrollX + WIN_WIDTH + 120.0; wx += step) {
            double sx = wx - scrollX;
            if (sx < -90.0 || sx > WIN_WIDTH + 90.0) continue;
            f(sx, wx);
        }
    }

    // ---------------- 远处桁架：窗外第二层结构 ----------------
    void drawFarStructure(SDL_Renderer* r) {
        const double par = 0.42;
        const double step = 250.0;
        double base = scrollX * par;
        double first = std::floor((base - 120.0) / step) * step;
        for (double wx = first; wx < base + WIN_WIDTH + 120.0; wx += step) {
            int sx = (int)(wx - base);
            lineR(r, sx, 24, sx, WALL_Y - 2, 46, 56, 74, 90);
            lineR(r, sx + 1, 24, sx + 1, WALL_Y - 2, 30, 37, 49, 100);
            lineR(r, sx - 7, 42, sx + 7, 42, 62, 74, 96, 80);
            lineR(r, sx - 7, 210, sx + 7, 210, 62, 74, 96, 55);
            // 远端柱上的小灯
            if (hashU((unsigned)(int)(wx * 0.21)) % 5 == 0)
                fillR(r, sx - 1, 96, 3, 3, 110, 210, 238, 100);
        }
        // 远处横梁（分段，避免变成整屏网格）
        for (double wx = first; wx < base + WIN_WIDTH + 120.0; wx += step) {
            int sx = (int)(wx - base);
            lineR(r, sx, 72, sx + (int)step, 72, 50, 60, 78, 45);
            lineR(r, sx, 170, sx + (int)step, 170, 50, 60, 78, 35);
        }
        lineR(r, 0, 24, WIN_WIDTH, 24, 56, 66, 84, 70);
    }

    // ---------------- 窗外宇宙 ----------------
    void drawSky(SDL_Renderer* r) {
        fillR(r, 0, 0, WIN_WIDTH, WIN_HEIGHT, 0, 0, 0, 255);

        // 巨大的行星弧面：几乎不动，给廊桥一个"窗外"的尺度
        const double pcx = -40.0 - scrollX * 0.006;
        const double pcy = 640.0;
        const double pr = 470.0;
        for (int k = 0; k < 4; ++k) {
            double rr = pr - k * 10.0;
            int a = 92 - k * 16;
            if (a < 18) a = 18;
            arcR(r, pcx, pcy, rr, -M_PI * 0.74, -M_PI * 0.26,
                 82 + k * 10, 128 + k * 12, 178 + k * 12, a);
        }
        // 受光边缘 + 晨昏线
        arcR(r, pcx, pcy, pr + 1, -M_PI * 0.74, -M_PI * 0.26, 150, 200, 240, 110);
        arcR(r, pcx + 34, pcy + 12, pr - 5, -M_PI * 0.70, -M_PI * 0.30, 16, 24, 36, 190);
        // 行星表面几道极淡的云带
        for (int k = 0; k < 3; ++k)
            arcR(r, pcx + 8 * k, pcy + 20 * k, pr - 24 - 18 * k,
                 -M_PI * 0.62, -M_PI * 0.38, 110, 158, 200, 42);

        // 远处 Moonwell 结构：一个缓慢漂移的环 + 天线 + 航行灯
        double stx = std::fmod(690.0 - scrollX * 0.032, 1900.0);
        if (stx < -260.0) stx += 1900.0;
        if (stx > -260.0 && stx < WIN_WIDTH + 260.0) {
            double sy = 118.0;
            ellipseR(r, stx, sy, 52, 18, 108, 136, 170, 88);
            ellipseR(r, stx, sy, 64, 23, 82, 106, 138, 60);
            lineR(r, (int)(stx - 84), (int)sy, (int)(stx - 64), (int)sy, 96, 120, 150, 80);
            lineR(r, (int)(stx + 64), (int)sy, (int)(stx + 92), (int)sy, 96, 120, 150, 80);
            lineR(r, (int)stx, (int)(sy - 18), (int)stx, (int)(sy - 58), 96, 120, 150, 75);
            lineR(r, (int)(stx - 12), (int)(sy - 58), (int)(stx + 12), (int)(sy - 58), 96, 120, 150, 75);
            lineR(r, (int)stx, (int)(sy + 18), (int)stx, (int)(sy + 54), 84, 104, 130, 60);
            lineR(r, (int)(stx - 16), (int)(sy + 54), (int)(stx + 16), (int)(sy + 54), 84, 104, 130, 60);
            for (int k = -2; k <= 2; ++k)
                lineR(r, (int)stx + k * 6, (int)(sy + 18), (int)stx + k * 6, (int)(sy + 48),
                      70, 90, 116, 35);
            // 航行灯：慢闪
            double blink = std::sin(scrollX * 0.02 + stx * 0.02);
            if (blink > 0.2) {
                fillR(r, (int)(stx - 2), (int)(sy - 46), 4, 4, 255, 90, 78, 170);
                fillR(r, (int)(stx + 62), (int)(sy - 2), 3, 3, 110, 230, 255, 120);
            }
        }

        // 星空（原系统保留）
        for (const auto& s : stars) {
            int sx = (int)(s.wx - scrollX * STAR_PARALLAX);
            int sy = (int)s.wy;
            if (sx < -20 || sx > WIN_WIDTH + 20) continue;
            float bright = 0.35f + 0.65f * std::fabs(std::sin(s.phase));
            int b = (int)(bright * 240); if (b < 40) b = 40; if (b > 255) b = 255;
            SDL_SetRenderDrawColor(r, (Uint8)b, (Uint8)b, (Uint8)b, 255);
            SDL_RenderDrawPoint(r, sx, sy);
            if (s.isCross && bright > 0.6f) {
                int cb = (int)(bright * 160); if (cb > 160) cb = 160;
                lineR(r, sx - 4, sy, sx + 4, sy, cb, cb, cb, 255);
                lineR(r, sx, sy - 4, sx, sy + 4, cb, cb, cb, 255);
            }
        }

        // 廊桥尽头的冷光
        for (int k = 0; k < 5; ++k)
            arcR(r, VP_X, 104, 14.0 + k * 16.0, 0, 2.0 * M_PI,
                 60, 120, 150, 10 - k);
    }

    // ---------------- 顶部桁架与吊灯 ----------------
    void drawCeiling(SDL_Renderer* r) {
        // 顶梁
        fillR(r, 0, 0, WIN_WIDTH, 12, 20, 24, 33, 255);
        lineR(r, 0, 12, WIN_WIDTH, 12, 108, 124, 150, 230);
        lineR(r, 0, 13, WIN_WIDTH, 13, 36, 42, 54, 230);
        // 梁分段
        for (double wx = std::floor((scrollX - 64.0) / 64.0) * 64.0;
             wx < scrollX + WIN_WIDTH + 80.0; wx += 64.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, 2, sx, 12, 58, 68, 86, 200);
        }
        forEachLamp([&](double sx, double wx) {
            int x = (int)sx;
            // 吊杆 + 灯罩（下移到 y=26，避免贴着画面顶端）
            lineR(r, x, 12, x, 26, 128, 142, 168, 220);
            lineR(r, x - 2, 12, x - 2, 26, 74, 86, 108, 190);
            fillR(r, x - 17, 26, 34, 7, 42, 49, 63, 255);
            boxR(r, x - 17, 26, 34, 7, 142, 158, 184, 200);
            fillR(r, x - 13, 33, 26, 3, 170, 232, 252, 170);
            // 灯芯与光晕（轻微呼吸）
            double pulse = 0.72 + 0.28 * std::sin(scrollX * 0.018 + wx * 0.01);
            bool flicker = hashU((unsigned)(int)(wx * 0.17)) % 19 == 0 &&
                           ((int)(scrollX * 0.15)) % 46 < 12;
            if (!flicker) {
                int a = (int)(72 * pulse);
                fillR(r, x - 14, 34, 28, 10, 120, 205, 235, a);
                fillR(r, x - 7, 36, 14, 5, 225, 248, 255, 230);
                fillR(r, x - 2, 36, 4, 5, 255, 255, 255, 250);
            }
            // 灯两侧斜撑
            lineR(r, x, 26, x - 52, 0, 82, 94, 116, 130);
            lineR(r, x, 26, x + 52, 0, 82, 94, 116, 130);
        });
    }

    // ---------------- 地面 ----------------
    void drawFloor(SDL_Renderer* r) {
        // 由远及近的微妙明度梯度
        const int BANDS = 8;
        for (int i = 0; i < BANDS; ++i) {
            int y0 = WALL_Y + (WIN_HEIGHT - WALL_Y) * i / BANDS;
            int y1 = WALL_Y + (WIN_HEIGHT - WALL_Y) * (i + 1) / BANDS;
            fillR(r, 0, y0, WIN_WIDTH, y1 - y0,
                  34 + i * 2, 38 + i * 2, 49 + i * 2, 255);
        }
        // 墙脚
        fillR(r, 0, WALL_Y - 4, WIN_WIDTH, 4, 34, 39, 50, 255);
        lineR(r, 0, WALL_Y, WIN_WIDTH, WALL_Y, 160, 178, 206, 245);
        lineR(r, 0, WALL_Y + 1, WIN_WIDTH, WALL_Y + 1, 56, 64, 80, 240);

        // 墙脚灯带：短促的冷光虚线随廊桥移动
        for (double wx = std::floor((scrollX - 80.0) / 64.0) * 64.0;
             wx < scrollX + WIN_WIDTH + 80.0; wx += 64.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, WALL_Y + 6, sx + 22, WALL_Y + 6, 140, 232, 255, 200);
            lineR(r, sx + 3, WALL_Y + 7, sx + 19, WALL_Y + 7, 210, 248, 255, 120);
        }

        // 远处/近处的纵向地板缝（保留原系统）
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            int y = (int)seamTracks[t].y;
            int alpha = 190 - t * 20;
            if (alpha < 30) alpha = 30;
            for (double wx : seamTracks[t].wxs) {
                double sx = wx - scrollX;
                if (sx < -40 || sx > WIN_WIDTH + 40) continue;
                int halfW = 15 + t * 8;
                lineR(r, (int)sx - halfW, y, (int)sx + halfW, y, 96, 108, 130, alpha);
            }
            // 连续的纵深缝，把地面读成一条向前的通道
            lineR(r, 0, y, WIN_WIDTH, y, 72, 84, 104, 44 + t * 7);
        }

        // 地面横向分块（随廊桥滚动）
        for (double wx = std::floor((scrollX - 60.0) / 92.0) * 92.0;
             wx < scrollX + WIN_WIDTH + 60.0; wx += 92.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, WALL_Y + 2, sx, WIN_HEIGHT, 62, 74, 94, 40);
            lineR(r, sx + 1, WALL_Y + 2, sx + 1, WIN_HEIGHT, 38, 46, 60, 28);
        }

        // 近处前景压边，避免画面下缘空掉
        fillR(r, 0, WIN_HEIGHT - 18, WIN_WIDTH, 18, 12, 15, 21, 255);
        lineR(r, 0, WIN_HEIGHT - 18, WIN_WIDTH, WIN_HEIGHT - 18, 92, 108, 132, 190);
        lineR(r, 0, WIN_HEIGHT - 16, WIN_WIDTH, WIN_HEIGHT - 16, 90, 200, 230, 22);

        // 立柱在湿地面的倒影
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -40 || sx > WIN_WIDTH + 40) continue;
            double topX = VP_X + (sx - VP_X) * 0.85;
            lineR(r, (int)sx - 1, WALL_Y + 3, (int)sx - 1, WALL_Y + 34, 120, 156, 190, 48);
            lineR(r, (int)sx + 2, WALL_Y + 3, (int)(sx + (sx - topX) * 0.06), WALL_Y + 42,
                  104, 136, 166, 30);
        }

        // 吊灯在地面上的光池 + 竖直反光
        forEachLamp([&](double sx, double wx) {
            (void)wx;
            for (int k = 0; k < 3; ++k)
                ellipseR(r, sx, WALL_Y + 30 + k * 15, 56 - k * 12, 7 + k * 3,
                         120, 208, 238, 44 - k * 8);
            lineR(r, (int)sx, WALL_Y + 4, (int)sx, WALL_Y + 74, 150, 220, 245, 18);
            lineR(r, (int)sx + 1, WALL_Y + 8, (int)sx + 1, WALL_Y + 58, 190, 238, 255, 10);
            fillR(r, (int)sx - 2, WALL_Y + 5, 5, 3, 205, 245, 255, 120);
        });

        // 立柱底部的警示斜纹
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -30 || sx > WIN_WIDTH + 30) continue;
            for (int k = 0; k < 3; ++k) {
                int bx = (int)sx - 18 + k * 12;
                lineR(r, bx, WALL_Y + 5, bx + 7, WALL_Y + 11, 232, 170, 62, 110);
            }
        }
    }

    // ---------------- 玻璃幕墙 ----------------
    void pillarScreenX(const Pillar& p, double& botX, double& topX) const {
        botX = p.wx - scrollX;
        topX = VP_X + (botX - VP_X) * 0.85;
    }

    void drawGlassPanels(SDL_Renderer* r) {
        for (size_t i = 0; i + 1 < pillars.size(); ++i) {
            double lbx, ltx, rbx, rtx;
            pillarScreenX(pillars[i], lbx, ltx);
            pillarScreenX(pillars[i+1], rbx, rtx);
            const double glassT = 12.0, glassB = WALL_Y;

            double minX = std::min(std::min(lbx, ltx), std::min(rbx, rtx));
            double maxX = std::max(std::max(lbx, ltx), std::max(rbx, rtx));
            if (maxX < -100 || minX > WIN_WIDTH + 100) continue;

            // 玻璃四边：两层线做出框料厚度
            lineR(r, (int)lbx, (int)glassB, (int)ltx, (int)glassT, 178, 210, 238, 215);
            lineR(r, (int)rbx, (int)glassB, (int)rtx, (int)glassT, 178, 210, 238, 215);
            lineR(r, (int)(lbx + 2), (int)glassB - 2, (int)(ltx + 2), (int)glassT, 226, 244, 255, 120);
            lineR(r, (int)(rbx - 2), (int)glassB - 2, (int)(rtx - 2), (int)glassT, 226, 244, 255, 120);
            lineR(r, (int)ltx, (int)glassT, (int)rtx, (int)glassT, 198, 226, 250, 190);
            lineR(r, (int)lbx, (int)glassB, (int)rbx, (int)glassB, 170, 202, 230, 170);

            // 横向/纵向分格
            for (int hi = 1; hi <= 3; ++hi) {
                double t = hi / 4.0;
                int ly = (int)(glassT + (glassB - glassT) * t);
                int lx = (int)(ltx + (lbx - ltx) * t);
                int rx = (int)(rtx + (rbx - rtx) * t);
                lineR(r, lx, ly, rx, ly, 170, 204, 234, 90);
            }
            for (int vi = 1; vi <= 2; ++vi) {
                double t = vi / 3.0;
                int bx = (int)(lbx + (rbx - lbx) * t);
                int tx = (int)(ltx + (rtx - ltx) * t);
                lineR(r, bx, (int)glassB, tx, (int)glassT, 170, 204, 234, 75);
            }

            // 四角 L 形高光
            lineR(r, (int)ltx, (int)glassT, (int)ltx + 7, (int)glassT, 232, 246, 255, 210);
            lineR(r, (int)ltx, (int)glassT, (int)ltx, (int)glassT + 8, 232, 246, 255, 210);
            lineR(r, (int)rtx, (int)glassT, (int)rtx - 7, (int)glassT, 232, 246, 255, 210);
            lineR(r, (int)rtx, (int)glassT, (int)rtx, (int)glassT + 8, 232, 246, 255, 210);
            lineR(r, (int)lbx, (int)glassB, (int)lbx + 7, (int)glassB, 222, 240, 255, 180);
            lineR(r, (int)lbx, (int)glassB, (int)lbx, (int)glassB - 7, 222, 240, 255, 180);
            lineR(r, (int)rbx, (int)glassB, (int)rbx - 7, (int)glassB, 222, 240, 255, 180);
            lineR(r, (int)rbx, (int)glassB, (int)rbx, (int)glassB - 7, 222, 240, 255, 180);

            // 缓慢移动的玻璃反光 + 固定的一束冷光
            double cx = (ltx + rtx + lbx + rbx) * 0.25;
            double sheen = std::fmod(scrollX * 0.22 + i * 57.0, 170.0);
            int sx0 = (int)(lbx + sheen);
            lineR(r, sx0, (int)glassB - 12, sx0 + 26, (int)glassB - 92, 200, 230, 252, 36);
            lineR(r, sx0 + 3, (int)glassB - 12, sx0 + 29, (int)glassB - 92, 228, 244, 255, 18);
            lineR(r, (int)cx + 14, (int)glassB - 8, (int)cx + 22, (int)glassT + 22,
                  190, 222, 246, 28);

            // 玻璃上的细小灰尘/划痕
            for (int k = 0; k < 3; ++k) {
                int hx = (int)(lbx + 18 + hash01((int)(i * 31 + k * 7)) * (rbx - lbx - 36));
                int hy = (int)(glassT + 18 + hash01((int)(i * 47 + k * 13)) * (glassB - glassT - 36));
                fillR(r, hx, hy, 1, 1, 210, 232, 250, 90);
            }

            // 底部压条
            lineR(r, (int)lbx, (int)glassB - 3, (int)rbx, (int)glassB - 3, 76, 86, 104, 210);
            fillR(r, (int)lbx + 3, (int)glassB - 2, 3, 2, 130, 146, 170, 120);
            fillR(r, (int)rbx - 6, (int)glassB - 2, 3, 2, 130, 146, 170, 120);
        }
    }

    // ---------------- 立柱 ----------------
    void drawPillars(SDL_Renderer* r) {
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -120 || sx > WIN_WIDTH + 120) continue;

            double botX = sx;
            const double botY = WALL_Y;
            double topX = VP_X + (botX - VP_X) * 0.85;
            const double topY = 0;

            int pw = p.isMajor ? 6 : 3;
            double dx = topX - botX, dy = topY - botY;
            double len = std::sqrt(dx*dx + dy*dy);
            if (len < 1.0) continue;
            double nx = -dy / len, ny = dx / len;

            // 柱身
            for (int w = -pw; w <= pw; ++w) {
                int x0 = (int)(botX + nx * w), y0 = (int)(botY + ny * w);
                int x1 = (int)(topX + nx * w), y1 = (int)(topY + ny * w);
                lineR(r, x0, y0, x1, y1, 64, 68, 82, 250);
            }
            // 亮边
            lineR(r, (int)(botX + nx * pw), (int)(botY + ny * pw),
                  (int)(topX + nx * pw), (int)(topY + ny * pw), 112, 128, 162, 235);
            lineR(r, (int)(botX - nx * pw), (int)(botY - ny * pw),
                  (int)(topX - nx * pw), (int)(topY - ny * pw), 146, 164, 196, 235);

            // 柱身灯带：分段亮起
            double pulse = 0.55 + 0.45 * std::sin(scrollX * 0.03 + p.wx * 0.02);
            int seg = (int)((botY - 16) / 26);
            for (int s = 0; s < seg; ++s) {
                double f = (s + 0.5) / seg;
                int yy = (int)(botY + (topY - botY) * f);
                int xx = (int)(botX + (topX - botX) * f + nx * (pw + 1));
                if (s % 3 == 0) {
                    int a = (int)(130 + 90 * pulse);
                    lineR(r, xx - 1, yy, xx + 2, yy, 120, 228, 255, a);
                }
            }

            // 柱脚底座 + 地脚螺栓
            fillR(r, (int)sx - (p.isMajor ? 16 : 11), WALL_Y - 6, (p.isMajor ? 32 : 22), 10,
                  44, 50, 64, 250);
            lineR(r, (int)sx - (p.isMajor ? 18 : 12), WALL_Y + 3,
                  (int)sx + (p.isMajor ? 18 : 12), WALL_Y + 3, 112, 128, 156, 220);
            fillR(r, (int)sx - 7, WALL_Y - 2, 2, 2, 150, 164, 188, 160);
            fillR(r, (int)sx + 5, WALL_Y - 2, 2, 2, 150, 164, 188, 160);

            // 底部三角支撑
            int gs = pw + 4;
            lineR(r, (int)(botX - nx * gs), (int)(botY - ny * gs), (int)botX, (int)botY + 7,
                  88, 92, 108, 220);
            lineR(r, (int)(botX + nx * gs), (int)(botY + ny * gs), (int)botX, (int)botY + 7,
                  88, 92, 108, 220);
            lineR(r, (int)(botX - nx * gs), (int)(botY - ny * gs),
                  (int)(botX + nx * gs), (int)(botY + ny * gs), 82, 86, 102, 210);

            // 主柱：顶部检修灯 + 电缆
            if (p.isMajor) {
                double pulse2 = 0.5 + 0.5 * std::sin(scrollX * 0.05 + p.wx * 0.01);
                fillR(r, (int)(botX + nx * (pw + 4)) - 2, WALL_Y - 92, 4, 4,
                      255, 186, 80, (int)(150 + 90 * pulse2));
                // 柱身上的小型全息标牌
                int sx0 = (int)(botX - nx * (pw + 12)) - 7;
                int sy0 = WALL_Y - 168;
                fillR(r, sx0, sy0, 15, 22, 20, 28, 38, 220);
                boxR(r, sx0, sy0, 15, 22, 110, 210, 238, 140);
                for (int q = 0; q < 3; ++q)
                    lineR(r, sx0 + 3, sy0 + 5 + q * 5, sx0 + 11, sy0 + 5 + q * 5,
                          110, 220, 245, 150);
                // 柱脚警示信标
                fillR(r, (int)sx - 9, WALL_Y - 108, 3, 3, 255, 120, 70,
                      (int)(110 + 90 * pulse2));
                lineR(r, (int)(botX + nx * (pw + 7)), WALL_Y - 20,
                      (int)(topX + nx * (pw + 7)), 30, 100, 112, 136, 160);
                for (int c = 0; c < 4; ++c) {
                    double f = (c + 0.5) / 4.0;
                    int yy = (int)(WALL_Y * (1.0 - f) + 20 * f);
                    int xx = (int)(botX + (topX - botX) * f + nx * (pw + 7));
                    fillR(r, xx, yy, 4, 2, 132, 148, 176, 210);
                }
            }
        }
    }

    // ---------------- 近景栏杆：给廊桥一个前景层 ----------------
    void drawForegroundRail(SDL_Renderer* r) {
        const double par = 1.35;
        const double step = 230.0;
        double base = scrollX * par;
        // 连续的黑色扶手（近景，压住画面下缘）
        fillR(r, 0, 536, WIN_WIDTH, 8, 11, 14, 19, 255);
        lineR(r, 0, 535, WIN_WIDTH, 535, 78, 90, 110, 170);
        lineR(r, 0, 541, WIN_WIDTH, 541, 42, 50, 64, 200);
        lineR(r, 0, 543, WIN_WIDTH, 543, 92, 204, 232, 40);
        double first = std::floor((base - 160.0) / step) * step;
        for (double wx = first; wx < base + WIN_WIDTH + 160.0; wx += step) {
            int sx = (int)(wx - base);
            // 立柱
            fillR(r, sx - 4, 536, 9, WIN_HEIGHT - 536, 9, 12, 17, 255);
            lineR(r, sx - 4, 536, sx - 4, WIN_HEIGHT, 44, 52, 66, 220);
            lineR(r, sx + 4, 536, sx + 4, WIN_HEIGHT, 26, 32, 42, 220);
            lineR(r, sx - 3, 536, sx + 3, 548, 60, 70, 88, 90);
            // 立柱上的小灯
            fillR(r, sx - 1, 556, 3, 3, 120, 220, 245, 90);
            // 扶手接头
            fillR(r, sx - 7, 532, 14, 6, 20, 25, 33, 255);
            lineR(r, sx - 7, 532, sx + 7, 532, 96, 110, 134, 120);
        }
        // 近景底部压暗
        fillR(r, 0, WIN_HEIGHT - 10, WIN_WIDTH, 10, 6, 7, 10, 255);
    }

    // ---------------- 灯光锥 / 尘埃 ----------------
    void drawAmbient(SDL_Renderer* r) {
        forEachLamp([&](double sx, double wx) {
            (void)wx;
            int x = (int)sx;
            // 吊灯向下的柔和光锥（多条低透明度弦线堆出体积）
            for (int k = 1; k <= 13; ++k) {
                double t = k / 13.0;
                int yy = (int)(40 + t * 238);
                int hw = (int)(12 + t * 78);
                int a = (int)(11 * (1.0 - t) + 2);
                lineR(r, x - hw, yy, x + hw, yy, 120, 178, 208, a);
            }
            lineR(r, x, 38, x - 86, WALL_Y, 110, 170, 200, 15);
            lineR(r, x, 38, x + 86, WALL_Y, 110, 170, 200, 15);
        });

        // 廊桥里的悬浮尘埃
        for (int i = 0; i < 40; ++i) {
            double wx = i * 191.0 + hash01(i * 7 + 3) * 140.0;
            double sx = std::fmod(wx - scrollX * 0.72, WIN_WIDTH + 120.0);
            if (sx < -20.0) sx += WIN_WIDTH + 120.0;
            double sy = 36.0 + hash01(i * 13 + 5) * 226.0;
            double tw = 0.35 + 0.65 * std::fabs(std::sin(scrollX * 0.012 + i * 0.7));
            int a = (int)(30 + 62 * tw);
            fillR(r, (int)sx, (int)sy, 1, 1, 190, 224, 246, a);
        }
    }

};
