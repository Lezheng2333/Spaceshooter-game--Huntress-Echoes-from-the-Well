#pragma once

#include "constants.h"
#include "renderer.h"
#include "aim_assist.h"
#include "art_fx.h"

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
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        ArtFX::BlendGuard guard(r);
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

        // 主机体：深色蒙皮 + 双层白色描边。填充被限制在原机体轮廓内，
        // 碰撞/瞄准仍使用既有的玩家坐标与 NoseOffset，不改变玩法判定。
        SDL_Point hull[4] = {hr(0, -13), hr(-9, 5), hr(0, 11), hr(9, 5)};
        ArtFX::fillPoly(r, hull, 4, 7, 11, 14, 236);
        for (int i = 0; i < 4; ++i) {
            SDL_Point& a = hull[i]; SDL_Point& b = hull[(i + 1) % 4];
            ArtFX::glowLine(r, a.x, a.y, b.x, b.y, 225, 241, 235, 175, 1);
        }
        // 机翼：稍带 roll 的翼展，填充让浅色背景里也有清晰剪影。
        SDL_Point lw[4] = {hrr(-4, 1), hrr(-16, 7), hrr(-11, 10), hrr(-3, 6)};
        SDL_Point rw[4] = {hrr(4, 1), hrr(16, 7), hrr(11, 10), hrr(3, 6)};
        ArtFX::fillPoly(r, lw, 4, 9, 14, 17, 220);
        ArtFX::fillPoly(r, rw, 4, 9, 14, 17, 220);
        for (int i = 0; i < 4; ++i) {
            SDL_Point& a = lw[i]; SDL_Point& b = lw[(i + 1) % 4];
            ArtFX::glowLine(r, a.x, a.y, b.x, b.y, 210, 229, 224, 150, 1);
            a = rw[i]; b = rw[(i + 1) % 4];
            ArtFX::glowLine(r, a.x, a.y, b.x, b.y, 210, 229, 224, 150, 1);
        }
        // 座舱：冷青色玻璃，既呼应第二章 NightElf，也成为玩家最亮的部件。
        SDL_Point ck[4] = {hr(0, -7), hr(-3, 2), hr(0, 6), hr(3, 2)};
        ArtFX::fillPoly(r, ck, 4, 70, 151, 170, 155);
        SDL_Point ckTop = hr(0, -5), ckTail = hr(0, 3);
        ArtFX::glowLine(r, ckTop.x, ckTop.y, ckTail.x, ckTail.y, 196, 248, 255, 210, 1);

        // 双引擎尾焰：只影响几个像素，作为读档/截图时的呼吸动画。
        int tick = getX() / 11 + getY() / 7 + (int)(getRollAngle() * 10.0);
        int flick = 2 + (tick + px) % 4;
        SDL_Point e1 = hrr(-3, 8), e2 = hrr(-3, 14 + flick);
        SDL_Point e3 = hrr(3, 8), e4 = hrr(3, 14 + flick);
        ArtFX::glowLine(r, e1.x, e1.y, e2.x, e2.y, 94, 214, 255, 170, 2);
        ArtFX::glowLine(r, e3.x, e3.y, e4.x, e4.y, 94, 214, 255, 170, 2);
        SDL_SetRenderDrawColor(r, 235, 252, 255, 235);
        SDL_RenderDrawPoint(r, e1.x, e1.y + 1);
        SDL_RenderDrawPoint(r, e3.x, e3.y + 1);

        // 舷灯：左红右绿；与参考版本一样的航空器识别语言。
        SDL_Point nvL = hrr(-15, 6), nvR = hrr(15, 6);
        SDL_SetRenderDrawColor(r, 255, 92, 84, 220);
        SDL_RenderDrawPoint(r, nvL.x, nvL.y);
        SDL_SetRenderDrawColor(r, 92, 255, 152, 220);
        SDL_RenderDrawPoint(r, nvR.x, nvR.y);
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
        ArtFX::BlendGuard guard(r);
        int px = getX(), py = getY();

        // 深色填充 + 浅色外轮廓，与第一章玩家机同属一个视觉家族。
        SDL_Point hull[4] = {{px + 12, py}, {px - 6, py - 10},
                             {px - 12, py}, {px - 6, py + 10}};
        ArtFX::fillPoly(r, hull, 4, 7, 11, 14, 236);
        for (int i = 0; i < 4; ++i) {
            SDL_Point& a = hull[i]; SDL_Point& b = hull[(i + 1) % 4];
            ArtFX::glowLine(r, a.x, a.y, b.x, b.y, 220, 238, 233, 165, 1);
        }

        // 上/下翼，保留原 1:1 侧视比例。
        SDL_Point uw[4] = {{px + 1, py - 5}, {px - 5, py - 15},
                           {px - 8, py - 12}, {px - 2, py - 4}};
        SDL_Point lw[4] = {{px + 1, py + 5}, {px - 5, py + 15},
                           {px - 8, py + 12}, {px - 2, py + 4}};
        ArtFX::fillPoly(r, uw, 4, 9, 14, 17, 218);
        ArtFX::fillPoly(r, lw, 4, 9, 14, 17, 218);
        for (int i = 0; i < 4; ++i) {
            ArtFX::glowLine(r, uw[i].x, uw[i].y, uw[(i+1)%4].x, uw[(i+1)%4].y, 196, 216, 210, 130, 1);
            ArtFX::glowLine(r, lw[i].x, lw[i].y, lw[(i+1)%4].x, lw[(i+1)%4].y, 196, 216, 210, 130, 1);
        }

        // 座舱玻璃与尾喷。
        ArtFX::glowLine(r, px + 6, py - 1, px + 9, py + 1, 205, 245, 255, 220, 1);
        int tick = getX() / 11 + getY() / 7 + (int)(getRollAngle() * 10.0);
        int flick = 3 + (tick + py) % 4;
        ArtFX::glowLine(r, px - 10, py, px - 16 - flick, py, 96, 214, 255, 155, 2);
        SDL_SetRenderDrawColor(r, 236, 252, 255, 230);
        SDL_RenderDrawPoint(r, px - 11, py);
        // 翼尖识别灯。
        SDL_SetRenderDrawColor(r, 255, 92, 84, 210);
        SDL_RenderDrawPoint(r, px - 5, py - 14);
        SDL_SetRenderDrawColor(r, 92, 255, 152, 210);
        SDL_RenderDrawPoint(r, px - 5, py + 14);
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
        ArtFX::BlendGuard guard(r);
        int px = getX(), py = getY();

        // 暗夜精灵：前掠翼截击机。轮廓仍在原 32px NoseOffset / 30px 翼展内，
        // 保持命中判定与视觉大小一致。
        int noseX = px + 32;
        int wingSpan = 8;
        int tailX = px - 13;
        SDL_Point hull[6] = {
            {noseX, py},
            {px + 8, py - wingSpan},
            {px - 4, py - 13},
            {tailX, py},
            {px - 4, py + 13},
            {px + 8, py + wingSpan}
        };
        ArtFX::fillPoly(r, hull, 6, 7, 13, 18, 238);
        for (int i = 0; i < 6; ++i) {
            SDL_Point& a = hull[i]; SDL_Point& b = hull[(i + 1) % 6];
            ArtFX::glowLine(r, a.x, a.y, b.x, b.y, 180, 239, 255, 170, 1);
        }
        // 机翼上的双发光导轨 + 中线。
        ArtFX::glowLine(r, px + 7, py - wingSpan, noseX - 3, py - wingSpan, 100, 190, 255, 100, 1);
        ArtFX::glowLine(r, px + 7, py + wingSpan, noseX - 3, py + wingSpan, 100, 190, 255, 100, 1);
        ArtFX::glowLine(r, px - 3, py, px + 20, py, 120, 214, 245, 90, 1);
        // 驾驶舱冷白光。
        SDL_Point ck[4] = {{px + 9, py - 4}, {px + 17, py - 2},
                           {px + 17, py + 2}, {px + 9, py + 4}};
        ArtFX::fillPoly(r, ck, 4, 132, 224, 246, 210);
        ArtFX::glowLine(r, px + 9, py, px + 18, py, 236, 255, 255, 220, 1);

        // 双发尾焰：NightElf 比训练机更冷、更亮。
        int tick = getX() / 9 + getY() / 5 + (int)(getRollAngle() * 12.0);
        int flick = 4 + (tick + py) % 5;
        ArtFX::glowLine(r, tailX + 1, py - 4, tailX - 10 - flick, py - 5, 88, 214, 255, 175, 2);
        ArtFX::glowLine(r, tailX + 1, py + 4, tailX - 10 - flick, py + 5, 88, 214, 255, 175, 2);
        SDL_SetRenderDrawColor(r, 236, 252, 255, 230);
        SDL_RenderDrawPoint(r, tailX, py - 4);
        SDL_RenderDrawPoint(r, tailX, py + 4);
        // 翼尖小灯。
        SDL_SetRenderDrawColor(r, 255, 105, 82, 220);
        SDL_RenderDrawPoint(r, px - 4, py - 13);
        SDL_SetRenderDrawColor(r, 92, 255, 166, 220);
        SDL_RenderDrawPoint(r, px - 4, py + 13);
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
