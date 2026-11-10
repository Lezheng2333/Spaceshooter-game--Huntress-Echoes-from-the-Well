#pragma once

#include "../audio.h"
#include "../bullets.h"
#include "../constants.h"
#include "../floating_text.h"
#include "../particles.h"
#include "../player.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch2ShooterBase (shared enemy bullet system) ==============
class Ch2ShooterBase {
protected:
    std::vector<Ch2EnemyBullet> bullets;
    int& hpRef;
    bool& goRef;

    void updateBullets(BulletManager& bulletMgr, ParticleManager& particleMgr, AudioEngine& audio,
                       Player& pl, FloatingTextManager& ftMgr, int* energyHits = nullptr) {
        for (auto& b : bullets) {
            if (!b.active) continue;
            b.x += b.dx; b.y += b.dy;
            if (b.x < -20 || b.x > WIN_WIDTH+20 || b.y < -20 || b.y > WIN_HEIGHT+20) b.active = false;
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Ch2EnemyBullet& b){ return !b.active; }), bullets.end());
        // Player bullets vs enemy bullets
        for (auto& b : bulletMgr.all()) {
            if (!b.active || !b.canDamage) continue;
            for (auto& eb : bullets) {
                if (!eb.active) continue;
                if (std::abs(b.x-eb.x) < 10 && std::abs(b.y-eb.y) < 10) {
                    b.active = false; b.canDamage = false; eb.hp--;
                    if (eb.hp <= 0) {
                        eb.active = false;
                        particleMgr.spawnExplosion(eb.x, eb.y, 4);
                        audio.sndCrystalCrush();
                        if (energyHits) (*energyHits) += 1;   // destroying an enemy bullet grants 1 energy
                    }
                    break;
                }
            }
        }
        // Player vs enemy bullets (skip if player invincible)
        if (pl.getInvFrames() <= 0) {
            for (auto& eb : bullets) {
                if (!eb.active) continue;
                double dx = pl.getX() - eb.x, dy = pl.getY() - eb.y;
                if (dx*dx + dy*dy < 16.0*16.0) {
                    eb.active = false; hpRef--;
                    pl.setInvFrames(60);  // 1 second invincibility
                    particleMgr.spawnExplosion(eb.x, eb.y, 8); audio.sndPlayerHit();
                    ftMgr.spawn((float)pl.getX()+1, (float)(pl.getY()-19), "HP -1", 0,0,0);
                    ftMgr.spawn((float)pl.getX(), (float)(pl.getY()-20), "HP -1", 255,50,50);
                    if (hpRef <= 0) { goRef = true; hpRef = 0; }
                    break;  // only one hit per frame
                }
            }
        }
    }

public:
    Ch2ShooterBase(int& hp, bool& go) : hpRef(hp), goRef(go) {}
    bool isGameOver() const { return goRef; }
    int getPlayerHP() const { return hpRef; }
    const std::vector<Ch2EnemyBullet>& getBullets() const { return bullets; }
    void resetBase() { bullets.clear(); hpRef = 3; goRef = false; }
    void clearBullets() { bullets.clear(); }

    // 存档：敌方子弹（hpRef/goRef 是 Game 的引用，由 Game 层单独存）
    template <class Ar> void visitBase(Ar& ar) { ar.ioVecObj(bullets); }

    static void computeHPColor(double hpR, int& r, int& g, int& b) {
        r = 255; g = (int)(150*hpR + 80*(1-hpR)); b = (int)(100*hpR + 30*(1-hpR));
    }

    void drawBullets(SDL_Renderer* r) const {
        for (const auto& b : bullets) {
            if (!b.active) continue;
            int bx=(int)b.x, by=(int)b.y, rad=4;
            double hpR=(double)b.hp/3.0;
            SDL_SetRenderDrawColor(r, 180,60,200,(Uint8)(160+95*hpR));
            SDL_RenderDrawLine(r,bx-rad,by,bx+rad,by);
            SDL_RenderDrawLine(r,bx,by-rad,bx,by+rad);
            SDL_RenderDrawLine(r,bx-2,by-2,bx+2,by+2);
            SDL_RenderDrawLine(r,bx+2,by-2,bx-2,by+2);
            SDL_SetRenderDrawColor(r, 220,140,240,200);
            SDL_RenderDrawPoint(r,bx,by);
        }
    }

};


// ============== NightElfEnergy [DORMANT — NightElf 激活后联动] ==============
class NightElfEnergy {
public:
    static const int MAX_ENERGY = 100;      // 直接命中 +2，上限翻倍，纯命中充能时间保持不变
    static const int HIT_WINDOW = 30;       // 0.5s at 60fps without hit → decay
    static const int TRIPLE_DURATION = 900;  // 15s at 60fps
    static const int COUNTDOWN_START = 180;  // last 3s countdown beeps

    NightElfEnergy() : energy(0), lastHitFrame(-999), frameCounter(0),
        tripleActive(false), tripleJustEntered(false), tripleTimer(0) {}

    void reset() {
        energy = 0; lastHitFrame = -999; frameCounter = 0;
        tripleActive = false; tripleTimer = 0; tripleJustEntered = false;
    }

    void update(int hitsThisFrame) {
        frameCounter++;
        tripleJustEntered = false;
        if (tripleActive) {
            tripleTimer--;
            if (tripleTimer <= 0) {
                // Rapid decay back to 0 over 1 second
                energy -= MAX_ENERGY / 60.0;
                if (energy <= 0) { energy = 0; tripleActive = false; tripleTimer = 0; }
            }
            return;
        }
        if (hitsThisFrame > 0) {
            energy += hitsThisFrame;
            if (energy >= MAX_ENERGY) {
                energy = MAX_ENERGY;
                tripleActive = true; tripleTimer = TRIPLE_DURATION;
                tripleJustEntered = true;
            }
            lastHitFrame = frameCounter;
        } else if (frameCounter - lastHitFrame > HIT_WINDOW && energy > 0) {
            // Full bar decays in 5 seconds at the new 100-point cap.
            energy -= MAX_ENERGY / 300.0; // MAX_ENERGY / (5*60)
            if (energy < 0) energy = 0;
        }
    }

    void onHit() { if (!tripleActive) { energy += 2.0; if (energy >= MAX_ENERGY) energy = MAX_ENERGY; } lastHitFrame = frameCounter; }
    void checkTripleTrigger() {
        if (!tripleActive && energy >= MAX_ENERGY) {
            tripleActive = true; tripleTimer = TRIPLE_DURATION; tripleJustEntered = true;
        }
    }

    bool isTripleActive() const { return tripleActive; }
    bool justEnteredTriple() const { return tripleJustEntered; }
    int getTripleTimer() const { return tripleTimer; }
    float getFill() const { return (float)(energy / MAX_ENERGY); }
    // 节点存档生成/调试用：直接把能量条设到指定值
    void setEnergy(double e) { energy = e < 0.0 ? 0.0 : (e > MAX_ENERGY ? (double)MAX_ENERGY : e); }

    // 存档：白色能量条（能量值/命中窗口计时/三连发剩余时间）
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(energy); ar.ioNum(lastHitFrame); ar.ioNum(frameCounter);
        ar.ioBool(tripleActive); ar.ioBool(tripleJustEntered); ar.ioNum(tripleTimer);
    }
    bool isDecaying() const { return !tripleActive && energy > 0 && frameCounter - lastHitFrame > HIT_WINDOW; }
    bool isCharging() const { return !tripleActive && energy > 0 && frameCounter - lastHitFrame <= HIT_WINDOW; }

private:
    double energy;
    int lastHitFrame, frameCounter;
    bool tripleActive, tripleJustEntered;
    int tripleTimer;
};