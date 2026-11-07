#pragma once

#include "../constants.h"
#include "../renderer.h"
#include <algorithm>
#include <cmath>

// ============== Ch2LabScene (central research lab — NightElf pedestal room) ==============
// 完整场景（不再叠在廊桥背景上）：
//   - 圆形研究大厅：上半部是巨大的环形观景窗，下半部是环形地板
//   - 左侧气闸门是玩家飞入的入口，右侧是 Warden 后来封死的出口
//   - 中央是暗夜精灵原型机的全息基座
// 进入时按 timer 做一次"通电"：墙灯逐段亮起、全息环展开，正好覆盖转场飞入。
// 动画全部由已入档的 timer 推导，不改变存档布局。
class Ch2LabScene {
public:
    double cx, cy, radius;
    int timer;

    // Pedestal / NightElf parking spot (Game aligns the parked plane here)
    static const int PARK_X = 430;
    static const int PARK_Y = 300;

    Ch2LabScene() { reset(); }

    void reset() { cx = PARK_X; cy = PARK_Y; radius = 250.0; timer = 0; }
    void update() { timer++; }

    // 存档：研究室计时（房间几何是常量，只需存 timer 保证动画帧一致）
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(cx); ar.ioNum(cy); ar.ioNum(radius); ar.ioNum(timer);
    }

    void draw(SDL_Renderer* r) const {
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        drawBackdrop(r);
        drawWindow(r);
        drawRoomShell(r);
        drawFloor(r);
        drawPortal(r);
        drawExitDoor(r);
        drawPedestal(r);
        drawHolo(r);
        drawAmbience(r);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

private:
    // ---------------- small helpers ----------------
    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static unsigned hashU(unsigned x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
    static double hash01(int x) { return (hashU((unsigned)x) % 4096) / 4096.0; }
    static void fill(SDL_Renderer* r, int x, int y, int w, int h,
                     int cr, int cg, int cb, int ca = 255) {
        if (w <= 0 || h <= 0) return;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderFillRect(r, &rr);
    }
    static void line(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                     int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void box(SDL_Renderer* r, int x, int y, int w, int h,
                    int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderDrawRect(r, &rr);
    }
    static void arc(SDL_Renderer* r, double x, double y, double rad,
                    double a0, double a1, int cr, int cg, int cb, int ca) {
        int steps = (int)(std::fabs(a1 - a0) * rad / 3.0);
        if (steps < 3) steps = 3;
        if (steps > 128) steps = 128;
        int px = (int)(x + rad * std::cos(a0));
        int py = (int)(y + rad * std::sin(a0));
        for (int i = 1; i <= steps; ++i) {
            double a = a0 + (a1 - a0) * i / steps;
            int nx = (int)(x + rad * std::cos(a));
            int ny = (int)(y + rad * std::sin(a));
            if (i < steps) line(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static void ellipse(SDL_Renderer* r, double x, double y, double rx, double ry,
                        int cr, int cg, int cb, int ca) {
        const int SEG = 48;
        int px = (int)(x + rx), py = (int)y;
        for (int i = 1; i <= SEG; ++i) {
            double a = 2.0 * M_PI * i / SEG;
            int nx = (int)(x + rx * std::cos(a));
            int ny = (int)(y + ry * std::sin(a));
            line(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static void fillEllipse(SDL_Renderer* r, double x, double y, double rx, double ry,
                            int cr, int cg, int cb, int ca) {
        int y0 = (int)(y - ry), y1 = (int)(y + ry);
        for (int yy = y0; yy <= y1; ++yy) {
            double dy = (yy - y) / ry;
            if (std::fabs(dy) > 1.0) continue;
            double hw = rx * std::sqrt(1.0 - dy * dy);
            fill(r, (int)(x - hw), yy, (int)(hw * 2), 1, cr, cg, cb, ca);
        }
    }

    // ---------------- backdrop：房间外的暗色结构 ----------------
    void drawBackdrop(SDL_Renderer* r) const {
        fill(r, 0, 0, WIN_WIDTH, WIN_HEIGHT, 4, 5, 8, 255);
        // 屏幕边缘的暗色立柱，避免圆厅"浮"在黑底上
        for (int i = 0; i < 9; ++i) {
            int x = 30 + i * 96;
            line(r, x, 0, x, WIN_HEIGHT, 11, 14, 19, 255);
            line(r, x + 1, 0, x + 1, WIN_HEIGHT, 7, 9, 12, 255);
        }
        // 从圆厅外环放射出去的支撑梁
        for (int i = 0; i < 8; ++i) {
            double a = 2.0 * M_PI * i / 8.0 + M_PI / 8.0;
            int x0 = (int)(cx + std::cos(a) * (radius + 6));
            int y0 = (int)(cy + std::sin(a) * (radius + 6));
            int x1 = (int)(cx + std::cos(a) * 620.0);
            int y1 = (int)(cy + std::sin(a) * 620.0);
            line(r, x0, y0, x1, y1, 15, 19, 26, 255);
            line(r, x0 + 1, y0 + 1, x1 + 1, y1 + 1, 8, 10, 14, 255);
        }
        // 外环的冷光晕
        for (int k = 0; k < 3; ++k)
            arc(r, cx, cy, radius + 8 + k * 6, 0, 2.0 * M_PI, 40, 62, 80, 18 - k * 4);
    }

    // ---------------- 环形观景窗 + 星空 ----------------
    void drawWindow(SDL_Renderer* r) const {
        const double wx = cx, wy = cy - 20.0;
        const double wr = 200.0;

        // 玻璃底色（整圆，随后地板会盖住下半）
        for (int y = (int)(wy - wr); y <= (int)(wy + wr); ++y) {
            double dy = y - wy;
            double hw = std::sqrt(std::max(0.0, wr * wr - dy * dy));
            fill(r, (int)(wx - hw), y, (int)(hw * 2), 1, 5, 7, 12, 255);
        }
        // 窗内星空：只画在地板线以上
        for (int i = 0; i < 130; ++i) {
            double sx = wx - wr + hash01(i * 3 + 1) * 2.0 * wr;
            double sy = wy - wr + hash01(i * 7 + 5) * 2.0 * wr;
            double dx = sx - wx, dy = sy - wy;
            if (dx * dx + dy * dy > (wr - 8) * (wr - 8)) continue;
            if (sy > cy - 5) continue;
            double tw = 0.35 + 0.65 * std::fabs(std::sin(timer * 0.018 + i * 0.9));
            int b = (int)(95 + 150 * tw);
            if (b > 255) b = 255;
            fill(r, (int)sx, (int)sy, 1, 1, b, b, b, 255);
            if (i % 19 == 0) {
                line(r, (int)sx - 3, (int)sy, (int)sx + 3, (int)sy, b, b, b, 255);
                line(r, (int)sx, (int)sy - 3, (int)sx, (int)sy + 3, b, b, b, 255);
            }
        }
        // 窗外远处的行星边缘（被窗户裁掉一部分）
        arc(r, cx + 96, cy + 286, 300, -M_PI * 0.86, -M_PI * 0.40, 84, 128, 174, 105);
        arc(r, cx + 96, cy + 288, 301, -M_PI * 0.84, -M_PI * 0.42, 140, 188, 230, 80);
        for (int k = 1; k <= 3; ++k)
            arc(r, cx + 96, cy + 286 + k * 5, 292 - k * 15,
                -M_PI * 0.80, -M_PI * 0.46, 100, 142, 184, 40);

        // 窗框：内圈 + 外圈 + 放射状窗棂
        for (int pass = 0; pass < 2; ++pass) {
            int rr = (int)(wr + 2 + pass * 3);
            arc(r, wx, wy, rr, 0, 2.0 * M_PI, 150, 176, 205, pass ? 70 : 130);
        }
        for (int i = 0; i < 12; ++i) {
            double a = 2.0 * M_PI * i / 12.0;
            int x0 = (int)(wx + std::cos(a) * (wr + 1));
            int y0 = (int)(wy + std::sin(a) * (wr + 1));
            int x1 = (int)(wx + std::cos(a) * (radius - 2));
            int y1 = (int)(wy + std::sin(a) * (radius - 2));
            line(r, x0, y0, x1, y1, 96, 112, 138, 95);
            line(r, x0 + 1, y0 + 1, x1 + 1, y1 + 1, 58, 70, 88, 80);
            // 窗棂螺栓
            fill(r, x0 - 1, y0 - 1, 2, 2, 150, 166, 192, 130);
        }
        // 环形窗框上的状态灯（通电后亮）
        int lit = std::min(12, timer / 5 + 1);
        for (int i = 0; i < 12; ++i) {
            if (i >= lit) continue;
            double a = 2.0 * M_PI * i / 12.0 + 0.13;
            int x = (int)(wx + std::cos(a) * (wr + 10));
            int y = (int)(wy + std::sin(a) * (wr + 10));
            if (y > cy - 4) continue;
            fill(r, x - 1, y - 1, 3, 3, 110, 225, 255, 190);
            fill(r, x - 3, y - 3, 7, 7, 90, 190, 230, 30);
        }
    }

    // ---------------- 圆厅壳体：外环 / 墙板 / 顶部灯 ----------------
    void drawRoomShell(SDL_Renderer* r) const {
        double boot = clamp01(timer / 42.0);
        // 外环（双线 + 分段亮条）
        arc(r, cx, cy, radius, 0, 2.0 * M_PI, 118, 136, 164, 200);
        arc(r, cx, cy, radius - 3, 0, 2.0 * M_PI, 66, 78, 96, 220);
        arc(r, cx, cy, radius - 6, 0, 2.0 * M_PI, 34, 41, 54, 210);

        // 墙板分格：外环到窗框之间的环带
        for (int i = 0; i < 24; ++i) {
            double a0 = 2.0 * M_PI * i / 24.0;
            double a1 = 2.0 * M_PI * (i + 1) / 24.0;
            int rr = 226;
            arc(r, cx, cy, rr, a0, a1, 78, 90, 112, 50);
            if (i % 3 == 0) {
                // 主肋
                int x0 = (int)(cx + std::cos(a0) * (radius - 6));
                int y0 = (int)(cy + std::sin(a0) * (radius - 6));
                int x1 = (int)(cx + std::cos(a0) * 206);
                int y1 = (int)(cy + std::sin(a0) * 206);
                line(r, x0, y0, x1, y1, 92, 106, 132, 90);
            }
            // 墙灯：逐段点亮
            if (i < (int)(24 * boot)) {
                double a = (a0 + a1) * 0.5;
                int x = (int)(cx + std::cos(a) * (radius - 12));
                int y = (int)(cy + std::sin(a) * (radius - 12));
                fill(r, x - 1, y - 1, 3, 3, 108, 224, 255, 190);
                fill(r, x - 4, y - 4, 9, 9, 80, 190, 230, 26);
            }
        }
        // 顶部穹顶灯
        fill(r, (int)cx - 20, 58, 40, 6, 40, 47, 60, 255);
        box(r, (int)cx - 20, 58, 40, 6, 130, 146, 172, 190);
        fill(r, (int)cx - 12, 64, 24, 5, 170, 232, 252, (int)(180 * boot));
        fill(r, (int)cx - 5, 64, 10, 4, 255, 255, 255, (int)(230 * boot));
        if (boot > 0.1) {
            for (int k = 0; k < 3; ++k)
                ellipse(r, cx, 74, 80 + k * 30, 8 + k * 3, 150, 220, 245, (int)(26 * boot));
        }
    }

    // ---------------- 下半圆地板 ----------------
    void drawFloor(SDL_Renderer* r) const {
        // 半圆地板填充（按 y 逐行，避免出现矩形直角）
        for (int y = (int)cy; y <= (int)(cy + radius); y += 2) {
            double dy = y - cy;
            double hw = std::sqrt(std::max(0.0, radius * radius - dy * dy));
            int shade = 17 + (int)((y - cy) * 12.0 / radius);
            fill(r, (int)(cx - hw), y, (int)(hw * 2), 2, shade, shade + 3, shade + 10, 255);
        }
        // 墙脚线（房间后墙与地板交线）
        line(r, (int)(cx - radius), (int)cy, (int)(cx + radius), (int)cy, 120, 140, 168, 170);
        line(r, (int)(cx - radius), (int)cy + 1, (int)(cx + radius), (int)cy + 1, 46, 54, 68, 190);

        // 同心环 + 放射线
        for (int k = 1; k <= 5; ++k) {
            double rr = 34.0 + k * 38.0;
            if (rr > radius - 6) break;
            arc(r, cx, cy, rr, 0, M_PI, 92, 116, 144, 105 - k * 9);
            arc(r, cx, cy, rr + 1, 0, M_PI, 150, 182, 212, 42);
        }
        for (int i = 0; i <= 8; ++i) {
            double a = M_PI * i / 8.0;
            int x0 = (int)(cx + std::cos(a) * 54);
            int y0 = (int)(cy + std::sin(a) * 54);
            int x1 = (int)(cx + std::cos(a) * (radius - 12));
            int y1 = (int)(cy + std::sin(a) * (radius - 12));
            line(r, x0, y0, x1, y1, 74, 90, 114, 58);
        }
        // 外沿灯带
        for (int i = 0; i < 16; ++i) {
            double a0 = M_PI * i / 16.0 + 0.02;
            double a1 = a0 + 0.055;
            arc(r, cx, cy, radius - 10, a0, a1, 100, 216, 244, 120);
        }
        // 窗光落在地板上的冷色反光
        for (int k = 0; k < 6; ++k)
            fill(r, (int)cx - 70 + k * 4, (int)cy + 10, 140 - k * 8, 8,
                 80, 140, 175, 14);
        // 从中心向外的脉冲环（每 90 帧一次）
        int ph = timer % 90;
        double pulseR = 28.0 + ph * 2.4;
        if (pulseR < radius - 12) {
            int a = (int)(90 * (1.0 - ph / 90.0));
            arc(r, cx, cy, pulseR, 0, M_PI, 110, 226, 255, a);
        }
        // 地面的缓慢雷达扫线（只在下半圆）
        double sa = M_PI * 0.5 + std::sin(timer * 0.008) * 0.9;
        line(r, (int)cx, (int)cy,
             (int)(cx + std::cos(sa) * (radius - 18)),
             (int)(cy + std::sin(sa) * (radius - 18)),
             100, 200, 230, 34);
        // 地板填色会盖住外环下半圈，补画回来
        arc(r, cx, cy, radius, 0, 2.0 * M_PI, 112, 130, 158, 190);
        arc(r, cx, cy, radius - 3, 0, 2.0 * M_PI, 62, 74, 92, 200);
    }

    // ---------------- 左侧气闸：玩家从这里飞入 ----------------
    void drawPortal(SDL_Renderer* r) const {
        // 门后的短通道
        fill(r, 74, 252, 116, 96, 5, 7, 10, 255);
        line(r, 74, 252, 190, 252, 66, 78, 96, 190);
        line(r, 74, 348, 190, 348, 66, 78, 96, 190);
        line(r, 74, 252, 190, 252, 100, 118, 142, 80);
        // 通道透视线
        for (int k = 1; k <= 3; ++k) {
            double t = k / 4.0;
            line(r, (int)(74 + 40 * t), (int)(252 + 8 * t),
                 (int)(74 + 40 * t), (int)(348 - 8 * t), 40, 52, 68, 70);
        }
        // 通道尽头的光 + 灯带
        fill(r, 78, 272, 10, 56, 120, 220, 245, 26);
        fill(r, 82, 280, 5, 40, 190, 242, 255, 90);
        line(r, 84, 300, 190, 300, 120, 224, 250, 150);
        line(r, 84, 296, 190, 296, 160, 238, 255, 60);

        // 外框
        fill(r, 126, 236, 126, 128, 26, 31, 41, 255);
        box(r, 126, 236, 126, 128, 142, 160, 188, 230);
        box(r, 130, 240, 118, 120, 78, 90, 112, 210);
        // 开门后的两扇门叶（收进上下框里）
        fill(r, 138, 242, 28, 10, 44, 51, 65, 255);
        fill(r, 138, 348, 28, 10, 44, 51, 65, 255);
        fill(r, 218, 242, 26, 10, 44, 51, 65, 255);
        fill(r, 218, 348, 26, 10, 44, 51, 65, 255);
        // 门框内侧冷光
        line(r, 140, 252, 140, 348, 128, 218, 245, 130);
        line(r, 236, 252, 236, 348, 128, 218, 245, 100);
        // 门框警示斜纹
        for (int k = 0; k < 7; ++k) {
            int bx = 130 + k * 17;
            line(r, bx, 244, bx + 8, 252, 224, 166, 62, 130);
            line(r, bx, 348, bx + 8, 356, 224, 166, 62, 130);
        }
        // 门顶状态灯
        double pulse = 0.5 + 0.5 * std::sin(timer * 0.06);
        fill(r, 184, 228, 6, 5, 120, 230, 255, (int)(110 + 110 * pulse));
        fill(r, 181, 225, 12, 11, 90, 200, 235, 26);
        // 门槛
        line(r, 126, 362, 252, 362, 104, 118, 142, 180);
        line(r, 136, 366, 242, 366, 60, 70, 88, 160);
    }

    // ---------------- 右侧出口：Warden 后来封死的地方 ----------------
    void drawExitDoor(SDL_Renderer* r) const {
        // 门框
        fill(r, 640, 206, 86, 188, 24, 28, 37, 255);
        box(r, 640, 206, 86, 188, 136, 152, 180, 220);
        box(r, 645, 211, 76, 178, 72, 84, 104, 210);
        // 两扇门板
        fill(r, 650, 220, 30, 168, 34, 39, 51, 255);
        fill(r, 682, 220, 30, 168, 30, 34, 45, 255);
        line(r, 681, 220, 681, 388, 110, 124, 148, 170);
        // 门板细节
        for (int y = 238; y < 380; y += 26) {
            line(r, 652, y, 678, y, 58, 66, 82, 160);
            line(r, 684, y, 710, y, 58, 66, 82, 160);
        }
        // 中央密封条（红色警示）
        double pulse = 0.5 + 0.5 * std::sin(timer * 0.045);
        line(r, 679, 220, 679, 388, 190, 50, 46, 160);
        line(r, 680, 220, 680, 388, 255, 84, 70, (int)(90 + 80 * pulse));
        // 门顶红色状态灯
        for (int k = 0; k < 3; ++k) {
            fill(r, 656 + k * 20, 213, 8, 4, 255, 70, 58,
                 (int)(90 + 120 * ((k % 2) ? pulse : 1.0 - pulse)));
        }
        // 底部警示斜纹
        for (int k = 0; k < 8; ++k) {
            int bx = 648 + k * 10;
            line(r, bx, 380, bx + 7, 388, 220, 158, 58, 90);
        }
        // 门底灯光
        line(r, 642, 396, 724, 396, 90, 110, 136, 90);
    }

    // ---------------- 中央基座 ----------------
    void drawPedestal(SDL_Renderer* r) const {
        double boot = clamp01(timer / 38.0);
        // 地面基座光晕
        ellipse(r, cx, 366, 116, 22, 80, 200, 225, (int)(45 * boot));
        ellipse(r, cx, 366, 100, 18, 110, 230, 250, (int)(30 * boot));

        // 底座下沿
        fillEllipse(r, cx, 404, 120, 22, 24, 29, 38, 255);
        ellipse(r, cx, 404, 120, 22, 96, 110, 134, 190);
        // 底座主体（圆柱）
        fill(r, (int)cx - 96, 382, 192, 22, 30, 36, 47, 255);
        line(r, (int)cx - 96, 382, (int)cx - 96, 404, 116, 132, 160, 170);
        line(r, (int)cx + 96, 382, (int)cx + 96, 404, 116, 132, 160, 170);
        // 上平台
        fillEllipse(r, cx, 382, 104, 19, 38, 45, 58, 255);
        ellipse(r, cx, 382, 104, 19, 146, 164, 192, 210);
        ellipse(r, cx, 380, 78, 14, 96, 224, 255, (int)(80 * boot));
        // 中央立柱
        fill(r, (int)cx - 26, 348, 52, 34, 33, 39, 51, 255);
        line(r, (int)cx - 26, 348, (int)cx - 20, 382, 116, 132, 160, 160);
        line(r, (int)cx + 26, 348, (int)cx + 20, 382, 116, 132, 160, 160);
        // 悬浮光环平台
        fillEllipse(r, cx, 348, 66, 13, 42, 49, 64, 255);
        ellipse(r, cx, 348, 66, 13, 170, 190, 220, 190);
        ellipse(r, cx, 346, 46, 9, 120, 235, 255, (int)(90 * boot));
        // 基座侧面的状态灯
        for (int k = 0; k < 6; ++k) {
            double a = M_PI * (0.15 + 0.7 * k / 5.0);
            int x = (int)(cx + std::cos(a) * 92);
            int y = (int)(382 + std::sin(a) * 15);
            bool on = timer > 8 + k * 4;
            fill(r, x - 1, y - 1, 3, 3, on ? 120 : 60, on ? 230 : 70, on ? 255 : 80,
                 on ? 190 : 120);
        }
        // 基座到地面的几根导管
        for (int k = -2; k <= 2; ++k) {
            int x = (int)cx + k * 34;
            line(r, x, 402, x + k * 4, 442, 62, 74, 92, 90);
        }
    }

    // ---------------- 全息环 / 光柱 ----------------
    void drawHolo(SDL_Renderer* r) const {
        double holo = clamp01((timer - 24) / 34.0);
        if (holo <= 0.01) return;
        // 穹顶到飞机的光柱
        for (int k = 1; k <= 6; ++k) {
            double t = k / 7.0;
            int hw = (int)(16 + t * 44);
            int y = (int)(70 + (cy - 70) * t);
            line(r, (int)cx - hw, y, (int)cx + hw, y, 130, 205, 235,
                 (int)(16 * holo));
        }
        line(r, (int)cx - 14, 72, (int)cx - 60, 296, 120, 205, 235, (int)(40 * holo));
        line(r, (int)cx + 14, 72, (int)cx + 60, 296, 120, 205, 235, (int)(40 * holo));
        line(r, (int)cx, 74, (int)cx, 292, 170, 235, 255, (int)(74 * holo));

        // 环绕原型机的三层全息环
        for (int k = 0; k < 3; ++k) {
            double phase = timer * 0.018 + k * 2.1;
            double rx = 64.0 + k * 20.0;
            double ry = 13.0 + k * 4.0 + std::sin(phase) * 2.0;
            double oy = std::sin(phase * 0.8) * 5.0;
            int a = (int)((64 + k * 16) * holo);
            ellipse(r, cx, cy + 12 + oy, rx, ry, 100, 225, 255, a);
            // 环上的光点
            double ga = phase * 1.6;
            int gx = (int)(cx + std::cos(ga) * rx);
            int gy = (int)(cy + 12 + oy + std::sin(ga) * ry);
            fill(r, gx - 1, gy - 1, 3, 3, 190, 250, 255, (int)(180 * holo));
        }
        // 扫描弧
        double sa = timer * 0.03;
        arc(r, cx, cy + 12, 84, sa, sa + 1.1, 120, 255, 200, (int)(70 * holo));
        // 基座到机体的连接光
        line(r, (int)cx, 332, (int)cx, 310, 140, 245, 220, (int)(90 * holo));
        fill(r, (int)cx - 3, 330, 6, 4, 190, 255, 235, (int)(150 * holo));
    }

    // ---------------- 浮动尘埃 / 扫描余波 ----------------
    void drawAmbience(SDL_Renderer* r) const {
        double holo = clamp01((timer - 20) / 36.0);
        // 光柱里的尘埃
        for (int i = 0; i < 26; ++i) {
            double t = std::fmod(timer * 0.004 + hash01(i * 11 + 3), 1.0);
            int y = (int)(90 + t * 210);
            int hw = (int)(10 + t * 54);
            int x = (int)(cx - hw + hash01(i * 17 + 9) * hw * 2);
            int a = (int)((20 + 40 * hash01(i * 23 + 5)) * holo);
            fill(r, x, y, 1, 1, 190, 235, 255, a);
        }
        // 窗边偶尔的冷光脉冲
        double pulse = std::fabs(std::sin(timer * 0.012));
        if (pulse > 0.86) {
            for (int k = 0; k < 4; ++k)
                arc(r, cx, cy, radius - 18 - k * 4, -M_PI * 0.82, -M_PI * 0.18,
                    120, 210, 240, (int)((pulse - 0.86) * 220));
        }
        // 左侧气闸口的往返气流线
        for (int i = 0; i < 5; ++i) {
            int y = 268 + i * 14;
            int len = 18 + (int)(hash01(timer / 3 + i) * 20);
            line(r, 240, y, 240 + len, y, 130, 220, 245,
                 (int)(18 + 22 * hash01(i * 7 + 1)));
        }
    }
};
