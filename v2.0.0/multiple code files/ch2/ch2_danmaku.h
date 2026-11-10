#pragma once

#include "../audio.h"
#include "../bullets.h"
#include "../constants.h"
#include "../floating_text.h"
#include "../particles.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"
#include "../ch2/ch2_shooter_base.h"

// ============== Ch2DanmakuManager (Chapter 2) ==============
class Ch2DanmakuManager : public Ch2ShooterBase {
    std::vector<Ch2DanmakuEnemy> enemies;

    void computeLegs(Ch2DanmakuEnemy& e) {
        double dx = e.x - 400, dy = e.y + 60;
        double dist = std::sqrt(dx*dx + dy*dy);
        double scale = 0.55 + 0.45 * (dist / 340.0);
        if (scale < 0.50) scale = 0.50; if (scale > 1.25) scale = 1.25;
        int leg = (int)(16.0 * scale); if (leg < 6) leg = 6;
        double depthFactor = (e.x - 200) / 600.0;
        if (depthFactor < 0.1) depthFactor = 0.1; if (depthFactor > 0.7) depthFactor = 0.7;
        e.leg = leg;
        e.farLeg  = (int)(leg * (1.0 - depthFactor * 0.35));
        e.nearLeg = (int)(leg * (1.0 + depthFactor * 0.15));
        e.vpLean  = (int)(leg * depthFactor * 0.25);
    }

public:
    Ch2DanmakuManager(int& hp, bool& go) : Ch2ShooterBase(hp, go) {}
    void reset() { enemies.clear(); resetBase(); }
    void clearAll() { enemies.clear(); bullets.clear(); }

    // 存档：全部弹幕敌人 + 敌方子弹（含螺旋角/发射计时，读档后弹幕无缝继续）
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(enemies);
        Ch2ShooterBase::visitBase(ar);
    }

    void spawnEnemy() {
        Ch2DanmakuEnemy e;
        e.startX = WIN_WIDTH + 40;
        e.startY = (rand() % 2) ? -40 : WIN_HEIGHT + 40;  // top-right or bottom-right corner

        // Find non-overlapping target position (up to 5 retries)
        int tries = 0;
        bool blocked;
        do {
            e.targetX = 400 + rand() % 231;    // 400-630
            e.targetY = 200 + rand() % 161;    // 200-360, vertical spread
            blocked = false;
            for (const auto& ex : enemies) {
                if (!ex.active || ex.defeated) continue;
                double dx = e.targetX - ex.baseX;
                double dy = e.targetY - ex.baseY;
                if (dx*dx + dy*dy < 80.0*80.0) { blocked = true; break; }
            }
            tries++;
        } while (blocked && tries < 5);

        e.x = e.startX; e.y = e.startY;
        e.baseX = e.targetX; e.baseY = e.targetY;
        e.movePhase = 0;
        e.hp = 50; e.maxHp = 50;
        e.active = true; e.entering = true; e.defeated = false;
        e.enterFrame = 0; e.enterDuration = 35;
        e.invincibleFrames = 0;
        e.vulnTimer = 0;
        e.fireTimer = 0; e.fireInterval = 6;
        e.fireAngle = 0;
        e.defeatTimer = 0;
        e.lastHitByPulse = -1;
        enemies.push_back(e);
    }

    void update(BulletManager& bulletMgr, ParticleManager& particleMgr, AudioEngine& audio,
                int& score, Player& pl, FloatingTextManager& ftMgr, int& hitCount) {
        for (auto& e : enemies) {
            if (e.defeated) {
                if (e.defeatTimer > 0) {
                    e.defeatTimer--;
                    if (e.defeatTimer == 0) {
                        particleMgr.spawnExplosion(e.x, e.y, 30);
                        particleMgr.spawnExplosion(e.x-14, e.y-6, 18);
                        particleMgr.spawnExplosion(e.x+14, e.y+6, 18);
                        particleMgr.spawnExplosion(e.x, e.y-10, 12);
                        particleMgr.spawnExplosion(e.x+5, e.y+12, 12);
                        audio.sndExplosionBig();
                    }
                }
                continue;
            }
            if (!e.active) continue;

            if (e.entering) {
                e.enterFrame++;
                double raw = (double)e.enterFrame / e.enterDuration;
                double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                e.x = e.startX + (e.targetX - e.startX) * eased;
                e.y = e.startY + (e.targetY - e.startY) * eased;
                computeLegs(e);  // needed for draw during entrance
                if (e.enterFrame >= e.enterDuration) {
                    e.entering = false;
                    e.invincibleFrames = 300;
                    e.fireTimer = 0; e.fireAngle = 0;
                    e.baseX = e.targetX; e.baseY = e.targetY;
                    e.movePhase = 0;
                    e.moveSpeed = 0.005 + (rand() % 40) / 10000.0;  // 0.005~0.009
                }
                continue;
            }

            e.movePhase += e.moveSpeed;
            double osc = std::sin(e.movePhase) * 55.0;
            double vdx = e.baseX - 400, vdy = e.baseY + 60;
            double vlen = std::sqrt(vdx*vdx + vdy*vdy);
            if (vlen < 1.0) vlen = 1.0;
            double nx = vdx / vlen, ny = vdy / vlen;
            e.x = e.baseX + nx * osc;
            e.y = e.baseY + ny * osc;

            if (e.invincibleFrames > 0) {
                e.invincibleFrames--;
                e.fireTimer++;
                if (e.fireTimer >= 6) {
                    e.fireTimer = 0;
                    e.fireAngle += 0.35;
                    for (int s = 0; s < 2; ++s) {
                        Ch2EnemyBullet db;
                        db.x = e.x; db.y = e.y;
                        double a = e.fireAngle + s * M_PI;
                        db.dx = std::cos(a) * 1.5;
                        db.dy = std::sin(a) * 1.5;
                        db.hp = 3; db.active = true;
                        bullets.push_back(db);
                    }
                }
                if (e.invincibleFrames <= 0) e.vulnTimer = 180;
            } else if (e.vulnTimer > 0) {
                e.vulnTimer--;
                if (e.vulnTimer <= 0 && e.hp > 0) {
                    e.invincibleFrames = 300; e.fireTimer = 0;
                }
            }

            computeLegs(e);
            // Player bullets vs danmaku enemy AABB
            if (!e.entering && e.invincibleFrames <= 0 && e.vulnTimer > 0) {
                int fl = e.farLeg, nl = e.nearLeg, vl = e.vpLean;
                int ex = (int)e.x, ey = (int)e.y;
                for (auto& b : bulletMgr.all()) {
                    if (!b.active || !b.canDamage) continue;
                    int bx = (int)b.x, by = (int)b.y;
                    if (bx >= ex - fl && bx <= ex + nl && by >= ey - nl + vl && by <= ey + nl) {
                        b.active = false; b.canDamage = false; e.hp--; hitCount += 2;
                        particleMgr.spawnExplosion(b.x, b.y, 3); audio.sndHit();
                        if (e.hp <= 0) {
                            e.defeated = true; e.active = false;
                            e.defeatTimer = 70; score += 5;
                        }
                        break;
                    }
                }
            }
        }
        // Remove fully dead enemies (defeated and timer done)
        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
            [](const Ch2DanmakuEnemy& e){ return e.defeated && e.defeatTimer <= 0; }), enemies.end());
        Ch2ShooterBase::updateBullets(bulletMgr, particleMgr, audio, pl, ftMgr, &hitCount);
    }

    void drawEnemy(SDL_Renderer* r) const {
        for (const auto& e : enemies) {
            if (!e.active && !e.defeated) continue;
            if (e.defeated && e.defeatTimer <= 0) continue;
            int ex = (int)e.x, ey = (int)e.y;
            if (e.defeated && e.defeatTimer > 0) {
                if ((e.defeatTimer / 3) % 2) continue;
                int shake = (e.defeatTimer > 35) ? 6 : 3;
                ex += (rand() % (shake*2+1)) - shake;
                ey += (rand() % (shake*2+1)) - shake;
            }
            if (e.entering && e.enterFrame > 3) {
                for (int k = 1; k <= 5; ++k) {
                    double pastRaw = (double)(e.enterFrame - k * 2) / e.enterDuration;
                    if (pastRaw < 0) continue;
                    double pE = 1.0 - std::pow(1.0 - pastRaw, 3.0);
                    int px = (int)(e.startX + (e.targetX - e.startX) * pE);
                    int py = (int)(e.startY + (e.targetY - e.startY) * pE);
                    int alpha = 160 - k * 27;
                    SDL_SetRenderDrawColor(r, 100, 180, 255, (Uint8)alpha);
                    SDL_RenderDrawPoint(r, px, py);
                    SDL_RenderDrawPoint(r, px-1, py);
                    SDL_RenderDrawPoint(r, px+1, py);
                    SDL_RenderDrawPoint(r, px, py-1);
                    SDL_RenderDrawPoint(r, px, py+1);
                }
            }
            int rCol, gCol, bCol;
            if (e.entering || e.invincibleFrames > 0) { rCol=100; gCol=180; bCol=255; }
            else computeHPColor((double)e.hp / e.maxHp, rCol, gCol, bCol);
            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, 255);
            int fl = e.farLeg, nl = e.nearLeg, vl = e.vpLean;
            SDL_Point lt[3] = {{ex - vl, ey}, {ex - fl, ey - fl + vl}, {ex - fl, ey + fl}};
            SDL_RenderDrawLines(r, lt, 3);
            SDL_RenderDrawLine(r, lt[2].x, lt[2].y, lt[0].x, lt[0].y);
            SDL_Point rt[3] = {{ex - vl, ey}, {ex + nl, ey - nl + vl}, {ex + nl, ey + nl}};
            SDL_RenderDrawLines(r, rt, 3);
            SDL_RenderDrawLine(r, rt[2].x, rt[2].y, rt[0].x, rt[0].y);
            if (!e.entering && e.invincibleFrames <= 0) {
                int barW=50, barH=4, barX=ex-barW/2, barY=ey-e.leg-14;
                SDL_SetRenderDrawColor(r, 30,30,30,255);
                SDL_Rect bg={barX,barY,barW,barH}; SDL_RenderFillRect(r,&bg);
                int hpW=(int)((double)e.hp/e.maxHp*barW);
                SDL_SetRenderDrawColor(r, 220,30,30,255);
                SDL_Rect hpR={barX,barY,hpW,barH}; SDL_RenderFillRect(r,&hpR);
            }
        }
    }

    const std::vector<Ch2DanmakuEnemy>& getEnemies() const { return enemies; }
};
