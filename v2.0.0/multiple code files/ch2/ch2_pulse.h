#pragma once

#include "../audio.h"
#include "../particles.h"
#include "../art_fx.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch2PulseSystem (green energy bar + Shift pulse) ==============
class Ch2PulseSystem {
public:
    static const int MAX_ENERGY = 30;

    int energy;
    bool unlocked;      // false until skill orb absorbed
    bool draining;      // true when energy is rapidly draining after absorption cancel

    std::vector<Ch2PulseWave> waves;
    int nextWaveID;

    Ch2PulseSystem() : energy(0), unlocked(false), draining(false), nextWaveID(0) {}

    void reset() {
        energy = 0; unlocked = false; draining = false;
        waves.clear(); nextWaveID = 0;
    }

    void addEnergy(int hits) {
        if (!unlocked) return;
        if (hits <= 0) return;
        energy += hits;
        if (energy > MAX_ENERGY) energy = MAX_ENERGY;
    }

    void addAbsorbEnergy() {
        // Gradually fill during orb absorption (target: 30 over 300 frames = every 10 frames)
        energy += 1;
        if (energy > MAX_ENERGY) energy = MAX_ENERGY;
    }

    void startDrain() { draining = true; }
    bool isDraining() const { return draining; }

    bool canAbsorb() const { return !draining && energy == 0; }

    void drainTick() {
        if (!draining) return;
        energy -= 3;
        if (energy <= 0) { energy = 0; draining = false; }
    }

    float getFill() const {
        return (float)energy / MAX_ENERGY;
    }

    bool isFull() const { return unlocked && energy >= MAX_ENERGY; }

    // 存档：绿色脉冲能量/解锁状态/扩散中的冲击波环
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(energy); ar.ioBool(unlocked); ar.ioBool(draining);
        ar.ioVecObj(waves); ar.ioNum(nextWaveID);
    }

    // Breathing: bright green ↔ dark green when full (for HUD rendering)
    float getBreathAlpha() const {
        // 状态派生值：满能量时固定为高亮，避免绘制依赖真实时间。
        return isFull() ? 1.0f : 0.75f;
    }

    void release(double px, double py, ParticleManager& pm, AudioEngine& audio) {
        energy = 0; // consume all energy

        Ch2PulseWave w;
        w.x = px; w.y = py;
        w.radius = 20.0;
        w.id = nextWaveID++;
        w.active = true;
        waves.push_back(w);

        // White particle burst (similar to Ch1 base shockwave)
        for (int i = 0; i < 50; ++i) {
            double angle = (rand() % 360) * M_PI / 180.0;
            double speed = 2.0 + (rand() % 500) / 100.0;
            pm.spawnWhiteParticle(px, py,
                std::cos(angle) * speed,
                std::sin(angle) * speed,
                20 + rand() % 30);
        }
        audio.sndShockwave();
    }

    void update() {
        // Update expanding shockwave rings
        for (auto& w : waves) {
            if (!w.active) continue;
            w.radius += 8.0;
            if (w.radius > 520.0) w.active = false;
        }
        waves.erase(std::remove_if(waves.begin(), waves.end(),
            [](const Ch2PulseWave& w){ return !w.active; }), waves.end());
    }

    void draw(SDL_Renderer* r) const {
        ArtFX::BlendGuard guard(r);
        for (const auto& w : waves) {
            if (!w.active) continue;
            double progress = (w.radius - 20.0) / 500.0;
            if (progress > 1.0) progress = 1.0;
            if (progress < 0.0) progress = 0.0;
            int alpha = (int)(235.0 * (1.0 - progress));
            if (alpha < 10) alpha = 10;
            int cx = (int)w.x, cy = (int)w.y;
            const int SEG = 72;
            for (int ring = 0; ring < 2; ++ring) {
                double rad = ring == 0 ? w.radius : w.radius * 0.87;
                int aa = ring == 0 ? alpha : alpha * 2 / 3;
                SDL_SetRenderDrawColor(r, ring == 0 ? 196 : 94,
                                       ring == 0 ? 255 : 230,
                                       ring == 0 ? 226 : 255, (Uint8)aa);
                int prevX = cx + (int)rad, prevY = cy;
                for (int i = 1; i <= SEG; ++i) {
                    double a = 2.0 * M_PI * i / SEG;
                    int sx = cx + (int)(rad * std::cos(a));
                    int sy = cy + (int)(rad * std::sin(a));
                    if (ring == 0)
                        ArtFX::glowLine(r, prevX, prevY, sx, sy, 105, 255, 180, aa, 1);
                    else
                        SDL_RenderDrawLine(r, prevX, prevY, sx, sy);
                    prevX = sx; prevY = sy;
                }
            }
            // 波面上的能量刻度与中心冲击环。
            for (int i = 0; i < 12; ++i) {
                double a = i * M_PI / 6.0 + progress * 2.2;
                int x0 = cx + (int)(w.radius * std::cos(a));
                int y0 = cy + (int)(w.radius * std::sin(a));
                int x1 = cx + (int)((w.radius + 7) * std::cos(a));
                int y1 = cy + (int)((w.radius + 7) * std::sin(a));
                ArtFX::glowLine(r, x0, y0, x1, y1, 138, 255, 200, alpha / 2, 1);
            }
            for (int k = 1; k <= 3; ++k) {
                int rr = (int)(w.radius * 0.23 * k);
                if (rr < 2) continue;
                SDL_SetRenderDrawColor(r, 170, 255, 215, (Uint8)(alpha / (k + 2)));
                int px = cx + rr, py = cy;
                for (int i = 1; i <= 24; ++i) {
                    double a = 2.0 * M_PI * i / 24.0;
                    int nx = cx + (int)(rr * std::cos(a));
                    int ny = cy + (int)(rr * std::sin(a));
                    SDL_RenderDrawLine(r, px, py, nx, ny);
                    px = nx; py = ny;
                }
            }
        }
    }

    // Collision: pulse vs enemy bullets → destroy all bullets in radius.
    // Clearing bullets with the pulse intentionally grants no energy.
    void collideWithBullets(std::vector<Ch2EnemyBullet>& bullets, ParticleManager& pm) {
        for (auto& w : waves) {
            if (!w.active) continue;
            for (auto& b : bullets) {
                if (!b.active) continue;
                double dx = b.x - w.x, dy = b.y - w.y;
                if (dx*dx + dy*dy < (w.radius+4.0)*(w.radius+4.0)) {
                    b.active = false;
                    pm.spawnExplosion(b.x, b.y, 2);
                }
            }
        }
    }

    // Collision: pulse vs Ch2 alien enemies
    void collideWithAliens(Ch2AlienManager& am, ParticleManager& pm, AudioEngine& audio,
                           int& score, int& hitCount) {
        for (auto& w : waves) {
            if (!w.active) continue;
            auto& aliens = const_cast<std::vector<Ch2Alien>&>(am.getAliens());
            for (auto& a : aliens) {
                if (!a.active) continue;
                if (a.lastHitByPulse == w.id) continue;
                double dx = a.x - w.x, dy = a.y - w.y;
                if (dx*dx + dy*dy < (w.radius+22.0)*(w.radius+22.0)) {
                    a.lastHitByPulse = w.id;
                    a.hp -= 1; hitCount += 2;
                    pm.spawnExplosion(a.x, a.y, 4);
                    audio.sndShockwaveHit();
                    if (a.hp <= 0) {
                        a.defeated = true; a.active = false;
                        pm.spawnExplosion(a.x, a.y, 20);
                        pm.spawnExplosion(a.x-8, a.y-4, 12);
                        pm.spawnExplosion(a.x+8, a.y+4, 12);
                        audio.sndExplosionSmall(); score += 1;
                    }
                }
            }
        }
    }

    // Collision: pulse vs danmaku enemies
    void collideWithDanmaku(Ch2DanmakuManager& dm, ParticleManager& pm, AudioEngine& audio,
                            int& score, int& hitCount) {
        for (auto& w : waves) {
            if (!w.active) continue;
            auto& enemies = const_cast<std::vector<Ch2DanmakuEnemy>&>(dm.getEnemies());
            for (auto& e : enemies) {
                if (!e.active || e.entering) continue;
                if (e.lastHitByPulse == w.id) continue;
                if (e.defeated && e.defeatTimer > 0) continue;
                double dx = e.x - w.x, dy = e.y - w.y;
                double hitR = e.nearLeg + 10.0;
                if (dx*dx + dy*dy < (w.radius+hitR)*(w.radius+hitR)) {
                    e.lastHitByPulse = w.id;
                    e.hp -= 1; hitCount += 2;
                    pm.spawnExplosion(e.x, e.y, 4);
                    audio.sndShockwaveHit();
                    if (e.hp <= 0) {
                        e.defeated = true; e.invincibleFrames = 300;
                        e.defeatTimer = 70;
                        score += 5;
                        pm.spawnExplosion(e.x, e.y, 20);
                        audio.sndExplosionSmall();
                    }
                }
            }
        }
    }
};
