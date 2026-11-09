#pragma once

#include "art_fx.h"
#include "audio.h"
#include "constants.h"
#include "player.h"
#include "renderer.h"
#include "types.h"

// ============== BulletManager ==============
class BulletManager {
    std::vector<Ch1Bullet> bullets;
    int fireCooldown;
    int fireDelay;
    double bulletSpeed;
    static const int BASE_FIRE_DELAY = 7;
    static constexpr double BASE_BULLET_SPEED = 60.0;
    static constexpr double BULLET_RANGE = 370.0;

public:
    BulletManager() : fireCooldown(0), fireDelay(BASE_FIRE_DELAY), bulletSpeed(BASE_BULLET_SPEED) {}

    void reset() { bullets.clear(); fireCooldown = 0; updateParams(0); }

    void updateParams(int lv) {
        if (lv < 1) lv = 1;
        double factor = std::pow(0.9, lv - 1);
        fireDelay = (int)(BASE_FIRE_DELAY * factor);
        if (fireDelay < 3) fireDelay = 3;
        bulletSpeed = BASE_BULLET_SPEED * std::pow(1.1, lv - 1);
    }

    void addBullet(TrainingPlane& pl, AudioEngine* audio) {
        double tPlane = pl.getT();
        double spread = (rand() % 40 - 20) / 1000.0;
        double tBullet = tPlane + spread;
        if (tBullet < 0.0) tBullet = 0.0;
        if (tBullet > 1.0) tBullet = 1.0;

        double ty = pl.getY() - BULLET_RANGE;
        double tx = perspLeft(ty) + tBullet * perspWidth(ty);

        double dirX = tx - pl.getX();
        double dirY = ty - pl.getY();
        double len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 0.001) return;

        Ch1Bullet b;
        b.x = (double)pl.getX();
        b.y = (double)pl.getY();
        b.startX = pl.getX(); b.startY = pl.getY();
        b.dx = dirX / len; b.dy = dirY / len;
        b.active = true; b.canDamage = true;
        b.sideScroll = false; b.blueBeam = false; b.beamTargetIndex = -1;
        bullets.push_back(b);
        if (audio) audio->sndShoot();
    }

    void addBossBeam(double bossX, double bossY, double ax, double ay, int targetIdx) {
        Ch1Bullet beam;
        beam.x = bossX; beam.y = bossY;
        double bdx = ax - bossX, bdy = ay - bossY;
        double blen = std::sqrt(bdx*bdx + bdy*bdy);
        if (blen > 1.0) { beam.dx = bdx / blen; beam.dy = bdy / blen; }
        else { beam.dx = 0; beam.dy = -1; }
        beam.startX = bossX; beam.startY = bossY;
        beam.active = true; beam.canDamage = false; beam.sideScroll = false;
        beam.blueBeam = true; beam.beamTargetIndex = targetIdx;
        bullets.push_back(beam);
    }

    void addBulletSideScroll(Player& pl, AudioEngine* audio) {
        addBulletSideScrollAt(pl, 14, 0, audio);
    }
    void addBulletSideScrollAt(Player& pl, int ox, int oy, AudioEngine* audio) {
        Ch1Bullet b;
        b.x = (double)pl.getX() + ox;
        b.y = (double)pl.getY() + oy;
        b.startX = b.x; b.startY = b.y;
        double scatter = (rand() % 16 - 8) / 50.0;
        b.dx = 11.0; b.dy = scatter;
        b.active = true; b.canDamage = true;
        b.sideScroll = true;
        b.blueBeam = false; b.beamTargetIndex = -1;
        bullets.push_back(b);
        if (audio) audio->sndShoot();
    }

    bool canFire() const { return fireCooldown == 0; }
    void decrementCooldown() { if (fireCooldown > 0) fireCooldown--; }
    void setCooldown() { fireCooldown = fireDelay; }

    void update(const std::vector<Ch1Alien>& aliens) {
        for (auto& b : bullets) {
            if (!b.active) continue;
            if (b.sideScroll) {
                b.x += b.dx; b.y += b.dy;
                if (b.x > WIN_WIDTH + 30) b.active = false;
                continue;
            }
            if (b.blueBeam) {
                if (b.beamTargetIndex >= 0 && b.beamTargetIndex < (int)aliens.size()) {
                    const Ch1Alien& ta = aliens[b.beamTargetIndex];
                    if (ta.active) {
                        double ax = perspLeft(ta.y) + ta.t * perspWidth(ta.y);
                        double tdx = ax - b.x, tdy = ta.y - b.y;
                        double tlen = std::sqrt(tdx*tdx + tdy*tdy);
                        if (tlen > 1.0) { b.dx = tdx / tlen; b.dy = tdy / tlen; }
                    }
                }
                b.x += b.dx * 4.0;
                b.y += b.dy * 4.0;
                if (b.x < -60 || b.x > WIN_WIDTH + 60 || b.y < -60 || b.y > WIN_HEIGHT + 60)
                    b.active = false;
                continue;
            }
            double dist = std::sqrt((b.x - b.startX) * (b.x - b.startX) +
                                    (b.y - b.startY) * (b.y - b.startY));
            double remainRatio = 1.0 - dist / BULLET_RANGE;
            if (remainRatio <= 0.03) { b.active = false; continue; }
            if (remainRatio < 0.20) b.canDamage = false;
            double speed = bulletSpeed * remainRatio * remainRatio;
            b.x += b.dx * speed;
            b.y += b.dy * speed;
            if (b.x < -20 || b.x > WIN_WIDTH + 20 || b.y < -30 || b.y > WIN_HEIGHT + 10)
                b.active = false;
        }
    }

    void draw(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        for (const auto& b : bullets) {
            if (!b.active) continue;
            if (b.sideScroll) {
                // 侧滚弹：冷白核心 + 双层拖尾；靠近弹头时更亮更粗。
                int bx = (int)b.x, by = (int)b.y;
                int dy = (int)(b.dy * 3.0);
                ArtFX::glowLine(renderer, bx - 20, by - dy * 2, bx + 4, by, 76, 154, 255, 55, 1);
                ArtFX::glowLine(renderer, bx - 10, by - dy, bx + 10, by, 148, 222, 255, 125, 1);
                SDL_SetRenderDrawColor(renderer, 225, 249, 255, 245);
                SDL_RenderDrawLine(renderer, bx + 2, by, bx + 15, by);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawPoint(renderer, bx + 15, by);
                continue;
            }
            double dist = std::sqrt((b.x - b.startX) * (b.x - b.startX) +
                                    (b.y - b.startY) * (b.y - b.startY));
            double remainRatio = 1.0 - dist / BULLET_RANGE;
            if (remainRatio < 0.0) remainRatio = 0.0;
            double bodyLen = 18.0 * remainRatio;
            if (bodyLen < 2.0) bodyLen = 2.0;

            int alpha = 255;
            if (!b.canDamage && remainRatio < 0.15) {
                alpha = (int)(255.0 * (remainRatio - 0.03) / 0.12);
                if (alpha < 24) alpha = 24;
                if (alpha > 255) alpha = 255;
            }
            double hx = b.x + b.dx * bodyLen * 0.5;
            double hy = b.y + b.dy * bodyLen * 0.5;
            double tx = b.x - b.dx * bodyLen * 0.5;
            double ty = b.y - b.dy * bodyLen * 0.5;
            double trailLen = bodyLen * 1.25;
            double trx = tx - b.dx * trailLen;
            double try_ = ty - b.dy * trailLen;
            int trailAlpha = (int)(alpha * 0.30);
            if (trailAlpha < 10) trailAlpha = 10;

            int cr = b.blueBeam ? 94 : 220;
            int cg = b.blueBeam ? 194 : 239;
            int cb = b.blueBeam ? 255 : 235;
            ArtFX::glowLine(renderer, (int)tx, (int)ty, (int)trx, (int)try_,
                            b.blueBeam ? 56 : 120, b.blueBeam ? 144 : 178, 255,
                            trailAlpha, b.blueBeam ? 2 : 1);
            SDL_SetRenderDrawColor(renderer, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)alpha);
            SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)hx, (int)hy);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, (Uint8)std::min(255, alpha + 20));
            SDL_RenderDrawPoint(renderer, (int)hx, (int)hy);
            if (b.blueBeam) {
                // 吸收光束：额外的并行细线让它与普通玩家弹明显区分。
                SDL_SetRenderDrawColor(renderer, 88, 206, 255, (Uint8)(alpha / 2));
                SDL_RenderDrawLine(renderer, (int)tx, (int)ty - 2, (int)hx, (int)hy - 2);
                SDL_RenderDrawLine(renderer, (int)tx, (int)ty + 2, (int)hx, (int)hy + 2);
            }
        }
    }

    void removeInactive() {
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Ch1Bullet& b){ return !b.active; }), bullets.end());
    }

    // 存档：飞行中的子弹 + 射击冷却参数
    template <class Ar> void visit(Ar& ar) {
        ar.ioVecObj(bullets);
        ar.ioNum(fireCooldown); ar.ioNum(fireDelay); ar.ioNum(bulletSpeed);
    }

    std::vector<Ch1Bullet>& all() { return bullets; }
    int getFireDelay() const { return fireDelay; }
    double getBulletSpeed() const { return bulletSpeed; }
    static double getBulletRange() { return BULLET_RANGE; }
};
