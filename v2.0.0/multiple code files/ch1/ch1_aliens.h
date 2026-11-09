#pragma once

#include "../audio.h"
#include "../constants.h"
#include "../particles.h"
#include "../art_fx.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch1AlienManager ==============
class Ch1AlienManager {
    std::vector<Ch1Alien> aliens;
    int spawnTimer;
    double baseSpeed;
    int spawnInterval;
    static const int ALIEN_MIN_HP = 3;
    static const int ALIEN_MAX_HP = 5;
    int hpBonus;  // per-chapter override for alien HP

public:
    Ch1AlienManager() : spawnTimer(0), baseSpeed(1.75), spawnInterval(110), hpBonus(0) {}

    void reset() {
        aliens.clear(); spawnTimer = 0; baseSpeed = 1.75;
        spawnInterval = 110; hpBonus = 0;
    }

    void applyChapterConfig(const ChapterConfig& cfg) {
        baseSpeed = cfg.baseAlienSpeed;
        spawnInterval = cfg.baseSpawnInterval;
        hpBonus = 0;
    }

    void updateMovementParams(int difficultyTimer) {
        if (difficultyTimer % 720 == 0) baseSpeed += 0.4;
    }

    int currentSpawnInterval(int score, int difficultyTimer) {
        int base = 110 - score * 95 / 150;
        if (base < 12) base = 12;
        base -= (difficultyTimer / 720) * 5;
        if (base < 8) base = 8;
        return base;
    }

    void spawnAlien(int score) {
        Ch1Alien a;
        a.targetT = 0.15 + (rand() % 700) / 1000.0;
        a.enterFrame = 0;
        a.enterDuration = 20 + rand() % 25;
        a.entering = true;
        a.enterFromBoss = false;
        a.invincibleFrames = -1;
        a.lastHitBySW = -1; a.lastHealHit = -1;
        a.beingAbsorbed = false;
        a.absorbFrame = 0; a.absorbDuration = 0;
        a.absorbStartX = 0; a.absorbStartY = 0;
        a.alienType = 0;
        int hpBonus = (score >= 60) ? (score / 30 - 1) * 2 : 0;
        a.hp = ALIEN_MIN_HP + hpBonus + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
        a.maxHp = a.hp;
        a.active = true;

        // Entry type
        int entryType = rand() % 3;
        if (entryType < 2) {
            a.enterFromTop = false;
            a.enterStartY = 0; a.enterTargetY = 0;
            a.y = 30.0 + (rand() % 60);
            a.startT = (entryType == 0) ? -0.15 : 1.15;
            a.t = a.startT;
        } else {
            a.enterFromTop = true;
            a.enterStartY = -40.0 - (rand() % 30);
            a.enterTargetY = 30.0 + (rand() % 60);
            a.y = a.enterStartY;
            a.startT = a.targetT;
            a.t = a.targetT;
        }
        aliens.push_back(a);
    }

    void spawnAlienFromBoss(double bossX, double bossY, int score) {
        Ch1Alien a;
        a.startT = (bossX - perspLeft(bossY)) / perspWidth(bossY);
        if (a.startT < 0.1) a.startT = 0.1;
        if (a.startT > 0.9) a.startT = 0.9;
        a.targetT = 0.08 + (rand() % 840) / 1000.0;
        a.t = a.startT;
        a.enterStartY = bossY;
        a.enterTargetY = bossY + 50.0 + (rand() % 180);
        a.y = bossY;
        a.enterFromBoss = true;
        a.enterFromTop = false;
        a.entering = true;
        a.enterFrame = 0;
        a.enterDuration = 16 + rand() % 18;
        a.invincibleFrames = -1;
        a.lastHitBySW = -1; a.lastHealHit = -1;
        a.beingAbsorbed = false;
        a.absorbFrame = 0; a.absorbDuration = 0;
        a.absorbStartX = 0; a.absorbStartY = 0;
        a.alienType = 0;
        int hpBonus = (score >= 60) ? (score / 30 - 1) * 2 : 0;
        a.hp = ALIEN_MIN_HP + hpBonus + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
        a.maxHp = a.hp;
        a.active = true;
        aliens.push_back(a);
    }

    void update(bool duringBossAbsorb, double speedFactor, bool& gameOver, int& baseHP,
                ParticleManager& pm, AudioEngine* audio) {
        const double A = WIN_WIDTH / 2.0 - 15.0;
        const double B = 75.0;
        for (auto& a : aliens) {
            if (!a.active) continue;

            if (a.entering) {
                a.enterFrame++;
                double raw = (double)a.enterFrame / a.enterDuration;
                double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                if (raw >= 1.0) {
                    raw = 1.0; eased = 1.0;
                    a.entering = false; a.invincibleFrames = 3;
                    double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                    pm.spawnExplosion(ax, a.y, 8);
                }
                if (a.enterFromBoss) {
                    a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                    a.t = a.startT + (a.targetT - a.startT) * eased;
                } else if (a.enterFromTop) {
                    a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                } else {
                    a.t = a.startT + (a.targetT - a.startT) * eased;
                }
            }

            if (!a.entering) {
                if (a.invincibleFrames > 0) a.invincibleFrames--;
                double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                double sf = (depthBelow < 0) ? 0.08 : 0.08 + 0.92 * depthBelow;
                a.y += baseSpeed * sf * (duringBossAbsorb ? speedFactor : 1.0);
            }

            if (a.y > WIN_HEIGHT + 30) { a.active = false; continue; }

            // Base collision
            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
            double dx = (ax - CENTER_X) / A;
            double dy = (WIN_HEIGHT - a.y) / B;
            if (dx*dx + dy*dy < 1.0) {
                pm.spawnExplosion(ax, a.y, 35);
                a.active = false;
                if (a.invincibleFrames == 0) {
                    baseHP--;
                    if (audio) audio->sndBaseDamage();
                    if (baseHP <= 0) { gameOver = true; }
                }
            }
        }
    }

    void draw(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        for (const auto& a : aliens) {
            if (!a.active) continue;
            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
            double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
            double scale = (depthBelow < 0) ? 0.17 : 0.17 + 0.83 * depthBelow;
            if (scale < 0.14) scale = 0.14;
            if (scale > 1.0)  scale = 1.0;
            int s = (int)(29.0 * scale);
            if (s < 4) s = 4;

            // Entry trail：保留已有的 3 段残影，但改成逐渐变大的星尘光点。
            if (a.entering && a.enterFrame > 3) {
                for (int k = 1; k <= 4; ++k) {
                    double pastRaw = (double)(a.enterFrame - k * 3) / a.enterDuration;
                    if (pastRaw < 0) continue;
                    double pastEased = 1.0 - std::pow(1.0 - pastRaw, 3.0);
                    double pastX, pastY;
                    if (a.enterFromBoss) {
                        pastY = a.enterStartY + (a.enterTargetY - a.enterStartY) * pastEased;
                        double pastT2 = a.startT + (a.targetT - a.startT) * pastEased;
                        pastX = perspLeft(pastY) + pastT2 * perspWidth(pastY);
                    } else if (a.enterFromTop) {
                        pastY = a.enterStartY + (a.enterTargetY - a.enterStartY) * pastEased;
                        pastX = perspLeft(pastY) + a.t * perspWidth(pastY);
                    } else {
                        pastY = a.y;
                        double pastT = a.startT + (a.targetT - a.startT) * pastEased;
                        pastX = perspLeft(pastY) + pastT * perspWidth(pastY);
                    }
                    int alpha = 130 - k * 25;
                    if (alpha < 18) continue;
                    int rr = k == 1 ? 128 : 90;
                    ArtFX::starGlint(renderer, (int)pastX, (int)pastY, 1 + (k == 1 ? 1 : 0),
                                     rr, 174, 255, alpha);
                }
            }

            double hpRatio = (double)a.hp / (a.maxHp > 0 ? a.maxHp : 1);
            bool inv = a.entering || a.invincibleFrames != 0;
            int r, g, b;
            if (inv) { r = 98; g = 180; b = 255; }
            else {
                r = 255;
                g = (int)(154 * hpRatio + 70 * (1 - hpRatio));
                b = (int)(82 * hpRatio + 26 * (1 - hpRatio));
            }
            int ix = (int)ax, iy = (int)a.y;

            // 六边形能量风筝：比旧菱形多两个折面，仍严格围绕原中心点绘制。
            SDL_Point hull[6] = {
                {ix, iy - s},
                {ix + s * 2 / 3, iy - s / 5},
                {ix + s / 3, iy + s * 2 / 3},
                {ix, iy + s},
                {ix - s / 3, iy + s * 2 / 3},
                {ix - s * 2 / 3, iy - s / 5}
            };
            ArtFX::fillPoly(renderer, hull, 6, r / 5 + 8, g / 6 + 6, b / 8 + 6, 224);
            for (int i = 0; i < 6; ++i) {
                SDL_Point& p0 = hull[i]; SDL_Point& p1 = hull[(i + 1) % 6];
                int a0 = (i == 0 || i == 5) ? 245 : 170;
                ArtFX::glowLine(renderer, p0.x, p0.y, p1.x, p1.y, r, g, b, a0, 1);
            }
            // 翼面折线 / 核心。低血量时出现不规则的断裂线，但位置仍然固定。
            ArtFX::glowLine(renderer, ix - s * 2 / 3, iy - s / 5,
                            ix, iy + s / 2, r, g, b, 105, 1);
            ArtFX::glowLine(renderer, ix + s * 2 / 3, iy - s / 5,
                            ix, iy + s / 2, r, g, b, 105, 1);
            SDL_Point core[4] = {{ix, iy - s/3}, {ix + s/5, iy}, {ix, iy + s/3}, {ix - s/5, iy}};
            ArtFX::fillPoly(renderer, core, 4, 255, inv ? 220 : 220, inv ? 255 : 168, 210);
            if (hpRatio < 0.55 && !inv) {
                ArtFX::glowLine(renderer, ix - s/2, iy - s/4, ix - s/8, iy + s/6, 255, 210, 138, 115, 1);
                ArtFX::glowLine(renderer, ix + s/3, iy - s/3, ix + s/2, iy + s/5, 255, 190, 114, 95, 1);
            }
            if (inv && !a.entering) {
                // 吸收/无敌状态：冷蓝六边形护罩。
                SDL_Point ring[6];
                int rr = s + 5;
                for (int i = 0; i < 6; ++i) {
                    double ang = 2.0 * M_PI * i / 6.0 + M_PI / 6.0;
                    ring[i] = {ix + (int)(rr * std::cos(ang)), iy + (int)(rr * std::sin(ang))};
                }
                for (int i = 0; i < 6; ++i)
                    ArtFX::glowLine(renderer, ring[i].x, ring[i].y,
                                    ring[(i+1)%6].x, ring[(i+1)%6].y, 82, 170, 255, 70, 1);
            }
            // 后向推进器残烬：从风筝后缘向上散开，强调其朝基地俯冲。
            if (!inv) {
                ArtFX::glowLine(renderer, ix - s/4, iy - s * 4/5,
                                ix - s/5, iy - s - 4, r, g, b, 70, 1);
                ArtFX::glowLine(renderer, ix + s/4, iy - s * 4/5,
                                ix + s/5, iy - s - 3, r, g, b, 55, 1);
            }
        }
    }

    void removeInactive() {
        aliens.erase(std::remove_if(aliens.begin(), aliens.end(),
            [](const Ch1Alien& a){ return !a.active; }), aliens.end());
    }

    std::vector<Ch1Alien>& all() { return aliens; }
    void pushAlien(const Ch1Alien& a) { aliens.push_back(a); }
    int countAlive() const {
        int c = 0;
        for (const auto& a : aliens) if (a.active) c++;
        return c;
    }
    void setAllInvincible() { for (auto& a : aliens) a.invincibleFrames = -1; }
    void setAllVulnerable() { for (auto& a : aliens) a.invincibleFrames = 0; }
    int& spawnTimerRef() { return spawnTimer; }

    // 存档：全部外星飞船 + 出场计时 + 难度参数
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(aliens);
        ar.ioNum(spawnTimer); ar.ioNum(baseSpeed); ar.ioNum(spawnInterval); ar.ioNum(hpBonus);
    }
};
