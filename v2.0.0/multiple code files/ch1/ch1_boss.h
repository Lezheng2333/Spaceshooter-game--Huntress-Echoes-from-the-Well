#pragma once

#include "../audio.h"
#include "../bullets.h"
#include "../constants.h"
#include "../font.h"
#include "../particles.h"
#include "../art_fx.h"
#include "../renderer.h"
#include "../types.h"
#include "../ch1/ch1_aliens.h"

// ============== Ch1Boss ==============
class Ch1Boss {
    // Core state
    double y;
    int hp, maxHp, bonusHp;
    bool active;
    double x;
    double moveTime;
    int flashTimer;
    int lastHitBySW;

    // Enter
    bool entering;
    int enterFrame, enterDuration;

    // Phase
    bool phase2Triggered;
    int shakeTimer;
    int shakeX, shakeY;

public:
    enum AbsorbState { IDLE, BEAM_FLYING, SPIRALING, COOLDOWN };
private:
    // Absorb state machine
    int absorbTimer;
    int absorbIndex;
    int absorbCooldown;
    AbsorbState absorbState;
    int absorbTargetIdx;
    int postAbsorbTimer;
    bool healWavesEnabled;

    // Heal wave system
    std::vector<Ch1HealWave> healWaves;
    int healWaveTimer;
    int nextCh1HealWaveID;

    // Config
    const BossConfig* cfg;

public:
    Ch1Boss() : y(-300), hp(1000), maxHp(1000), bonusHp(0), active(false),
             x(CENTER_X), moveTime(0), flashTimer(0), lastHitBySW(-1),
             entering(false), enterFrame(0), enterDuration(150),
             phase2Triggered(false), shakeTimer(0), shakeX(0), shakeY(0),
             absorbTimer(0), absorbIndex(0), absorbCooldown(0),
             absorbState(IDLE), absorbTargetIdx(-1), postAbsorbTimer(0),
             healWavesEnabled(false), healWaveTimer(0), nextCh1HealWaveID(0), cfg(nullptr) {}

    void setConfig(const BossConfig* c) { cfg = c; }

    void reset() {
        y = -300; hp = 1000; maxHp = 1000; bonusHp = 0;
        active = false; x = CENTER_X; moveTime = 0; flashTimer = 0; lastHitBySW = -1;
        entering = false; enterFrame = 0; enterDuration = 150;
        phase2Triggered = false; shakeTimer = 0; shakeX = shakeY = 0;
        absorbTimer = 0; absorbIndex = 0; absorbCooldown = 0;
        absorbState = IDLE; absorbTargetIdx = -1; postAbsorbTimer = 0;
        healWavesEnabled = false; healWaveTimer = 0; nextCh1HealWaveID = 0;
        healWaves.clear();
    }

    // Accessors
    double getX() const { return x; }
    double getY() const { return y; }
    void setY(double ny) { y = ny; }
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getBonusHp() const { return bonusHp; }
    int getLastHitBySW() const { return lastHitBySW; }
    int& lastHitBySWRef() { return lastHitBySW; }
    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
    void setMaxHp(int m) { maxHp = m; }
    bool isEntering() const { return entering; }
    int getShakeTimer() const { return shakeTimer; }
    int getShakeX() const { return shakeX; }
    int getShakeY() const { return shakeY; }
    int getFlashTimer() const { return flashTimer; }
    int& bonusHpRef() { return bonusHp; }
    int& hpRef() { return hp; }
    bool isPhase2Triggered() const { return phase2Triggered; }
    bool isCh1HealWavesEnabled() const { return healWavesEnabled; }
    std::vector<Ch1HealWave>& getCh1HealWaves() { return healWaves; }

    // 存档：Telamondo 全部战斗状态（cfg 指针不入档，读档后由 Game 重新绑定章节配置）
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(y); ar.ioNum(hp); ar.ioNum(maxHp); ar.ioNum(bonusHp);
        ar.ioBool(active); ar.ioNum(x); ar.ioNum(moveTime); ar.ioNum(flashTimer);
        ar.ioNum(lastHitBySW);
        ar.ioBool(entering); ar.ioNum(enterFrame); ar.ioNum(enterDuration);
        ar.ioBool(phase2Triggered); ar.ioNum(shakeTimer); ar.ioNum(shakeX); ar.ioNum(shakeY);
        ar.ioNum(absorbTimer); ar.ioNum(absorbIndex); ar.ioNum(absorbCooldown);
        ar.ioEnum(absorbState); ar.ioNum(absorbTargetIdx); ar.ioNum(postAbsorbTimer);
        ar.ioBool(healWavesEnabled);
        ar.ioVecObj(healWaves); ar.ioNum(healWaveTimer); ar.ioNum(nextCh1HealWaveID);
    }

    void takeDamage(int dmg) {
        flashTimer = 5;
        if (bonusHp > 0) {
            if (dmg <= bonusHp) { bonusHp -= dmg; return; }
            dmg -= bonusHp; bonusHp = 0;
        }
        hp -= dmg;
    }

    void trigger() {
        active = true;
        y = -300;
        hp = cfg ? cfg->maxHp : 1000;
        maxHp = cfg ? cfg->maxHp : 1000;
        bonusHp = 0;
        lastHitBySW = -1;
        entering = true;
        enterFrame = 0;
        enterDuration = cfg ? cfg->enterDuration : 150;
        phase2Triggered = false;
        absorbTimer = 0; absorbIndex = 0; absorbCooldown = 0;
        absorbState = IDLE; absorbTargetIdx = -1; postAbsorbTimer = 0;
        healWavesEnabled = false; healWaveTimer = 0; nextCh1HealWaveID = 0;
        healWaves.clear();
        x = CENTER_X; moveTime = 0;
    }

    void updateEnterAnimation(AudioEngine* audio) {
        if (!entering) return;
        enterFrame++;
        double raw = (double)enterFrame / enterDuration;
        if (raw > 1.0) raw = 1.0;
        double eased = 1.0 - std::pow(1.0 - raw, 3.0);
        y = -300.0 + (90.0 + 300.0) * eased;
        if (raw >= 1.0) {
            entering = false;
            shakeTimer = cfg ? cfg->shakeDuration : 60;
            if (audio) audio->sndBossEntrance();
        }
    }

    void updateShake(AudioEngine* audio) {
        if (shakeTimer > 0) {
            shakeTimer--;
            shakeX = (rand() % 10) - 5;
            shakeY = (rand() % 10) - 5;
            if (shakeTimer % 12 == 0 && audio) audio->sndShake();
        } else { shakeX = shakeY = 0; }
    }

    void updatePostAbsorbShake(AudioEngine* audio) {
        if (postAbsorbTimer > 0) {
            postAbsorbTimer--;
            shakeX = (rand() % 10) - 5; shakeY = (rand() % 10) - 5;
            if (postAbsorbTimer % 10 == 0 && audio) audio->sndShake();
        }
    }

    void triggerPhase2() {
        phase2Triggered = true;
        shakeTimer = 120;
        absorbTimer = 0;
        postAbsorbTimer = 0;
        absorbCooldown = 0;
        absorbState = IDLE;
        absorbTargetIdx = -1;
        setCh1HealWavesEnabled(false);
    }

    void updateMovement() {
        moveTime += 0.025;
        x = CENTER_X + std::sin(moveTime) * (cfg ? cfg->moveAmplitudeX : 140.0);
        y = 90.0 + std::sin(moveTime * 2.0) * std::cos(moveTime) * (cfg ? cfg->moveAmplitudeY : 25.0);
        if (flashTimer > 0) flashTimer--;
    }

    void updateCh1HealWaves(ParticleManager& pm, Ch1AlienManager& aliens, AudioEngine* audio) {
        if (!healWavesEnabled) return;
        healWaveTimer++;
        int interval = cfg ? cfg->healWaveInterval : 420;
        if (healWaveTimer >= interval) {
            healWaveTimer = 0;
            Ch1HealWave hw; hw.radius = 15; hw.id = nextCh1HealWaveID++; hw.active = true;
            healWaves.push_back(hw);
        }
        for (auto& hw : healWaves) {
            if (!hw.active) continue;
            hw.radius += 4.0;
            if (hw.radius > 700) hw.active = false;
        }
        // Heal wave collision
        for (auto& hw : healWaves) {
            if (!hw.active) continue;
            for (auto& a : aliens.all()) {
                if (!a.active || a.entering || a.beingAbsorbed) continue;
                if (a.lastHealHit == hw.id) continue;
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                double dist = std::sqrt((ax - x)*(ax - x) + (a.y - y)*(a.y - y));
                if (std::abs(dist - hw.radius) < 25.0) {
                    a.lastHealHit = hw.id;
                    if (audio) audio->sndBossHeal();
                    int healAmt = cfg ? cfg->healHpPerWave : 30;
                    bonusHp += healAmt;
                    int barEndX = CENTER_X - 200 + (int)((double)(hp + bonusHp) / maxHp * 400);
                    if (barEndX > WIN_WIDTH) barEndX = WIN_WIDTH;
                    for (int i = 0; i < 10; ++i) {
                        Ch1Particle p;
                        p.x = ax + (rand()%10-5); p.y = a.y + (rand()%10-5);
                        p.vx = (barEndX - p.x) / 60.0 + (rand()%10-5)/10.0;
                        p.vy = (16 - p.y) / 30.0 + (rand()%10-5)/10.0;
                        p.life = 30 + rand()%15; p.active = true;
                        p.whiteParticle = false; p.greenParticle = false; p.redParticle = true;
                        pm.all().push_back(p);
                    }
                }
            }
        }
        healWaves.erase(std::remove_if(healWaves.begin(), healWaves.end(),
            [](const Ch1HealWave& hw){ return !hw.active; }), healWaves.end());
    }

    // Shared absorb state machine (deduplicated from INTRO and PHASE2)
    bool updateAbsorbStateMachine(Ch1AlienManager& aliens, BulletManager& bullets,
                                   ParticleManager&, AudioEngine*) {
        if (absorbCooldown > 0) absorbCooldown--;

        if (absorbState == IDLE) {
            int idx = -1;
            for (int i = 0; i < (int)aliens.all().size(); ++i) {
                if (aliens.all()[i].active && !aliens.all()[i].beingAbsorbed) { idx = i; break; }
            }
            if (idx >= 0) {
                Ch1Alien& a = aliens.all()[idx];
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                bullets.addBossBeam(x, y, ax, a.y, idx);
                absorbTargetIdx = idx;
                absorbState = BEAM_FLYING;
            } else if (absorbCooldown <= 0) {
                for (auto& aa : aliens.all()) {
                    if (aa.active) { bonusHp += (cfg ? cfg->bonusHpPerAlien : 50); aa.active = false; }
                }
                aliens.all().clear();
                postAbsorbTimer = 30;
                absorbTimer = -1;
                absorbState = IDLE;
                return true;  // all done
            }
        }
        // Safety net: beam lost target
        if (absorbState == BEAM_FLYING) {
            bool beamAlive = false;
            for (auto& bb : bullets.all())
                if (bb.active && bb.blueBeam && bb.beamTargetIndex == absorbTargetIdx)
                    { beamAlive = true; break; }
            if (!beamAlive) { absorbState = COOLDOWN; absorbCooldown = 18; }
        }
        // Spiral complete → cooldown
        if (absorbState == SPIRALING && absorbTargetIdx >= 0) {
            if (!aliens.all()[absorbTargetIdx].active || !aliens.all()[absorbTargetIdx].beingAbsorbed) {
                absorbState = COOLDOWN; absorbCooldown = 18;
            }
        }
        if (absorbState == COOLDOWN && absorbCooldown <= 0) absorbState = IDLE;
        return false;
    }

    // Shared absorb animation (deduplicated)
    void updateAbsorbAnimations(Ch1AlienManager& aliens, ParticleManager& pm) {
        for (auto& a : aliens.all()) {
            if (!a.active || !a.beingAbsorbed) continue;
            a.absorbFrame++;
            double raw = (double)a.absorbFrame / a.absorbDuration;
            if (raw >= 1.0) {
                raw = 1.0; a.active = false;
                int gain = cfg ? cfg->bonusHpPerAlien : 50;
                bonusHp += gain;
                absorbCooldown = 18;
                int barEndX = CENTER_X - 200 + (int)((double)(hp + bonusHp) / maxHp * 400);
                if (barEndX > WIN_WIDTH) barEndX = WIN_WIDTH;
                for (int i = 0; i < 12; ++i) {
                    Ch1Particle p;
                    p.x = x + (rand()%30-15); p.y = y + (rand()%20-10);
                    p.vx = (barEndX - p.x) / 60.0 + (rand()%10-5)/10.0;
                    p.vy = (16 - p.y) / 60.0 + (rand()%10-5)/10.0;
                    p.life = 60 + rand()%20; p.active = true;
                    p.whiteParticle = false; p.greenParticle = false; p.redParticle = true;
                    pm.all().push_back(p);
                }
            } else {
                double eased = 1.0 - std::pow(1.0 - raw, 2.0);
                double startDist = std::sqrt(
                    (a.absorbStartX - x)*(a.absorbStartX - x) +
                    (a.absorbStartY - y)*(a.absorbStartY - y));
                double startAngle = std::atan2(a.absorbStartY - y, a.absorbStartX - x);
                double angle = startAngle + raw * M_PI * 5.0;
                double radius = startDist * (1.0 - eased);
                a.y = y + std::sin(angle) * radius;
                double tx = x + std::cos(angle) * radius;
                a.t = (tx - perspLeft(a.y)) / perspWidth(a.y);
            }
        }
    }

    void drawCircularShockwave(SDL_Renderer* renderer) const {
        if (shakeTimer <= 0) return;
        ArtFX::BlendGuard guard(renderer);
        double progress = 1.0 - (double)shakeTimer / 120.0;
        if (progress > 1.0) progress = 1.0;
        if (progress < 0.0) progress = 0.0;
        for (int ring = 0; ring < 3; ++ring) {
            double radius = 40.0 + progress * (520.0 + ring * 34.0);
            int alpha = (int)(185.0 * (1.0 - progress) / (ring + 1));
            if (alpha < 5) continue;
            const int SEG = 64;
            int prevX = -10000, prevY = 0;
            for (int i = 0; i <= SEG; ++i) {
                double angle = 2.0 * M_PI * i / SEG;
                double sy = y + std::sin(angle) * radius;
                double rx = radius * perspWidth(sy) / perspWidth(y);
                int sx = (int)(x + std::cos(angle) * rx);
                if (i > 0) {
                    if (ring == 0)
                        ArtFX::glowLine(renderer, prevX, prevY, sx, (int)sy, 255, 74, 78, alpha, 1);
                    else {
                        SDL_SetRenderDrawColor(renderer, 255, 74, 78, (Uint8)alpha);
                        SDL_RenderDrawLine(renderer, prevX, prevY, sx, (int)sy);
                    }
                }
                prevX = sx; prevY = (int)sy;
            }
        }
    }

    void drawCh1HealWaves(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        for (const auto& hw : healWaves) {
            if (!hw.active) continue;
            const int SEG = 64;
            int prevX = -10000, prevY = 0;
            for (int i = 0; i <= SEG; ++i) {
                double angle = 2.0 * M_PI * i / SEG;
                double sy = y + std::sin(angle) * hw.radius;
                double rx = hw.radius * perspWidth(sy) / perspWidth(y);
                int sx = (int)(x + std::cos(angle) * rx);
                if (i > 0) {
                    ArtFX::glowLine(renderer, prevX, prevY, sx, (int)sy, 255, 82, 98, 72, 1);
                    if (i % 8 == 0) {
                        SDL_SetRenderDrawColor(renderer, 255, 186, 178, 90);
                        SDL_RenderDrawLine(renderer, prevX, prevY, sx, (int)sy);
                    }
                }
                prevX = sx; prevY = (int)sy;
            }
        }
    }

    void drawBody(SDL_Renderer* renderer) const {
        if (!active || y < -125) return;
        ArtFX::BlendGuard guard(renderer);
        double scale = 0.5 + 0.5 * ((y + 100) / 200.0);
        if (scale > 1.0) scale = 1.0;
        if (scale < 0.18) scale = 0.18;
        int s = (int)((cfg ? cfg->size : 80.0) * scale);
        if (s < 20) s = 20;
        int ix = (int)x, iy = (int)y;
        bool hot = flashTimer > 0;
        bool p2 = phase2Triggered;
        int br = hot ? 255 : (p2 ? 255 : 210);
        int bg = hot ? 226 : (p2 ? 92 : 60);
        int bb = hot ? 210 : (p2 ? 82 : 62);

        // 防御奇点：两圈不对称的赤红轨道。它们纯粹是视觉护盾，
        // 不参与 hit test，因此不会改变 Boss 的命中窗口。
        double orbit = moveTime * (p2 ? 1.65 : 1.05);
        for (int ring = 0; ring < 2; ++ring) {
            int rr = (int)(s * (1.48 + ring * 0.28));
            int alpha = ring == 0 ? 72 : 48;
            int px = ix + rr, py = iy;
            for (int i = 1; i <= 56; ++i) {
                double a = 2.0 * M_PI * i / 56.0 + orbit * (ring == 0 ? 1.0 : -0.65);
                int nx = ix + (int)(rr * std::cos(a));
                int ny = iy + (int)(rr * 0.72 * std::sin(a));
                if (i > 1 && (i % 8 != 0)) {
                    SDL_SetRenderDrawColor(renderer, 255, 72, 84, (Uint8)alpha);
                    SDL_RenderDrawLine(renderer, px, py, nx, ny);
                }
                if (i % 8 == 0) {
                    ArtFX::starGlint(renderer, nx, ny, ring == 0 ? 2 : 1, 255, 113, 112, 120);
                }
                px = nx; py = ny;
            }
        }

        // 船体：层叠的暗红装甲。上宽下窄，保持"堡垒"而不是放大敌机的剪影。
        SDL_Point hull[8] = {
            {ix, iy - s},
            {ix + s * 5 / 9, iy - s * 7 / 10},
            {ix + s * 9 / 10, iy - s / 5},
            {ix + s * 3 / 4, iy + s / 3},
            {ix, iy + s * 2 / 3},
            {ix - s * 3 / 4, iy + s / 3},
            {ix - s * 9 / 10, iy - s / 5},
            {ix - s * 5 / 9, iy - s * 7 / 10}
        };
        ArtFX::fillPoly(renderer, hull, 8, 36, 7, 13, 245);
        for (int i = 0; i < 8; ++i) {
            SDL_Point& a = hull[i]; SDL_Point& b = hull[(i + 1) % 8];
            ArtFX::glowLine(renderer, a.x, a.y, b.x, b.y, br, bg, bb, hot ? 245 : 195, 1);
        }
        // 肩部装甲舱与下方龙骨。
        SDL_Point lPod[4] = {{ix - s * 8 / 10, iy - s / 5}, {ix - s * 5 / 9, iy - s * 6 / 10},
                             {ix - s * 4 / 9, iy - s / 3}, {ix - s * 5 / 9, iy + s / 8}};
        SDL_Point rPod[4] = {{ix + s * 8 / 10, iy - s / 5}, {ix + s * 5 / 9, iy - s * 6 / 10},
                             {ix + s * 4 / 9, iy - s / 3}, {ix + s * 5 / 9, iy + s / 8}};
        ArtFX::fillPoly(renderer, lPod, 4, 58, 13, 20, 225);
        ArtFX::fillPoly(renderer, rPod, 4, 58, 13, 20, 225);
        for (int i = 0; i < 4; ++i) {
            ArtFX::glowLine(renderer, lPod[i].x, lPod[i].y, lPod[(i+1)%4].x, lPod[(i+1)%4].y, br, bg, bb, 155, 1);
            ArtFX::glowLine(renderer, rPod[i].x, rPod[i].y, rPod[(i+1)%4].x, rPod[(i+1)%4].y, br, bg, bb, 155, 1);
        }
        // 舰艏与肩部之间的装甲脊线。
        ArtFX::glowLine(renderer, ix, iy - s, ix, iy + s * 2 / 3, br, bg, bb, 90, 1);
        ArtFX::glowLine(renderer, ix - s * 9 / 10, iy - s / 5, ix + s * 9 / 10, iy - s / 5, br, bg, bb, 70, 1);
        ArtFX::glowLine(renderer, ix - s * 3 / 4, iy + s / 3, ix + s * 3 / 4, iy + s / 3, br, bg, bb, 65, 1);

        // 中央核心：双层环 + 竖缝，鲜明的"有生命的堡垒"焦点。
        double pulse = 0.5 + 0.5 * std::sin(moveTime * 2.8);
        int cr = hot ? 255 : (p2 ? 255 : (int)(185 + pulse * 40));
        int cg = hot ? 238 : (p2 ? 118 : (int)(48 + pulse * 16));
        int cb = hot ? 224 : (p2 ? 98 : 66);
        ArtFX::fillPoly(renderer, hull, 8, 18, 3, 7, 120); // 内层暗色，压住背景纹理
        int coreR = (int)(s * 0.34);
        // 四层羽化光环
        for (int layer = 4; layer >= 0; --layer) {
            int rr = coreR + 4 + layer * 4;
            int al = 18 + layer * 24;
            SDL_SetRenderDrawColor(renderer, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)al);
            int px = ix + rr, py = iy;
            for (int i = 1; i <= 28; ++i) {
                double a = 2.0 * M_PI * i / 28.0;
                int nx = ix + (int)(rr * std::cos(a));
                int ny = iy + (int)(rr * std::sin(a));
                SDL_RenderDrawLine(renderer, px, py, nx, ny);
                px = nx; py = ny;
            }
        }
        // 实心核心 + 竖直瞳孔缝
        for (int yy = -coreR; yy <= coreR; ++yy) {
            int hw = (int)std::sqrt((double)std::max(0, coreR * coreR - yy * yy));
            ArtFX::fillPoly(renderer, (SDL_Point[]){{ix - hw, iy + yy}, {ix + hw, iy + yy},
                                                    {ix + hw, iy + yy + 1}, {ix - hw, iy + yy + 1}}, 4,
                            cr / 3, cg / 3, cb / 3, 210);
        }
        SDL_SetRenderDrawColor(renderer, (Uint8)cr, (Uint8)(cg + 20), (Uint8)cb, 245);
        SDL_Rect slit = {ix - s / 12, iy - s / 5, std::max(2, s / 6), std::max(5, s * 2 / 5)};
        SDL_RenderFillRect(renderer, &slit);
        SDL_SetRenderDrawColor(renderer, 255, 239, 220, 210);
        SDL_RenderDrawLine(renderer, ix, iy - s / 5, ix, iy + s / 5);
        // 核心外的刻度环。
        for (int i = 0; i < 12; ++i) {
            double a = i * 2.0 * M_PI / 12.0 + orbit * 0.4;
            int x0 = ix + (int)((coreR + 2) * std::cos(a));
            int y0 = iy + (int)((coreR + 2) * std::sin(a));
            int x1 = ix + (int)((coreR + 6) * std::cos(a));
            int y1 = iy + (int)((coreR + 6) * std::sin(a));
            ArtFX::glowLine(renderer, x0, y0, x1, y1, cr, cg, cb, 150, 1);
        }

        // 吸收中的蓝色能量悬浮在核心周围，提示"它在回血"。
        if (bonusHp > 0 || hot) {
            int rr = (int)(s * 0.58);
            for (int i = 0; i < 8; ++i) {
                double a = i * M_PI / 4.0 + moveTime * 0.45;
                int px = ix + (int)(rr * std::cos(a));
                int py = iy + (int)(rr * 0.55 * std::sin(a));
                ArtFX::starGlint(renderer, px, py, 1, 90, 180, 255, 75);
            }
        }
        // 肩部推进器微光。
        for (int side = -1; side <= 1; side += 2) {
            int px = ix + side * (s * 5 / 9);
            int py = iy + s / 16;
            ArtFX::glowLine(renderer, px, py, px + side * 5, py + 8, 255, 88, 68, 95, 1);
        }
    }

    void drawHPBar(SDL_Renderer* renderer, const Font& font) const {
        if (!active) return;
        ArtFX::BlendGuard guard(renderer);
        const int BAR_W = 400, BAR_H = 14;
        const int BAR_X = CENTER_X - BAR_W / 2, BAR_Y = 8;
        SDL_SetRenderDrawColor(renderer, 12, 7, 9, 235);
        SDL_Rect bg = {BAR_X - 2, BAR_Y - 2, BAR_W + 4, BAR_H + 4};
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 84, 31, 35, 220);
        SDL_Rect border = {BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2};
        SDL_RenderDrawRect(renderer, &border);

        int baseW = (int)((double)hp / (maxHp > 0 ? maxHp : 1) * BAR_W);
        if (baseW > BAR_W) baseW = BAR_W; if (baseW < 0) baseW = 0;
        int bonusW = (int)((double)bonusHp / (maxHp > 0 ? maxHp : 1) * BAR_W);
        if (baseW + bonusW > BAR_W) bonusW = BAR_W - baseW;
        if (baseW > 0) {
            SDL_SetRenderDrawColor(renderer, 188, 26, 35, 255);
            SDL_Rect baseR = {BAR_X, BAR_Y, baseW, BAR_H};
            SDL_RenderFillRect(renderer, &baseR);
            SDL_SetRenderDrawColor(renderer, 255, 92, 88, 180);
            SDL_Rect hi = {BAR_X, BAR_Y, baseW, 1};
            SDL_RenderFillRect(renderer, &hi);
        }
        if (bonusW > 0) {
            SDL_SetRenderDrawColor(renderer, 255, 145, 164, 255);
            SDL_Rect bonusR = {BAR_X + baseW, BAR_Y, bonusW, BAR_H};
            SDL_RenderFillRect(renderer, &bonusR);
            SDL_SetRenderDrawColor(renderer, 255, 220, 225, 145);
            SDL_Rect hi = {BAR_X + baseW, BAR_Y, bonusW, 1};
            SDL_RenderFillRect(renderer, &hi);
        }
        SDL_SetRenderDrawColor(renderer, 70, 13, 18, 145);
        for (int i = 1; i < 20; ++i)
            SDL_RenderDrawLine(renderer, BAR_X + BAR_W * i / 20, BAR_Y,
                               BAR_X + BAR_W * i / 20, BAR_Y + BAR_H - 1);
        if (cfg && cfg->name) {
            int nameLen = (int)strlen(cfg->name);
            int namePxW = nameLen * 6 * 2;
            int nameX = BAR_X - namePxW - 10;
            if (nameX < 4) nameX = 4;
            font.drawStringColor(renderer, cfg->name, nameX, BAR_Y + 1, 2,
                                 236, 208, 198, 255);
            SDL_SetRenderDrawColor(renderer, 255, 92, 88, 110);
            SDL_RenderDrawLine(renderer, nameX, BAR_Y + BAR_H + 1,
                               nameX + namePxW - 4, BAR_Y + BAR_H + 1);
        }
    }

    // Writable refs for game loop
    int& absorbTimerRef() { return absorbTimer; }
    int& postAbsorbTimerRef() { return postAbsorbTimer; }
    int& shakeTimerRef() { return shakeTimer; }
    int& shakeXRef() { return shakeX; }
    int& shakeYRef() { return shakeY; }
    int& flashTimerRef() { return flashTimer; }
    int& absorbCooldownRef() { return absorbCooldown; }
    bool& phase2TriggeredRef() { return phase2Triggered; }
    bool& enteringRef() { return entering; }
    AbsorbState& absorbStateRef() { return absorbState; }
    int& absorbTargetIdxRef() { return absorbTargetIdx; }
    void setCh1HealWavesEnabled(bool v) { healWavesEnabled = v; }
    void setLastHitBySW(int id) { lastHitBySW = id; }
};
