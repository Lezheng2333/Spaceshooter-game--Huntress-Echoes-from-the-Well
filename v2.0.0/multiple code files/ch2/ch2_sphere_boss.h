#pragma once

class Ch2Background;

#include "../constants.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"
#include <algorithm>
#include <cmath>
#include <vector>

// ============== Ch2SphereBoss ==============
class Ch2SphereBoss {
public:
    enum State { INACTIVE, ENTERING, ACTIVATING, FIGHT, SHATTERING,
                 COLLAPSING, SHAKING, RUSHING_OUT, DONE };

    struct Cell {
        double relX, relY;
        int activationOrder, shatterOrder;
        bool onSphere, activated;
        int colorState; // 0=blue 1=orange 2=white
        template <class Ar> void visit(Ar& ar) {
            ar.ioNum(relX); ar.ioNum(relY);
            ar.ioNum(activationOrder); ar.ioNum(shatterOrder);
            ar.ioBool(onSphere); ar.ioBool(activated); ar.ioNum(colorState);
        }
    };
    struct SphereDebris {
        double x, y, vx, vy, rotAngle, rotSpeed;
        bool onGround, rushing;
        double shakeDelay, shakePhase;
        double rushVx, rushVy;
        double floorY;     // per-debris landing Y for perspective floor scatter
        template <class Ar> void visit(Ar& ar) {
            ar.ioNum(x); ar.ioNum(y); ar.ioNum(vx); ar.ioNum(vy);
            ar.ioNum(rotAngle); ar.ioNum(rotSpeed);
            ar.ioBool(onGround); ar.ioBool(rushing);
            ar.ioNum(shakeDelay); ar.ioNum(shakePhase);
            ar.ioNum(rushVx); ar.ioNum(rushVy); ar.ioNum(floorY);
        }
    };

    Ch2SphereBoss() : cx(900), cy(290), worldX(1200), radius(150),
        hp(500), maxHp(500), state(INACTIVE), stateTimer(0),
        activationIdx(0), shatterIdx(0), rushIdx(0),
        fixedSeed(12345), entryStartX(900), entryBgStopped(false),
        bg(nullptr), playerRef(nullptr), bgTargetSpeed(-1.0) {}

    void init(Ch2Background* b, Player* p) { bg = b; playerRef = p; }
    void reset() { state = INACTIVE; stateTimer = 0; cells.clear(); activationByOrder.clear();
        debris.clear(); activationIdx = shatterIdx = rushIdx = 0; hp = maxHp; }

    State getState() const { return state; }
    int getStateTimer() const { return stateTimer; }
    bool isActive() const { return state != INACTIVE && state != DONE; }

    void startEntering() {
        reset();
        generateCells();
        state = ENTERING; stateTimer = 0;
        worldX = 1200; // far right, scrolls in with background
        cx = worldX; cy = 290;
    }

    // Normal flow entry: sphere appears 1200 units ahead of the CURRENT scroll,
    // so it works regardless of how far the corridor has already scrolled.
    void startEnteringAt(double scrollX) {
        reset();
        generateCells();
        state = ENTERING; stateTimer = 0;
        worldX = scrollX + 1200.0;
        cx = worldX - scrollX;  // = 1200 on screen
        cy = 290;
    }

    // Test mode: spawn sphere at center in FIGHT state immediately
    void startAtCenter() {
        reset();
        worldX = WIN_WIDTH / 2; cx = worldX; cy = 290;
        generateCells();
        for (auto& c : cells) { c.activated = true; c.colorState = 1; }
        bgTargetSpeed = 0.0; // stop background
        state = FIGHT; stateTimer = 0;
    }

    // Update screen position from world coords
    void syncScreenPos(double scrollX) {
        cx = worldX - scrollX;
    }

    double getCx() const { return cx; }
    double getCy() const { return cy; }
    double getRadius() const { return radius; }
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    double getBgTargetSpeed() const { return bgTargetSpeed; }
    void clearBgTargetSpeed() { bgTargetSpeed = -1.0; }
    void takeDamage(int dmg) { if (state == FIGHT) { hp -= dmg; if (hp < 0) hp = 0; } }

    int countOnSphere() const {
        int n = 0;
        for (const auto& c : cells) if (c.onSphere) n++;
        return n;
    }

    // 一颗子弹打掉最多 maxDrops 个菱形；HP 严格按实际掉落数扣除，
    // 保证血条见底时球体上不再有菱形。
    int hitDiamonds(int maxDrops) {
        if (state != FIGHT) return 0;
        int dropped = popDiamonds(maxDrops);
        hp -= dropped;
        if (hp < 0) hp = 0;
        return dropped;
    }

    // 兼容旧存档：旧版 maxHp=500、每发只掉 1 个；读档后把血量重新映射到剩余菱形数。
    void normalizeHpToCells() {
        if (cells.empty()) return;
        int total = (int)cells.size();
        int on = countOnSphere();
        if (maxHp != total) maxHp = total;
        if (state == FIGHT || state == ACTIVATING || state == ENTERING) hp = on;
        else if (hp > maxHp) hp = maxHp;
    }

    // 存档：球体 Boss 全部状态（六角格子激活/碎裂进度、碎片、激活顺序表）
    // 注意：activationByOrder 会被打乱，必须原样存取；bg/playerRef 是指针，不进存档。
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(cx); ar.ioNum(cy); ar.ioNum(worldX); ar.ioNum(radius);
        ar.ioNum(hp); ar.ioNum(maxHp);
        ar.ioVecObj(cells);
        ar.ioVecNum(activationByOrder);
        ar.ioVecObj(debris);
        ar.ioEnum(state); ar.ioNum(stateTimer);
        ar.ioNum(activationIdx); ar.ioNum(shatterIdx); ar.ioNum(rushIdx);
        ar.ioNum(fixedSeed); ar.ioNum(entryStartX); ar.ioBool(entryBgStopped);
        ar.ioNum(bgTargetSpeed);
    }

    void update() {
        if (state == INACTIVE || state == DONE) return;
        stateTimer++;

        switch (state) {
        case ENTERING: {
            // Sphere scrolls in with background; slow down as it approaches center
            double targetScreenX = WIN_WIDTH / 2;
            double distToTarget = std::abs(cx - targetScreenX);
            double slowFactor = 1.0;
            if (distToTarget < 300) {
                slowFactor = distToTarget / 300.0;
                if (slowFactor < 0.05) slowFactor = 0.05;
            }
            bgTargetSpeed = 3.5 * slowFactor;
            // Lock position and stop bg when sphere reaches center
            if (distToTarget < 8) {
                cx = targetScreenX; bgTargetSpeed = 0.0;
                state = ACTIVATING; stateTimer = 0;
            }
            break;
        }
        case ACTIVATING: {
            // Slow stochastic wave from bottom to top (~180 frames)
            int remaining = (int)cells.size() - activationIdx;
            if (remaining <= 0) { state = FIGHT; stateTimer = 0; break; }
            int perFrame = remaining / 160 + 1; // slow pace
            int window = remaining / 20 + 8;     // lookahead window for randomness
            if (window > remaining) window = remaining;
            for (int i = 0; i < perFrame && activationIdx < (int)cells.size(); ++i) {
                // Pick randomly within the upcoming window
                int pick = activationIdx + (rand() % window);
                if (pick >= (int)cells.size()) pick = (int)cells.size() - 1;
                int ci = activationByOrder[pick];
                if (!cells[ci].activated) {
                    cells[ci].activated = true; cells[ci].colorState = 1;
                    // Swap picked cell down to activationIdx so it's marked done
                    std::swap(activationByOrder[pick], activationByOrder[activationIdx]);
                }
                // Advance activationIdx past any already-activated cells
                while (activationIdx < (int)cells.size() &&
                       cells[activationByOrder[activationIdx]].activated)
                    activationIdx++;
            }
            if (activationIdx >= (int)cells.size()) {
                state = FIGHT; stateTimer = 0;
            }
            break;
        }
        case FIGHT: {
            updateDebrisPhysics();
            bool allOff = true;
            for (auto& c : cells) if (c.onSphere) { allOff = false; break; }
            if (hp <= 0 || allOff) { state = SHATTERING; stateTimer = 0; shatterIdx = 0; }
            break;
        }
        case SHATTERING: {
            // Pop remaining cells rapidly
            int perFrame = (int)cells.size() / 60 + 1;
            popDiamonds(perFrame);
            updateDebrisPhysics();
            bool allOff = true;
            for (auto& c : cells) if (c.onSphere) { allOff = false; break; }
            // 血条见底后：黑球先幽灵化飘走，再等最后几块真正落到地面，最后才开始地面震动演出
            bool allGrounded = true;
            for (const auto& d : debris) {
                if (!d.onGround && !d.rushing) { allGrounded = false; break; }
            }
            if (allOff && allGrounded && stateTimer >= GHOST_END) {
                // Assign progressive shake delays for chain reaction
                int n = (int)debris.size();
                for (int i = 0; i < n; ++i) {
                    // First debris: starts immediately, then wave propagates
                    double baseDelay = (double)i / n * 180.0; // spread over ~3s
                    debris[i].shakeDelay = baseDelay;
                    debris[i].shakePhase = 0;
                }
                state = SHAKING; stateTimer = 0;
            }
            break;
        }
        case SHAKING: {
            if (debris.empty()) { state = DONE; stateTimer = 0; bgTargetSpeed = 3.5; break; }
            int shakeDuration = 120; // frames each debris shakes before rushing out
            for (auto& d : debris) {
                if (d.rushing) {
                    d.x += d.rushVx; d.y += d.rushVy;
                    d.rotAngle += 0.15;
                    continue;
                }
                if (stateTimer >= d.shakeDelay) {
                    double localT = stateTimer - d.shakeDelay;
                    // Shake: increasing amplitude
                    double amp = std::min(localT * 0.04, 3.5);
                    d.shakePhase = std::sin(localT * 0.7) * amp;
                    // After shakeDuration, rush out in random direction
                    if (localT >= shakeDuration) {
                        d.rushing = true;
                        double angle = (rand() % 6283) / 1000.0;
                        double speed = 4.0 + (rand() % 400) / 100.0;
                        d.rushVx = std::cos(angle) * speed;
                        d.rushVy = std::sin(angle) * speed;
                        d.shakePhase = 0;
                    }
                }
            }
            // Remove off-screen debris
            debris.erase(std::remove_if(debris.begin(), debris.end(),
                [](const SphereDebris& d){ return d.rushing && (d.x<-60||d.x>WIN_WIDTH+60||d.y<-60||d.y>WIN_HEIGHT+60); }),
                debris.end());
            // Done when all debris gone
            if (debris.empty()) {
                state = DONE; stateTimer = 0;
                bgTargetSpeed = 3.5;
            }
            break;
        }
        default: break;
        }
    }

    void updateDebrisPhysics() {
        for (auto& d : debris) {
            if (d.onGround || d.rushing) continue;
            d.vy += 0.3; d.x += d.vx; d.y += d.vy;
            d.rotAngle += d.rotSpeed;
            if (d.y > d.floorY) {
                d.y = d.floorY; d.vy *= -0.3; d.vx *= 0.7;
                if (std::abs(d.vy) < 0.5) { d.vy = 0; d.vx = 0; d.onGround = true; }
            }
        }
    }

    void popSingleCell(Cell& c) {
        c.onSphere = false; c.colorState = 2;
        double sx = cx + c.relX, sy = cy + c.relY;
        // Perspective floor scatter: debris lands around the sphere on the floor
        // depthFraction: 0=top of sphere (near WALL_Y), 1=bottom (near viewer)
        double depthFraction = (c.relY + radius) / (2.0 * radius);
        if (depthFraction < 0.1) depthFraction = 0.1;
        if (depthFraction > 0.9) depthFraction = 0.9;
        double dFloorY = 280.0 + (cy + radius - 280.0) * depthFraction;
        dFloorY += (rand() % 60 - 30); // slight random variation
        if (dFloorY < 280) dFloorY = 280;
        if (dFloorY > cy + radius) dFloorY = cy + radius;
        // Horizontal scatter wider at bottom (closer), narrower at top (farther)
        double hSpread = 40.0 + 160.0 * depthFraction;
        double targetX = cx + (rand() % (int)(hSpread * 2)) - hSpread;
        double dx = targetX - sx;
        double angle = (rand() % 6283) / 1000.0;
        double speed = 1.5 + (rand() % 100) / 100.0 * 2.0;
        SphereDebris d;
        d.x = sx; d.y = sy;
        d.vx = dx * 0.05 + std::cos(angle) * speed;
        d.vy = -2.0 - (rand() % 40) / 10.0;
        d.floorY = dFloorY;
        d.rotAngle = 0; d.rotSpeed = (rand() % 20 - 10) / 100.0;
        d.onGround = false; d.rushing = false;
        d.shakeDelay = 0; d.shakePhase = 0; d.rushVx = 0; d.rushVy = 0;
        debris.push_back(d);
    }

    // Pop diamonds off the sphere (called on bullet hit); returns the actual count.
    int popDiamonds(int count) {
        // 数组顺序与随机碎裂顺序不同；每次寻找仍在球体上的最小序号，
        // 避免游标越过尚未处理的低序号单元后永远卡在 SHATTERING。
        int dropped = 0;
        for (int i = 0; i < count; ++i) {
            Cell* next = nullptr;
            for (auto& c : cells)
                if (c.onSphere && (!next || c.shatterOrder < next->shatterOrder)) next = &c;
            if (!next) break;
            popSingleCell(*next);
            shatterIdx++;
            dropped++;
        }
        return dropped;
    }

    void draw(SDL_Renderer* r) const {
        if (state == INACTIVE || state == DONE) return;
        if (cells.empty()) return;
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

        if (state == ENTERING || state == ACTIVATING || state == FIGHT) {
            drawBody(r);
            drawPits(r);
        }
        if (state == SHATTERING) drawGhost(r);
        for (const auto& cell : cells) {
            if (cell.onSphere) drawCell(r, cell);
        }
        for (const auto& d : debris) drawDebris(r, d);

        SDL_SetRenderDrawBlendMode(r, prev);
    }

private:
    static const int GHOST_END = 130;   // 黑球幽灵扭曲/飘走时长；结束后地面震动
    double cx, cy, worldX, radius;
    int hp, maxHp;
    std::vector<Cell> cells;
    std::vector<int> activationByOrder; // maps activationOrder → cell index
    std::vector<SphereDebris> debris;
    State state; int stateTimer;
    int activationIdx, shatterIdx, rushIdx;
    uint32_t fixedSeed;
    double entryStartX;
    bool entryBgStopped;
    Ch2Background* bg;
    Player* playerRef;
    double bgTargetSpeed; // speed request for Ch2Background

    // ---- 小工具 ----
    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double smooth01(double t) { t = clamp01(t); return t * t * (3.0 - 2.0 * t); }
    static void setColor(SDL_Renderer* r, int cr, int cg, int cb, int ca) {
        if (ca < 0) ca = 0; if (ca > 255) ca = 255;
        SDL_SetRenderDrawColor(r, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)ca);
    }
    static void fillR(SDL_Renderer* r, double x, double y, double w, double h,
                      int cr, int cg, int cb, int ca = 255) {
        if (w <= 0 || h <= 0) return;
        setColor(r, cr, cg, cb, ca);
        SDL_Rect rr = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderFillRect(r, &rr);
    }
    static void lineR(SDL_Renderer* r, int x0, int y0, int x1, int y1,
                      int cr, int cg, int cb, int ca = 255) {
        setColor(r, cr, cg, cb, ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }
    static void arcR(SDL_Renderer* r, double x, double y, double rad,
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
            if (i < steps) lineR(r, px, py, nx, ny, cr, cg, cb, ca);
            px = nx; py = ny;
        }
    }
    static void ellipseR(SDL_Renderer* r, double x, double y, double rx, double ry,
                         int cr, int cg, int cb, int ca) {
        int steps = 48;
        for (int i = 0; i < steps; ++i) {
            double a0 = 2.0 * M_PI * i / steps;
            double a1 = 2.0 * M_PI * (i + 1) / steps;
            lineR(r, (int)(x + std::cos(a0) * rx), (int)(y + std::sin(a0) * ry),
                     (int)(x + std::cos(a1) * rx), (int)(y + std::sin(a1) * ry),
                  cr, cg, cb, ca);
        }
    }
    static void fillEllipseR(SDL_Renderer* r, double x, double y, double rx, double ry,
                             int cr, int cg, int cb, int ca) {
        int y0 = (int)(y - ry), y1 = (int)(y + ry);
        for (int yy = y0; yy <= y1; ++yy) {
            double dy = (yy - y) / ry;
            if (std::fabs(dy) > 1.0) continue;
            double hw = rx * std::sqrt(1.0 - dy * dy);
            fillR(r, x - hw, yy, hw * 2.0, 1, cr, cg, cb, ca);
        }
    }
    static void diamondR(SDL_Renderer* r, double x, double y, double sz, double tilt,
                         int cr, int cg, int cb, int ca, bool diag) {
        double hw = sz * 2.0 / 3.0;
        double ca_ = std::cos(tilt), sa_ = std::sin(tilt);
        auto pt = [&](double lx, double ly, int& ox, int& oy) {
            ox = (int)(x + lx * ca_ - ly * sa_);
            oy = (int)(y + lx * sa_ + ly * ca_);
        };
        int x0, y0, x1, y1, x2, y2, x3, y3;
        pt(0, -sz, x0, y0); pt(hw, 0, x1, y1); pt(0, sz, x2, y2); pt(-hw, 0, x3, y3);
        setColor(r, cr, cg, cb, ca);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
        SDL_RenderDrawLine(r, x1, y1, x2, y2);
        SDL_RenderDrawLine(r, x2, y2, x3, y3);
        SDL_RenderDrawLine(r, x3, y3, x0, y0);
        if (diag) {
            SDL_RenderDrawLine(r, x3, y3, x1, y1);
            SDL_RenderDrawLine(r, x0, y0, x2, y2);
        }
    }
    static double hash01(double x) {
        double h = std::sin(x * 12.9898) * 43758.5453;
        return h - std::floor(h);
    }

    // 三处暗坑：位置/半径固定，生成时删掉格子，绘制时在壳内点亮微光
    static void pitInfo(int i, double& x, double& y, double& r) {
        static const double px[3] = {-54.0, 46.0, 16.0};
        static const double py[3] = {-42.0, -18.0, 70.0};
        static const double pr[3] = {22.0, 19.0, 24.0};
        if (i < 0) i = 0; if (i > 2) i = 2;
        x = px[i]; y = py[i]; r = pr[i];
    }
    static bool inPit(double x, double y) {
        for (int i = 0; i < 3; ++i) {
            double px, py, pr; pitInfo(i, px, py, pr);
            double dx = x - px, dy = y - py;
            double ang = std::atan2(dy, dx);
            double rr = pr * (0.86 + 0.22 * hash01(ang * 3.1 + i * 4.7));
            if (dx*dx + dy*dy < rr*rr) return true;
        }
        return false;
    }

    double breatheScale() const { return 1.0 + 0.008 * std::sin(stateTimer * 0.03); }
    double heatLevel() const {
        int total = 0, act = 0;
        for (const auto& c : cells) if (c.onSphere) { total++; if (c.activated) act++; }
        return total > 0 ? (double)act / total : 0.0;
    }

    void drawBody(SDL_Renderer* r) const {
        double breathe = breatheScale();
        int rad = (int)(radius * breathe);
        // 半透明的深色本体：让壳后面的星空消失，球体变成有质量的“生物”
        for (int yy = -rad; yy <= rad; yy += 2) {
            double hw = std::sqrt(std::max(0.0, (double)rad*rad - (double)yy*yy));
            int a = 232 + (int)(10 * std::sin(yy * 0.11 + stateTimer * 0.01));
            if (a > 244) a = 244;
            fillR(r, cx - hw, cy + yy, hw * 2.0, 2, 7, 9, 15, a);
        }
        double heat = heatLevel();
        double pulse = 0.62 + 0.38 * std::sin(stateTimer * 0.045);
        int gr = (int)(50 + 190 * heat), gg = (int)(64 + 52 * heat), gb = (int)(120 - 58 * heat);
        // 体内缓慢搏动的核心光
        for (int yy = -86; yy <= 86; yy += 4) {
            double hw = 78.0 * std::sqrt(std::max(0.0, 1.0 - (yy / 86.0) * (yy / 86.0)));
            int a = (int)((5 + 13 * heat) * pulse);
            fillR(r, cx - hw, cy + yy, hw * 2.0, 4, gr, gg, gb, a);
        }
        fillR(r, cx - 4, cy - 4, 9, 9, gr, gg, gb, (int)(34 + 55 * heat));
        fillR(r, cx - 1, cy - 1, 3, 3, 255, (int)(196 + 40 * heat),
              (int)(176 - 50 * heat), 120);
        // 不规则外膜
        for (int k = 0; k < 8; ++k) {
            double a0 = k * M_PI / 4.0 + 0.04 * std::sin(stateTimer * 0.012 + k * 1.7);
            double a1 = a0 + M_PI / 4.0;
            double rr = rad * (0.985 + 0.012 * std::sin(k * 2.3 + stateTimer * 0.018));
            arcR(r, cx, cy, rr, a0, a1, 32, 44, 60, 70);
            arcR(r, cx, cy, rr + 2.0, a0, a1, 18, 26, 38, 85);
        }
        // 边缘的短棘刺：打破完美圆形轮廓
        for (int i = 0; i < 14; ++i) {
            double ang = i * 2.0 * M_PI / 14.0
                       + 0.07 * std::sin(stateTimer * 0.02 + i * 1.9);
            double r0 = rad - 2.0;
            double r1 = rad + 4.0 + 6.0 * hash01(i * 3.7 + 1.2);
            lineR(r, (int)(cx + std::cos(ang) * r0), (int)(cy + std::sin(ang) * r0),
                     (int)(cx + std::cos(ang) * r1), (int)(cy + std::sin(ang) * r1),
                  44, 58, 78, 90);
        }
        // 几根更长、更不规则的棘刺，让轮廓不像标准圆
        for (int i = 0; i < 6; ++i) {
            double ang = (i * 1.13 + 0.37) * M_PI
                       + 0.10 * std::sin(stateTimer * 0.017 + i * 2.3);
            double r0 = rad - 1.0;
            double r1 = rad + 8.0 + 14.0 * hash01(i * 7.3 + 4.1);
            lineR(r, (int)(cx + std::cos(ang) * r0), (int)(cy + std::sin(ang) * r0),
                     (int)(cx + std::cos(ang) * r1), (int)(cy + std::sin(ang) * r1),
                  72, 88, 112, 110);
        }
    }

    void drawPits(SDL_Renderer* r) const {
        double breathe = breatheScale();
        double heat = heatLevel();
        double pulse = 0.55 + 0.45 * std::sin(stateTimer * 0.05 + 1.3);
        for (int i = 0; i < 3; ++i) {
            double px, py, pr; pitInfo(i, px, py, pr);
            double bx = cx + px * breathe, by = cy + py * breathe;
            double rr = pr * breathe;
            fillEllipseR(r, bx, by, rr + 3.0, rr * 0.92, 2, 3, 6, 240);
            ellipseR(r, bx, by, rr + 2.0, rr * 0.90, 42, 58, 80, 60);
            int gr = (int)(70 + 185 * heat);
            int gg = (int)(100 - 40 * heat);
            int gb = (int)(150 - 90 * heat);
            fillEllipseR(r, bx, by, rr * 0.66, rr * 0.56, gr, gg, gb,
                         (int)((12 + 22 * heat) * pulse));
            fillEllipseR(r, bx, by, rr * 0.30, rr * 0.24, 255, 180, 120, 32);
            ellipseR(r, bx, by, rr * 0.52, rr * 0.44, 255, 118, 86,
                     (int)(30 + 30 * pulse));
            fillR(r, bx - 1, by - 1, 3, 3, 255, 226, 196, (int)(26 + 34 * pulse));
        }
    }

    // 黑球本体在 HP 归零后的幽灵阶段：扭曲、拉长、长出尾须，
    // 带着原来的三个暗坑面孔向上飘走；随后才让地面残骸震动。
    void drawGhost(SDL_Renderer* r) const {
        double t = clamp01(stateTimer / double(GHOST_END));
        double e = smooth01(t);
        double fade = 1.0 - smooth01(clamp01((t - 0.52) / 0.48));
        if (fade <= 0.01) return;
        double gx = cx + 44.0 * e + 9.0 * std::sin(t * 8.0);
        double gy = cy - 96.0 * e - 8.0 * std::sin(t * 5.0);
        double gs = 1.0 + 0.12 * e;
        double rad = radius * gs;

        // 背后的幽光：让纯黑轮廓在星空里能被看见
        for (int k = 0; k < 4; ++k) {
            double rr = rad + 6 + k * 7;
            arcR(r, gx, gy, rr, -M_PI * 0.95, M_PI * 0.55,
                 70, 120, 155, (int)((22 - k * 3) * fade));
        }

        // 被拉长/扭曲的黑色本体
        for (int yy = -(int)rad; yy <= (int)rad; yy += 2) {
            double dy = (double)yy;
            double hw = std::sqrt(std::max(0.0, rad*rad - dy*dy));
            double wav = 1.0 + 0.18 * e * std::sin(dy * 0.075 + t * 9.0)
                             + 0.09 * e * std::sin(dy * 0.23 - t * 13.0);
            int xoff = (int)(std::sin(dy * 0.045 + t * 6.0) * 16.0 * e);
            int a = (int)(200 * fade);
            fillR(r, gx + xoff - hw * wav, gy + dy, hw * wav * 2.0, 2, 1, 2, 4, a);
        }
        // 下方拖出的黑雾尾须
        for (int k = 0; k < 40; ++k) {
            double u = k / 40.0;
            double ty = rad + u * (54.0 + 22.0 * e);
            double hw = (1.0 - u) * (19.0 + 8.0 * std::sin(t * 7.0 + k * 0.4));
            int xoff = (int)(std::sin(t * 5.0 + u * 4.0) * 11.0 * e * u);
            fillR(r, gx + xoff - hw, gy + ty, hw * 2.0, 2, 1, 2, 4,
                  (int)(170 * fade * (1.0 - u)));
        }

        // 不规则、带波纹的苍白轮廓 + 两道余影
        auto drawRim = [&](double ox, double oy, double rr, double amp, int aBase) {
            int steps = 72;
            int px = 0, py = 0;
            for (int i = 0; i <= steps; ++i) {
                double ang = 2.0 * M_PI * i / steps;
                double wr = rr * (1.0 + 0.15 * amp * std::sin(ang * 5.0 + t * 7.0)
                                      + 0.08 * amp * std::sin(ang * 9.0 - t * 11.0));
                int x = (int)(ox + std::cos(ang) * wr);
                int y = (int)(oy + std::sin(ang) * wr * (1.0 + 0.06 * amp));
                if (i > 0) lineR(r, px, py, x, y, 150, 205, 230, aBase);
                px = x; py = y;
            }
        };
        for (int k = 2; k >= 1; --k) {
            double t2 = std::max(0.0, t - 0.10 * k);
            double e2 = smooth01(t2);
            double ox = cx + 44.0 * e2 + 9.0 * std::sin(t2 * 8.0) - 12.0 * k * e;
            double oy = cy - 96.0 * e2 - 8.0 * std::sin(t2 * 5.0) + 8.0 * k * e;
            double rr = radius * (1.0 + 0.10 * e2);
            drawRim(ox, oy, rr, e2, (int)(18 * fade));
        }
        drawRim(gx, gy, rad, e, (int)(90 * fade));
        drawRim(gx, gy, rad + 2.0, e, (int)(40 * fade));

        // 残留的三个暗坑：拉长成竖缝，保留这张“脸”的陌生感
        for (int i = 0; i < 3; ++i) {
            double px, py, pr; pitInfo(i, px, py, pr);
            double bx = gx + px * gs + std::sin(t * 6.0 + i * 2.1) * 4.0 * e;
            double by = gy + py * gs + std::cos(t * 4.7 + i) * 3.0 * e;
            double rx = pr * (0.85 + 0.25 * e);
            double ry = pr * (1.05 + 0.75 * e);
            fillEllipseR(r, bx, by, rx, ry, 0, 0, 1, (int)(210 * fade));
            ellipseR(r, bx, by, rx + 1.0, ry + 1.0, 155, 205, 232, (int)(70 * fade));
            ellipseR(r, bx, by, rx * 0.62, ry * 0.58, 190, 110, 145, (int)(30 * fade));
            fillR(r, bx - 1, by - 1, 3, 3, 235, 200, 175, (int)(55 * fade));
        }

        // 幽灵触须：从身体边缘垂下的细线
        for (int k = 0; k < 6; ++k) {
            double base = -M_PI * 0.15 + k * M_PI / 5.0;
            double px = gx + std::cos(base) * rad * 0.95;
            double py = gy + std::sin(base) * rad * 0.92;
            for (int s = 1; s <= 8; ++s) {
                double u = s / 8.0;
                double nx = px + std::sin(t * 6.0 + k + u * 5.0) * 8.0 * u * e;
                double ny = py + u * (28.0 + 16.0 * e);
                int a = (int)((22 - k * 2) * fade * (1.0 - u * 0.5));
                if (a < 2) break;
                if (s == 1) lineR(r, (int)px, (int)py, (int)nx, (int)ny, 140, 190, 220, a);
                else lineR(r, (int)px, (int)py, (int)nx, (int)ny, 140, 190, 220, a);
                px = nx; py = ny;
            }
        }
        // 最后一点余温
        fillR(r, gx - 2, gy - 2, 5, 5, 255, 120, 80, (int)(30 * fade * (1.0 - t)));
    }

    void drawCell(SDL_Renderer* r, const Cell& cell) const {
        double breathe = breatheScale();
        double h1 = hash01(cell.relX * 0.73 + cell.relY * 1.17 + cell.shatterOrder * 2.31);
        double h2 = hash01(cell.relX * 1.91 - cell.relY * 0.57 + cell.shatterOrder * 4.73);
        double h3 = hash01(cell.relX * 0.31 + cell.relY * 2.17 + cell.shatterOrder * 1.19);
        double dist = std::sqrt(cell.relX * cell.relX + cell.relY * cell.relY);
        double edgeScale = 0.55 + 0.45 * (1.0 - dist / radius);
        double sz = 10.0 * edgeScale * (0.70 + 0.60 * h2);
        if (sz < 2.5) sz = 2.5;

        double tilt = (h1 - 0.5) * 0.55;
        double jx = 0.0, jy = 0.0;
        bool tremble = h3 < 0.075;
        if (tremble) {
            double ph = stateTimer * 0.32 + h1 * 6.2832;
            jx = std::sin(ph) * 1.5;
            jy = std::cos(ph * 1.27) * 1.0;
            tilt += 0.13 * std::sin(ph * 0.9);
            sz *= 1.0 + 0.07 * std::sin(ph * 1.6);
        }
        double sx = cx + cell.relX * breathe + jx;
        double sy = cy + cell.relY * breathe + jy;

        int rCol, gCol, bCol;
        if (cell.colorState == 0) { rCol = 40; gCol = 130; bCol = 230; }
        else if (cell.colorState == 1) { rCol = 230; gCol = 140; bCol = 50; }
        else { rCol = 255; gCol = 255; bCol = 240; }
        double ny = cell.relY / std::max(1.0, dist);
        double bright = 0.35 + 0.65 * (-ny * 0.55 + 0.45);
        rCol = (int)(rCol * bright); gCol = (int)(gCol * bright); bCol = (int)(bCol * bright);
        if (rCol > 255) rCol = 255; if (gCol > 255) gCol = 255; if (bCol > 255) bCol = 255;

        // 激活后的橙色细胞带一层很淡的辉光，像被点亮的感染区
        if (cell.colorState == 1) {
            double gp = 0.55 + 0.45 * std::sin(stateTimer * 0.06 + h1 * 9.0);
            diamondR(r, sx, sy, sz + 3.2, tilt, 255, 120, 45, (int)(20 * gp), false);
        }
        diamondR(r, sx, sy, sz, tilt, rCol, gCol, bCol, 255, true);
        if (cell.colorState == 1) {
            fillR(r, sx - 1, sy - 1, 2, 2, 255, 205, 130, 54);
        }
    }

    // 地面震动/飞出沿用改版前的清晰灰白菱形：高对比、无光晕、无拖尾，
    // 让逐颗震颤和随机飞出的节奏更容易看清。
    void drawDebris(SDL_Renderer* r, const SphereDebris& d) const {
        int sz = 8;
        int hw = sz * 2 / 3;
        int rCol = 200, gCol = 200, bCol = 200;
        int alpha = d.rushing ? 180 : 200;
        int ex = (int)(d.x + d.shakePhase), ey = (int)d.y;
        double cRot = std::cos(d.rotAngle), sRot = std::sin(d.rotAngle);
        SDL_Point pts[4] = {
            {ex + (int)(-sz * sRot), ey + (int)(-sz * cRot)},
            {ex + (int)( hw * cRot), ey + (int)( hw * sRot)},
            {ex + (int)( sz * sRot), ey + (int)( sz * cRot)},
            {ex + (int)(-hw * cRot), ey + (int)(-hw * sRot)}
        };
        setColor(r, rCol, gCol, bCol, alpha);
        SDL_RenderDrawLines(r, pts, 4);
        SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
        SDL_RenderDrawLine(r, ex - (int)(hw*cRot), ey - (int)(hw*sRot),
                              ex + (int)(hw*cRot), ey + (int)(hw*sRot));
    }

    void generateCells() {
        cells.clear();
        const double SPACING = 16.0;
        const double ROW_H = SPACING * 0.8660254; // sqrt(3)/2

        // Center the hex grid on (0,0) for symmetry
        int halfRows = (int)(radius / ROW_H) + 2;
        int halfCols = (int)(radius / SPACING) + 2;

        for (int row = -halfRows; row <= halfRows; ++row) {
            double rowY = row * ROW_H;
            double rowOff = (row % 2 == 0) ? 0.0 : SPACING * 0.5;
            for (int col = -halfCols; col <= halfCols; ++col) {
                double px = col * SPACING + rowOff;
                double py = rowY;
                double dist = std::sqrt(px*px + py*py);
                if (dist > radius - 2.0) continue;

                // 有机形变：低频“瓣” + 缓慢旋涡，把规则六角网格拧成不规则的卵/菌团
                double ang = std::atan2(py, px);
                double lobe = 1.0 + 0.060 * std::sin(ang * 3.0 + 0.9)
                                   + 0.038 * std::cos(ang * 5.0 - 0.4)
                                   + 0.020 * std::sin(dist * 0.085);
                double wx = px * lobe, wy = py * lobe;
                double twist = 0.085 * (1.0 - dist / radius)
                             + 0.025 * std::sin(ang * 2.0 + 0.6);
                double ca = std::cos(twist), sa = std::sin(twist);
                double qx = wx * ca - wy * sa;
                double qy = wx * sa + wy * ca;
                if (std::sqrt(qx*qx + qy*qy) > radius - 1.0) continue;
                // 三处不规则的暗坑：不是眼睛，但会打破对称、形成“被注视”的陌生感
                if (inPit(qx, qy)) continue;

                Cell c;
                c.relX = qx; c.relY = qy;
                c.onSphere = true;
                c.activated = false;
                c.colorState = 0; // blue
                c.activationOrder = 0;
                c.shatterOrder = 0;
                cells.push_back(c);
            }
        }

        // Assign activationOrder: sort by Y descending (bottom to top on screen)
        std::vector<int> idxByY(cells.size());
        for (size_t i = 0; i < idxByY.size(); ++i) idxByY[i] = (int)i;
        std::sort(idxByY.begin(), idxByY.end(), [this](int a, int b) {
            return cells[a].relY > cells[b].relY; // larger Y = lower on screen = activated first
        });
        activationByOrder.resize(cells.size());
        for (size_t i = 0; i < idxByY.size(); ++i) {
            cells[idxByY[i]].activationOrder = (int)i;
            activationByOrder[i] = idxByY[i]; // order→index mapping
        }

        // Assign shatterOrder: Fisher-Yates shuffle with fixed seed
        std::vector<int> shatterIdx(cells.size());
        for (size_t i = 0; i < shatterIdx.size(); ++i) shatterIdx[i] = (int)i;
        uint32_t rng = fixedSeed;
        for (int i = (int)shatterIdx.size() - 1; i > 0; --i) {
            rng = rng * 1103515245 + 12345;
            int j = (int)(rng % (uint32_t)(i + 1));
            std::swap(shatterIdx[i], shatterIdx[j]);
        }
        for (size_t i = 0; i < shatterIdx.size(); ++i)
            cells[shatterIdx[i]].shatterOrder = (int)i;

        // 血条 = 球体菱形总数：每次实际掉几个就扣几点，见底时球体上正好没有菱形。
        maxHp = (int)cells.size();
        hp = maxHp;
    }
};
