#pragma once

#include "../constants.h"
#include "../renderer.h"
#include <algorithm>
#include <cmath>

// ============== Ch2AirlockScene (indoor security airlock at the end of the corridor) ==============
// 廊桥尽头的室内小型真空门禁：比开场大门小，嵌在廊桥右侧的钢制舱壁上。
// 无状态绘制：阶段 / 开门进度全部由 Game 已入档的 ch2PhaseTimer 推导，
// 因此不新增存档字段，旧档也不会因这个场景改变布局。
//
// 时间轴（帧）：
//   0..30   通电自检（镜头电火花）
//   30..58  红色扫描束展开
//   58..160 搜寻 / 下扫
//   160..250 锁定后：慢闪 → 闪得更快 → 亮占空比增大（搜寻变锁定）
//   250..266 红光常亮（确认）
//   266     验证通过（转绿）
//   278..348 双门板对开
//   312+    允许通过
class Ch2AirlockScene {
public:
    static const int FRAME_L = 650;   // left frame column
    static const int AP_L = 695;      // aperture left
    static const int MID = 740;       // panel split
    static const int AP_R = 790;      // aperture right
    static const int FRAME_R = WIN_WIDTH;
    static const int SCAN_X = 692;    // scanner lens
    static const int SCAN_Y = 100;

    enum {
        BOOT_END = 30,
        DEPLOY_END = 58,
        LOCK_AT = 160,
        STEADY_AT = 250,
        VERIFY_AT = 266,
        OPEN_START = 278,
        OPEN_END = 348,
        PASS_AT = 312
    };

    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double smooth01(double t) { t = clamp01(t); return t * t * (3.0 - 2.0 * t); }
    static double easeOutCubic(double t) { t = clamp01(t); double u = 1.0 - t; return 1.0 - u * u * u; }

    static double openT(int timer) {
        return smooth01(clamp01((timer - OPEN_START) / double(OPEN_END - OPEN_START)));
    }
    static bool canPass(int timer) { return timer >= PASS_AT; }
    static bool isOpen(int timer) { return timer >= OPEN_END; }

    // 红光的"搜寻 → 锁定"呼吸：锁定后闪动逐渐变快、亮的时间变长，最后常亮。
    static double redPulse(int timer) {
        if (timer < DEPLOY_END)
            return 0.55 + 0.45 * std::fabs(std::sin(timer * 0.35));
        if (timer < LOCK_AT) {
            double ph = std::fmod(timer, 56.0) / 56.0;
            return ph < 0.52 ? 1.0 : 0.26;
        }
        if (timer < STEADY_AT) {
            double k = clamp01((timer - LOCK_AT) / double(STEADY_AT - LOCK_AT));
            double period = 56.0 - 40.0 * k;
            double duty = 0.52 + 0.48 * k;
            double ph = std::fmod(timer, period) / period;
            double on = ph < duty ? 1.0 : 0.24;
            return on * (0.94 + 0.06 * std::sin(timer * 0.5));
        }
        return 1.0;
    }

    // ======== DRAW: bulkhead / panels / scanner (background layer, behind the plane) ========
    static void draw(SDL_Renderer* r, int timer) {
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        drawInteriorLight(r, timer);
        drawPanels(r, timer);
        drawFrame(r, timer);
        drawScanner(r, timer);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

    // ======== DRAW: red scan / reticle / vignette (over the plane) ========
    static void drawScan(SDL_Renderer* r, int timer, double px, double py) {
        if (timer < BOOT_END) {
            drawBootSparks(r, timer);
            return;
        }
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        if (timer >= VERIFY_AT) {
            // 验证通过：绿闪后迅速收束
            int age = timer - VERIFY_AT;
            if (age < 24) {
                int a = (int)(210 * (1.0 - age / 24.0));
                arc(r, px, py, 24 + age * 5.0, 0, 2.0 * M_PI, 120, 255, 170, a);
                arc(r, px, py, 17 + age * 4.0, 0, 2.0 * M_PI, 80, 220, 150, a / 2);
            }
            SDL_SetRenderDrawBlendMode(r, prev);
            return;
        }

        double deployRaw = clamp01((timer - BOOT_END) / double(DEPLOY_END - BOOT_END));
        double ext = easeOutCubic(deployRaw);
        double halfW = 32.0 - 8.0 * smooth01((timer - DEPLOY_END) /
                                             double(LOCK_AT - DEPLOY_END));
        if (timer < DEPLOY_END) halfW = 34.0;
        double lockT = clamp01((timer - DEPLOY_END) / double(LOCK_AT - DEPLOY_END));
        double rp = redPulse(timer);
        // 锁定重音：锁定瞬间与随后几帧强制常亮，再接回"闪动变快 → 常亮"
        if (timer >= LOCK_AT && timer < LOCK_AT + 4) rp = 1.0;
        double aimY = SCAN_Y + 6 + (py - SCAN_Y - 6) * smooth01(clamp01(deployRaw * 1.6));
        double aimX = SCAN_X + 4 + (px - SCAN_X - 4) * smooth01(clamp01(deployRaw * 1.6));
        if (timer >= LOCK_AT) { aimX = px; aimY = py; }

        // 红色扇形扫描：低透明度弦线堆出体积
        if (ext > 0.02) {
            double dx = aimX - SCAN_X, dy = aimY - SCAN_Y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len > 2.0) {
                double nx = -dy / len, ny = dx / len;
                for (int i = 1; i <= 30; ++i) {
                    double t = i / 30.0;
                    double w = halfW * (0.10 + 0.90 * t);
                    double hx = SCAN_X + dx * t * ext;
                    double hy = SCAN_Y + dy * t * ext;
                    setColor(r, 255, 62, 56, (int)(rp * (11 + 8 * t)));
                    SDL_RenderDrawLine(r, (int)(hx + nx * w), (int)(hy + ny * w),
                                          (int)(hx - nx * w), (int)(hy - ny * w));
                }
                double bx = SCAN_X + dx * ext, by = SCAN_Y + dy * ext;
                setColor(r, 255, 78, 68, (int)(rp * 120));
                SDL_RenderDrawLine(r, SCAN_X, SCAN_Y,
                                   (int)(bx + nx * halfW), (int)(by + ny * halfW));
                SDL_RenderDrawLine(r, SCAN_X, SCAN_Y,
                                   (int)(bx - nx * halfW), (int)(by - ny * halfW));
                for (int k = 0; k < 3; ++k) {
                    double t = std::fmod(timer * 0.012 + k / 3.0, 1.0);
                    double hx = SCAN_X + dx * t * ext;
                    double hy = SCAN_Y + dy * t * ext;
                    double w = halfW * (0.10 + 0.90 * t);
                    setColor(r, 255, 160, 150, (int)(rp * 46));
                    SDL_RenderDrawLine(r, (int)(hx + nx * w), (int)(hy + ny * w),
                                          (int)(hx - nx * w), (int)(hy - ny * w));
                }
            }
        }

        // 锁定瞬间的亮环
        if (timer >= LOCK_AT && timer < LOCK_AT + 6) {
            int age = timer - LOCK_AT;
            int a = 210 - age * 28;
            arc(r, px, py, 26 + age * 5.0, 0, 2.0 * M_PI, 255, 126, 104, a);
        }
        // 锁定框 / 环形进度 / 栅格
        if (timer >= DEPLOY_END) {
            int dim = (int)(180 * rp);
            double size = 40.0 - 14.0 * smooth01(lockT);
            int s = (int)size, L = 8;
            int cr = 255, cg = 78, cb = 68;
            setColor(r, cr, cg, cb, dim);
            SDL_RenderDrawLine(r, (int)px - s, (int)py - s, (int)px - s + L, (int)py - s);
            SDL_RenderDrawLine(r, (int)px - s, (int)py - s, (int)px - s, (int)py - s + L);
            SDL_RenderDrawLine(r, (int)px + s, (int)py - s, (int)px + s - L, (int)py - s);
            SDL_RenderDrawLine(r, (int)px + s, (int)py - s, (int)px + s, (int)py - s + L);
            SDL_RenderDrawLine(r, (int)px - s, (int)py + s, (int)px - s + L, (int)py + s);
            SDL_RenderDrawLine(r, (int)px - s, (int)py + s, (int)px - s, (int)py + s - L);
            SDL_RenderDrawLine(r, (int)px + s, (int)py + s, (int)px + s - L, (int)py + s);
            SDL_RenderDrawLine(r, (int)px + s, (int)py + s, (int)px + s, (int)py + s - L);
            if (lockT > 0.1) {
                setColor(r, cr, cg, cb, dim);
                SDL_RenderDrawLine(r, (int)px - 6, (int)py, (int)px + 6, (int)py);
                SDL_RenderDrawLine(r, (int)px, (int)py - 6, (int)px, (int)py + 6);
            }
            if (lockT > 0.99) {
                int a = (int)((90 + 70 * std::fabs(std::sin(timer * 0.25))) * rp);
                arc(r, px, py, s + 8, 0, 2.0 * M_PI, cr, cg, cb, a);
                for (int y = (int)py - 22; y <= (int)py + 22; y += 4) {
                    int sy = (int)py - 22 + ((int)(timer * 2.0) % 44);
                    int d = std::abs(y - sy);
                    int aa = (int)((d < 4 ? 80 : 18) * rp);
                    line(r, (int)px - 22, y, (int)px + 22, y, 255, 100, 90, aa);
                }
            } else {
                double a0 = -M_PI / 2;
                arc(r, px, py, s + 8, a0, a0 + 2.0 * M_PI * smooth01(lockT),
                    cr, cg, cb, (int)(170 * rp));
            }
        }

        // 红色边缘压迫
        double vig = (timer < LOCK_AT ? 0.40 : 0.62) * (0.55 + 0.45 * rp);
        int base = (int)(vig * 26);
        for (int i = 0; i < 10; ++i) {
            int inset = i * 3;
            int a = base * (10 - i) / 10;
            if (a < 2) continue;
            setColor(r, 255, 40, 34, a);
            SDL_Rect rr = {inset, inset, WIN_WIDTH - 2 * inset, WIN_HEIGHT - 2 * inset};
            SDL_RenderDrawRect(r, &rr);
        }

        SDL_SetRenderDrawBlendMode(r, prev);
    }

private:
    static void setColor(SDL_Renderer* r, int cr, int cg, int cb, int ca) {
        if (ca < 0) ca = 0;
        if (ca > 255) ca = 255;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
    }
    static void fill(SDL_Renderer* r, int x, int y, int w, int h,
                     int cr, int cg, int cb, int ca = 255) {
        if (w <= 0 || h <= 0) return;
        setColor(r, cr, cg, cb, ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderFillRect(r, &rr);
    }
    static void line(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                     int cr, int cg, int cb, int ca = 255) {
        setColor(r, cr, cg, cb, ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void box(SDL_Renderer* r, int x, int y, int w, int h,
                    int cr, int cg, int cb, int ca = 255) {
        setColor(r, cr, cg, cb, ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderDrawRect(r, &rr);
    }
    static void arc(SDL_Renderer* r, double x, double y, double rad,
                    double a0, double a1, int cr, int cg, int cb, int ca) {
        int steps = (int)(std::fabs(a1 - a0) * rad / 3.0);
        if (steps < 3) steps = 3;
        if (steps > 96) steps = 96;
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
    static void seg7(SDL_Renderer* r, int x, int y, int w, int h, int d,
                     int cr, int cg, int cb, int ca) {
        static const unsigned char mask[10] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
        };
        unsigned m = mask[(d % 10 + 10) % 10];
        int t = 2, hh = h / 2;
        auto s = [&](int sx, int sy, int sw, int sh) { fill(r, sx, sy, sw, sh, cr, cg, cb, ca); };
        if (m & 0x01) s(x + t, y, w - 2 * t, t);
        if (m & 0x02) s(x + w - t, y + t, t, hh - t);
        if (m & 0x04) s(x + w - t, y + hh, t, hh - t);
        if (m & 0x08) s(x + t, y + h - t, w - 2 * t, t);
        if (m & 0x10) s(x, y + hh, t, hh - t);
        if (m & 0x20) s(x, y + t, t, hh - t);
        if (m & 0x40) s(x + t, y + hh - t / 2, w - 2 * t, t);
    }

    // 验证 / 开门 / 待机三套配色
    static void doorColor(int timer, int& cr, int& cg, int& cb) {
        if (timer >= VERIFY_AT) { cr = 110; cg = 228; cb = 255; }
        else if (timer >= LOCK_AT) { cr = 255; cg = 74; cb = 60; }
        else { cr = 255; cg = 120; cb = 72; }
    }

    // 门后的明亮内舱：门板移开时露出，给"放行 → 进入研究室"一个光源
    static void drawInteriorLight(SDL_Renderer* r, int timer) {
        (void)timer;
        fill(r, AP_L, 14, AP_R - AP_L, WIN_HEIGHT - 28, 7, 10, 15, 255);
        for (int k = 0; k < 10; ++k) {
            int x0 = AP_L + k * 10;
            int a = 8 + (k % 5) * 5;
            fill(r, x0, 18, 9, WIN_HEIGHT - 36, 80, 150, 180, a);
        }
        fill(r, MID - 8, 18, 16, WIN_HEIGHT - 36, 150, 226, 245, 20);
        fill(r, MID - 3, 18, 6, WIN_HEIGHT - 36, 226, 248, 255, 55);
        fill(r, MID - 1, 18, 2, WIN_HEIGHT - 36, 255, 255, 255, 120);
        line(r, AP_L, 14, AP_L, WIN_HEIGHT - 14, 120, 200, 225, 60);
        line(r, AP_R, 14, AP_R, WIN_HEIGHT - 14, 120, 200, 225, 60);
    }

    static void drawPanels(SDL_Renderer* r, int timer) {
        double t = openT(timer);
        int leftX = AP_L - (int)(45.0 * t);
        int rightX = MID + (int)(50.0 * t);
        int lr, lg, lb;
        // 门板：左板 45 宽、右板 50 宽；打开后分别藏进左门柱/屏幕外
        const int leftW = MID - AP_L;         // 45
        const int rightW = AP_R - MID;        // 50
        // 左侧门板
        fill(r, leftX, 14, leftW, WIN_HEIGHT - 28, 31, 36, 47, 255);
        fill(r, leftX + 3, 14, 3, WIN_HEIGHT - 28, 20, 23, 31, 255);
        fill(r, leftX + leftW - 4, 14, 4, WIN_HEIGHT - 28, 24, 28, 37, 255);
        // 右侧门板
        fill(r, rightX, 14, rightW, WIN_HEIGHT - 28, 29, 34, 45, 255);
        fill(r, rightX + 3, 14, 3, WIN_HEIGHT - 28, 20, 23, 31, 255);
        fill(r, rightX, 14, 3, WIN_HEIGHT - 28, 38, 44, 57, 255);
        // 横向装甲带 + 铆钉
        for (int y = 38; y < WIN_HEIGHT - 30; y += 58) {
            line(r, leftX + 4, y, leftX + leftW - 4, y, 62, 71, 88, 150);
            line(r, leftX + 4, y + 1, leftX + leftW - 4, y + 1, 18, 21, 28, 180);
            fill(r, leftX + 6, y + 5, 2, 2, 132, 148, 172, 120);
            fill(r, leftX + leftW - 8, y + 5, 2, 2, 132, 148, 172, 120);
            line(r, rightX + 4, y, rightX + rightW - 4, y, 62, 71, 88, 150);
            line(r, rightX + 4, y + 1, rightX + rightW - 4, y + 1, 18, 21, 28, 180);
            fill(r, rightX + 6, y + 5, 2, 2, 132, 148, 172, 120);
            fill(r, rightX + rightW - 8, y + 5, 2, 2, 132, 148, 172, 120);
        }
        // 门板内侧的加强板
        fill(r, leftX + 10, 90, leftW - 20, 150, 36, 42, 55, 255);
        box(r, leftX + 10, 90, leftW - 20, 150, 84, 96, 118, 100);
        fill(r, rightX + 8, 90, rightW - 20, 150, 34, 40, 52, 255);
        box(r, rightX + 8, 90, rightW - 20, 150, 84, 96, 118, 100);
        // 门缝密封条（红光 → 绿光 → 青光）
        doorColor(timer, lr, lg, lb);
        int a = (int)(255 * (1.0 - 0.35 * t));
        line(r, leftX + leftW - 1, 18, leftX + leftW - 1, WIN_HEIGHT - 18, lr, lg, lb, a);
        line(r, leftX + leftW - 3, 18, leftX + leftW - 3, WIN_HEIGHT - 18, lr, lg, lb, a / 3);
        line(r, rightX + 1, 18, rightX + 1, WIN_HEIGHT - 18, lr, lg, lb, a);
        line(r, rightX + 3, 18, rightX + 3, WIN_HEIGHT - 18, lr, lg, lb, a / 3);
        // 门缝处的警示斜纹（只在内侧边缘，开门时淡出）
        int ha = (int)(160 * (1.0 - smooth01(t)));
        for (int y = 20; y < WIN_HEIGHT - 20; y += 18) {
            line(r, leftX + leftW - 9, y, leftX + leftW - 1, y + 8, 225, 164, 58, ha);
            line(r, rightX + 1, y, rightX + 9, y + 8, 225, 164, 58, ha);
        }
    }

    static void drawFrame(SDL_Renderer* r, int timer) {
        // 左门柱
        fill(r, FRAME_L, 0, AP_L - FRAME_L, WIN_HEIGHT, 25, 30, 39, 255);
        fill(r, FRAME_L, 0, 8, WIN_HEIGHT, 19, 23, 30, 255);
        line(r, FRAME_L + 1, 0, FRAME_L + 1, WIN_HEIGHT, 132, 148, 174, 200);
        line(r, AP_L - 4, 0, AP_L - 4, WIN_HEIGHT, 120, 136, 162, 210);
        line(r, AP_L - 3, 0, AP_L - 3, WIN_HEIGHT, 54, 64, 80, 220);
        // 右门柱
        fill(r, AP_R, 0, FRAME_R - AP_R, WIN_HEIGHT, 25, 30, 39, 255);
        line(r, AP_R + 1, 0, AP_R + 1, WIN_HEIGHT, 120, 136, 162, 190);
        // 顶/底横梁
        fill(r, FRAME_L, 0, FRAME_R - FRAME_L, 14, 28, 33, 43, 255);
        fill(r, FRAME_L, WIN_HEIGHT - 14, FRAME_R - FRAME_L, 14, 28, 33, 43, 255);
        line(r, FRAME_L, 14, FRAME_R, 14, 104, 120, 146, 180);
        line(r, FRAME_L, WIN_HEIGHT - 14, FRAME_R, WIN_HEIGHT - 14, 104, 120, 146, 180);
        for (int x = FRAME_L + 14; x < FRAME_R; x += 26) {
            fill(r, x, 7, 2, 2, 132, 148, 172, 100);
            fill(r, x, WIN_HEIGHT - 9, 2, 2, 132, 148, 172, 100);
        }

        // 左柱状态灯：8 格，随扫描/验证/开门变色
        int lr, lg, lb;
        doorColor(timer, lr, lg, lb);
        int lit = std::min(8, timer / 8 + 1);
        for (int i = 0; i < 8; ++i) {
            int y = 176 + i * 38;
            bool on = i < lit;
            if (on) {
                fill(r, 658, y - 2, 9, 20, lr, lg, lb, 26);
                fill(r, 660, y, 5, 16, lr, lg, lb, 230);
                fill(r, 661, y + 2, 3, 12, 255, 255, 255, 170);
            } else {
                fill(r, 660, y, 5, 16, 24, 28, 36, 255);
                box(r, 660, y, 5, 16, 54, 62, 78, 170);
            }
        }
        // 七段门禁编号 12 + 进度条
        int ir = timer >= VERIFY_AT ? 110 : 230;
        int ig = timer >= VERIFY_AT ? 228 : 120;
        int ib = timer >= VERIFY_AT ? 255 : 70;
        seg7(r, 662, 128, 8, 18, 1, ir, ig, ib, 220);
        seg7(r, 674, 128, 8, 18, 2, ir, ig, ib, 220);
        fill(r, 660, 154, 26, 2, 48, 56, 72, 220);
        int w = (int)(26 * clamp01(timer / double(VERIFY_AT)));
        if (w > 0) fill(r, 660, 154, w, 2, ir, ig, ib, 230);
        // 线缆槽
        line(r, 653, 200, 653, 540, 48, 56, 72, 160);
        line(r, 654, 200, 654, 540, 72, 84, 104, 110);
        for (int y = 230; y < 540; y += 46) fill(r, 651, y, 6, 3, 82, 96, 118, 130);
        // 底部警示斜纹
        for (int k = 0; k < 6; ++k) {
            int x = FRAME_L + 6 + k * 7;
            line(r, x, 520, x + 8, 532, 226, 166, 58, 120);
            line(r, x, 536, x + 8, 548, 226, 166, 58, 100);
        }
    }

    static void drawScanner(SDL_Renderer* r, int timer) {
        int lr, lg, lb;
        doorColor(timer, lr, lg, lb);
        int ly = SCAN_Y;
        // 镜头壳体（装在左柱内侧）
        fill(r, 662, ly - 18, 32, 36, 40, 46, 60, 255);
        box(r, 662, ly - 18, 32, 36, 146, 164, 190, 210);
        fill(r, 666, ly - 14, 24, 5, 58, 66, 84, 220);
        for (int i = 0; i < 3; ++i) fill(r, 668 + i * 6, ly - 5, 3, 10, 20, 24, 32, 230);
        // 镜头
        fill(r, 688, ly - 5, 8, 10, 34, 38, 50, 255);
        double pulse = (timer >= VERIFY_AT) ? 1.0 : redPulse(timer);
        int a = (int)((60 + 120 * pulse) * (timer >= VERIFY_AT ? 0.8 : 1.0));
        fill(r, 690, ly - 3, 5, 6, lr, lg, lb, a);
        fill(r, 692, ly - 1, 2, 2, 255, 240, 226, 220);
        fill(r, 688, ly - 9, 12, 18, lr, lg, lb, (int)(22 * pulse));
        // 顶部警示灯
        double ep = 0.4 + 0.6 * std::fabs(std::sin(timer * 0.045));
        if (timer < VERIFY_AT) {
            fill(r, 674, 62, 5, 4, 255, 74, 60, (int)(110 + 100 * ep));
            fill(r, 669, 58, 15, 12, 255, 60, 50, (int)(24 * ep));
        }
    }

    static void drawBootSparks(SDL_Renderer* r, int timer) {
        bool on = (timer % 8) < 5;
        if (!on) return;
        for (int i = 0; i < 5; ++i) {
            unsigned h = (unsigned)(timer / 2 * 29 + i * 13);
            int x = SCAN_X - 10 + (int)((h % 100) / 100.0 * 20);
            int y = SCAN_Y - 4 + (int)((h >> 8) % 10);
            line(r, x, y, x + 4 - (int)((h >> 4) % 8), y + 5, 255, 170, 120, 160);
        }
        fill(r, SCAN_X - 10, SCAN_Y - 6, 22, 12, 255, 60, 50, 26);
        fill(r, SCAN_X - 5, SCAN_Y - 3, 10, 6, 255, 120, 100, 100);
    }
};
