#pragma once
// ============== ShipArt ==============
// 暗夜精灵家族共用的像素画笔 + 三套机体绘制：
//   paintNightElf        现役 NightElf（定稿方案：大后掠翼 + 翼尖双发 + 中央主发）
//   paintDryad           休眠资产「树妖」（月银飞翼 + 三座短舱）
//   paintDruidWraithlight 休眠资产「德鲁伊·幽光」（无尾箭头 + 三缝尾喷）
// 纯绘制：只读参数，不修改任何游戏状态；所有坐标相对机头中心 (0,0)。
// ==============
#include <SDL.h>
#include "art_fx.h"
#include "constants.h"

namespace ShipArt {

struct Pen {
    SDL_Renderer* r;
    int px, py;
    int frame;

    Pen(SDL_Renderer* rr, int x, int y, int f) : r(rr), px(x), py(y), frame(f) {}

    SDL_Point at(int x, int y) const {
        SDL_Point p = {px + x, py + y};
        return p;
    }

    // 局部坐标多边形填充（点数上限与 ArtFX::fillPoly 一致）
    void fill(const int* v, int n, int cr, int cg, int cb, int ca = 255) const {
        if (!r || n < 3 || n > 32) return;
        SDL_Point p[32];
        for (int i = 0; i < n; ++i) { p[i].x = px + v[i * 2]; p[i].y = py + v[i * 2 + 1]; }
        ArtFX::fillPoly(r, p, n, cr, cg, cb, ca);
    }

    // 把同一组上半机坐标镜像到下半机再填充
    void fillMirror(const int* v, int n, int cr, int cg, int cb, int ca = 255) const {
        if (!r || n < 3 || n > 32) return;
        SDL_Point p[32];
        for (int i = 0; i < n; ++i) { p[i].x = px + v[i * 2]; p[i].y = py - v[i * 2 + 1]; }
        ArtFX::fillPoly(r, p, n, cr, cg, cb, ca);
    }

    // 对称机身上/下两半用不同明度填充：上亮下暗，形成体积感
    void fillSym(const int* v, int n, int tr, int tg, int tb, int br, int bg, int bb) const {
        fill(v, n, tr, tg, tb);
        fillMirror(v, n, br, bg, bb);
    }

    void line(int x0, int y0, int x1, int y1, int cr, int cg, int cb, int ca = 255) const {
        if (!r) return;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawLine(r, px + x0, py + y0, px + x1, py + y1);
    }

    void dot(int x, int y, int cr, int cg, int cb, int ca = 255) const {
        if (!r) return;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawPoint(r, px + x, py + y);
    }

    // 分层尾焰：短而宽的叶片，根部有实心亮核，尖端迅速收细。
    // 刻意不用 ArtFX::glowLine —— 它会画出三条平行条纹，像百叶窗。
    void flame(int nx, int ny, int len, int hw, int cr, int cg, int cb) const {
        int flick = (frame % 4 == 2) ? 1 : 0;
        int L = len + flick;
        // 外焰：软边，长度就是火焰总长
        int outer[18] = {nx + 1, ny, nx, ny - hw - 1, nx - 2, ny - hw - 1,
                         nx - L * 2 / 3, ny - 1, nx - L, ny,
                         nx - L * 2 / 3, ny + 1, nx - 2, ny + hw + 1,
                         nx, ny + hw + 1};
        fill(outer, 8, cr / 2, cg / 2, cb / 2, 96);

        // 内焰：根部宽、尖端收在 55% 处
        int L2 = L * 3 / 5;
        int inner[12] = {nx, ny - hw, nx - 2, ny - hw,
                         nx - L2, ny - 1, nx - L2 - 1, ny,
                         nx - L2, ny + 1, nx - 2, ny + hw};
        fill(inner, 6, cr, cg, cb, 175);

        // 白热核心只保留 3px，避免整条尾巴变成白色激光
        line(nx - 1, ny, nx - 4, ny, 240, 254, 255, 220);
        dot(nx - 1, ny, 252, 255, 255);
        dot(nx - 5, ny, 200, 244, 255, 130);
    }

    // 喷口：一个 2px 深的暗槽 + 上下唇口高光 + 中心白点
    void nozzle(int nx, int ny, int hw, int cr, int cg, int cb) const {
        int slot[8] = {nx - 1, ny - hw, nx + 1, ny - hw,
                       nx + 1, ny + hw, nx - 1, ny + hw};
        fill(slot, 4, 4, 7, 12, 255);
        line(nx - 1, ny - hw, nx + 1, ny - hw, cr, cg, cb, 190);
        line(nx - 1, ny + hw, nx + 1, ny + hw, cr, cg, cb, 190);
        dot(nx, ny, 244, 255, 255);
    }

    // 喷口外的一圈柔光，让三发在暗背景上先"亮"起来
    void exhaustGlow(int x, int y, int cr, int cg, int cb) const {
        int a[8] = {x + 1, y, x, y - 2, x - 4, y, x, y + 2};
        fill(a, 4, cr, cg, cb, 35);
        int b[8] = {x, y, x - 1, y - 1, x - 3, y, x - 1, y + 1};
        fill(b, 4, cr, cg, cb, 65);
    }

    // 中轴座舱：以 y=0 为轴线左右对称的尖头胶囊，沿用第一版概念稿的造型。
    // x 是座舱尾端，len 是总长，h 是玻璃半高（含框后总高 2h+2）。
    // 深框 + 深青玻璃 + 顶部高光，不再是一整块亮蓝色。
    void canopy(int x, int len, int h) const {
        int fr[12] = {x + len, 0, x + len - 2, -h - 1, x + 3, -h - 1,
                      x, 0, x + 3, h + 1, x + len - 2, h + 1};
        fill(fr, 6, 5, 9, 16, 255);
        int gl[12] = {x + len - 2, 0, x + len - 3, -h, x + 4, -h,
                      x + 2, 0, x + 4, h, x + len - 3, h};
        fill(gl, 6, 82, 176, 204, 235);
        line(x + 3, -1, x + len - 4, -1, 218, 250, 255, 235);
        line(x + 3, -h - 1, x + len - 3, -h - 1, 188, 234, 248, 190);
        dot(x + len - 5, -1, 248, 255, 255);
    }

    // 翼尖航行灯：上红下绿，沿用航空器识别语言
    void navLights(int x, int y) const {
        dot(x, y, 255, 92, 84, 225);
        dot(x, -y, 92, 255, 166, 225);
    }
};

inline void paintNightElf(SDL_Renderer* r, int px, int py, int tick) {
    ArtFX::BlendGuard guard(r);
    Pen p(r, px, py, tick);

    // 三股尾迹全部落在同一条尾线上：两侧翼尖短舱 -11 / 中央主发 0 / +11
    p.exhaustGlow(-15, -11, 96, 214, 246);
    p.exhaustGlow(-15, 11, 96, 214, 246);
    p.exhaustGlow(-16, 0, 120, 232, 255);
    p.flame(-15, -11, 9, 2, 84, 206, 255);
    p.flame(-15, 11, 9, 2, 84, 206, 255);
    p.flame(-16, 0, 12, 2, 104, 224, 255);

    // 大后掠翼：从机身一直铺到翼尖，翼尖集成两台发动机
    int wing[14] = {10, -4, 2, -9, -6, -14, -10, -14, -13, -10, -10, -4, -4, -5};
    p.fill(wing, 7, 30, 44, 60);
    p.fillMirror(wing, 7, 17, 25, 37);
    int lead[14] = {10, -4, 2, -9, -6, -14, -8, -14, -1, -10, 6, -6, 8, -4};
    p.fill(lead, 7, 48, 66, 86);
    p.fillMirror(lead, 7, 28, 39, 52);
    p.line(10, -4, 2, -9, 202, 244, 255, 220);
    p.line(2, -9, -6, -14, 168, 220, 246, 180);
    p.line(-6, -14, -10, -14, 118, 174, 204, 125);
    p.line(-10, -14, -13, -10, 82, 130, 158, 100);
    p.line(10, 4, 2, 9, 106, 156, 188, 130);
    p.line(2, 9, -6, 14, 82, 124, 152, 105);
    // 翼面主梁 + 控制面
    p.line(4, -6, -10, -12, 82, 122, 150, 95);
    p.line(4, 6, -10, 12, 58, 88, 110, 80);
    p.line(-2, -8, -11, -8, 4, 7, 12, 155);
    // 金色前缘细线：只在翼根到中段
    p.line(9, -5, -1, -10, 255, 196, 120, 110);

    // 翼尖发动机短舱：被机翼包住，只露出进气口和喷口
    int pod[16] = {-3, -10, -1, -11, -4, -12, -10, -12,
                   -15, -11, -10, -10, -4, -10, -2, -9};
    p.fill(pod, 8, 34, 48, 64);
    p.fillMirror(pod, 8, 22, 32, 45);
    int podLit[12] = {-3, -10, -2, -11, -9, -11, -14, -11,
                      -10, -10, -4, -10};
    p.fill(podLit, 6, 54, 72, 92);
    p.fillMirror(podLit, 6, 36, 48, 62);
    p.line(-3, -10, -1, -11, 214, 246, 255, 220);
    p.line(-1, -11, -10, -12, 160, 216, 242, 150);
    p.line(-3, 10, -1, 11, 74, 118, 148, 100);
    // 进气口金圈
    p.line(-3, -10, -2, -9, 255, 202, 128, 185);
    p.line(-3, 10, -2, 9, 160, 120, 74, 100);
    p.dot(-3, -10, 255, 226, 170, 220);

    // 中央前翼：小、利
    int canard[8] = {22, -2, 29, -6, 27, -7, 19, -3};
    p.fill(canard, 4, 34, 48, 66);
    p.fillMirror(canard, 4, 22, 32, 45);
    p.line(22, -2, 29, -6, 255, 202, 128, 220);
    p.line(29, -6, 27, -7, 206, 154, 92, 140);
    p.line(22, 2, 29, 6, 92, 124, 146, 85);

    // 中央机身
    int hull[18] = {34, 0, 28, -2, 22, -3, 16, -4, 8, -5, 0, -6,
                    -7, -5, -12, -4, -16, 0};
    p.fillSym(hull, 9, 24, 35, 50, 24, 35, 50);
    int dorsal[20] = {33, 0, 27, -1, 21, -2, 15, -3, 7, -4, 0, -5,
                      -6, -4, -11, -3, -15, 0, 33, 0};
    p.fill(dorsal, 10, 42, 60, 80);
    int spine[16] = {31, -1, 24, -2, 17, -3, 10, -4, 3, -4,
                     -3, -3, -8, -2, -13, 0};
    p.fill(spine, 8, 54, 76, 98);
    int ventral[18] = {33, 0, 27, 1, 21, 2, 15, 3, 7, 4, 0, 5,
                       -6, 4, -11, 3, -15, 0};
    p.fill(ventral, 9, 13, 19, 30);
    p.line(30, 2, 8, 4, 62, 94, 120, 90);

    p.line(34, 0, 28, -2, 232, 250, 255, 245);
    p.line(28, -2, 22, -3, 204, 240, 255, 210);
    p.line(22, -3, 16, -4, 170, 224, 248, 170);
    p.line(16, -4, 8, -5, 132, 196, 226, 135);
    p.line(8, -5, 0, -6, 96, 158, 190, 110);
    p.line(34, 0, 28, 2, 96, 146, 178, 145);
    p.line(28, 2, 22, 3, 78, 122, 154, 125);
    p.line(22, 3, 8, 5, 60, 98, 126, 100);

    // 金色脊线 + 机鼻传感器
    p.line(29, -1, 14, -3, 255, 196, 120, 115);
    p.dot(33, 0, 220, 242, 255, 240);

    // 中轴座舱：回到机身中线，尖头胶囊 + 金色脊线
    p.canopy(14, 10, 2);

    // 中央尾块 + 三喷口
    int block[12] = {-13, -5, -15, -4, -16, 0, -15, 4, -13, 5, -11, 0};
    p.fill(block, 6, 20, 30, 42);
    p.line(-13, -5, -15, -4, 150, 206, 234, 130);
    p.nozzle(-15, -11, 2, 104, 220, 250);
    p.nozzle(-15, 11, 2, 104, 220, 250);
    p.nozzle(-16, 0, 2, 128, 234, 255);

    p.navLights(-9, 13);
}

inline void paintDryad(SDL_Renderer* r, int px, int py, int tick) {
    ArtFX::BlendGuard guard(r);
    Pen p(r, px, py, tick);

    // 三股短尾焰先画，被翼面与短舱压住根部
    p.exhaustGlow(-16, -9, 96, 224, 220);
    p.exhaustGlow(-16, 9, 96, 224, 220);
    p.exhaustGlow(-17, 0, 120, 246, 240);
    p.flame(-16, -9, 9, 2, 78, 214, 210);
    p.flame(-16, 9, 9, 2, 78, 214, 210);
    p.flame(-17, 0, 12, 2, 104, 236, 224);

    // 翼尖羽刃：向后上方/后下方掠出，给飞翼一个羽刃式的收尾
    int feather[8] = {-4, -14, -11, -18, -14, -16, -8, -13};
    p.fill(feather, 4, 24, 36, 46);
    p.fillMirror(feather, 4, 15, 23, 32);
    p.line(-4, -14, -11, -18, 196, 204, 255, 170);
    p.line(-4, 14, -11, 18, 70, 122, 110, 95);

    // 新月翼面：机鼻细长，翼尖在 x≈-2 处张开到 ±15
    int wing[22] = {34, 0, 28, -2, 22, -4, 15, -8, 8, -12, 1, -15,
                    -6, -15, -11, -12, -11, -6, -8, -2, -10, 0};
    p.fill(wing, 11, 58, 68, 84);
    p.fillMirror(wing, 11, 42, 52, 66);

    // 前缘受光带：只压 3px 宽的一条，形成金属翼的"折边"
    int lead[26] = {34, 0, 28, -2, 22, -4, 15, -8, 8, -12, 1, -15,
                    -2, -12, 3, -11, 8, -9, 14, -6, 20, -3, 26, -1, 31, 0};
    p.fill(lead, 13, 88, 102, 120);

    // 后缘暗带：把翼面从"一块平板"压出纵深
    int trailing[20] = {-6, -15, -11, -12, -11, -6, -8, -2, -10, 0,
                        -7, 0, -5, -2, -8, -5, -8, -9, -4, -12};
    p.fill(trailing, 10, 30, 40, 54);
    p.fillMirror(trailing, 10, 24, 33, 45);

    // 前缘薄荷色光带
    p.line(33, 0, 28, -2, 206, 206, 255, 240);
    p.line(28, -2, 22, -4, 190, 196, 255, 220);
    p.line(22, -4, 15, -8, 166, 178, 255, 180);
    p.line(15, -8, 8, -12, 132, 150, 230, 140);
    p.line(8, -12, 1, -15, 96, 112, 190, 110);
    p.line(33, 0, 28, 2, 82, 94, 122, 95);
    p.line(28, 2, 22, 4, 66, 78, 104, 80);

    // 翼面主梁 + 控制面分缝
    p.line(27, -4, 2, -12, 72, 110, 112, 100);
    p.line(20, -6, 4, -12, 58, 90, 92, 85);
    p.line(10, -8, -8, -7, 4, 7, 12, 160);
    p.line(10, 8, -8, 7, 4, 7, 12, 130);
    // 翼根加强线
    p.line(-1, -4, -9, -3, 58, 86, 84, 105);

    // 中央脊线：让飞翼里仍有一条"机身"可读
    int spine[16] = {32, 0, 24, -1, 15, -2, 6, -3, -2, -2,
                     -8, -1, -12, 0, -12, 1};
    p.fill(spine, 8, 80, 94, 110);
    p.line(31, -1, 2, -3, 96, 150, 150, 90);
    p.line(2, -3, -10, -1, 64, 104, 106, 75);

    // 三座短舱：上下各一 + 中央一座，全部从翼后缘探出
    int pod[12] = {-7, -7, -11, -6, -16, -7, -16, -9, -12, -10, -8, -9};
    p.fill(pod, 6, 66, 78, 94);
    p.fillMirror(pod, 6, 48, 58, 72);
    p.line(-7, -7, -12, -6, 176, 190, 255, 130);
    p.line(-7, 7, -12, 6, 56, 100, 96, 90);
    int centerPod[12] = {-8, -2, -12, -1, -17, -2, -17, 2, -12, 1, -8, 2};
    p.fill(centerPod, 6, 72, 86, 102);
    p.line(-8, -2, -12, -1, 196, 206, 255, 135);

    // 中轴座舱：回到机身中线，尖头胶囊 + 月紫高光
    p.canopy(15, 10, 2);

    // 翼面双光点：只保留一点短光斑
    p.line(-3, -9, -1, -9, 150, 168, 240, 80);
    p.dot(-2, -9, 196, 210, 255, 190);
    p.line(-3, 9, -1, 9, 130, 148, 220, 70);
    p.dot(-2, 9, 196, 210, 255, 170);

    // 喷口
    p.nozzle(-16, -9, 2, 104, 226, 214);
    p.nozzle(-16, 9, 2, 104, 226, 214);
    p.nozzle(-17, 0, 2, 180, 248, 255);

    p.navLights(-5, 15);
}

inline void paintDruidWraithlight(SDL_Renderer* r, int px, int py, int tick) {
    ArtFX::BlendGuard guard(r);
    Pen p(r, px, py, tick);

    // 三缝式喷口：-7 / 0 / +7，尾焰收得很短，像高压喷流
    p.exhaustGlow(-14, -7, 110, 218, 255);
    p.exhaustGlow(-14, 7, 110, 218, 255);
    p.exhaustGlow(-13, 0, 138, 236, 255);
    p.flame(-14, -7, 8, 2, 104, 216, 255);
    p.flame(-14, 7, 8, 2, 104, 216, 255);
    p.flame(-13, 0, 11, 2, 132, 234, 255);

    // 微型外倾尾刃：只露一点，保持隐身机的干净
    int fin[8] = {-2, -14, -9, -18, -12, -16, -6, -12};
    p.fill(fin, 4, 18, 28, 40);
    p.fillMirror(fin, 4, 12, 19, 28);
    p.line(-2, -14, -9, -18, 176, 232, 255, 135);
    p.line(-2, 14, -9, 18, 70, 112, 146, 95);

    // 箭头翼面：长机鼻 + 折线前缘 + 平直后缘
    int hull[22] = {36, 0, 30, -2, 24, -4, 18, -7, 11, -12, 3, -15,
                    -5, -15, -13, -10, -15, -6, -13, -2, -13, 0};
    p.fillSym(hull, 11, 24, 35, 50, 24, 35, 50);

    // 上表面：一整块受光折面，前缘再压一条更亮的窄带
    int facet[22] = {35, 0, 29, -2, 23, -4, 17, -6, 10, -11, 3, -13,
                     -3, -13, -10, -9, -12, -6, -10, -3, -10, 0};
    p.fill(facet, 11, 40, 56, 74);

    // 前缘亮带：冰白色，宽度 2~3px
    int lead[24] = {36, 0, 30, -2, 24, -4, 18, -7, 11, -12, 3, -15,
                    0, -13, 8, -10, 15, -6, 21, -3, 27, -1, 33, 0};
    p.fill(lead, 12, 60, 82, 104);

    // 后缘暗带：把翼面从"平板"压出纵深
    int trail[20] = {-5, -15, -13, -10, -15, -6, -13, -2, -13, 0,
                     -10, 0, -10, -3, -12, -6, -10, -9, -8, -13};
    p.fill(trail, 10, 14, 20, 30);
    p.fillMirror(trail, 10, 10, 15, 24);

    // 机头两侧折线前翼（chined forebody）
    int chine[8] = {27, -2, 33, -4, 31, -5, 25, -3};
    p.fill(chine, 4, 36, 52, 70);
    p.fillMirror(chine, 4, 22, 32, 46);
    p.line(27, -2, 33, -4, 216, 246, 255, 180);
    p.line(27, 2, 33, 4, 84, 124, 154, 95);

    // 冰白轮廓光：机鼻 → 折线前缘 → 翼尖
    p.line(36, 0, 30, -2, 240, 252, 255, 248);
    p.line(30, -2, 24, -4, 222, 246, 255, 220);
    p.line(24, -4, 18, -7, 194, 234, 255, 185);
    p.line(18, -7, 11, -12, 156, 212, 242, 145);
    p.line(11, -12, 3, -15, 118, 176, 210, 118);
    p.line(36, 0, 30, 2, 96, 148, 180, 105);
    p.line(30, 2, 24, 4, 70, 112, 142, 85);

    // 翼面分缝：两级冷灰面板
    p.line(30, -1, 8, -6, 130, 170, 200, 100);
    p.line(8, -6, -8, -10, 82, 124, 154, 95);
    p.line(20, -4, -4, -9, 58, 94, 120, 85);
    p.line(20, 4, -4, 9, 46, 76, 100, 70);
    p.line(-3, -12, -10, -8, 4, 7, 13, 165);
    p.line(-3, 12, -10, 8, 4, 7, 13, 135);

    // 鼻端感应器
    p.line(31, -1, 20, -2, 180, 214, 240, 100);
    p.dot(35, 0, 234, 248, 255, 240);

    // 中轴座舱：回到机身中线，窄一点的尖头胶囊
    p.canopy(15, 9, 2);

    // 尾部三喷口：中间一个略靠后，形成"三缝"节奏
    int notchU[6] = {-11, -5, -13, -4, -13, -2};
    int notchL[6] = {-11, 5, -13, 4, -13, 2};
    p.fill(notchU, 3, 4, 7, 13, 220);
    p.fill(notchL, 3, 4, 7, 13, 220);
    p.nozzle(-14, -7, 2, 120, 226, 255);
    p.nozzle(-14, 7, 2, 120, 226, 255);
    p.nozzle(-13, 0, 2, 146, 240, 255);

    p.navLights(-6, 15);
}


// ---------------- 训练机 / 训练机升级 共用双视角画笔 ----------------
// 训练机 B（雨燕）与升级型 D（狮鹫）共用同一套"俯视平面图"：
// 第一章透视 + 第二章旋转 90° 侧滚，保证两章是同一架飞机。
struct TrainerView {
    SDL_Renderer* r;
    int px, py;
    bool side;
    double hnx, hny, c, s;
    int tick;

    TrainerView(SDL_Renderer* rr, int x, int y, bool sd, double roll, int tk)
        : r(rr), px(x), py(y), side(sd), hnx(0.0), hny(-1.0), c(1.0), s(0.0), tick(tk) {
        if (!side) {
            double tPlane = (px - perspLeft(py)) / perspWidth(py);
            if (tPlane < 0.0) tPlane = 0.0;
            if (tPlane > 1.0) tPlane = 1.0;
            double hx = perspLeft(HORIZON_Y) + tPlane * perspWidth(HORIZON_Y);
            double hdx = hx - px, hdy = HORIZON_Y - py;
            if (hdy > 0) { hdx = 0; hdy = -1; }
            double hlen = std::sqrt(hdx*hdx + hdy*hdy);
            if (hlen < 0.01) { hdx = 0; hdy = -1; hlen = 1; }
            hnx = hdx / hlen;
            hny = hdy / hlen;
            c = std::cos(roll);
            s = std::sin(roll);
        }
    }

    // 机身/尾翼：第一章只做透视，不跟随机翼横滚
    SDL_Point map(int lx, int ly) const {
        if (side) {
            SDL_Point p = {px - ly, py + lx};
            return p;
        }
        SDL_Point p = {px + (int)(lx * (-hny) - ly * hnx),
                       py + (int)(lx * hnx + ly * (-hny))};
        return p;
    }

    // 机翼：第一章跟随机翼横滚
    SDL_Point mapWing(int lx, int ly) const {
        if (side) {
            SDL_Point p = {px - ly, py + lx};
            return p;
        }
        double rx = lx * c, ry = ly + lx * s * 0.40;
        SDL_Point p = {px + (int)(rx * (-hny) - ry * hnx),
                       py + (int)(rx * hnx + ry * (-hny))};
        return p;
    }

    void fill(const int* v, int n, int cr, int cg, int cb, int ca = 255, bool wing = false) const {
        if (!r || n < 3 || n > 32) return;
        SDL_Point p[32];
        for (int i = 0; i < n; ++i)
            p[i] = wing ? mapWing(v[i * 2], v[i * 2 + 1]) : map(v[i * 2], v[i * 2 + 1]);
        ArtFX::fillPoly(r, p, n, cr, cg, cb, ca);
    }

    // 右半机翼/尾翼镜像到左半
    void fillMirror(const int* v, int n, int cr, int cg, int cb, int ca = 255, bool wing = false) const {
        if (!r || n < 3 || n > 32) return;
        SDL_Point p[32];
        for (int i = 0; i < n; ++i) {
            int lx = -v[i * 2], ly = v[i * 2 + 1];
            p[i] = wing ? mapWing(lx, ly) : map(lx, ly);
        }
        ArtFX::fillPoly(r, p, n, cr, cg, cb, ca);
    }

    void line(int x0, int y0, int x1, int y1, int cr, int cg, int cb, int ca = 255, bool wing = false) const {
        if (!r) return;
        SDL_Point a = wing ? mapWing(x0, y0) : map(x0, y0);
        SDL_Point b = wing ? mapWing(x1, y1) : map(x1, y1);
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawLine(r, a.x, a.y, b.x, b.y);
    }

    void dot(int lx, int ly, int cr, int cg, int cb, int ca = 255) const {
        if (!r) return;
        SDL_Point p = map(lx, ly);
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
        SDL_RenderDrawPoint(r, p.x, p.y);
    }

    // 尾焰：局部 +y 是机尾方向，所以第一章朝画面下方、第二章朝画面左侧
    void flame(int lx, int ly, int len, int hw, int cr, int cg, int cb) const {
        int L = len + ((tick % 4 == 2) ? 1 : 0);
        int outer[10] = {lx - hw, ly, lx + hw, ly,
                         lx + hw - 1, ly + L * 2 / 3,
                         lx, ly + L,
                         lx - hw + 1, ly + L * 2 / 3};
        fill(outer, 5, cr / 2, cg / 2, cb / 2, 95);
        int inner[6] = {lx - hw + 1, ly, lx + hw - 1, ly, lx, ly + L * 3 / 5};
        fill(inner, 3, cr, cg, cb, 170);
        line(lx, ly, lx, ly + L * 2 / 5, 235, 252, 255, 215);
        dot(lx, ly, 252, 255, 255);
    }

    void nozzle(int lx, int ly, int hw, int cr, int cg, int cb) const {
        int slot[8] = {lx - 1, ly - hw, lx + 1, ly - hw,
                       lx + 1, ly + hw, lx - 1, ly + hw};
        fill(slot, 4, 4, 7, 12, 255);
        line(lx - 1, ly - hw, lx + 1, ly - hw, cr, cg, cb, 190);
        line(lx - 1, ly + hw, lx + 1, ly + hw, cr, cg, cb, 190);
        dot(lx, ly, 244, 255, 255);
    }

    // 中轴座舱：俯视图是尖头胶囊；第二章旋转后是一条横向玻璃缝
    void canopy(int ly, int len, int hw) const {
        int fr[12] = {0, ly - len / 2, hw + 1, ly - len / 4,
                      hw + 1, ly + len / 4, 0, ly + len / 2,
                      -hw - 1, ly + len / 4, -hw - 1, ly - len / 4};
        fill(fr, 6, 5, 9, 16, 255);
        int gl[12] = {0, ly - len / 2 + 1, hw, ly - len / 4,
                      hw, ly + len / 4, 0, ly + len / 2 - 1,
                      -hw, ly + len / 4, -hw, ly - len / 4};
        fill(gl, 6, 72, 168, 196, 235);
        line(0, ly - len / 2 + 1, 0, ly + len / 2 - 1, 225, 250, 255, 230);
        dot(0, ly - len / 2 + 2, 250, 255, 255);
    }

    void nav(int lx, int ly) const {
        dot(-lx, ly, 255, 92, 84, 225);
        dot(lx, ly, 92, 255, 166, 225);
    }
};

static void paintSwift(TrainerView& v) {
    v.flame(0, 11, 8, 2, 100, 220, 255);

    int wr[8] = {3, -1, 13, 7, 12, 10, 3, 6};
    v.fill(wr, 4, 26, 38, 54, 255, true);
    v.fillMirror(wr, 4, 26, 38, 54, 255, true);
    v.line(3, -1, 13, 7, 228, 246, 255, 225, true);
    v.line(13, 7, 12, 10, 176, 218, 240, 150, true);
    v.line(12, 10, 3, 6, 100, 150, 180, 110, true);
    v.line(-3, -1, -13, 7, 228, 246, 255, 225, true);
    v.line(-13, 7, -12, 10, 176, 218, 240, 150, true);

    int tail[8] = {1, 9, 6, 13, 5, 14, 0, 12};
    v.fill(tail, 4, 24, 36, 50, 255, true);
    v.fillMirror(tail, 4, 24, 36, 50, 255, true);
    v.line(1, 9, 6, 13, 180, 220, 240, 160, true);
    v.line(-1, 9, -6, 13, 180, 220, 240, 160, true);

    int hull[16] = {0, -16, 2, -10, 3, -2, 2, 8, 0, 12, -2, 8, -3, -2, -2, -10};
    v.fill(hull, 8, 18, 28, 40);
    int spine[12] = {0, -14, 1, -7, 1, 6, 0, 9, -1, 6, -1, -7};
    v.fill(spine, 6, 42, 58, 76);
    v.line(0, -16, 2, -10, 234, 250, 255, 240);
    v.line(2, -10, 3, -2, 184, 224, 244, 180);
    v.line(3, -2, 2, 8, 120, 170, 200, 120);
    v.line(0, -16, -2, -10, 234, 250, 255, 240);
    v.line(-2, -10, -3, -2, 184, 224, 244, 180);

    // 冰蓝训练涂装
    v.line(0, -15, 0, -12, 130, 220, 255, 220);
    v.line(7, 2, 9, 6, 130, 220, 255, 170, true);
    v.line(-7, 2, -9, 6, 130, 220, 255, 170, true);

    v.canopy(-7, 9, 2);
    v.nozzle(0, 11, 2, 110, 224, 252);
    v.nav(13, 8);
}

static void paintGryphon(TrainerView& v) {
    v.flame(0, 10, 8, 2, 110, 224, 255);

    int wr[8] = {4, -3, 14, 9, 13, 12, 4, 7};
    v.fill(wr, 4, 26, 38, 54, 255, true);
    v.fillMirror(wr, 4, 26, 38, 54, 255, true);
    v.line(4, -3, 14, 9, 230, 246, 255, 225, true);
    v.line(14, 9, 13, 12, 176, 218, 240, 150, true);
    v.line(13, 12, 4, 7, 100, 150, 180, 105, true);
    v.line(-4, -3, -14, 9, 230, 246, 255, 225, true);

    // 前翼
    int canard[8] = {4, -9, 8, -6, 7, -5, 3, -7};
    v.fill(canard, 4, 40, 56, 74, 255, true);
    v.fillMirror(canard, 4, 40, 56, 74, 255, true);
    v.line(4, -9, 8, -6, 255, 205, 120, 210, true);
    v.line(-4, -9, -8, -6, 255, 205, 120, 210, true);

    int hull[16] = {0, -16, 3, -10, 4, -2, 3, 7, 0, 11, -3, 7, -4, -2, -3, -10};
    v.fill(hull, 8, 18, 28, 40);
    int spine[12] = {0, -14, 2, -8, 2, 7, 0, 9, -2, 7, -2, -8};
    v.fill(spine, 6, 42, 58, 76);
    v.line(0, -16, 3, -10, 234, 250, 255, 240);
    v.line(3, -10, 4, -2, 184, 224, 244, 180);
    v.line(4, -2, 3, 7, 120, 170, 200, 115);
    v.line(0, -16, -3, -10, 234, 250, 255, 240);
    v.line(-3, -10, -4, -2, 184, 224, 244, 180);

    // 金色训练涂装
    v.line(-2, -13, 2, -13, 255, 205, 120, 230);
    v.line(9, 3, 11, 9, 255, 205, 120, 160, true);
    v.line(-9, 3, -11, 9, 255, 205, 120, 160, true);

    v.canopy(-7, 9, 2);
    v.nozzle(0, 10, 2, 110, 224, 252);
    v.nav(13, 10);
}

} // namespace ShipArt
