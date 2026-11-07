#pragma once

#include "../constants.h"
#include "../renderer.h"
#include "../audio.h"
#include <algorithm>
#include <cmath>

// ============== Ch2GateScene (Chapter 2 opening: vacuum door + pulse unlock sequence) ==============
// Scripted scene: the trainer arrives at Moonwell's vacuum gate.
//   DIALOGUE  - characters talk (driven by Game via DialogueSystem)
//   PULSING   - Ally fires 5 pulse groups (2s apart), 5 concentric rings each, draining energy bar
//   SCANNING  - the facility is force-booted: status cells light up one by one, the scanner
//               emitter flickers, then a red fan sweeps down and locks onto the plane
//   OPENING   - lock bolts retract, the massive panels shudder and split open
//   OPEN      - player flies right through the doorway
//
// Art direction: the whole gate is drawn as layered hard-surface line art on dark fills
// (matching the side-scroll style), with three light states telling the story:
//   dead -> amber emergency boot -> tense red scan -> cool cyan "door open".
// All animation is derived from serialized fields only (stage / timer / doorOpen / beamTimer /
// pulseCount), so no save layout change is needed and read-back frames stay deterministic.
class Ch2GateScene {
public:
    enum Stage { DIALOGUE, PULSING, SCANNING, OPENING, OPEN };

    // ---- Layout (screen space) ----
    static const int DOOR_L = 625;         // outer structure left edge
    static const int DOOR_R = WIN_WIDTH;   // outer structure right edge (continues off-screen)
    static const int AP_L = 655;           // door aperture left edge
    static const int AP_R = 775;           // door aperture right edge
    static const int DOOR_T = 36;          // aperture top (under the top beam)
    static const int DOOR_B = 564;         // aperture bottom (above the bottom beam)
    static const int DOOR_MID = 300;       // horizontal split line between the two panels
    static const int SCAN_X = 702;         // scanner gimbal center
    static const int SCAN_TOP_Y = 54;      // top emitter lens
    static const int SCAN_BOT_Y = 546;     // bottom emitter lens

    // ---- Animation timeline (frames) ----
    enum {
        SCAN_BOOT_END   = 28,   // emitter boot / flicker
        SCAN_DEPLOY_END = 54,   // cone extends
        SCAN_SWEEP_END  = 116,  // cone swings down and locks
        SCAN_VERIFY_AT  = 196,  // ID accepted: reticle turns green
        SCAN_HOLD_END   = 206,  // beam starts retracting
        SCAN_TOTAL      = 268,  // scan finished -> OPENING
        OPEN_UNLOCK     = 36,   // bolts retract / panels shudder before moving
        OPEN_TRAVEL     = 112,  // panel slide duration (doorOpen 0 -> 1)
        PANEL_TRAVEL    = 350   // pixels each panel slides to clear the aperture
    };

    struct RingGroup {
        double r0;      // base radius (innermost ring)
        int age;        // frames alive
        bool alive;
        template <class Ar> void visit(Ar& ar) { ar.ioNum(r0); ar.ioNum(age); ar.ioBool(alive); }
    };

    Stage stage;
    int timer;
    int pulseCount;                 // groups emitted so far (0..5)
    std::vector<RingGroup> groups;  // active expanding ring groups
    double doorOpen;                // 0..1 (panel split progress)
    bool lightsOn;
    bool beamActive;
    int beamTimer;
    bool justEmitted;               // pulse group emitted this frame (consumed by Game)

    Ch2GateScene() { reset(); }

    void reset() {
        stage = DIALOGUE; timer = 0; pulseCount = 0;
        groups.clear(); doorOpen = 0.0;
        lightsOn = false; beamActive = false; beamTimer = 0;
        justEmitted = false;
    }

    Stage getStage() const { return stage; }
    bool isOpen() const { return stage == OPEN; }
    bool isFullyOpen() const { return doorOpen >= 1.0; }

    void startPulsing() { if (stage == DIALOGUE) { stage = PULSING; timer = 0; } }

    // TAB skip: jump straight to the open door (no dialogue / pulses / scan)
    void skipToOpen() {
        stage = OPEN; doorOpen = 1.0; groups.clear();
        beamActive = false; lightsOn = true; pulseCount = 5;
    }

    // 存档：门禁序列阶段 + 计时 + 扩散中的脉冲环组 + 开门进度 + 扫描光束
    template <class Ar> void visit(Ar& ar) {
        ar.ioEnum(stage); ar.ioNum(timer); ar.ioNum(pulseCount);
        ar.ioVecObj(groups);
        ar.ioNum(doorOpen); ar.ioBool(lightsOn); ar.ioBool(beamActive);
        ar.ioNum(beamTimer); ar.ioBool(justEmitted);
    }

    // Returns 1 if a pulse group was emitted this frame (Game drains the energy bar)
    int update(double, double, AudioEngine& audio) {
        justEmitted = false;
        timer++;

        // Ring groups expand & fade
        for (auto& g : groups) { g.age++; g.r0 += 2.8; if (g.age > 60) g.alive = false; }
        groups.erase(std::remove_if(groups.begin(), groups.end(),
            [](const RingGroup& g){ return !g.alive; }), groups.end());

        switch (stage) {
        case DIALOGUE: break;

        case PULSING: {
            // One pulse group every 2 seconds (120 frames), 5 groups total
            if (timer % 120 == 0 && pulseCount < 5) {
                RingGroup g;
                g.r0 = 18.0; g.age = 0; g.alive = true;
                groups.push_back(g);
                pulseCount++;
                justEmitted = true;
                audio.sndShockwave();
            }
            // After the 5th group finishes expanding → scan stage
            if (pulseCount >= 5 && groups.empty()) {
                stage = SCANNING; timer = 0;
                lightsOn = true; beamActive = true; beamTimer = 0;
            }
            break;
        }

        case SCANNING: {
            beamTimer++;
            // 扫描器通电 → 锁定 → 验证通过，三个节点各给一声，强化"设施被强行唤醒"的压迫感
            if (beamTimer == SCAN_BOOT_END) audio.sndTripleCountdown();
            if (beamTimer == SCAN_SWEEP_END) audio.sndTripleOn();
            if (beamTimer == SCAN_VERIFY_AT) audio.sndTripleOn();
            if (beamTimer >= SCAN_TOTAL) {
                beamActive = false; stage = OPENING; timer = 0;
            }
            break;
        }

        case OPENING: {
            // 前 36 帧是解锁段：门闩回缩、门板震颤，门缝还没张开
            if (timer == OPEN_UNLOCK) audio.sndShockwave();
            if (timer > OPEN_UNLOCK) {
                doorOpen += 1.0 / OPEN_TRAVEL;
                if (doorOpen >= 1.0) { doorOpen = 1.0; stage = OPEN; timer = 0; }
            }
            break;
        }

        case OPEN: break;
        }
        return justEmitted ? 1 : 0;
    }

    // ==================================================================================
    // DRAW: vacuum door / hull / scanners (background layer)
    // ==================================================================================
    void drawDoor(SDL_Renderer* r) const {
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        drawHull(r);
        const double off = PANEL_TRAVEL * openEase();
        // 门洞底色：解锁段门板震颤会露出 1~2px 缝，避免透出星空
        fill(r, AP_L, DOOR_T, AP_R - AP_L, DOOR_B - DOOR_T, 8, 10, 14);
        drawInterior(r, off);
        {
            SDL_Rect clip = {AP_L, DOOR_T, AP_R - AP_L, DOOR_B - DOOR_T};
            SDL_RenderSetClipRect(r, &clip);
            drawPanel(r, true, off);
            drawPanel(r, false, off);
            drawSeamLocks(r);
            SDL_RenderSetClipRect(r, NULL);
        }
        drawFrame(r);
        drawScanners(r);
        drawUnlockFlash(r);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

    // ======== 白色脉冲环（Ally 的强制唤醒信号）========
    void drawRings(SDL_Renderer* r, double px, double py) const {
        // 圆环靠 alpha 做淡出；SDL 默认绘制混合是 NONE，必须显式开 BLEND 才能渐隐
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (const auto& g : groups) {
            double fade = 1.0 - (double)g.age / 60.0;
            if (fade <= 0) continue;
            // 5 concentric circles: outermost brightest, innermost darkest
            for (int k = 4; k >= 0; --k) {
                double ringR = g.r0 + k * 16.0;
                if (ringR < 4) continue;
                double ringFade = fade * (k == 4 ? 1.0 : (0.55 + 0.45 * k / 4.0));
                int alpha = (int)(215.0 * ringFade);
                if (alpha < 8) continue;
                SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)alpha);
                const int SEG = 48;
                SDL_Point prev;
                for (int i = 0; i <= SEG; ++i) {
                    double a = 2.0 * M_PI * i / SEG;
                    int sx = (int)(px + ringR * std::cos(a));
                    int sy = (int)(py + ringR * std::sin(a));
                    if (i > 0) SDL_RenderDrawLine(r, prev.x, prev.y, sx, sy);
                    prev = {sx, sy};
                }
            }
        }
        SDL_SetRenderDrawBlendMode(r, prev);
    }

    // ==================================================================================
    // DRAW: red scanner startup + fan beam (over the plane)
    // ==================================================================================
    void drawScanBeam(SDL_Renderer* r, double px, double py) const {
        if (!beamActive || stage != SCANNING) return;

        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        // ---- 0. 通电抖动：镜头还没射出光束，只有电火花与红辉 ----
        if (beamTimer < SCAN_BOOT_END) {
            drawEmitterSparks(r, true);
            drawEmitterSparks(r, false);
            drawScanVignette(r, 0.30);
            SDL_SetRenderDrawBlendMode(r, prev);
            return;
        }

        // ---- 1. 光锥的展开 / 下扫 / 锁定 / 回收包络 ----
        double ext = 1.0, halfW = 24.0, alpha = 1.0;
        double deployRaw = clamp01((beamTimer - SCAN_BOOT_END) /
                                   double(SCAN_DEPLOY_END - SCAN_BOOT_END));
        if (beamTimer < SCAN_DEPLOY_END) {
            ext = easeOutCubic(deployRaw);
            halfW = 48.0 - 16.0 * deployRaw;
        } else if (beamTimer < SCAN_SWEEP_END) {
            double raw = (beamTimer - SCAN_DEPLOY_END) /
                         double(SCAN_SWEEP_END - SCAN_DEPLOY_END);
            halfW = 32.0 - 9.0 * smooth01(raw);
        } else if (beamTimer < SCAN_HOLD_END) {
            halfW = 23.0 + 1.6 * std::sin(beamTimer * 0.40);
        } else {
            double raw = clamp01((beamTimer - SCAN_HOLD_END) /
                                 double(SCAN_TOTAL - SCAN_HOLD_END));
            ext = 1.0 - easeInCubic(raw);
            halfW = 23.0 * (1.0 - 0.65 * raw);
            alpha = 1.0 - raw;
        }

        // 供电不稳：刚启动时红束会断续，稳定后才连续扫描
        if (beamTimer < SCAN_DEPLOY_END + 10 && hashU((unsigned)(beamTimer / 2 + 7)) % 5 == 0) {
            ext *= 0.35;
            alpha *= 0.55;
        }

        // 目标点：先在门洞里立起一道扫描帘，再整体扫向玩家并锁定
        double aimX = SCAN_X - 6.0;
        double aimY = DOOR_T + 40.0;
        if (beamTimer >= SCAN_SWEEP_END) {
            aimX = px; aimY = py;
        } else if (beamTimer > SCAN_DEPLOY_END) {
            double raw = smooth01((beamTimer - SCAN_DEPLOY_END) /
                                  double(SCAN_SWEEP_END - SCAN_DEPLOY_END));
            aimX += (px - aimX) * raw;
            aimY += (py - aimY) * raw;
        }

        bool verified = beamTimer >= SCAN_VERIFY_AT;
        int cr = verified ? 120 : 255;
        int cg = verified ? 255 : 62;
        int cb = verified ? 168 : 58;

        // ---- 2. 上下两束锥光（下束稍暗，形成扫描门架）----
        if (ext > 0.02) {
            // 下束先画，避免压住上束
            drawCone(r, SCAN_X, SCAN_BOT_Y, aimX, aimY, halfW * 0.95, ext,
                     alpha * 0.80, cr, cg, cb);
            drawCone(r, SCAN_X, SCAN_TOP_Y, aimX, aimY, halfW, ext,
                     alpha, cr, cg, cb);

            // 光锥内的扫描栅格：三道亮带从镜头向目标推移
            if (beamTimer >= SCAN_DEPLOY_END) {
                for (int k = 0; k < 3; ++k) {
                    double pos = std::fmod(beamTimer * 0.011 + k / 3.0, 1.0);
                    double t = 0.08 + 0.92 * pos;
                    double hx = SCAN_X + (aimX - SCAN_X) * t * ext;
                    double hy = SCAN_TOP_Y + (aimY - SCAN_TOP_Y) * t * ext;
                    double w = halfW * (0.10 + 0.90 * t);
                    double dx = aimX - SCAN_X, dy = aimY - SCAN_TOP_Y;
                    double len = std::sqrt(dx * dx + dy * dy);
                    if (len < 1.0) continue;
                    double nx = -dy / len, ny = dx / len;
                    setColorA(r, 255, 175, 170, (int)(alpha * 54));
                    SDL_RenderDrawLine(r, (int)(hx + nx * w), (int)(hy + ny * w),
                                          (int)(hx - nx * w), (int)(hy - ny * w));
                }
            }
        }

        // ---- 3. 玩家身上的锁定框 + 环形锁进度 ----
        if (beamTimer >= SCAN_DEPLOY_END) {
            double lockT = clamp01((beamTimer - SCAN_DEPLOY_END) /
                                   double(SCAN_SWEEP_END - SCAN_DEPLOY_END));
            drawReticle(r, aimX, aimY, lockT, verified, beamTimer);
            // 锁定后的栅格扫描：细横线从锁定框上沿往下刷
            if (lockT > 0.99) {
                int top = (int)aimY - 26, bot = (int)aimY + 26;
                for (int y = top; y <= bot; y += 4) {
                    int sweepY = top + ((int)(beamTimer * 2.2) % (bot - top));
                    int d = std::abs(y - sweepY);
                    int a = d < 5 ? 90 : 22;
                    line(r, (int)aimX - 26, y, (int)aimX + 26, y,
                         verified ? 120 : 255, verified ? 255 : 96, verified ? 170 : 88, a);
                }
            }
        }

        // ---- 4. 验证通过的绿色余波 ----
        if (verified && beamTimer < SCAN_HOLD_END + 26) {
            int age = beamTimer - SCAN_VERIFY_AT;
            double rr = 26.0 + age * 5.0;
            int a = (int)(210.0 * clamp01(1.0 - age / 26.0));
            arc(r, aimX, aimY, rr, 0, 2.0 * M_PI, 120, 255, 178, a);
            arc(r, aimX, aimY, rr + 7, 0, 2.0 * M_PI, 90, 220, 150, a / 2);
        }

        // ---- 5. 红色压迫感（屏幕边缘）与锁定闪光 ----
        double vig = 0.35 + 0.25 * deployRaw;
        if (beamTimer >= SCAN_SWEEP_END) {
            vig = 0.62 + 0.18 * std::sin(beamTimer * 0.22);
            if (verified) vig *= 0.45;
        }
        if (beamTimer >= SCAN_HOLD_END) {
            double raw = clamp01((beamTimer - SCAN_HOLD_END) /
                                 double(SCAN_TOTAL - SCAN_HOLD_END));
            vig *= (1.0 - raw);
        }
        drawScanVignette(r, vig);

        // 锁定瞬间：只让锁定框闪一下白，不做全屏红闪（避免像画面故障）
        if (beamTimer >= SCAN_SWEEP_END && beamTimer < SCAN_SWEEP_END + 4 && !verified) {
            int a = 190 - (beamTimer - SCAN_SWEEP_END) * 45;
            arc(r, aimX, aimY, 30 + (beamTimer - SCAN_SWEEP_END) * 4, 0, 2.0 * M_PI,
                255, 170, 150, a);
        }

        SDL_SetRenderDrawBlendMode(r, prev);
    }

private:
    // ==================== small math / color helpers ====================
    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double smooth01(double t) { t = clamp01(t); return t * t * (3.0 - 2.0 * t); }
    static double easeOutCubic(double t) { t = clamp01(t); double u = 1.0 - t; return 1.0 - u * u * u; }
    static double easeInCubic(double t) { t = clamp01(t); return t * t * t; }

    // 确定性噪声：绘制只依赖帧号，读档逐像素校验不会漂移
    static unsigned hashU(unsigned x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
    static double hash01(int x) { return (hashU((unsigned)x) % 4096) / 4096.0; }

    // 带透明度的绘制（调用方已开 BLENDMODE_BLEND）
    static void setColorA(SDL_Renderer* r, int cr, int cg, int cb, int ca) {
        if (ca < 0) ca = 0; if (ca > 255) ca = 255;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
    }
    static void fill(SDL_Renderer* r, int x, int y, int w, int h,
                     int cr, int cg, int cb, int ca = 255) {
        setColorA(r, cr, cg, cb, ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderFillRect(r, &rr);
    }
    static void box(SDL_Renderer* r, int x, int y, int w, int h,
                    int cr, int cg, int cb, int ca = 255) {
        setColorA(r, cr, cg, cb, ca);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderDrawRect(r, &rr);
    }
    static void line(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                     int cr, int cg, int cb, int ca = 255) {
        setColorA(r, cr, cg, cb, ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void arc(SDL_Renderer* r, double cx, double cy, double rad,
                    double a0, double a1, int cr, int cg, int cb, int ca) {
        int steps = (int)(std::fabs(a1 - a0) * rad / 3.0);
        if (steps < 2) steps = 2;
        if (steps > 96) steps = 96;
        int px = (int)(cx + rad * std::cos(a0));
        int py = (int)(cy + rad * std::sin(a0));
        for (int i = 1; i <= steps; ++i) {
            double a = a0 + (a1 - a0) * i / steps;
            int nx = (int)(cx + rad * std::cos(a));
            int ny = (int)(cy + rad * std::sin(a));
            if (i < steps) { setColorA(r, cr, cg, cb, ca); SDL_RenderDrawLine(r, px, py, nx, ny); }
            px = nx; py = ny;
        }
    }

    // ==================== animation state → visuals ====================
    // 设施供电等级：0 = 完全断电，1 = 全功率。用来驱动壁灯、门缝与仪表亮度。
    double powerLevel() const {
        double p;
        switch (stage) {
        case DIALOGUE: p = 0.0; break;
        case PULSING:  p = 0.16 + 0.17 * pulseCount; break;
        case SCANNING: p = 0.55 + 0.45 * std::min(1.0, beamTimer / 36.0); break;
        default:       p = 1.0; break;
        }
        // 被强唤醒的设施电压不稳：偶尔整体压暗一帧
        bool booting = (stage == PULSING) ||
                       (stage == SCANNING && beamTimer < SCAN_BOOT_END + 40);
        if (booting && hashU((unsigned)(timer / 2 + 13)) % 11 == 0) p *= 0.45;
        return clamp01(p);
    }

    // 门柱上的状态灯：脉冲每来一发点亮两格，扫描时继续补满
    int litCells() const {
        if (stage == DIALOGUE) return 0;
        if (stage == PULSING)  return std::min(10, pulseCount * 2);
        // 扫描开始时整条灯柱从底部重新点亮：设施被强行重启的"通电"瞬间
        if (stage == SCANNING) return std::min(10, 1 + beamTimer / 4);
        return 10;
    }

    double openEase() const { return smooth01(clamp01(doorOpen)); }

    // 解锁段门板的高频震颤（只抖门板，结构保持刚性）
    int shudderX() const {
        if (stage != OPENING || timer >= OPEN_UNLOCK + 10) return 0;
        double f = 1.0 - timer / double(OPEN_UNLOCK + 10);
        return (int)std::lround(std::sin(timer * 2.35) * 2.2 * f);
    }

    // 门缝颜色：断电暗红 → 扫描红 → 验证绿 → 开启青
    void sealColor(int& cr, int& cg, int& cb, int& ca) const {
        double p = powerLevel();
        if (stage == OPEN || stage == OPENING) { cr = 105; cg = 226; cb = 255; ca = 255; }
        else if (stage == SCANNING) {
            if (beamTimer >= SCAN_VERIFY_AT) { cr = 120; cg = 255; cb = 170; ca = 255; }
            else {
                double pulse = 0.72 + 0.28 * std::sin(timer * 0.42);
                cr = (int)(255 * pulse); cg = (int)(62 * p); cb = (int)(54 * p); ca = 255;
            }
        } else {
            double breathe = 0.55 + 0.45 * std::sin(timer * 0.06);
            cr = (int)(120 * p * breathe + 22); cg = (int)(26 * p); cb = (int)(24 * p);
            ca = (int)(110 + 120 * p);
        }
    }

    void lightColor(int& cr, int& cg, int& cb) const {
        if (stage == OPENING || stage == OPEN) { cr = 108; cg = 224; cb = 255; }
        else if (stage == SCANNING) {
            // 验证通过：整条灯柱由警戒红转成确认绿，再随开门转青
            if (beamTimer >= SCAN_VERIFY_AT) { cr = 120; cg = 255; cb = 170; }
            else { cr = 255; cg = 82; cb = 70; }
        }
        else { cr = 255; cg = 178; cb = 64; }
    }

    // 7 段数码管：给门禁编号做一块小仪表
    static void seg7(SDL_Renderer* r, int x, int y, int w, int h, int d,
                     int cr, int cg, int cb, int ca) {
        static const unsigned char mask[10] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
        };
        unsigned m = mask[(d % 10 + 10) % 10];
        int t = 2, hh = h / 2;
        auto s = [&](int sx, int sy, int sw, int sh) { fill(r, sx, sy, sw, sh, cr, cg, cb, ca); };
        if (m & 0x01) s(x + t, y, w - 2 * t, t);                 // a
        if (m & 0x02) s(x + w - t, y + t, t, hh - t);            // b
        if (m & 0x04) s(x + w - t, y + hh, t, hh - t);           // c
        if (m & 0x08) s(x + t, y + h - t, w - 2 * t, t);         // d
        if (m & 0x10) s(x, y + hh, t, hh - t);                   // e
        if (m & 0x20) s(x, y + t, t, hh - t);                    // f
        if (m & 0x40) s(x + t, y + hh - t / 2, w - 2 * t, t);    // g
    }

    // ==================== hull / frame ====================
    void drawHull(SDL_Renderer* r) const {
        // 顶梁 / 底梁
        fill(r, DOOR_L, 0, DOOR_R - DOOR_L, DOOR_T, 15, 18, 25);
        fill(r, DOOR_L, DOOR_B, DOOR_R - DOOR_L, WIN_HEIGHT - DOOR_B, 15, 18, 25);
        // 左右门柱
        fill(r, DOOR_L, DOOR_T, AP_L - DOOR_L, DOOR_B - DOOR_T, 13, 16, 22);
        fill(r, AP_R, DOOR_T, DOOR_R - AP_R, DOOR_B - DOOR_T, 13, 16, 22);

        // 大块分板：用两档灰度把结构分成"厚钢板"而不是一根线
        fill(r, DOOR_L, 0, 18, DOOR_T, 20, 24, 33);
        fill(r, DOOR_L + 18, 0, 34, DOOR_T, 12, 15, 21);
        fill(r, DOOR_L, DOOR_B, 18, WIN_HEIGHT - DOOR_B, 20, 24, 33);
        fill(r, DOOR_L + 18, DOOR_B, 34, WIN_HEIGHT - DOOR_B, 12, 15, 21);
        for (int y = DOOR_T + 60; y < DOOR_B - 20; y += 120) {
            fill(r, DOOR_L, y, AP_L - DOOR_L, 34, 22, 26, 35);
            fill(r, AP_R, y, DOOR_R - AP_R, 34, 22, 26, 35);
        }
        // 柱身横向加强肋
        for (int y = DOOR_T + 20; y < DOOR_B; y += 60) {
            line(r, DOOR_L, y, AP_L - 1, y, 58, 66, 82, 150);
            line(r, DOOR_L, y + 1, AP_L - 1, y + 1, 28, 33, 43, 200);
            line(r, AP_R + 1, y, DOOR_R, y, 58, 66, 82, 150);
        }
    }

    void drawFrame(SDL_Renderer* r) const {
        double p = powerLevel();
        // 内框高光（门洞四周的厚边）
        line(r, AP_L - 3, DOOR_T, AP_L - 3, DOOR_B, 118, 132, 156, 210);
        line(r, AP_L - 2, DOOR_T, AP_L - 2, DOOR_B, 58, 66, 84, 220);
        line(r, AP_R + 2, DOOR_T, AP_R + 2, DOOR_B, 118, 132, 156, 210);
        line(r, AP_R + 3, DOOR_T, AP_R + 3, DOOR_B, 58, 66, 84, 220);
        line(r, DOOR_L, DOOR_T - 2, DOOR_R, DOOR_T - 2, 96, 108, 130, 200);
        line(r, DOOR_L, DOOR_B + 2, DOOR_R, DOOR_B + 2, 96, 108, 130, 200);
        line(r, DOOR_L, 1, DOOR_R, 1, 74, 84, 104, 180);
        line(r, DOOR_L, WIN_HEIGHT - 2, DOOR_R, WIN_HEIGHT - 2, 74, 84, 104, 180);

        // ---- 八边形门洞：四角倒角，让开口不是普通矩形 ----
        {
            const int C = 22;
            for (int i = 0; i < C; ++i) {
                int w = C - i;
                fill(r, AP_L, DOOR_T + i, w, 1, 21, 25, 34, 255);
                fill(r, AP_R - w, DOOR_T + i, w, 1, 21, 25, 34, 255);
                fill(r, AP_L, DOOR_B - 1 - i, w, 1, 21, 25, 34, 255);
                fill(r, AP_R - w, DOOR_B - 1 - i, w, 1, 21, 25, 34, 255);
            }
            // 倒角受光边
            line(r, AP_L + C, DOOR_T + 1, AP_L + 1, DOOR_T + C, 132, 146, 170, 230);
            line(r, AP_R - C, DOOR_T + 1, AP_R - 1, DOOR_T + C, 132, 146, 170, 230);
            line(r, AP_L + C, DOOR_B - 2, AP_L + 1, DOOR_B - C - 1, 132, 146, 170, 230);
            line(r, AP_R - C, DOOR_B - 2, AP_R - 1, DOOR_B - C - 1, 132, 146, 170, 230);
            // 倒角上的固定螺栓
            for (int k = 1; k <= 2; ++k) {
                int ox = C * k / 3;
                fill(r, AP_L + ox, DOOR_T + C - ox, 2, 2, 150, 164, 188, 200);
                fill(r, AP_R - ox - 1, DOOR_T + C - ox, 2, 2, 150, 164, 188, 200);
                fill(r, AP_L + ox, DOOR_B - C + ox - 1, 2, 2, 150, 164, 188, 200);
                fill(r, AP_R - ox - 1, DOOR_B - C + ox - 1, 2, 2, 150, 164, 188, 200);
            }
        }
        // ---- 左柱上下肩部：把门框做成嵌入岩壁的巨型结构 ----
        {
            for (int i = 0; i < 28; ++i) {
                fill(r, DOOR_L - i - 1, DOOR_T + i, i + 1, 1, 18, 21, 29, 255);
                fill(r, DOOR_L - i - 1, DOOR_B - 1 - i, i + 1, 1, 18, 21, 29, 255);
            }
            line(r, DOOR_L - 28, DOOR_T, DOOR_L, DOOR_T + 28, 96, 108, 130, 220);
            line(r, DOOR_L - 28, DOOR_B, DOOR_L, DOOR_B - 28, 96, 108, 130, 220);
            line(r, DOOR_L - 28, DOOR_T + 1, DOOR_L - 1, DOOR_T + 29, 52, 60, 76, 220);
            line(r, DOOR_L - 28, DOOR_B - 1, DOOR_L - 1, DOOR_B - 29, 52, 60, 76, 220);
            // 肩部斜撑上的小灯
            for (int k = 0; k < 3; ++k) {
                int ox = 6 + k * 8;
                fill(r, DOOR_L - ox, DOOR_T + ox - 2, 3, 3, 255, 176, 64, 140);
                fill(r, DOOR_L - ox, DOOR_B - ox - 1, 3, 3, 255, 176, 64, 140);
            }
        }
        // 顶梁/底梁分格 + 铆钉
        for (int x = DOOR_L + 20; x < DOOR_R; x += 26) {
            line(r, x, 2, x, DOOR_T - 2, 34, 40, 52, 210);
            fill(r, x + 3, 15, 2, 2, 104, 116, 138, 180);
            line(r, x, DOOR_B + 2, x, WIN_HEIGHT - 2, 34, 40, 52, 210);
            fill(r, x + 3, WIN_HEIGHT - 17, 2, 2, 104, 116, 138, 180);
        }
        // 左柱：5 组对接夹具，探进太空里增加体量感
        for (int i = 0; i < 5; ++i) {
            int y = 92 + i * 100;
            fill(r, 604, y, 22, 9, 30, 35, 46);
            box(r, 604, y, 22, 9, 92, 103, 122, 210);
            fill(r, 600, y - 3, 6, 15, 44, 51, 66, 255);
            box(r, 600, y - 3, 6, 15, 112, 124, 146, 220);
            fill(r, 602, y, 2, 9, 255, 176, 64, 170);
            line(r, 626, y + 4, 646, y + 4, 84, 95, 114, 200);
        }
        // 门柱状态灯柱（10 格，脉冲逐格点亮）
        int lit = litCells();
        int lr, lg, lb;
        lightColor(lr, lg, lb);
        for (int i = 0; i < 10; ++i) {
            int y = 70 + i * 48;
            bool on = i < lit;
            bool flicker = on && stage != OPEN && stage != OPENING &&
                           hashU((unsigned)(i * 37 + timer / 2)) % 7 == 0;
            if (on && !flicker) {
                fill(r, 646, y - 2, 12, 20, lr, lg, lb, 22);
                fill(r, 648, y, 6, 16, (int)(lr * p), (int)(lg * p), (int)(lb * p), 235);
                fill(r, 649, y + 2, 4, 12, 255, 255, 255, 190);
                fill(r, 778, y - 2, 12, 20, lr, lg, lb, 22);
                fill(r, 780, y, 6, 16, (int)(lr * p), (int)(lg * p), (int)(lb * p), 235);
                fill(r, 781, y + 2, 4, 12, 255, 255, 255, 190);
            } else {
                fill(r, 648, y, 6, 16, 22, 26, 35, 255);
                box(r, 648, y, 6, 16, 44, 51, 65, 190);
                fill(r, 780, y, 6, 16, 22, 26, 35, 255);
                box(r, 780, y, 6, 16, 44, 51, 65, 190);
            }
        }
        // 顶梁仪表：门禁编号 07（扫描时数字乱跳 = 在读码）+ 进度条
        {
            int d0 = 0, d1 = 7;
            if (stage == SCANNING && beamTimer < SCAN_VERIFY_AT) {
                d0 = (int)(hashU((unsigned)(beamTimer / 3 + 5)) % 10);
                d1 = (int)(hashU((unsigned)(beamTimer / 3 + 91)) % 10);
            }
            int ir = (int)(180 + 60 * p), ig = (int)(120 + 50 * p), ib = (int)(40 + 20 * p);
            if (stage == SCANNING && beamTimer >= SCAN_VERIFY_AT) { ir = 110; ig = 245; ib = 165; }
            if (stage == OPENING || stage == OPEN) { ir = 108; ig = 224; ib = 255; }
            seg7(r, 638, 9, 8, 18, d0, ir, ig, ib, (int)(120 + 120 * p));
            seg7(r, 648, 9, 8, 18, d1, ir, ig, ib, (int)(120 + 120 * p));
            fill(r, 634, 30, 24, 2, 46, 53, 68, 220);
            if (doorOpen > 0.0 || stage == OPENING) {
                fill(r, 634, 30, (int)(24 * clamp01(doorOpen)), 2, 108, 224, 255, 255);
            } else if (stage == SCANNING) {
                int w = (int)(24 * clamp01(beamTimer / double(SCAN_TOTAL)));
                if (beamTimer >= SCAN_VERIFY_AT) fill(r, 634, 30, w, 2, 120, 255, 170, 230);
                else fill(r, 634, 30, w, 2, 255, 82, 70, 220);
            }
        }
        // 警示斜纹（顶梁左端 / 底梁左端）
        for (int i = 0; i < 5; ++i) {
            int x = DOOR_L + 6 + i * 7;
            line(r, x, 4, x + 4, 12, 232, 166, 52, 170);
            line(r, x, WIN_HEIGHT - 12, x + 4, WIN_HEIGHT - 4, 232, 166, 52, 170);
        }
        // 门柱线缆槽
        line(r, 630, DOOR_T + 6, 630, DOOR_B - 6, 48, 55, 70, 200);
        line(r, 631, DOOR_T + 6, 631, DOOR_B - 6, 74, 84, 102, 130);
        for (int y = DOOR_T + 30; y < DOOR_B; y += 46) {
            fill(r, 628, y, 5, 3, 66, 74, 90, 220);
        }
        // 断电时唯一还在闪的备用红灯
        if (stage == DIALOGUE) {
            bool blink = (int)(timer / 40) % 2 == 0;
            if (blink) {
                fill(r, 647, 296, 10, 8, 190, 34, 28, 34);
                fill(r, 650, 298, 4, 4, 235, 70, 54, 190);
            }
        }
    }

    // ==================== door panels ====================
    void drawPanel(SDL_Renderer* r, bool top, double off) const {
        const int jx = shudderX();
        const int x0 = AP_L + jx, x1 = AP_R + jx;
        const int w = x1 - x0;
        int y0, y1;
        if (top) { y0 = -320; y1 = DOOR_MID - (int)off; }
        else     { y0 = DOOR_MID + (int)off; y1 = WIN_HEIGHT + 320; }
        if (y1 - y0 < 4) return;

        // 底板 + 左右立边（左暗右亮，做出厚度）
        fill(r, x0, y0, w, y1 - y0, 31, 36, 48);
        fill(r, x0, y0, 9, y1 - y0, 19, 22, 30);
        fill(r, x1 - 9, y0, 9, y1 - y0, 24, 28, 38);
        fill(r, x0 + 9, y0, 3, y1 - y0, 14, 17, 23);
        fill(r, x1 - 12, y0, 3, y1 - y0, 48, 55, 70);

        // ---- 横向装甲带：从门缝往两侧排，接缝处对齐 ----
        const int BAND = 54;
        int k = 0;
        for (int y = (top ? y1 : y0); top ? y > y0 : y < y1;
             y += top ? -BAND : BAND, ++k) {
            int by = top ? y - BAND : y;         // 该带的上沿
            int bh = BAND;
            if (by + bh < y0 || by > y1) continue;
            bool bright = (k & 1) == 0;
            // 主装甲板
            fill(r, x0 + 11, by + 2, w - 22, bh - 4,
                 bright ? 39 : 30, bright ? 45 : 35, bright ? 60 : 46);
            // 上沿受光 / 下沿投影
            line(r, x0 + 11, by, x1 - 11, by, 96, 108, 130, 210);
            line(r, x0 + 11, by + 1, x1 - 11, by + 1, 132, 146, 170, 110);
            line(r, x0 + 11, by + bh - 1, x1 - 11, by + bh - 1, 12, 15, 21, 240);
            line(r, x0 + 11, by + bh - 2, x1 - 11, by + bh - 2, 24, 28, 38, 200);
            // 中央凸起的加强板
            fill(r, x0 + 38, by + 8, w - 76, bh - 16,
                 bright ? 45 : 35, bright ? 52 : 40, bright ? 69 : 53);
            line(r, x0 + 38, by + 8, x1 - 38, by + 8, 118, 132, 156, 150);
            line(r, x0 + 38, by + bh - 9, x1 - 38, by + bh - 9, 16, 19, 26, 200);
            // 两侧通风槽
            for (int v = 0; v < 3; ++v) {
                line(r, x0 + 17 + v * 5, by + 20, x0 + 17 + v * 5, by + 36, 11, 13, 19, 220);
                line(r, x0 + 18 + v * 5, by + 20, x0 + 18 + v * 5, by + 36, 66, 75, 93, 110);
                line(r, x1 - 21 + v * 5, by + 20, x1 - 21 + v * 5, by + 36, 11, 13, 19, 220);
                line(r, x1 - 20 + v * 5, by + 20, x1 - 20 + v * 5, by + 36, 66, 75, 93, 110);
            }
            // 四角铆钉
            fill(r, x0 + 14, by + 6, 2, 2, 138, 152, 176, 210);
            fill(r, x1 - 16, by + 6, 2, 2, 138, 152, 176, 210);
            fill(r, x0 + 14, by + bh - 9, 2, 2, 138, 152, 176, 210);
            fill(r, x1 - 16, by + bh - 9, 2, 2, 138, 152, 176, 210);
            // 每隔三块画一组朝外的重载箭头（暗示门板向两侧退让）
            if (k % 3 == 1) {
                for (int a = 0; a < 2; ++a) {
                    int ax = x0 + 44 + a * 12, ay = by + bh / 2;
                    line(r, ax, ay - 8, ax + 7, ay, 108, 120, 142, 150);
                    line(r, ax + 7, ay, ax, ay + 8, 108, 120, 142, 150);
                    int bx = x1 - 44 - a * 12;
                    line(r, bx, ay - 8, bx - 7, ay, 108, 120, 142, 150);
                    line(r, bx - 7, ay, bx, ay + 8, 108, 120, 142, 150);
                }
            }
        }
        // 纵向导轨槽（门板沿此上下滑）
        for (int x : {x0 + 24, x1 - 27}) {
            line(r, x, y0, x, y1, 64, 73, 92, 140);
            line(r, x + 1, y0, x + 1, y1, 30, 36, 46, 180);
        }
        // 面板内部能量导管
        double p = powerLevel();
        int lr, lg, lb;
        lightColor(lr, lg, lb);
        for (int y = y0 + 20; y < y1; y += BAND) {
            fill(r, x0 + 34, y, 2, 30, lr, lg, lb, (int)(24 + 34 * p));
            fill(r, x1 - 36, y, 2, 30, lr, lg, lb, (int)(24 + 34 * p));
        }

        // ---- 门缝边缘：警示斜纹 + 发光密封条 ----
        const int edgeY = top ? y1 - 15 : y0;
        double openFade = clamp01(1.0 - off / 80.0);
        fill(r, x0 + 2, edgeY, w - 4, 15, 24, 20, 16, (int)(220 * openFade));
        for (int x = x0 - 8; x < x1 + 8; x += 7) {
            bool amber = (((x - x0) / 7) & 1) == 0;
            int xa = x, xb = x + 15;
            line(r, xa, edgeY + 14, xb, edgeY,
                 amber ? 232 : 52, amber ? 162 : 42, amber ? 48 : 30,
                 (int)(205 * openFade));
        }
        int sr, sg, sb, sa;
        sealColor(sr, sg, sb, sa);
        int sy = top ? y1 - 1 : y0;
        // 开门瞬间门板边缘先热后冷
        if (stage == OPENING && doorOpen < 0.10) { sr = 255; sg = 186; sb = 110; }
        line(r, x0 + 2, sy - 3, x1 - 2, sy - 3, sr, sg, sb, (int)(sa * 0.22));
        line(r, x0 + 2, sy - 2, x1 - 2, sy - 2, sr, sg, sb, (int)(sa * 0.45));
        line(r, x0 + 1, sy, x1 - 1, sy, sr, sg, sb, sa);
        line(r, x0 + 2, sy + 2, x1 - 2, sy + 2, sr, sg, sb, (int)(sa * 0.45));
        line(r, x0 + 2, sy + 3, x1 - 2, sy + 3, sr, sg, sb, (int)(sa * 0.22));
        // 门板边缘定位灯
        if (off < 1.0) {
            for (int kk = 0; kk < 3; ++kk) {
                int lx = x0 + 22 + kk * 34;
                bool on = stage != DIALOGUE && (int)(timer / 18 + kk) % 3 != 0;
                fill(r, lx, DOOR_MID + (top ? -2 : -1), 5, 3,
                     on ? 255 : 70, on ? 158 : 60, on ? 66 : 52, on ? 235 : 160);
            }
        }
    }

    // 门缝锁定夹：解锁段先回缩，然后门板才分开
    void drawSeamLocks(SDL_Renderer* r) const {
        if (stage != OPENING || doorOpen > 0.001) return;
        if (timer > OPEN_UNLOCK + 4) return;
        double t = clamp01(timer / double(OPEN_UNLOCK));
        int slide = (int)(t * 13.0);
        int alpha = (int)(255 * clamp01(1.0 - t * 0.75));
        for (int i = 0; i < 3; ++i) {
            int cx = 684 + i * 32;
            // 上下两半各向后退，露出门缝
            fill(r, cx - 11, DOOR_MID - 13 - slide, 22, 13, 52, 59, 75, alpha);
            box(r, cx - 11, DOOR_MID - 13 - slide, 22, 13, 120, 134, 158, alpha);
            fill(r, cx - 5, DOOR_MID - 9 - slide, 3, 4, 175, 190, 214, alpha);
            fill(r, cx + 4, DOOR_MID - 9 - slide, 3, 4, 175, 190, 214, alpha);
            fill(r, cx - 11, DOOR_MID + slide, 22, 13, 52, 59, 75, alpha);
            box(r, cx - 11, DOOR_MID + slide, 22, 13, 120, 134, 158, alpha);
            fill(r, cx - 5, DOOR_MID + 5 + slide, 3, 4, 175, 190, 214, alpha);
            fill(r, cx + 4, DOOR_MID + 5 + slide, 3, 4, 175, 190, 214, alpha);
        }
    }

    // 解锁完成的一瞬间：门缝爆出一线白光 + 火花，随后门板开始滑动
    void drawUnlockFlash(SDL_Renderer* r) const {
        if (stage != OPENING) return;
        if (timer < OPEN_UNLOCK - 2 || timer > OPEN_UNLOCK + 5) return;
        int age = timer - (OPEN_UNLOCK - 2);
        int a = 190 - age * 34;
        if (a < 0) a = 0;
        // 整条门缝被点亮
        for (int i = -3; i <= 3; ++i) {
            line(r, AP_L - 2, DOOR_MID + i, AP_R + 2, DOOR_MID + i,
                 215, 246, 255, i == 0 ? a : a / 3);
        }
        // 沿门缝飞出的火花
        for (int i = 0; i < 16; ++i) {
            int seed = i * 53 + 11;
            int sx = AP_L + 6 + (int)(hash01(seed) * (AP_R - AP_L - 12));
            double ang = (hash01(seed + 1) - 0.5) * 3.0;
            int len = 5 + (int)(hash01(seed + 2) * 14);
            int dx = (int)(std::cos(ang) * len);
            int dy = (int)(std::sin(ang) * len * 0.6);
            line(r, sx, DOOR_MID, sx + dx, DOOR_MID + dy,
                 255, 210 - i * 4, 150, a);
        }
    }

    // 门洞内部：纵深隧道，门一开就能看到设施内部的光
    void drawInterior(SDL_Renderer* r, double off) const {
        if (off < 2.0) return;
        SDL_Rect clip = {AP_L, DOOR_T, AP_R - AP_L, DOOR_B - DOOR_T};
        SDL_RenderSetClipRect(r, &clip);

        fill(r, AP_L, DOOR_T, AP_R - AP_L, DOOR_B - DOOR_T, 5, 7, 11);
        const int cx = (AP_L + AP_R) / 2, cy = DOOR_MID;
        const int hw = (AP_R - AP_L) / 2, hh = (DOOR_B - DOOR_T) / 2;
        const int FAR_HW = 22, FAR_HH = 64;   // 远端出口的半宽/半高

        // 内侧导轨（门板就在这两条轨道上滑动）
        for (int side = 0; side < 2; ++side) {
            int rx = side == 0 ? AP_L + 2 : AP_R - 5;
            fill(r, rx, DOOR_T, 3, DOOR_B - DOOR_T, 32, 38, 50);
            line(r, rx + 1, DOOR_T, rx + 1, DOOR_B, 104, 206, 236, 95);
            for (int y = DOOR_T + 5; y < DOOR_B; y += 16) {
                line(r, rx, y, rx + 3, y, 86, 190, 220, 70);
            }
        }

        // 由近到远 8 层通道框：从门洞矩形插值到远端出口，形成一点透视
        for (int i = 1; i <= 8; ++i) {
            double t = i / 9.0;
            int w = (int)(hw + (FAR_HW - hw) * t);
            int h = (int)(hh + (FAR_HH - hh) * t);
            box(r, cx - w, cy - h, w * 2, h * 2,
                52 + i * 8, 132 + i * 13, 166 + i * 10, 32 + i * 10);
            // 通道顶灯 / 地灯
            line(r, cx - w + 2, cy - h, cx + w - 2, cy - h, 120, 226, 250, 12 + i * 7);
            line(r, cx - w + 2, cy + h, cx + w - 2, cy + h, 120, 226, 250, 12 + i * 7);
            // 每两层补一根墙肋
            if (i % 2 == 1) {
                line(r, cx - w, cy - h + 8, cx - w, cy + h - 8, 92, 196, 226, 34 + i * 6);
                line(r, cx + w, cy - h + 8, cx + w, cy + h - 8, 92, 196, 226, 34 + i * 6);
            }
        }
        // 远端出口：一层层堆到发白
        fill(r, cx - FAR_HW - 8, cy - FAR_HH - 10, (FAR_HW + 8) * 2, (FAR_HH + 10) * 2,
             70, 180, 225, 26);
        fill(r, cx - FAR_HW, cy - FAR_HH, FAR_HW * 2, FAR_HH * 2, 140, 228, 250, 46);
        fill(r, cx - FAR_HW + 8, cy - FAR_HH + 12, (FAR_HW - 8) * 2, (FAR_HH - 12) * 2,
             208, 246, 255, 74);
        fill(r, cx - 8, cy - 40, 16, 80, 244, 253, 255, 130);
        fill(r, cx - 2, cy - 28, 4, 56, 255, 255, 255, 200);
        // 远端出口的横向灯带
        for (int k = 0; k < 5; ++k) {
            int ly = cy - FAR_HH + 16 + k * 24;
            line(r, cx - FAR_HW + 2, ly, cx + FAR_HW - 2, ly, 150, 232, 255, 50);
        }
        // 通道里的悬浮微尘
        for (int i = 0; i < 16; ++i) {
            int dx = cx - 40 + (int)(hash01(i * 31 + 7) * 80);
            int dy = cy - 110 + (int)(hash01(i * 47 + 3) * 220);
            fill(r, dx, dy, 1, 1, 205, 242, 255, 70);
        }
        // 门洞边缘的冷光
        if (doorOpen > 0.02) {
            line(r, AP_L + 1, DOOR_T, AP_L + 1, DOOR_B, 150, 232, 255, 90);
            line(r, AP_R - 2, DOOR_T, AP_R - 2, DOOR_B, 150, 232, 255, 90);
        }
        SDL_RenderSetClipRect(r, NULL);
    }

    // ==================== scanners ====================
    void drawScanners(SDL_Renderer* r) const {
        drawScannerHead(r, true);
        drawScannerHead(r, false);
    }

    void drawScannerHead(SDL_Renderer* r, bool top) const {
        const int cx = SCAN_X;
        const int hy0 = top ? 5 : WIN_HEIGHT - 41;
        const int hy1 = top ? 41 : WIN_HEIGHT - 5;
        const int baseY = top ? hy1 : hy0;      // 与外壳相接的一端
        const int tipY  = top ? baseY + 12 : baseY - 12;  // 伸进门洞的镜头口

        // 外壳
        fill(r, cx - 25, hy0, 50, hy1 - hy0, 38, 44, 58);
        box(r, cx - 25, hy0, 50, hy1 - hy0, 108, 120, 142, 235);
        fill(r, cx - 21, top ? hy0 + 3 : hy1 - 7, 42, 4, 58, 66, 84, 210);
        for (int i = 0; i < 3; ++i) {
            fill(r, cx - 18 + i * 7, hy0 + 11, 4, 15, 24, 28, 38, 230);
        }
        // 镜头喉管（梯形）
        {
            setColorA(r, 108, 120, 142, 235);
            SDL_Point pts[4] = {
                {cx - 15, baseY}, {cx + 15, baseY}, {cx + 9, tipY}, {cx - 9, tipY}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
        }
        fill(r, cx - 4, top ? tipY - 4 : tipY, 8, 4, 40, 44, 54, 255);

        // 镜头灯：待机暗红 / 扫描红 / 验证绿 / 开门青
        int cr, cg, cb;
        bool active = beamActive && stage == SCANNING;
        bool done = (stage == OPENING || stage == OPEN);
        if (active) {
            if (beamTimer >= SCAN_VERIFY_AT) { cr = 120; cg = 255; cb = 170; }
            else { cr = 255; cg = 62; cb = 54; }
        } else if (done) { cr = 110; cg = 226; cb = 255; }
        else { cr = 120; cg = 34; cb = 30; }

        int glow = active ? (int)(45 + 40 * std::fabs(std::sin(timer * 0.30))) : 22;
        int ly = top ? tipY - 2 : tipY + 1;
        fill(r, cx - 12, top ? ly - 6 : ly, 24, 7, cr, cg, cb, glow / 2);
        fill(r, cx - 6, top ? ly - 4 : ly - 1, 12, 5, cr, cg, cb, glow);
        fill(r, cx - 3, top ? ly - 3 : ly - 1, 6, 4,
             active ? 255 : cr, active ? 188 : cg, active ? 170 : cb, 245);

        // 两侧警示灯
        bool warn = (stage == PULSING || stage == SCANNING) && (timer / 14) % 2 == 0;
        int wy = top ? 22 : WIN_HEIGHT - 28;
        for (int k = 0; k < 2; ++k) {
            int bx = cx - 31 + k * 62;
            fill(r, bx, wy, 6, 6, warn ? 255 : 74, warn ? 80 : 38, warn ? 60 : 34,
                 warn ? 235 : 170);
            if (warn) fill(r, bx - 2, wy - 2, 10, 10, 255, 70, 52, 34);
        }
        // 通电时的镜头电火花
        if (beamActive && stage == SCANNING && beamTimer < SCAN_BOOT_END) {
            for (int i = 0; i < 3; ++i) {
                if (hashU((unsigned)(beamTimer / 2 + i * 11)) % 3 != 0) continue;
                int x = cx - 10 + (int)(hash01(beamTimer * 7 + i) * 20);
                line(r, x, tipY, x + (int)(hash01(beamTimer * 5 + i + 3) * 8) - 4,
                     tipY + (top ? 6 : -6), 255, 170, 120, 200);
            }
        }
    }

    void drawEmitterSparks(SDL_Renderer* r, bool top) const {
        int oy = top ? SCAN_TOP_Y : SCAN_BOT_Y;
        bool on = (beamTimer % 8) < 5;
        if (!on) return;
        for (int i = 0; i < 5; ++i) {
            int seed = beamTimer / 2 * 29 + i * 13 + (top ? 0 : 101);
            int x = SCAN_X - 12 + (int)(hash01(seed) * 24);
            int y = oy + (int)(hash01(seed + 1) * 10) - 5;
            int dx = (int)(hash01(seed + 2) * 10) - 5;
            int dy = (int)(hash01(seed + 3) * 10) - 5;
            line(r, x, y, x + dx, y + dy, 255, 150 + (int)(hash01(seed) * 80), 110, 160);
        }
        // 镜头充能红光
        fill(r, SCAN_X - 14, oy - 8, 28, 16, 255, 60, 48, 26);
        fill(r, SCAN_X - 8, oy - 5, 16, 10, 255, 70, 58, 42);
        fill(r, SCAN_X - 3, oy - 3, 6, 6, 255, 150, 130, 220);
    }

    // ==================== scan beam helpers ====================
    void drawCone(SDL_Renderer* r, double ax, double ay, double tx, double ty,
                  double halfW, double ext, double alpha, int cr, int cg, int cb) const {
        if (ext <= 0.01 || halfW < 1.0) return;
        double dx = tx - ax, dy = ty - ay;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 2.0) return;
        double nx = -dy / len, ny = dx / len;
        // 锥体：由窄到宽的横向弦线堆出体积感（低透明度叠加成辉光）
        const int N = 44;
        for (int i = 1; i <= N; ++i) {
            double t = i / double(N);
            double w = halfW * (0.08 + 0.92 * t);
            double hx = ax + dx * t * ext;
            double hy = ay + dy * t * ext;
            int a = (int)(alpha * (14.0 + 10.0 * t));
            setColorA(r, cr, cg, cb, a);
            SDL_RenderDrawLine(r, (int)(hx + nx * w), (int)(hy + ny * w),
                                  (int)(hx - nx * w), (int)(hy - ny * w));
        }
        // 内芯：更窄更亮的一条光束
        for (int i = 1; i <= 22; ++i) {
            double t = i / 22.0;
            double w = halfW * 0.24 * (0.10 + 0.90 * t);
            double hx = ax + dx * t * ext;
            double hy = ay + dy * t * ext;
            setColorA(r, cr, cg, cb, (int)(alpha * 30));
            SDL_RenderDrawLine(r, (int)(hx + nx * w), (int)(hy + ny * w),
                                  (int)(hx - nx * w), (int)(hy - ny * w));
        }
        // 边缘光线
        double bx = ax + dx * ext, by = ay + dy * ext;
        setColorA(r, cr, cg, cb, (int)(alpha * 135));
        SDL_RenderDrawLine(r, (int)ax, (int)ay,
                           (int)(bx + nx * halfW), (int)(by + ny * halfW));
        SDL_RenderDrawLine(r, (int)ax, (int)ay,
                           (int)(bx - nx * halfW), (int)(by - ny * halfW));
        // 中心虚线
        for (int k = 0; k < 8; ++k) {
            double t0 = k / 8.0, t1 = t0 + 0.06;
            setColorA(r, 255, 190, 180, (int)(alpha * 46));
            SDL_RenderDrawLine(r, (int)(ax + dx * t0 * ext), (int)(ay + dy * t0 * ext),
                                  (int)(ax + dx * t1 * ext), (int)(ay + dy * t1 * ext));
        }
        // 发射口光斑
        fill(r, (int)ax - 7, (int)ay - 3, 14, 6, cr, cg, cb, (int)(alpha * 60));
        fill(r, (int)ax - 3, (int)ay - 2, 6, 4, 255, 210, 190, (int)(alpha * 120));
    }

    void drawReticle(SDL_Renderer* r, double tx, double ty, double lockT,
                     bool verified, int bt) const {
        int cr = verified ? 120 : 255;
        int cg = verified ? 255 : 72;
        int cb = verified ? 170 : 62;
        double size = 46.0 - 18.0 * smooth01(lockT);
        int s = (int)size;
        int L = 9;
        int a = verified ? 210 : 180;
        // 四角括号
        line(r, (int)tx - s, (int)ty - s, (int)tx - s + L, (int)ty - s, cr, cg, cb, a);
        line(r, (int)tx - s, (int)ty - s, (int)tx - s, (int)ty - s + L, cr, cg, cb, a);
        line(r, (int)tx + s, (int)ty - s, (int)tx + s - L, (int)ty - s, cr, cg, cb, a);
        line(r, (int)tx + s, (int)ty - s, (int)tx + s, (int)ty - s + L, cr, cg, cb, a);
        line(r, (int)tx - s, (int)ty + s, (int)tx - s + L, (int)ty + s, cr, cg, cb, a);
        line(r, (int)tx - s, (int)ty + s, (int)tx - s, (int)ty + s - L, cr, cg, cb, a);
        line(r, (int)tx + s, (int)ty + s, (int)tx + s - L, (int)ty + s, cr, cg, cb, a);
        line(r, (int)tx + s, (int)ty + s, (int)tx + s, (int)ty + s - L, cr, cg, cb, a);
        // 十字准星 + 锁定环
        if (lockT > 0.15) {
            line(r, (int)tx - 7, (int)ty, (int)tx + 7, (int)ty, cr, cg, cb, a);
            line(r, (int)tx, (int)ty - 7, (int)tx, (int)ty + 7, cr, cg, cb, a);
        }
        if (lockT > 0.99) {
            double pulse = 0.5 + 0.5 * std::fabs(std::sin(bt * 0.25));
            arc(r, tx, ty, s + 9, 0, 2.0 * M_PI, cr, cg, cb, (int)(90 + 70 * pulse));
            // 环上的四根刻度
            for (int k = 0; k < 4; ++k) {
                double ang = M_PI / 2 * k + bt * 0.01;
                double ux = std::cos(ang), uy = std::sin(ang);
                line(r, (int)(tx + ux * (s + 5)), (int)(ty + uy * (s + 5)),
                     (int)(tx + ux * (s + 13)), (int)(ty + uy * (s + 13)),
                     cr, cg, cb, 170);
            }
        } else {
            // 锁定进度弧（从 12 点方向顺时针）
            double a0 = -M_PI / 2;
            arc(r, tx, ty, s + 9, a0, a0 + 2.0 * M_PI * smooth01(lockT),
                cr, cg, cb, 190);
        }
    }

    void drawScanVignette(SDL_Renderer* r, double intensity) const {
        double v = clamp01(intensity);
        if (v <= 0.02) return;
        int base = (int)(v * 30);
        for (int i = 0; i < 12; ++i) {
            int inset = i * 3;
            int a = base * (12 - i) / 12;
            if (a < 2) continue;
            setColorA(r, 255, 34, 30, a);
            SDL_Rect rr = {inset, inset, WIN_WIDTH - 2 * inset, WIN_HEIGHT - 2 * inset};
            SDL_RenderDrawRect(r, &rr);
        }
    }
};
