#pragma once

#include "constants.h"
#include "renderer.h"
#include "aim_assist.h"

// ============== Player (shared player state) ==============
class Player {
protected:
    int x, y;
    double rollAngle, rollTarget;
    int lastMoveDir;
    int invFrames;  // invincibility after taking damage
    static constexpr double ROLL_SPEED = 0.314;
    static const int HALF_WIDTH = 15;

public:
    AimAssist aimAssist;  // every player has aim assist

    Player() : x(CENTER_X), y(WIN_HEIGHT - 80), rollAngle(0), rollTarget(0), lastMoveDir(0), invFrames(0) {}

    int getX() const { return x; }
    int getY() const { return y; }
    double getRollAngle() const { return rollAngle; }
    int getInvFrames() const { return invFrames; }
    void setX(int nx) { x = nx; }
    void setY(int ny) { y = ny; }
    void setRollTarget(double rt) { rollTarget = rt; }
    void setRollAngle(double ra) { rollAngle = ra; }
    void setInvFrames(int f) { invFrames = f; }
    // 存档：基础状态（子类若新增状态，覆写 visit 并先调用 Player::visit）
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y);
        ar.ioNum(rollAngle); ar.ioNum(rollTarget);
        ar.ioNum(lastMoveDir); ar.ioNum(invFrames);
        ar.ioObj(aimAssist);
    }
    void updateInvFrames() { if (invFrames > 0) invFrames--; }
    void resetState() {
        rollAngle = 0; rollTarget = 0; lastMoveDir = 0;
        invFrames = 0;
    }
    virtual void draw(SDL_Renderer*) const {}
    virtual void handleInput(const Uint8*) {}
    virtual int getGunCount() const { return 1; }
    virtual void getGunOffset(int, int& ox, int& oy) const { ox = 14; oy = 0; }
    virtual int getNoseOffset() const { return 14; }
};

// ============== TrainingPlane (Chapter 1 perspective movement) ==============
class TrainingPlane : public Player {
public:
    TrainingPlane() : Player() {}

    void draw(SDL_Renderer* r) const {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        int px = getX(), py = getY();
        double tPlane = (px - perspLeft(py)) / perspWidth(py);
        if (tPlane < 0.0) tPlane = 0.0; if (tPlane > 1.0) tPlane = 1.0;
        double hx = perspLeft(HORIZON_Y) + tPlane * perspWidth(HORIZON_Y);
        double hdx = hx - px, hdy = HORIZON_Y - py;
        if (hdy > 0) { hdx = 0; hdy = -1; }
        double hlen = std::sqrt(hdx*hdx + hdy*hdy);
        if (hlen < 0.01) { hdx = 0; hdy = -1; hlen = 1; }
        double hnx = hdx / hlen, hny = hdy / hlen;
        double c = std::cos(getRollAngle()), s = std::sin(getRollAngle());

        auto hr = [&](int lx, int ly) -> SDL_Point {
            return {px + (int)(lx * (-hny) - ly * hnx),
                    py + (int)(lx * hnx  + ly * (-hny))};
        };
        auto hrr = [&](int lx, int ly) -> SDL_Point {
            double rx = lx * c, ry = ly + lx * s * 0.40;
            return {px + (int)(rx * (-hny) - ry * hnx),
                    py + (int)(rx * hnx  + ry * (-hny))};
        };

        SDL_Point nose = hr(0, -12), leftB = hr(-10, 6), rightB = hr(10, 6);
        SDL_Point body[3] = {nose, leftB, rightB};
        SDL_RenderDrawLines(r, body, 3);
        SDL_RenderDrawLine(r, leftB.x, leftB.y, rightB.x, rightB.y);
        SDL_Point lw1 = hrr(-6, 0), lw2 = hrr(-14, 4);
        SDL_RenderDrawLine(r, lw1.x, lw1.y, lw2.x, lw2.y);
        SDL_Point rw1 = hrr(6, 0),  rw2 = hrr(14, 4);
        SDL_RenderDrawLine(r, rw1.x, rw1.y, rw2.x, rw2.y);
        SDL_Point tail1 = hr(0, 6), tail2 = hr(0, 12);
        SDL_RenderDrawLine(r, tail1.x, tail1.y, tail2.x, tail2.y);
    }

    void reset() {
        x = CENTER_X; y = WIN_HEIGHT - 80;
        resetState();
    }

    void handleInput(const Uint8* keys) {
        bool moveLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        bool moveRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        bool moveUp    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool moveDown  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

        // Perspective speed scaling: slower near horizon, faster near bottom
        const double REF_Y = 400.0;
        double refWidth = perspWidth(REF_Y);
        double speedFactor = perspWidth(y) / (refWidth > 0 ? refWidth : 1.0);
        int hStep = (int)(7.0 * speedFactor + 0.5);
        int vStep = (int)(6.0 * speedFactor + 0.5);

        if (moveLeft)  x -= hStep;
        if (moveRight) x += hStep;

        double minX = perspLeft(y) + 18;
        double maxX = perspRight(y) - 18;
        if (x < minX) x = (int)minX;
        if (x > maxX) x = (int)maxX;

        if (moveUp)    y -= vStep;
        if (moveDown)  y += vStep;
        if (y < 220) y = 220;
        if (y > WIN_HEIGHT - 30) y = WIN_HEIGHT - 30;

        int curDir = 0;
        if (moveRight && !moveLeft) curDir = 1;
        else if (moveLeft && !moveRight) curDir = -1;
        if (curDir != 0 && lastMoveDir != curDir)
            rollTarget += (curDir > 0 ? 2.0 * M_PI : -2.0 * M_PI);
        lastMoveDir = curDir;

        double diff = rollTarget - rollAngle;
        if (std::fabs(diff) > 0.005) {
            if (std::fabs(diff) < ROLL_SPEED) rollAngle = rollTarget;
            else rollAngle += (diff > 0 ? ROLL_SPEED : -ROLL_SPEED);
        }
    }

    double getT() const {
        double t = (x - perspLeft(y)) / perspWidth(y);
        if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
        return t;
    }
};

// ============== Ch2Trainer (Chapter 2 side-scrolling trainer) ==============
// Same aircraft as Ch1 TrainingPlane, rotated 90 deg for side-scrolling view.
// Ch1 raw coords (nose-up top-down): nose(0,-12) leftB(-10,6) rightB(10,6)
//   wings lw1(-6,0)→lw2(-14,4)  rw1(6,0)→rw2(14,4)  tail(0,6)→(0,12)
// Ch2 rotated (x,y)→(-y,x), same scale as Ch1:
//   nose(12,0)  topB(-6,-10)  bottomB(-6,10)
//   uw(0,-6)→uw2(-4,-14)  lw(0,6)→lw2(-4,14)  tail(-6,0)→(-12,0)
class Ch2Trainer : public Player {
public:
    Ch2Trainer() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        // Triangular body (same as Ch1 trainer, rotated -90 deg, 1:1 scale)
        SDL_Point nose    = {px + 12, py};
        SDL_Point topB    = {px - 6, py - 10};
        SDL_Point bottomB = {px - 6, py + 10};
        SDL_Point body[3] = {nose, topB, bottomB};
        SDL_RenderDrawLines(r, body, 3);
        SDL_RenderDrawLine(r, topB.x, topB.y, bottomB.x, bottomB.y);

        // Upper wing (Ch1 left wing, rotated)
        SDL_Point uw1 = {px, py - 6};
        SDL_Point uw2 = {px - 4, py - 14};
        SDL_RenderDrawLine(r, uw1.x, uw1.y, uw2.x, uw2.y);

        // Lower wing (Ch1 right wing, rotated)
        SDL_Point lw1 = {px, py + 6};
        SDL_Point lw2 = {px - 4, py + 14};
        SDL_RenderDrawLine(r, lw1.x, lw1.y, lw2.x, lw2.y);

        // Tail (Ch1 center tail, rotated)
        SDL_Point tail1 = {px - 6, py};
        SDL_Point tail2 = {px - 12, py};
        SDL_RenderDrawLine(r, tail1.x, tail1.y, tail2.x, tail2.y);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int, int& ox, int& oy) const override {
        ox = 12; oy = 0;
    }
    int getNoseOffset() const override { return 12; }
};

// ============== NightElf (plane 2) [门禁序列后激活] ==============
class NightElf : public Player {
    bool tripleFire;   // linked to NightElfEnergy triple mode (3 guns while active)

public:
    NightElf() : Player(), tripleFire(false) {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
        tripleFire = false;
    }

    void setTripleFire(bool t) { tripleFire = t; }
    bool isTripleFire() const { return tripleFire; }

    // 存档：Player 基础状态 + 三连发标记（与白色能量条联动，必须存）
    template <class Ar> void visit(Ar& ar) {
        Player::visit(ar);
        ar.ioBool(tripleFire);
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        int wingX = px + 2, wingSpan = 8;
        int noseX = wingX + 30;          // 30° nose: tan(15°)=8/30≈0.27
        int tailX = wingX + 5;           // 120° tail, reflected across wing line

        // Body outline: nose → upper wing → tail → lower wing → nose
        SDL_Point body[5] = {
            {noseX, py},
            {wingX, py - wingSpan},
            {tailX, py},
            {wingX, py + wingSpan},
            {noseX, py}
        };
        SDL_RenderDrawLines(r, body, 5);

        // Parallel lines from wing tips forward, slightly shorter than nose
        SDL_RenderDrawLine(r, wingX, py - wingSpan, noseX - 3, py - wingSpan);
        SDL_RenderDrawLine(r, wingX, py + wingSpan, noseX - 3, py + wingSpan);

        // Fill upper half with correct lower boundary
        for (int sx = wingX; sx <= noseX; sx += 2) {
            double tn = (double)(sx - wingX) / (noseX - wingX);
            int uy = (py - wingSpan) + (int)(wingSpan * tn);
            int ly;
            if (sx <= tailX) {
                double tt = (double)(sx - wingX) / (tailX - wingX);
                ly = (py - wingSpan) + (int)(wingSpan * tt);
            } else {
                ly = py;
            }
            SDL_RenderDrawLine(r, sx, uy, sx, ly);
        }
    }

    int getGunCount() const override { return tripleFire ? 3 : 1; }
    void getGunOffset(int idx, int& ox, int& oy) const override {
        if (idx == 0)      { ox = 32; oy = 0; }    // nose tip
        else if (idx == 1) { ox = 29; oy = -8; }   // upper wing tip
        else               { ox = 29; oy = 8; }    // lower wing tip
    }
    int getNoseOffset() const override { return 32; }
};

// ============== Druid (plane 3) [DORMANT — Chapter 3 激活] ==============
class Druid : public Player {
public:
    Druid() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        int noseLen = 14, tailLen = 10, wingSpan = 8;
        int noseX = px + noseLen, tailX = px - tailLen;

        // Swallowtail body: nose(60°) → wings → tail tips → 90° notch → back
        SDL_Point body[7] = {
            {noseX, py},                       // 0: nose tip
            {px, py - wingSpan},               // 1: upper wing
            {tailX, py - wingSpan/2},          // 2: upper tail tip
            {tailX + 4, py},                   // 3: notch center (90° V)
            {tailX, py + wingSpan/2},          // 4: lower tail tip
            {px, py + wingSpan},               // 5: lower wing
            {noseX, py}                        // 6: back to nose
        };
        SDL_RenderDrawLines(r, body, 7);

        // Parallel lines from wing tips toward nose (shorter than nose)
        SDL_RenderDrawLine(r, px, py - wingSpan, px + 11, py - wingSpan);
        SDL_RenderDrawLine(r, px, py + wingSpan, px + 11, py + wingSpan);

        // Center guide line (navigation arrow feel)
        SDL_RenderDrawLine(r, px, py, noseX, py);
    }
};
