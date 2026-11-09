#pragma once

#include "../audio.h"
#include "../particles.h"
#include "../renderer.h"
#include "../types.h"

// ============== ChapterManager ==============
// ============== Ch2SkillOrb (floating skill orb → shield break → core → absorption) ==============
struct ShieldDebris {
    double sx, sy, vx, vy;
    int life;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(sx); ar.ioNum(sy); ar.ioNum(vx); ar.ioNum(vy); ar.ioNum(life);
    }
};

class Ch2SkillOrb {
public:
    enum State { INACTIVE, FLOATING, CORE, ABSORBING, ABSORBED };
    double x, y, vx, vy;
    double radius;       // 14px
    int shieldHp;        // 18 hits to break
    bool shieldBroken;
    State state;
    int absorbTimer;
    std::vector<ShieldDebris> shieldDebris;

    Ch2SkillOrb() : x(0), y(0), vx(0), vy(0), radius(14.0), shieldHp(18), shieldBroken(false),
                    state(INACTIVE), absorbTimer(0) {}

    // 存档：技能球位置/漂移速度/护罩血量/吸收进度 + 护罩碎片
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(vx); ar.ioNum(vy); ar.ioNum(radius);
        ar.ioNum(shieldHp); ar.ioBool(shieldBroken); ar.ioEnum(state); ar.ioNum(absorbTimer);
        ar.ioVecObj(shieldDebris);
    }

    void spawn(double sx, double sy) {
        x = sx; y = sy;
        vx = 0.6 + (rand()%80)/100.0;  // 0.6–1.4 px/frame
        vy = 0.8 + (rand()%60)/100.0;  // 0.8–1.4 px/frame
        if (rand()%2) vx = -vx; if (rand()%2) vy = -vy;
        radius = 14.0;
        shieldHp = 18; shieldBroken = false;
        shieldDebris.clear();
        state = FLOATING;
        absorbTimer = 0;
    }

    void update() {
        if (state == INACTIVE || state == ABSORBED) return;
        // Update shield debris
        for (auto& d : shieldDebris) {
            d.sx += d.vx; d.sy += d.vy;
            d.vx *= 0.96; d.vy *= 0.96;
            d.life--;
        }
        shieldDebris.erase(std::remove_if(shieldDebris.begin(), shieldDebris.end(),
            [](const ShieldDebris& d){ return d.life <= 0; }), shieldDebris.end());

        if (state == FLOATING) {
            x += vx; y += vy;
            if (x - radius < 10.0)  { x = 10.0 + radius; vx = std::abs(vx);  bounceAngle(false); }
            if (x + radius > 790.0) { x = 790.0 - radius; vx = -std::abs(vx); bounceAngle(true); }
            if (y - radius < 10.0)  { y = 10.0 + radius; vy = std::abs(vy);  bounceAngle(false); }
            if (y + radius > 590.0) { y = 590.0 - radius; vy = -std::abs(vy); bounceAngle(false); }
        }
    }

    void tickAbsorb() {
        if (state != ABSORBING) return;
        absorbTimer--;
        if (absorbTimer <= 0) state = ABSORBED;
    }

    void stopAbsorb() {
        if (state != ABSORBING) return;
        state = CORE;  // back to core if player releases Shift
    }

    void registerHit(ParticleManager& pm, AudioEngine& audio) {
        if (state != FLOATING || shieldBroken) return;
        shieldHp--;
        // Bullet shatter effect at impact point
        double ix = x + (rand()%12-6), iy = y + (rand()%12-6);
        pm.spawnExplosion(ix, iy, 6);
        for (int i = 0; i < 10; ++i) {
            double a = rand() % 6283 / 1000.0;
            double sp = 2.0 + (rand() % 300) / 100.0;
            pm.spawnWhiteParticle(ix, iy, std::cos(a)*sp, std::sin(a)*sp, 12+rand()%15);
        }
        audio.sndShieldHit();
        if (shieldHp <= 0) {
            breakShield(pm);
            audio.sndCrystalCrush();
        }
    }

    void breakShield(ParticleManager& pm) {
        shieldBroken = true;
        state = CORE;
        radius = 12.0;
        vx = 0; vy = 0;
        // 18-gon segments fly outward
        for (int i = 0; i < 18; ++i) {
            ShieldDebris d;
            double a = 2.0 * M_PI * i / 18.0;
            d.sx = x + 16.0 * std::cos(a);
            d.sy = y + 16.0 * std::sin(a);
            double speed = 1.5 + (rand()%250)/100.0;
            double spread = a + (rand()%30-15)*M_PI/180.0;
            d.vx = std::cos(spread) * speed;
            d.vy = std::sin(spread) * speed;
            d.life = 25 + rand() % 20;
            shieldDebris.push_back(d);
        }
        pm.spawnExplosion(x, y, 12);
    }

    void startAbsorb() {
        if (state != CORE) return;
        state = ABSORBING;
        absorbTimer = 300; // 5 seconds
    }

    bool isActive() const { return state != INACTIVE && state != ABSORBED; }
    bool isCore() const { return state == CORE || state == ABSORBING; }
    bool pulseUnlocked() const { return state == ABSORBED; }
    void reset() { state = INACTIVE; shieldDebris.clear(); shieldBroken = false; }

    void draw(SDL_Renderer* r) const {
        if (state == INACTIVE || state == ABSORBED) return;
        int cx = (int)x, cy = (int)y;
        // Shield debris
        for (const auto& d : shieldDebris) {
            int alpha = std::min(255, d.life * 10);
            SDL_SetRenderDrawColor(r, 220, 220, 255, (Uint8)alpha);
            SDL_RenderDrawLine(r, (int)d.sx, (int)d.sy, (int)(d.sx-d.vx*2), (int)(d.sy-d.vy*2));
        }
        // Shield shell (18-gon) — only when floating and not broken
        if (state == FLOATING && !shieldBroken) {
            double shR = radius + 5.0;
            SDL_Point poly[18];
            for (int i = 0; i < 18; ++i) {
                double a = 2.0 * M_PI * i / 18.0;
                poly[i] = {(int)(cx + shR * std::cos(a)), (int)(cy + shR * std::sin(a))};
            }
            int hullAlpha = 120 + (int)((double)shieldHp / 18.0 * 110);
            // Draw 3x to make thick visible lines
            for (int pass = 0; pass < 3; ++pass) {
                SDL_SetRenderDrawColor(r, 230, 230, 255, (Uint8)(hullAlpha / (pass+1)));
                for (int i = 0; i < 17; ++i)
                    SDL_RenderDrawLine(r, poly[i].x, poly[i].y, poly[i+1].x, poly[i+1].y);
                SDL_RenderDrawLine(r, poly[17].x, poly[17].y, poly[0].x, poly[0].y);
            }
            // Vertex dots
            SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)hullAlpha);
            for (int i = 0; i < 18; ++i) {
                SDL_RenderDrawPoint(r, poly[i].x, poly[i].y);
                SDL_RenderDrawPoint(r, poly[i].x-1, poly[i].y);
                SDL_RenderDrawPoint(r, poly[i].x+1, poly[i].y);
                SDL_RenderDrawPoint(r, poly[i].x, poly[i].y-1);
                SDL_RenderDrawPoint(r, poly[i].x, poly[i].y+1);
            }
        }
        // Inner glow core: pale yellow center → white edge (multi-layer feathering)
        int rsz = (int)radius;
        // Outer glow: 6 feathered layers with decreasing alpha
        for (int layer = 5; layer >= 0; --layer) {
            int lr = rsz + 4 + layer * 3;
            int al = 10 + layer * 25;  // 10, 35, 60, 85, 110, 135
            SDL_SetRenderDrawColor(r, 255, 255, (Uint8)(180 + layer * 12), (Uint8)al);
            int steps = 12 + layer * 4;
            for (int i = 0; i < steps; ++i) {
                double a = 2.0 * M_PI * i / steps;
                SDL_RenderDrawPoint(r, cx+(int)(lr*std::cos(a)), cy+(int)(lr*std::sin(a)));
            }
        }
        // Main core fill (yellow-white breathing, with edge gradient)
        // 呼吸亮度由存档内状态决定，保证读档后的同一帧像素一致。
        int breathSeed = (int)(x * 7.0 + y * 11.0) + shieldHp * 13 + absorbTimer * 3;
        int br = 180 + (int)(40.0 * std::sin(breathSeed * 0.07));
        if (state == CORE || state == ABSORBING) {
            br = 180 + (int)(75.0 * std::sin(breathSeed * 0.11));
        }
        for (int dy = -rsz; dy <= rsz; ++dy) {
            int dx = (int)std::sqrt((double)std::max(0, rsz*rsz - dy*dy));
            double edgeFrac = std::abs(dy) / (double)rsz;
            // Breathing brightness fades toward edges
            int rv = (int)(br - edgeFrac * 60 + 40);
            int gv = (int)(br - edgeFrac * 60 + 40);
            int bv = (int)(std::max(20, br/3 - (int)(edgeFrac * 30)));
            int al = (int)(240 - edgeFrac * 80);
            SDL_SetRenderDrawColor(r, (Uint8)std::min(255, std::max(0, rv)),
                (Uint8)std::min(255, std::max(0, gv)), (Uint8)std::min(255, std::max(0, bv)), (Uint8)al);
            SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
        }
        // Central hot dot
        SDL_SetRenderDrawColor(r, 255, 255, 220, 255);
        for (int i = 0; i < 12; ++i) {
            double a = i * M_PI / 6.0;
            SDL_RenderDrawPoint(r, cx+(int)(2*std::cos(a)), cy+(int)(2*std::sin(a)));
        }
        SDL_RenderDrawPoint(r, cx, cy);
    }

private:
    void bounceAngle(bool hitRight) {
        double speed = std::sqrt(vx*vx + vy*vy);
        if (speed < 0.3) { vx *= 2.0; vy *= 2.0; speed *= 2.0; }
        double angle = (45.0 + (rand()%46)) * M_PI / 180.0; // 45–90°
        double sx = hitRight ? -1.0 : 1.0;
        vx = std::cos(angle) * speed * sx;
        vy = std::sin(angle) * speed * (vy > 0 ? 1.0 : -1.0);
    }
};
