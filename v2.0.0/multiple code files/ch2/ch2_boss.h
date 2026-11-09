#pragma once

#include "../audio.h"
#include "../constants.h"
#include "../floating_text.h"
#include "../font.h"
#include "../art_fx.h"
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

    void takeDamage(int dmg, AudioEngine* audio = nullptr) {
        if (state != FIGHT && state != ENRAGED) return;
        hp -= dmg;
        flashTimer = 5;
        if (hp <= ENRAGE_HP && state == FIGHT) {
            state = ENRAGED; pattern = 0; patternTimer = 0; volleyCount = 0;
            if (audio) audio->sndBossEnrage();
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
                    audio.sndWardenBurst();
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
                    audio.sndEnemyShoot();
                    volleyCount++;
                    if (volleyCount >= 3) {
                        volleyCount = 0; pattern = 2; patternTimer = 0;
                        audio.sndWardenSpiral();
                    }
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
                if (patternTimer >= cycle) {
                    pattern = 0; patternTimer = 0;
                    audio.sndWardenAim();
                }
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
        ArtFX::BlendGuard guard(r);
        int cx = (int)x, cy = (int)y;
        bool enraged = (state == ENRAGED);

        int br, bg, bb;
        if (enraged)      { br = 255; bg = 106; bb = 118; }
        else              { br = 122; bg = 214; bb = 255; }
        if (flashTimer > 0) { br = 255; bg = 238; bb = 218; }
        if (state == DYING) {
            double fade = 1.0 - deathTimer / 200.0;
            if (fade < 0.0) fade = 0.0;
            br = (int)(255 * fade); bg = (int)(184 * fade); bb = (int)(142 * fade);
        }

        // 外层脉冲圆环：战斗状态越激烈，环越亮。
        double pulse = 0.5 + 0.5 * std::sin(timer * 0.075);
        for (int ring = 0; ring < 3; ++ring) {
            int rr = 62 + ring * 17;
            int alpha = (int)((enraged ? 76 : 58) * (0.65 + 0.35 * pulse)) / (ring + 1);
            int px = cx + rr, py = cy;
            for (int i = 1; i <= 56; ++i) {
                double a = 2.0 * M_PI * i / 56.0 + spinAngle * (ring == 1 ? -0.7 : 0.45);
                int nx = cx + (int)(rr * std::cos(a));
                int ny = cy + (int)(rr * std::sin(a));
                if (i % 5 != 0 && alpha > 0) {
                    SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, (Uint8)alpha);
                    SDL_RenderDrawLine(r, px, py, nx, ny);
                }
                px = nx; py = ny;
            }
        }

        // ---- 4 rotating blades (填充成能量刃，不再是单线) ----
        for (int k = 0; k < 4; ++k) {
            double a = spinAngle * 1.5 + k * M_PI / 2.0;
            int bx0 = cx + (int)(std::cos(a) * 28), by0 = cy + (int)(std::sin(a) * 28);
            int bx1 = cx + (int)(std::cos(a) * 88), by1 = cy + (int)(std::sin(a) * 88);
            int nx = (int)(-std::sin(a)), ny = (int)(std::cos(a));
            SDL_Point blade[4] = {{bx0 + nx * 3, by0 + ny * 3}, {bx1 + nx, by1 + ny},
                                  {bx1 - nx, by1 - ny}, {bx0 - nx * 3, by0 - ny * 3}};
            ArtFX::fillPoly(r, blade, 4, br / 4 + 6, bg / 4 + 8, bb / 4 + 10, 180);
            ArtFX::glowLine(r, bx0, by0, bx1, by1, br, bg, bb, 155, 1);
            int tx = cx + (int)(std::cos(a) * 94), ty = cy + (int)(std::sin(a) * 94);
            ArtFX::starGlint(r, tx, ty, 2, br, bg, bb, 170);
        }

        // ---- 双层轨道碎片 ----
        for (int i = 0; i < 12; ++i) {
            bool outer = (i % 2 == 0);
            double rr = outer ? 72.0 : 48.0;
            double a = spinAngle * (outer ? 1.0 : -1.5) + i * M_PI / 6.0;
            int ox = cx + (int)(rr * std::cos(a));
            int oy = cy + (int)(rr * 0.62 * std::sin(a));
            int ss = outer ? 7 : 5;
            SDL_Point sh[4] = {{ox, oy - ss}, {ox + ss * 2 / 3, oy},
                               {ox, oy + ss}, {ox - ss * 2 / 3, oy}};
            ArtFX::fillPoly(r, sh, 4, br / 5 + 6, bg / 5 + 8, bb / 5 + 9, 220);
            for (int q = 0; q < 4; ++q)
                ArtFX::glowLine(r, sh[q].x, sh[q].y, sh[(q+1)%4].x, sh[(q+1)%4].y,
                                br, bg, bb, 160, 1);
            // 能量系绳
            int tx = cx + (int)(42.0 * std::cos(a));
            int ty = cy + (int)(26.0 * std::sin(a));
            SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, 40);
            SDL_RenderDrawLine(r, tx, ty, ox, oy);
        }

        // ---- 主体：八边形能量躯干 + 中央核心眼 ----
        const int cs = 42;
        SDL_Point hull[8] = {
            {cx, cy - cs},
            {cx + cs * 2 / 3, cy - cs * 2 / 3},
            {cx + cs, cy},
            {cx + cs * 2 / 3, cy + cs * 2 / 3},
            {cx, cy + cs},
            {cx - cs * 2 / 3, cy + cs * 2 / 3},
            {cx - cs, cy},
            {cx - cs * 2 / 3, cy - cs * 2 / 3}
        };
        ArtFX::fillPoly(r, hull, 8, br / 5 + 4, bg / 5 + 7, bb / 5 + 9, 238);
        for (int i = 0; i < 8; ++i)
            ArtFX::glowLine(r, hull[i].x, hull[i].y, hull[(i+1)%8].x, hull[(i+1)%8].y,
                            br, bg, bb, 205, 1);
        // 内部支撑肋
        for (int i = 0; i < 4; ++i) {
            double a = i * M_PI / 2.0 + spinAngle * 0.2;
            int px = cx + (int)(cs * 0.78 * std::cos(a));
            int py = cy + (int)(cs * 0.78 * std::sin(a));
            ArtFX::glowLine(r, cx, cy, px, py, br, bg, bb, 55, 1);
        }
        // 核心光环
        int coreR = 22 + (int)(pulse * (enraged ? 4 : 2));
        for (int layer = 4; layer >= 0; --layer) {
            int rr = coreR + 2 + layer * 4;
            int alpha = 14 + layer * 24;
            SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)bg, (Uint8)bb, (Uint8)alpha);
            int px = cx + rr, py = cy;
            for (int i = 1; i <= 28; ++i) {
                double a = 2.0 * M_PI * i / 28.0;
                int nx = cx + (int)(rr * std::cos(a));
                int ny = cy + (int)(rr * std::sin(a));
                SDL_RenderDrawLine(r, px, py, nx, ny);
                px = nx; py = ny;
            }
        }
        // 实心眼：外暗环 + 热瞳孔 + 竖直缝。
        for (int yy = -coreR; yy <= coreR; ++yy) {
            int hw = (int)std::sqrt((double)std::max(0, coreR * coreR - yy * yy));
            ArtFX::fillPoly(r, (SDL_Point[]){{cx - hw, cy + yy}, {cx + hw, cy + yy},
                                             {cx + hw, cy + yy + 1}, {cx - hw, cy + yy + 1}}, 4,
                            br / 3, bg / 3, bb / 3, 200);
        }
        SDL_SetRenderDrawColor(r, (Uint8)br, (Uint8)(bg + 24), (Uint8)bb, 240);
        SDL_Rect iris = {cx - 7, cy - 14, 14, 28};
        SDL_RenderFillRect(r, &iris);
        SDL_SetRenderDrawColor(r, 244, 255, 255, 220);
        SDL_RenderDrawLine(r, cx, cy - 11, cx, cy + 11);
        ArtFX::starGlint(r, cx, cy, 3, 235, 255, 255, 220);

        if (state == DYING) {
            for (int k = 0; k < 4; ++k) {
                int px = cx + (int)(std::cos(deathTimer * 0.13 + k * 1.7) * (22 + k * 9));
                int py = cy + (int)(std::sin(deathTimer * 0.17 + k * 1.7) * (15 + k * 7));
                ArtFX::starGlint(r, px, py, 1, 255, 160, 105, 180);
            }
        }
    }

    void drawBullets(SDL_Renderer* r) const {
        ArtFX::BlendGuard guard(r);
        for (const auto& b : bullets) {
            if (!b.active) continue;
            int bx = (int)b.x, by = (int)b.y;
            int tx = (int)(b.x - b.dx * 4.0);
            int ty = (int)(b.y - b.dy * 4.0);
            ArtFX::glowLine(r, tx, ty, bx, by, 164, 67, 212, 80, 1);
            // 菱形能量弹 + 亮核。
            SDL_Point orb[4] = {{bx, by - 5}, {bx + 4, by}, {bx, by + 5}, {bx - 4, by}};
            ArtFX::fillPoly(r, orb, 4, 118, 32, 156, 210);
            for (int i = 0; i < 4; ++i)
                ArtFX::glowLine(r, orb[i].x, orb[i].y, orb[(i+1)%4].x, orb[(i+1)%4].y,
                                219, 126, 245, 200, 1);
            SDL_SetRenderDrawColor(r, 255, 230, 255, 245);
            SDL_RenderDrawPoint(r, bx, by);
        }
    }

    void drawHPBar(SDL_Renderer* renderer, const Font& font) const {
        if (state == INACTIVE || state == DYING) return;
        ArtFX::BlendGuard guard(renderer);
        const int BAR_W = 300, BAR_H = 14;
        const int BAR_X = CENTER_X - BAR_W / 2, BAR_Y = 8;
        // 轨道 + 底框
        SDL_SetRenderDrawColor(renderer, 11, 10, 15, 235);
        SDL_Rect bg = {BAR_X - 2, BAR_Y - 2, BAR_W + 4, BAR_H + 4};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 84, 35, 58, 220);
        SDL_Rect border = {BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2};
        SDL_RenderDrawRect(renderer, &border);
        int fillW = (int)((double)hp / maxHp * BAR_W);
        if (fillW > BAR_W) fillW = BAR_W; if (fillW < 0) fillW = 0;
        if (fillW > 0) {
            bool enraged = state == ENRAGED;
            SDL_SetRenderDrawColor(renderer, enraged ? 255 : 188, enraged ? 58 : 35, 74, 255);
            SDL_Rect fill = {BAR_X, BAR_Y, fillW, BAR_H};
            SDL_RenderFillRect(renderer, &fill);
            SDL_SetRenderDrawColor(renderer, 255, enraged ? 128 : 90, 112, 180);
            SDL_Rect hi = {BAR_X, BAR_Y, fillW, 1};
            SDL_RenderFillRect(renderer, &hi);
            SDL_SetRenderDrawColor(renderer, 92, 19, 34, 145);
            for (int i = 1; i < 14; ++i)
                SDL_RenderDrawLine(renderer, BAR_X + BAR_W * i / 14, BAR_Y,
                                   BAR_X + BAR_W * i / 14, BAR_Y + BAR_H - 1);
        }
        const char* name = (state == ENRAGED) ? "MOONWELL WARDEN // ENRAGED" : "MOONWELL WARDEN";
        int nameLen = (int)strlen(name);
        int namePxW = nameLen * 12;
        int nameX = BAR_X + (BAR_W - namePxW) / 2;
        if (nameX < 4) nameX = 4;
        font.drawStringColor(renderer, name, nameX, BAR_Y + BAR_H + 5, 2,
                             state == ENRAGED ? 255 : 212,
                             state == ENRAGED ? 146 : 231,
                             state == ENRAGED ? 144 : 239, 240);
    }
};

const double Ch2WardenBoss::BODY_RADIUS = 48.0;
