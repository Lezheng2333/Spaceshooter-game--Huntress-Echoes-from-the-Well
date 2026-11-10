#pragma once

#include "../audio.h"
#include "../bullets.h"
#include "../floating_text.h"
#include "../particles.h"
#include "../art_fx.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"
#include "../ch2/ch2_shooter_base.h"

// ============== Ch2AlienManager (Chapter 2 regular alien) ==============
struct Ch2Alien : EnemyData {
    double x, y, startX, startY, targetX, targetY;
    int fireVolleyTimer, fireBurstCount, fireBurstTimer;
    int lastHitByPulse;  // pulse wave ID already applied
    template <class Ar> void visit(Ar& ar) {
        visitBase(ar);
        ar.ioNum(x); ar.ioNum(y);
        ar.ioNum(startX); ar.ioNum(startY); ar.ioNum(targetX); ar.ioNum(targetY);
        ar.ioNum(fireVolleyTimer); ar.ioNum(fireBurstCount); ar.ioNum(fireBurstTimer);
        ar.ioNum(lastHitByPulse);
    }
};

class Ch2AlienManager : public Ch2ShooterBase {
    static const int ALIEN_HP = 10;
    static const int VOLNEY_COOLDOWN = 180;
    static const int BURST_INTERVAL = 8;
    static const int BURST_COUNT = 5;

    std::vector<Ch2Alien> aliens;

    void fireBullet(Player& pl, Ch2Alien& a) {
        double dx = pl.getX() - a.x, dy = pl.getY() - a.y;
        double d = std::sqrt(dx*dx + dy*dy); if (d < 1) d = 1;
        Ch2EnemyBullet b;
        b.x = a.x; b.y = a.y;
        b.dx = dx / d * 1.5; b.dy = dy / d * 1.5;
        b.hp = 3; b.active = true;
        bullets.push_back(b);
    }

    void spawnOne() {
        Ch2Alien a;
        a.targetX = 640 + rand() % 141;
        a.targetY = 40 + rand() % 521;
        int side = rand() % 4;
        if (side == 0)      { a.startX = 810; a.startY = 40 + rand() % 521; }        // right edge (off-screen)
        else if (side == 1) { a.startX = -10; a.startY = 40 + rand() % 521; }        // left edge (off-screen)
        else if (side == 2) { a.startX = 40 + rand() % 721; a.startY = -10; }         // top edge (full width)
        else                { a.startX = 40 + rand() % 721; a.startY = 610; }         // bottom edge (full width)
        a.x = a.startX; a.y = a.startY;
        a.hp = ALIEN_HP; a.active = true; a.entering = true; a.defeated = false;
        a.enterFrame = 0; a.enterDuration = 20 + rand() % 26;
        a.invincibleFrames = 0;
        a.fireVolleyTimer = 0; a.fireBurstCount = 0; a.fireBurstTimer = 0;
        a.lastHitByPulse = -1;
        aliens.push_back(a);
    }

public:
    Ch2AlienManager(int& hp, bool& go) : Ch2ShooterBase(hp, go) {}
    void reset() { aliens.clear(); resetBase(); }
    void clearAll() { aliens.clear(); bullets.clear(); }
    void forceSpawn() { spawnOne(); }

    // 球体碎裂后的第一波：从地面升起，橙色/灰白残留，明确是刚才弹到地上的菱形
    void forceSpawnFromShards() {
        Ch2Alien a;
        a.targetX = 540 + rand() % 221;
        a.targetY = 70 + rand() % 440;
        a.startX = 100 + rand() % 601;
        a.startY = WIN_HEIGHT + 16;
        a.x = a.startX; a.y = a.startY;
        a.hp = ALIEN_HP; a.active = true; a.entering = true; a.defeated = false;
        a.enterFrame = 0; a.enterDuration = 40 + rand() % 22;
        a.invincibleFrames = 0;
        a.fireVolleyTimer = 0; a.fireBurstCount = 0; a.fireBurstTimer = 0;
        a.lastHitByPulse = -1;
        aliens.push_back(a);
    }
    int countLiving() const {
        int cnt = 0;
        for (const auto& a : aliens) { if (!a.defeated && a.active) cnt++; }
        return cnt;
    }
    const std::vector<Ch2Alien>& getAliens() const { return aliens; }

    // 存档：全部普敌 + 敌方子弹
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(aliens);
        Ch2ShooterBase::visitBase(ar);
    }

    void update(BulletManager& bulletMgr, ParticleManager& particleMgr, AudioEngine& audio,
                int& score, Player& pl, FloatingTextManager& ftMgr, int& hitCount) {
        for (auto& a : aliens) {
            if (!a.active && !a.defeated) continue;
            if (a.defeated) continue;
            if (a.entering) {
                a.enterFrame++;
                double raw = (double)a.enterFrame / a.enterDuration;
                double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                if (raw >= 1.0) { raw = 1.0; eased = 1.0; a.entering = false; a.invincibleFrames = 3; a.fireVolleyTimer = 0; }
                a.x = a.startX + (a.targetX - a.startX) * eased;
                a.y = a.startY + (a.targetY - a.startY) * eased;
                continue;
            }
            if (a.invincibleFrames > 0) {
                a.invincibleFrames--;
                if (a.invincibleFrames <= 0) a.fireVolleyTimer = 0;
            } else {
                a.x -= 0.35;
                if (a.x < -40) {
                    a.active = false; a.defeated = true;
                    audio.sndAlienEscape();
                    continue;
                }
                if (a.fireBurstCount > 0) {
                    a.fireBurstTimer--;
                    if (a.fireBurstTimer <= 0) {
                        a.fireBurstTimer = BURST_INTERVAL;
                        fireBullet(pl, a); audio.sndEnemyShoot();
                        a.fireBurstCount--;
                        if (a.fireBurstCount <= 0) a.fireVolleyTimer = VOLNEY_COOLDOWN;
                    }
                } else {
                    a.fireVolleyTimer--;
                    if (a.fireVolleyTimer <= 0) { a.fireBurstCount = BURST_COUNT; a.fireBurstTimer = 0; }
                }
            }
            // Player bullets vs alien collision
            if (!a.entering && a.invincibleFrames <= 0) {
                for (auto& b : bulletMgr.all()) {
                    if (!b.active || !b.canDamage) continue;
                    double dx = b.x - a.x, dy = b.y - a.y;
                    if (dx*dx + dy*dy < 22.0*22.0) {
                        b.active = false; b.canDamage = false; a.hp--; hitCount += 2;
                        particleMgr.spawnExplosion(b.x, b.y, 3); audio.sndHit();
                        if (a.hp <= 0) {
                            a.defeated = true; a.active = false;
                            particleMgr.spawnExplosion(a.x, a.y, 20);
                            particleMgr.spawnExplosion(a.x-8, a.y-4, 12);
                            particleMgr.spawnExplosion(a.x+8, a.y+4, 12);
                            audio.sndExplosionSmall(); score += 1;
                        }
                        break;
                    }
                }
            }
        }
        // Remove defeated/inactive aliens
        aliens.erase(std::remove_if(aliens.begin(), aliens.end(),
            [](const Ch2Alien& a){ return !a.active && !a.defeated; }), aliens.end());
        Ch2ShooterBase::updateBullets(bulletMgr, particleMgr, audio, pl, ftMgr, &hitCount);
    }

    void drawEnemy(SDL_Renderer* r) const {
        ArtFX::BlendGuard guard(r);
        for (const auto& a : aliens) {
            if (!a.active) continue;
            int ex = (int)a.x, ey = (int)a.y, sz = 13;
            int rCol, gCol, bCol;
            bool shardEnter = a.entering && a.startY > WIN_HEIGHT - 20;
            if (a.entering || a.invincibleFrames > 0) {
                if (shardEnter) { rCol=238; gCol=164; bCol=82; }
                else { rCol=100; gCol=180; bCol=255; }
            } else computeHPColor((double)a.hp / ALIEN_HP, rCol, gCol, bCol);
            if (a.entering && a.enterFrame > 3) {
                int tr = shardEnter ? 238 : 100;
                int tg = shardEnter ? 164 : 180;
                int tb = shardEnter ? 82 : 255;
                for (int k = 1; k <= 4; ++k) {
                    double pRaw = (double)(a.enterFrame - k*3) / a.enterDuration;
                    if (pRaw < 0) continue;
                    double pE = 1.0 - std::pow(1.0 - pRaw, 3.0);
                    int px = (int)(a.startX + (a.targetX - a.startX) * pE);
                    int py = (int)(a.startY + (a.targetY - a.startY) * pE);
                    ArtFX::starGlint(r, px, py, k <= 2 ? 1 : 0,
                                     tr, tg, tb, 150 - k*26);
                }
            }
            int hw = sz * 2 / 3;
            SDL_Point hull[6] = {
                {ex, ey - sz},
                {ex + hw, ey - sz / 4},
                {ex + hw / 2, ey + sz * 2 / 3},
                {ex, ey + sz},
                {ex - hw / 2, ey + sz * 2 / 3},
                {ex - hw, ey - sz / 4}
            };
            ArtFX::fillPoly(r, hull, 6, rCol / 5 + 8, gCol / 6 + 7, bCol / 8 + 7, 225);
            for (int i = 0; i < 6; ++i) {
                SDL_Point& p0 = hull[i]; SDL_Point& p1 = hull[(i + 1) % 6];
                ArtFX::glowLine(r, p0.x, p0.y, p1.x, p1.y, rCol, gCol, bCol,
                                (i == 0 || i == 5) ? 235 : 155, 1);
            }
            // 中心能量孔 + 横向翼梁。
            ArtFX::glowLine(r, ex - hw, ey - sz/4, ex + hw, ey - sz/4, rCol, gCol, bCol, 100, 1);
            SDL_Point core[4] = {{ex, ey - sz/3}, {ex + 3, ey}, {ex, ey + sz/3}, {ex - 3, ey}};
            ArtFX::fillPoly(r, core, 4, 255, 220, 168, 215);
            // 朝玩家运动的侧滚体感：右侧两条短推进尾迹。
            if (!a.entering && a.invincibleFrames <= 0) {
                ArtFX::glowLine(r, ex + hw + 2, ey - 3, ex + hw + 8, ey - 4, rCol, gCol, bCol, 85, 1);
                ArtFX::glowLine(r, ex + hw + 2, ey + 4, ex + hw + 7, ey + 5, rCol, gCol, bCol, 65, 1);
            }
            if (a.invincibleFrames > 0) {
                SDL_Point ring[8];
                int rr = sz + 5;
                for (int i = 0; i < 8; ++i) {
                    double ang = 2.0 * M_PI * i / 8.0;
                    ring[i] = {ex + (int)(rr * std::cos(ang)), ey + (int)(rr * std::sin(ang))};
                }
                for (int i = 0; i < 8; ++i)
                    ArtFX::glowLine(r, ring[i].x, ring[i].y, ring[(i+1)%8].x, ring[(i+1)%8].y,
                                    92, 176, 255, 80, 1);
            }
            // 球体残片升起的第一波保留橙色暖光，以衔接上一段演出。
            if (shardEnter) {
                SDL_SetRenderDrawColor(r, 255, 166, 78, 125);
                SDL_RenderDrawPoint(r, ex - 2, ey - 1);
                SDL_RenderDrawPoint(r, ex + 2, ey + 1);
            }
        }
    }
};
