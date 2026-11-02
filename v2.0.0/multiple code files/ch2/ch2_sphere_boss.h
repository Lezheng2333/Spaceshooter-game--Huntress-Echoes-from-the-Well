#pragma once

class Ch2Background;

#include "../constants.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"

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
            int minTime = allOff ? 20 : 90;
            if (allOff && stateTimer >= minTime) {
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

    // Pop diamonds off the sphere (called on bullet hit)
    void popDiamonds(int count) {
        // 数组顺序与随机碎裂顺序不同；每次寻找仍在球体上的最小序号，
        // 避免游标越过尚未处理的低序号单元后永远卡在 SHATTERING。
        for (int i = 0; i < count; ++i) {
            Cell* next = nullptr;
            for (auto& c : cells)
                if (c.onSphere && (!next || c.shatterOrder < next->shatterOrder)) next = &c;
            if (!next) break;
            popSingleCell(*next);
            shatterIdx++;
        }
    }

    void draw(SDL_Renderer* r) const {
        if (state == INACTIVE || state == DONE) return;
        if (cells.empty()) return;

        for (const auto& cell : cells) {
            if (!cell.onSphere) continue;
            double sx = cx + cell.relX;
            double sy = cy + cell.relY;
            // Compute distance from center for edge compression
            double dist = std::sqrt(cell.relX * cell.relX + cell.relY * cell.relY);
            double edgeScale = 0.55 + 0.45 * (1.0 - dist / radius);
            double sz = 10.0 * edgeScale;
            if (sz < 2.5) sz = 2.5;
            int hw = (int)(sz * 2.0 / 3.0);

            // Color
            int rCol, gCol, bCol;
            if (cell.colorState == 0) { // blue (invincible)
                rCol = 40; gCol = 130; bCol = 230;
            } else if (cell.colorState == 1) { // orange (attackable)
                rCol = 230; gCol = 140; bCol = 50;
            } else { // white (shattering flash)
                rCol = 255; gCol = 255; bCol = 240;
            }
            // Top-down lighting: brighter at top, darker at bottom
            double ny = cell.relY / std::max(1.0, dist); // upward normal component
            double bright = 0.35 + 0.65 * (-ny * 0.55 + 0.45);
            rCol = (int)(rCol * bright); gCol = (int)(gCol * bright); bCol = (int)(bCol * bright);
            if (rCol > 255) rCol = 255; if (gCol > 255) gCol = 255; if (bCol > 255) bCol = 255;

            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, 255);
            int ex = (int)sx, ey = (int)sy;
            // Diamond: 4 vertices + closing line + 2 diagonals
            SDL_Point pts[4] = {
                {ex, ey - (int)sz},
                {ex + hw, ey},
                {ex, ey + (int)sz},
                {ex - hw, ey}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(r, ex - hw, ey, ex + hw, ey);
        }

        // Draw debris (broken-off diamonds)
        for (const auto& d : debris) {
            int sz = 8;
            int hw = sz * 2 / 3;
            int rCol = 200, gCol = 200, bCol = 200; // grey-white debris
            int alpha = d.rushing ? 180 : 200;
            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, (Uint8)alpha);
            int ex = (int)(d.x + d.shakePhase), ey = (int)d.y;
            double cRot = std::cos(d.rotAngle), sRot = std::sin(d.rotAngle);
            SDL_Point pts[4] = {
                {ex + (int)(-sz * sRot), ey + (int)(-sz * cRot)},
                {ex + (int)( hw * cRot), ey + (int)( hw * sRot)},
                {ex + (int)( sz * sRot), ey + (int)( sz * cRot)},
                {ex + (int)(-hw * cRot), ey + (int)(-hw * sRot)}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(r, ex - (int)(hw*cRot), ey - (int)(hw*sRot),
                                  ex + (int)(hw*cRot), ey + (int)(hw*sRot));
        }
    }

private:
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
                Cell c;
                c.relX = px; c.relY = py;
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
    }
};
