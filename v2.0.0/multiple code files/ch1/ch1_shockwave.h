#pragma once

#include "../audio.h"
#include "../constants.h"
#include "../floating_text.h"
#include "../particles.h"
#include "../player.h"
#include "../art_fx.h"
#include "../renderer.h"
#include "../types.h"

// ============== Ch1ShockwaveManager ==============
class Ch1ShockwaveManager {
    std::vector<Ch1Shockwave> shockwaves;
    int nextID;
    int timer;
    int interval;
    int damage;
    int kills;
    int lastLevel;
    bool pending;

public:
    Ch1ShockwaveManager() : nextID(0), timer(0), interval(720), damage(3), kills(0), lastLevel(0), pending(false) {}

    void reset() {
        shockwaves.clear(); nextID = 0; timer = 0;
        kills = 0; lastLevel = 0; pending = false;
        updateParams(0);
    }

    void updateParams(int lv) {
        if (lv < 1) lv = 1;
        damage = 3 + (lv - 1);
        int sec = 12 - lv * 2; if (sec < 1) sec = 1;
        interval = sec * 60;
    }

    void attemptAutoRelease(int score, Player& pl, FloatingTextManager& ft, AudioEngine* audio, ParticleManager* pm) {
        if (score < 30) return;
        int curLv = score / 30;
        if (curLv > lastLevel) {
            lastLevel = curLv;
            pending = true;
            timer = 0;
            if (curLv < 6)
                ft.spawn((float)pl.getX(), (float)(pl.getY() - 30), "LEVEL UP!");
        }
        if (pending) {
            pending = false;
            spawn(audio, pm);
        } else {
            timer++;
            if (timer >= interval) { timer = 0; spawn(audio, pm); }
        }
    }

    void spawn(AudioEngine* audio, ParticleManager* pm = nullptr) {
        if (audio) audio->sndShockwave();
        Ch1Shockwave sw;
        sw.y = WIN_HEIGHT;
        sw.id = nextID++;
        sw.active = true;
        shockwaves.push_back(sw);
        if (pm) {
            const double BA = WIN_WIDTH / 2.0 - 15.0;
            const double BB = 75.0;
            for (int i = 0; i < 40; ++i) {
                double t = (rand() % 1000) / 1000.0;
                double sx = CENTER_X + BA * (2.0 * t - 1.0);
                double ratio = (sx - CENTER_X) / BA;
                if (ratio > 1.0) ratio = 1.0;
                if (ratio < -1.0) ratio = -1.0;
                double sy = WIN_HEIGHT - BB * std::sqrt(1.0 - ratio * ratio);
                double nx = (sx - CENTER_X) / BA;
                double ny = (sy - WIN_HEIGHT) / BB;
                double nlen = std::sqrt(nx*nx + ny*ny);
                if (nlen < 0.01) { nx = 0; ny = -1; }
                else { nx /= nlen; ny /= nlen; }
                double speed = 1.5 + (rand() % 350) / 100.0;
                pm->spawnGreenParticle(
                    sx + (rand()%10-5), sy + (rand()%10-5),
                    nx * speed + (rand()%40-20)/20.0,
                    ny * speed + (rand()%40-20)/20.0,
                    20 + rand() % 25);
            }
        }
    }

    void update() {
        for (auto& sw : shockwaves) {
            if (!sw.active) continue;
            double progress = (WIN_HEIGHT - sw.y) / WIN_HEIGHT;
            if (progress > 1.0) progress = 1.0;
            double speed = 2.0 * (1.0 + progress * 5.0);
            sw.y -= speed;
            if (sw.y < -30) sw.active = false;
        }
    }

    void draw(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        const double BASE_B = 75.0;
        for (const auto& sw : shockwaves) {
            if (!sw.active) continue;
            double progress = (WIN_HEIGHT - sw.y) / WIN_HEIGHT;
            if (progress > 1.0) progress = 1.0;
            if (progress < 0.0) progress = 0.0;
            int alpha = (int)(255.0 * std::pow(1.0 - progress, 3.0));
            if (alpha < 8) alpha = 8;
            double swA = perspWidth(sw.y) / 2.0;
            double swB = BASE_B * swA / (perspWidth(WIN_HEIGHT) / 2.0);
            SDL_Point prev;
            // 主波前：外柔光 + 白绿核心 + 内侧第二道波。
            for (int pass = 0; pass < 2; ++pass) {
                double mul = pass == 0 ? 1.0 : 0.86;
                int aa = pass == 0 ? alpha : alpha / 2;
                SDL_SetRenderDrawColor(renderer, pass == 0 ? 50 : 175, 255,
                                       pass == 0 ? 100 : 196, (Uint8)aa);
                for (int i = 0; i <= 80; ++i) {
                    double t = (double)i / 80.0;
                    int sx = (int)(CENTER_X + (swA * mul) * (2.0 * t - 1.0));
                    double ratio = (double)(sx - CENTER_X) / (swA * mul);
                    if (ratio > 1.0) ratio = 1.0;
                    if (ratio < -1.0) ratio = -1.0;
                    int sy = (int)(sw.y - (swB * mul) * std::sqrt(1.0 - ratio * ratio));
                    if (i > 0) {
                        if (pass == 0)
                            ArtFX::glowLine(renderer, prev.x, prev.y, sx, sy, 54, 255, 112, aa, 1);
                        else
                            SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, sy);
                    }
                    prev = {sx, sy};
                }
            }
            // 波面上的五个高亮节点，帮助玩家读取波前的速度。
            double phase = progress * 10.0;
            for (int i = 0; i < 5; ++i) {
                double t = (i + 0.5) / 5.0;
                int sx = (int)(CENTER_X + swA * (2.0 * t - 1.0));
                double ratio = (double)(sx - CENTER_X) / swA;
                if (ratio > 1.0) ratio = 1.0;
                if (ratio < -1.0) ratio = -1.0;
                int sy = (int)(sw.y - swB * std::sqrt(1.0 - ratio * ratio));
                int aa = (int)(alpha * (0.45 + 0.55 * std::fabs(std::sin(phase + i))));
                ArtFX::starGlint(renderer, sx, sy, 1, 210, 255, 220, aa);
            }
        }
    }

    bool collideWithAlien(Ch1Alien& a, ParticleManager& pm, AudioEngine* audio, int& score) {
        for (auto& sw : shockwaves) {
            if (!sw.active) continue;
            if (a.lastHitBySW == sw.id) continue;
            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
            double swA = perspWidth(sw.y) / 2.0;
            double swB = 75.0 * swA / (perspWidth(WIN_HEIGHT) / 2.0);
            double ddx = (ax - CENTER_X) / swA;
            double ddy = (sw.y - a.y) / swB;
            if (ddx*ddx + ddy*ddy < 1.0) {
                a.lastHitBySW = sw.id;
                if (audio) audio->sndShockwaveHit();
                a.hp -= damage;
                pm.spawnExplosion(ax, a.y, 4);
                if (a.hp <= 0) {
                    a.active = false;
                    pm.spawnExplosion(ax, a.y, 22);
                    if (audio) audio->sndExplosionBig();
                    kills++;
                    if (kills >= 3) { kills -= 3; score++; }
                }
                return true;
            }
        }
        return false;
    }

    int collideWithBoss(double bossX, double bossY, int& bossLastHitBySW, int& bossBonusHp, int& bossHp,
                        int& bossFlashTimer, ParticleManager& pm, AudioEngine* audio) {
        int dmgDealt = 0;
        for (auto& sw : shockwaves) {
            if (!sw.active) continue;
            if (bossLastHitBySW == sw.id) continue;
            double dx = bossX - CENTER_X;
            double dy = bossY - sw.y;
            double swA = perspWidth(sw.y) / 2.0;
            double swB = 75.0 * swA / (perspWidth(WIN_HEIGHT) / 2.0);
            double ddx = dx / swA, ddy = dy / swB;
            if (ddx*ddx + ddy*ddy < 1.0 || (std::abs(dx) < swA && std::abs(dy) < swB*0.8)) {
                bossLastHitBySW = sw.id;
                bossFlashTimer = 8;
                if (audio) audio->sndBossHit();
                pm.spawnExplosion(bossX, bossY, 10);
                int dmg = damage;
                if (bossBonusHp > 0) {
                    int d = dmg < bossBonusHp ? dmg : bossBonusHp;
                    bossBonusHp -= d; dmg -= d;
                }
                if (dmg > 0) bossHp -= dmg;
                dmgDealt += damage;
                break;
            }
        }
        return dmgDealt;
    }

    void removeInactive() {
        shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(),
            [](const Ch1Shockwave& sw){ return !sw.active; }), shockwaves.end());
    }

    // 存档：冲击波 + 等级/伤害/计时/待释放标记
    // （timer/lastLevel/pending 必须一起存，否则读档瞬间会立刻放出一发冲击波）
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(shockwaves);
        ar.ioNum(nextID); ar.ioNum(timer); ar.ioNum(interval); ar.ioNum(damage);
        ar.ioNum(kills); ar.ioNum(lastLevel); ar.ioBool(pending);
    }

    std::vector<Ch1Shockwave>& all() { return shockwaves; }
    int getLevel() const { return damage - 2; }
    void setPending(bool p) { pending = p; }
};
