#pragma once

#include "font.h"
#include "renderer.h"
#include "art_fx.h"
#include "types.h"

// ============== ParticleManager ==============
class ParticleManager {
    std::vector<Ch1Particle> particles;
public:
    void spawnExplosion(double x, double y, int count) {
        for (int i = 0; i < count; ++i) {
            Ch1Particle p;
            p.x = x; p.y = y;
            double angle = (rand() % 360) * M_PI / 180.0;
            double speed = 1.2 + (rand() % 400) / 100.0;
            p.vx = std::cos(angle) * speed;
            p.vy = std::sin(angle) * speed;
            p.life = 15 + rand() % 20;
            p.active = true;
            p.whiteParticle = false;
            p.greenParticle = false;
            p.redParticle = false;
            particles.push_back(p);
        }
    }

    void spawnWhiteParticle(double x, double y, double vx, double vy, int life) {
        Ch1Particle p;
        p.x = x; p.y = y; p.vx = vx; p.vy = vy;
        p.life = life; p.active = true;
        p.whiteParticle = true;
        p.greenParticle = false; p.redParticle = false;
        particles.push_back(p);
    }

    void spawnGreenParticle(double x, double y, double vx, double vy, int life) {
        Ch1Particle p;
        p.x = x; p.y = y; p.vx = vx; p.vy = vy;
        p.life = life; p.active = true;
        p.whiteParticle = false;
        p.greenParticle = true; p.redParticle = false;
        particles.push_back(p);
    }

    void spawnRedParticle(double x, double y, double vx, double vy, int life) {
        Ch1Particle p;
        p.x = x; p.y = y; p.vx = vx; p.vy = vy;
        p.life = life; p.active = true;
        p.whiteParticle = false;
        p.greenParticle = false; p.redParticle = true;
        particles.push_back(p);
    }

    void spawnDigitShatter(const Font& font, char digit, int digitScale, int cx, int cy) {
        const FontChar& fc = font.getChar(digit);
        int dw = 5 * digitScale, dh = 7 * digitScale;
        int dx = cx - dw / 2, dy = cy - dh / 2;
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = fc.rows[row];
            for (int col = 0; col < 5; ++col) {
                if (!(bits & (1 << (4 - col)))) continue;
                double px = dx + col * digitScale + digitScale / 2.0;
                double py = dy + row * digitScale + digitScale / 2.0;
                int n = 4 + rand() % 3;
                for (int k = 0; k < n; ++k) {
                    Ch1Particle p;
                    p.x = px + (rand() % (digitScale + 1)) - digitScale / 2.0;
                    p.y = py + (rand() % (digitScale + 1)) - digitScale / 2.0;
                    double angle = (rand() % 360) * M_PI / 180.0;
                    double speed = 2.0 + (rand() % 500) / 100.0;
                    p.vx = std::cos(angle) * speed;
                    p.vy = std::sin(angle) * speed;
                    p.life = 22 + rand() % 24;
                    p.active = true;
                    p.whiteParticle = true;
                    particles.push_back(p);
                }
            }
        }
    }

    void spawnFireworks(double cx, double cy) {
        for (int k = 0; k < 12; ++k) {
            Ch1Particle p;
            double angle = (rand() % 360) * M_PI / 180.0;
            double speed = 2.5 + (rand() % 500) / 100.0;
            p.x = cx + (rand() % 240 - 120);
            p.y = cy - (rand() % 40);
            p.vx = std::cos(angle) * speed;
            p.vy = -std::fabs(std::sin(angle) * speed) - 1.5;
            p.life = 45 + rand() % 35;
            p.active = true;
            p.whiteParticle = (rand()%2 == 0);
            p.greenParticle = (rand()%2 == 0);
            p.redParticle = !p.whiteParticle && !p.greenParticle;
            particles.push_back(p);
        }
    }

    void update() {
        for (auto& p : particles) {
            if (!p.active) continue;
            p.x += p.vx; p.y += p.vy;
            p.vx *= 0.96; p.vy *= 0.96;
            p.life--;
            if (p.life <= 0) p.active = false;
        }
    }

    void draw(SDL_Renderer* renderer) const {
        ArtFX::BlendGuard guard(renderer);
        for (const auto& p : particles) {
            if (!p.active) continue;
            // 旧代码 life 可超过 35（烟花），fade 超过 1 会让颜色分量溢出；
            // clamp 后既修掉这个老问题，也让长寿命火花自然淡出。
            double fade = (double)p.life / 35.0;
            if (fade > 1.0) fade = 1.0;
            if (fade < 0.0) fade = 0.0;
            int brightness = (int)(255.0 * fade);
            int cr, cg, cb;
            bool bright = p.whiteParticle || p.greenParticle || p.redParticle;
            if (p.whiteParticle) {
                cr = brightness; cg = brightness; cb = brightness;
            } else if (p.greenParticle) {
                cr = (int)(brightness * 0.16); cg = brightness; cb = (int)(brightness * 0.42);
            } else if (p.redParticle) {
                cr = brightness; cg = (int)(brightness * 0.20); cb = (int)(brightness * 0.18);
            } else {
                cr = brightness; cg = (int)(brightness * 0.55); cb = (int)(brightness * 0.12);
            }
            int a = (int)(255.0 * fade);
            if (a < 12) a = 12;
            int x = (int)p.x, y = (int)p.y;
            // 运动拖尾：用速度反推一条短线，视觉上从方块升级为火花。
            int tx = x - (int)(p.vx * 0.35);
            int ty = y - (int)(p.vy * 0.35);
            if (std::abs(tx - x) + std::abs(ty - y) < 2) { tx = x - (int)(p.vx * 0.8); ty = y - (int)(p.vy * 0.8); }
            if (!bright || fade > 0.45) {
                SDL_SetRenderDrawColor(renderer, (Uint8)(cr / 2), (Uint8)(cg / 2), (Uint8)(cb / 2), (Uint8)(a / 2));
                SDL_RenderDrawLine(renderer, x, y, tx, ty);
            }
            SDL_SetRenderDrawColor(renderer, (Uint8)cr, (Uint8)cg, (Uint8)cb, (Uint8)a);
            int sz = bright ? (fade > 0.5 ? 3 : 2) : 2;
            SDL_Rect rect = {x - sz / 2, y - sz / 2, sz, sz};
            SDL_RenderFillRect(renderer, &rect);
            if (bright && fade > 0.7) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, (Uint8)(a / 2));
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }

    void removeInactive() {
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Ch1Particle& p){ return !p.active; }), particles.end());
    }

    // 存档：全部存活粒子（含寿命/速度/颜色标记）
    template <class Ar> void visit(Ar& ar) { ar.ioVecObj(particles); }

    std::vector<Ch1Particle>& all() { return particles; }
};
