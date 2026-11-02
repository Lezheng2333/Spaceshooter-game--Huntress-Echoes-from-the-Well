#pragma once

#include "../audio.h"
#include "../constants.h"
#include "../floating_text.h"
#include "../font.h"
#include "../particles.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch2WardenBoss (Chapter 2 final boss — "MOONWELL WARDEN") ==============
// An energy construct guarding Moonwell's exit. Large hovering diamond core
// with a rotating ring of shard-orbiters and sweeping blades.
//   ENTERING - descends from the top (invincible)
//   FIGHT    - pattern cycle: radial burst → aimed volley → spiral
//   ENRAGED  - HP < 50%: faster, denser, more bullets
//   DYING    - chain explosions, then the epilogue starts
class Ch2WardenBoss {
public:
    enum State { INACTIVE, ENTERING, FIGHT, ENRAGED, DYING };

    State state;
    int timer;
    double x, y;
    int hp, maxHp;
    int flashTimer;
    double spinAngle;
    int pattern;          // 0=radial burst 1=aimed volley 2=spiral
    int patternTimer;
    int volleyCount;
    int summonTimer;
    bool minionRequested;
    std::vector<Ch2EnemyBullet> bullets;
    int lastHitByPulse;   // pulse wave ID already applied
    int deathTimer;
    bool defeated;

    static const int MAX_HP = 350;
    static const int ENRAGE_HP = 175;
    static const double BODY_RADIUS;   // 48

    Ch2WardenBoss() { reset(); }

    void reset() {
        state = INACTIVE; timer = 0;
        x = 760; y = -120;
        hp = MAX_HP; maxHp = MAX_HP;
        flashTimer = 0; spinAngle = 0;
        pattern = 0; patternTimer = 0; volleyCount = 0;
        summonTimer = 420; minionRequested = false;
        bullets.clear(); lastHitByPulse = -1;
        deathTimer = 0; defeated = false;
    }

    void startEntering() { reset(); state = ENTERING; }

    State getState() const { return state; }
    double getX() const { return x; }
    double getY() const { return y; }
    double getRadius() const { return BODY_RADIUS; }
    bool wantsMinion() const { return minionRequested; }
    void clearMinionRequest() { minionRequested = false; }
    bool isDefeated() const { return defeated; }
    void clearBullets() { bullets.clear(); }
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }

    // 存档：Warden 全部战斗状态（含弹幕模式循环进度/召唤计时）
    template <class Ar> void visit(Ar& ar) {
        ar.ioEnum(state); ar.ioNum(timer); ar.ioNum(x); ar.ioNum(y);
        ar.ioNum(hp); ar.ioNum(maxHp); ar.ioNum(flashTimer); ar.ioNum(spinAngle);
        ar.ioNum(pattern); ar.ioNum(patternTimer); ar.ioNum(volleyCount);
        ar.ioNum(summonTimer); ar.ioBool(minionRequested);
        ar.ioVecObj(bullets);
        ar.ioNum(lastHitByPulse); ar.ioNum(deathTimer); ar.ioBool(defeated);
    }

    void takeDamage(int dmg) {
        if (state != FIGHT && state != ENRAGED) return;
        hp -= dmg;
        flashTimer = 5;
        if (hp <= ENRAGE_HP && state == FIGHT) {
            state = ENRAGED; pattern = 0; patternTimer = 0; volleyCount = 0;
        }
        if (hp <= 0) {
            hp = 0; state = DYING; deathTimer = 0; bullets.clear();
        }
    }

    void fireBullet(double angle, double speed) {
        Ch2EnemyBullet b;
        b.x = x; b.y = y;
        b.dx = std::cos(angle) * speed;
        b.dy = std::sin(angle) * speed;
        b.hp = 3; b.active = true;
        bullets.push_back(b);
    }

    void fireRadial(int n, double speed, double off) {
        for (int i = 0; i < n; ++i)
            fireBullet(off + 2.0 * M_PI * i / n, speed);
    }

    void fireAimed(const Player& pl, double speed, double spread) {
        double dx = pl.getX() - x, dy = pl.getY() - y;
        double d = std::sqrt(dx*dx + dy*dy);
        if (d < 1.0) d = 1.0;
        fireBullet(std::atan2(dy, dx) + spread, speed);
    }

    void update(Player& pl, int& hpRef, bool& goRef,
                ParticleManager& pm, AudioEngine& audio, FloatingTextManager& ftMgr) {
        if (state == INACTIVE) return;
        timer++;
        if (flashTimer > 0) flashTimer--;

        // Bullets flight
        for (auto& b : bullets) {
            if (!b.active) continue;
            b.x += b.dx; b.y += b.dy;
            if (b.x < -20 || b.x > WIN_WIDTH + 20 || b.y < -20 || b.y > WIN_HEIGHT + 20)
                b.active = false;
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Ch2EnemyBullet& b){ return !b.active; }), bullets.end());

        switch (state) {
        case ENTERING: {
            y += 3.0;
            x = 760.0 - 160.0 * std::min(1.0, (y + 120.0) / 290.0);
            if (y >= 170) {
                y = 170;
                state = FIGHT; timer = 0; pattern = 0; patternTimer = 0;
                audio.sndBossEntrance();
            }
            break;
        }
        case FIGHT:
        case ENRAGED: {
            bool enraged = (state == ENRAGED);
            spinAngle += enraged ? 0.045 : 0.025;
            // Swaying hover
            x = 600.0 + std::sin(timer * 0.009) * 55.0;
            y = 170.0 + std::sin(timer * 0.013) * 42.0;

            patternTimer++;
            int cycle = enraged ? 130 : 190;
            switch (pattern) {
            case 0: {   // Radial burst
                if (patternTimer >= cycle) {
                    fireRadial(enraged ? 20 : 14, enraged ? 1.9 : 1.5, spinAngle * 0.5);
                    audio.sndShoot();
                    pattern = 1; patternTimer = 0;
                }
                break;
            }
            case 1: {   // Aimed volleys
                if (patternTimer > 0 && patternTimer % 42 == 0) {
                    fireAimed(pl, 2.2, 0.0);
                    if (enraged) {
                        fireAimed(pl, 2.2, 0.22);
                        fireAimed(pl, 2.2, -0.22);
                    }
                    audio.sndShoot();
                    volleyCount++;
                    if (volleyCount >= 3) { volleyCount = 0; pattern = 2; patternTimer = 0; }
                }
                break;
            }
            case 2: {   // Spiral
                if (patternTimer > 0 && patternTimer % 7 == 0) {
                    double a = patternTimer * 0.13 + spinAngle;
                    fireBullet(a, 1.6);
                    fireBullet(a + M_PI, 1.6);
                    if (enraged) {
                        fireBullet(a + M_PI / 2, 1.6);
                        fireBullet(a - M_PI / 2, 1.6);
                    }
                }
                if (patternTimer >= cycle) { pattern = 0; patternTimer = 0; }
                break;
            }
            }

            // Minion summons
            summonTimer--;
            if (summonTimer <= 0) {
                minionRequested = true;
                summonTimer = enraged ? 360 : 480;
            }
            break;
        }
        case DYING: {
            deathTimer++;
            if (deathTimer == 1) audio.sndExplosionBig();
            if (deathTimer % 8 == 0)
                pm.spawnExplosion(x + (rand() % 110 - 55), y + (rand() % 70 - 35), 10);
            if (deathTimer == 60 || deathTimer == 130) {
                pm.spawnExplosion(x, y, 40);
                audio.sndExplosionBig();
            }
            if (deathTimer >= 200) defeated = true;
            break;
        }
        default: break;
        }

        // Body vs player collision
        if ((state == FIGHT || state == ENRAGED) && pl.getInvFrames() <= 0) {
            double dx = pl.getX() - x, dy = pl.getY() - y;
            if (dx*dx + dy*dy < (BODY_RADIUS + 14.0) * (BODY_RADIUS + 14.0)) {
                hpRef--;
                pl.setInvFrames(60);
                pm.spawnExplosion(pl.getX(), pl.getY(), 8);
                audio.sndPlayerHit();
                ftMgr.spawn((float)pl.getX() + 1, (float)(pl.getY() - 19), "HP -1", 0, 0, 0);
                ftMgr.spawn((float)pl.getX(), (float)(pl.getY() - 20), "HP -1", 255, 50, 50);
                if (hpRef <= 0) { goRef = true; hpRef = 0; }
            }
        }
        // Bullets vs player collision
        if (state != DYING && pl.getInvFrames() <= 0) {
            for (auto& eb : bullets) {
                if (!eb.active) continue;
                double dx = pl.getX() - eb.x, dy = pl.getY() - eb.y;
                if (dx*dx + dy*dy < 14.0 * 14.0) {
                    eb.active = false;
                    hpRef--;
                    pl.setInvFrames(60);
                    pm.spawnExplosion(eb.x, eb.y, 8);
                    audio.sndPlayerHit();
                    ftMgr.spawn((float)pl.getX() + 1, (float)(pl.getY() - 19), "HP -1", 0, 0, 0);
                    ftMgr.spawn((float)pl.getX(), (float)(pl.getY() - 20), "HP -1", 255, 50, 50);
                    if (hpRef <= 0) { goRef = true; hpRef = 0; }
                    break;  // one hit per frame
                }
            }
        }
    }

    void draw(SDL_Renderer* r) const {
        if (state == INACTIVE || defeated) return;
        int cx = (int)x, cy = (int)y;
        bool enraged = (state == ENRAGED);

        int br, bg, bb;
        if (enraged)      { br = 255; bg = 110; bb = 110; }
        else              { br = 120; bg = 210; bb = 255; }
        if (flashTimer > 0) { br = 255; bg = 230; bb = 210; }
        if (state == DYING) {
            // 机体随连锁爆炸衰暗，而不是进入 DYING 的第一帧直接消失。
            double fade = 1.0 - deathTimer / 200.0;
            br = (int)(255 * fade); bg = (int)(180 * fade); bb = (int)(140 * fade);
        }

        // ---- 4 rotating blades ----
        for (int k = 0; k < 4; ++k) {
            double a = spinAngle * 1.5 + k * M_PI / 2.0;
            int bx0 = cx + (int)(std::cos(a) * 30), by0 = cy + (int)(std::sin(a) * 30);
            int bx1 = cx + (int)(std::cos(a) * 78), by1 = cy + (int)(std::sin(a) * 78);
            SDL_SetRenderDrawColor(r, (Uint8)(br / 2), (Uint8)(bg / 2), (Uint8)(bb / 2), 200);
            SDL_RenderDrawLine(r, bx0, by0, bx1, by1);
            // Blade tip diamond
            int tx = cx + (int)(std::cos(a) * 84), ty = cy + (int)(std::sin(a) * 84);
            SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, 255);
            SDL_Point tip[4] = {
                {tx, ty - 6}, {tx + 4, ty}, {tx, ty + 6}, {tx - 4, ty}
            };
            SDL_RenderDrawLines(r, tip, 4);
            SDL_RenderDrawLine(r, tip[3].x, tip[3].y, tip[0].x, tip[0].y);
        }

        // ---- Faint outer ring ----
        SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, 90);
        SDL_Point prev;
        for (int i = 0; i <= 48; ++i) {
            double a = 2.0 * M_PI * i / 48.0;
            int sx = cx + (int)(62.0 * std::cos(a));
            int sy = cy + (int)(62.0 * std::sin(a));
            if (i > 0) SDL_RenderDrawLine(r, prev.x, prev.y, sx, sy);
            prev = {sx, sy};
        }

        // ---- 8 orbiting shard diamonds ----
        for (int i = 0; i < 8; ++i) {
            double a = spinAngle + 2.0 * M_PI * i / 8.0;
            int ox = cx + (int)(std::cos(a) * 62.0);
            int oy = cy + (int)(std::sin(a) * 62.0);
            SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, 230);
            SDL_Point sh[4] = {
                {ox, oy - 7}, {ox + 5, oy}, {ox, oy + 7}, {ox - 5, oy}
            };
            SDL_RenderDrawLines(r, sh, 4);
            SDL_RenderDrawLine(r, sh[3].x, sh[3].y, sh[0].x, sh[0].y);
            SDL_RenderDrawLine(r, ox - 5, oy, ox + 5, oy);
        }

        // ---- Core diamond ----
        const int cs = 30;
        SDL_Point core[4] = {
            {cx, cy - cs}, {cx + cs * 2 / 3, cy},
            {cx, cy + cs}, {cx - cs * 2 / 3, cy}
        };
        SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, 255);
        SDL_RenderDrawLines(r, core, 4);
        SDL_RenderDrawLine(r, core[3].x, core[3].y, core[0].x, core[0].y);
        SDL_RenderDrawLine(r, cx - cs * 2 / 3, cy, cx + cs * 2 / 3, cy);
        // Inner cross + hot center
        SDL_SetRenderDrawColor(r, 255, 255, 255, 210);
        SDL_RenderDrawLine(r, cx, cy - cs / 2, cx, cy + cs / 2);
        SDL_RenderDrawLine(r, cx - cs / 3, cy, cx + cs / 3, cy);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        for (int i = 0; i < 8; ++i) {
            double a = i * M_PI / 4.0;
            SDL_RenderDrawPoint(r, cx + (int)(3 * std::cos(a)), cy + (int)(3 * std::sin(a)));
        }
        SDL_RenderDrawPoint(r, cx, cy);
    }

    void drawBullets(SDL_Renderer* r) const {
        for (const auto& b : bullets) {
            if (!b.active) continue;
            int bx = (int)b.x, by = (int)b.y, rad = 4;
            double hpR = (double)b.hp / 3.0;
            SDL_SetRenderDrawColor(r, 180, 60, 200, (Uint8)(160 + 95 * hpR));
            SDL_RenderDrawLine(r, bx - rad, by, bx + rad, by);
            SDL_RenderDrawLine(r, bx, by - rad, bx, by + rad);
            SDL_RenderDrawLine(r, bx - 2, by - 2, bx + 2, by + 2);
            SDL_RenderDrawLine(r, bx + 2, by - 2, bx - 2, by + 2);
            SDL_SetRenderDrawColor(r, 220, 140, 240, 200);
            SDL_RenderDrawPoint(r, bx, by);
        }
    }

    void drawHPBar(SDL_Renderer* renderer, const Font& font) const {
        if (state == INACTIVE || state == DYING) return;
        const int BAR_W = 300, BAR_H = 14;
        const int BAR_X = CENTER_X - BAR_W / 2, BAR_Y = 8;
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_Rect bg = {BAR_X, BAR_Y, BAR_W, BAR_H};
        SDL_RenderFillRect(renderer, &bg);

        int fillW = (int)((double)hp / maxHp * BAR_W);
        if (fillW > BAR_W) fillW = BAR_W;
        SDL_SetRenderDrawColor(renderer, (state == ENRAGED) ? 255 : 220, 30, 30, 255);
        SDL_Rect fill = {BAR_X, BAR_Y, fillW, BAR_H};
        SDL_RenderFillRect(renderer, &fill);

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_Rect border = {BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2};
        SDL_RenderDrawRect(renderer, &border);

        const char* name = "MOONWELL WARDEN";
        int nameLen = (int)strlen(name);
        int namePxW = nameLen * 12;  // scale 2, char width 12
        int nameX = BAR_X + (BAR_W - namePxW) / 2;
        if (nameX < 4) nameX = 4;
        for (int i = 0; name[i]; ++i) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            font.drawChar(renderer, name[i], nameX + i * 12, BAR_Y + BAR_H + 5, 2);
        }
    }
};

const double Ch2WardenBoss::BODY_RADIUS = 48.0;
