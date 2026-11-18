#pragma once

#include "constants.h"
#include "renderer.h"
#include "aim_assist.h"
#include "art_fx.h"
#include "ship_art.h"

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

    // 碰撞盒：把机体近似成"沿机头方向的胶囊"（水平线段 + 半径）。
    // 只让机身中段参与判定，翼尖、尾焰、雷达罩尖都不吃弹，
    // 避免"擦到翼尖就掉血"的不公平命中。各机体按自己的轮廓覆写。
    virtual double getHitBack() const { return 7.0; }   // 线段在机尾方向的长度
    virtual double getHitFront() const { return 9.0; }  // 线段在机头方向的长度
    virtual double getHitRadius() const { return 4.0; } // 机身半径（不含子弹半径）
    bool hitByCircle(double cx, double cy, double r) const {
        double x0 = (double)x - getHitBack();
        double x1 = (double)x + getHitFront();
        double t = (cx - x0) / (x1 - x0);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double sx = x0 + t * (x1 - x0);
        double dx = cx - sx;
        double dy = cy - (double)y;
        double rad = getHitRadius() + r;
        return dx * dx + dy * dy <= rad * rad;
    }
};

// ============== TrainingPlane (Chapter 1 perspective movement) ==============
class TrainingPlane : public Player {
public:
    TrainingPlane() : Player() {}

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        ArtFX::BlendGuard guard(r);
        int tick = getX() / 11 + getY() / 7 + (int)(getRollAngle() * 10.0);
        ShipArt::TrainerView v(r, getX(), getY(), false, getRollAngle(), tick);
        ShipArt::paintSwift(v);
    }

    void reset() {
        x = CENTER_X; y = WIN_HEIGHT - 80;
        resetState();
    }

    void handleInput(const Uint8* keys) override {
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
    int getNoseOffset() const override { return 16; }
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
        int tick = getX() / 11 + getY() / 7 + (int)(getRollAngle() * 10.0);
        ShipArt::TrainerView v(r, getX(), getY(), true, 0.0, tick);
        ShipArt::paintSwift(v);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int, int& ox, int& oy) const override {
        ox = 16; oy = 0;
    }
    int getNoseOffset() const override { return 16; }
    // 雨燕的碰撞盒：只覆盖机身中段（翼尖/尾焰不参与）
    double getHitBack() const override { return 8.0; }
    double getHitFront() const override { return 9.0; }
    double getHitRadius() const override { return 4.0; }
};

// ============== TrainerUpgrade (训练机升级·狮鹫) [DORMANT — 训练机后续升级] ==============
// 方案 D 的定稿：三角翼 + 前翼 + 单发，作为训练机的升级型号保留。
// 默认画第二章侧滚视图；drawView(r, false) 可画第一章透视，供素材工坊双状态预览。
// 游戏内尚未激活，不接入 Game、不新增存档字段。
class TrainerUpgrade : public Player {
public:
    TrainerUpgrade() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override { paintView(r, true); }

    void paintView(SDL_Renderer* r, bool side) const {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        ArtFX::BlendGuard guard(r);
        int tick = getX() / 11 + getY() / 7 + (int)(getRollAngle() * 10.0);
        ShipArt::TrainerView v(r, getX(), getY(), side, getRollAngle(), tick);
        ShipArt::paintGryphon(v);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int, int& ox, int& oy) const override { ox = 16; oy = 0; }
    int getNoseOffset() const override { return 16; }
    double getHitBack() const override { return 8.0; }
    double getHitFront() const override { return 9.0; }
    double getHitRadius() const override { return 4.0; }
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
        int tick = getX() / 9 + getY() / 5 + (int)(getRollAngle() * 12.0);
        ShipArt::paintNightElf(r, getX(), getY(), tick);
    }

    int getGunCount() const override { return tripleFire ? 3 : 1; }
    void getGunOffset(int idx, int& ox, int& oy) const override {
        if (idx == 0)      { ox = 34; oy = 0; }    // nose tip
        else if (idx == 1) { ox = 28; oy = -6; }   // upper canard tip
        else               { ox = 28; oy = 6; }    // lower canard tip
    }
    int getNoseOffset() const override { return 34; }
    // 暗夜精灵 C 的碰撞盒：只覆盖机身中段（长机鼻尖端/翼尖不参与）
    double getHitBack() const override { return 12.0; }
    double getHitFront() const override { return 22.0; }
    double getHitRadius() const override { return 5.0; }
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

// ============== Dryad (树妖) [DORMANT — 第三章候选 / 暗夜精灵家族] ==============
// 方案 B 的定稿：月银飞翼 + 羽刃翼尖 + 三座短舱。游戏内尚未激活，
// 只作为休眠资产保留，供第三章或后续版本调用；原版 Druid 不受影响。
class Dryad : public Player {
public:
    Dryad() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int tick = getX() / 9 + getY() / 5 + (int)(getRollAngle() * 12.0);
        ShipArt::paintDryad(r, getX(), getY(), tick);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int, int& ox, int& oy) const override { ox = 34; oy = 0; }
    int getNoseOffset() const override { return 34; }
};

// ============== DruidWraithlight (德鲁伊·幽光) [DORMANT — 原版 Druid 的迭代版] ==============
// 方案 E 的定稿：无尾隐身箭头 + 三缝式尾喷口。游戏内尚未激活，
// 作为原版 Druid 的迭代型号保留；原版 Druid 的绘制与行为完全不动。
class DruidWraithlight : public Player {
public:
    DruidWraithlight() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int tick = getX() / 9 + getY() / 5 + (int)(getRollAngle() * 12.0);
        ShipArt::paintDruidWraithlight(r, getX(), getY(), tick);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int, int& ox, int& oy) const override { ox = 36; oy = 0; }
    int getNoseOffset() const override { return 36; }
};

