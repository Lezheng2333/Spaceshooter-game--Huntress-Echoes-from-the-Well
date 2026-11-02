#pragma once

#include "../constants.h"
#include "../renderer.h"
#include "../audio.h"

// ============== Ch2GateScene (Chapter 2 opening: vacuum door + pulse unlock sequence) ==============
// Scripted scene: the trainer arrives at Moonwell's vacuum gate.
//   DIALOGUE  - characters talk (driven by Game via DialogueSystem)
//   PULSING   - Ally fires 5 pulse groups (2s apart), 5 concentric rings each, draining energy bar
//   SCANNING  - indicator lights on, red fan-shaped scan beam locks onto the plane
//   OPENING   - the massive door splits open
//   OPEN      - player flies right through the doorway
class Ch2GateScene {
public:
    enum Stage { DIALOGUE, PULSING, SCANNING, OPENING, OPEN };

    static const int DOOR_L = 650;   // hull band left edge
    static const int DOOR_R = 790;   // hull band right edge
    static const int DOOR_T = 40;    // door panel top
    static const int DOOR_B = 560;   // door panel bottom
    static const int DOOR_MID = 300; // split line between the two panels
    static const int SCAN_X = 720;   // scanner x position

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
            if (beamTimer >= 120) { beamActive = false; stage = OPENING; timer = 0; }
            break;
        }

        case OPENING: {
            doorOpen += 1.0 / 90.0;
            if (doorOpen >= 1.0) { doorOpen = 1.0; stage = OPEN; timer = 0; }
            break;
        }

        case OPEN: break;
        }
        return justEmitted ? 1 : 0;
    }

    // ======== DRAW: door / hull / scanners (drawn in the background layer) ========
    void drawDoor(SDL_Renderer* r) const {
        // ---- Facility hull band (right side wall) ----
        SDL_SetRenderDrawColor(r, 45, 48, 58, 160);
        SDL_RenderDrawLine(r, DOOR_L, 0, DOOR_L, WIN_HEIGHT);
        SDL_RenderDrawLine(r, DOOR_R, 0, DOOR_R, WIN_HEIGHT);
        // Hatch lines across the hull
        SDL_SetRenderDrawColor(r, 60, 63, 74, 90);
        for (int y = 10; y < WIN_HEIGHT; y += 13) {
            SDL_RenderDrawLine(r, DOOR_L + 4, y, DOOR_R - 4, y);
        }
        SDL_SetRenderDrawColor(r, 52, 55, 66, 140);
        SDL_RenderDrawLine(r, DOOR_L + 2, 0, DOOR_L + 2, WIN_HEIGHT);
        SDL_RenderDrawLine(r, DOOR_R - 2, 0, DOOR_R - 2, WIN_HEIGHT);

        // ---- Indicator lights along the left frame ----
        for (int i = 0; i < 6; ++i) {
            int ly = 70 + i * 88;
            bool on = lightsOn;
            if (on) {
                // Glow
                SDL_SetRenderDrawColor(r, 255, 190, 70, 60);
                SDL_RenderDrawLine(r, DOOR_L + 8, ly - 3, DOOR_L + 8, ly + 3);
                SDL_SetRenderDrawColor(r, 255, 190, 70, 255);
            } else {
                SDL_SetRenderDrawColor(r, 70, 30, 30, 200);
            }
            for (int dy = -2; dy <= 2; ++dy) {
                SDL_RenderDrawPoint(r, DOOR_L + 8, ly + dy);
                SDL_RenderDrawPoint(r, DOOR_L + 10, ly + dy);
            }
        }

        // ---- Door panels (split apart while opening) ----
        const int PL = DOOR_L + 2, PR = DOOR_R - 2;
        const double off = doorOpen * 265.0;

        // Top panel (slides up)
        {
            int ty = DOOR_T - (int)off;        // panel top y (moves up)
            int by = DOOR_MID - (int)off;      // panel bottom y (moves up)
            SDL_SetRenderDrawColor(r, 90, 95, 112, 230);
            SDL_RenderDrawLine(r, PL, ty, PR, ty);
            SDL_RenderDrawLine(r, PL, by, PR, by);
            SDL_RenderDrawLine(r, PL, ty, PL, by);
            SDL_RenderDrawLine(r, PR, ty, PR, by);
            // Panel bracing (diagonal cross)
            SDL_SetRenderDrawColor(r, 70, 74, 88, 170);
            SDL_RenderDrawLine(r, PL, ty, PR, by);
            SDL_RenderDrawLine(r, PR, ty, PL, by);
            // Inner inset
            SDL_SetRenderDrawColor(r, 78, 82, 97, 160);
            SDL_RenderDrawLine(r, PL + 8, ty + 8, PR - 8, ty + 8);
            SDL_RenderDrawLine(r, PL + 8, by - 8, PR - 8, by - 8);
            SDL_RenderDrawLine(r, PL + 8, ty + 8, PL + 8, by - 8);
            SDL_RenderDrawLine(r, PR - 8, ty + 8, PR - 8, by - 8);
        }

        // Bottom panel (slides down)
        {
            int ty = DOOR_MID + (int)off;
            int by = DOOR_B + (int)off;
            SDL_SetRenderDrawColor(r, 90, 95, 112, 230);
            SDL_RenderDrawLine(r, PL, ty, PR, ty);
            SDL_RenderDrawLine(r, PL, by, PR, by);
            SDL_RenderDrawLine(r, PL, ty, PL, by);
            SDL_RenderDrawLine(r, PR, ty, PR, by);
            SDL_SetRenderDrawColor(r, 70, 74, 88, 170);
            SDL_RenderDrawLine(r, PL, ty, PR, by);
            SDL_RenderDrawLine(r, PR, ty, PL, by);
            SDL_SetRenderDrawColor(r, 78, 82, 97, 160);
            SDL_RenderDrawLine(r, PL + 8, ty + 8, PR - 8, ty + 8);
            SDL_RenderDrawLine(r, PL + 8, by - 8, PR - 8, by - 8);
            SDL_RenderDrawLine(r, PL + 8, ty + 8, PL + 8, by - 8);
            SDL_RenderDrawLine(r, PR - 8, ty + 8, PR - 8, by - 8);
        }

        // ---- Doorway (visible between the panels while opening) ----
        if (doorOpen > 0.01) {
            int topY = DOOR_T + (int)off;
            int botY = DOOR_B - (int)off;
            if (botY - topY > 4) {
                // Bright doorway edges
                SDL_SetRenderDrawColor(r, 220, 240, 255, 200);
                SDL_RenderDrawLine(r, PL, topY, PL, botY);
                SDL_RenderDrawLine(r, PR, topY, PR, botY);
                // Light rays pouring out
                for (int i = 1; i < 6; ++i) {
                    int ly = topY + (botY - topY) * i / 6;
                    int al = 50 + (i % 3) * 25;
                    SDL_SetRenderDrawColor(r, 200, 230, 255, (Uint8)al);
                    SDL_RenderDrawLine(r, PL + 8, ly, PR - 8, ly);
                }
            }
        }

        // ---- Scanner heads (top & bottom of the door) ----
        // Top scanner: box + lens at bottom
        SDL_SetRenderDrawColor(r, 110, 118, 136, 240);
        SDL_RenderDrawLine(r, SCAN_X - 16, 6, SCAN_X + 16, 6);
        SDL_RenderDrawLine(r, SCAN_X - 16, 6, SCAN_X - 16, 34);
        SDL_RenderDrawLine(r, SCAN_X + 16, 6, SCAN_X + 16, 34);
        // Lens (triangle pointing down)
        int lensR, lensG, lensB;
        if (beamActive) { lensR = 255; lensG = 70; lensB = 70; }
        else { lensR = 130; lensG = 138; lensB = 156; }
        SDL_SetRenderDrawColor(r, (Uint8)lensR, (Uint8)lensG, (Uint8)lensB, 255);
        SDL_Point lens[3] = {{SCAN_X - 8, 34}, {SCAN_X + 8, 34}, {SCAN_X, 46}};
        SDL_RenderDrawLines(r, lens, 3);
        SDL_RenderDrawLine(r, lens[2].x, lens[2].y, lens[0].x, lens[0].y);
        if (beamActive) {
            SDL_SetRenderDrawColor(r, 255, 100, 100, 120);
            for (int g = 1; g <= 3; ++g)
                SDL_SetRenderDrawColor(r, 255, 70, 70, (Uint8)(50 / g));
        }

        // Bottom scanner (mirrored)
        SDL_SetRenderDrawColor(r, 110, 118, 136, 240);
        SDL_RenderDrawLine(r, SCAN_X - 16, WIN_HEIGHT - 6, SCAN_X + 16, WIN_HEIGHT - 6);
        SDL_RenderDrawLine(r, SCAN_X - 16, WIN_HEIGHT - 34, SCAN_X - 16, WIN_HEIGHT - 6);
        SDL_RenderDrawLine(r, SCAN_X + 16, WIN_HEIGHT - 34, SCAN_X + 16, WIN_HEIGHT - 6);
        SDL_SetRenderDrawColor(r, (Uint8)lensR, (Uint8)lensG, (Uint8)lensB, 255);
        SDL_Point lensBtm[3] = {{SCAN_X - 8, WIN_HEIGHT - 34}, {SCAN_X + 8, WIN_HEIGHT - 34}, {SCAN_X, WIN_HEIGHT - 46}};
        SDL_RenderDrawLines(r, lensBtm, 3);
        SDL_RenderDrawLine(r, lensBtm[2].x, lensBtm[2].y, lensBtm[0].x, lensBtm[0].y);
    }

    // ======== DRAW: red fan-shaped scan beam (over the plane) ========
    void drawScanBeam(SDL_Renderer* r, double px, double py) const {
        if (!beamActive) return;

        // Sweep: 0..1 over first 40 frames, then locked (with slight pulse)
        double t;
        if (beamTimer < 40) {
            double raw = beamTimer / 40.0;
            t = 1.0 - std::pow(1.0 - raw, 3.0);   // cubic ease-out
        } else {
            t = 1.0;
        }
        if (beamTimer >= 100) {
            // Retract
            double raw = (beamTimer - 100) / 20.0;
            t = 1.0 - raw; if (t < 0) t = 0;
        }
        if (t <= 0.0) return;

        double apexX = SCAN_X, apexY = 46.0;
        // Target: a narrow line segment centered on the plane
        double baseHalf = 26.0 * t;
        double tx = px, ty = py;
        double segTopX = tx, segTopY = ty - baseHalf;
        double segBotX = tx, segBotY = ty + baseHalf;

        int alpha = 140;
        if (beamTimer >= 40 && beamTimer < 100) {
            // Locked: pulsing glow
            alpha = 110 + (int)(50 * std::fabs(std::sin(beamTimer * 0.2)));
        }
        SDL_SetRenderDrawColor(r, 255, 70, 70, (Uint8)alpha);

        // Fan outline: apex → segment ends
        SDL_RenderDrawLine(r, (int)apexX, (int)apexY, (int)segTopX, (int)segTopY);
        SDL_RenderDrawLine(r, (int)apexX, (int)apexY, (int)segBotX, (int)segBotY);
        SDL_RenderDrawLine(r, (int)segTopX, (int)segTopY, (int)segBotX, (int)segBotY);

        // Inner spread lines
        for (int i = 1; i <= 3; ++i) {
            double f = i / 4.0;
            double mx = (int)(segTopX + (segBotX - segTopX) * f);
            double my = (int)(segTopY + (segBotY - segTopY) * f);
            SDL_SetRenderDrawColor(r, 255, 90, 90, (Uint8)(alpha * 0.6));
            SDL_RenderDrawLine(r, (int)apexX, (int)apexY, (int)mx, (int)my);
        }
        // Locked marker on the plane
        if (beamTimer >= 40) {
            SDL_SetRenderDrawColor(r, 255, 120, 120, 220);
            SDL_RenderDrawLine(r, (int)tx - 10, (int)ty, (int)tx + 10, (int)ty);
            SDL_RenderDrawLine(r, (int)tx, (int)ty - 10, (int)tx, (int)ty + 10);
        }
    }

    // ======== DRAW: white concentric pulse rings (over the plane) ========
    void drawRings(SDL_Renderer* r, double px, double py) const {
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
    }
};
