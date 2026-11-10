#pragma once

#include "../art_fx.h"
#include "../constants.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch1Background ==============
// 程序化深空 + 地平线基地。视觉分层与任务语言保持一致：
//   深空（星云/行星/星场） → 大气雾 → 透视地面 → 酸绿基地 → 前景灯光
// 所有新增视觉元素要么由已存档的 Star 字段推导，要么由可选 frame 决定；
// 不新增存档字段，读档后第一帧与存档帧保持一致。
class Ch1Background {
    std::vector<Star> stars;
    const ChapterConfig& chapterCfg;

    static void fillA(SDL_Renderer* r, int x, int y, int w, int h,
                      int cr, int cg, int cb, int ca = 255) {
        if (w <= 0 || h <= 0 || ca <= 0) return;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderFillRect(r, &rr);
    }
    static void lineA(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                      int cr, int cg, int cb, int ca = 255) {
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void arcA(SDL_Renderer* r, int cx, int cy, int rx, int ry,
                     double a0, double a1, int cr, int cg, int cb, int ca,
                     int seg = 48) {
        int px = cx + (int)(rx * std::cos(a0));
        int py = cy + (int)(ry * std::sin(a0));
        for (int i = 1; i <= seg; ++i) {
            double a = a0 + (a1 - a0) * i / seg;
            int nx = cx + (int)(rx * std::cos(a));
            int ny = cy + (int)(ry * std::sin(a));
            lineA(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static void fillCircleA(SDL_Renderer* r, int cx, int cy, int rad,
                            int cr, int cg, int cb, int ca) {
        if (rad <= 0) return;
        for (int y = -rad; y <= rad; ++y) {
            int hw = (int)std::sqrt((double)(rad * rad - y * y));
            fillA(r, cx - hw, cy + y, hw * 2 + 1, 1, cr, cg, cb, ca);
        }
    }

    // 深空底色：黑 → 蓝黑，并叠两条斜向银河尘带。菜单与战斗共用。
    void drawDeepSpace(SDL_Renderer* r, bool menu) const {
        int hy = menu ? WIN_HEIGHT : chapterCfg.horizonY;
        for (int i = 0; i < 18; ++i) {
            int y0 = hy * i / 18, y1 = hy * (i + 1) / 18;
            int b = 2 + i * 5 / 18;
            fillA(r, 0, y0, WIN_WIDTH, y1 - y0, 2, 3, b, 255);
        }
        // 银河带：两条非常淡的斜向尘埃带，避免大面积纯黑。
        for (int x = 0; x < WIN_WIDTH; x += 2) {
            double wave = std::sin(x * 0.008 + 0.7) * 20.0;
            int y = (int)(60 + x * 0.18 + wave);
            int a = 10 + (int)(6.0 * (0.5 + 0.5 * std::sin(x * 0.035)));
            fillA(r, x, y, 2, 1, 66, 82, 112, a);
            int y2 = (int)(150 + x * 0.10 + wave * 0.6);
            fillA(r, x, y2, 2, 1, 48, 72, 96, a / 2);
        }
        // 三块低亮度星云，提供色彩层次。
        static const int neb[3][4] = {{230, 74, 120, 38}, {552, 118, 150, 30}, {680, 55, 100, 26}};
        for (int k = 0; k < 3; ++k) {
            int nx = neb[k][0], ny = neb[k][1], nr = neb[k][2];
            for (int y = -nr; y <= nr; y += 2) {
                int hw = (int)(nr * std::sqrt(std::max(0.0, 1.0 - (double)(y*y)/(nr*nr))));
                int a = neb[k][3] * (1 - std::abs(y) / (double)nr);
                if (a <= 0) continue;
                fillA(r, nx - hw, ny + y, hw * 2, 2,
                      k == 0 ? 36 : (k == 1 ? 46 : 30),
                      k == 0 ? 68 : (k == 1 ? 58 : 52),
                      k == 0 ? 112 : (k == 1 ? 104 : 92), a);
            }
        }
    }

    void drawPlanet(SDL_Renderer* r) const {
        // 右上角的冷色气态行星：轻微遮挡在菜单边缘，不进入战斗可读区域。
        const int cx = 676, cy = 92, rad = 84;
        fillCircleA(r, cx, cy, rad + 8, 33, 57, 83, 20);
        fillCircleA(r, cx, cy, rad, 10, 21, 34, 245);
        // 大气边缘 + 云带
        arcA(r, cx, cy, rad, rad, 0, 2.0 * M_PI, 73, 142, 156, 110, 56);
        arcA(r, cx, cy, rad - 2, rad - 2, 0, 2.0 * M_PI, 35, 91, 108, 90, 56);
        for (int k = 0; k < 4; ++k) {
            int yy = cy - 36 + k * 22;
            arcA(r, cx, yy + 16, rad - 10, 13 + k * 2, M_PI * 1.04, M_PI * 1.93,
                 46, 102, 116, 64 - k * 8, 32);
        }
        // 行星环（前弧亮、后弧暗），把它从"圆"变成有体量的天体。
        arcA(r, cx, cy + 5, rad + 34, 16, M_PI * 0.05, M_PI * 0.95, 92, 152, 158, 55, 48);
        arcA(r, cx, cy + 5, rad + 42, 20, M_PI * 0.10, M_PI * 0.90, 58, 110, 126, 48, 48);
        arcA(r, cx, cy + 5, rad + 34, 16, M_PI * 1.05, M_PI * 1.95, 32, 70, 89, 80, 48);
    }

    void drawDistantStation(SDL_Renderer* r) const {
        // 地平线左侧的远景前哨：只是几个像素的剪影 + 两盏航标灯。
        const int bx = 102, by = chapterCfg.horizonY - 26;
        lineA(r, bx, by, bx, by + 26, 18, 30, 38, 180);
        lineA(r, bx - 20, by, bx + 20, by, 18, 30, 38, 150);
        lineA(r, bx - 10, by + 7, bx + 10, by + 7, 24, 39, 47, 120);
        fillA(r, bx - 22, by - 2, 2, 2, 120, 210, 190, 160);
        fillA(r, bx + 20, by - 2, 2, 2, 220, 128, 76, 150);
    }

    void drawStarField(SDL_Renderer* r, bool menu) const {
        float brightness = chapterCfg.starBrightness;
        for (size_t i = 0; i < stars.size(); ++i) {
            // 第一章星光保持"稀疏导航星"而非满屏礼花：只绘制一部分星点，
            // 并降低闪烁对比度；战斗时比标题菜单再少一些。
            if (menu) {
                if ((int)((i * 7 + 1) % 10) < 3) continue;
            } else {
                if ((int)((i * 7 + 1) % 10) < 5) continue;
            }
            const auto& s = stars[i];
            int hy = menu ? WIN_HEIGHT : chapterCfg.horizonY;
            if (s.y >= hy) continue;
            float osc = 0.5f + 0.5f * std::sin(s.phase * 0.55f + i * 1.37f);
            float bright = 0.58f + 0.42f * osc;
            int base = (int)(bright * 174 * brightness);
            if (base < 42) base = 42;
            if (base > 210) base = 210;
            unsigned h = ArtFX::hashU((unsigned)i * 2654435761u + (unsigned)(s.x * 7));
            int cr = base, cg = base, cb = base;
            if ((h % 11) == 0) { cr = base / 2 + 38; cg = base; cb = (int)(base * 1.02f); }
            else if ((h % 17) == 0) { cr = base; cg = (int)(base * 0.84f); cb = (int)(base * 0.60f); }
            int x = (int)s.x, y = (int)s.y;
            fillA(r, x, y, 1, 1, cr, cg, cb, 255);
            // 十字星芒非常稀少，避免前景高亮星持续抢视线。
            if ((h % 71) == 0 && bright > 0.90f) {
                ArtFX::starGlint(r, x, y, 1 + (int)(h % 2), cr, cg, cb, 62);
            } else if ((h % 19) == 0 && bright > 0.82f) {
                SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, 52);
                SDL_RenderDrawPoint(r, x - 1, y);
                SDL_RenderDrawPoint(r, x + 1, y);
            }
        }
        // 战斗层不再叠第二套 90 星尘；仅保留极少的静态微尘维持深空层次。
        if (!menu) {
            for (int i = 0; i < 22; ++i) {
                int x = (int)(ArtFX::hash01(i * 17 + 3) * WIN_WIDTH);
                int y = (int)(ArtFX::hash01(i * 31 + 11) * (chapterCfg.horizonY - 12));
                fillA(r, x, y, 1, 1, 66, 82, 92, 28);
            }
        }
    }

    void drawAtmosphere(SDL_Renderer* r) const {
        int hy = chapterCfg.horizonY;
        // 地平线冷雾：多层水平线，把太空与地面连接起来。
        for (int k = 0; k < 11; ++k) {
            int y = hy - 9 + k;
            int a = k < 5 ? 22 + k * 5 : 40 - (k - 5) * 4;
            lineA(r, 0, y, WIN_WIDTH, y, 22, 58, 58, a);
        }
        lineA(r, 0, hy, WIN_WIDTH, hy, 64, 170, 134, 190);
        lineA(r, 0, hy + 1, WIN_WIDTH, hy + 1, 25, 74, 62, 130);
        // 地平线航标灯阵列，间隔固定，不随机。
        for (int x = 24; x < WIN_WIDTH; x += 48) {
            int dx = x - CENTER_X;
            int y = hy + 10 + (std::abs(dx) / 160);
            fillA(r, x, y, 2, 2, 82, 150, 116, 120);
        }
    }

    void drawGround(SDL_Renderer* r) const {
        int hy = chapterCfg.horizonY;
        // 地平线以下：由深到浅的青绿空气透视地面。
        for (int i = 0; i < 24; ++i) {
            int y0 = hy + (WIN_HEIGHT - hy) * i / 24;
            int y1 = hy + (WIN_HEIGHT - hy) * (i + 1) / 24;
            int g = 5 + i * 12 / 24;
            fillA(r, 0, y0, WIN_WIDTH, y1 - y0, 2, g, g + 2, 255);
        }
        // 地面中央的跑道光池与横向扫描线。
        for (int i = 0; i < 10; ++i) {
            int y = hy + 22 + i * 34;
            int a = 12 + i * 3;
            lineA(r, 90, y, WIN_WIDTH - 90, y, 34, 88, 67, a);
        }
        ArtFX::BlendGuard guard(r);
        // 放射透视网格：中间亮、两侧暗，形成跑道感。
        const int NUM_RADIATE = 25;
        for (int i = 0; i < NUM_RADIATE; ++i) {
            double t = (double)i / (NUM_RADIATE - 1);
            int bx = (int)(-180 + t * (WIN_WIDTH + 360));
            int hx = (int)(HORIZON_LEFT + t * (HORIZON_RIGHT - HORIZON_LEFT));
            int a = 18 + (int)(18.0 * (1.0 - std::fabs(t - 0.5) * 2.0));
            lineA(r, bx, WIN_HEIGHT, hx, hy, 34, 116, 87, a);
        }
        // 深度线：越近越亮，保留原版的空间纵深感。
        const int NUM_DEPTH = 18;
        for (int i = 0; i < NUM_DEPTH; ++i) {
            double ratio = (double)(i + 1) / (NUM_DEPTH + 1);
            double dy = hy + (WIN_HEIGHT - hy) * (1.0 - ratio) * (1.0 - ratio);
            int a = 18 + (int)(ratio * 34);
            lineA(r, 0, (int)dy, WIN_WIDTH, (int)dy, 30, 95, 73, a);
        }
        lineA(r, 0, hy, WIN_WIDTH, hy, chapterCfg.groundColorR, chapterCfg.groundColorG, chapterCfg.groundColorB, 255);
    }

    // 基地灯与结构的确定性闪烁；frame 由 Game 的 saved playFrames 传入。
    void drawBaseLights(SDL_Renderer* r, uint32_t frame) const {
        for (int i = 0; i < 18; ++i) {
            double t = (double)i / 17.0;
            int sx = (int)(CENTER_X + 370 * (2.0 * t - 1.0));
            double ratio = (sx - CENTER_X) / 370.0;
            if (ratio < -1.0 || ratio > 1.0) continue;
            int sy = (int)(WIN_HEIGHT - 75 * std::sqrt(1.0 - ratio * ratio));
            int phase = (int)((frame / 18 + i * 3) % 10);
            bool hot = (phase < 2);
            fillA(r, sx, sy - 2, 2, 2, 86, 238, 144, hot ? 235 : 115);
            if (hot) fillA(r, sx - 2, sy - 4, 6, 6, 64, 208, 116, 22);
        }
    }

public:
    Ch1Background(const ChapterConfig& cfg) : chapterCfg(cfg) {
        for (int i = 0; i < cfg.starCount; ++i)
            stars.push_back({(float)(rand() % WIN_WIDTH), (float)(rand() % cfg.horizonY),
                             (rand() % 628) / 100.0f, 0.01f + (rand() % 40) / 1000.0f,
                             (rand() % 30 - 15) / 200.0f});
    }

    void update() {
        for (auto& s : stars) {
            // 慢速相位推进：保留星星呼吸感，但不再快速频闪干扰瞄准。
            s.phase += s.twinkleSpeed * 0.55f;
            if (s.phase > 2.0f * M_PI) s.phase -= 2.0f * M_PI;
            s.x += s.driftSpeed;
            if (s.x < -10) s.x = WIN_WIDTH + 10;
            if (s.x > WIN_WIDTH + 10) s.x = -10;
        }
    }

    // 旧接口保留：asset studio / 其他绘制入口仍可无参数调用。
    void drawStars(SDL_Renderer* renderer) const { drawStarField(renderer, false); }

    void drawStarsFullscreen(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        drawDeepSpace(renderer, true);
        drawPlanet(renderer);
        // 菜单星场增加一点前景亮星和星团，避免空白。
        drawStarField(renderer, true);
        // 低亮度星团：由固定 hash 构成，不影响任何游戏状态。
        for (int i = 0; i < 18; ++i) {
            int x = (int)(ArtFX::hash01(i * 29 + 5) * WIN_WIDTH);
            int y = (int)(ArtFX::hash01(i * 47 + 13) * WIN_HEIGHT);
            int rr = 2 + i % 4;
            for (int k = 0; k < 6; ++k) {
                int px = x + (int)(std::cos(k * 1.04) * rr * 2);
                int py = y + (int)(std::sin(k * 1.04) * rr);
                fillA(renderer, px, py, 1, 1, 95, 110, 130, 100);
            }
        }
    }

    void drawBackground(SDL_Renderer* renderer, uint32_t frame = 0) const {
        ArtFX::BlendGuard guard(renderer);
        drawDeepSpace(renderer, false);
        drawPlanet(renderer);
        drawDistantStation(renderer);
        drawStarField(renderer, false);
        drawAtmosphere(renderer);
        drawGround(renderer);
        // 地面上的远处灯光，随 frame 缓慢呼吸。
        int blink = (frame / 24) % 2;
        for (int i = 0; i < 7; ++i) {
            int x = 60 + i * 112;
            fillA(renderer, x, chapterCfg.horizonY + 18 + (i % 3) * 9, 2, 2,
                  200, 234, 176, blink && (i % 2) ? 210 : 110);
        }
    }

    // 存档：星空（位置/闪烁相位/漂移），保证读档后第一帧与存档帧一致。
    template <class Ar> void visit(Ar& ar) { ar.ioVecObj(stars); }

    void drawBase(SDL_Renderer* renderer, uint32_t frame = 0) const {
        SDL_Renderer* r = renderer;
        ArtFX::BlendGuard guard(r);
        const int BASE_H = 75;
        const double A = WIN_WIDTH / 2.0 - 15.0;
        const double B = BASE_H;

        // 先画一个真正的暗绿机身填充，白色/亮绿线只做轮廓与结构。
        for (int x = 0; x <= WIN_WIDTH; ++x) {
            double ratio = (x - CENTER_X) / A;
            if (ratio < -1.0 || ratio > 1.0) continue;
            int topY = (int)(WIN_HEIGHT - B * std::sqrt(1.0 - ratio * ratio));
            int g = 25 + (int)(16.0 * (1.0 - std::fabs(ratio)));
            fillA(r, x, topY, 1, WIN_HEIGHT - topY, 3, g, g + 7, 230);
        }
        // 外轮廓 + 内层装甲带。
        arcA(r, CENTER_X, WIN_HEIGHT, (int)A, (int)B, M_PI, M_PI * 2.0, 58, 210, 116, 255, 80);
        arcA(r, CENTER_X, WIN_HEIGHT, (int)(A - 9), (int)(B - 5), M_PI, M_PI * 2.0, 34, 128, 78, 220, 80);
        arcA(r, CENTER_X, WIN_HEIGHT, (int)(A - 20), (int)(B - 13), M_PI, M_PI * 2.0, 22, 79, 55, 190, 72);
        // 分段肋板：只画上半段，避免在底部中央堆成网格。
        for (int i = 0; i < 34; ++i) {
            double t = (double)i / 33.0;
            int sx = (int)(CENTER_X + A * (2.0 * t - 1.0));
            double ratio = (sx - CENTER_X) / A;
            if (ratio < -1.0 || ratio > 1.0) continue;
            int topY = (int)(WIN_HEIGHT - B * std::sqrt(1.0 - ratio * ratio));
            int a = 80 + (i % 3) * 18;
            lineA(r, sx, topY + 4, sx, WIN_HEIGHT - 2, 35, 148, 90, a);
            if (i % 5 == 0) {
                fillA(r, sx - 1, topY + 14, 2, 2, 110, 255, 157, 145);
            }
        }
        // 中央机库/跑道：一个深色梯形凹槽，让基地有功能中心。
        SDL_Point bay[4] = {{286, 552}, {514, 552}, {548, 600}, {252, 600}};
        ArtFX::fillPoly(r, bay, 4, 2, 20, 17, 238);
        lineA(r, 286, 552, 514, 552, 66, 223, 126, 220);
        lineA(r, 286, 552, 252, 600, 48, 158, 98, 160);
        lineA(r, 514, 552, 548, 600, 48, 158, 98, 160);
        // 机库中心跑道灯
        for (int i = 0; i < 9; ++i) {
            int y = 558 + i * 5;
            int w = 14 + i * 7;
            lineA(r, CENTER_X - w, y, CENTER_X - 8, y, 86, 255, 150, 100 + i * 9);
            lineA(r, CENTER_X + 8, y, CENTER_X + w, y, 86, 255, 150, 100 + i * 9);
        }
        // 停机坪/炮塔环：椭圆环让基地从"半圆线框"变成可停靠设施。
        arcA(r, CENTER_X, 553, 92, 13, 0, 2.0 * M_PI, 42, 162, 94, 170, 56);
        arcA(r, CENTER_X, 553, 77, 9, 0, 2.0 * M_PI, 92, 255, 158, 90, 48);
        lineA(r, CENTER_X - 92, 553, CENTER_X + 92, 553, 150, 255, 190, 95);
        // 两侧小防炮/天线，丰富剪影但不进入弹道区域。
        for (int side = -1; side <= 1; side += 2) {
            int bx = CENTER_X + side * 300;
            int by = 542;
            lineA(r, bx, by, bx + side * 28, by - 30, 61, 186, 111, 190);
            lineA(r, bx + side * 28, by - 30, bx + side * 18, by - 42, 74, 222, 135, 190);
            fillA(r, bx + side * 16, by - 45, 3, 3, 183, 255, 192, 190);
        }
        // 环境叙事：三处受损火点与轻烟。用 frame 做节奏，不引入随机状态。
        for (int k = 0; k < 3; ++k) {
            int fx = 214 + k * 168;
            int baseY = 550 + (k == 1 ? -11 : 0);
            int flick = 1 + (int)((frame / 3 + k * 5) % 4);
            fillA(r, fx, baseY, 2 + flick, 1, 255, 146 + k * 24, 50, 220);
            fillA(r, fx - 1, baseY - 2, 4 + flick, 2, 255, 194, 84, 160);
            for (int s = 0; s < 5; ++s) {
                int sx = fx + (int)(std::sin((frame + k * 37 + s * 19) * 0.035) * (5 + s * 2));
                int sy = baseY - 8 - s * 8 - (int)(frame / 7 + k * 3) % 10;
                int a = 34 - s * 5;
                fillA(r, sx, sy, 3 + s, 2 + s / 2, 82, 89, 84, a);
            }
        }
        // 外缘灯带与航标。
        drawBaseLights(r, frame);
        // 机库的柔和绿色地面反射。
        for (int i = 0; i < 5; ++i)
            arcA(r, CENTER_X, 600, 116 + i * 19, 12 + i * 3, M_PI, M_PI * 2.0,
                 44, 220, 107, 20 - i * 3, 56);
    }
};
