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
        drawGlassPanels(r);   // 玻璃幕墙是背景面：先画，再叠室内结构
        drawCeiling(r);
        drawFloor(r);
        drawPillars(r);
        drawAmbient(r);
        drawSpeedMotes(r);
        drawForegroundRail(r);
        drawVignette(r);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

    // 第二章门禁场景专用深空背景：不画廊桥室内结构，只保留全屏、
    // 稀疏、慢速的导航星与远处 Moonwell 星体，避免遮住右侧门禁和左侧玩家。
    void drawGateBackdrop(SDL_Renderer* r) const {
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        drawGateSpaceGradient(r);
        drawGateNebula(r);
        drawGatePlanet(r);
        drawGateStars(r);
        drawGateDust(r);
        // 极轻的边缘压暗，不改变星场的全屏覆盖。
        fillR(r, 0, 0, WIN_WIDTH, 3, 0, 0, 0, 70);
        fillR(r, 0, WIN_HEIGHT - 3, WIN_WIDTH, 3, 0, 0, 0, 90);
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
    static void fillEllipseR(SDL_Renderer* r, double x, double y, double rx, double ry,
                             int cr, int cg, int cb, int ca) {
        int y0 = (int)(y - ry), y1 = (int)(y + ry);
        for (int yy = y0; yy <= y1; ++yy) {
            double dy = (yy - y) / ry;
            if (std::fabs(dy) > 1.0) continue;
            double hw = rx * std::sqrt(1.0 - dy * dy);
            fillR(r, (int)(x - hw), yy, (int)(hw * 2), 1, cr, cg, cb, ca);
        }
    }
    static unsigned hashU(unsigned x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
    static double hash01(int x) { return (hashU((unsigned)x) % 4096) / 4096.0; }

    // ---------------- 门禁专用深空背景 ----------------
    void drawGateSpaceGradient(SDL_Renderer* r) const {
        for (int i = 0; i < 24; ++i) {
            int y0 = WIN_HEIGHT * i / 24;
            int y1 = WIN_HEIGHT * (i + 1) / 24;
            int rr = 2 + i * 4 / 23;
            int gg = 5 + i * 7 / 23;
            int bb = 10 + i * 7 / 23;
            fillR(r, 0, y0, WIN_WIDTH, y1 - y0, rr, gg, bb, 255);
        }
    }

    void drawGateNebula(SDL_Renderer* r) const {
        // 大而暗的星云只负责给下半屏一点层次，不产生亮斑。
        fillEllipseR(r, 250, 380, 330, 160, 16, 34, 40, 15);
        fillEllipseR(r, 230, 400, 230, 105, 20, 44, 50, 10);
        fillEllipseR(r, 630, 170, 230, 115, 31, 27, 55, 8);
        fillEllipseR(r, 520, 300, 170, 90, 24, 38, 46, 7);
        fillEllipseR(r, 470, 520, 310, 95, 14, 32, 39, 8);
        for (int i = 0; i < 5; ++i) {
            double y = 300.0 + i * 22.0;
            int a = 6 + i;
            lineR(r, 0, (int)y, WIN_WIDTH, (int)(y + 8), 46, 77, 83, a);
        }
    }

    void drawGatePlanet(SDL_Renderer* r) const {
        const double cx = 205.0, cy = 492.0, rx = 188.0, ry = 142.0;
        fillEllipseR(r, cx, cy, rx + 24, ry + 18, 23, 52, 63, 10);
        fillEllipseR(r, cx, cy, rx, ry, 6, 14, 21, 232);
        fillEllipseR(r, cx - 18, cy - 16, rx - 16, ry - 15, 9, 21, 29, 120);
        // 暗色云带。
        fillEllipseR(r, cx - 4, cy - 48, rx * 0.84, 16, 26, 50, 57, 22);
        fillEllipseR(r, cx - 12, cy - 12, rx * 0.92, 13, 20, 42, 50, 18);
        fillEllipseR(r, cx - 20, cy + 28, rx * 0.78, 18, 24, 48, 55, 16);
        // 右上侧受光边缘。
        int px = 0, py = 0;
        for (int i = 0; i <= 54; ++i) {
            double a = -M_PI * 0.95 + M_PI * 0.90 * i / 54.0;
            int nx = (int)(cx + rx * std::cos(a));
            int ny = (int)(cy + ry * std::sin(a));
            if (i > 0) {
                lineR(r, px, py, nx, ny, 40, 96, 116, 68);
                lineR(r, px + 3, py + 2, nx + 3, ny + 2, 58, 122, 140, 32);
            }
            px = nx; py = ny;
        }
        // 远处 Moonwell 环站：两条极淡的椭圆环，部分穿过星体。
        ellipseR(r, 210, 466, 258, 38, 31, 68, 81, 36);
        ellipseR(r, 210, 466, 274, 43, 24, 53, 66, 21);
        for (int i = 1; i < 6; ++i) {
            if (i == 3) continue;
            double t = i / 6.0;
            int x = (int)(210 + 250 * std::cos(t * M_PI * 2.0));
            int y = (int)(466 + 37 * std::sin(t * M_PI * 2.0));
            fillR(r, x, y, 2, 2, 78, 125, 137, 65);
        }
    }

    void drawGateStars(SDL_Renderer* r) const {
        // 只取已有 400 颗星中的约 1/5；亮度低、闪烁幅度小、无频繁十字星芒。
        for (size_t i = 0; i < stars.size(); ++i) {
            if ((i % 5) != 0) continue;
            const auto& s = stars[i];
            double sx = std::fmod(s.wx - scrollX * 0.012 + 6000.0, WIN_WIDTH + 80.0) - 40.0;
            double sy = s.wy;
            if (sx < -2 || sx > WIN_WIDTH + 2 || sy < 0 || sy > WIN_HEIGHT) continue;
            double tw = 0.76 + 0.24 * std::sin(s.phase * 0.28 + i * 0.91);
            int b = 50 + (int)(tw * 98.0);
            int cr = b, cg = b + 5, cb = b + 14;
            if (cr > 150) cr = 150;
            if (cg > 162) cg = 162;
            if (cb > 180) cb = 180;
            fillR(r, (int)sx, (int)sy, 1, 1, cr, cg, cb, 255);
            if ((i % 97) == 0) {
                int a = 30 + (int)(tw * 24);
                lineR(r, (int)sx - 2, (int)sy, (int)sx + 2, (int)sy, cr, cg, cb, a);
                lineR(r, (int)sx, (int)sy - 2, (int)sx, (int)sy + 2, cr, cg, cb, a);
            }
        }
    }

    void drawGateDust(SDL_Renderer* r) const {
        // 固定哈希微尘：铺满全屏但不闪烁，补足下半屏的深空密度。
        for (int i = 0; i < 108; ++i) {
            int x = (int)(hash01(i * 23 + 5) * WIN_WIDTH);
            int y = (int)(hash01(i * 41 + 11) * WIN_HEIGHT);
            int a = 14 + (int)(hash01(i * 19 + 3) * 18);
            fillR(r, x, y, 1, 1, 66, 89, 101, a);
        }
        for (int i = 0; i < 16; ++i) {
            int x = (int)(hash01(i * 37 + 19) * WIN_WIDTH);
            int y = (int)(hash01(i * 53 + 7) * WIN_HEIGHT);
            fillR(r, x, y, 1, 1, 96, 120, 130, 28);
        }
    }

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
    // 深空由远到近：星云/星系 -> 星场 -> 行星 -> 月球/空间站/碎石/航船 -> 恒星
    void drawSky(SDL_Renderer* r) {
        // 极轻的蓝黑渐变，避免整片纯黑
        for (int i = 0; i < 12; ++i) {
            int y0 = 280 * i / 12, y1 = 280 * (i + 1) / 12;
            int b = 2 + i / 3;
            fillR(r, 0, y0, WIN_WIDTH, y1 - y0, 1, 2, b, 255);
        }

        drawMilkyWay(r);
        drawNebula(r);
        drawDistantGalaxy(r);
        drawStarField(r);
        drawPlanet(r);
        drawMoon(r);
        drawDistantStation(r);
        drawSpaceDebris(r);
        drawTraffic(r);
        drawSun(r);

        // 廊桥尽头的冷光
        for (int k = 0; k < 5; ++k)
            arcR(r, VP_X, 104, 14.0 + k * 16.0, 0, 2.0 * M_PI,
                 60, 120, 150, 10 - k);
    }

    // 银河带：一条很淡的斜向星尘，给深空一个更大的结构
    void drawMilkyWay(SDL_Renderer* r) {
        for (int i = 0; i < 170; ++i) {
            double along = i / 170.0;
            double x = -60.0 + along * 940.0;
            double y = 218.0 - along * 286.0 + (hash01(i * 13 + 5) - 0.5) * 74.0;
            double tw = 0.35 + 0.65 * std::fabs(std::sin(scrollX * 0.005 + i * 0.7));
            int a = (int)(10 + 26 * tw);
            fillR(r, (int)x, (int)y, 1, 1, 128, 148, 188, a);
            if (i % 11 == 0)
                fillR(r, (int)x, (int)y, 1, 1, 190, 208, 238, (int)(40 + 40 * tw));
        }
        // 几条极淡的星尘条纹
        for (int k = 0; k < 5; ++k) {
            double x0 = -40.0 + k * 190.0;
            double y0 = 210.0 - k * 58.0;
            lineR(r, (int)x0, (int)(y0 + 20), (int)(x0 + 90), (int)(y0 - 10),
                  86, 104, 146, 7);
            lineR(r, (int)x0, (int)(y0 + 28), (int)(x0 + 120), (int)(y0 - 2),
                  70, 88, 128, 5);
        }
    }

    // 星云：低透明度长弧 + 星尘团，压在星场后面
    void drawNebula(SDL_Renderer* r) {
        const double nx = 150.0 - scrollX * 0.003;
        const double ny = 92.0;
        for (int k = 0; k < 7; ++k) {
            double ox = std::sin(k * 1.7) * 90.0;
            double oy = std::cos(k * 2.3) * 34.0;
            int a = 18 - k;
            if (a < 6) a = 6;
            arcR(r, nx + ox, ny + oy, 150.0 + k * 34.0,
                 -M_PI * 0.58, -M_PI * 0.08, 52, 78, 132, a);
        }
        // 右侧更淡的一团
        for (int k = 0; k < 6; ++k) {
            arcR(r, 650.0 + std::sin(k * 2.1) * 40.0, 190.0 + k * 8,
                 120.0 + k * 26.0, -M_PI * 0.75, -M_PI * 0.35, 92, 66, 124, 14 - k);
        }
        // 几缕横向星云丝，避免看起来像随机弧线
        for (int k = 0; k < 10; ++k) {
            double x0 = 40.0 + k * 24.0;
            double y0 = 60.0 + std::sin(k * 1.3) * 26.0;
            lineR(r, (int)x0, (int)y0, (int)(x0 + 34 + k * 2), (int)(y0 - 7),
                  70, 96, 150, 7);
        }
        // 星尘团：稀疏点，带轻微闪烁
        for (int i = 0; i < 46; ++i) {
            double sx = 40.0 + hash01(i * 13 + 3) * 260.0 - scrollX * 0.003;
            double sy = 28.0 + hash01(i * 29 + 7) * 118.0;
            if (sx < -30 || sx > WIN_WIDTH + 30) continue;
            double tw = 0.4 + 0.6 * std::fabs(std::sin(scrollX * 0.006 + i * 0.8));
            int b = (int)(70 + 70 * tw);
            fillR(r, (int)sx, (int)sy, 1, 1, b, b + 10, b + 26, (int)(26 + 34 * tw));
        }
    }

    // 远处星系：一个很淡的椭圆核 + 两条旋臂
    void drawDistantGalaxy(SDL_Renderer* r) {
        double gx = 248.0 - scrollX * 0.0015;
        double gy = 54.0;
        if (gx < -80 || gx > WIN_WIDTH + 80) return;
        for (int k = 0; k < 3; ++k) {
            ellipseR(r, gx, gy, 18.0 + k * 9.0, 5.0 + k * 2.5,
                     90, 112, 160, 12 - k * 3);
        }
        arcR(r, gx - 2, gy, 26.0, -0.6, 2.2, 120, 148, 196, 16);
        arcR(r, gx + 2, gy, 21.0, 2.6, 5.2, 110, 136, 184, 12);
        fillR(r, (int)gx - 1, (int)gy - 1, 3, 3, 210, 226, 250, 30);
    }

    // 星场：保留原 400 颗存档星，再叠两层程序化远景星
    void drawStarField(SDL_Renderer* r) {
        // 原星空（含十字芒星）
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
        // 远景细星：极慢视差，填满深空
        for (int i = 0; i < 150; ++i) {
            double wx = i * 83.0 + hash01(i * 7 + 1) * 97.0;
            double sx = std::fmod(wx - scrollX * 0.035, WIN_WIDTH + 220.0);
            if (sx < -110.0) sx += WIN_WIDTH + 220.0;
            if (sx > WIN_WIDTH + 20) continue;
            double sy = 8.0 + hash01(i * 13 + 5) * 258.0;
            double tw = 0.45 + 0.55 * std::fabs(std::sin(scrollX * 0.008 + i * 0.37));
            int b = (int)(70 + 90 * tw);
            fillR(r, (int)sx, (int)sy, 1, 1, b, b, b + 12, (int)(70 + 100 * tw));
        }
        // 第二层稍亮远星
        for (int i = 0; i < 46; ++i) {
            double wx = i * 271.0 + hash01(i * 5 + 9) * 140.0;
            double sx = std::fmod(wx - scrollX * 0.075, WIN_WIDTH + 260.0);
            if (sx < -130.0) sx += WIN_WIDTH + 260.0;
            if (sx > WIN_WIDTH + 20) continue;
            double sy = 14.0 + hash01(i * 17 + 2) * 250.0;
            int b = 120 + (int)(hash01(i * 31 + 4) * 100);
            fillR(r, (int)sx, (int)sy, 1, 1, b, b, b, 180);
        }
        // 两处稀疏星团：打破均匀星点的"撒盐"感
        drawStarCluster(r, 624.0, 176.0, 22, 31);
        drawStarCluster(r, 88.0, 30.0, 16, 71);
        // 少量亮星：十字芒 + 柔晕，带色温
        struct BS { int x, y, size, cr, cg, cb; };
        static const BS bright[7] = {
            { 78, 42, 4, 255, 224, 180}, { 214, 118, 3, 180, 214, 255},
            { 372, 30, 3, 255, 246, 226}, { 498, 168, 3, 190, 220, 255},
            { 612, 72, 4, 255, 230, 190}, { 720, 196, 3, 200, 224, 255},
            { 304, 236, 2, 230, 238, 255}
        };
        for (int i = 0; i < 7; ++i) {
            double tw = 0.55 + 0.45 * std::fabs(std::sin(scrollX * 0.01 + i * 1.4));
            drawBrightStar(r, bright[i].x, bright[i].y, bright[i].size,
                           bright[i].cr, bright[i].cg, bright[i].cb, tw);
        }
    }

    void drawBrightStar(SDL_Renderer* r, int x, int y, int size,
                        int cr, int cg, int cb, double tw) {
        int a = (int)(120 + 100 * tw);
        // 柔晕
        fillR(r, x - 4, y - 4, 9, 9, cr, cg, cb, (int)(10 + 8 * tw));
        fillR(r, x - 2, y - 2, 5, 5, cr, cg, cb, (int)(35 + 20 * tw));
        // 核心
        fillR(r, x, y, 2, 2, cr, cg, cb, 255);
        // 十字与斜向芒
        lineR(r, x - size, y, x + size, y, cr, cg, cb, a);
        lineR(r, x, y - size, x, y + size, cr, cg, cb, a);
        lineR(r, x - size / 2, y - size / 2, x + size / 2, y + size / 2,
              cr, cg, cb, a / 3);
        lineR(r, x - size / 2, y + size / 2, x + size / 2, y - size / 2,
              cr, cg, cb, a / 3);
    }

    // 稀疏星团：一小片更密的星点 + 极淡的团雾
    void drawStarCluster(SDL_Renderer* r, double cx, double cy, int count, int seed) {
        for (int i = 0; i < count; ++i) {
            double sx = cx + (hash01(seed + i * 17) - 0.5) * 118.0;
            double sy = cy + (hash01(seed + i * 29 + 3) - 0.5) * 58.0;
            if (sx < 0 || sx > WIN_WIDTH) continue;
            double tw = 0.4 + 0.6 * std::fabs(std::sin(scrollX * 0.012 + i * 0.9));
            int b = (int)(110 + 110 * tw);
            fillR(r, (int)sx, (int)sy, 1, 1, b, b, 230, (int)(50 + 90 * tw));
        }
        for (int k = 0; k < 2; ++k)
            fillR(r, (int)cx - 34, (int)cy - 18 + k * 12, 68, 7, 70, 104, 160, 4);
    }

    // 巨大行星：实心弧面 + 大气辉光 + 晨昏渐变 + 云带/风暴
    void drawPlanet(SDL_Renderer* r) {
        const double pcx = 20.0 - scrollX * 0.004;
        const double pcy = 640.0;
        const double pr = 480.0;
        const double a0 = -M_PI * 0.80, a1 = -M_PI * 0.20;

        // 1) 实心弧面（遮住背后的星），横向按夜→晨昏→昼做六段渐变
        for (int y = (int)(pcy - pr); y <= WALL_Y; y += 2) {
            double dy = y - pcy;
            double hw = std::sqrt(std::max(0.0, pr * pr - dy * dy));
            int l = (int)std::max(0.0, pcx - hw);
            int rr = (int)std::min((double)WIN_WIDTH, pcx + hw);
            if (rr <= l) continue;
            int span = rr - l;
            for (int s = 0; s < 6; ++s) {
                int x0 = l + span * s / 6;
                int x1 = l + span * (s + 1) / 6;
                if (x1 <= x0) continue;
                int cr = 4 + s * 3, cg = 7 + s * 4, cb = 12 + s * 6;
                fillR(r, x0, y, x1 - x0, 2, cr, cg, cb, 255);
            }
        }

        // 2) 大气外层辉光（昼侧更亮）
        for (int k = 0; k < 9; ++k) {
            double rr = pr + 2 + k * 3.5;
            int a = 26 - k * 2;
            if (a < 4) a = 4;
            arcR(r, pcx, pcy, rr, -M_PI * 0.62, -M_PI * 0.18, 78, 148, 210, a);
        }
        for (int k = 0; k < 3; ++k)
            arcR(r, pcx, pcy, pr + 2 + k * 5.0, -M_PI * 0.80, -M_PI * 0.58,
                 40, 78, 120, 12 - k * 2);

        // 3) 明暗两侧的边缘
        arcR(r, pcx, pcy, pr, a0, a1, 36, 62, 92, 80);
        arcR(r, pcx, pcy, pr, -M_PI * 0.56, -M_PI * 0.18, 158, 212, 250, 165);
        arcR(r, pcx, pcy, pr + 1, -M_PI * 0.56, -M_PI * 0.18, 220, 245, 255, 120);
        arcR(r, pcx, pcy, pr - 1, -M_PI * 0.56, -M_PI * 0.18, 190, 228, 255, 70);
        arcR(r, pcx, pcy, pr, -M_PI * 0.80, -M_PI * 0.57, 24, 44, 70, 95);

        // 4) 云带：极缓慢"自转"，远看只是细弧
        double rot = scrollX * 0.00035;
        for (int k = 0; k < 6; ++k) {
            double rr = pr - 18.0 - k * 27.0;
            double b0 = -M_PI * 0.78 + rot * (0.3 + k * 0.08);
            double b1 = -M_PI * 0.26 + rot * (0.3 + k * 0.08);
            int a = 56 - k * 6;
            if (a < 12) a = 12;
            int cr = 70 + k * 10, cg = 118 + k * 12, cb = 166 + k * 10;
            arcR(r, pcx, pcy, rr, b0, b1, cr, cg, cb, a);
            arcR(r, pcx, pcy, rr - 2.0, b0, b1, cr - 18, cg - 18, cb - 16, a / 2);
        }

        // 5) 云顶高光：只在昼侧，给云带一点厚度
        for (int k = 0; k < 3; ++k) {
            double rr = pr - 30.0 - k * 34.0;
            arcR(r, pcx, pcy, rr, -M_PI * 0.54, -M_PI * 0.28,
                 190, 220, 245, 30);
            arcR(r, pcx, pcy, rr - 1.0, -M_PI * 0.52, -M_PI * 0.30,
                 160, 198, 230, 20);
        }
        // 6) 晨昏线附近的一层暗带
        arcR(r, pcx - 12, pcy, pr - 70.0, -M_PI * 0.74, -M_PI * 0.48,
             18, 34, 56, 100);
        arcR(r, pcx - 4, pcy, pr - 96.0, -M_PI * 0.72, -M_PI * 0.50,
             24, 44, 68, 65);
        arcR(r, pcx + 4, pcy, pr - 56.0, -M_PI * 0.66, -M_PI * 0.46,
             42, 70, 104, 40);

        // 6) 风暴/气旋
        ellipseR(r, pcx + 118, 230, 32, 8, 128, 182, 222, 62);
        ellipseR(r, pcx + 118, 230, 17, 5, 168, 214, 244, 52);
        ellipseR(r, pcx + 118, 230, 7, 2, 215, 240, 255, 40);
        arcR(r, pcx + 100, 230, 24, -0.6, 2.1, 110, 168, 212, 48);
        ellipseR(r, pcx - 48, 252, 20, 6, 96, 150, 198, 45);
        ellipseR(r, pcx - 48, 252, 9, 3, 130, 180, 218, 30);

        // 7) 陆地/浅海暗示：几段不规则短线
        for (int k = 0; k < 5; ++k) {
            double lx = pcx + 10 + k * 38.0;
            double ly = 250.0 + std::sin(k * 1.7) * 12.0;
            lineR(r, (int)lx, (int)ly, (int)(lx + 16), (int)(ly - 4),
                  42, 82, 112, 46);
            lineR(r, (int)(lx + 16), (int)(ly - 4), (int)(lx + 27), (int)ly,
                  42, 82, 112, 36);
            lineR(r, (int)(lx + 4), (int)(ly + 4), (int)(lx + 20), (int)(ly + 1),
                  30, 62, 90, 28);
        }

        // 8) 昼侧最亮的边缘点
        int hx = (int)(pcx + pr * std::cos(-M_PI * 0.36));
        int hy = (int)(pcy + pr * std::sin(-M_PI * 0.36));
        fillR(r, hx - 2, hy - 2, 5, 5, 210, 240, 255, 55);
        fillR(r, hx, hy, 1, 1, 255, 255, 255, 220);
    }

    // 远处小卫星：一弯新月 + 少量环形山
    void drawMoon(SDL_Renderer* r) {
        double mx = 126.0 - scrollX * 0.0025;
        double my = 74.0;
        double mr = 17.0;
        if (mx < -40 || mx > WIN_WIDTH + 40) return;
        for (int k = 0; k < 3; ++k)
            arcR(r, mx, my, mr + 2 + k * 2, 0, 2.0 * M_PI, 90, 104, 126, 10 - k * 3);
        arcR(r, mx, my, mr, 0, 2.0 * M_PI, 116, 132, 156, 55);
        // 新月阴影
        arcR(r, mx + 5, my - 2, mr - 3, -M_PI * 0.7, M_PI * 0.4, 24, 30, 42, 90);
        // 环形山
        ellipseR(r, mx - 5, my - 4, 4, 3, 80, 94, 116, 40);
        ellipseR(r, mx + 4, my + 6, 3, 2, 80, 94, 116, 35);
        fillR(r, (int)(mx + mr * 0.6), (int)(my - mr * 0.7), 1, 1, 220, 230, 245, 120);
    }

    // 远处 Moonwell 空间站：环 + 辐条 + 太阳能板 + 航行灯
    void drawDistantStation(SDL_Renderer* r) {
        double stx = std::fmod(690.0 - scrollX * 0.032 + 1900.0, 1900.0);
        if (stx < -300.0) stx += 1900.0;
        if (stx < -300.0 || stx > WIN_WIDTH + 300.0) return;
        double sy = 138.0;
        double rot = scrollX * 0.0006;

        // 环形骨架
        ellipseR(r, stx, sy, 58, 20, 98, 126, 160, 80);
        ellipseR(r, stx, sy, 70, 25, 78, 102, 134, 52);
        // 辐条
        for (int i = 0; i < 6; ++i) {
            double a = rot + i * M_PI / 3.0;
            int x0 = (int)(stx + std::cos(a) * 20);
            int y0 = (int)(sy + std::sin(a) * 7);
            int x1 = (int)(stx + std::cos(a) * 58);
            int y1 = (int)(sy + std::sin(a) * 20);
            lineR(r, x0, y0, x1, y1, 118, 144, 178, 70);
            fillR(r, x1 - 1, y1 - 1, 3, 3, 100, 210, 238, 90);
        }
        // 中心舱
        fillR(r, (int)stx - 4, (int)sy - 9, 9, 19, 34, 42, 56, 235);
        boxR(r, (int)stx - 4, (int)sy - 9, 9, 19, 150, 170, 198, 150);
        // 天线
        lineR(r, (int)stx, (int)(sy - 20), (int)stx, (int)(sy - 64), 118, 144, 178, 80);
        lineR(r, (int)(stx - 12), (int)(sy - 64), (int)(stx + 12), (int)(sy - 64),
              118, 144, 178, 70);
        // 太阳能板（两侧）
        for (int side = -1; side <= 1; side += 2) {
            int bx = (int)(stx + side * 76);
            fillR(r, bx - 20, (int)sy - 8, 40, 16, 22, 30, 42, 200);
            for (int k = 1; k < 4; ++k)
                lineR(r, bx - 20 + k * 10, (int)sy - 8, bx - 20 + k * 10, (int)sy + 8,
                      78, 104, 136, 70);
            lineR(r, bx - 20, (int)sy, bx + 20, (int)sy, 78, 104, 136, 70);
            lineR(r, bx - 20, (int)sy - 8, bx + 20, (int)sy - 8, 96, 122, 156, 80);
            lineR(r, bx - 20, (int)sy + 8, bx + 20, (int)sy + 8, 96, 122, 156, 80);
        }
        // 对接舱暖光
        fillR(r, (int)stx + 62, (int)sy - 4, 9, 9, 255, 190, 90, 90);
        fillR(r, (int)stx + 65, (int)sy - 1, 3, 3, 255, 230, 170, 210);
        // 航行灯：错相闪烁
        double b1 = std::sin(scrollX * 0.03 + 0.0);
        double b2 = std::sin(scrollX * 0.027 + 2.4);
        if (b1 > 0.15) fillR(r, (int)stx - 2, (int)(sy - 66), 4, 4, 255, 86, 74, 200);
        if (b2 > 0.15) fillR(r, (int)stx + 62, (int)(sy + 6), 3, 3, 110, 228, 255, 170);
        fillR(r, (int)stx - 1, (int)sy - 1, 3, 3, 220, 240, 255, 170);
    }

    // 窗外漂浮碎石：小多边形 + 环形山，慢速视差
    void drawSpaceDebris(SDL_Renderer* r) {
        for (int i = 0; i < 4; ++i) {
            double par = 0.08 + i * 0.035;
            double sx = std::fmod(i * 377.0 + 140.0 - scrollX * par, 1500.0);
            if (sx < -160.0) sx += 1500.0;
            if (sx < -100.0 || sx > WIN_WIDTH + 100.0) continue;
            double sy = 182.0 + i * 24.0 + 6.0 * std::sin(scrollX * 0.003 + i);
            double rad = 7.0 + i * 3.0;
            SDL_Point pts[7];
            for (int k = 0; k < 7; ++k) {
                double a = 2.0 * M_PI * k / 7.0 + i;
                double rr = rad * (0.72 + hash01(i * 17 + k * 5) * 0.5);
                pts[k] = {(int)(sx + std::cos(a) * rr), (int)(sy + std::sin(a) * rr)};
            }
            for (int k = 0; k < 7; ++k)
                lineR(r, pts[k].x, pts[k].y, pts[(k + 1) % 7].x, pts[(k + 1) % 7].y,
                      112, 128, 150, 62);
            ellipseR(r, sx - rad * 0.3, sy - rad * 0.2, rad * 0.28, rad * 0.2,
                     96, 112, 134, 50);
            ellipseR(r, sx + rad * 0.35, sy + rad * 0.3, rad * 0.18, rad * 0.14,
                     96, 112, 134, 40);
        }
    }

    // 远处航船：一个很小的剪影 + 引擎光点
    void drawTraffic(SDL_Renderer* r) {
        double sx = std::fmod(360.0 - scrollX * 0.12, 1600.0);
        if (sx < -240.0) sx += 1600.0;
        if (sx < -100.0 || sx > WIN_WIDTH + 100.0) return;
        double sy = 88.0 + 5.0 * std::sin(scrollX * 0.004);
        // 船体
        lineR(r, (int)sx - 7, (int)sy, (int)sx + 7, (int)sy, 140, 164, 194, 95);
        lineR(r, (int)sx + 7, (int)sy, (int)sx + 2, (int)sy - 3, 140, 164, 194, 80);
        lineR(r, (int)sx + 7, (int)sy, (int)sx + 2, (int)sy + 3, 140, 164, 194, 80);
        fillR(r, (int)sx - 1, (int)sy - 1, 3, 3, 200, 232, 250, 150);
        // 尾焰/航迹
        for (int k = 1; k <= 5; ++k) {
            int a = 50 - k * 8;
            if (a < 6) a = 6;
            fillR(r, (int)sx - 8 - k * 3, (int)sy, 1, 1, 120, 206, 240, a);
        }
        double blink = std::sin(scrollX * 0.05);
        if (blink > 0.3) fillR(r, (int)sx - 8, (int)sy - 1, 2, 2, 255, 110, 90, 190);
    }

    // 远处恒星：给行星昼侧一个明确的光源
    void drawSun(SDL_Renderer* r) {
        int sx = 772, sy = 50;
        for (int k = 5; k >= 1; --k) {
            int s = k * 4;
            fillR(r, sx - s, sy - s, s * 2 + 1, s * 2 + 1, 120, 180, 230, 5 + k);
        }
        fillR(r, sx - 4, sy - 4, 9, 9, 255, 242, 214, 55);
        fillR(r, sx - 2, sy - 2, 5, 5, 255, 252, 238, 200);
        lineR(r, sx - 20, sy, sx + 20, sy, 255, 244, 220, 120);
        lineR(r, sx, sy - 20, sx, sy + 20, 255, 244, 220, 120);
        lineR(r, sx - 11, sy - 11, sx + 11, sy + 11, 255, 244, 220, 36);
        lineR(r, sx - 11, sy + 11, sx + 11, sy - 11, 255, 244, 220, 36);
    }

    // 灯的供电状态：整条廊桥偶尔电压不稳（确定性，读档一致）
    double powerFlicker() const {
        double ph = std::fmod(scrollX, 1200.0);
        if (ph < 6.0) return 0.25;
        if (ph > 510.0 && ph < 515.0) return 0.55;
        return 1.0;
    }
    double lampPulse(double wx) const {
        return 0.74 + 0.26 * std::sin(scrollX * 0.02 + wx * 0.011);
    }
    bool lampFlicker(double wx) const {
        unsigned h = hashU((unsigned)(int)(wx * 0.17));
        if (h % 17 != 0) return false;
        double ph = std::fmod(scrollX * 0.22 + wx * 0.013, 190.0);
        return ph < 7.0;
    }
    bool lampEmergency(double wx) const {
        int idx = (int)std::floor(wx / 240.0);
        idx %= 5;
        if (idx < 0) idx += 5;
        return idx == 0;
    }

    // ---------------- 顶部桁架与吊灯 ----------------
    void drawCeiling(SDL_Renderer* r) {
        // 顶部空间与桁架
        fillR(r, 0, 0, WIN_WIDTH, 30, 15, 18, 24, 255);
        fillR(r, 0, 0, WIN_WIDTH, 7, 11, 14, 19, 255);
        lineR(r, 0, 8, WIN_WIDTH, 8, 124, 140, 166, 205);
        lineR(r, 0, 9, WIN_WIDTH, 9, 42, 50, 64, 230);
        lineR(r, 0, 20, WIN_WIDTH, 20, 108, 124, 148, 195);
        lineR(r, 0, 21, WIN_WIDTH, 21, 38, 46, 60, 230);
        lineR(r, 0, 29, WIN_WIDTH, 29, 74, 86, 108, 165);

        // 纵向桁架：立杆 + 交叉腹杆
        for (double wx = std::floor((scrollX - 80.0) / 64.0) * 64.0;
             wx < scrollX + WIN_WIDTH + 80.0; wx += 64.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, 9, sx, 21, 86, 100, 124, 175);
            lineR(r, sx + 1, 9, sx + 1, 21, 44, 52, 66, 200);
            int idx = (int)std::floor(wx / 64.0);
            if (idx % 2 == 0) {
                lineR(r, sx, 9, sx + 64, 21, 66, 78, 98, 95);
                lineR(r, sx + 64, 9, sx, 21, 66, 78, 98, 80);
            }
            fillR(r, sx - 1, 19, 3, 3, 120, 136, 160, 85);
        }
        // 电缆槽
        lineR(r, 0, 26, WIN_WIDTH, 26, 58, 68, 86, 120);
        lineR(r, 0, 27, WIN_WIDTH, 27, 34, 41, 53, 160);
        for (double wx = std::floor((scrollX - 48.0) / 48.0) * 48.0;
             wx < scrollX + WIN_WIDTH + 48.0; wx += 48.0) {
            int sx = (int)(wx - scrollX);
            fillR(r, sx - 1, 25, 3, 4, 84, 98, 120, 120);
        }

        forEachLamp([&](double sx, double wx) {
            int x = (int)sx;
            double global = powerFlicker();
            double pulse = lampPulse(wx) * global;
            bool flick = lampFlicker(wx);
            if (flick) pulse *= 0.12;
            bool emergency = lampEmergency(wx);

            // 吊架 + 软管
            fillR(r, x - 8, 21, 16, 4, 27, 32, 42, 255);
            boxR(r, x - 8, 21, 16, 4, 118, 134, 158, 150);
            lineR(r, x - 2, 25, x - 2, 32, 100, 114, 138, 200);
            lineR(r, x + 2, 25, x + 2, 32, 64, 76, 96, 180);
            lineR(r, x + 6, 24, x + 4, 28, 70, 80, 98, 110);
            lineR(r, x + 4, 28, x + 6, 31, 70, 80, 98, 110);

            // 灯罩
            fillR(r, x - 21, 31, 42, 13, 34, 40, 52, 255);
            boxR(r, x - 21, 31, 42, 13, 140, 158, 186, 205);
            fillR(r, x - 19, 32, 38, 3, 52, 60, 78, 255);
            for (int g = 0; g < 3; ++g)
                fillR(r, x - 18 + g * 6, 36, 3, 6, 18, 22, 30, 255);
            for (int g = 0; g < 3; ++g)
                fillR(r, x + 8 + g * 3, 36, 2, 6, 18, 22, 30, 255);
            fillR(r, x - 18, 38, 2, 2, 156, 172, 196, 150);
            fillR(r, x + 16, 38, 2, 2, 156, 172, 196, 150);

            // 发光的扩散板（外冷内暖）
            fillR(r, x - 15, 44, 30, 4, 40, 46, 58, 255);
            boxR(r, x - 15, 44, 30, 4, 120, 136, 160, 160);
            if (pulse > 0.02) {
                int a = (int)(215 * pulse);
                fillR(r, x - 14, 45, 28, 2, 188, 238, 255, a);
                fillR(r, x - 11, 45, 22, 2, 244, 250, 255, (int)(a * 0.8));
                fillR(r, x - 4, 45, 8, 2, 255, 244, 218, (int)(a * 0.9));
                fillR(r, x - 26, 43, 52, 12, 118, 198, 234, (int)(24 * pulse));
                fillR(r, x - 20, 46, 40, 8, 150, 220, 248, (int)(20 * pulse));
                fillR(r, x - 18, 26, 36, 2, 110, 190, 230, (int)(24 * pulse));
            }
            // 状态灯
            bool amber = (hashU((unsigned)(int)(wx * 0.7)) % 3 == 0);
            fillR(r, x + 17, 34, 2, 2, amber ? 255 : 120, amber ? 190 : 235,
                  amber ? 80 : 210, 190);

            // 每 5 盏一盏红色应急灯（断电时仍亮）
            if (emergency) {
                double ep = 0.4 + 0.6 * std::fabs(std::sin(scrollX * 0.045 + wx * 0.01));
                fillR(r, x - 19, 28, 6, 4, 255, 70, 58, (int)(120 + 110 * ep));
                fillR(r, x - 24, 25, 16, 9, 255, 60, 50, (int)(24 * ep));
                fillR(r, x - 42, 29, 84, 1, 255, 70, 58, (int)(18 * ep));
            }

            // 部分灯具挂检修牌
            if (hashU((unsigned)(int)(wx * 0.31)) % 4 == 0) {
                lineR(r, x - 14, 44, x - 14, 53, 90, 102, 124, 100);
                fillR(r, x - 17, 53, 7, 5, 26, 31, 41, 200);
                lineR(r, x - 16, 55, x - 12, 55, 120, 136, 160, 100);
            }
        });
    }

    // ---------------- 地面 ----------------
    void drawFloor(SDL_Renderer* r) {
        drawFloorBase(r);
        drawFloorDetails(r);
        drawFloorReflections(r);
        drawFloorPools(r);
        drawFloorHazards(r);
    }

    void drawFloorBase(SDL_Renderer* r) {
        // 由远及近的明度梯度
        const int BANDS = 10;
        for (int i = 0; i < BANDS; ++i) {
            int y0 = WALL_Y + (WIN_HEIGHT - WALL_Y) * i / BANDS;
            int y1 = WALL_Y + (WIN_HEIGHT - WALL_Y) * (i + 1) / BANDS;
            fillR(r, 0, y0, WIN_WIDTH, y1 - y0,
                  31 + i * 2, 35 + i * 2, 45 + i * 2, 255);
        }
        // 墙脚
        fillR(r, 0, WALL_Y - 5, WIN_WIDTH, 5, 30, 35, 45, 255);
        lineR(r, 0, WALL_Y, WIN_WIDTH, WALL_Y, 160, 178, 206, 245);
        lineR(r, 0, WALL_Y + 1, WIN_WIDTH, WALL_Y + 1, 56, 64, 80, 240);

        // 湿地面的冷色反光：贴着墙脚的一道渐隐光晕
        fillR(r, 0, WALL_Y + 3, WIN_WIDTH, 7, 46, 86, 112, 14);
        fillR(r, 0, WALL_Y + 10, WIN_WIDTH, 9, 46, 86, 112, 9);
        fillR(r, 0, WALL_Y + 19, WIN_WIDTH, 13, 46, 86, 112, 5);

        // 墙脚灯带：短促的冷光虚线随廊桥移动
        for (double wx = std::floor((scrollX - 80.0) / 64.0) * 64.0;
             wx < scrollX + WIN_WIDTH + 80.0; wx += 64.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, WALL_Y + 6, sx + 22, WALL_Y + 6, 140, 232, 255, 175);
            lineR(r, sx + 3, WALL_Y + 7, sx + 19, WALL_Y + 7, 210, 248, 255, 100);
        }
        // 墙脚暗色线缆槽 + 卡扣
        lineR(r, 0, WALL_Y + 16, WIN_WIDTH, WALL_Y + 16, 48, 58, 74, 110);
        for (double wx = std::floor((scrollX - 48.0) / 48.0) * 48.0;
             wx < scrollX + WIN_WIDTH + 48.0; wx += 48.0) {
            int sx = (int)(wx - scrollX);
            fillR(r, sx - 1, WALL_Y + 14, 3, 4, 74, 86, 106, 100);
        }

        // 纵向地板缝（保留原系统）
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
            lineR(r, 0, y - 1, WIN_WIDTH, y - 1, 110, 126, 150, 20);
            lineR(r, 0, y, WIN_WIDTH, y, 76, 90, 112, 62 + t * 8);
            lineR(r, 0, y + 1, WIN_WIDTH, y + 1, 12, 15, 21, 40);
        }

        // 横向地板分块
        for (double wx = std::floor((scrollX - 60.0) / 92.0) * 92.0;
             wx < scrollX + WIN_WIDTH + 60.0; wx += 92.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx - 1, WALL_Y + 2, sx - 1, WIN_HEIGHT, 92, 106, 128, 20);
            lineR(r, sx, WALL_Y + 2, sx, WIN_HEIGHT, 62, 74, 94, 40);
            lineR(r, sx + 1, WALL_Y + 2, sx + 1, WIN_HEIGHT, 38, 46, 60, 28);
            lineR(r, sx + 2, WALL_Y + 2, sx + 2, WIN_HEIGHT, 16, 19, 26, 30);
        }

        // 近处压边
        fillR(r, 0, WIN_HEIGHT - 18, WIN_WIDTH, 18, 12, 15, 21, 255);
        lineR(r, 0, WIN_HEIGHT - 18, WIN_WIDTH, WIN_HEIGHT - 18, 92, 108, 132, 190);
        lineR(r, 0, WIN_HEIGHT - 16, WIN_WIDTH, WIN_HEIGHT - 16, 90, 200, 230, 22);
    }

    void drawFloorDetails(SDL_Renderer* r) {
        // 面板交点螺栓：给地面一个"真实尺寸"
        for (double wx = std::floor((scrollX - 60.0) / 92.0) * 92.0;
             wx < scrollX + WIN_WIDTH + 60.0; wx += 92.0) {
            int sx = (int)(wx - scrollX);
            for (int t = 0; t < N_SEAM_TRACKS; ++t) {
                int y = (int)seamTracks[t].y;
                if (y < WALL_Y + 8 || y > WIN_HEIGHT - 24) continue;
                fillR(r, sx - 1, y - 1, 2, 2, 126, 142, 166, 70);
                fillR(r, sx + 1, y + 1, 1, 1, 70, 82, 100, 60);
            }
        }
        // 每 690 单位一个检修舱盖
        for (double wx = std::floor((scrollX - 500.0) / 690.0) * 690.0;
             wx < scrollX + WIN_WIDTH + 400.0; wx += 690.0) {
            int sx = (int)(wx - scrollX);
            int y = WALL_Y + 86;
            if (sx < -70 || sx > WIN_WIDTH + 70) continue;
            fillR(r, sx, y, 48, 26, 25, 30, 39, 200);
            boxR(r, sx, y, 48, 26, 88, 102, 124, 130);
            boxR(r, sx + 3, y + 3, 42, 20, 54, 64, 80, 100);
            lineR(r, sx + 6, y + 6, sx + 42, y + 20, 60, 72, 90, 80);
            fillR(r, sx + 5, y + 5, 2, 2, 130, 146, 170, 100);
            fillR(r, sx + 41, y + 5, 2, 2, 130, 146, 170, 100);
            fillR(r, sx + 5, y + 19, 2, 2, 130, 146, 170, 100);
            fillR(r, sx + 41, y + 19, 2, 2, 130, 146, 170, 100);
        }
        // 近处地面跑道灯：给前景一点实际光源
        for (double wx = std::floor((scrollX - 60.0) / 110.0) * 110.0;
             wx < scrollX + WIN_WIDTH + 60.0; wx += 110.0) {
            int sx = (int)(wx - scrollX);
            int y = WIN_HEIGHT - 84;
            fillR(r, sx - 4, y - 3, 9, 7, 90, 180, 215, 14);
            fillR(r, sx - 1, y - 1, 4, 3, 140, 226, 248, 80);
            lineR(r, sx - 2, y + 5, sx + 3, y + 5, 80, 150, 176, 30);
        }
        // 地面导向箭头
        for (double wx = std::floor((scrollX - 80.0) / 320.0) * 320.0;
             wx < scrollX + WIN_WIDTH + 80.0; wx += 320.0) {
            int sx = (int)(wx - scrollX);
            int y = WALL_Y + 52;
            lineR(r, sx, y, sx + 9, y - 5, 96, 128, 150, 48);
            lineR(r, sx, y, sx + 9, y + 5, 96, 128, 150, 48);
            lineR(r, sx + 2, y, sx + 11, y - 5, 70, 96, 116, 34);
            lineR(r, sx + 2, y, sx + 11, y + 5, 70, 96, 116, 34);
        }
    }

    void drawFloorReflections(SDL_Renderer* r) {
        // 立柱倒影：分段+波纹偏移，像潮湿金属地面上的倒影
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -60 || sx > WIN_WIDTH + 60) continue;
            double topX = VP_X + (sx - VP_X) * 0.85;
            int pw = p.isMajor ? 6 : 3;
            for (int k = -1; k <= 1; ++k) {
                int x0 = (int)(sx + k * pw);
                for (int yy = WALL_Y + 2; yy < WALL_Y + 142; yy += 6) {
                    double t = (yy - WALL_Y) / 142.0;
                    int ox = (int)(std::sin(scrollX * 0.07 + yy * 0.12 + p.wx * 0.01) * 2.4 * t);
                    int xa = (int)(x0 + (sx - topX) * 0.18 * t + ox);
                    int a = (int)(66 * (1.0 - t) * (k == 0 ? 1.0 : 0.7));
                    if (a < 3) continue;
                    lineR(r, xa, yy, xa, yy + 4, 118, 158, 192, a);
                }
            }
            // 柱脚接触高光 + 地面冷光晕
            fillEllipseR(r, sx, WALL_Y + 6, 26, 5, 60, 114, 144, 12);
            fillR(r, (int)sx - pw - 2, WALL_Y + 2, pw * 2 + 5, 2, 150, 214, 238, 90);
            lineR(r, (int)sx, WALL_Y + 4, (int)(sx + (sx - topX) * 0.16), WALL_Y + 110,
                  120, 156, 190, 16);
            // 柱身灯带的倒影
            double pulse = 0.55 + 0.45 * std::sin(scrollX * 0.03 + p.wx * 0.02);
            for (int k = 0; k < 3; ++k)
                fillR(r, (int)sx + pw + 1, WALL_Y + 10 + k * 20, 2, 2,
                      130, 220, 248, (int)(50 + 70 * pulse));
            // 主柱应急灯的暖色倒影
            if (p.isMajor) {
                double ep = 0.5 + 0.5 * std::sin(scrollX * 0.05 + p.wx * 0.01);
                fillR(r, (int)sx + 2, WALL_Y + 8, 2, 34, 255, 150, 70, (int)(20 + 30 * ep));
            }
            // 玻璃边框在墙脚的短倒影
            lineR(r, (int)sx - pw - 4, WALL_Y + 4, (int)sx - pw - 4, WALL_Y + 20,
                  100, 138, 166, 24);
            lineR(r, (int)sx + pw + 4, WALL_Y + 4, (int)sx + pw + 4, WALL_Y + 20,
                  100, 138, 166, 24);
        }

        // 吊灯的竖向反光（拉长、带波纹）
        forEachLamp([&](double sx, double wx) {
            int x = (int)sx;
            double p = lampPulse(wx) * powerFlicker();
            if (lampFlicker(wx)) p *= 0.15;
            if (p < 0.05) return;
            for (int k = 0; k < 5; ++k) {
                int yy = WALL_Y + 3 + k * 13;
                int w = 2 + k * 2;
                int ox = (int)(std::sin(scrollX * 0.09 + k * 1.7 + wx * 0.01) * 1.5);
                lineR(r, x - w + ox, yy, x + w + ox, yy, 140, 212, 238,
                      (int)((26 - k * 4) * p));
            }
            lineR(r, x, WALL_Y + 2, x, WALL_Y + 150, 170, 226, 248, (int)(18 * p));
            lineR(r, x - 1, WALL_Y + 30, x - 1, WALL_Y + 110, 130, 190, 220, (int)(10 * p));
        });
    }

    void drawFloorPools(SDL_Renderer* r) {
        forEachLamp([&](double sx, double wx) {
            int x = (int)sx;
            double p = lampPulse(wx) * powerFlicker();
            if (lampFlicker(wx)) p *= 0.12;
            if (p < 0.04) return;
            // 大范围柔和光晕（照亮脚下一整片湿地）
            fillEllipseR(r, x, WALL_Y + 66, 150, 42, 38, 88, 110, (int)(8 * p));
            fillEllipseR(r, x, WALL_Y + 56, 108, 28, 48, 108, 134, (int)(10 * p));
            // 核心光池：填充 + 外圈 + 高光
            fillEllipseR(r, x, WALL_Y + 40, 82, 16, 40, 98, 122, (int)(15 * p));
            fillEllipseR(r, x, WALL_Y + 37, 56, 10, 70, 152, 182, (int)(24 * p));
            fillEllipseR(r, x, WALL_Y + 34, 30, 5, 128, 216, 242, (int)(32 * p));
            ellipseR(r, x, WALL_Y + 36, 58, 9, 120, 208, 238, (int)(48 * p));
            ellipseR(r, x, WALL_Y + 47, 44, 7, 96, 176, 206, (int)(24 * p));
            // 池面波纹
            for (int k = 0; k < 3; ++k) {
                int yy = WALL_Y + 33 + k * 7;
                int hw = 20 + k * 12;
                lineR(r, x - hw, yy, x + hw, yy, 130, 206, 230, (int)(14 * p));
            }
            // 从灯下向画面近处铺开的光楔（湿地面的长条反光）
            fillEllipseR(r, x, WALL_Y + 112, 138, 46, 32, 74, 96, 4);
            for (int yy = WALL_Y + 4; yy < WIN_HEIGHT - 22; yy += 6) {
                double t = (yy - WALL_Y) / (double)(WIN_HEIGHT - WALL_Y);
                int hw = (int)(38 + t * 96);
                int a = (int)(9.0 * (1.0 - t) * p) + 2;
                lineR(r, x - hw, yy, x + hw, yy, 60, 118, 146, a);
            }
            // 灯下最亮的接触点
            fillR(r, x - 3, WALL_Y + 3, 7, 2, 210, 244, 255, (int)(120 * p));
        });
    }

    void drawFloorHazards(SDL_Renderer* r) {
        // 立柱底部警示斜纹（加一层暗底，避免贴图感）
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -40 || sx > WIN_WIDTH + 40) continue;
            fillR(r, (int)sx - 22, WALL_Y + 4, 44, 10, 20, 20, 18, 120);
            for (int k = 0; k < 4; ++k) {
                int bx = (int)sx - 20 + k * 11;
                lineR(r, bx, WALL_Y + 5, bx + 7, WALL_Y + 12, 232, 170, 62, 110);
            }
        }
        // 地面中央的导向虚线（比墙脚更明亮）
        for (double wx = std::floor((scrollX - 40.0) / 36.0) * 36.0;
             wx < scrollX + WIN_WIDTH + 40.0; wx += 36.0) {
            int sx = (int)(wx - scrollX);
            lineR(r, sx, WALL_Y + 24, sx + 16, WALL_Y + 24, 110, 200, 226, 28);
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

            // 玻璃本身的冷色薄雾：越靠近底部越明显，让幕墙有"实体感"
            for (int yy = (int)glassT; yy < (int)glassB; yy += 3) {
                double t = (yy - glassT) / (glassB - glassT);
                int lx = (int)(ltx + (lbx - ltx) * t);
                int rx = (int)(rtx + (rbx - rtx) * t);
                int a = 4 + (int)(9 * t);
                fillR(r, lx + 1, yy, rx - lx - 2, 3, 42, 74, 108, a);
            }
            // 灯在玻璃上的竖向反光
            forEachLamp([&](double lsx, double lwx) {
                (void)lwx;
                if (lsx < lbx || lsx > rbx) return;
                int lx = (int)lsx;
                lineR(r, lx - 2, (int)glassT + 26, lx - 2, (int)glassB - 24,
                      120, 170, 204, 18);
                lineR(r, lx, (int)glassT + 20, lx, (int)glassB - 18,
                      170, 216, 244, 26);
                lineR(r, lx + 2, (int)glassT + 34, lx + 2, (int)glassB - 30,
                      120, 170, 204, 8);
            });
            // 底部冷凝水痕
            for (int k = 0; k < 7; ++k) {
                int hx = (int)(lbx + 12 + hash01((int)(i * 19 + k * 11)) * (rbx - lbx - 24));
                int hy = (int)(glassB - 8 - hash01((int)(i * 23 + k * 7)) * 44);
                fillR(r, hx, hy, 1, 1, 200, 226, 246, 40);
            }

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
            if (sx < -140 || sx > WIN_WIDTH + 140) continue;

            const double botX = sx;
            const double botY = WALL_Y;
            const double topX = VP_X + (botX - VP_X) * 0.85;
            const double topY = 0;
            const int pw = p.isMajor ? 8 : 4;
            double dx = topX - botX, dy = topY - botY;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0) continue;
            double nx = -dy / len, ny = dx / len;

            // 实心柱身：先填暗色，挡住背后的玻璃/行星，再画边缘
            for (int yy = 0; yy < (int)botY; yy += 3) {
                double f = (botY - yy) / botY;
                int cx = (int)(botX + (topX - botX) * f);
                fillR(r, cx - pw, yy, pw * 2 + 1, 3, 30, 34, 43, 255);
            }
            // 左右受光边
            lineR(r, (int)(botX - nx * pw), (int)(botY - ny * pw),
                  (int)(topX - nx * pw), (int)(topY - ny * pw), 138, 156, 188, 235);
            lineR(r, (int)(botX - nx * (pw + 1)), (int)(botY - ny * (pw + 1)),
                  (int)(topX - nx * (pw + 1)), (int)(topY - ny * (pw + 1)), 70, 82, 102, 220);
            lineR(r, (int)(botX + nx * pw), (int)(botY + ny * pw),
                  (int)(topX + nx * pw), (int)(topY + ny * pw), 104, 118, 146, 230);
            lineR(r, (int)(botX + nx * (pw + 1)), (int)(botY + ny * (pw + 1)),
                  (int)(topX + nx * (pw + 1)), (int)(topY + ny * (pw + 1)), 42, 50, 64, 230);
            // 中央凹槽
            lineR(r, (int)botX, (int)botY, (int)topX, (int)topY, 46, 52, 66, 160);

            // 横向分板 + 铆钉
            for (int k = 1; k <= 4; ++k) {
                double f = k / 5.0;
                int y = (int)(botY - botY * f);
                int cx = (int)(botX + (topX - botX) * f);
                lineR(r, cx - pw, y, cx + pw, y, 70, 80, 98, 120);
                lineR(r, cx - pw, y + 1, cx + pw, y + 1, 22, 26, 34, 150);
                fillR(r, cx - pw + 1, y + 3, 2, 2, 132, 148, 172, 110);
                fillR(r, cx + pw - 2, y + 3, 2, 2, 132, 148, 172, 110);
            }

            // 柱身灯带：分段亮起，带一点发光晕
            double pulse = 0.55 + 0.45 * std::sin(scrollX * 0.03 + p.wx * 0.02);
            int seg = (int)((botY - 16) / 26);
            for (int s = 0; s < seg; ++s) {
                double f = (s + 0.5) / seg;
                int yy = (int)(botY + (topY - botY) * f);
                int xx = (int)(botX + (topX - botX) * f + nx * (pw + 1));
                if (s % 3 == 0) {
                    int a = (int)(130 + 90 * pulse);
                    fillR(r, xx - 2, yy - 1, 6, 3, 70, 170, 200, 22);
                    lineR(r, xx - 1, yy, xx + 2, yy, 120, 228, 255, a);
                }
            }

            // 顶部柱帽
            {
                int cx = (int)topX;
                fillR(r, cx - pw - 5, 11, pw * 2 + 11, 9, 34, 39, 51, 255);
                boxR(r, cx - pw - 5, 11, pw * 2 + 11, 9, 132, 148, 172, 160);
                fillR(r, cx - pw - 2, 14, 2, 2, 148, 164, 188, 120);
                fillR(r, cx + pw, 14, 2, 2, 148, 164, 188, 120);
            }

            // 柱脚底座 + 地脚螺栓
            fillR(r, (int)sx - (p.isMajor ? 20 : 14), WALL_Y - 7,
                  (p.isMajor ? 40 : 28), 11, 44, 50, 64, 250);
            boxR(r, (int)sx - (p.isMajor ? 20 : 14), WALL_Y - 7,
                 (p.isMajor ? 40 : 28), 11, 128, 144, 170, 180);
            lineR(r, (int)sx - (p.isMajor ? 22 : 15), WALL_Y + 4,
                  (int)sx + (p.isMajor ? 22 : 15), WALL_Y + 4, 112, 128, 156, 220);
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

            // 柱身线缆（每根柱都有）
            {
                int ox = (int)(nx * (pw + 4));
                lineR(r, (int)botX + ox, WALL_Y - 12, (int)topX + ox, 24,
                      72, 82, 100, 120);
                for (int c = 0; c < 5; ++c) {
                    double f = (c + 0.5) / 5.0;
                    int yy = (int)(WALL_Y * (1.0 - f) + 24 * f);
                    int xx = (int)(botX + (topX - botX) * f) + ox;
                    fillR(r, xx - 1, yy, 4, 2, 104, 118, 140, 120);
                }
            }

            // 主柱：检修灯、全息标牌、维护梯
            if (p.isMajor) {
                double pulse2 = 0.5 + 0.5 * std::sin(scrollX * 0.05 + p.wx * 0.01);
                fillR(r, (int)(botX + nx * (pw + 4)) - 2, WALL_Y - 92, 4, 4,
                      255, 186, 80, (int)(150 + 90 * pulse2));
                fillR(r, (int)(botX + nx * (pw + 4)) - 5, WALL_Y - 95, 10, 10,
                      255, 150, 70, (int)(22 * pulse2));
                // 全息标牌
                int sx0 = (int)(botX - nx * (pw + 12)) - 7;
                int sy0 = WALL_Y - 168;
                fillR(r, sx0, sy0, 15, 22, 20, 28, 38, 220);
                boxR(r, sx0, sy0, 15, 22, 110, 210, 238, 140);
                for (int q = 0; q < 3; ++q)
                    lineR(r, sx0 + 3, sy0 + 5 + q * 5, sx0 + 11, sy0 + 5 + q * 5,
                          110, 220, 245, 150);
                // 维护梯
                int lx = (int)(botX + nx * (pw + 5)) + 2;
                int lx2 = lx + 7;
                lineR(r, lx, WALL_Y - 210, lx, WALL_Y - 62, 82, 96, 118, 90);
                lineR(r, lx2, WALL_Y - 210, lx2, WALL_Y - 62, 82, 96, 118, 90);
                for (int rung = 0; rung < 12; ++rung)
                    lineR(r, lx, WALL_Y - 204 + rung * 12, lx2, WALL_Y - 204 + rung * 12,
                          72, 84, 104, 70);
            } else {
                // 次要柱：小型状态面板
                int sx0 = (int)(botX - nx * (pw + 9)) - 4;
                int sy0 = WALL_Y - 132;
                fillR(r, sx0, sy0, 9, 15, 22, 29, 39, 180);
                boxR(r, sx0, sy0, 9, 15, 92, 160, 182, 100);
                for (int q = 0; q < 2; ++q)
                    lineR(r, sx0 + 2, sy0 + 4 + q * 5, sx0 + 7, sy0 + 4 + q * 5,
                          92, 190, 214, 110);
            }
        }
    }

    // ---------------- 漂浮尘埃 / 速度光点 ----------------
    // 全部由已存档的 scrollX 推导，读档后位置一致；作为廊桥高速移动的
    // 微弱视觉证据，不参与任何碰撞或生成逻辑。
    void drawSpeedMotes(SDL_Renderer* r) {
        const double W = WIN_WIDTH + 80.0;
        for (int i = 0; i < 52; ++i) {
            double depth = 0.15 + 0.85 * hash01(i * 13 + 1);
            double speed = 0.06 + depth * 0.38;
            double base = hash01(i * 7 + 3) * W;
            double x = base - std::fmod(scrollX * speed, W);
            if (x < -40.0) x += W;
            double y = 82.0 + hash01(i * 11 + 7) * 430.0;
            int len = 1 + (int)(depth * 11.0);
            int a = 18 + (int)(depth * 52.0);
            if (i % 9 == 0)
                lineR(r, (int)x, (int)y, (int)(x + len), (int)y, 162, 218, 236, a);
            else if (i % 4 == 0)
                lineR(r, (int)x, (int)y, (int)(x + len), (int)y, 118, 194, 154, a / 2);
            else
                fillR(r, (int)x, (int)y, 1, 1, 116, 166, 182, a);
        }
        // 贴近地面的暖色微粒，与冷色玻璃形成一点点色彩平衡。
        for (int i = 0; i < 14; ++i) {
            double base = hash01(i * 31 + 17) * W;
            double x = base - std::fmod(scrollX * (0.4 + hash01(i + 5) * 0.5), W);
            if (x < -20.0) x += W;
            double y = 300.0 + hash01(i * 23 + 9) * 220.0;
            fillR(r, (int)x, (int)y, 1, 1, 226, 176, 96, 28 + (i % 5) * 8);
        }
    }

    // ---------------- 近景栏杆：给廊桥一个前景层 ----------------
    void drawForegroundRail(SDL_Renderer* r) {
        const double par = 1.35;
        const double step = 230.0;
        double base = scrollX * par;
        // 连续的黑色扶手（近景，压住画面下缘）
        fillR(r, 0, 536, WIN_WIDTH, 8, 11, 14, 19, 255);
        lineR(r, 0, 535, WIN_WIDTH, 535, 112, 128, 154, 200);
        lineR(r, 0, 541, WIN_WIDTH, 541, 42, 50, 64, 200);
        lineR(r, 0, 543, WIN_WIDTH, 543, 92, 204, 232, 90);
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
            // 两道立柱之间的链索（轻微下垂）
            int nx2 = sx + (int)step;
            lineR(r, sx + 5, 550, sx + (int)step / 2, 558, 74, 86, 104, 110);
            lineR(r, sx + (int)step / 2, 558, nx2 - 5, 550, 74, 86, 104, 110);
            // 警示标签
            if (((int)std::floor(wx / step)) % 3 == 0) {
                fillR(r, sx + 7, 546, 9, 12, 34, 40, 52, 190);
                lineR(r, sx + 9, 549, sx + 14, 549, 200, 160, 70, 120);
                lineR(r, sx + 9, 552, sx + 14, 552, 200, 160, 70, 90);
            }
        }
        // 近景底部压暗
        fillR(r, 0, WIN_HEIGHT - 10, WIN_WIDTH, 10, 6, 7, 10, 255);
    }

    // ---------------- 暗角：把视线收进廊桥中心 ----------------
    void drawVignette(SDL_Renderer* r) {
        for (int i = 0; i < 12; ++i) {
            int inset = i * 3;
            int a = 4 + i;
            boxR(r, inset, inset, WIN_WIDTH - 2 * inset, WIN_HEIGHT - 2 * inset,
                 0, 0, 0, a);
        }
        // 顶部和底部略深，压住画面边缘
        fillR(r, 0, 0, WIN_WIDTH, 2, 0, 0, 0, 90);
        fillR(r, 0, WIN_HEIGHT - 2, WIN_WIDTH, 2, 0, 0, 0, 90);
    }

    // ---------------- 灯光锥 / 尘埃 / 电火花 ----------------
    void drawAmbient(SDL_Renderer* r) {
        double global = powerFlicker();
        forEachLamp([&](double sx, double wx) {
            int x = (int)sx;
            bool flick = lampFlicker(wx);
            double p = lampPulse(wx) * global * (flick ? 0.10 : 1.0);
            if (p < 0.03) return;

            // 体积光锥：由窄到宽的低透明度弦线堆出空气感
            for (int k = 1; k <= 18; ++k) {
                double t = k / 18.0;
                int yy = (int)(50 + t * 228);
                int hw = (int)(14 + t * 92);
                int a = (int)(20 * p * (1.0 - t) + 3);
                lineR(r, x - hw, yy, x + hw, yy, 118, 176, 206, a);
            }
            lineR(r, x, 52, x - 104, WALL_Y, 110, 170, 200, (int)(30 * p));
            lineR(r, x, 52, x + 104, WALL_Y, 110, 170, 200, (int)(30 * p));
            lineR(r, x, 52, x, WALL_Y, 168, 220, 244, (int)(22 * p));
            lineR(r, x - 1, 60, x - 1, 260, 130, 186, 214, (int)(8 * p));
            lineR(r, x + 1, 60, x + 1, 260, 130, 186, 214, (int)(8 * p));

            // 光锥里缓慢漂浮的尘埃
            for (int m = 0; m < 6; ++m) {
                double t = std::fmod(scrollX * 0.003 + hash01((int)(wx * 0.7) + m * 13), 1.0);
                int yy = 50 + (int)(t * 225);
                int hw = (int)(10 + t * 80);
                int dx = (int)(hash01((int)(wx * 0.9) + m * 17) * 2 * hw) - hw;
                int a = (int)((18 + 30 * hash01(m * 7 + 3)) * p);
                fillR(r, x + dx, yy, 1, 1, 192, 232, 252, a);
            }

            // 应急灯的红光在玻璃/空气里的一点溢出
            if (lampEmergency(wx)) {
                double ep = 0.4 + 0.6 * std::fabs(std::sin(scrollX * 0.045 + wx * 0.01));
                fillR(r, x - 30, 50, 60, 52, 255, 58, 50, (int)(3 * ep));
            }
        });

        // 廊桥里的悬浮尘埃：远近两层
        for (int i = 0; i < 52; ++i) {
            double wx = i * 151.0 + hash01(i * 7 + 3) * 130.0;
            double sx = std::fmod(wx - scrollX * (0.55 + (i % 3) * 0.12), WIN_WIDTH + 120.0);
            if (sx < -20.0) sx += WIN_WIDTH + 120.0;
            double sy = 36.0 + hash01(i * 13 + 5) * 230.0;
            double tw = 0.35 + 0.65 * std::fabs(std::sin(scrollX * 0.012 + i * 0.7));
            int a = (int)(24 + 58 * tw);
            fillR(r, (int)sx, (int)sy, 1, 1, 190, 224, 246, a);
            if (i % 9 == 0) fillR(r, (int)sx, (int)sy, 2, 1, 176, 214, 238, a / 2);
        }

        // 主柱偶尔的电火花：给"年久失修"一点生命
        for (const auto& p : pillars) {
            if (!p.isMajor) continue;
            double sx = p.wx - scrollX;
            if (sx < 20 || sx > WIN_WIDTH - 20) continue;
            double ph = std::fmod(scrollX * 0.021 + p.wx * 0.017, 9.0);
            if (ph < 0.10) {
                int x = (int)sx - 9;
                int y = WALL_Y - 152;
                lineR(r, x, y, x - 4, y + 7, 255, 206, 140, 200);
                lineR(r, x, y, x + 3, y + 9, 255, 224, 170, 160);
                lineR(r, x - 3, y + 4, x + 5, y + 12, 255, 190, 120, 120);
            }
        }
    }

};
