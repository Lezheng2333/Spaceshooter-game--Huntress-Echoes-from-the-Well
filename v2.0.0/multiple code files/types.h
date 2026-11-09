#pragma once

#include <cmath>
#include <cstdio>


// ============== 共享数据结构 ==============
// 注：所有会被存档的状态结构体都实现 template<class Ar> void visit(Ar& ar)，
//     字段顺序即存档字节顺序，新增字段必须加在末尾（否则旧存档判为不兼容，
//     由 SAVE_FORMAT_VERSION 把关）。
struct Star {
    float x, y; float phase; float twinkleSpeed; float driftSpeed;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(phase); ar.ioNum(twinkleSpeed); ar.ioNum(driftSpeed);
    }
};

struct FloatingText {
    float x, y;
    int life, totalLife;
    char text[32];
    int r, g, b;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(life); ar.ioNum(totalLife);
        ar.ioBytes(text, sizeof(text));
        ar.ioNum(r); ar.ioNum(g); ar.ioNum(b);
    }
};

struct BossConfig {
    int maxHp;
    double moveAmplitudeX, moveAmplitudeY, moveFrequency, size;
    int enterDuration, absorbDuration, bonusHpPerAlien;
    int healWaveInterval, healHpPerWave, shakeDuration;
    bool hasCh1HealWaves, hasPhase2;
    const char* name;
};

struct ChapterConfig {
    int chapterNumber;
    const char* title;
    bool unlocked;
    int baseSpawnInterval;
    double baseAlienSpeed;
    int baseFireDelay;
    double baseBulletSpeed;
    int baseShockwaveDamage, baseShockwaveInterval;
    int bossTriggerScore;
    int alienTypesMask;
    double fastAlienChance, tankAlienChance;
    BossConfig bossConfig;
    int horizonY, groundColorR, groundColorG, groundColorB;
    int skyColorR, skyColorG, skyColorB;
    int starCount;
    float starBrightness;
    bool hasMeteorShowers, hasMovingBase, hasTimeLimit, isSideScrolling;
    int timeLimitSeconds;
};

struct FontChar { unsigned char rows[7]; };

struct ActiveSound {
    float freq, sweepEnd;
    int totalSamples, samplesLeft;
    float volume;
    int type, band;
    float phase;
    float filterLP;   // 仅音频内部使用：低通噪声/锯齿的滤波状态
};

struct MenuItem {
    const char* label;
    bool enabled;
    bool isToggle;
    bool* toggleValue;
    int* sliderValue;
    int sliderMin, sliderMax;
    bool sliderSymmetric;
    int actionId;
};


// === Chapter 1 data structures ===
struct Ch1Particle {
    double x, y, vx, vy;
    int life;
    bool active;
    bool whiteParticle, greenParticle, redParticle;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(vx); ar.ioNum(vy); ar.ioNum(life); ar.ioBool(active);
        ar.ioBool(whiteParticle); ar.ioBool(greenParticle); ar.ioBool(redParticle);
    }
};

struct BulletBase {
    double x, y, dx, dy;
    bool active;
    template <class Ar> void visitBase(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(dx); ar.ioNum(dy); ar.ioBool(active);
    }
};

struct Ch1Bullet : BulletBase {
    double startX, startY;
    bool canDamage, blueBeam;
    bool sideScroll;      // Ch2: simple rightward flight, no range decay
    int beamTargetIndex;
    template <class Ar> void visit(Ar& ar) {
        visitBase(ar);
        ar.ioNum(startX); ar.ioNum(startY); ar.ioBool(canDamage); ar.ioBool(blueBeam);
        ar.ioBool(sideScroll); ar.ioNum(beamTargetIndex);
    }
};

// === Enemy base traits (shared by all enemy types) ===
struct EnemyData {
    bool active, entering, defeated;
    int enterFrame, enterDuration;
    int invincibleFrames;
    int hp, maxHp;
    template <class Ar> void visitBase(Ar& ar) {
        ar.ioBool(active); ar.ioBool(entering); ar.ioBool(defeated);
        ar.ioNum(enterFrame); ar.ioNum(enterDuration); ar.ioNum(invincibleFrames);
        ar.ioNum(hp); ar.ioNum(maxHp);
    }
};

struct Ch1Alien : EnemyData {
    double y, t;
    double startT, targetT;
    double enterStartY, enterTargetY;
    bool enterFromTop, enterFromBoss;
    int lastHitBySW, lastHealHit;
    bool beingAbsorbed;
    int absorbFrame, absorbDuration;
    double absorbStartX, absorbStartY;
    int alienType;
    int behaviorData[4];
    template <class Ar> void visit(Ar& ar) {
        visitBase(ar);
        ar.ioNum(y); ar.ioNum(t); ar.ioNum(startT); ar.ioNum(targetT);
        ar.ioNum(enterStartY); ar.ioNum(enterTargetY);
        ar.ioBool(enterFromTop); ar.ioBool(enterFromBoss);
        ar.ioNum(lastHitBySW); ar.ioNum(lastHealHit);
        ar.ioBool(beingAbsorbed);
        ar.ioNum(absorbFrame); ar.ioNum(absorbDuration);
        ar.ioNum(absorbStartX); ar.ioNum(absorbStartY);
        ar.ioNum(alienType);
        ar.ioBytes(behaviorData, sizeof(behaviorData));
    }
};

struct Ch1Shockwave {
    double y; int id; bool active;
    template <class Ar> void visit(Ar& ar) { ar.ioNum(y); ar.ioNum(id); ar.ioBool(active); }
};

struct Ch1HealWave {
    double radius; int id; bool active;
    template <class Ar> void visit(Ar& ar) { ar.ioNum(radius); ar.ioNum(id); ar.ioBool(active); }
};


// === Chapter 2 enemy bullet (shared base) ===
struct Ch2EnemyBullet {
    double x, y, dx, dy;
    int hp; bool active;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(dx); ar.ioNum(dy); ar.ioNum(hp); ar.ioBool(active);
    }
};

// === Chapter 2 pulse wave ===
struct Ch2PulseWave {
    double x, y;      // origin center (player position at release)
    double radius;    // current radius
    int id;           // unique ID for per-enemy collision tracking
    bool active;
    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(x); ar.ioNum(y); ar.ioNum(radius); ar.ioNum(id); ar.ioBool(active);
    }
};

// === Chapter 2 danmaku data structures ===
struct Ch2DanmakuEnemy : EnemyData {
    double x, y;          // current center position
    double startX, startY, targetX, targetY;  // entrance trajectory
    double baseX, baseY;  // oscillation center (after entrance)
    double movePhase;     // oscillation phase (radians)
    double moveSpeed;     // per-enemy oscillation speed variation
    int leg, farLeg, nearLeg, vpLean;  // computed leg sizes for collision/draw
    int vulnTimer;         // counts down vulnerable time (0 = done)
    int fireTimer;
    int fireInterval;      // frames between spiral shots
    double fireAngle;      // current spiral angle
    int defeatTimer;       // countdown after defeat for animation
    int lastHitByPulse;    // pulse wave ID already applied to this enemy
    template <class Ar> void visit(Ar& ar) {
        visitBase(ar);
        ar.ioNum(x); ar.ioNum(y);
        ar.ioNum(startX); ar.ioNum(startY); ar.ioNum(targetX); ar.ioNum(targetY);
        ar.ioNum(baseX); ar.ioNum(baseY);
        ar.ioNum(movePhase); ar.ioNum(moveSpeed);
        ar.ioNum(leg); ar.ioNum(farLeg); ar.ioNum(nearLeg); ar.ioNum(vpLean);
        ar.ioNum(vulnTimer); ar.ioNum(fireTimer); ar.ioNum(fireInterval); ar.ioNum(fireAngle);
        ar.ioNum(defeatTimer); ar.ioNum(lastHitByPulse);
    }
};
