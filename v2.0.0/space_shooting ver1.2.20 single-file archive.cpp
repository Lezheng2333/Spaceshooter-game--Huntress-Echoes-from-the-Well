#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>


// ============== 窗口 & 透视常量 ==============
const int WIN_WIDTH = 800;
const int WIN_HEIGHT = 600;
const int CENTER_X = WIN_WIDTH / 2;
const int HORIZON_Y = 200;
const double HORIZON_LEFT  = WIN_WIDTH * 0.25;
const double HORIZON_RIGHT = WIN_WIDTH * 0.75;

inline double perspLeft(double y) {
    return HORIZON_LEFT  * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspRight(double y) {
    return WIN_WIDTH - HORIZON_LEFT * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspWidth(double y) {
    return perspRight(y) - perspLeft(y);
}


// ============== 前置声明 ==============
class Font;
class Renderer;
class AudioEngine;
class FloatingTextManager;
class ParticleManager;
class TrainingPlane;
class Ch2Trainer;
class NightElf;
class BulletManager;
class Ch1ShockwaveManager;
class Ch1AlienManager;
class Ch1Boss;
class ChapterManager;
class Ch1Background;
class Ch2SphereBoss;
class Ch2Background;
class Ch2DanmakuManager;
class Ch2AlienManager;
class UIRenderer;
class MenuStateMachine;
class Game;


// ============== 共享数据结构 ==============
struct Star { float x, y; float phase; float twinkleSpeed; float driftSpeed; };

struct FloatingText {
    float x, y;
    int life, totalLife;
    char text[32];
    int r, g, b;
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
};

struct BulletBase {
    double x, y, dx, dy;
    bool active;
};

struct Ch1Bullet : BulletBase {
    double startX, startY;
    bool canDamage, blueBeam;
    bool sideScroll;      // Ch2: simple rightward flight, no range decay
    int beamTargetIndex;
};

// === Enemy base traits (shared by all enemy types) ===
struct EnemyData {
    bool active, entering, defeated;
    int enterFrame, enterDuration;
    int invincibleFrames;
    int hp, maxHp;
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
};

struct Ch1Shockwave { double y; int id; bool active; };

struct Ch1HealWave { double radius; int id; bool active; };


// === Chapter 2 enemy bullet (shared base) ===
struct Ch2EnemyBullet {
    double x, y, dx, dy;
    int hp; bool active;
};

// === Chapter 2 pulse wave ===
struct Ch2PulseWave {
    double x, y;      // origin center (player position at release)
    double radius;    // current radius
    int id;           // unique ID for per-enemy collision tracking
    bool active;
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
};


// ============== Font 类 ==============
class Font {
    FontChar chars[128];
public:
    Font() {
        auto setChar = [this](char c, const char* bits) {
            for (int r = 0; r < 7; ++r)
                chars[(int)c].rows[r] = (unsigned char)bits[r];
        };
        setChar('0', "\x0E\x11\x13\x15\x19\x11\x0E");
        setChar('1', "\x04\x0C\x04\x04\x04\x04\x0E");
        setChar('2', "\x0E\x11\x01\x06\x08\x10\x1F");
        setChar('3', "\x0E\x11\x01\x0E\x01\x11\x0E");
        setChar('4', "\x02\x06\x0A\x12\x1F\x02\x02");
        setChar('5', "\x1F\x10\x1E\x01\x01\x11\x0E");
        setChar('6', "\x0E\x10\x10\x1E\x11\x11\x0E");
        setChar('7', "\x1F\x01\x02\x04\x04\x08\x08");
        setChar('8', "\x0E\x11\x11\x0E\x11\x11\x0E");
        setChar('9', "\x0E\x11\x11\x0F\x01\x11\x0E");
        setChar('A', "\x0E\x11\x11\x1F\x11\x11\x11");
        setChar('B', "\x1E\x11\x11\x1E\x11\x11\x1E");
        setChar('C', "\x0E\x11\x10\x10\x10\x11\x0E");
        setChar('D', "\x1E\x11\x11\x11\x11\x11\x1E");
        setChar('E', "\x1F\x10\x10\x1E\x10\x10\x1F");
        setChar('F', "\x1F\x10\x10\x1E\x10\x10\x10");
        setChar('G', "\x0E\x11\x10\x17\x11\x11\x0E");
        setChar('H', "\x11\x11\x11\x1F\x11\x11\x11");
        setChar('I', "\x0E\x04\x04\x04\x04\x04\x0E");
        setChar('J', "\x07\x02\x02\x02\x02\x12\x0C");
        setChar('K', "\x11\x12\x14\x18\x14\x12\x11");
        setChar('L', "\x10\x10\x10\x10\x10\x10\x1F");
        setChar('M', "\x11\x1B\x15\x15\x11\x11\x11");
        setChar('N', "\x11\x19\x15\x13\x11\x11\x11");
        setChar('O', "\x0E\x11\x11\x11\x11\x11\x0E");
        setChar('P', "\x1E\x11\x11\x1E\x10\x10\x10");
        setChar('Q', "\x0E\x11\x11\x11\x15\x12\x0D");
        setChar('R', "\x1E\x11\x11\x1E\x14\x12\x11");
        setChar('S', "\x0E\x11\x10\x0E\x01\x11\x0E");
        setChar('T', "\x1F\x04\x04\x04\x04\x04\x04");
        setChar('U', "\x11\x11\x11\x11\x11\x11\x0E");
        setChar('V', "\x11\x11\x11\x11\x0A\x0A\x04");
        setChar('W', "\x11\x11\x11\x15\x15\x1B\x11");
        setChar('X', "\x11\x11\x0A\x04\x0A\x11\x11");
        setChar('Y', "\x11\x11\x0A\x04\x04\x04\x04");
        setChar('Z', "\x1F\x01\x02\x04\x08\x10\x1F");
        setChar('a', "\x00\x00\x0E\x01\x0F\x11\x0F");
        setChar('b', "\x10\x10\x1E\x11\x11\x11\x0E");
        setChar('c', "\x00\x00\x0E\x10\x10\x11\x0E");
        setChar('d', "\x01\x01\x0F\x11\x11\x11\x0F");
        setChar('e', "\x00\x00\x0E\x11\x1F\x10\x0E");
        setChar('f', "\x02\x04\x0E\x04\x04\x04\x0E");
        setChar('g', "\x00\x0F\x11\x0F\x01\x19\x0F");
        setChar('h', "\x10\x10\x16\x19\x11\x11\x11");
        setChar('i', "\x04\x00\x0C\x04\x04\x04\x0E");
        setChar('j', "\x02\x00\x06\x02\x02\x12\x0C");
        setChar('k', "\x10\x10\x12\x14\x18\x14\x12");
        setChar('l', "\x0C\x04\x04\x04\x04\x04\x0E");
        setChar('m', "\x00\x00\x1B\x15\x15\x11\x11");
        setChar('n', "\x00\x00\x1E\x11\x11\x11\x11");
        setChar('o', "\x00\x00\x0E\x11\x11\x11\x0E");
        setChar('p', "\x00\x00\x1E\x11\x11\x1E\x10");
        setChar('q', "\x00\x00\x0F\x11\x11\x0F\x01");
        setChar('r', "\x00\x00\x16\x19\x10\x10\x10");
        setChar('s', "\x00\x00\x0E\x10\x0E\x01\x1E");
        setChar('t', "\x08\x08\x1C\x08\x08\x09\x06");
        setChar('u', "\x00\x00\x11\x11\x11\x13\x0D");
        setChar('v', "\x00\x00\x11\x11\x0A\x0A\x04");
        setChar('w', "\x00\x00\x11\x15\x15\x1B\x11");
        setChar('x', "\x00\x00\x11\x0A\x04\x0A\x11");
        setChar('y', "\x00\x00\x11\x11\x0F\x01\x0E");
        setChar('z', "\x00\x00\x1F\x02\x04\x08\x1F");
        setChar(' ', "\x00\x00\x00\x00\x00\x00\x00");
        setChar('!', "\x04\x04\x04\x04\x00\x00\x04");
        setChar('"', "\x0A\x0A\x00\x00\x00\x00\x00");
        setChar('#', "\x0A\x0A\x1F\x0A\x1F\x0A\x0A");
        setChar('$', "\x04\x1E\x14\x0E\x05\x1E\x04");
        setChar('%', "\x11\x01\x02\x04\x08\x10\x11");
        setChar('&', "\x0C\x12\x14\x0C\x15\x12\x0D");
        setChar('\'', "\x04\x04\x00\x00\x00\x00\x00");
        setChar('(', "\x04\x08\x08\x08\x08\x08\x04");
        setChar(')', "\x04\x02\x02\x02\x02\x02\x04");
        setChar('*', "\x00\x0A\x1F\x1F\x0E\x04\x00");
        setChar('+', "\x00\x00\x04\x0E\x04\x00\x00");
        setChar(',', "\x00\x00\x00\x00\x00\x06\x0C");
        setChar('-', "\x00\x00\x00\x1F\x00\x00\x00");
        setChar('.', "\x00\x00\x00\x00\x00\x00\x04");
        setChar('/', "\x01\x02\x02\x04\x08\x08\x10");
        setChar(':', "\x00\x00\x04\x00\x00\x04\x00");
        setChar(';', "\x04\x00\x00\x00\x00\x06\x0C");
        setChar('<', "\x00\x02\x04\x08\x04\x02\x00");
        setChar('=', "\x00\x1F\x00\x1F\x00\x00\x00");
        setChar('>', "\x00\x08\x04\x02\x04\x08\x00");
        setChar('?', "\x0E\x11\x01\x02\x04\x00\x04");
        setChar('@', "\x0E\x11\x17\x15\x16\x10\x0E");
        setChar('[', "\x0E\x08\x08\x08\x08\x08\x0E");
        setChar('\\',"\x10\x08\x04\x02\x01\x00\x00");
        setChar(']', "\x0E\x02\x02\x02\x02\x02\x0E");
        setChar('^', "\x04\x0A\x11\x00\x00\x00\x00");
        setChar('_', "\x00\x00\x00\x00\x00\x00\x1F");
        setChar('`', "\x08\x04\x00\x00\x00\x00\x00");
        setChar('{', "\x03\x04\x04\x0C\x04\x04\x03");
        setChar('|', "\x04\x04\x04\x04\x04\x04\x04");
        setChar('}', "\x18\x04\x04\x06\x04\x04\x18");
        setChar('~', "\x00\x0A\x15\x00\x00\x00\x00");
    }

    void drawChar(SDL_Renderer* renderer, char c, int x, int y, int scale, int mul=1) const {
        const FontChar& fc = chars[(int)c];
        int s = scale * mul;
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = fc.rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_Rect r2 = {x + col * s, y + row * s, s, s};
                    SDL_RenderFillRect(renderer, &r2);
                }
            }
        }
    }

    void drawString(SDL_Renderer* renderer, const char* str, int x, int y, int scale, int mul=1) const {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int cx = x;
        int step = 6 * scale * mul;
        for (const char* p = str; *p; ++p) {
            if (*p == ' ') { cx += step; continue; }
            drawChar(renderer, *p, cx, y, scale, mul);
            cx += step;
        }
    }

    void drawCharFloat(SDL_Renderer* r, char c, float x, float y, float scale) const {
        const FontChar& fc = chars[(int)c];
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = fc.rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_FRect fr = {x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRectF(r, &fr);
                }
            }
        }
    }

    const FontChar& getChar(char c) const { return chars[(int)c]; }
};


// ============== Renderer 类 ==============
class Renderer {
    SDL_Renderer* sdlRenderer;
public:
    Renderer(SDL_Window* window) {
        sdlRenderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    ~Renderer() { if (sdlRenderer) SDL_DestroyRenderer(sdlRenderer); }
    SDL_Renderer* get() const { return sdlRenderer; }

    void setColor(int r, int g, int b, int a = 255) {
        SDL_SetRenderDrawColor(sdlRenderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    }
    void clear() { SDL_RenderClear(sdlRenderer); }
    void present() { SDL_RenderPresent(sdlRenderer); }
    void drawPoint(int x, int y) { SDL_RenderDrawPoint(sdlRenderer, x, y); }
    void drawLine(int x1, int y1, int x2, int y2) {
        SDL_RenderDrawLine(sdlRenderer, x1, y1, x2, y2);
    }
    void drawRect(const SDL_Rect* r) { SDL_RenderFillRect(sdlRenderer, r); }
    void drawRectBorder(const SDL_Rect* r) { SDL_RenderDrawRect(sdlRenderer, r); }
    void setTarget(SDL_Texture* tex) { SDL_SetRenderTarget(sdlRenderer, tex); }
    void resetTarget() { SDL_SetRenderTarget(sdlRenderer, NULL); }
    SDL_Texture* createTexture(Uint32 fmt, int access, int w, int h) {
        return SDL_CreateTexture(sdlRenderer, fmt, access, w, h);
    }
    void copyTexture(SDL_Texture* tex, const SDL_Rect* src, const SDL_Rect* dst) {
        SDL_RenderCopy(sdlRenderer, tex, src, dst);
    }
    void destroyTexture(SDL_Texture* tex) { SDL_DestroyTexture(tex); }
};


// ============== AudioEngine 类 ==============
class AudioEngine {
    SDL_AudioDeviceID audioDev;
    int bgmVolume, sfxVolume, eqLow, eqMid, eqHigh;
    bool bossMusicOn;
    volatile bool ch2Bgm;
    volatile bool bgmOff;       // silence BGM on start/pause screens
    std::vector<ActiveSound> activeSnd;

    // BGM state
    float bgmPhase, bgmPulsePhase;
    int bgmStepCounter, bgmNoteIndex;
    float bgmNoteFreq;
    int bgmNoteLen;

    // Ch2 BGM bass melody state
    int ch2BassIdx, ch2BassLen;
    float ch2BassFreq, ch2BassPhase;

    static const float BGM_MELODY[32];
    static const int BGM_NOTE_DUR[32];
    static const float BOSS_MELODY[16];
    static const int BOSS_DUR[16];
    static const float CH2_MELODY[126];
    static const int CH2_MEL_DUR[126];
    static const float CH2_BASS[64];
    static const int CH2_BASS_DUR[64];

    static void sdlAudioCB(void* userdata, Uint8* stream, int len) {
        ((AudioEngine*)userdata)->audioCB(stream, len);
    }

    void audioCB(Uint8* stream, int len) {
        float* buf = (float*)stream;
        int n = len / (int)sizeof(float);
        memset(buf, 0, len);

        bool bossFight = bossMusicOn;
        bool isCh2 = ch2Bgm && !bossFight;

        if (!bgmOff) {
        if (isCh2) {
            // ======== Chapter 2 BGM: Ambient pad style, ~60 BPM, C# major ========
            const int CH2_TICK = 700;
            const int CH2_NOTE_COUNT = 126;
            const int CH2_BASS_COUNT = 64;

            for (int i = 0; i < n; ++i) {
                // Melody: soft pad envelope (slow attack, full sustain, slow release)
                if (bgmNoteLen <= 0) {
                    bgmNoteIndex = (bgmNoteIndex + 1) % CH2_NOTE_COUNT;
                    bgmNoteLen = CH2_MEL_DUR[bgmNoteIndex] * CH2_TICK;
                    bgmNoteFreq = CH2_MELODY[bgmNoteIndex];
                }
                bgmNoteLen--;
                float melTotalLen = (float)(CH2_MEL_DUR[bgmNoteIndex] * CH2_TICK);
                float melT = (float)bgmNoteLen / melTotalLen;
                float melEnv;
                if (melT < 0.12f)
                    melEnv = melT / 0.12f;                      // soft release
                else if (melT > 0.85f)
                    melEnv = (1.0f - melT) / 0.15f;             // slow attack
                else
                    melEnv = 0.80f;                              // full gentle sustain

                bgmPhase += bgmNoteFreq / 44100.0f;
                if (bgmPhase > 1.0f) bgmPhase -= 2.0f;
                float melody = sinf(bgmPhase * 2.0f * M_PI) * melEnv * 0.050f;

                // Bass: deep ambient pad (smooth attack, long sustain)
                if (ch2BassLen <= 0) {
                    ch2BassIdx = (ch2BassIdx + 1) % CH2_BASS_COUNT;
                    ch2BassLen = CH2_BASS_DUR[ch2BassIdx] * CH2_TICK;
                    ch2BassFreq = CH2_BASS[ch2BassIdx];
                }
                ch2BassLen--;
                float bassTotalLen = (float)(CH2_BASS_DUR[ch2BassIdx] * CH2_TICK);
                float bassT = (float)ch2BassLen / bassTotalLen;
                float bassEnv;
                if (bassT < 0.10f)
                    bassEnv = bassT / 0.10f;                    // gentle release
                else if (bassT > 0.80f)
                    bassEnv = 0.40f + 0.60f * (1.0f - bassT) / 0.20f;  // smooth attack
                else
                    bassEnv = 0.85f;                             // warm sustain

                ch2BassPhase += ch2BassFreq / 44100.0f;
                if (ch2BassPhase > 2.0f) ch2BassPhase -= 2.0f;
                float bass = sinf(ch2BassPhase * 2.0f * M_PI) * bassEnv * 0.055f;

                buf[i] = (melody + bass) * (bgmVolume * bgmVolume / 70.0f);
                bgmStepCounter++;
            }
        } else {
            const float* melodyTbl = bossFight ? BOSS_MELODY : BGM_MELODY;
            const int*   durTbl    = bossFight ? BOSS_DUR    : BGM_NOTE_DUR;
            int noteCount = bossFight ? 16 : 32;
            int tickLen   = bossFight ? 450 : 800;

            for (int i = 0; i < n; ++i) {
                if (bgmNoteLen <= 0) {
                    bgmNoteIndex = (bgmNoteIndex + 1) % noteCount;
                    bgmNoteLen = durTbl[bgmNoteIndex] * tickLen;
                    bgmNoteFreq = melodyTbl[bgmNoteIndex];
                }
                bgmNoteLen--;
                float totalLen = (float)(durTbl[bgmNoteIndex] * tickLen);
                float t = (float)bgmNoteLen / totalLen;
                float env = 1.0f;
                if (t < 0.05f) env = t / 0.05f;
                else if (t > 0.75f) env = (1.0f - t) / 0.25f;

                bgmPhase += bgmNoteFreq / 44100.0f;
                if (bgmPhase > 1.0f) bgmPhase -= 2.0f;
                float melodyVol = bossFight ? 0.055f : 0.045f;
                float melody = sinf(bgmPhase * 2.0f * M_PI) * env * melodyVol;

                float bassFreq = bossFight ? 65.0f : 55.0f;
                float pulseSpeed = bossFight ? 0.0015f : 0.0005f;
                bgmPulsePhase += bassFreq / 44100.0f;
                if (bgmPulsePhase > 2.0f) bgmPulsePhase -= 2.0f;
                float pulse = 0.5f + 0.5f * sinf(bgmStepCounter * pulseSpeed);
                float bassVol = bossFight ? 0.038f : 0.030f;
                float bass = sinf(bgmPulsePhase * 2.0f * M_PI) * pulse * bassVol;

                buf[i] = (melody + bass) * (bgmVolume * bgmVolume / 70.0f);
                bgmStepCounter++;
            }
        }
        } // !bgmOff

        float eqGain[3];
        eqGain[0] = std::pow(10.0f, eqLow  / 20.0f);
        eqGain[1] = std::pow(10.0f, eqMid  / 20.0f);
        eqGain[2] = std::pow(10.0f, eqHigh / 20.0f);

        for (auto& s : activeSnd) {
            if (s.samplesLeft <= 0) continue;
            int process = n < s.samplesLeft ? n : s.samplesLeft;
            for (int i = 0; i < process; ++i) {
                float t = 1.0f - (float)s.samplesLeft / s.totalSamples;
                float env = 1.0f;
                if (t < 0.02f) env = t / 0.02f;
                else if (t > 0.55f) env = (1.0f - t) / 0.45f;

                float freq = s.freq;
                if (s.type == 3) freq = s.freq + (s.sweepEnd - s.freq) * t;

                s.phase += freq / 44100.0f;
                if (s.phase > 2.0f) s.phase -= 2.0f;

                float smp = 0.0f;
                if (s.type == 2)
                    smp = ((rand() % 2000) / 1000.0f - 1.0f);
                else if (s.type == 1)
                    smp = (sinf(s.phase * 2.0f * M_PI) > 0.0f ? 1.0f : -1.0f) * 0.5f;
                else
                    smp = sinf(s.phase * 2.0f * M_PI);

                buf[i] += smp * env * s.volume * (sfxVolume * sfxVolume / 70.0f) * eqGain[s.band];
                s.samplesLeft--;
            }
        }
        for (int i = 0; i < n; ++i) {
            if (buf[i] > 0.9f) buf[i] = 0.9f;
            if (buf[i] < -0.9f) buf[i] = -0.9f;
        }
        activeSnd.erase(std::remove_if(activeSnd.begin(), activeSnd.end(),
            [](const ActiveSound& s){ return s.samplesLeft <= 0; }), activeSnd.end());
    }

public:
    AudioEngine() : audioDev(0), bgmVolume(7), sfxVolume(7),
                    eqLow(0), eqMid(0), eqHigh(0), bossMusicOn(false),
                    ch2Bgm(false), bgmOff(false),
                    bgmPhase(0), bgmPulsePhase(0), bgmStepCounter(0),
                    bgmNoteIndex(0), bgmNoteFreq(0), bgmNoteLen(0),
                    ch2BassIdx(0), ch2BassLen(0), ch2BassFreq(0), ch2BassPhase(0) {
        SDL_AudioSpec want;
        SDL_memset(&want, 0, sizeof(want));
        want.freq = 44100;
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 1024;
        want.callback = sdlAudioCB;
        want.userdata = this;
        audioDev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
    }

    ~AudioEngine() { if (audioDev) SDL_CloseAudioDevice(audioDev); }

    void setBossMusic(bool on) { bossMusicOn = on; }
    void setCh2Bgm(bool on) { ch2Bgm = on; }
    void setBgmOff(bool off) { bgmOff = off; }
    void setStartBgm(bool on) { /* placeholder: start screen BGM interface */ (void)on; }

    void playSound(float freq, float sweepEnd, int durMs, float vol, int type, int band) {
        if (!audioDev) return;
        SDL_LockAudioDevice(audioDev);
        ActiveSound s;
        s.freq = freq; s.sweepEnd = sweepEnd;
        s.totalSamples = durMs * 44100 / 1000;
        s.samplesLeft = s.totalSamples;
        s.volume = vol; s.type = type; s.phase = 0.0f;
        s.band = band;
        activeSnd.push_back(s);
        SDL_UnlockAudioDevice(audioDev);
    }

    // 预定义 SFX
    void sndShoot()     { playSound(1200, 600, 28, 0.20f, 3, 2); }
    void sndHit()       { playSound(350, 0, 35, 0.22f, 1, 1); }
    void sndExplosionSmall() {
        playSound(600, 0, 35, 0.16f, 2, 2);    // high crisp noise crack
        playSound(1400, 0, 16, 0.09f, 0, 2);   // sine tap for "snap"
    }
    void sndExplosionBig()   {
        playSound(500, 0, 48, 0.12f, 2, 2);    // layered high noise
        playSound(200, 0, 40, 0.09f, 2, 1);    // mid noise fill
        playSound(1800, 400, 22, 0.07f, 3, 2); // quick high sweep (shattering)
    }
    void sndShockwave() { playSound(60, 150, 250, 0.30f, 3, 0); }
    void sndShockwaveHit()   { playSound(100, 0, 60, 0.25f, 0, 1); }
    void sndBossHeal()  { playSound(300, 600, 120, 0.22f, 3, 1); }
    void sndBossAbsorb() { playSound(200, 80, 200, 0.25f, 3, 0); }
    void sndBossEntrance() {
        playSound(60, 25, 700, 0.35f, 3, 0);
        playSound(150, 60, 550, 0.25f, 3, 0);
        playSound(280, 160, 350, 0.18f, 3, 0);
        playSound(180, 90, 500, 0.20f, 1, 1);
        playSound(40, 0, 120, 0.30f, 2, 0);
    }
    void sndBaseDamage() { playSound(45, 0, 150, 0.28f, 2, 0); }
    void sndBossHit() {
        playSound(280, 0, 28, 0.20f, 1, 2);
        playSound(80, 0, 35, 0.28f, 1, 0);
    }
    void sndShake()   { playSound(35, 0, 80, 0.12f, 2, 0); }
    void sndCrystalCrush() {
        playSound(2200, 500, 24, 0.15f, 1, 1);   // square sweep: 8-bit crystal shatter
        playSound(800, 0, 18, 0.12f, 3, 2);      // noise crunch: impact texture
        playSound(3600, 1200, 10, 0.06f, 0, 2);  // sine sparkle: bright glass top
    }
    void sndVillainTalk() {
        playSound(3500, 2400, 16, 0.09f, 3, 2);  // high sweep: radar chirp
        playSound(500, 0, 12, 0.05f, 0, 1);       // low sine body: radio depth
    }
    void sndTeletype()  { playSound(2400, 1800, 20, 0.10f, 3, 2); }
    void sndTripleOn()  { playSound(2600, 0, 22, 0.16f, 0, 2);    // crisp metallic ping
                          playSound(3200, 0, 14, 0.10f, 0, 2); }  // harmonic overtone
    void sndTripleCountdown() { playSound(1600, 0, 8, 0.08f, 0, 2); } // short beep
    void sndAllyTalk()  { playSound(3200, 2200, 14, 0.07f, 0, 2); }  // AI copilot: high sine blip
    void sndTowerTalk() { playSound(800, 400, 16, 0.08f, 3, 1); }   // tower AI: mid sweep
    void sndBryssaTalk(){ playSound(1500, 600, 12, 0.07f, 1, 2); }  // human comms: mid-high square
    void sndSystemTalk(){ playSound(1000, 0, 10, 0.05f, 0, 2); }    // system msg: pure sine ping
    void sndPlayerHit() { // heavy damage impact
        playSound(80, 0, 55, 0.22f, 2, 0);    // low thud
        playSound(200, 0, 40, 0.16f, 1, 1);   // mid impact
        playSound(40, 0, 70, 0.18f, 2, 0);    // deep rumble
    }

    int getBgmVolume() const { return bgmVolume; }
    int getSfxVolume() const { return sfxVolume; }
    int getEqLow() const { return eqLow; }
    int getEqMid() const { return eqMid; }
    int getEqHigh() const { return eqHigh; }

    void adjBgmVolume(int d) { bgmVolume += d; if (bgmVolume < 1) bgmVolume = 1; if (bgmVolume > 10) bgmVolume = 10; }
    void adjSfxVolume(int d) { sfxVolume += d; if (sfxVolume < 1) sfxVolume = 1; if (sfxVolume > 10) sfxVolume = 10; }
    void adjEqLow(int d) { eqLow += d; if (eqLow < -5) eqLow = -5; if (eqLow > 5) eqLow = 5; }
    void adjEqMid(int d) { eqMid += d; if (eqMid < -5) eqMid = -5; if (eqMid > 5) eqMid = 5; }
    void adjEqHigh(int d) { eqHigh += d; if (eqHigh < -5) eqHigh = -5; if (eqHigh > 5) eqHigh = 5; }
};

const float AudioEngine::BGM_MELODY[32] = {
    110,130,146,174,196,174,146,130,  110,130,146,174,196,220,196,174,
    130,146,174,196,220,196,174,146,  110,130,146,174,196,174,146,130
};
const int AudioEngine::BGM_NOTE_DUR[32] = {
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,12,12,
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,16,16
};
const float AudioEngine::BOSS_MELODY[16] = {
    196,220,261,294,330,294,261,220,  261,294,330,392,330,294,261,220
};
const int AudioEngine::BOSS_DUR[16] = {
    6,6,4,4,6,6,4,4,  6,4,4,6,8,6,4,8
};

// Chapter 2 BGM: 32s ambient pad loop — C# major, ~60 BPM
// 126 melody notes (16th notes) + 64 bass notes (8th notes)
const float AudioEngine::CH2_MELODY[126] = {
       0,    0,  415,  415,  277,  415,  277,    0,
       0,    0,    0,    0,  466,  466,  466,    0,
     466,  466,  349,  349,  349,  349,  349,  349,
     349,  349,  466,  466,  311,  311,  466,  466,
    1047,  349,  349,  349,  156,  156,  156,  156,
     349,  262,  175,  349,  175,  349,  175,  175,
       0,    0,    0,    0,  932,  932,  932,    0,
     932,  932,  932, 1047, 1047, 1047,  349,  349,
     349,  349,  349, 1047, 1047,  349,  175,  175,
     175,  175,  932,  932,  932,    0,    0,    0,
       0,    0,    0, 1047, 1047, 1047,  932,  932,
     932,  932,  415,  415,  415,  415,  415,  415,
     415,  415,  175,  175,  175,  175, 1047, 1047,
    1047,  175,  208,  208,  208,  208,  117,  117,
     117,  117,  415,  415,  175,  175,    0,    0,
       0,    0,    0,  415,  415,  277,
};
const int AudioEngine::CH2_MEL_DUR[126] = {
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,
};
const float AudioEngine::CH2_BASS[64] = {
     69,  69,  69,  69,  69,  69,  58,  58,
     58,  58,  65,  65,  65,  65,  69,  69,
     69,  69,  78,  78,  78,  78,  87,  87,
     87,  87,  73,  69,  69,  69,  69,  69,
     87,  87,  87,  87,  87,  87,  69,  69,
     69,  69,  69,  69,  87,  87,  69,  69,
     69,  69,  87,  87,  87,  87, 104,  58,
     58,  58,  69,  69,  69,  69,  69,  69,
};
const int AudioEngine::CH2_BASS_DUR[64] = {
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
};


// ============== FloatingTextManager ==============
class FloatingTextManager {
    std::vector<FloatingText> texts;
public:
    void spawn(float x, float y, const char* txt, int r=50, int g=255, int b_=80) {
        FloatingText ft;
        ft.x = x; ft.y = y;
        ft.life = 90; ft.totalLife = 90;
        ft.r = r; ft.g = g; ft.b = b_;
        snprintf(ft.text, sizeof(ft.text), "%s", txt);
        texts.push_back(ft);
    }

    void update() {
        for (auto& ft : texts) ft.y -= 0.7f;
        for (auto& ft : texts) ft.life--;
        texts.erase(std::remove_if(texts.begin(), texts.end(),
            [](const FloatingText& ft){ return ft.life <= 0; }), texts.end());
    }

    void draw(SDL_Renderer* renderer) const {
        for (const auto& ft : texts) {
            float t = (float)ft.life / ft.totalLife;
            if (t < 0.05f) continue;
            int r = (int)(ft.r * t), g = (int)(ft.g * t), b_ = (int)(ft.b * t);
            SDL_SetRenderDrawColor(renderer, r, g, b_, 255);
            // Use external font reference — handled in Game::draw
        }
    }

    const std::vector<FloatingText>& all() const { return texts; }
    void clear() { texts.clear(); }
};


// ============== NarrationSystem ==============
// Night Elf dark green color scheme
// Center narration: chapter intro/outro, blocking (gameplay paused)
// Left dialogue: in-game teammate hints, non-blocking (future use)
// Shared text drawing helper
static void drawTextLine(SDL_Renderer* r, const Font& font, const std::string& text,
                         int tx, int ty, int scale, int mul, int charW) {
    for (char c : text) {
        if (c == ' ') { tx += charW; continue; }
        font.drawChar(r, c, tx, ty, scale, mul);
        tx += charW;
    }
}

// ============== DialogueHistory ==============
class DialogueHistory {
public:
    struct Entry {
        std::string speaker;
        std::vector<std::string> lines;
        int numLines;
    };

    void add(const std::string& speaker, const std::vector<std::string>& lines, int numLines) {
        entries.push_back({speaker, lines, numLines});
    }
    const Entry& get(int i) const { return entries[i]; }
    const Entry& operator[](int i) const { return entries[i]; }
    int size() const { return (int)entries.size(); }
    void clear() { entries.clear(); scroll = 0; focusSlot = 2; }

    int scroll = 0;
    int focusSlot = 2;  // 0=top, 1=mid, 2=bottom

    void resetView() {
        scroll = 0;
        int total = size();
        focusSlot = (total >= 3) ? 2 : (total > 0 ? total - 1 : 2);
    }

    void moveUp() {
        int total = size();
        if (total == 0) return;
        int maxSlot = (total >= 3) ? 2 : total - 1;
        if (focusSlot > maxSlot) focusSlot = maxSlot;
        int bottomIdx = total - 1 - scroll;
        bool canScrollOlder = (bottomIdx > maxSlot);
        if (focusSlot > 0) focusSlot--;
        else if (canScrollOlder) scroll++;
    }
    void moveDown() {
        int total = size();
        if (total == 0) return;
        int maxSlot = (total >= 3) ? 2 : total - 1;
        if (focusSlot > maxSlot) focusSlot = maxSlot;
        if (focusSlot < maxSlot) focusSlot++;
        else if (scroll > 0) scroll--;
    }

private:
    std::vector<Entry> entries;
};

// ============== DialogueSystem ==============
class DialogueSystem {
    struct Line {
        std::string text;
        std::string speaker;
        bool sameSpeaker;
        bool historyRecorded;
        std::vector<std::string> vlines;
        int numLines;
        int state;       // 0=idle, 1=popin, 2=typewriter, 3=display, 4=fadeout
        int timer;
        int revealed;
        int typeTimer;
        float y;
        float fadeStartY;
    };

    static const int POPUP_FRAMES = 15;
    static const int FADE_FRAMES = 40;
    static const int WRAP_CHARS = 20;
    static const int TYPE_SPEED = 2;

    static double marioEase(double t) {
        if (t >= 1.0) return 1.0;
        if (t <= 0.0) return 0.0;
        const double c1 = 1.70158;
        const double c3 = c1 + 1.0;
        return 1.0 + c3 * std::pow(t - 1.0, 3) + c1 * std::pow(t - 1.0, 2);
    }

    std::vector<Line> queue;
    int idx = 0;
    bool active = false;
    bool enterWas = false;
    int ticks = 0;

public:
    DialogueHistory history;

    void queueDialogue(const char* speaker, const char* text) {
        Line l;
        l.text = text;
        l.speaker = speaker ? speaker : "";
        l.sameSpeaker = false;
        l.historyRecorded = false;
        l.state = 0; l.timer = 0; l.revealed = 0; l.typeTimer = 0;
        l.y = 200.0f; l.fadeStartY = 0;
        std::string s(text);
        while ((int)s.length() > WRAP_CHARS) {
            int brk = WRAP_CHARS;
            while (brk > 0 && s[brk] != ' ') brk--;
            if (brk == 0) brk = WRAP_CHARS;
            l.vlines.push_back(s.substr(0, brk));
            s = s.substr(brk + 1);
        }
        if (!s.empty()) l.vlines.push_back(s);
        l.numLines = (int)l.vlines.size();
        queue.push_back(l);
    }

    void start() {
        if (idx > 0) { queue.erase(queue.begin(), queue.begin() + idx); idx = 0; }
        if (queue.empty()) return;
        for (int i = 0; i < (int)queue.size(); ++i)
            queue[i].sameSpeaker = (i + 1 < (int)queue.size() && queue[i].speaker == queue[i+1].speaker);
        queue[idx].state = 1; queue[idx].timer = 0;
        queue[idx].revealed = 0; queue[idx].typeTimer = 0;
        queue[idx].y = 200.0f;
        active = true;
    }

    bool isActive() const { return active; }
    int popTicks() { int n = ticks; ticks = 0; return n; }
    const std::string& currentSpeaker() const {
        static std::string none;
        if (!active || idx >= (int)queue.size()) return none;
        return queue[idx].speaker;
    }

    void reset() {
        queue.clear(); idx = 0; active = false; enterWas = false; ticks = 0;
        history.clear();
    }

    void update(bool enterPressed) {
        if (!active || idx >= (int)queue.size()) { active = false; return; }
        auto& l = queue[idx];
        bool skip = enterPressed && !enterWas;
        int totalChars = 0;
        for (auto& vl : l.vlines) totalChars += (int)vl.length();

        switch (l.state) {
            case 1: // Pop-in
                l.timer++;
                if (skip || l.timer >= POPUP_FRAMES) {
                    l.state = 2; l.timer = 0;
                    if (!l.historyRecorded) {
                        l.historyRecorded = true;
                        history.add(l.speaker, l.vlines, l.numLines);
                    }
                }
                break;
            case 2: // Typewriter
                if (l.revealed < totalChars) {
                    if (skip) l.revealed = totalChars;
                    else { l.typeTimer++; if (l.typeTimer >= TYPE_SPEED) { l.typeTimer = 0; l.revealed++; ticks++; } }
                } else { l.state = 3; l.timer = 0; }
                break;
            case 3: // Display
                l.timer++;
                if (skip || l.timer >= 120) { l.state = 4; l.timer = 0; l.fadeStartY = l.y; }
                break;
            case 4: { // Fade out
                l.timer++;
                double t = (double)l.timer / FADE_FRAMES;
                l.y = l.fadeStartY - (float)(50.0 * (1.0 - std::pow(1.0 - t, 3.0)));
                if (skip || l.timer >= FADE_FRAMES) {
                    idx++;
                    if (idx >= (int)queue.size()) { active = false; enterWas = enterPressed; return; }
                    auto& next = queue[idx];
                    next.state = 1; next.timer = 0; next.revealed = 0; next.typeTimer = 0;
                    next.y = 200.0f;
                }
                break;
            }
        }
        enterWas = enterPressed;
    }

    void draw(SDL_Renderer* r, const Font& font) {
        if (!active || idx >= (int)queue.size()) return;
        auto& l = queue[idx];
        if (l.vlines.empty()) return;

        const int CH_W = 12, CH_H = 14, PAD_X = 14, PAD_Y = 10, LINE_GAP = 4;
        double bright = 1.0;
        int floatOff = 0;
        if (l.state == 4) {
            double t = (double)l.timer / FADE_FRAMES;
            bright = 1.0 - t; if (bright < 0.0) bright = 0.0;
            floatOff = (int)(50.0 * (1.0 - std::pow(1.0 - t, 3.0)));
        }
        int drawY = (int)l.y - floatOff;
        if (l.state == 1) return;

        const int TR = 50, TG = 155, TB = 70;
        int rr = (int)(TR * bright), gg = (int)(TG * bright), bb = (int)(TB * bright);
        int sr = (int)(180 * bright), sg = (int)(200 * bright), sb = (int)(160 * bright);
        int speakerH = l.speaker.empty() ? 0 : CH_H + 2;

        // Content
        int charsLeft = l.revealed;
        SDL_SetRenderDrawColor(r, (Uint8)rr, (Uint8)gg, (Uint8)bb, 255);
        for (int li = 0; li < l.numLines && charsLeft > 0; ++li) {
            int show = charsLeft;
            if (show > (int)l.vlines[li].length()) show = (int)l.vlines[li].length();
            drawTextLine(r, font, l.vlines[li].substr(0, show),
                         18 + PAD_X, drawY + PAD_Y + speakerH + li * (CH_H + LINE_GAP),
                         1, 2, CH_W);
            charsLeft -= show;
        }
        // Speaker (on top)
        if (!l.speaker.empty()) {
            SDL_SetRenderDrawColor(r, (Uint8)sr, (Uint8)sg, (Uint8)sb, 255);
            drawTextLine(r, font, l.speaker, 18 + PAD_X, drawY + PAD_Y, 1, 2, CH_W);
        }
    }
};

// ============== NarrationSystem (center narration only) ==============
class NarrationSystem {
    struct Line {
        std::vector<std::string> vlines;
        int numLines;
        int revealed;
        int popupTimer;
        int typeTimer;
    };

    static const int POPUP_FRAMES = 18;
    static const int TYPE_SPEED = 2;

    static double marioEase(double t) {
        if (t >= 1.0) return 1.0;
        if (t <= 0.0) return 0.0;
        const double c1 = 1.70158;
        const double c3 = c1 + 1.0;
        return 1.0 + c3 * std::pow(t - 1.0, 3) + c1 * std::pow(t - 1.0, 2);
    }

    std::vector<Line> lines;
    int curLine = 0;
    bool active = false;
    int bgAlpha = 0;
    int bgTimer = 0;
    bool enterWas = false;
    int ticks = 0;

public:
    void queue(const char* text) {
        Line l;
        l.revealed = 0; l.popupTimer = 0; l.typeTimer = 0;
        std::string s(text);
        const int maxChars = 36;
        std::vector<std::string> segs;
        size_t pos = 0;
        while (pos < s.length()) {
            size_t nl = s.find('\n', pos);
            if (nl == std::string::npos) nl = s.length();
            segs.push_back(s.substr(pos, nl - pos));
            pos = nl + 1;
        }
        for (auto& seg : segs) {
            while ((int)seg.length() > maxChars) {
                int brk = maxChars;
                while (brk > 0 && seg[brk] != ' ') brk--;
                if (brk == 0) brk = maxChars;
                l.vlines.push_back(seg.substr(0, brk));
                seg = seg.substr(brk + 1);
            }
            if (!seg.empty()) l.vlines.push_back(seg);
        }
        l.numLines = (int)l.vlines.size();
        lines.push_back(l);
    }

    void start() {
        if (lines.empty()) return;
        curLine = 0; active = true; bgAlpha = 0; bgTimer = 0; enterWas = true;
        for (auto& l : lines) { l.revealed = 0; l.popupTimer = 0; l.typeTimer = 0; }
    }

    bool isActive() const { return active; }
    int popTicks() { int n = ticks; ticks = 0; return n; }

    void reset() {
        lines.clear(); curLine = 0; active = false;
        bgAlpha = 0; bgTimer = 0; enterWas = false; ticks = 0;
    }

    void update(bool enterPressed) {
        if (!active || lines.empty()) {
            if (bgAlpha > 0) { bgTimer++; if (bgTimer >= 3) { bgAlpha -= 4; bgTimer = 0; } }
            return;
        }
        if (bgAlpha < 180) { bgTimer++; if (bgTimer >= 2) { bgAlpha += 4; bgTimer = 0; } }
        auto& cur = lines[curLine];
        if (cur.popupTimer < POPUP_FRAMES) { cur.popupTimer++; return; }
        int fullLen = 0;
        for (auto& vl : cur.vlines) fullLen += (int)vl.length();
        if (cur.revealed < fullLen) {
            cur.typeTimer++;
            if (cur.typeTimer >= TYPE_SPEED) { cur.typeTimer = 0; cur.revealed++; ticks++; }
            if (enterPressed && !enterWas) cur.revealed = fullLen;
            enterWas = enterPressed;
            return;
        }
        if (enterPressed && !enterWas) { curLine++; if (curLine >= (int)lines.size()) active = false; }
        enterWas = enterPressed;
    }

    void draw(SDL_Renderer* r, const Font& font) {
        if (!active && bgAlpha <= 0) return;
        if (lines.empty() || curLine >= (int)lines.size()) return;
        auto& cur = lines[curLine];
        int textScale = 3, charW = 18, charH = 21, lineGap = 6;
        int maxLineLen = 0;
        for (auto& vl : cur.vlines)
            if ((int)vl.length() > maxLineLen) maxLineLen = (int)vl.length();

        double popScale = marioEase((double)cur.popupTimer / POPUP_FRAMES);
        int boxW = maxLineLen * charW + 50;
        int boxH = cur.numLines * charH + (cur.numLines - 1) * lineGap + 40;
        int boxX = CENTER_X - boxW / 2, boxY = WIN_HEIGHT / 2 - boxH / 2;
        int drawW = (int)(boxW * popScale), drawH = (int)(boxH * popScale);
        int drawX = boxX + (boxW - drawW) / 2, drawY = boxY + (boxH - drawH) / 2;
        if (drawW < 10 || drawH < 10) return;

        const int TR = 50, TG = 155, TB = 70;
        Uint8 ba = (Uint8)(bgAlpha);
        SDL_SetRenderDrawColor(r, 10, 25, 15, (Uint8)(ba * 0.85));
        SDL_Rect bgRect = {drawX, drawY, drawW, drawH};
        SDL_RenderFillRect(r, &bgRect);
        SDL_SetRenderDrawColor(r, TR, TG, TB, (Uint8)(ba * 0.7));
        SDL_RenderDrawRect(r, &bgRect);
        if (cur.popupTimer < POPUP_FRAMES) return;

        SDL_SetRenderDrawColor(r, TR, TG, TB, 255);
        int charsLeft = cur.revealed;
        for (int li = 0; li < cur.numLines && charsLeft > 0; ++li) {
            int show = charsLeft;
            if (show > (int)cur.vlines[li].length()) show = (int)cur.vlines[li].length();
            drawTextLine(r, font, cur.vlines[li].substr(0, show),
                         drawX + 25, drawY + 22 + li * (charH + lineGap), 3, 1, charW);
            charsLeft -= show;
        }
    }
};


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
        for (const auto& p : particles) {
            if (!p.active) continue;
            double fade = (double)p.life / 35.0;
            if (fade < 0.0) fade = 0.0;
            int brightness = (int)(255 * fade);
            if (p.whiteParticle) {
                SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            } else if (p.greenParticle) {
                SDL_SetRenderDrawColor(renderer, (int)(brightness * 0.2), brightness, (int)(brightness * 0.3), 255);
            } else if (p.redParticle) {
                SDL_SetRenderDrawColor(renderer, brightness, (int)(brightness * 0.2), (int)(brightness * 0.2), 255);
            } else {
                int r = brightness, g = (int)(brightness * 0.55), b = (int)(brightness * 0.1);
                SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            }
            int sz = (int)((p.whiteParticle || p.greenParticle) ? 5.0 * fade : 3.5 * fade);
            if (sz < 1) sz = 1;
            SDL_Rect rect = {(int)(p.x - sz/2), (int)(p.y - sz/2), sz, sz};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    void removeInactive() {
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Ch1Particle& p){ return !p.active; }), particles.end());
    }

    std::vector<Ch1Particle>& all() { return particles; }
};


// ============== AimAssist (owned by Player) ==============
class AimAssist {
    double snapProgress;
    static void updateProgress(double& sp, bool hasTarget) {
        if (hasTarget) sp += 0.05; else sp -= 0.12;
        if (sp > 1.0) sp = 1.0; if (sp < 0.0) sp = 0.0;
    }
public:
    AimAssist() : snapProgress(0) {}
    void update(bool hasTarget) { updateProgress(snapProgress, hasTarget); }
    double getSnapProgress() const { return snapProgress; }
    void draw(SDL_Renderer* r, double drawX, double drawY, int dotBig, int dotSmall) const {
        double ss = 8.0 * (1.0 - snapProgress);
        if (ss > 0.5) {
            SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)(140 * (1.0 - snapProgress)));
            int sx = (int)(drawX - ss), side = (int)(ss * 2);
            int gapPx = side * 6 / 10; if (gapPx < 2) gapPx = 2;
            int edgePx = (side - gapPx) / 2; if (edgePx < 1) edgePx = 1;
            int sy = (int)(drawY - ss);
            if (side >= 3) {
                SDL_RenderDrawLine(r, sx, sy, sx+edgePx, sy);
                SDL_RenderDrawLine(r, sx+edgePx+gapPx, sy, sx+side, sy);
                SDL_RenderDrawLine(r, sx, sy+side, sx+edgePx, sy+side);
                SDL_RenderDrawLine(r, sx+edgePx+gapPx, sy+side, sx+side, sy+side);
                SDL_RenderDrawLine(r, sx, sy, sx, sy+edgePx);
                SDL_RenderDrawLine(r, sx, sy+edgePx+gapPx, sx, sy+side);
                SDL_RenderDrawLine(r, sx+side, sy, sx+side, sy+edgePx);
                SDL_RenderDrawLine(r, sx+side, sy+edgePx+gapPx, sx+side, sy+side);
            }
        }
        int dotSize = (snapProgress > 0.5) ? dotBig : dotSmall;
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_Rect dot = {(int)(drawX - dotSize/2), (int)(drawY - dotSize/2), dotSize, dotSize};
        SDL_RenderFillRect(r, &dot);
    }
};

// ============== Player (shared player state) ==============
class Player {
protected:
    int x, y;
    double rollAngle, rollTarget;
    int lastMoveDir;
    int invFrames;  // invincibility after taking damage
    static constexpr double ROLL_SPEED = 0.314;
    static const int HALF_WIDTH = 15;

public:
    AimAssist aimAssist;  // every player has aim assist

    Player() : x(CENTER_X), y(WIN_HEIGHT - 80), rollAngle(0), rollTarget(0), lastMoveDir(0), invFrames(0) {}

    int getX() const { return x; }
    int getY() const { return y; }
    double getRollAngle() const { return rollAngle; }
    int getInvFrames() const { return invFrames; }
    void setX(int nx) { x = nx; }
    void setY(int ny) { y = ny; }
    void setRollTarget(double rt) { rollTarget = rt; }
    void setRollAngle(double ra) { rollAngle = ra; }
    void setInvFrames(int f) { invFrames = f; }
    void updateInvFrames() { if (invFrames > 0) invFrames--; }
    void resetState() {
        rollAngle = 0; rollTarget = 0; lastMoveDir = 0;
        invFrames = 0;
    }
    virtual void draw(SDL_Renderer*) const {}
    virtual void handleInput(const Uint8*) {}
    virtual int getGunCount() const { return 1; }
    virtual void getGunOffset(int, int& ox, int& oy) const { ox = 14; oy = 0; }
    virtual int getNoseOffset() const { return 14; }
};

// ============== TrainingPlane (Chapter 1 perspective movement) ==============
class TrainingPlane : public Player {
public:
    TrainingPlane() : Player() {}

    void draw(SDL_Renderer* r) const {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        int px = getX(), py = getY();
        double tPlane = (px - perspLeft(py)) / perspWidth(py);
        if (tPlane < 0.0) tPlane = 0.0; if (tPlane > 1.0) tPlane = 1.0;
        double hx = perspLeft(HORIZON_Y) + tPlane * perspWidth(HORIZON_Y);
        double hdx = hx - px, hdy = HORIZON_Y - py;
        if (hdy > 0) { hdx = 0; hdy = -1; }
        double hlen = std::sqrt(hdx*hdx + hdy*hdy);
        if (hlen < 0.01) { hdx = 0; hdy = -1; hlen = 1; }
        double hnx = hdx / hlen, hny = hdy / hlen;
        double c = std::cos(getRollAngle()), s = std::sin(getRollAngle());

        auto hr = [&](int lx, int ly) -> SDL_Point {
            return {px + (int)(lx * (-hny) - ly * hnx),
                    py + (int)(lx * hnx  + ly * (-hny))};
        };
        auto hrr = [&](int lx, int ly) -> SDL_Point {
            double rx = lx * c, ry = ly + lx * s * 0.40;
            return {px + (int)(rx * (-hny) - ry * hnx),
                    py + (int)(rx * hnx  + ry * (-hny))};
        };

        SDL_Point nose = hr(0, -12), leftB = hr(-10, 6), rightB = hr(10, 6);
        SDL_Point body[3] = {nose, leftB, rightB};
        SDL_RenderDrawLines(r, body, 3);
        SDL_RenderDrawLine(r, leftB.x, leftB.y, rightB.x, rightB.y);
        SDL_Point lw1 = hrr(-6, 0), lw2 = hrr(-14, 4);
        SDL_RenderDrawLine(r, lw1.x, lw1.y, lw2.x, lw2.y);
        SDL_Point rw1 = hrr(6, 0),  rw2 = hrr(14, 4);
        SDL_RenderDrawLine(r, rw1.x, rw1.y, rw2.x, rw2.y);
        SDL_Point tail1 = hr(0, 6), tail2 = hr(0, 12);
        SDL_RenderDrawLine(r, tail1.x, tail1.y, tail2.x, tail2.y);
    }

    void reset() {
        x = CENTER_X; y = WIN_HEIGHT - 80;
        resetState();
    }

    void handleInput(const Uint8* keys) {
        bool moveLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        bool moveRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        bool moveUp    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool moveDown  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

        // Perspective speed scaling: slower near horizon, faster near bottom
        const double REF_Y = 400.0;
        double refWidth = perspWidth(REF_Y);
        double speedFactor = perspWidth(y) / (refWidth > 0 ? refWidth : 1.0);
        int hStep = (int)(7.0 * speedFactor + 0.5);
        int vStep = (int)(6.0 * speedFactor + 0.5);

        if (moveLeft)  x -= hStep;
        if (moveRight) x += hStep;

        double minX = perspLeft(y) + 18;
        double maxX = perspRight(y) - 18;
        if (x < minX) x = (int)minX;
        if (x > maxX) x = (int)maxX;

        if (moveUp)    y -= vStep;
        if (moveDown)  y += vStep;
        if (y < 220) y = 220;
        if (y > WIN_HEIGHT - 30) y = WIN_HEIGHT - 30;

        int curDir = 0;
        if (moveRight && !moveLeft) curDir = 1;
        else if (moveLeft && !moveRight) curDir = -1;
        if (curDir != 0 && lastMoveDir != curDir)
            rollTarget += (curDir > 0 ? 2.0 * M_PI : -2.0 * M_PI);
        lastMoveDir = curDir;

        double diff = rollTarget - rollAngle;
        if (std::fabs(diff) > 0.005) {
            if (std::fabs(diff) < ROLL_SPEED) rollAngle = rollTarget;
            else rollAngle += (diff > 0 ? ROLL_SPEED : -ROLL_SPEED);
        }
    }

    double getT() const {
        double t = (x - perspLeft(y)) / perspWidth(y);
        if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
        return t;
    }
};

// ============== Ch2Trainer (Chapter 2 side-scrolling trainer) ==============
// Same aircraft as Ch1 TrainingPlane, rotated 90 deg for side-scrolling view.
// Ch1 raw coords (nose-up top-down): nose(0,-12) leftB(-10,6) rightB(10,6)
//   wings lw1(-6,0)→lw2(-14,4)  rw1(6,0)→rw2(14,4)  tail(0,6)→(0,12)
// Ch2 rotated (x,y)→(-y,x), same scale as Ch1:
//   nose(12,0)  topB(-6,-10)  bottomB(-6,10)
//   uw(0,-6)→uw2(-4,-14)  lw(0,6)→lw2(-4,14)  tail(-6,0)→(-12,0)
class Ch2Trainer : public Player {
public:
    Ch2Trainer() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        // Triangular body (same as Ch1 trainer, rotated -90 deg, 1:1 scale)
        SDL_Point nose    = {px + 12, py};
        SDL_Point topB    = {px - 6, py - 10};
        SDL_Point bottomB = {px - 6, py + 10};
        SDL_Point body[3] = {nose, topB, bottomB};
        SDL_RenderDrawLines(r, body, 3);
        SDL_RenderDrawLine(r, topB.x, topB.y, bottomB.x, bottomB.y);

        // Upper wing (Ch1 left wing, rotated)
        SDL_Point uw1 = {px, py - 6};
        SDL_Point uw2 = {px - 4, py - 14};
        SDL_RenderDrawLine(r, uw1.x, uw1.y, uw2.x, uw2.y);

        // Lower wing (Ch1 right wing, rotated)
        SDL_Point lw1 = {px, py + 6};
        SDL_Point lw2 = {px - 4, py + 14};
        SDL_RenderDrawLine(r, lw1.x, lw1.y, lw2.x, lw2.y);

        // Tail (Ch1 center tail, rotated)
        SDL_Point tail1 = {px - 6, py};
        SDL_Point tail2 = {px - 12, py};
        SDL_RenderDrawLine(r, tail1.x, tail1.y, tail2.x, tail2.y);
    }

    int getGunCount() const override { return 1; }
    void getGunOffset(int idx, int& ox, int& oy) const override {
        ox = 12; oy = 0;
    }
    int getNoseOffset() const override { return 12; }
};

// ============== NightElf (plane 2, Ch2 default) ==============
class NightElf : public Player {
public:
    NightElf() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const override {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        int wingX = px + 2, wingSpan = 8;
        int noseX = wingX + 30;          // 30° nose: tan(15°)=8/30≈0.27
        int tailX = wingX + 5;           // 120° tail, reflected across wing line

        // Body outline: nose → upper wing → tail → lower wing → nose
        SDL_Point body[5] = {
            {noseX, py},
            {wingX, py - wingSpan},
            {tailX, py},
            {wingX, py + wingSpan},
            {noseX, py}
        };
        SDL_RenderDrawLines(r, body, 5);

        // Parallel lines from wing tips forward, slightly shorter than nose
        SDL_RenderDrawLine(r, wingX, py - wingSpan, noseX - 3, py - wingSpan);
        SDL_RenderDrawLine(r, wingX, py + wingSpan, noseX - 3, py + wingSpan);

        // Fill upper half with correct lower boundary
        for (int sx = wingX; sx <= noseX; sx += 2) {
            double tn = (double)(sx - wingX) / (noseX - wingX);
            int uy = (py - wingSpan) + (int)(wingSpan * tn);
            int ly;
            if (sx <= tailX) {
                double tt = (double)(sx - wingX) / (tailX - wingX);
                ly = (py - wingSpan) + (int)(wingSpan * tt);
            } else {
                ly = py;
            }
            SDL_RenderDrawLine(r, sx, uy, sx, ly);
        }
    }

    int getGunCount() const override { return 1; } // single-fire for now
    void getGunOffset(int idx, int& ox, int& oy) const override {
        if (idx == 0)      { ox = 32; oy = 0; }    // nose tip
        else if (idx == 1) { ox = 29; oy = -8; }   // upper wing tip
        else               { ox = 29; oy = 8; }    // lower wing tip
    }
    int getNoseOffset() const override { return 32; }
};

// ============== Druid (plane 3, saved design) ==============
class Druid : public Player {
public:
    Druid() : Player() {}

    void reset() {
        x = 100; y = WIN_HEIGHT / 2;
        resetState();
    }

    void draw(SDL_Renderer* r) const {
        if (getInvFrames() > 0 && (getInvFrames() / 4) % 2 == 0) return;
        int px = getX(), py = getY();
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        int noseLen = 14, tailLen = 10, wingSpan = 8;
        int noseX = px + noseLen, tailX = px - tailLen;

        // Swallowtail body: nose(60°) → wings → tail tips → 90° notch → back
        SDL_Point body[7] = {
            {noseX, py},                       // 0: nose tip
            {px, py - wingSpan},               // 1: upper wing
            {tailX, py - wingSpan/2},          // 2: upper tail tip
            {tailX + 4, py},                   // 3: notch center (90° V)
            {tailX, py + wingSpan/2},          // 4: lower tail tip
            {px, py + wingSpan},               // 5: lower wing
            {noseX, py}                        // 6: back to nose
        };
        SDL_RenderDrawLines(r, body, 7);

        // Parallel lines from wing tips toward nose (shorter than nose)
        SDL_RenderDrawLine(r, px, py - wingSpan, px + 11, py - wingSpan);
        SDL_RenderDrawLine(r, px, py + wingSpan, px + 11, py + wingSpan);

        // Center guide line (navigation arrow feel)
        SDL_RenderDrawLine(r, px, py, noseX, py);
    }
};


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
        for (const auto& b : bullets) {
            if (!b.active) continue;
            if (b.sideScroll) {
                // Rightward bullet: white line + short trail
                SDL_SetRenderDrawColor(renderer, 80, 80, 80, 100);
                SDL_RenderDrawLine(renderer, (int)b.x-8, (int)b.y, (int)b.x-2, (int)b.y);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(renderer, (int)b.x-2, (int)b.y, (int)b.x+14, (int)b.y);
                continue;
            }
            double dist = std::sqrt((b.x - b.startX) * (b.x - b.startX) +
                                    (b.y - b.startY) * (b.y - b.startY));
            double remainRatio = 1.0 - dist / BULLET_RANGE;
            if (remainRatio < 0.0) remainRatio = 0.0;
            double bodyLen = 16.0 * remainRatio;
            if (bodyLen < 1.5) bodyLen = 1.5;

            int alpha = 255;
            if (!b.canDamage && remainRatio < 0.15) {
                alpha = (int)(255.0 * (remainRatio - 0.03) / 0.12);
                if (alpha < 20) alpha = 20;
                if (alpha > 255) alpha = 255;
            }

            double hx = b.x + b.dx * bodyLen * 0.5;
            double hy = b.y + b.dy * bodyLen * 0.5;
            double tx = b.x - b.dx * bodyLen * 0.5;
            double ty = b.y - b.dy * bodyLen * 0.5;
            double trailLen = bodyLen * 0.7;
            double trx = tx - b.dx * trailLen;
            double try_ = ty - b.dy * trailLen;
            int trailAlpha = (int)(alpha * 0.35);
            if (trailAlpha < 8) trailAlpha = 8;

            if (b.blueBeam)
                SDL_SetRenderDrawColor(renderer, 60, 160, 255, (unsigned char)alpha);
            else
                SDL_SetRenderDrawColor(renderer, 90, 90, 90, (unsigned char)trailAlpha);
            SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)trx, (int)try_);
            if (b.blueBeam)
                SDL_SetRenderDrawColor(renderer, 100, 200, 255, (unsigned char)alpha);
            else
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, (unsigned char)alpha);
            SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)hx, (int)hy);
        }
    }

    void removeInactive() {
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Ch1Bullet& b){ return !b.active; }), bullets.end());
    }

    std::vector<Ch1Bullet>& all() { return bullets; }
    int getFireDelay() const { return fireDelay; }
    double getBulletSpeed() const { return bulletSpeed; }
    static double getBulletRange() { return BULLET_RANGE; }
};


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
            SDL_SetRenderDrawColor(renderer, 50, 255, 100, (unsigned char)alpha);
            const int SEG = 80;
            SDL_Point prev;
            for (int i = 0; i <= SEG; ++i) {
                double t = (double)i / SEG;
                int sx = (int)(CENTER_X + swA * (2.0 * t - 1.0));
                double ratio = (double)(sx - CENTER_X) / swA;
                if (ratio > 1.0) ratio = 1.0;
                if (ratio < -1.0) ratio = -1.0;
                int sy = (int)(sw.y - swB * std::sqrt(1.0 - ratio * ratio));
                if (i > 0) SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, sy);
                prev = {sx, sy};
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

    std::vector<Ch1Shockwave>& all() { return shockwaves; }
    int getLevel() const { return damage - 2; }
    void setPending(bool p) { pending = p; }
};


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
        for (const auto& a : aliens) {
            if (!a.active) continue;
            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
            double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
            double scale = (depthBelow < 0) ? 0.17 : 0.17 + 0.83 * depthBelow;
            if (scale < 0.14) scale = 0.14;
            if (scale > 1.0)  scale = 1.0;
            int s = (int)(28.0 * scale);
            if (s < 3) s = 3;

            // Entry trail
            if (a.entering && a.enterFrame > 3) {
                for (int k = 1; k <= 3; ++k) {
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
                    int alpha = 150 - k * 40;
                    SDL_SetRenderDrawColor(renderer, 100, 180, 255, (unsigned char)alpha);
                    SDL_RenderDrawPoint(renderer, (int)pastX, (int)pastY);
                }
            }

            double hpRatio = (double)a.hp / a.maxHp;
            bool inv = a.entering || a.invincibleFrames != 0;
            int r, g, b;
            if (inv) { r = 100; g = 180; b = 255; }
            else { r = 255; g = (int)(150 * hpRatio + 80 * (1 - hpRatio)); b = (int)(100 * hpRatio + 30 * (1 - hpRatio)); }
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);

            int ix = (int)ax, iy = (int)a.y;
            SDL_Point pts[4] = {
                {ix, iy - s}, {ix + s * 2 / 3, iy},
                {ix, iy + s}, {ix - s * 2 / 3, iy}
            };
            SDL_RenderDrawLines(renderer, pts, 4);
            SDL_RenderDrawLine(renderer, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(renderer, ix - s*2/3, iy, ix + s*2/3, iy);
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
};


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
        double progress = 1.0 - (double)shakeTimer / 120.0;
        if (progress > 1.0) progress = 1.0;
        double radius = 40.0 + progress * 520.0;
        int alpha = (int)(180.0 * (1.0 - progress));
        if (alpha < 8) return;
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, (unsigned char)alpha);
        const int SEG = 64;
        SDL_Point prev;
        for (int i = 0; i <= SEG; ++i) {
            double angle = 2.0 * M_PI * i / SEG;
            double sy = y + std::sin(angle) * radius;
            double rx = radius * perspWidth(sy) / perspWidth(y);
            int sx = (int)(x + std::cos(angle) * rx);
            if (i > 0) SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, (int)sy);
            prev = {sx, (int)sy};
        }
    }

    void drawCh1HealWaves(SDL_Renderer* renderer) const {
        for (const auto& hw : healWaves) {
            if (!hw.active) continue;
            SDL_SetRenderDrawColor(renderer, 255, 60, 60, 80);
            const int SEG = 60;
            SDL_Point prev;
            for (int i = 0; i <= SEG; ++i) {
                double angle = 2.0 * M_PI * i / SEG;
                double sy = y + std::sin(angle) * hw.radius;
                double rx = hw.radius * perspWidth(sy) / perspWidth(y);
                int sx = (int)(x + std::cos(angle) * rx);
                if (i > 0) SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, (int)sy);
                prev = {sx, (int)sy};
            }
        }
    }

    void drawBody(SDL_Renderer* renderer) const {
        if (!active || y < -100) return;
        double scale = 0.5 + 0.5 * ((y + 100) / 200.0);
        if (scale > 1.0) scale = 1.0;
        int s = (int)((cfg ? cfg->size : 80.0) * scale);
        if (s < 20) s = 20;
        int ix = (int)x, iy = (int)y;

        if (flashTimer > 0)
            SDL_SetRenderDrawColor(renderer, 255, 200, 200, 255);
        else
            SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
        SDL_Point pts[4] = {
            {ix, iy - s}, {ix + s*2/3, iy},
            {ix, iy + s/2}, {ix - s*2/3, iy}
        };
        SDL_RenderDrawLines(renderer, pts, 4);
        SDL_RenderDrawLine(renderer, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
        SDL_RenderDrawLine(renderer, ix - s*2/3, iy, ix + s*2/3, iy);
        SDL_RenderDrawLine(renderer, ix, iy - s, ix, iy + s/2);

        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 100);
        int r = (int)(s * 0.9);
        for (int i = 0; i < 2; ++i) {
            int rr = r + i * 10;
            SDL_Point ring[6] = {
                {ix, iy - rr}, {ix + rr*3/4, iy - rr/2},
                {ix + rr*3/4, iy + rr/2}, {ix, iy + rr},
                {ix - rr*3/4, iy + rr/2}, {ix - rr*3/4, iy - rr/2}
            };
            SDL_RenderDrawLines(renderer, ring, 6);
            SDL_RenderDrawLine(renderer, ring[5].x, ring[5].y, ring[0].x, ring[0].y);
        }
    }

    void drawHPBar(SDL_Renderer* renderer, const Font& font) const {
        if (!active) return;
        const int BAR_W = 400, BAR_H = 14;
        const int BAR_X = CENTER_X - BAR_W / 2, BAR_Y = 8;
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_Rect bg = {BAR_X, BAR_Y, BAR_W, BAR_H};
        SDL_RenderFillRect(renderer, &bg);

        int baseW = (int)((double)hp / maxHp * BAR_W);
        if (baseW > BAR_W) baseW = BAR_W;
        int bonusW = (int)((double)bonusHp / maxHp * BAR_W);

        SDL_SetRenderDrawColor(renderer, 220, 30, 30, 255);
        SDL_Rect baseR = {BAR_X, BAR_Y, baseW, BAR_H};
        SDL_RenderFillRect(renderer, &baseR);
        if (bonusHp > 0 && bonusW > 0) {
            SDL_SetRenderDrawColor(renderer, 255, 140, 140, 255);
            SDL_Rect bonusR = {BAR_X + baseW, BAR_Y, bonusW, BAR_H};
            SDL_RenderFillRect(renderer, &bonusR);
        }
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        int borderW = baseW + bonusW + 2;
        if (borderW < BAR_W + 2) borderW = BAR_W + 2;
        SDL_Rect border = {BAR_X-1, BAR_Y-1, borderW, BAR_H+2};
        SDL_RenderDrawRect(renderer, &border);
        if (cfg && cfg->name) {
            int nameLen = (int)strlen(cfg->name);
            int namePxW = nameLen * 6 * 2;  // font size 2, each char 12px
            int nameX = BAR_X - namePxW - 8;
            for (int i = 0; cfg->name[i]; ++i) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                font.drawChar(renderer, cfg->name[i], nameX + i * 12, BAR_Y + 1, 2);
            }
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


// ============== Ch1Background ==============
class Ch1Background {
    std::vector<Star> stars;
    const ChapterConfig& chapterCfg;

public:
    Ch1Background(const ChapterConfig& cfg) : chapterCfg(cfg) {
        for (int i = 0; i < cfg.starCount; ++i)
            stars.push_back({(float)(rand() % WIN_WIDTH), (float)(rand() % cfg.horizonY),
                             (rand() % 628) / 100.0f, 0.01f + (rand() % 40) / 1000.0f,
                             (rand() % 30 - 15) / 200.0f});
    }

    void update() {
        for (auto& s : stars) {
            s.phase += s.twinkleSpeed;
            if (s.phase > 2.0f * M_PI) s.phase -= 2.0f * M_PI;
            s.x += s.driftSpeed;
            if (s.x < -10) s.x = WIN_WIDTH + 10;
            if (s.x > WIN_WIDTH + 10) s.x = -10;
        }
    }

    void drawStars(SDL_Renderer* renderer) const {
        float brightness = chapterCfg.starBrightness;
        for (const auto& s : stars) {
            float bright = 0.45f + 0.55f * std::fabs(std::sin(s.phase));
            int b = (int)(bright * 200 * brightness);
            if (b < 60) b = 60;
            SDL_SetRenderDrawColor(renderer, b, b, b, 255);
            SDL_RenderDrawPoint(renderer, (int)s.x, (int)s.y);
        }
    }

    void drawStarsFullscreen(SDL_Renderer* renderer) const {
        float brightness = chapterCfg.starBrightness;
        int hy = chapterCfg.horizonY;
        // Sky portion: existing stars [0, hy)
        for (const auto& s : stars) {
            float bright = 0.45f + 0.55f * std::fabs(std::sin(s.phase));
            int b = (int)(bright * 200 * brightness);
            if (b < 60) b = 60;
            SDL_SetRenderDrawColor(renderer, b, b, b, 255);
            SDL_RenderDrawPoint(renderer, (int)s.x, (int)s.y);
        }
        // Ground portion: remap stars to [hy, WIN_HEIGHT]
        float remap = (float)(WIN_HEIGHT - hy) / hy;
        for (const auto& s : stars) {
            float y2 = hy + s.y * remap;
            if (y2 >= WIN_HEIGHT) continue;
            float bright = 0.40f + 0.60f * std::fabs(std::sin(s.phase + 2.3f));
            int b = (int)(bright * 200 * brightness);
            if (b < 55) b = 55;
            SDL_SetRenderDrawColor(renderer, b, b, b, 255);
            SDL_RenderDrawPoint(renderer, (int)(s.x + s.driftSpeed * 300), (int)y2);
        }
    }

    void drawBackground(SDL_Renderer* renderer) const {
        int hy = chapterCfg.horizonY;
        // Ground
        SDL_SetRenderDrawColor(renderer, chapterCfg.groundColorR, chapterCfg.groundColorG, chapterCfg.groundColorB, 255);
        SDL_RenderDrawLine(renderer, 0, hy, WIN_WIDTH, hy);
        drawStars(renderer);

        // Perspective radiate lines
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        const int NUM_RADIATE = 17;
        for (int i = 0; i < NUM_RADIATE; ++i) {
            double t = (double)i / (NUM_RADIATE - 1);
            int bx = (int)(t * WIN_WIDTH);
            int hx = (int)(HORIZON_LEFT + t * (HORIZON_RIGHT - HORIZON_LEFT));
            SDL_RenderDrawLine(renderer, bx, WIN_HEIGHT, hx, hy);
        }

        // Depth lines
        SDL_SetRenderDrawColor(renderer, 45, 45, 45, 255);
        const int NUM_DEPTH = 16;
        for (int i = 0; i < NUM_DEPTH; ++i) {
            double ratio = (double)(i + 1) / (NUM_DEPTH + 1);
            double dy = hy + (WIN_HEIGHT - hy) * (1.0 - ratio) * (1.0 - ratio);
            SDL_RenderDrawLine(renderer, 0, (int)dy, WIN_WIDTH, (int)dy);
        }
    }

    void drawBase(SDL_Renderer* renderer) const {
        const int BASE_H = 75;
        const double A = WIN_WIDTH / 2.0 - 15.0;
        const double B = BASE_H;
        SDL_SetRenderDrawColor(renderer, 50, 180, 80, 255);
        const int SEG = 80;
        SDL_Point prev;
        for (int i = 0; i <= SEG; ++i) {
            double t = (double)i / SEG;
            int sx = (int)(CENTER_X + A * (2.0 * t - 1.0));
            double ratio = (double)(sx - CENTER_X) / A;
            if (ratio > 1.0) ratio = 1.0;
            if (ratio < -1.0) ratio = -1.0;
            int sy = (int)(WIN_HEIGHT - B * std::sqrt(1.0 - ratio * ratio));
            if (i > 0) SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, sy);
            prev = {sx, sy};
        }
        SDL_SetRenderDrawColor(renderer, 30, 130, 55, 255);
        for (int i = 0; i < 30; ++i) {
            double t = (double)i / 29.0;
            int sx = (int)(CENTER_X + A * 0.85 * (2.0 * t - 1.0));
            double ratio = (double)(sx - CENTER_X) / A;
            if (ratio > 1.0) ratio = 1.0;
            if (ratio < -1.0) ratio = -1.0;
            int topY = (int)(WIN_HEIGHT - B * std::sqrt(1.0 - ratio * ratio));
            SDL_RenderDrawLine(renderer, sx, topY + 3, sx, WIN_HEIGHT);
        }
        SDL_SetRenderDrawColor(renderer, 50, 180, 80, 255);
        SDL_RenderDrawLine(renderer, CENTER_X - (int)A, WIN_HEIGHT, CENTER_X + (int)A, WIN_HEIGHT);
    }
};


// ============== Ch2SphereBoss ==============
class Ch2SphereBoss {
public:
    enum State { INACTIVE, ENTERING, ACTIVATING, FIGHT, SHATTERING,
                 COLLAPSING, SHAKING, RUSHING_OUT, DONE };

    struct Cell {
        double relX, relY;
        int activationOrder, shatterOrder;
        bool onSphere, activated;
        int colorState; // 0=blue 1=orange 2=white
    };
    struct SphereDebris {
        double x, y, vx, vy, rotAngle, rotSpeed;
        bool onGround, rushing;
        double shakeDelay, shakePhase;
        double rushVx, rushVy;
        double floorY;     // per-debris landing Y for perspective floor scatter
    };

    Ch2SphereBoss() : cx(900), cy(290), worldX(1200), radius(150),
        hp(500), maxHp(500), state(INACTIVE), stateTimer(0),
        activationIdx(0), shatterIdx(0), rushIdx(0),
        fixedSeed(12345), entryStartX(900), entryBgStopped(false),
        bg(nullptr), playerRef(nullptr), bgTargetSpeed(-1.0) {}

    void init(Ch2Background* b, Player* p) { bg = b; playerRef = p; }
    void reset() { state = INACTIVE; stateTimer = 0; cells.clear(); activationByOrder.clear();
        debris.clear(); activationIdx = shatterIdx = rushIdx = 0; hp = maxHp; }

    State getState() const { return state; }
    bool isActive() const { return state != INACTIVE && state != DONE; }

    void startEntering() {
        reset();
        generateCells();
        state = ENTERING; stateTimer = 0;
        worldX = 1200; // far right, scrolls in with background
        cx = worldX; cy = 290;
    }

    // Test mode: spawn sphere at center in FIGHT state immediately
    void startAtCenter() {
        reset();
        worldX = WIN_WIDTH / 2; cx = worldX; cy = 290;
        generateCells();
        for (auto& c : cells) { c.activated = true; c.colorState = 1; }
        bgTargetSpeed = 0.0; // stop background
        state = FIGHT; stateTimer = 0;
    }

    // Update screen position from world coords
    void syncScreenPos(double scrollX) {
        cx = worldX - scrollX;
    }

    double getCx() const { return cx; }
    double getCy() const { return cy; }
    double getRadius() const { return radius; }
    double getBgTargetSpeed() const { return bgTargetSpeed; }
    void clearBgTargetSpeed() { bgTargetSpeed = -1.0; }
    void takeDamage(int dmg) { if (state == FIGHT) { hp -= dmg; if (hp < 0) hp = 0; } }

    void update() {
        if (state == INACTIVE || state == DONE) return;
        stateTimer++;

        switch (state) {
        case ENTERING: {
            // Sphere scrolls in with background; slow down as it approaches center
            double targetScreenX = WIN_WIDTH / 2;
            double distToTarget = std::abs(cx - targetScreenX);
            double slowFactor = 1.0;
            if (distToTarget < 300) {
                slowFactor = distToTarget / 300.0;
                if (slowFactor < 0.05) slowFactor = 0.05;
            }
            bgTargetSpeed = 3.5 * slowFactor;
            // Lock position and stop bg when sphere reaches center
            if (distToTarget < 8) {
                cx = targetScreenX; bgTargetSpeed = 0.0;
                state = ACTIVATING; stateTimer = 0;
            }
            break;
        }
        case ACTIVATING: {
            // Slow stochastic wave from bottom to top (~180 frames)
            int remaining = (int)cells.size() - activationIdx;
            if (remaining <= 0) { state = FIGHT; stateTimer = 0; break; }
            int perFrame = remaining / 160 + 1; // slow pace
            int window = remaining / 20 + 8;     // lookahead window for randomness
            if (window > remaining) window = remaining;
            for (int i = 0; i < perFrame && activationIdx < (int)cells.size(); ++i) {
                // Pick randomly within the upcoming window
                int pick = activationIdx + (rand() % window);
                if (pick >= (int)cells.size()) pick = (int)cells.size() - 1;
                int ci = activationByOrder[pick];
                if (!cells[ci].activated) {
                    cells[ci].activated = true; cells[ci].colorState = 1;
                    // Swap picked cell down to activationIdx so it's marked done
                    std::swap(activationByOrder[pick], activationByOrder[activationIdx]);
                }
                // Advance activationIdx past any already-activated cells
                while (activationIdx < (int)cells.size() &&
                       cells[activationByOrder[activationIdx]].activated)
                    activationIdx++;
            }
            if (activationIdx >= (int)cells.size()) {
                state = FIGHT; stateTimer = 0;
            }
            break;
        }
        case FIGHT: {
            updateDebrisPhysics();
            bool allOff = true;
            for (auto& c : cells) if (c.onSphere) { allOff = false; break; }
            if (hp <= 0 || allOff) { state = SHATTERING; stateTimer = 0; shatterIdx = 0; }
            break;
        }
        case SHATTERING: {
            // Pop remaining cells rapidly
            int perFrame = (int)cells.size() / 60 + 1;
            int popped = 0;
            for (auto& c : cells) {
                if (!c.onSphere) continue;
                if (c.shatterOrder >= shatterIdx && popped < perFrame) {
                    popSingleCell(c);
                    popped++;
                }
            }
            shatterIdx += popped;
            updateDebrisPhysics();
            bool allOff = true;
            for (auto& c : cells) if (c.onSphere) { allOff = false; break; }
            int minTime = allOff ? 20 : 90;
            if (allOff && stateTimer >= minTime) {
                // Assign progressive shake delays for chain reaction
                int n = (int)debris.size();
                for (int i = 0; i < n; ++i) {
                    // First debris: starts immediately, then wave propagates
                    double baseDelay = (double)i / n * 180.0; // spread over ~3s
                    debris[i].shakeDelay = baseDelay;
                    debris[i].shakePhase = 0;
                }
                state = SHAKING; stateTimer = 0;
            }
            break;
        }
        case SHAKING: {
            if (debris.empty()) { state = DONE; stateTimer = 0; bgTargetSpeed = 3.5; break; }
            int shakeDuration = 120; // frames each debris shakes before rushing out
            for (auto& d : debris) {
                if (d.rushing) {
                    d.x += d.rushVx; d.y += d.rushVy;
                    d.rotAngle += 0.15;
                    continue;
                }
                if (stateTimer >= d.shakeDelay) {
                    double localT = stateTimer - d.shakeDelay;
                    // Shake: increasing amplitude
                    double amp = std::min(localT * 0.04, 3.5);
                    d.shakePhase = std::sin(localT * 0.7) * amp;
                    // After shakeDuration, rush out in random direction
                    if (localT >= shakeDuration) {
                        d.rushing = true;
                        double angle = (rand() % 6283) / 1000.0;
                        double speed = 4.0 + (rand() % 400) / 100.0;
                        d.rushVx = std::cos(angle) * speed;
                        d.rushVy = std::sin(angle) * speed;
                        d.shakePhase = 0;
                    }
                }
            }
            // Remove off-screen debris
            debris.erase(std::remove_if(debris.begin(), debris.end(),
                [](const SphereDebris& d){ return d.rushing && (d.x<-60||d.x>WIN_WIDTH+60||d.y<-60||d.y>WIN_HEIGHT+60); }),
                debris.end());
            // Done when all debris gone
            if (debris.empty()) {
                state = DONE; stateTimer = 0;
                bgTargetSpeed = 3.5;
            }
            break;
        }
        default: break;
        }
    }

    void updateDebrisPhysics() {
        for (auto& d : debris) {
            if (d.onGround || d.rushing) continue;
            d.vy += 0.3; d.x += d.vx; d.y += d.vy;
            d.rotAngle += d.rotSpeed;
            if (d.y > d.floorY) {
                d.y = d.floorY; d.vy *= -0.3; d.vx *= 0.7;
                if (std::abs(d.vy) < 0.5) { d.vy = 0; d.vx = 0; d.onGround = true; }
            }
        }
    }

    void popSingleCell(Cell& c) {
        c.onSphere = false; c.colorState = 2;
        double sx = cx + c.relX, sy = cy + c.relY;
        // Perspective floor scatter: debris lands around the sphere on the floor
        // depthFraction: 0=top of sphere (near WALL_Y), 1=bottom (near viewer)
        double depthFraction = (c.relY + radius) / (2.0 * radius);
        if (depthFraction < 0.1) depthFraction = 0.1;
        if (depthFraction > 0.9) depthFraction = 0.9;
        double dFloorY = 280.0 + (cy + radius - 280.0) * depthFraction;
        dFloorY += (rand() % 60 - 30); // slight random variation
        if (dFloorY < 280) dFloorY = 280;
        if (dFloorY > cy + radius) dFloorY = cy + radius;
        // Horizontal scatter wider at bottom (closer), narrower at top (farther)
        double hSpread = 40.0 + 160.0 * depthFraction;
        double targetX = cx + (rand() % (int)(hSpread * 2)) - hSpread;
        double dx = targetX - sx;
        double angle = (rand() % 6283) / 1000.0;
        double speed = 1.5 + (rand() % 100) / 100.0 * 2.0;
        SphereDebris d;
        d.x = sx; d.y = sy;
        d.vx = dx * 0.05 + std::cos(angle) * speed;
        d.vy = -2.0 - (rand() % 40) / 10.0;
        d.floorY = dFloorY;
        d.rotAngle = 0; d.rotSpeed = (rand() % 20 - 10) / 100.0;
        d.onGround = false; d.rushing = false;
        d.shakeDelay = 0; d.shakePhase = 0; d.rushVx = 0; d.rushVy = 0;
        debris.push_back(d);
    }

    // Pop diamonds off the sphere (called on bullet hit)
    void popDiamonds(int count) {
        int popped = 0;
        for (auto& c : cells) {
            if (popped >= count) break;
            if (!c.onSphere) continue;
            if (c.shatterOrder == shatterIdx + popped) {
                popSingleCell(c); popped++;
            }
        }
        shatterIdx += popped;
    }

    void draw(SDL_Renderer* r) const {
        if (state == INACTIVE || state == DONE) return;
        if (cells.empty()) return;

        for (const auto& cell : cells) {
            if (!cell.onSphere) continue;
            double sx = cx + cell.relX;
            double sy = cy + cell.relY;
            // Compute distance from center for edge compression
            double dist = std::sqrt(cell.relX * cell.relX + cell.relY * cell.relY);
            double edgeScale = 0.55 + 0.45 * (1.0 - dist / radius);
            double sz = 10.0 * edgeScale;
            if (sz < 2.5) sz = 2.5;
            int hw = (int)(sz * 2.0 / 3.0);

            // Color
            int rCol, gCol, bCol;
            if (cell.colorState == 0) { // blue (invincible)
                rCol = 40; gCol = 130; bCol = 230;
            } else if (cell.colorState == 1) { // orange (attackable)
                rCol = 230; gCol = 140; bCol = 50;
            } else { // white (shattering flash)
                rCol = 255; gCol = 255; bCol = 240;
            }
            // Top-down lighting: brighter at top, darker at bottom
            double ny = cell.relY / dist; // upward normal component
            double bright = 0.35 + 0.65 * (-ny * 0.55 + 0.45);
            rCol = (int)(rCol * bright); gCol = (int)(gCol * bright); bCol = (int)(bCol * bright);
            if (rCol > 255) rCol = 255; if (gCol > 255) gCol = 255; if (bCol > 255) bCol = 255;

            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, 255);
            int ex = (int)sx, ey = (int)sy;
            // Diamond: 4 vertices + closing line + 2 diagonals
            SDL_Point pts[4] = {
                {ex, ey - (int)sz},
                {ex + hw, ey},
                {ex, ey + (int)sz},
                {ex - hw, ey}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(r, ex - hw, ey, ex + hw, ey);
        }

        // Draw debris (broken-off diamonds)
        for (const auto& d : debris) {
            int sz = 8;
            int hw = sz * 2 / 3;
            int rCol = 200, gCol = 200, bCol = 200; // grey-white debris
            int alpha = d.rushing ? 180 : 200;
            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, (Uint8)alpha);
            int ex = (int)(d.x + d.shakePhase), ey = (int)d.y;
            double cRot = std::cos(d.rotAngle), sRot = std::sin(d.rotAngle);
            SDL_Point pts[4] = {
                {ex + (int)(-sz * sRot), ey + (int)(-sz * cRot)},
                {ex + (int)( hw * cRot), ey + (int)( hw * sRot)},
                {ex + (int)( sz * sRot), ey + (int)( sz * cRot)},
                {ex + (int)(-hw * cRot), ey + (int)(-hw * sRot)}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(r, ex - (int)(hw*cRot), ey - (int)(hw*sRot),
                                  ex + (int)(hw*cRot), ey + (int)(hw*sRot));
        }
    }

private:
    double cx, cy, worldX, radius;
    int hp, maxHp;
    std::vector<Cell> cells;
    std::vector<int> activationByOrder; // maps activationOrder → cell index
    std::vector<SphereDebris> debris;
    State state; int stateTimer;
    int activationIdx, shatterIdx, rushIdx;
    uint32_t fixedSeed;
    double entryStartX;
    bool entryBgStopped;
    Ch2Background* bg;
    Player* playerRef;
    double bgTargetSpeed; // speed request for Ch2Background

    void generateCells() {
        cells.clear();
        const double SPACING = 16.0;
        const double ROW_H = SPACING * 0.8660254; // sqrt(3)/2

        // Center the hex grid on (0,0) for symmetry
        int halfRows = (int)(radius / ROW_H) + 2;
        int halfCols = (int)(radius / SPACING) + 2;

        for (int row = -halfRows; row <= halfRows; ++row) {
            double rowY = row * ROW_H;
            double rowOff = (row % 2 == 0) ? 0.0 : SPACING * 0.5;
            for (int col = -halfCols; col <= halfCols; ++col) {
                double px = col * SPACING + rowOff;
                double py = rowY;
                double dist = std::sqrt(px*px + py*py);
                if (dist > radius - 2.0) continue;
                Cell c;
                c.relX = px; c.relY = py;
                c.onSphere = true;
                c.activated = false;
                c.colorState = 0; // blue
                c.activationOrder = 0;
                c.shatterOrder = 0;
                cells.push_back(c);
            }
        }

        // Assign activationOrder: sort by Y descending (bottom to top on screen)
        std::vector<int> idxByY(cells.size());
        for (size_t i = 0; i < idxByY.size(); ++i) idxByY[i] = (int)i;
        std::sort(idxByY.begin(), idxByY.end(), [this](int a, int b) {
            return cells[a].relY > cells[b].relY; // larger Y = lower on screen = activated first
        });
        activationByOrder.resize(cells.size());
        for (size_t i = 0; i < idxByY.size(); ++i) {
            cells[idxByY[i]].activationOrder = (int)i;
            activationByOrder[i] = idxByY[i]; // order→index mapping
        }

        // Assign shatterOrder: Fisher-Yates shuffle with fixed seed
        std::vector<int> shatterIdx(cells.size());
        for (size_t i = 0; i < shatterIdx.size(); ++i) shatterIdx[i] = (int)i;
        uint32_t rng = fixedSeed;
        for (int i = (int)shatterIdx.size() - 1; i > 0; --i) {
            rng = rng * 1103515245 + 12345;
            int j = (int)(rng % (uint32_t)(i + 1));
            std::swap(shatterIdx[i], shatterIdx[j]);
        }
        for (size_t i = 0; i < shatterIdx.size(); ++i)
            cells[shatterIdx[i]].shatterOrder = (int)i;
    }
};


// ============== Ch2Background ==============
class Ch2Background {
private:
    struct Star2 { double wx, wy; float phase, twinkleSpeed; bool isCross; };
    struct Pillar { double wx; bool isMajor; };

    std::vector<Star2> stars;
    std::vector<Pillar> pillars;
    double scrollX, scrollSpeed, genNext;
    static constexpr double STAR_PARALLAX = 0.015;
    static constexpr double PILLAR_SPACING = 330.0; // ~1.5s between pillars
    static const int VP_X = CENTER_X;    // pillar/glass wall vanishing point x
    static const int VP_Y = -60;        // pillar/glass wall vanishing point y (above screen)
    static const int WALL_Y = 280;      // floor meets glass wall

    // Longitudinal floor seams (horizontal lines along corridor, at fixed depths)
    // Each track = a fixed y position on the floor, seams scroll left at same speed
    struct SeamTrack {
        double y;           // fixed screen y for this depth track
        double spacing;     // world spacing between seams at this depth (near=wider, far=narrower)
        double genNext;     // next world-x to generate
        std::vector<double> wxs; // world-x positions of seams
    };
    static const int N_SEAM_TRACKS = 5;
    SeamTrack seamTracks[N_SEAM_TRACKS];

    void genStars() {
        for (int i = 0; i < 400; ++i) {
            Star2 s;
            s.wx = rand() % 6000; s.wy = rand() % WIN_HEIGHT;
            s.phase = (rand() % 628) / 100.0f;
            s.twinkleSpeed = 0.008f + (rand() % 50) / 1000.0f;
            s.isCross = (rand() % 100 < 10);
            stars.push_back(s);
        }
    }

    void genAhead() {
        double ahead = scrollX + WIN_WIDTH + 600.0; // margin: generate well off-screen to avoid visible pop-in
        // Pillars
        while (genNext < ahead) {
            pillars.push_back({genNext, (int)(genNext / PILLAR_SPACING) % 5 == 0});
            genNext += PILLAR_SPACING;
        }
        // Floor seams per track
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            while (seamTracks[t].genNext < ahead) {
                seamTracks[t].wxs.push_back(seamTracks[t].genNext);
                seamTracks[t].genNext += seamTracks[t].spacing;
            }
        }
    }

    template<typename T>
    void pruneVec(std::vector<T>& v, double cutoff) {
        int w = 0;
        for (int i = 0; i < (int)v.size(); ++i)
            if (v[i].wx >= cutoff) v[w++] = v[i];
        v.resize(w);
    }
    template<typename T>
    void pruneDoubleVec(std::vector<T>& v, double cutoff) {
        int w = 0;
        for (int i = 0; i < (int)v.size(); ++i)
            if (v[i] >= cutoff) v[w++] = v[i];
        v.resize(w);
    }
    void pruneBehind() {
        double cutoff = scrollX - 500.0;
        pruneVec(pillars, cutoff);
        for (int t = 0; t < N_SEAM_TRACKS; ++t)
            pruneDoubleVec(seamTracks[t].wxs, cutoff);
    }

    void pruneStars() {
        double cutoff = scrollX * STAR_PARALLAX - 400.0;
        double wrap = scrollX * STAR_PARALLAX + WIN_WIDTH + 400.0;
        for (auto& s : stars) {
            if (s.wx < cutoff) { s.wx += 6000.0; s.phase = (rand() % 628) / 100.0f; }
            if (s.wx > wrap) { s.wx -= 6000.0; }
        }
    }

public:
    Ch2Background() : scrollX(0), scrollSpeed(3.5), genNext(-200.0) {
        genStars();
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            double frac = (double)(t + 1) / (N_SEAM_TRACKS + 1);
            seamTracks[t].y = WALL_Y + (WIN_HEIGHT - WALL_Y) * frac * frac;
            seamTracks[t].spacing = 45.0 + t * 22.0;
            seamTracks[t].genNext = -200.0;
        }
    }
    void reset() {
        scrollX = 0; scrollSpeed = 3.5; genNext = -200.0;
        pillars.clear(); stars.clear(); genStars();
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            seamTracks[t].wxs.clear();
            seamTracks[t].genNext = -200.0;
        }
    }
    void setSpeed(double s) { scrollSpeed = s; }
    double getScrollX() const { return scrollX; }

    void update() {
        scrollX += scrollSpeed;
        for (auto& s : stars) {
            s.phase += s.twinkleSpeed;
            if (s.phase > 2.0f * M_PI) s.phase -= 2.0f * M_PI;
        }
        pruneStars();
        genAhead();
        pruneBehind();
    }

    void draw(SDL_Renderer* r) {
        drawSky(r);
        drawFloor(r);
        drawGlassPanels(r);
        drawPillars(r);
    }

private:
    void drawSky(SDL_Renderer* r) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_Rect bg = {0, 0, WIN_WIDTH, WIN_HEIGHT};
        SDL_RenderFillRect(r, &bg);
        for (const auto& s : stars) {
            int sx = (int)(s.wx - scrollX * STAR_PARALLAX);
            int sy = (int)s.wy;
            if (sx < -20 || sx > WIN_WIDTH + 20) continue;
            float bright = 0.35f + 0.65f * std::fabs(std::sin(s.phase));
            int b = (int)(bright * 240); if (b < 40) b = 40; if (b > 255) b = 255;
            SDL_SetRenderDrawColor(r, b, b, b, 255);
            SDL_RenderDrawPoint(r, sx, sy);
            if (s.isCross && bright > 0.6f) {
                int cb = (int)(bright * 160); if (cb > 160) cb = 160;
                SDL_SetRenderDrawColor(r, cb, cb, cb, 255);
                int len = (bright > 0.82f) ? 4 : 2;
                SDL_RenderDrawLine(r, sx - len, sy, sx + len, sy);
                SDL_RenderDrawLine(r, sx, sy - len, sx, sy + len);
            }
        }
    }

    // ======== FLOOR ========
    void drawFloor(SDL_Renderer* r) {
        // Floor fill
        SDL_SetRenderDrawColor(r, 22, 24, 30, 255);
        SDL_Rect floorRect = {0, WALL_Y, WIN_WIDTH, WIN_HEIGHT - WALL_Y};
        SDL_RenderFillRect(r, &floorRect);

        // Longitudinal floor seams (horizontal lines along corridor, at fixed depths)
        // Each track has a fixed y, seams scroll left at same speed as pillars
        for (int t = 0; t < N_SEAM_TRACKS; ++t) {
            int y = (int)seamTracks[t].y;
            int alpha = 180 - t * 22; // nearer = brighter
            SDL_SetRenderDrawColor(r, 45, 48, 58, alpha);
            for (double wx : seamTracks[t].wxs) {
                double sx = wx - scrollX;
                if (sx < -40 || sx > WIN_WIDTH + 40) continue;
                // Each seam is a short horizontal dash at its fixed y
                int halfW = 15 + t * 8; // wider near camera
                SDL_RenderDrawLine(r, (int)sx - halfW, y, (int)sx + halfW, y);
            }
        }

        // Horizon junction line (where floor meets glass wall)
        SDL_SetRenderDrawColor(r, 35, 38, 45, 255);
        SDL_RenderDrawLine(r, 0, WALL_Y, WIN_WIDTH, WALL_Y);
        SDL_SetRenderDrawColor(r, 55, 58, 68, 255);
        SDL_RenderDrawLine(r, 0, WALL_Y + 2, WIN_WIDTH, WALL_Y + 2);
    }

    // ======== GLASS PANELS ========
    void pillarScreenX(const Pillar& p, double& botX, double& topX) const {
        botX = p.wx - scrollX;
        topX = VP_X + (botX - VP_X) * 0.85;
    }

    void drawGlassPanels(SDL_Renderer* r) {
        for (size_t i = 0; i + 1 < pillars.size(); ++i) {
            double lbx, ltx, rbx, rtx;
            pillarScreenX(pillars[i], lbx, ltx);
            pillarScreenX(pillars[i+1], rbx, rtx);
            double glassT = 0, glassB = WALL_Y;

            // Cull only if ENTIRE pane is off-screen
            double minX = std::min(std::min(lbx, ltx), std::min(rbx, rtx));
            double maxX = std::max(std::max(lbx, ltx), std::max(rbx, rtx));
            if (maxX < -100 || minX > WIN_WIDTH + 100) continue;

            // Border colour
            SDL_SetRenderDrawColor(r, 170, 200, 230, 95);
            // Left edge (follows pillar angle)
            SDL_RenderDrawLine(r, (int)lbx, (int)glassB, (int)ltx, (int)glassT);
            // Right edge
            SDL_RenderDrawLine(r, (int)rbx, (int)glassB, (int)rtx, (int)glassT);
            // Top edge
            SDL_RenderDrawLine(r, (int)ltx, (int)glassT, (int)rtx, (int)glassT);
            // Bottom edge
            SDL_RenderDrawLine(r, (int)lbx, (int)glassB, (int)rbx, (int)glassB);

            // Internal horizontal seams (2-3 subtle lines, Minecraft pane style)
            SDL_SetRenderDrawColor(r, 165, 195, 225, 55);
            for (int hi = 1; hi <= 3; ++hi) {
                double t = (double)hi / 4.0;
                int ly = (int)(glassT + (glassB - glassT) * t);
                int lx = (int)(ltx + (lbx - ltx) * t);
                int rx = (int)(rtx + (rbx - rtx) * t);
                SDL_RenderDrawLine(r, lx, ly, rx, ly);
            }

            // Internal vertical seams (1-2 subtle lines)
            for (int vi = 1; vi <= 2; ++vi) {
                double t = (double)vi / 3.0;
                int bx = (int)(lbx + (rbx - lbx) * t);
                int tx = (int)(ltx + (rtx - ltx) * t);
                SDL_RenderDrawLine(r, bx, (int)glassB, tx, (int)glassT);
            }

            // Corner highlights (Minecraft signature)
            SDL_SetRenderDrawColor(r, 220, 235, 255, 130);
            // Top-left corner
            SDL_RenderDrawLine(r, (int)ltx, (int)glassT, (int)ltx + 6, (int)glassT);
            SDL_RenderDrawLine(r, (int)ltx, (int)glassT, (int)ltx, (int)glassT + 6);
            // Top-right corner
            SDL_RenderDrawLine(r, (int)rtx, (int)glassT, (int)rtx - 6, (int)glassT);
            SDL_RenderDrawLine(r, (int)rtx, (int)glassT, (int)rtx, (int)glassT + 6);
            // Bottom-left corner
            SDL_RenderDrawLine(r, (int)lbx, (int)glassB, (int)lbx + 6, (int)glassB);
            SDL_RenderDrawLine(r, (int)lbx, (int)glassB, (int)lbx, (int)glassB - 6);
            // Bottom-right corner
            SDL_RenderDrawLine(r, (int)rbx, (int)glassB, (int)rbx - 6, (int)glassB);
            SDL_RenderDrawLine(r, (int)rbx, (int)glassB, (int)rbx, (int)glassB - 6);

            // Center sheen (subtle diagonal glow)
            double cx = (ltx + rtx + lbx + rbx) * 0.25;
            double cy = glassB * 0.35;
            SDL_SetRenderDrawColor(r, 200, 225, 250, 45);
            SDL_RenderDrawLine(r, (int)(cx - 15), (int)(cy - 8), (int)(cx + 10), (int)(cy + 6));
        }
    }

    // ======== PILLARS ========
    void drawPillars(SDL_Renderer* r) {
        for (const auto& p : pillars) {
            double sx = p.wx - scrollX;
            if (sx < -120 || sx > WIN_WIDTH + 120) continue; // full off-screen cull

            double botX = sx;
            double botY = WALL_Y;
            // Top leans slightly toward vanishing point (above screen)
            double topX = VP_X + (botX - VP_X) * 0.85;
            double topY = 0;

            int pw = p.isMajor ? 5 : 3;
            double dx = topX - botX, dy = topY - botY;
            double len = std::sqrt(dx*dx + dy*dy);
            if (len < 1.0) continue;
            double nx = -dy / len, ny = dx / len;

            // Pillar body
            SDL_SetRenderDrawColor(r, 55, 55, 65, 230);
            for (int w = -pw; w <= pw; ++w) {
                int x0 = (int)(botX + nx * w), y0 = (int)(botY + ny * w);
                int x1 = (int)(topX + nx * w), y1 = (int)(topY + ny * w);
                SDL_RenderDrawLine(r, x0, y0, x1, y1);
            }

            // Highlight edges
            SDL_SetRenderDrawColor(r, 80, 90, 120, 180);
            SDL_RenderDrawLine(r, (int)(botX+nx*pw), (int)(botY+ny*pw),
                               (int)(topX+nx*pw), (int)(topY+ny*pw));
            SDL_SetRenderDrawColor(r, 110, 120, 145, 180);
            SDL_RenderDrawLine(r, (int)(botX-nx*pw), (int)(botY-ny*pw),
                               (int)(topX-nx*pw), (int)(topY-ny*pw));

            // Gusset
            int gs = pw + 3;
            SDL_SetRenderDrawColor(r, 65, 65, 75, 180);
            SDL_RenderDrawLine(r, (int)(botX-nx*gs), (int)(botY-ny*gs), (int)botX, (int)botY + 6);
            SDL_RenderDrawLine(r, (int)(botX+nx*gs), (int)(botY+ny*gs), (int)botX, (int)botY + 6);
            SDL_RenderDrawLine(r, (int)(botX-nx*gs), (int)(botY-ny*gs), (int)(botX+nx*gs), (int)(botY+ny*gs));
        }
    }
};


// ============== Ch2ShooterBase (shared enemy bullet system) ==============
class Ch2ShooterBase {
protected:
    std::vector<Ch2EnemyBullet> bullets;
    int& hpRef;
    bool& goRef;

    void updateBullets(BulletManager& bulletMgr, ParticleManager& particleMgr, AudioEngine& audio,
                       Player& pl, FloatingTextManager& ftMgr, int* hitCount = nullptr) {
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
                    if (eb.hp <= 0) { eb.active = false; particleMgr.spawnExplosion(eb.x, eb.y, 4); audio.sndCrystalCrush(); }
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

// ============== NightElfEnergy (white energy bar + triple-fire) ==============
class NightElfEnergy {
public:
    static const int MAX_ENERGY = 50;
    static const int HIT_WINDOW = 30;       // 0.5s at 60fps without hit → decay
    static const int TRIPLE_DURATION = 900;  // 15s at 60fps
    static const int COUNTDOWN_START = 180;  // last 3s countdown beeps

    NightElfEnergy() : energy(0), lastHitFrame(-999), frameCounter(0),
        tripleActive(false), tripleTimer(0), tripleJustEntered(false) {}

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
            // Decay: 50 energy in 5 seconds → 0.1667 per frame
            energy -= MAX_ENERGY / 300.0; // MAX_ENERGY / (5*60)
            if (energy < 0) energy = 0;
        }
    }

    void onHit() { if (!tripleActive) { energy += 1.0; if (energy >= MAX_ENERGY) energy = MAX_ENERGY; } lastHitFrame = frameCounter; }
    void checkTripleTrigger() {
        if (!tripleActive && energy >= MAX_ENERGY) {
            tripleActive = true; tripleTimer = TRIPLE_DURATION; tripleJustEntered = true;
        }
    }

    bool isTripleActive() const { return tripleActive; }
    bool justEnteredTriple() const { return tripleJustEntered; }
    int getTripleTimer() const { return tripleTimer; }
    float getFill() const { return (float)(energy / MAX_ENERGY); }
    bool isDecaying() const { return !tripleActive && energy > 0 && frameCounter - lastHitFrame > HIT_WINDOW; }
    bool isCharging() const { return !tripleActive && energy > 0 && frameCounter - lastHitFrame <= HIT_WINDOW; }

private:
    double energy;
    int lastHitFrame, frameCounter;
    bool tripleActive, tripleJustEntered;
    int tripleTimer;
};


// ============== HUDBase (shared HUD drawing) ==============
class HUDBase {
public:
    // rightEdge = rightmost pixel of the HUD column (all elements aligned)
    static void drawScore(SDL_Renderer* r, const Font& font, int score, int rightEdge, int y) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE:%-4d", score);
        font.drawString(r, buf, rightEdge - 120, y, 2);  // 10 chars * 12px = 120px
    }
    static void drawHPHearts(SDL_Renderer* r, const Font& font, int hp, int maxHp, int rightEdge, int y) {
        for (int i = 0; i < maxHp; ++i) {
            int hx = rightEdge - 12 - i * 14;  // char is 12px wide at scale 2
            SDL_SetRenderDrawColor(r, (i < hp) ? 255 : 70, 20, 20, 255);
            font.drawChar(r, '*', hx, y, 2);
        }
    }
    static void drawEnergyBar(SDL_Renderer* r, int rightEdge, int y, int w, int h, float fill, bool breathing = false) {
        int x = rightEdge - w;
        SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
        SDL_Rect bg = {x, y, w, h}; SDL_RenderFillRect(r, &bg);
        int fw = (int)(fill * w); if (fw > w) fw = w;
        if (fw > 0) {
            int green;
            if (breathing) {
                float b = 0.6f + 0.4f * (float)std::sin(SDL_GetTicks() * 0.008);
                green = (int)(60 + 195 * b);
            } else {
                green = (fill > 0.9f) ? 255 : (fill > 0.3f ? 180 : 100);
            }
            SDL_SetRenderDrawColor(r, 30, (Uint8)green, 50, 255);
            SDL_Rect fr = {x, y, fw, h}; SDL_RenderFillRect(r, &fr);
        }
    }
};

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
                        b.active = false; b.canDamage = false; e.hp--; hitCount++;
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


// ============== Ch2AlienManager (Chapter 2 regular alien) ==============
struct Ch2Alien : EnemyData {
    double x, y, startX, startY, targetX, targetY;
    int fireVolleyTimer, fireBurstCount, fireBurstTimer;
    int lastHitByPulse;  // pulse wave ID already applied
};

class Ch2AlienManager : public Ch2ShooterBase {
    static const int ALIEN_HP = 10;
    static const int VOLNEY_COOLDOWN = 180;
    static const int BURST_INTERVAL = 8;
    static const int BURST_COUNT = 5;

    std::vector<Ch2Alien> aliens;

    void fireBullet(Player& pl, Ch2Alien& a) {
        double dx = pl.getX() - a.x, dy = pl.getY() - a.y;
        double d = std::sqrt(dx*dx + dy*dy); if (d < 1) d = 1;
        Ch2EnemyBullet b;
        b.x = a.x; b.y = a.y;
        b.dx = dx / d * 1.5; b.dy = dy / d * 1.5;
        b.hp = 3; b.active = true;
        bullets.push_back(b);
    }

    void spawnOne() {
        Ch2Alien a;
        a.targetX = 640 + rand() % 141;
        a.targetY = 40 + rand() % 521;
        int side = rand() % 4;
        if (side == 0)      { a.startX = 810; a.startY = 40 + rand() % 521; }        // right edge (off-screen)
        else if (side == 1) { a.startX = -10; a.startY = 40 + rand() % 521; }        // left edge (off-screen)
        else if (side == 2) { a.startX = 40 + rand() % 721; a.startY = -10; }         // top edge (full width)
        else                { a.startX = 40 + rand() % 721; a.startY = 610; }         // bottom edge (full width)
        a.x = a.startX; a.y = a.startY;
        a.hp = ALIEN_HP; a.active = true; a.entering = true; a.defeated = false;
        a.enterFrame = 0; a.enterDuration = 20 + rand() % 26;
        a.invincibleFrames = 0;
        a.fireVolleyTimer = 0; a.fireBurstCount = 0; a.fireBurstTimer = 0;
        a.lastHitByPulse = -1;
        aliens.push_back(a);
    }

public:
    Ch2AlienManager(int& hp, bool& go) : Ch2ShooterBase(hp, go) {}
    void reset() { aliens.clear(); resetBase(); }
    void forceSpawn() { spawnOne(); }
    int countLiving() const {
        int cnt = 0;
        for (const auto& a : aliens) { if (!a.defeated && a.active) cnt++; }
        return cnt;
    }
    const std::vector<Ch2Alien>& getAliens() const { return aliens; }

    void update(BulletManager& bulletMgr, ParticleManager& particleMgr, AudioEngine& audio,
                int& score, Player& pl, FloatingTextManager& ftMgr, int& hitCount) {
        for (auto& a : aliens) {
            if (!a.active && !a.defeated) continue;
            if (a.defeated) continue;
            if (a.entering) {
                a.enterFrame++;
                double raw = (double)a.enterFrame / a.enterDuration;
                double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                if (raw >= 1.0) { raw = 1.0; eased = 1.0; a.entering = false; a.invincibleFrames = 3; a.fireVolleyTimer = 0; }
                a.x = a.startX + (a.targetX - a.startX) * eased;
                a.y = a.startY + (a.targetY - a.startY) * eased;
                continue;
            }
            if (a.invincibleFrames > 0) {
                a.invincibleFrames--;
                if (a.invincibleFrames <= 0) a.fireVolleyTimer = 0;
            } else {
                a.x -= 0.35;
                if (a.x < -40) { a.active = false; a.defeated = true; continue; }
                if (a.fireBurstCount > 0) {
                    a.fireBurstTimer--;
                    if (a.fireBurstTimer <= 0) {
                        a.fireBurstTimer = BURST_INTERVAL;
                        fireBullet(pl, a); audio.sndShoot();
                        a.fireBurstCount--;
                        if (a.fireBurstCount <= 0) a.fireVolleyTimer = VOLNEY_COOLDOWN;
                    }
                } else {
                    a.fireVolleyTimer--;
                    if (a.fireVolleyTimer <= 0) { a.fireBurstCount = BURST_COUNT; a.fireBurstTimer = 0; }
                }
            }
            // Player bullets vs alien collision
            if (!a.entering && a.invincibleFrames <= 0) {
                for (auto& b : bulletMgr.all()) {
                    if (!b.active || !b.canDamage) continue;
                    double dx = b.x - a.x, dy = b.y - a.y;
                    if (dx*dx + dy*dy < 22.0*22.0) {
                        b.active = false; b.canDamage = false; a.hp--; hitCount++;
                        particleMgr.spawnExplosion(b.x, b.y, 3); audio.sndHit();
                        if (a.hp <= 0) {
                            a.defeated = true; a.active = false;
                            particleMgr.spawnExplosion(a.x, a.y, 20);
                            particleMgr.spawnExplosion(a.x-8, a.y-4, 12);
                            particleMgr.spawnExplosion(a.x+8, a.y+4, 12);
                            audio.sndExplosionSmall(); score += 1;
                        }
                        break;
                    }
                }
            }
        }
        // Remove defeated/inactive aliens
        aliens.erase(std::remove_if(aliens.begin(), aliens.end(),
            [](const Ch2Alien& a){ return !a.active && !a.defeated; }), aliens.end());
        Ch2ShooterBase::updateBullets(bulletMgr, particleMgr, audio, pl, ftMgr, &hitCount);
    }

    void drawEnemy(SDL_Renderer* r) const {
        for (const auto& a : aliens) {
            if (!a.active) continue;
            int ex = (int)a.x, ey = (int)a.y, sz = 12;
            int rCol, gCol, bCol;
            if (a.entering || a.invincibleFrames > 0) { rCol=100; gCol=180; bCol=255; }
            else computeHPColor((double)a.hp / ALIEN_HP, rCol, gCol, bCol);
            if (a.entering && a.enterFrame > 3) {
                for (int k = 1; k <= 3; ++k) {
                    double pRaw = (double)(a.enterFrame - k*3) / a.enterDuration;
                    if (pRaw < 0) continue;
                    double pE = 1.0 - std::pow(1.0 - pRaw, 3.0);
                    int px = (int)(a.startX + (a.targetX - a.startX) * pE);
                    int py = (int)(a.startY + (a.targetY - a.startY) * pE);
                    SDL_SetRenderDrawColor(r, 100, 180, 255, (Uint8)(150 - k*40));
                    SDL_RenderDrawPoint(r, px, py);
                }
            }
            SDL_SetRenderDrawColor(r, (Uint8)rCol, (Uint8)gCol, (Uint8)bCol, 255);
            int hw = sz * 2 / 3;
            SDL_Point pts[4] = {
                {ex, ey - sz}, {ex + hw, ey},
                {ex, ey + sz}, {ex - hw, ey}
            };
            SDL_RenderDrawLines(r, pts, 4);
            SDL_RenderDrawLine(r, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
            SDL_RenderDrawLine(r, ex - hw, ey, ex + hw, ey);
        }
    }
};


// ============== ChapterManager ==============
// ============== Ch2SkillOrb (floating skill orb → shield break → core → absorption) ==============
struct ShieldDebris {
    double sx, sy, vx, vy;
    int life;
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
        audio.sndHit();
        if (shieldHp <= 0) breakShield(pm);
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
        int br = 180 + (int)(40 * std::sin(SDL_GetTicks() * 0.010));
        if (state == CORE || state == ABSORBING) {
            br = 180 + (int)(75 * std::sin(SDL_GetTicks() * 0.015));
        }
        for (int dy = -rsz; dy <= rsz; ++dy) {
            int dx = (int)std::sqrt((double)std::max(0, rsz*rsz - dy*dy));
            double edgeFrac = std::abs(dy) / (double)rsz;
            // Breathing brightness fades toward edges
            int rv = (int)(br - edgeFrac * 60 + 40);
            int gv = (int)(br - edgeFrac * 60 + 40);
            int bv = (int)(std::max(20, br/3 - (int)(edgeFrac * 30)));
            int al = (int)(240 - edgeFrac * 80);
            SDL_SetRenderDrawColor(r, (Uint8)rv, (Uint8)gv, (Uint8)bv, (Uint8)al);
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

    // Breathing: bright green ↔ dark green when full (for HUD rendering)
    float getBreathAlpha() const {
        if (!isFull()) return 1.0f;
        return 0.6f + 0.4f * (float)std::sin(SDL_GetTicks() * 0.008);
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
        for (const auto& w : waves) {
            if (!w.active) continue;
            double progress = (w.radius - 20.0) / 500.0; // 0→1 over life
            if (progress > 1.0) progress = 1.0;
            int alpha = (int)(220.0 * (1.0 - progress)); if (alpha < 10) alpha = 10;
            SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)alpha);
            const int SEG = 64;
            SDL_Point prev;
            int cx = (int)w.x, cy = (int)w.y;
            for (int i = 0; i <= SEG; ++i) {
                double a = 2.0 * M_PI * i / SEG;
                int sx = cx + (int)(w.radius * std::cos(a));
                int sy = cy + (int)(w.radius * std::sin(a));
                if (i > 0) SDL_RenderDrawLine(r, prev.x, prev.y, sx, sy);
                prev = {sx, sy};
            }
            // Second ring (thinner, slightly smaller)
            double r2 = w.radius * 0.85;
            int alpha2 = (int)(140.0 * (1.0 - progress)); if (alpha2 < 8) alpha2 = 8;
            SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)alpha2);
            for (int i = 0; i <= SEG; ++i) {
                double a = 2.0 * M_PI * i / SEG;
                int sx = cx + (int)(r2 * std::cos(a));
                int sy = cy + (int)(r2 * std::sin(a));
                if (i > 0) SDL_RenderDrawLine(r, prev.x, prev.y, sx, sy);
                prev = {sx, sy};
            }
        }
    }

    // Collision: pulse vs enemy bullets → destroy all bullets in radius
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
                    a.hp -= 1; hitCount++;
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
                    e.hp -= 1; hitCount++;
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

class ChapterManager {
    int currentIdx;
    bool unlocked[5];
    ChapterConfig chapters[5];

    void initChapter(int idx, const char* title, bool ul, int spawnInt, double alienSpd,
                     int fireDly, double bullSpd, int swDmg, int swInt, int bossTrig,
                     BossConfig bc, int hy, int gcR, int gcG, int gcB,
                     int scR, int scG, int scB, int stars, float starB) {
        chapters[idx].chapterNumber = idx + 1;
        chapters[idx].title = title;
        chapters[idx].unlocked = ul;
        chapters[idx].baseSpawnInterval = spawnInt;
        chapters[idx].baseAlienSpeed = alienSpd;
        chapters[idx].baseFireDelay = fireDly;
        chapters[idx].baseBulletSpeed = bullSpd;
        chapters[idx].baseShockwaveDamage = swDmg;
        chapters[idx].baseShockwaveInterval = swInt;
        chapters[idx].bossTriggerScore = bossTrig;
        chapters[idx].bossConfig = bc;
        chapters[idx].horizonY = hy;
        chapters[idx].groundColorR = gcR; chapters[idx].groundColorG = gcG; chapters[idx].groundColorB = gcB;
        chapters[idx].skyColorR = scR; chapters[idx].skyColorG = scG; chapters[idx].skyColorB = scB;
        chapters[idx].starCount = stars;
        chapters[idx].starBrightness = starB;
        chapters[idx].alienTypesMask = 1;
        chapters[idx].fastAlienChance = 0;
        chapters[idx].tankAlienChance = 0;
        chapters[idx].hasMeteorShowers = false;
        chapters[idx].hasMovingBase = false;
        chapters[idx].hasTimeLimit = false;
        chapters[idx].isSideScrolling = false;
        chapters[idx].timeLimitSeconds = 0;
    }

public:
    ChapterManager() : currentIdx(0) {
        for (int i = 0; i < 5; ++i) unlocked[i] = (i == 0);

        // Chapter 1: 默认参数（与当前游戏一致）
        BossConfig bc1 = {1000, 140.0, 25.0, 0.025, 80.0, 150, 70, 50, 420, 30, 60, true, true, "TELAMONDO"};
        initChapter(0, "FIRST FLIGHT", true, 110, 1.75, 7, 60.0, 3, 12, 200,
                   bc1, 200, 80,80,80, 50,50,50, 100, 1.0f);

        // Chapter 2: 侧滚廊桥 (Boss待设计，暂用Ch1配置)
        initChapter(1, "DEEP SPACE", false, 100, 2.0, 6, 65.0, 3, 11, 220,
                   bc1, 200, 60,60,80, 40,40,60, 120, 1.0f);
        chapters[1].isSideScrolling = true;

        // Chapters 3-5: Boss待设计，暂用Ch1配置
        initChapter(2, "ENEMY FORTRESS", false, 90, 2.25, 6, 65.0, 4, 10, 250,
                   bc1, 200, 40,60,40, 30,40,30, 130, 1.0f);
        initChapter(3, "ASTEROID BELT", false, 80, 2.5, 5, 70.0, 4, 9, 250,
                   bc1, 200, 70,50,30, 50,40,30, 140, 1.0f);
        initChapter(4, "FINAL ASSAULT", false, 70, 2.75, 5, 75.0, 5, 8, 280,
                   bc1, 200, 60,30,30, 35,25,25, 150, 1.0f);
    }

    void selectChapter(int idx) { currentIdx = idx; }
    int getCurrentIndex() const { return currentIdx; }
    bool isUnlocked(int idx) const { return unlocked[idx]; }
    void unlockChapter(int idx) { unlocked[idx] = true; }
    const ChapterConfig& getConfig() const { return chapters[currentIdx]; }
    const ChapterConfig& getChapterConfig(int idx) const { return chapters[idx]; }
    void reset() { currentIdx = 0; }
};


// ============== UIRenderer ==============
class UIRenderer {
public:
    static void drawMenuCursor(SDL_Renderer* r, int x, int y, int size = 0) {
        int s = size > 0 ? size : 10;
        SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
        SDL_RenderDrawLine(r, x, y, x + s, y + s/2);
        SDL_RenderDrawLine(r, x, y, x + s, y - s/2);
        SDL_RenderDrawLine(r, x + s, y - s/2, x + s, y + s/2);
    }

    static void drawMenuUnderline(SDL_Renderer* r, int x, int y, int w) {
        SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
        SDL_RenderDrawLine(r, x, y, x + w, y);
    }

    static void drawSlider(SDL_Renderer* r, int x, int y, int w, int val, int lo, int hi, bool sym) {
        int range = hi - lo;
        int fillW = (val - lo) * w / range;
        SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
        SDL_Rect bg = {x, y, w, 12};
        SDL_RenderFillRect(r, &bg);
        if (sym) {
            int zeroX = x + w / 2;
            if (fillW >= w / 2) {
                SDL_SetRenderDrawColor(r, 100, 220, 100, 255);
                SDL_Rect fg = {zeroX, y, fillW - w/2, 12};
                SDL_RenderFillRect(r, &fg);
            } else {
                SDL_SetRenderDrawColor(r, 220, 100, 100, 255);
                SDL_Rect fg = {zeroX + fillW - w/2, y, w/2 - fillW, 12};
                SDL_RenderFillRect(r, &fg);
            }
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            SDL_RenderDrawLine(r, zeroX, y, zeroX, y + 12);
        } else {
            SDL_SetRenderDrawColor(r, 100, 200, 100, 255);
            SDL_Rect fg = {x, y, fillW, 12};
            SDL_RenderFillRect(r, &fg);
        }
        SDL_SetRenderDrawColor(r, 150, 150, 150, 255);
        SDL_RenderDrawRect(r, &bg);
    }

    static void drawHalfTransparentOverlay(SDL_Renderer* r, int alpha) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)alpha);
        for (int yy = 0; yy < WIN_HEIGHT; yy += 3) {
            SDL_Rect rect = {0, yy, WIN_WIDTH, 2};
            SDL_RenderFillRect(r, &rect);
        }
    }
};


// ============== MenuStateMachine ==============
class MenuStateMachine {
public:
    enum ScreenType { NONE, START, CHAPTER, TEST, OPTIONS, SOUND, PAUSE, GAMEOVER, MISSION_COMPLETE };
    enum Action { ACT_NONE, ACT_START_GAME, ACT_SELECT_CHAPTER, ACT_BACK_TO_MENU, ACT_QUIT,
                  ACT_RESUME, ACT_RESTART, ACT_TOGGLE_OPTION, ACT_OPTIONS, ACT_SOUND,
                  ACT_BACK_TO_MAIN, ACT_TEST_START, ACT_CHAPTER_START, ACT_EXIT_PAUSE_OPTIONS };

    struct StateSnapshot {
        ScreenType screen;
        int cursor;
        int extraData;
    };

private:
    ScreenType currentScreen;
    int cursor;
    bool justEntered;
    // Edge detection
    bool upWas, downWas, enterWas, escWas, leftWas, rightWas;
    // Extra
    int soundCursor;
    bool optionFromPause;

public:
    MenuStateMachine() : currentScreen(START), cursor(0), justEntered(true),
        upWas(false), downWas(false), enterWas(false), escWas(false),
        leftWas(false), rightWas(false), soundCursor(0), optionFromPause(false) {}

    void enter(ScreenType s) {
        currentScreen = s;
        cursor = 0;
        justEntered = true;
        upWas = downWas = enterWas = escWas = leftWas = rightWas = false;
    }

    void enterSound() { soundCursor = 0; justEntered = true; }

    ScreenType getScreen() const { return currentScreen; }
    int getCursor() const { return cursor; }
    int getSoundCursor() const { return soundCursor; }

    void update(const Uint8* keys) {
        bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        bool escNow   = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
        bool leftNow  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];

        if (justEntered) {
            upWas = upNow; downWas = downNow; enterWas = enterNow;
            escWas = escNow; leftWas = leftNow; rightWas = rightNow;
            justEntered = false;
            return;
        }

        int maxItems = 5;
        switch (currentScreen) {
            case START:    maxItems = 5; break;
            case CHAPTER:  maxItems = 5; break;
            case TEST:     maxItems = 9; break;
            case OPTIONS:  maxItems = 2; break;
            case SOUND:    maxItems = 6; break;
            case PAUSE:    maxItems = 5; break;
            case GAMEOVER: maxItems = 2; break;
            default: return;
        }

        if (upNow && !upWas) {
            if (currentScreen == SOUND && soundCursor > 0) soundCursor--;
            else cursor = (cursor - 1 + maxItems) % maxItems;
        }
        if (downNow && !downWas) {
            if (currentScreen == SOUND && soundCursor < maxItems - 1) soundCursor++;
            else cursor = (cursor + 1) % maxItems;
        }
        upWas = upNow; downWas = downNow; enterWas = enterNow;
        escWas = escNow; leftWas = leftNow; rightWas = rightNow;
    }

    bool isEnterPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        return enterNow && !enterWas;
    }

    bool isEscPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool escNow = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
        return escNow && !escWas;
    }

    bool isLeftPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool leftNow = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        return leftNow && !leftWas;
    }

    bool isRightPressed() const {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        return rightNow && !rightWas;
    }

    void setOptionFromPause(bool v) { optionFromPause = v; }
    bool getOptionFromPause() const { return optionFromPause; }
};


// ============== Game 类 ==============
// Small helper: reads the 4 standard menu keys at once
struct MenuKeys {
    bool up, down, enter, esc;
    MenuKeys(const Uint8* keys) :
        up(keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]),
        down(keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]),
        enter(keys[SDL_SCANCODE_RETURN]),
        esc(keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE]) {}
};

enum GamePhase { PHASE_PLAY, PHASE_BOSS_INTRO, PHASE_BOSS_FIGHT, PHASE_BOSS_PHASE2, PHASE_BOSS_DEFEAT };

class Game {
    Renderer& renderer;
    AudioEngine& audio;
    Font font;

    TrainingPlane trainingPlane;
    Ch2Trainer ch2Trainer;
    NightElf nightElf;
    Player* player;
    BulletManager bulletMgr;
    Ch1AlienManager alienMgr;
    ParticleManager particleMgr;
    Ch1ShockwaveManager shockwaveMgr;
    Ch1Boss boss;
    FloatingTextManager floatingTextMgr;
    NarrationSystem narration;
    DialogueSystem dialogueSys;
    ChapterManager chapterMgr;

    SDL_Texture* shakeTex;

    // Core state
    GamePhase phase;
    int score;
    int baseHP;
    int difficultyTimer;
    bool paused;
    bool gameOver;
    bool aimAssistOn;
    bool inNarration;
    bool ch1DialogueDone;
    bool bossPhase2DialogueTriggered;
    bool triggeredScores[256];
    int baseFireTimer;
    int lastScore;
    bool pauseHistoryFocused;
    bool enemiesEnabled;

    // Screens
    bool atStartScreen, atTestSelect, atChapterSelect, atOptionScreen, atSoundMenu;
    bool optionFromPause;
    bool optionJustEntered;

    // Menu state
    int startMenuSelection, testScoreSelection, chapterSelection, menuSelection;
    int pauseMenuSelection, optionCursor;
    int testChapterSelection;
    bool testAtChapterSelect;

    // Countdown
    int countdown, countdownFrame;

    // Sound menu cursor
    int soundCursor;

    // Ch1Boss defeat sequence
    int bossDefeatTimer, defeatAlienTimer, defeatReturnTimer, defeatFWTimer;
    int defeatMCDelay, defeatFadeTimer;
    bool missionCompleteShown, missionComplete;
    int mcMenuSelection;

    // Chapter unlock tracking
    bool isNormalPlay;

    // Ch2 energy barrier (right wall only)
    int wallFlashTimer;
    int wallContactY;      // Y position where plane touched the barrier
    int wallAnimFrame;     // animates lightning/sparks while touching

    // Ch2 enemy systems
    int ch2PlayerHP; bool ch2GameOver;
    Ch2AlienManager ch2AlienMgr;
    Ch2DanmakuManager dmMgr;
    int dmFireCooldown;    // player fire cooldown in side-scroll mode
    Ch2SphereBoss sphereBoss;
    bool sphereBossActive;
    NightElfEnergy nightElfEnergy;
    int playerHitCount;
    int tripleBeepCounter;
    Ch2PulseSystem pulseSystem;
    Ch2SkillOrb skillOrb;
    bool pulseOrbDropped; // first danmaku has dropped the orb
    bool shiftWas;        // Shift key edge detection
    bool shiftJustPressed; // true for one frame when Shift first pressed

    // Timing
    Uint32 lastTime;

    // Edge detection helpers for menus
    bool upWas, downWas, enterWas, escWas, leftWas, rightWas;
    int autoSpawnPhase;    // 0=idle,1=spawnW1(3),2=fightW1,3=spawnW2(5),4=fightW2,5=wave3,6=danmaku+done
    int autoSpawnQueued;   // aliens left to spawn in current spawning phase
    int autoSpawnTimer;    // countdown frames to next spawn (0.2s=12)
    int autoSpawnWave3Reinf;  // wave3 reinforcement rounds done (0-4)
    int autoSpawnScoreBase;   // score when auto-spawn activated (for kill counting across all waves)
    int autoSpawnAliveLast;   // alive+queued snapshot for delta escape detection
    int autoSpawnKillsLast;   // kills snapshot for delta
    int lastShockwaveLevel;

    // Ch1Background (per chapter, persistent)
    Ch1Background* background;
    Ch2Background* sideBg;

public:
    Game(Renderer& r, AudioEngine& a, SDL_Window*)
        : renderer(r), audio(a),
          player(&trainingPlane), shakeTex(nullptr),
          phase(PHASE_PLAY), score(0), baseHP(10), difficultyTimer(0),
          paused(false), gameOver(false), aimAssistOn(false), inNarration(false), ch1DialogueDone(false), bossPhase2DialogueTriggered(false), baseFireTimer(0), lastScore(-1), pauseHistoryFocused(false), enemiesEnabled(false),
          atStartScreen(true), atTestSelect(false), atChapterSelect(false),
          atOptionScreen(false), atSoundMenu(false), optionFromPause(false), optionJustEntered(true),
          startMenuSelection(0), testScoreSelection(0), chapterSelection(0), menuSelection(0),
          pauseMenuSelection(0), optionCursor(0), testChapterSelection(0), testAtChapterSelect(true),
          countdown(-1), countdownFrame(0), soundCursor(0),
          bossDefeatTimer(0), defeatAlienTimer(0), defeatReturnTimer(0), defeatFWTimer(0),
          defeatMCDelay(0), defeatFadeTimer(0),
          missionCompleteShown(false), missionComplete(false), mcMenuSelection(0),
          isNormalPlay(false),
          wallFlashTimer(0), wallContactY(0), wallAnimFrame(0),
          ch2PlayerHP(3), ch2GameOver(false),
          ch2AlienMgr(ch2PlayerHP, ch2GameOver), dmMgr(ch2PlayerHP, ch2GameOver),
          dmFireCooldown(0), sphereBossActive(false), playerHitCount(0), tripleBeepCounter(0),
          pulseOrbDropped(false), shiftWas(true), shiftJustPressed(false),
          lastTime(0), upWas(false), downWas(false), enterWas(false), escWas(false),
          leftWas(false), rightWas(false),
          autoSpawnPhase(0), autoSpawnQueued(0), autoSpawnTimer(0),
          autoSpawnWave3Reinf(0), autoSpawnScoreBase(0),
          autoSpawnAliveLast(0), autoSpawnKillsLast(0),
          lastShockwaveLevel(0),
          background(nullptr), sideBg(nullptr) {
        boss.setConfig(&chapterMgr.getConfig().bossConfig);
        background = new Ch1Background(chapterMgr.getConfig());
        sideBg = new Ch2Background();
    }

    ~Game() { delete background; delete sideBg; }

    void resetGame() {
        score = 0;
        gameOver = false;
        atStartScreen = true;
        atTestSelect = false;
        atChapterSelect = false;
        atOptionScreen = false;
        atSoundMenu = false;
        menuSelection = 0;
        startMenuSelection = 0;
        testScoreSelection = 0;
        testChapterSelection = 0;
        testAtChapterSelect = true;
        chapterSelection = 0;
        pauseMenuSelection = 0;
        difficultyTimer = 0;
        lastShockwaveLevel = 0;
        baseHP = 10;
        if (chapterMgr.getConfig().isSideScrolling) {
            ch2Trainer.reset(); player = &ch2Trainer;
        } else {
            trainingPlane.reset(); player = &trainingPlane;
        }
        bulletMgr.reset();
        alienMgr.reset();
        particleMgr.all().clear();
        shockwaveMgr.reset();
        boss.reset();
        boss.setConfig(&chapterMgr.getConfig().bossConfig);
        floatingTextMgr.clear();
                narration.reset(); dialogueSys.reset(); inNarration = false; ch1DialogueDone = false; bossPhase2DialogueTriggered = false;
        memset(triggeredScores, 0, sizeof(triggeredScores)); baseFireTimer = 0; lastScore = -1; enemiesEnabled = false;
        phase = PHASE_PLAY;
        paused = false;
        missionComplete = false; missionCompleteShown = false; mcMenuSelection = 0;
        wallFlashTimer = 0; wallContactY = 0; wallAnimFrame = 0;
        ch2AlienMgr.reset(); dmMgr.reset(); dmFireCooldown = 0;
        sphereBoss.reset(); sphereBossActive = false;
        nightElfEnergy.reset(); playerHitCount = 0; tripleBeepCounter = 0;
        pulseSystem.reset(); skillOrb.reset(); pulseOrbDropped = false; shiftWas = true;
        autoSpawnPhase = 0; autoSpawnQueued = 0; autoSpawnTimer = 0;
        autoSpawnWave3Reinf = 0; autoSpawnScoreBase = 0;
        autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
        bossDefeatTimer = 0; defeatAlienTimer = 0; defeatReturnTimer = 0;
        defeatFWTimer = 0; defeatMCDelay = 0; defeatFadeTimer = 0;
        countdown = -1; countdownFrame = 0;
        soundCursor = 0;
        if (background) { delete background; background = new Ch1Background(chapterMgr.getConfig()); }
        if (sideBg) sideBg->reset();
    }

    void run() {
        lastTime = SDL_GetTicks();
        bool running = true;
        SDL_Event e;

        while (running) {
            bool escPressed = false;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) escPressed = true;
            }

            const Uint8* keys = SDL_GetKeyboardState(NULL);
            audio.setBossMusic(phase == PHASE_BOSS_FIGHT && boss.isActive());
            audio.setBgmOff(atStartScreen || atChapterSelect || atTestSelect || atOptionScreen || atSoundMenu || paused || gameOver || missionComplete);
            audio.setCh2Bgm(chapterMgr.getConfig().isSideScrolling &&
                !atStartScreen && !atChapterSelect && !atTestSelect && !atOptionScreen && !atSoundMenu && !gameOver);
            if (background) background->update();
            if (sideBg) sideBg->update();

            // ======== Esc key global ========
            if (escPressed && inNarration) {
                // ESC skips entire opening narration
                narration.reset(); inNarration = false;
            } else if (escPressed && !inNarration && !gameOver && !atStartScreen && !atTestSelect && !atChapterSelect
                && !atOptionScreen && !atSoundMenu && !missionComplete) {
                if (paused && countdown == -1) {
                    countdown = 3; countdownFrame = 0;
                } else {
                    paused = !paused;
                    pauseMenuSelection = 0;
                    if (paused) { pauseHistoryFocused = false; dialogueSys.history.resetView(); }
                }
            }

            // ======== Narrations ========
            if (inNarration) {
                updateNarration(keys);
                drawNarrationFrame();
            } else if (atStartScreen) {
                updateStartScreen(keys, running);
                drawStartScreen();
            } else if (atChapterSelect) {
                updateChapterScreen(keys);
                drawChapterScreen();
            } else if (atTestSelect) {
                updateTestScreen(keys);
                drawTestScreen();
            } else if (atOptionScreen && !atSoundMenu) {
                updateOptionScreen(keys);
                drawOptionScreen();
            } else if (atSoundMenu) {
                updateSoundMenu(keys);
                drawSoundMenu();
            } else if (gameOver) {
                updateGameOverScreen(keys, running);
                drawGameplayFrame();
                drawGameOverScreen();
            } else if (paused) {
                updatePaused(keys, running);
                drawGameplayFrame();
                if (countdown >= 0) drawCountdown();
                else if (paused) drawPauseMenu();
            } else {
                // ======== GAMEPLAY ========
                updateGameplay(keys);
                drawGameplayFrame();
                if (missionComplete) drawMissionComplete();
                if (paused && countdown >= 0) drawCountdown();
                else if (paused) drawPauseMenu();
            }

            renderer.present();

            Uint32 now = SDL_GetTicks();
            Uint32 elapsed = now - lastTime;
            if (elapsed < 16) SDL_Delay(16 - elapsed);
            lastTime = SDL_GetTicks();
        }
    }

private:
    // ======== START SCREEN ========
    void updateStartScreen(const Uint8* keys, bool& running) {
        static bool sJustEntered = true;
        bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];

        if (sJustEntered) {
            upWas = upNow; downWas = downNow; enterWas = enterNow;
            sJustEntered = false;
        }
        if (upNow && !upWas && startMenuSelection > 0)     startMenuSelection--;
        if (downNow && !downWas && startMenuSelection < 4) startMenuSelection++;
        if (enterNow && !enterWas) {
            if (startMenuSelection == 0) {
                resetGame(); atStartScreen = false;
                isNormalPlay = true;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                startChapterNarration();
                sJustEntered = true;
            } else if (startMenuSelection == 1) {
                atStartScreen = false; atChapterSelect = true;
                chapterSelection = 0; sJustEntered = true;
            } else if (startMenuSelection == 2) {
                atStartScreen = false; atTestSelect = true;
                testScoreSelection = 0; testChapterSelection = 0;
                testAtChapterSelect = true; sJustEntered = true;
            } else if (startMenuSelection == 3) {
                atStartScreen = false; atOptionScreen = true;
                optionFromPause = false; optionCursor = 0;
                optionJustEntered = true; sJustEntered = true;
            } else running = false;
        }
        upWas = upNow; downWas = downNow; enterWas = enterNow;
    }

    void drawStartScreen() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);

        font.drawString(r, "HUNTRESS", CENTER_X - 96, 70, 4);
        font.drawString(r, "ECHOES FROM THE WELL", CENTER_X - 117, 120, 3);
        SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
        SDL_RenderDrawLine(r, CENTER_X - 200, 140, CENTER_X + 200, 140);
        SDL_RenderDrawLine(r, CENTER_X - 200, 142, CENTER_X + 200, 142);

        const char* items[5] = {"PLAY", "CHAPTER", "TEST", "OPTIONS", "EXIT"};
        const int MENU_Y0 = 210, GAP = 48;
        for (int i = 0; i < 5; ++i) {
            int itemW = (int)strlen(items[i]) * 6 * 4;
            int itemX = CENTER_X - itemW / 2;
            int itemY = MENU_Y0 + i * GAP;
            font.drawString(r, items[i], itemX, itemY, 4);
            if (i == startMenuSelection) {
                UIRenderer::drawMenuCursor(r, itemX - 30, itemY + 14, 12);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 32, itemW);
            }
        }
        font.drawString(r, "W/S:select  ENTER:confirm", CENTER_X - 150, 490, 2);
        SDL_SetRenderDrawColor(r, 120, 120, 120, 255);
        font.drawString(r, "Ver 1.2.20 (archived)", 15, WIN_HEIGHT - 30, 2);
    }

    // ======== CHAPTER SCREEN ========
    void updateChapterScreen(const Uint8* keys) {
        static bool cJustEntered = true;
        MenuKeys mk(keys);
        if (cJustEntered) { upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc; cJustEntered=false; }
        if (mk.up && !upWas)    { int cs = chapterSelection; while (cs > 0) { cs--; if (chapterMgr.isUnlocked(cs)) { chapterSelection = cs; break; } } }
        if (mk.down && !downWas) { int cs = chapterSelection; while (cs < 4) { cs++; if (chapterMgr.isUnlocked(cs)) { chapterSelection = cs; break; } } }
        if (mk.enter && !enterWas) {
            if (chapterMgr.isUnlocked(chapterSelection)) {
                chapterMgr.selectChapter(chapterSelection);
                resetGame(); atStartScreen = false; atChapterSelect = false;
                isNormalPlay = true;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                startChapterNarration();
            }
        }
        if (mk.esc && !escWas) { atChapterSelect = false; atStartScreen = true; cJustEntered = true; }
        upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc;
    }

    void drawChapterScreen() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        font.drawString(r, "SELECT CHAPTER", CENTER_X - 180, 60, 4);
        SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
        SDL_RenderDrawLine(r, CENTER_X - 180, 100, CENTER_X + 180, 100);
        const char* chLabels[5] = {"CHAPTER 1", "CHAPTER 2", "CHAPTER 3", "CHAPTER 4", "CHAPTER 5"};
        const int Y0 = 150, GAP = 55;
        for (int i = 0; i < 5; ++i) {
            int itemW = (int)strlen(chLabels[i]) * 6 * 3;
            int itemX = CENTER_X - itemW / 2;
            int itemY = Y0 + i * GAP;
            bool locked = !chapterMgr.isUnlocked(i);
            SDL_SetRenderDrawColor(r, locked ? 80 : 255, locked ? 80 : 255, locked ? 80 : 255, 255);
            font.drawString(r, chLabels[i], itemX, itemY, 3);
            if (i == chapterSelection) {
                UIRenderer::drawMenuCursor(r, itemX - 24, itemY + 10, 10);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
            }
            if (locked) font.drawString(r, "(LOCKED)", itemX + itemW + 10, itemY, 2);
        }
        font.drawString(r, "W/S:select  ENTER:start  ESC:back", CENTER_X - 210, 490, 2);
    }

    // ======== TEST SCREEN ========
    void updateTestScreen(const Uint8* keys) {
        static bool tJustEntered = true;
        MenuKeys mk(keys);
        if (tJustEntered) { upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc; tJustEntered=false; return; }
        if (testAtChapterSelect) {
            // Level 1: Chapter selection
            if (mk.esc && !escWas) { atTestSelect = false; atStartScreen = true; tJustEntered = true; }
            if (mk.up && !upWas && testChapterSelection > 0)       testChapterSelection--;
            if (mk.down && !downWas && testChapterSelection < 4)   testChapterSelection++;
            if (mk.enter && !enterWas) {
                if (testChapterSelection == 0) { chapterMgr.selectChapter(0); testAtChapterSelect = false; tJustEntered = true; }
                else if (testChapterSelection == 1) {
                    chapterMgr.selectChapter(1);
                    testAtChapterSelect = false; testScoreSelection = 0; tJustEntered = true;
                }
            }
        } else {
            // Level 2: sub-menu for selected chapter
            bool isCh2 = chapterMgr.getConfig().isSideScrolling;
            int maxSel = isCh2 ? 2 : 9;
            if (mk.esc && !escWas) { testAtChapterSelect = true; tJustEntered = true; }
            if (mk.up && !upWas && testScoreSelection > 0)     testScoreSelection--;
            if (mk.down && !downWas && testScoreSelection < maxSel) testScoreSelection++;
        if (mk.enter && !enterWas) {
            int savedSel = testScoreSelection;
            if (isCh2) {
                // Chapter 2 sub-menu: two entry points
                resetGame(); atStartScreen = false; atTestSelect = false;
                isNormalPlay = false;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                dmFireCooldown = 0;
                if (savedSel == 0) {
                    // Option 0: Full boss entry (entrance animation → fight → debris → combat)
                    sphereBoss.init(sideBg, player);
                    sphereBossActive = true;
                    sphereBoss.startEntering();
                } else if (savedSel == 1) {
                    // Option 1: Skip to combat (boss done, auto-spawn wave 1)
                    autoSpawnPhase = 1; autoSpawnQueued = 3; autoSpawnTimer = 0;
                } else {
                    // Option 2: Score 25 + first danmaku spawn (for pulse orb testing)
                    score = 25; autoSpawnPhase = 6; autoSpawnScoreBase = 0;
                    dmMgr.spawnEnemy();
                }
                tJustEntered = true; return;
            }
            // Chapter 1 sub-menu: score/target selection
            resetGame();
            atTestSelect = false; tJustEntered = true;
            atStartScreen = false;
            isNormalPlay = false;
            alienMgr.applyChapterConfig(chapterMgr.getConfig());
            bulletMgr.updateParams(0);
            shockwaveMgr.updateParams(0);
            const int testScores[7] = {0, 30, 60, 90, 120, 150, 180};
            if (savedSel < 7) {
                score = testScores[savedSel];
                shockwaveMgr.setPending(true);
                bulletMgr.updateParams(score / 30);
                shockwaveMgr.updateParams(score / 30);
                // Score 0: use opening delay like normal play
                if (score == 0) { lastScore = -1; }
                else { lastScore = score; enemiesEnabled = true; }
            } else if (savedSel == 7) {
                // 200 BOSS: 直接进入Boss登场动画
                score = 200; enemiesEnabled = true;
                bulletMgr.updateParams(score / 30);
                shockwaveMgr.updateParams(score / 30);
                boss.trigger();
                phase = PHASE_BOSS_INTRO;
                alienMgr.setAllInvincible();
                for (int i = 0; i < 5; ++i) {
                    Ch1Alien a;
                    a.targetT = 0.15 + (rand() % 700) / 1000.0;
                    a.t = a.targetT; a.y = 120.0 + (rand() % 180);
                    a.entering = false; a.enterFromTop = false; a.enterFromBoss = false;
                    a.invincibleFrames = -1; a.lastHitBySW = -1; a.lastHealHit = -1;
                    a.absorbFrame = 0; a.absorbDuration = 0;
                    a.absorbStartX = 0; a.absorbStartY = 0; a.beingAbsorbed = false;
                    a.alienType = 0;
                    a.hp = 3 + rand() % 3; a.maxHp = a.hp;
                    a.active = true;
                    alienMgr.pushAlien(a);
                }
            } else if (savedSel == 8) {
                // BOSS PH.2: 直接进入Boss二阶段
                score = 200; enemiesEnabled = true;
                bulletMgr.updateParams(score / 30);
                shockwaveMgr.updateParams(score / 30);
                boss.setY(90);
                boss.hpRef() = 500; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true);
                boss.enteringRef() = false;
                boss.shakeTimerRef() = 0;
                boss.phase2TriggeredRef() = true;
                boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_PHASE2;
                boss.setCh1HealWavesEnabled(false);
                for (int i = 0; i < 5; ++i) {
                    Ch1Alien a;
                    a.targetT = 0.15 + (rand() % 700) / 1000.0;
                    a.t = a.targetT; a.y = 120.0 + (rand() % 180);
                    a.entering = false; a.enterFromTop = false; a.enterFromBoss = false;
                    a.invincibleFrames = -1; a.lastHitBySW = -1; a.lastHealHit = -1;
                    a.absorbFrame = 0; a.absorbDuration = 0;
                    a.absorbStartX = 0; a.absorbStartY = 0; a.beingAbsorbed = false;
                    a.alienType = 0;
                    a.hp = 3 + rand() % 3; a.maxHp = a.hp;
                    a.active = true;
                    alienMgr.pushAlien(a);
                }
            } else {
                // BOSS 1HP: 快速检验战败流程
                score = 200; enemiesEnabled = true;
                bulletMgr.updateParams(score / 30);
                shockwaveMgr.updateParams(score / 30);
                boss.setY(90);
                boss.hpRef() = 1; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true);
                boss.enteringRef() = false;
                boss.shakeTimerRef() = 0;
                boss.phase2TriggeredRef() = true;
                boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_FIGHT;
                boss.setCh1HealWavesEnabled(true);
                shockwaveMgr.setPending(true);
            }
            // Pre-populate Ch1 dialogue history for test mode
            if (chapterMgr.getConfig().chapterNumber == 1 && score > 0) {
                #define PH(scr, spk, ...) \
                    if (score > scr) { \
                        triggeredScores[scr] = true; \
                        dialogueSys.history.add(spk, __VA_ARGS__); \
                    }
                PH(0, "Ally (ai copilot)", {"Martha, you're the only one in the air.", "Hold on as long as you can. The base shockwave cannon is charging."}, 2);
                PH(3, "Ally (ai copilot)", {"These enemies are made of energy.", "Destroy them. We can collect the energy."}, 2);
                PH(15, "", {"Tower communication restored."}, 1);
                PH(20, "Tower (ai)", {"Shockwave cannon ready."}, 1);
                if (score > 20) dialogueSys.history.add("Bryssa from Tower", {"A little more energy!"}, 1);
                PH(30, "Tower (ai)", {"Defense system charged.", "More enemies incoming. Keep gathering energy."}, 2);
                PH(40, "Ally (ai copilot)", {"Stay strong, Martha!"}, 1);
                if (score > 40) {
                    dialogueSys.history.add("Bryssa from Tower", {"The trainer shares energy with the base.", "You and the base will upgrade together."}, 2);
                }
                PH(50, "Tower (ai)", {"Keep gathering energy."}, 1);
                PH(55, "Ally (ai copilot)", {"System checking.", "Done."}, 2);
                PH(61, "Tower (ai)", {"Base upgraded again."}, 1);
                if (score > 61) dialogueSys.history.add("Bryssa from Tower", {"Radar shows even more enemies! Watch out!"}, 1);
                PH(70, "Ally (ai copilot)", {"System checking result:", "Aiming assist system on this plane.", "You shall find it somewhere."}, 3);
                PH(80, "Ally (ai copilot)", {"I've lost contact with the tower!", "But you and the base can upgrade again soon."}, 2);
                PH(105, "", {"Tower communication restored."}, 1);
                if (score > 105) {
                    dialogueSys.history.add("Tower (ai)", {"Massive energy signature detected.", "Analyzing source..."}, 2);
                }
                PH(120, "Bryssa from Tower", {"It's a capital ship.", "Telamondo-class. Martha, this is what the defense system was built for."}, 2);
                PH(160, "Tower (ai)", {"Enemy capital ship approaching.", "Entering weapons range in 40 seconds."}, 2);
                PH(180, "Bryssa from Tower", {"Shockwave Defense System is fully charged", "Martha, just keep them off us!"}, 2);
                PH(195, "Ally (ai copilot)", {"Here it comes...!"}, 1);
                #undef PH
            }
            lastScore = score;
        }
    }
        upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc;
    }

    void drawTestScreen() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);

        if (testAtChapterSelect) {
            // Level 1: Chapter selection
            font.drawString(r, "TEST - SELECT CHAPTER", CENTER_X - 234, 60, 4);
            SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
            SDL_RenderDrawLine(r, CENTER_X - 180, 100, CENTER_X + 180, 100);
            const char* chLabels[5] = {"CHAPTER 1", "CHAPTER 2", "CHAPTER 3", "CHAPTER 4", "CHAPTER 5"};
            const int Y0 = 150, GAP = 55;
            for (int i = 0; i < 5; ++i) {
                int itemW = (int)strlen(chLabels[i]) * 6 * 3;
                int itemX = CENTER_X - itemW / 2;
                int itemY = Y0 + i * GAP;
                bool locked = (i > 1); // Chapter 1 & 2 unlocked for testing
                SDL_SetRenderDrawColor(r, locked ? 80 : 255, locked ? 80 : 255, locked ? 80 : 255, 255);
                font.drawString(r, chLabels[i], itemX, itemY, 3);
                if (i == testChapterSelection) {
                    UIRenderer::drawMenuCursor(r, itemX - 24, itemY + 10, 10);
                    UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
                }
                if (locked) font.drawString(r, "(LOCKED)", itemX + itemW + 10, itemY, 2);
            }
            font.drawString(r, "W/S:select  ENTER:enter  ESC:back", CENTER_X - 216, 490, 2);
        } else {
            // Level 2: Sub-menu for selected chapter
            bool isCh2 = chapterMgr.getConfig().isSideScrolling;
            if (isCh2) {
                font.drawString(r, "TEST - CHAPTER 2", CENTER_X - 192, 50, 4);
                SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
                SDL_RenderDrawLine(r, CENTER_X - 180, 90, CENTER_X + 180, 90);
                const char* labels[3] = {"SPHERE BOSS FULL", "COMBAT ONLY", "PULSE ORB TEST"};
                const int MENU_Y0 = 140, GAP = 52;
                for (int i = 0; i < 3; ++i) {
                    int itemW = (int)strlen(labels[i]) * 6 * 3;
                    int itemX = CENTER_X - itemW / 2;
                    int itemY = MENU_Y0 + i * GAP;
                    font.drawString(r, labels[i], itemX, itemY, 3);
                    if (i == testScoreSelection) {
                        UIRenderer::drawMenuCursor(r, itemX - 24, itemY + 10, 10);
                        UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
                    }
                }
                font.drawString(r, "W/S:select  ENTER:start  ESC:back", CENTER_X - 210, 490, 2);
            } else {
                font.drawString(r, "TEST - CHAPTER 1", CENTER_X - 192, 50, 4);
                SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
                SDL_RenderDrawLine(r, CENTER_X - 180, 90, CENTER_X + 180, 90);
                const char* testLabels[10] = {"0", "30", "60", "90", "120", "150", "180", "200 BOSS", "BOSS PH.2", "BOSS 1HP"};
                const int MENU_Y0 = 130, GAP = 48;
                for (int i = 0; i < 10; ++i) {
                    int itemW = (int)strlen(testLabels[i]) * 6 * 3;
                    int itemX = CENTER_X - itemW / 2;
                    int itemY = MENU_Y0 + i * GAP;
                    font.drawString(r, testLabels[i], itemX, itemY, 3);
                    if (i == testScoreSelection) {
                        UIRenderer::drawMenuCursor(r, itemX - 24, itemY + 10, 10);
                        UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
                    }
                }
                font.drawString(r, "W/S:select  ENTER:start  ESC:back", CENTER_X - 210, 490, 2);
            }
        }
    }

    // ======== OPTIONS SCREEN ========
    void updateOptionScreen(const Uint8* keys) {
        static bool oJustEntered = true;
        MenuKeys mk(keys);
        if (oJustEntered) { upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc; oJustEntered=false; }
        if (mk.up && !upWas && optionCursor > 0)    optionCursor--;
        if (mk.down && !downWas && optionCursor < 1) optionCursor++;
        if (mk.enter && !enterWas) {
            if (optionCursor == 0) aimAssistOn = !aimAssistOn;
            else if (optionCursor == 1) { atSoundMenu = true; oJustEntered = true; }
        }
        if (mk.esc && !escWas) {
            optionCursor = 0; atOptionScreen = false; oJustEntered = true;
            if (optionFromPause) paused = true;
            else atStartScreen = true;
        }
        upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc;
    }

    void drawOptionScreen() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        font.drawString(r, "OPTIONS", CENTER_X - 84, 40, 4);
        SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
        SDL_RenderDrawLine(r, CENTER_X - 180, 78, CENTER_X + 180, 78);
        const char* labels[2] = {"AIM ASSIST", "SOUND"};
        const int Y0 = 130, GAP = 70;
        for (int i = 0; i < 2; ++i) {
            int ly = Y0 + i * GAP;
            int lx = CENTER_X - 120;
            if (i == optionCursor) UIRenderer::drawMenuCursor(r, lx - 22, ly + 7, 10);
            font.drawString(r, labels[i], lx, ly, 3);
            if (i == 0) {
                SDL_SetRenderDrawColor(r, aimAssistOn ? 100 : 200, aimAssistOn ? 255 : 60, 100, 255);
                SDL_Rect tg = {CENTER_X + 80, ly - 2, 56, 26};
                SDL_RenderFillRect(r, &tg);
                font.drawString(r, aimAssistOn ? "ON" : "OFF", CENTER_X + 88, ly + 4, 2);
            } else {
                font.drawString(r, ">", CENTER_X + 80, ly, 3);
            }
            if (i == optionCursor) {
                int itemW = (int)strlen(labels[i]) * 6 * 3;
                UIRenderer::drawMenuUnderline(r, lx, ly + 24, itemW);
            }
        }
        font.drawString(r, "W/S:select  ENTER:confirm  ESC:back", CENTER_X - 216, 490, 2);
    }

    // ======== SOUND MENU ========
    void updateSoundMenu(const Uint8* keys) {
        static bool sJustEntered = true;
        MenuKeys mk(keys);
        bool leftNow = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        if (sJustEntered) {
            upWas=mk.up; downWas=mk.down; escWas=mk.esc; enterWas=mk.enter;
            leftWas=leftNow; rightWas=rightNow; sJustEntered=false;
        }
        if (mk.up && !upWas && soundCursor > 0)    soundCursor--;
        if (mk.down && !downWas && soundCursor < 5) soundCursor++;
        if (mk.esc && !escWas)  { atSoundMenu = false; sJustEntered = true; }
        if (mk.enter && !enterWas && soundCursor == 5) { atSoundMenu = false; sJustEntered = true; }

        if (leftNow && !leftWas) {
            if (soundCursor == 0) audio.adjBgmVolume(-1);
            if (soundCursor == 1) audio.adjSfxVolume(-1);
            if (soundCursor == 2) audio.adjEqLow(-1);
            if (soundCursor == 3) audio.adjEqMid(-1);
            if (soundCursor == 4) audio.adjEqHigh(-1);
        }
        if (rightNow && !rightWas) {
            if (soundCursor == 0) audio.adjBgmVolume(1);
            if (soundCursor == 1) audio.adjSfxVolume(1);
            if (soundCursor == 2) audio.adjEqLow(1);
            if (soundCursor == 3) audio.adjEqMid(1);
            if (soundCursor == 4) audio.adjEqHigh(1);
        }
        upWas=mk.up; downWas=mk.down; escWas=mk.esc; enterWas=mk.enter;
        leftWas=leftNow; rightWas=rightNow;
    }

    void drawSoundMenu() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        font.drawString(r, "SOUND", CENTER_X - 60, 40, 4);
        SDL_SetRenderDrawColor(r, 100, 100, 100, 255);
        SDL_RenderDrawLine(r, CENTER_X - 180, 78, CENTER_X + 180, 78);
        const char* labels[6] = {"BGM VOL", "SFX VOL", "EQ LOW", "EQ MID", "EQ HIGH", "BACK"};
        const int Y0 = 110, GAP = 48;
        for (int i = 0; i < 6; ++i) {
            int ly = Y0 + i * GAP;
            int lx = CENTER_X - 120;
            if (i == soundCursor) UIRenderer::drawMenuCursor(r, lx - 22, ly + 7, 10);
            font.drawString(r, labels[i], lx, ly, 3);
            if (i == soundCursor) {
                int itemW = (int)strlen(labels[i]) * 6 * 3;
                UIRenderer::drawMenuUnderline(r, lx, ly + 24, itemW);
            }
            int sx = CENTER_X + 20, sw = 160;
            char vbuf[8];
            switch (i) {
                case 0: snprintf(vbuf, sizeof(vbuf), "%d", audio.getBgmVolume());
                        UIRenderer::drawSlider(r, sx, ly+6, sw, audio.getBgmVolume(), 1, 10, false); break;
                case 1: snprintf(vbuf, sizeof(vbuf), "%d", audio.getSfxVolume());
                        UIRenderer::drawSlider(r, sx, ly+6, sw, audio.getSfxVolume(), 1, 10, false); break;
                case 2: snprintf(vbuf, sizeof(vbuf), "%+d", audio.getEqLow());
                        UIRenderer::drawSlider(r, sx, ly+6, sw, audio.getEqLow(), -5, 5, true); break;
                case 3: snprintf(vbuf, sizeof(vbuf), "%+d", audio.getEqMid());
                        UIRenderer::drawSlider(r, sx, ly+6, sw, audio.getEqMid(), -5, 5, true); break;
                case 4: snprintf(vbuf, sizeof(vbuf), "%+d", audio.getEqHigh());
                        UIRenderer::drawSlider(r, sx, ly+6, sw, audio.getEqHigh(), -5, 5, true); break;
            }
            if (i < 5) font.drawString(r, vbuf, sx + sw + 10, ly, 2);
        }
        font.drawString(r, "W/S:select  A/D:adjust  ENTER/ESC:back", CENTER_X - 232, 490, 2);
    }

    // ======== GAMEPLAY UPDATE ========
    void updateGameplay(const Uint8* keys) {
        bool isSide = chapterMgr.getConfig().isSideScrolling;
        if (isSide) {
            // ======== Chapter 2 side-scrolling update ========
            bool moveLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            bool moveRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            bool moveUp    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool moveDown  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool shoot     = keys[SDL_SCANCODE_SPACE];
            int px = player->getX(), py = player->getY();
            if (moveLeft)  px -= 6;
            if (moveRight) px += 6;
            if (moveUp)    py -= 6;
            if (moveDown)  py += 6;
            if (px < 10) px = 10;
            if (py < 10) py = 10;
            if (py > WIN_HEIGHT - 10) py = WIN_HEIGHT - 10;

            int noseX = px + player->getNoseOffset();
            // Energy wall only during combat (after auto-spawn begins)
            if (autoSpawnPhase > 0 && noseX > 642) {
                px = 642 - player->getNoseOffset();
                wallFlashTimer = 28; wallContactY = py; wallAnimFrame++;
            } else if (wallFlashTimer > 0) { wallFlashTimer--; wallAnimFrame++; }

            player->setX(px); player->setY(py);

            // TrainingPlane shooting (Chapter 1 original fire rate)
            if (dmFireCooldown > 0) dmFireCooldown--;
            if (shoot && dmFireCooldown <= 0) {
                int nGuns = player->getGunCount();
                for (int g = 0; g < nGuns; ++g) {
                    int ox, oy;
                    player->getGunOffset(g, ox, oy);
                    bulletMgr.addBulletSideScrollAt(*player, ox, oy, &audio);
                }
                dmFireCooldown = 7;
            }
            bulletMgr.update(alienMgr.all());
            bulletMgr.removeInactive();

            // ==== Ch2 auto-spawn wave system ====
            // Start after sphere boss finishes all animations (DONE state)
            if (autoSpawnPhase == 0 && sphereBossActive && sphereBoss.getState() == Ch2SphereBoss::DONE) {
                autoSpawnPhase = 1; autoSpawnQueued = 3; autoSpawnTimer = 0;
                autoSpawnScoreBase = score;  // record baseline for kill counting
                autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
            }
            // Spawn pump: processes any queued aliens (initial waves + escape replacements + reinforcements)
            if (autoSpawnQueued > 0) {
                if (autoSpawnTimer > 0) autoSpawnTimer--;
                if (autoSpawnTimer <= 0) {
                    ch2AlienMgr.forceSpawn();
                    autoSpawnQueued--;
                    autoSpawnTimer = 12; // 0.2s = 12 frames
                }
                // Auto-transition for waves 1,2 when initial batch fully spawned
                if (autoSpawnQueued <= 0 && (autoSpawnPhase == 1 || autoSpawnPhase == 3))
                    autoSpawnPhase++;
            }
            // Unified escape detection (fight phases 2,4,5): alive delta not from kills = escaped
            if (autoSpawnPhase == 2 || autoSpawnPhase == 4 || autoSpawnPhase == 5) {
                if (autoSpawnQueued <= 0) {
                    int alive = ch2AlienMgr.countLiving();
                    int kills = score - autoSpawnScoreBase;
                    int aliveLost = autoSpawnAliveLast - alive;
                    int killsGained = kills - autoSpawnKillsLast;
                    int escaped = aliveLost - killsGained;
                    if (escaped > 0) { autoSpawnQueued += escaped; autoSpawnTimer = 12; }
                    autoSpawnAliveLast = alive + autoSpawnQueued;
                    autoSpawnKillsLast = kills;
                }
            }
            // Wave1→Wave2 transition: field empty + score ≥ 3
            if (autoSpawnPhase == 2 && autoSpawnQueued <= 0) {
                if (ch2AlienMgr.countLiving() == 0 && (score - autoSpawnScoreBase) >= 3) {
                    autoSpawnPhase = 3; autoSpawnQueued = 5; autoSpawnTimer = 12;
                }
            }
            // Wave2→Wave3 transition: field empty + score ≥ 8
            if (autoSpawnPhase == 4 && autoSpawnQueued <= 0) {
                if (ch2AlienMgr.countLiving() == 0 && (score - autoSpawnScoreBase) >= 8) {
                    autoSpawnPhase = 5; autoSpawnQueued = 5; autoSpawnTimer = 12;
                    autoSpawnWave3Reinf = 0;
                }
            }
            // Wave 3: reinforcement + danmaku gate (phase 5)
            if (autoSpawnPhase == 5 && autoSpawnQueued <= 0) {
                int alive = ch2AlienMgr.countLiving();
                int kills = score - autoSpawnScoreBase - 8;  // kills within wave3 only
                // Reinforcement: every 3 wave3 kills, spawn 3 more (up to 4 rounds)
                while (autoSpawnWave3Reinf < 4 && kills >= 3 * (autoSpawnWave3Reinf + 1)) {
                    autoSpawnQueued += 3; autoSpawnWave3Reinf++;
                }
                // Danmaku gate: 4 reinf rounds done + field clear + no pending spawns
                if (autoSpawnWave3Reinf >= 4 && alive == 0 && autoSpawnQueued <= 0) {
                    dmMgr.spawnEnemy();
                    autoSpawnPhase = 6;
                }
            }
            player->updateInvFrames();
            ch2AlienMgr.update(bulletMgr, particleMgr, audio, score, *player, floatingTextMgr, playerHitCount);
            dmMgr.update(bulletMgr, particleMgr, audio, score, *player, floatingTextMgr, playerHitCount);
            if (ch2GameOver) gameOver = true;

            // Sphere boss update + scroll sync (only during ENTERING) + bg speed + bullet collision
            if (sphereBossActive) {
                if (sphereBoss.getState() == Ch2SphereBoss::ENTERING)
                    sphereBoss.syncScreenPos(sideBg->getScrollX());
                sphereBoss.update();
                double ts = sphereBoss.getBgTargetSpeed();
                if (ts >= 0 && sideBg) sideBg->setSpeed(ts);
            }
            if (sphereBossActive && sphereBoss.getState() == Ch2SphereBoss::FIGHT) {
                for (auto& b : bulletMgr.all()) {
                    if (!b.active || !b.canDamage) continue;
                    double dx = b.x - sphereBoss.getCx();
                    double dy = b.y - sphereBoss.getCy();
                    if (dx*dx + dy*dy < sphereBoss.getRadius() * sphereBoss.getRadius()) {
                        b.active = false;
                        sphereBoss.takeDamage(1);
                        sphereBoss.popDiamonds(1);
                        particleMgr.spawnExplosion(b.x, b.y, 2);
                    }
                }
            }

            // ==== Pulse energy / skill orb / Shift input ====
            bool shiftNow = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            shiftJustPressed = (shiftNow && !shiftWas);
            shiftWas = shiftNow;

            // Energy fill from bullet hits this frame
            pulseSystem.addEnergy(playerHitCount);
            playerHitCount = 0;

            // Skill orb: spawn on first danmaku defeat (after defeat animation ends)
            if (!pulseOrbDropped && !pulseSystem.unlocked) {
                for (const auto& de : dmMgr.getEnemies()) {
                    if (de.defeated && de.defeatTimer == 1) { // last frame of defeat animation
                        skillOrb.spawn(de.x, de.y);
                        pulseOrbDropped = true;
                        break;
                    }
                }
            }

            // Skill orb: update + bullet collision (shield break after 18 hits)
            if (skillOrb.isActive()) {
                skillOrb.update();
                if (skillOrb.state == Ch2SkillOrb::FLOATING) {
                    for (auto& b : bulletMgr.all()) {
                        if (!b.active || !b.canDamage) continue;
                        double dx = b.x - skillOrb.x, dy = b.y - skillOrb.y;
                        double hitR = skillOrb.radius + 5.0;
                        if (dx*dx + dy*dy < hitR*hitR) {
                            b.active = false; b.canDamage = false;
                            skillOrb.registerHit(particleMgr, audio);
                            break; // one hit per frame
                        }
                    }
                }
                // Orb absorption: hold Shift near core for 5 sec → fill energy bar
                // Release Shift during absorption → energy rapidly drains to 0 before retry
                if (skillOrb.isCore() && !skillOrb.pulseUnlocked()) {
                    double dx = skillOrb.x - player->getX();
                    double dy = skillOrb.y - player->getY();
                    bool nearPlayer = (dx*dx + dy*dy < 200.0*200.0);
                    // Drain energy to 0 if player released Shift or moved away
                    if (skillOrb.state == Ch2SkillOrb::ABSORBING && (!shiftNow || !nearPlayer) && !pulseSystem.isDraining()) {
                        skillOrb.stopAbsorb();
                        pulseSystem.startDrain();
                    }
                    // Start absorption: must be CORE, holding Shift, near player, energy at 0
                    if (skillOrb.state == Ch2SkillOrb::CORE && shiftNow && nearPlayer && pulseSystem.canAbsorb()) {
                        skillOrb.startAbsorb();
                    }
                    // Continuing absorption: each frame tick timer + fill energy
                    if (skillOrb.state == Ch2SkillOrb::ABSORBING && shiftNow && nearPlayer) {
                        skillOrb.tickAbsorb();
                        if (skillOrb.absorbTimer % 10 == 0) pulseSystem.addAbsorbEnergy();
                        for (int i = 0; i < 5; ++i) {
                            double sx = skillOrb.x + (rand()%14-7);
                            double sy = skillOrb.y + (rand()%14-7);
                            double vx = (player->getX() - sx) * 0.05 + (rand()%40-20)/15.0;
                            double vy = (player->getY() - sy) * 0.05 + (rand()%40-20)/15.0;
                            particleMgr.spawnWhiteParticle(sx, sy, vx, vy, 18 + rand() % 25);
                        }
                        if (skillOrb.pulseUnlocked()) {
                            pulseSystem.unlocked = true;
                            pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY; // snap to full
                            autoSpawnQueued += 10;
                        }
                    }
                    // Energy drain: -3 per frame until 0
                    if (pulseSystem.isDraining()) {
                        pulseSystem.drainTick();
                    }
                }
            }

            // Pulse release: single Shift press when energy full
            if (pulseSystem.unlocked && pulseSystem.isFull() && shiftJustPressed) {
                pulseSystem.release((float)player->getX(), (float)player->getY(), particleMgr, audio);
            }
            pulseSystem.update();

            // Pulse wave collision: destroy all enemy bullets
            pulseSystem.collideWithBullets(
                const_cast<std::vector<Ch2EnemyBullet>&>(ch2AlienMgr.getBullets()), particleMgr);
            pulseSystem.collideWithBullets(
                const_cast<std::vector<Ch2EnemyBullet>&>(dmMgr.getBullets()), particleMgr);
            // Pulse wave collision: 1 damage per enemy per wave
            pulseSystem.collideWithAliens(ch2AlienMgr, particleMgr, audio, score, playerHitCount);
            pulseSystem.collideWithDanmaku(dmMgr, particleMgr, audio, score, playerHitCount);

            floatingTextMgr.update();
            particleMgr.update();
            particleMgr.removeInactive();
            return;
        }

        if (phase != PHASE_BOSS_DEFEAT) {
            // Ch1Boss trigger
            if (score >= chapterMgr.getConfig().bossTriggerScore && phase == PHASE_PLAY) {
                boss.trigger();
                phase = PHASE_BOSS_INTRO;
                alienMgr.setAllInvincible();
            }

            // Difficulty growth
            if (phase == PHASE_PLAY) {
                difficultyTimer++;
                alienMgr.updateMovementParams(difficultyTimer);
            }

            // TrainingPlane input
            player->handleInput(keys);

            // Shooting
            if (keys[SDL_SCANCODE_SPACE] && bulletMgr.canFire()) {
                bulletMgr.addBullet((TrainingPlane&)*player, &audio);
                bulletMgr.setCooldown();
            }
            bulletMgr.decrementCooldown();

            // Enable enemies after score-0 dialogue finishes (Ch1 only; Ch2+ immediately)
            if (!enemiesEnabled && (chapterMgr.getConfig().chapterNumber != 1 || (triggeredScores[0] && !dialogueSys.isActive())))
                enemiesEnabled = true;

            // Spawn
            if (!enemiesEnabled) {
                if (phase == PHASE_PLAY) alienMgr.spawnTimerRef()--;
            } else if (phase == PHASE_BOSS_FIGHT && boss.isActive()) {
                if (alienMgr.spawnTimerRef() <= 0) {
                    alienMgr.spawnAlienFromBoss(boss.getX(), boss.getY(), score);
                    alienMgr.spawnTimerRef() = alienMgr.currentSpawnInterval(score, difficultyTimer) + (rand() % 20);
                }
                alienMgr.spawnTimerRef()--;
            } else if (phase == PHASE_PLAY) {
                if (alienMgr.spawnTimerRef() <= 0) {
                    alienMgr.spawnAlien(score);
                    alienMgr.spawnTimerRef() = alienMgr.currentSpawnInterval(score, difficultyTimer) + (rand() % 30);
                }
                alienMgr.spawnTimerRef()--;
            }

            // Ch1Shockwave
            if (score >= 30 && (phase == PHASE_PLAY || phase == PHASE_BOSS_FIGHT)) {
                shockwaveMgr.attemptAutoRelease(score, *player, floatingTextMgr, &audio, &particleMgr);
            }

            // Updates
            bulletMgr.update(alienMgr.all());
            if (phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2) {
                alienMgr.update(false, 0, gameOver, baseHP, particleMgr, &audio);
                if (gameOver) menuSelection = 0;
            }
            // Base fire effects (burning wreckage on the ground)
            baseFireTimer++;
            if (baseFireTimer % 4 == 0) {
                // Fire particles at 2-3 random spots on the base
                for (int fi = 0; fi < 3; ++fi) {
                    int fx = 100 + rand() % 600;
                    int fy = WIN_HEIGHT - 25 - rand() % 45;
                    particleMgr.spawnExplosion(fx, fy, 2 + rand() % 3);
                }
            }
            if (baseFireTimer % 30 == 0) {
                // Larger burst at a random base position
                int bx = 120 + rand() % 560;
                int by = WIN_HEIGHT - 20 - rand() % 35;
                particleMgr.spawnExplosion(bx, by, 8 + rand() % 10);
            }
            // Smoke: white particles rising from base
            if (baseFireTimer % 6 == 0) {
                for (int si = 0; si < 2; ++si) {
                    int sx = 80 + rand() % 640;
                    int sy = WIN_HEIGHT - 15 - rand() % 40;
                    double svx = (rand() % 30 - 15) / 20.0;
                    double svy = -(0.6 + (rand() % 40) / 100.0);
                    particleMgr.spawnWhiteParticle(sx, sy, svx, svy, 30 + rand() % 20);
                }
            }
            particleMgr.update();
            shockwaveMgr.update();
            floatingTextMgr.update();
            if (chapterMgr.getConfig().chapterNumber == 1) {
            if (!triggeredScores[0] && lastScore < 0 && score >= 0 && !dialogueSys.isActive()) {
                triggeredScores[0] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "Martha, you're the only one in the air.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Hold on as long as you can. The base shockwave cannon is charging.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[3] && lastScore < 3 && score >= 3 && !dialogueSys.isActive()) {
                triggeredScores[3] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "These enemies are made of energy.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Destroy them. We can collect the energy.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[15] && lastScore < 15 && score >= 15 && !dialogueSys.isActive()) {
                triggeredScores[15] = true;
                dialogueSys.queueDialogue("", "Tower communication restored.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[20] && lastScore < 20 && score >= 20 && !dialogueSys.isActive()) {
                triggeredScores[20] = true;
                dialogueSys.queueDialogue("Tower (ai)", "Shockwave cannon ready.");
                dialogueSys.queueDialogue("Bryssa from Tower", "A little more energy!");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[30] && lastScore < 30 && score >= 30 && !dialogueSys.isActive()) {
                triggeredScores[30] = true;
                dialogueSys.queueDialogue("Tower (ai)", "Defense system charged.");
                dialogueSys.queueDialogue("Tower (ai)", "More enemies incoming. Keep gathering energy.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[40] && lastScore < 40 && score >= 40 && !dialogueSys.isActive()) {
                triggeredScores[40] = true;
                dialogueSys.queueDialogue("Bryssa from Tower", "The trainer shares energy with the base.");
                dialogueSys.queueDialogue("Bryssa from Tower", "You and the base will upgrade together.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[50] && lastScore < 50 && score >= 50 && !dialogueSys.isActive()) {
                triggeredScores[50] = true;
                dialogueSys.queueDialogue("Tower (ai)", "Keep gathering energy.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[55] && lastScore < 55 && score >= 55 && !dialogueSys.isActive()) {
                triggeredScores[55] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "System checking.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Done.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[61] && lastScore < 61 && score >= 61 && !dialogueSys.isActive()) {
                triggeredScores[61] = true;
                dialogueSys.queueDialogue("Tower (ai)", "Base upgraded again.");
                dialogueSys.queueDialogue("Bryssa from Tower", "Radar shows even more enemies! Watch out!");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[70] && lastScore < 70 && score >= 70 && !dialogueSys.isActive()) {
                triggeredScores[70] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "System checking result:");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Aim assist system on this plane.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "You shall find it somewhere.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[80] && lastScore < 80 && score >= 80 && !dialogueSys.isActive()) {
                triggeredScores[80] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "I've lost contact with the tower!");
                dialogueSys.queueDialogue("Ally (ai copilot)", "But you and the base can upgrade again soon.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[105] && lastScore < 105 && score >= 105 && !dialogueSys.isActive()) {
                triggeredScores[105] = true;
                dialogueSys.queueDialogue("", "Tower communication restored.");
                dialogueSys.queueDialogue("Tower (ai)", "Massive energy signature detected.");
                dialogueSys.queueDialogue("Tower (ai)", "Analyzing source...");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[120] && lastScore < 120 && score >= 120 && !dialogueSys.isActive()) {
                triggeredScores[120] = true;
                dialogueSys.queueDialogue("Bryssa from Tower", "It's a capital ship.");
                dialogueSys.queueDialogue("Bryssa from Tower", "Telamondo-class. Martha, this is what the defense system was built for.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[160] && lastScore < 160 && score >= 160 && !dialogueSys.isActive()) {
                triggeredScores[160] = true;
                dialogueSys.queueDialogue("Tower (ai)", "Enemy capital ship approaching.");
                dialogueSys.queueDialogue("Tower (ai)", "Entering weapons range in 40 seconds.");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[180] && lastScore < 180 && score >= 180 && !dialogueSys.isActive()) {
                triggeredScores[180] = true;
                dialogueSys.queueDialogue("Bryssa from Tower", "Shockwave Defense System is fully charged");
                dialogueSys.queueDialogue("Bryssa from Tower", "Martha, just keep them off us!");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[195] && lastScore < 195 && score >= 195 && !dialogueSys.isActive()) {
                triggeredScores[195] = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "Here it comes...!");
                dialogueSys.start(); lastScore = score;
            }
            if (!triggeredScores[210] && lastScore < 210 && score >= 210 && !dialogueSys.isActive()) {
                triggeredScores[210] = true;
                dialogueSys.queueDialogue("Bryssa from Tower", "Base defense systems are strengthening.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Firepower systems being enhanced.");
                dialogueSys.start(); lastScore = score;
            }
            if (bossPhase2DialogueTriggered && !dialogueSys.isActive()) {
                bossPhase2DialogueTriggered = false;
                dialogueSys.queueDialogue("Ally (ai copilot)", "Telamondo can absorb energy!");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Our firepower can match it!");
                dialogueSys.start(); lastScore = score;
            }
            } // chapter 1 dialogue triggers
            if (!dialogueSys.isActive()) lastScore = score;
            dialogueSys.update(false);  // no ENTER skip for dialogue
            int ticks = dialogueSys.popTicks();
            const std::string& spk = dialogueSys.currentSpeaker();
            while (ticks-- > 0) {
                if (spk.find("Ally") != std::string::npos) audio.sndAllyTalk();
                else if (spk.find("Bryssa") != std::string::npos) audio.sndBryssaTalk();
                else if (spk.find("Tower") != std::string::npos) audio.sndTowerTalk();
                else if (spk.empty()) audio.sndSystemTalk();
                else audio.sndTeletype();
            }

            // Ch1Boss movement
            if (phase == PHASE_BOSS_FIGHT || phase == PHASE_BOSS_PHASE2) {
                boss.updateMovement();
            }
            if (phase == PHASE_BOSS_FIGHT && boss.isActive() && boss.isCh1HealWavesEnabled()) {
                boss.updateCh1HealWaves(particleMgr, alienMgr, &audio);
            }

            // Phase2 trigger
            int totalHP = boss.getHp() + boss.getBonusHp();
            if (phase == PHASE_BOSS_FIGHT && !boss.isPhase2Triggered() && boss.isActive() &&
                totalHP <= (boss.getMaxHp() + boss.getBonusHp()) / 2) {
                boss.triggerPhase2();
                phase = PHASE_BOSS_PHASE2;
                alienMgr.setAllInvincible();
                bossPhase2DialogueTriggered = true;
            }

            // ======== PHASE_BOSS_INTRO ========
            if (phase == PHASE_BOSS_INTRO) {
                boss.updateEnterAnimation(&audio);
                boss.updateShake(&audio);
                alienMgr.update(true, 0.2, gameOver, baseHP, particleMgr, &audio);
                alienMgr.setAllInvincible(); // keep all aliens blue after entry completes
                // Absorb logic
                if (!boss.isEntering() && boss.getShakeTimer() == 0 && boss.absorbTimerRef() >= 0) {
                    if (boss.updateAbsorbStateMachine(alienMgr, bulletMgr, particleMgr, &audio)) {
                        alienMgr.setAllVulnerable();
                    }
                    boss.updateAbsorbAnimations(alienMgr, particleMgr);
                }
                if (boss.postAbsorbTimerRef() > 0) {
                    boss.updatePostAbsorbShake(&audio);
                    if (boss.postAbsorbTimerRef() == 0) phase = PHASE_BOSS_FIGHT;
                }
            }

            // ======== PHASE_BOSS_PHASE2 ========
            if (phase == PHASE_BOSS_PHASE2) {
                boss.updateShake(&audio);
                alienMgr.update(true, 0.2, gameOver, baseHP, particleMgr, &audio);
                alienMgr.setAllInvincible(); // keep all aliens blue after entry completes
                if (boss.getShakeTimer() == 0 && boss.absorbTimerRef() >= 0) {
                    if (boss.updateAbsorbStateMachine(alienMgr, bulletMgr, particleMgr, &audio)) {
                        alienMgr.setAllVulnerable();
                        boss.setCh1HealWavesEnabled(true);
                    }
                    boss.updateAbsorbAnimations(alienMgr, particleMgr);
                }
                if (boss.postAbsorbTimerRef() > 0) {
                    boss.updatePostAbsorbShake(&audio);
                    if (boss.postAbsorbTimerRef() == 0) phase = PHASE_BOSS_FIGHT;
                }
            }

            // ======== COLLISION DETECTION ========
            // Ch1Bullet vs Ch1Alien
            for (auto& b : bulletMgr.all()) {
                if (!b.active || !b.canDamage) continue;
                for (auto& a : alienMgr.all()) {
                    if (!a.active) continue;
                    double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                    double dx = b.x - ax, dy = b.y - a.y;
                    double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                    double alienScale = (depthBelow < 0) ? 0.08 : 0.08 + 0.92 * depthBelow;
                    if (alienScale < 0.08) alienScale = 0.08;
                    if (alienScale > 1.0)  alienScale = 1.0;
                    double hitRadius = 28.0 * alienScale + 10.0;
                    // far (scale≈0.08) → hitR≈12, near (scale≈1.0) → hitR≈38
                    if (dx*dx + dy*dy < hitRadius * hitRadius) {
                        b.active = false; b.canDamage = false;
                        if (a.invincibleFrames != 0) {
                            particleMgr.spawnExplosion(b.x, b.y, 3);
                            break;
                        }
                        a.hp--;
                        audio.sndHit();
                        if (a.hp <= 0) {
                            a.active = false;
                            particleMgr.spawnExplosion(ax, a.y, 22);
                            audio.sndExplosionBig();
                            score++;
                            bulletMgr.updateParams(score / 30);
                            shockwaveMgr.updateParams(score / 30);
                        } else {
                            particleMgr.spawnExplosion(b.x, b.y, 4);
                        }
                        break;
                    }
                }
            }
            bulletMgr.removeInactive();

            // Ch1Shockwave vs Ch1Alien
            for (auto& a : alienMgr.all()) {
                if (!a.active || a.invincibleFrames != 0) continue;
                int scoreBefore = score;
                shockwaveMgr.collideWithAlien(a, particleMgr, &audio, score);
                if (score != scoreBefore) {
                    bulletMgr.updateParams(score / 30);
                    shockwaveMgr.updateParams(score / 30);
                }
            }

            // Blue beam hit detection
            for (auto& b : bulletMgr.all()) {
                if (!b.active || !b.blueBeam) continue;
                if (b.beamTargetIndex < 0 || b.beamTargetIndex >= (int)alienMgr.all().size()) continue;
                Ch1Alien& a = alienMgr.all()[b.beamTargetIndex];
                if (!a.active || a.beingAbsorbed) { b.active = false; continue; }
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                double dx = b.x - ax, dy = b.y - a.y;
                if (dx*dx + dy*dy < 28.0 * 28.0) {
                    b.active = false;
                    for (int i = 0; i < 12; ++i) {
                        particleMgr.spawnWhiteParticle(ax + (rand()%14-7), a.y + (rand()%14-7),
                            (rand()%30-15)/6.0, (rand()%30-15)/6.0, 15 + rand()%10);
                    }
                    a.beingAbsorbed = true;
                    audio.sndBossAbsorb();
                    a.absorbFrame = 0;
                    a.absorbDuration = 70;
                    a.absorbStartX = ax;
                    a.absorbStartY = a.y;
                    boss.absorbStateRef() = Ch1Boss::SPIRALING;
                }
            }

            // Ch1Bullet vs Ch1Boss
            if (boss.isActive() && phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2 && phase != PHASE_BOSS_DEFEAT) {
                for (auto& b : bulletMgr.all()) {
                    if (!b.active || !b.canDamage) continue;
                    double dx = b.x - boss.getX(), dy = b.y - boss.getY();
                    if (dx*dx + dy*dy < 55.0 * 55.0) {
                        b.active = false; b.canDamage = false;
                        boss.flashTimerRef() = 5;
                        audio.sndBossHit();
                        particleMgr.spawnExplosion(b.x, b.y, 4);
                        boss.takeDamage(1);
                        if (boss.getHp() <= 0) {
                            phase = PHASE_BOSS_DEFEAT;
                            bossDefeatTimer = 0;
                        }
                    }
                }
                // Ch1Shockwave vs Ch1Boss
                shockwaveMgr.collideWithBoss(boss.getX(), boss.getY(), boss.lastHitBySWRef(),
                    boss.bonusHpRef(), boss.hpRef(), boss.flashTimerRef(), particleMgr, &audio);
                if (boss.getHp() <= 0) {
                    phase = PHASE_BOSS_DEFEAT;
                    bossDefeatTimer = 0;
                }
            }

            // Cleanup
            bulletMgr.removeInactive();
            alienMgr.removeInactive();
            particleMgr.removeInactive();
            shockwaveMgr.removeInactive();
        } else {
            // ======== PHASE_BOSS_DEFEAT ========
            updateBossDefeat(keys);
        }
    }

    void updateBossDefeat(const Uint8* keys) {
        bossDefeatTimer++;
        particleMgr.update();
        floatingTextMgr.update();

        if (bossDefeatTimer < 300) {
            if (bossDefeatTimer % 6 == 0)
                particleMgr.spawnExplosion(boss.getX() + (rand()%90-45), boss.getY() + (rand()%55-27), 10);
            if (bossDefeatTimer % 20 == 0)
                particleMgr.spawnExplosion(boss.getX() + (rand()%120-60), boss.getY() + (rand()%70-35), 20);
        }
        if (bossDefeatTimer == 300) {
            particleMgr.spawnExplosion(boss.getX(), boss.getY(), 120);
            audio.sndExplosionBig();
            boss.setActive(false);
            defeatAlienTimer = 0;
        }

        if (bossDefeatTimer > 300) {
            defeatAlienTimer++;
            if (defeatAlienTimer >= 60) {
                defeatAlienTimer = 0;
                for (auto& a : alienMgr.all()) {
                    if (a.active) {
                        double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                        particleMgr.spawnExplosion(ax, a.y, 22);
                        audio.sndExplosionBig();
                        a.active = false;
                        break;
                    }
                }
            }
        }

        bool anyAlien = false;
        for (const auto& a : alienMgr.all()) if (a.active) anyAlien = true;
        if (!anyAlien && bossDefeatTimer > 300 && !missionCompleteShown) {
            defeatMCDelay++;
            if (defeatMCDelay >= 60) {
                missionCompleteShown = true;
                floatingTextMgr.spawn((float)player->getX(), (float)(player->getY() - 30),
                                      "MISSION COMPLETE!", 255, 255, 50);
            }
        }

        if (missionCompleteShown) {
            defeatReturnTimer++;
            if (defeatReturnTimer >= 120 && defeatReturnTimer < 200) {
                double t = (defeatReturnTimer - 120) / 80.0;
                double eased = t * t * (3.0 - 2.0 * t);
                int px = player->getX() + (int)((CENTER_X - player->getX()) * eased * 0.25);
                player->setX(px);
                if (std::abs(player->getX() - CENTER_X) < 2) player->setX(CENTER_X);
            }
            if (defeatReturnTimer == 200) {
                player->setRollTarget(0); player->setRollAngle(0);
            }
            if (defeatReturnTimer >= 200) {
                player->setY(player->getY() - 7);
            }
            if (player->getY() < -50) {
                defeatFWTimer++;
                if (defeatFWTimer % 8 == 0) {
                    particleMgr.spawnFireworks(CENTER_X, WIN_HEIGHT);
                }
                if (defeatFWTimer >= 120) {
                    missionComplete = true;
                    defeatFadeTimer++;
                    if (isNormalPlay && defeatFWTimer == 120) {
                        int cur = chapterMgr.getCurrentIndex();
                        if (cur < 4 && !chapterMgr.isUnlocked(cur + 1))
                            chapterMgr.unlockChapter(cur + 1);
                    }
                }
            }
        }

        if (missionComplete) {
            bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool enterNow = keys[SDL_SCANCODE_RETURN];
            static bool upWasM = false, downWasM = false, enterWasM = false;
            if (upNow && !upWasM && mcMenuSelection > 0) mcMenuSelection--;
            if (downNow && !downWasM && mcMenuSelection < 1) mcMenuSelection++;
            if (enterNow && !enterWasM) {
                if (mcMenuSelection == 0) {
                    int cur = chapterMgr.getCurrentIndex();
                    if (cur < 4) {
                        chapterMgr.selectChapter(cur + 1);
                        resetGame();
                        atStartScreen = false;
                        isNormalPlay = true;
                        alienMgr.applyChapterConfig(chapterMgr.getConfig());
                        bulletMgr.updateParams(0);
                        shockwaveMgr.updateParams(0);
                        startChapterNarration();
                    } else {
                        resetGame(); atStartScreen = true;
                    }
                } else {
                    resetGame(); atStartScreen = true;
                }
            }
            upWasM = upNow; downWasM = downNow; enterWasM = enterNow;
        }

        particleMgr.removeInactive();
        shockwaveMgr.removeInactive();
    }

    // ======== DRAW GAMEPLAY ========
    void drawGameplayFrame() {
        bool isSide = chapterMgr.getConfig().isSideScrolling;
        bool useShake = (boss.getShakeTimer() > 0);
        if (useShake) {
            shakeTex = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);
            SDL_SetRenderTarget(renderer.get(), shakeTex);
        }
        renderer.setColor(0, 0, 0);
        renderer.clear();

        if (!gameOver) {
            if (isSide && sideBg) {
                sideBg->draw(renderer.get());
            } else if (background) {
                background->drawBackground(renderer.get());
                background->drawBase(renderer.get());
            }

            if (!isSide) {
                for (const auto& sw : shockwaveMgr.all()) if (sw.active) shockwaveMgr.draw(renderer.get());
            }
            particleMgr.draw(renderer.get());
            dialogueSys.draw(renderer.get(), font);
            if (!isSide) {
                alienMgr.draw(renderer.get());
                bulletMgr.draw(renderer.get());
            }
            if (isSide) {
                if (sphereBossActive) sphereBoss.draw(renderer.get());
                bulletMgr.draw(renderer.get());
                player->draw(renderer.get());
                drawWallFlash();
                ch2AlienMgr.drawEnemy(renderer.get());
                ch2AlienMgr.drawBullets(renderer.get());
                dmMgr.drawEnemy(renderer.get());
                dmMgr.drawBullets(renderer.get());
                // Pulse waves on top of enemies
                pulseSystem.draw(renderer.get());
                skillOrb.draw(renderer.get());
                // Ch2 HUD: all aligned to rightEdge = WIN_WIDTH - 10
                HUDBase::drawScore(renderer.get(), font, score, WIN_WIDTH - 10, 10);
                HUDBase::drawHPHearts(renderer.get(), font, ch2PlayerHP, 3, WIN_WIDTH - 10, 28);
                float eFill = pulseSystem.getFill();
                HUDBase::drawEnergyBar(renderer.get(), WIN_WIDTH - 10, 46, 10*14, 6,
                    eFill, pulseSystem.isFull());
                if (aimAssistOn) drawAimAssistSide();
            } else player->draw(renderer.get());

            // Floating texts
            drawFloatingTexts();

            // Aim assist
            if (!isSide && aimAssistOn) drawAimAssist();

            if (!isSide && boss.isActive()) {
                if (phase == PHASE_BOSS_PHASE2) boss.drawCircularShockwave(renderer.get());
                boss.drawCh1HealWaves(renderer.get());
                boss.drawBody(renderer.get());
                boss.drawHPBar(renderer.get(), font);
            }

            if (!isSide) drawScoreHUD();
        } else {
            if (isSide && sideBg) {
                sideBg->draw(renderer.get());
            } else if (background) {
                background->drawBackground(renderer.get());
                background->drawBase(renderer.get());
            }
            particleMgr.draw(renderer.get());
        }

        if (useShake) {
            SDL_SetRenderTarget(renderer.get(), NULL);
            SDL_Rect dst = {boss.getShakeX(), boss.getShakeY(), WIN_WIDTH, WIN_HEIGHT};
            SDL_RenderCopy(renderer.get(), shakeTex, NULL, &dst);
            SDL_DestroyTexture(shakeTex);
        }
    }

    void drawScoreHUD() {
        SDL_Renderer* r = renderer.get();
        HUDBase::drawScore(r, font, score, WIN_WIDTH - 10, 10);
        HUDBase::drawHPHearts(r, font, baseHP, 10, WIN_WIDTH - 10, 28);
        int lev = score / 30;
        float eFill = (lev >= 6) ? 1.0f : (float)(score % 30) / 30.0f;
        HUDBase::drawEnergyBar(r, WIN_WIDTH - 10, 46, 10*14, 6, eFill);
    }

    void drawFloatingTexts() {
        for (const auto& ft : floatingTextMgr.all()) {
            float t = (float)ft.life / ft.totalLife;
            if (t < 0.05f) continue;
            int rv = (int)(ft.r * t), gv = (int)(ft.g * t), bv = (int)(ft.b * t);
            SDL_SetRenderDrawColor(renderer.get(), rv, gv, bv, 255);
            int cx = (int)(ft.x - strlen(ft.text) * 6 * 3 / 2);
            for (const char* p = ft.text; *p; ++p) {
                if (*p != ' ') font.drawChar(renderer.get(), *p, cx, (int)ft.y, 3);
                cx += 6 * 3;
            }
        }
    }


    void drawWallFlash() {
        if (wallFlashTimer <= 0) return;
        SDL_Renderer* r = renderer.get();
        float t = (float)wallFlashTimer / 30.0f;
        int alpha = (int)(255.0f * t);
        int bx = 642, cy = wallContactY;  // wall at nose-tip max reach (px=630 + nose 12)

        // === Barrier core: white-hot line at contact, fading vertically ===
        int coreH = 70;
        for (int dy = -coreH; dy <= coreH; ++dy) {
            int yy = cy + dy;
            if (yy < 0 || yy >= WIN_HEIGHT) continue;
            float dr = (float)std::abs(dy) / coreH;
            int la = (int)(alpha * (1.0f - dr * 0.82f));
            if (la < 8) continue;
            SDL_SetRenderDrawColor(r, 220, 240, 255, (Uint8)la);
            SDL_RenderDrawLine(r, bx, yy, bx, yy);
        }

        // === Inner glow: cyan-blue layers extending right (energy discharge) ===
        for (int g = 1; g <= 5; ++g) {
            int gx = bx + g;  // rightward
            float gt = (float)g / 5.0f;
            int ga = (int)(alpha * (1.0f - gt * 0.65f));
            if (ga < 14) continue;
            int gh = (int)(coreH * (1.0f - gt * 0.45f)) + 25;
            for (int dy = -gh; dy <= gh; ++dy) {
                int yy = cy + dy;
                if (yy < 0 || yy >= WIN_HEIGHT) continue;
                float dr = (float)std::abs(dy) / gh;
                int la = (int)(ga * (1.0f - dr * 0.78f));
                if (la < 6) continue;
                int rr = (int)(60 * (1.0f - gt));
                int gg = (int)(190 - gt * 85);
                int bb = (int)(215 + gt * 40);
                SDL_SetRenderDrawColor(r, (Uint8)rr, (Uint8)gg, (Uint8)bb, (Uint8)la);
                SDL_RenderDrawPoint(r, gx, yy);
            }
        }

        // === Lightning branches: jagged lines discharging rightward ===
        if (t > 0.20f) {
            int nBranches = 5;
            for (int b = 0; b < nBranches; ++b) {
                int seed = b * 41 + wallAnimFrame * 17;
                int lx = bx, ly = cy;
                int segs = 3 + (seed % 3);
                for (int s = 0; s < segs; ++s) {
                    int nx = lx + (5 + (seed + s * 7) % 16);  // rightward
                    int ny = ly + ((seed * 3 + s * 13) % 23 - 11);
                    if (nx > WIN_WIDTH + 30 || ny < 5 || ny >= WIN_HEIGHT - 5) break;
                    int sa = (int)(alpha * (1.0f - (float)s / segs) * 0.75f);
                    if (sa < 12) break;
                    SDL_SetRenderDrawColor(r, 180, 210, 255, (Uint8)sa);
                    SDL_RenderDrawLine(r, lx, ly, nx, ny);
                    SDL_SetRenderDrawColor(r, 255, 255, 240, (Uint8)(sa / 2));
                    SDL_RenderDrawLine(r, lx + 1, ly, nx + 1, ny);
                    lx = nx; ly = ny;
                }
            }
        }

        // === Energy sparks: bright dots discharging rightward ===
        if (t > 0.12f) {
            int nSparks = 22;
            for (int s = 0; s < nSparks; ++s) {
                int seed = s * 59 + wallAnimFrame * 37;
                int sx = bx + (3 + (seed % 32));  // rightward
                int sy = cy + ((seed * 7 + 19) % 130 - 65);
                if (sx > WIN_WIDTH + 20 || sy < 5 || sy >= WIN_HEIGHT - 5) continue;
                int sa = (int)(alpha * (0.55f + 0.45f * ((float)((seed + s * 11) % 100) / 100.0f)));
                if (sa < 22) continue;
                SDL_SetRenderDrawColor(r, 255, 255, 240, (Uint8)sa);
                SDL_RenderDrawPoint(r, sx, sy);
                if ((seed % 5) == 0) {
                    SDL_RenderDrawPoint(r, sx + 1, sy);
                    SDL_RenderDrawPoint(r, sx, sy - 1);
                }
            }
        }
    }

    void drawAimAssistSide() {
        SDL_Renderer* r = renderer.get();
        int px = player->getX(), py = player->getY();
        // Default: 200px ahead along the horizontal shooting ray
        double defaultDist = 200.0;
        double aimX = px + defaultDist, aimY = py;
        double bestT = 999.0;
        double snapX = aimX, snapY = aimY;
        // Ray: horizontal from player (dx=1, dy=0). Check targets along this ray.
        // No range limit (Ch2 bullets deal damage at any range).

        // Check Ch2 regular aliens (vulnerable state)
        for (const auto& ca : ch2AlienMgr.getAliens()) {
            if (ca.active && !ca.entering && ca.invincibleFrames <= 0) {
                double t = ca.x - px;
                if (t > 0) {
                    double lateral = std::fabs(ca.y - py);
                    if (lateral < 18.0 && t < bestT) { bestT = t; snapX = ca.x; snapY = ca.y; }
                }
            }
        }

        // Check Ch2 crystal bullets (from regular aliens)
        auto& cbullets = ch2AlienMgr.getBullets();
        for (const auto& cb : cbullets) {
            if (!cb.active) continue;
            double t = cb.x - px;
            if (t > 0) {
                double lateral = std::fabs(cb.y - py);
                if (lateral < 16.0 && t < bestT) { bestT = t; snapX = cb.x; snapY = cb.y; }
            }
        }

        // Check danmaku enemies (vulnerability window)
        for (const auto& de : dmMgr.getEnemies()) {
            if (de.active && !de.entering && de.invincibleFrames <= 0 && de.vulnTimer > 0) {
                double t = de.x - px;
                if (t > 0) {
                    double lateral = std::fabs(de.y - py);
                    if (lateral < 24.0 && t < bestT) { bestT = t; snapX = de.x; snapY = de.y; }
                }
            }
        }

        // Check danmaku enemy bullets
        auto& dbullets = dmMgr.getBullets();
        for (const auto& db : dbullets) {
            if (!db.active) continue;
            double t = db.x - px;
            if (t > 0) {
                double lateral = std::fabs(db.y - py);
                if (lateral < 16.0 && t < bestT) { bestT = t; snapX = db.x; snapY = db.y; }
            }
        }

        player->aimAssist.update(bestT < 999.0);
        double drawX = (bestT < 999.0) ? snapX : aimX;
        double drawY = (bestT < 999.0) ? snapY : aimY;
        player->aimAssist.draw(r, drawX, drawY, 3, 2);
    }

    void drawAimAssist() {
        SDL_Renderer* r = renderer.get();
        double tPlane = (player->getX() - perspLeft(player->getY())) / perspWidth(player->getY());
        if (tPlane < 0.0) tPlane = 0.0; if (tPlane > 1.0) tPlane = 1.0;
        double range = BulletManager::getBulletRange();
        double ty = player->getY() - range;
        double tx = perspLeft(ty) + tPlane * perspWidth(ty);
        double dx = tx - player->getX(), dy = ty - player->getY();
        double fullDist = std::sqrt(dx*dx + dy*dy);
        if (fullDist < 1.0) return;
        double defaultDist = fullDist * 0.40;

        double ex = player->getX() + dx / fullDist * defaultDist;
        double ey = player->getY() + dy / fullDist * defaultDist;

        // Snap to nearest enemy
        double bestT = 999.0;
        double snapX = ex, snapY = ey;
        if (std::fabs(dy) > 0.001) {
            for (const auto& a : alienMgr.all()) {
                if (!a.active || a.entering || a.invincibleFrames != 0) continue;
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                double t = (a.y - player->getY()) / dy;
                if (t < 0.0 || t > 0.80) continue;
                double hitX = player->getX() + dx * t;
                double dist = std::fabs(hitX - ax);
                double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                double alienScale = (depthBelow < 0) ? 0.08 : 0.08 + 0.92 * depthBelow;
                if (alienScale < 0.08) alienScale = 0.08;
                double snapR = 28.0 * alienScale + 10.0;  // identical to bullet hitRadius
                if (dist < snapR && t < bestT) { bestT = t; snapX = ax; snapY = a.y; }
            }
            if (boss.isActive() && phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2) {
                double bt = (boss.getY() - player->getY()) / dy;
                if (bt > 0.0 && bt < 0.80) {
                    double hitX = player->getX() + dx * bt;
                    if (std::fabs(hitX - boss.getX()) < 55.0 && bt < bestT) {  // same as bullet hit radius
                        bestT = bt; snapX = boss.getX(); snapY = boss.getY();
                    }
                }
            }
        }

        player->aimAssist.update(bestT < 999.0);
        double drawX = (bestT < 999.0) ? snapX : ex;
        double drawY = (bestT < 999.0) ? snapY : ey;
        player->aimAssist.draw(r, drawX, drawY, 5, 3);
    }

    // ======== PAUSED ========
    void updatePaused(const Uint8* keys, bool& running) {
        if (countdown >= 0) {
            if (countdownFrame == 0) {
                particleMgr.spawnDigitShatter(font, '0' + countdown, 18, CENTER_X, WIN_HEIGHT/2);
            }
            countdownFrame++;
            particleMgr.update();
            particleMgr.removeInactive();
            if (countdownFrame >= 45) {
                countdownFrame = 0;
                countdown--;
                if (countdown <= 0) {
                    paused = false; countdown = -1;
                    if (score >= 30) shockwaveMgr.setPending(true);
                }
            }
        } else {
            static bool pUpWas = false, pDownWas = false, pEnterWas = false, pLeftWas = false, pRightWas = false;
            bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool leftNow = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            bool enterNow = keys[SDL_SCANCODE_RETURN];

            // Left/Right: switch focus between menu and history
            if (leftNow && !pLeftWas)  pauseHistoryFocused = false;
            if (rightNow && !pRightWas) pauseHistoryFocused = true;

            if (!pauseHistoryFocused) {
                // Menu focused
                if (upNow && !pUpWas && pauseMenuSelection > 0)     pauseMenuSelection--;
                if (downNow && !pDownWas && pauseMenuSelection < 4) pauseMenuSelection++;
                if (enterNow && !pEnterWas) {
                    if (pauseMenuSelection == 0)      { countdown = 3; countdownFrame = 0; }
                    else if (pauseMenuSelection == 1) { resetGame(); paused = false; }
                    else if (pauseMenuSelection == 2) { paused = false; atOptionScreen = true; optionFromPause = true; }
                    else if (pauseMenuSelection == 3) { resetGame(); atStartScreen = true; paused = false; }
                    else                              running = false;
                }
            } else {
                // History focused: focus moves, then scrolls at boundaries
                if (upNow && !pUpWas) dialogueSys.history.moveUp();
                if (downNow && !pDownWas) dialogueSys.history.moveDown();
            }
            pUpWas = upNow; pDownWas = downNow; pEnterWas = enterNow;
            pLeftWas = leftNow; pRightWas = rightNow;
        }
    }

    void drawPauseMenu() {
        SDL_Renderer* r = renderer.get();
        UIRenderer::drawHalfTransparentOverlay(r, 180);

        // === Left half: menu ===
        bool menuFocus = !pauseHistoryFocused;
        font.drawString(r, "PAUSED", 50, 100, 3);
        const char* items[5] = {"RESUME", "RESTART", "OPTIONS", "BACK TO MAIN MENU", "EXIT"};
        const int MENU_Y0 = 160, GAP = 40;
        for (int i = 0; i < 5; ++i) {
            int itemW = (int)strlen(items[i]) * 6 * 3;
            int itemX = 50;
            int itemY = MENU_Y0 + i * GAP;
            font.drawString(r, items[i], itemX, itemY, 3);
            if (menuFocus && i == pauseMenuSelection) {
                UIRenderer::drawMenuCursor(r, itemX - 20, itemY + 10, 8);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
            }
        }
        font.drawString(r, "A/D:switch  W/S:menu  ENTER:confirm", 30, 480, 2);

        // === Right half: dialogue history ===
        const int RX = 420;
        font.drawString(r, "HISTORY DIALOGUE", RX, 100, 3);

        auto& hist = dialogueSys.history;
        int total = (int)hist.size();
        if (total == 0) return;

        const int CH_W = 12, CH_H = 14;
        const int RY_TOP = 160;
        const int LINE_GAP = 14;  // one blank line between blocks
        const int TR = 50, TG = 155, TB = 70;
        const int SR = 180, SG = 200, SB = 160;

        int bottomIdx = total - 1 - dialogueSys.history.scroll;
        if (bottomIdx < 0) { bottomIdx = 0; dialogueSys.history.scroll = total - 1; }
        int maxSlot = (total >= 3) ? 2 : total - 1;
        int fSlot = dialogueSys.history.focusSlot;
        if (fSlot > maxSlot) fSlot = maxSlot;

        // Draw items top to bottom with LINE_GAP between
        const int HIST_WRAP = 30;
        int curY = RY_TOP;
        for (int s = 0; s <= maxSlot; ++s) {
            int idx = bottomIdx - (maxSlot - s);
            if (idx < 0 || idx >= total) continue;
            auto& hl = hist[idx];

            // Opacity based on distance from focus
            int dist = (s > fSlot) ? s - fSlot : fSlot - s;
            int alpha = 100;
            if (dist == 1) alpha = 50;
            else if (dist >= 2) alpha = 30;
            if (!pauseHistoryFocused) alpha = 30;

            int rr = (int)(TR * alpha / 100.0);
            int gg = (int)(TG * alpha / 100.0);
            int bb = (int)(TB * alpha / 100.0);
            int sr2 = (int)(SR * alpha / 100.0);
            int sg2 = (int)(SG * alpha / 100.0);
            int sb2 = (int)(SB * alpha / 100.0);

            // Re-wrap lines for narrow history panel
            std::vector<std::string> wlines;
            for (auto& line : hl.lines) {
                std::string s = line;
                while ((int)s.length() > HIST_WRAP) {
                    int brk = HIST_WRAP;
                    while (brk > 0 && s[brk] != ' ') brk--;
                    if (brk == 0) brk = HIST_WRAP;
                    wlines.push_back(s.substr(0, brk));
                    s = s.substr(brk + 1);
                }
                if (!s.empty()) wlines.push_back(s);
            }
            int wNum = (int)wlines.size();
            int itemH = wNum * CH_H + (wNum - 1) * 4;
            int speakerOff = 0;
            if (!hl.speaker.empty()) { speakerOff = CH_H + 2; itemH += speakerOff; }

            int itemY = curY;

            // Speaker
            if (!hl.speaker.empty()) {
                SDL_SetRenderDrawColor(r, (Uint8)sr2, (Uint8)sg2, (Uint8)sb2, 255);
                int tx = RX, ty = itemY;
                for (char c : hl.speaker) {
                    if (c == ' ') { tx += CH_W; continue; }
                    font.drawChar(r, c, tx, ty, 1, 2);
                    tx += CH_W;
                }
                itemY += CH_H + 2;
            }
            // Content
            SDL_SetRenderDrawColor(r, (Uint8)rr, (Uint8)gg, (Uint8)bb, 255);
            for (int li = 0; li < wNum; ++li) {
                auto& line = wlines[li];
                int tx = RX;
                int ty = itemY + li * (CH_H + 4);
                for (char c : line) {
                    if (c == ' ') { tx += CH_W; continue; }
                    font.drawChar(r, c, tx, ty, 1, 2);
                    tx += CH_W;
                }
            }
            curY += itemH + LINE_GAP;
        }
    }

    void drawCountdown() {
        SDL_Renderer* r = renderer.get();
        UIRenderer::drawHalfTransparentOverlay(r, 160);
        double progress = countdownFrame / 45.0;
        if (progress > 1.0) progress = 1.0;
        double eased = 1.0 - std::pow(1.0 - progress, 2.5);
        char digit = '0' + countdown;
        int mainScale = 3 + (int)(eased * 15);
        for (int i = 2; i >= 0; --i) {
            double t = (double)(i + 1) / 3.0;
            int gs = mainScale + (int)(t * eased * 28.0);
            int alpha = (int)(180.0 * (1.0 - t) * (1.0 - eased * 0.4));
            if (alpha < 8) continue;
            int gw = 5 * gs, gh = 7 * gs;
            int gx = CENTER_X - gw / 2, gy = WIN_HEIGHT / 2 - gh / 2;
            SDL_SetRenderDrawColor(r, (Uint8)alpha, (Uint8)alpha, (Uint8)alpha, 255);
            font.drawChar(r, digit, gx, gy, gs);
        }
        int dw = 5 * mainScale, dh = 7 * mainScale;
        int dx = CENTER_X - dw / 2, dy = WIN_HEIGHT / 2 - dh / 2;
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        font.drawChar(r, digit, dx, dy, mainScale);
    }

    // ======== GAME OVER ========
    void updateGameOverScreen(const Uint8* keys, bool&) {
        static bool upWas2 = false, downWas2 = false, enterWas2 = false;
        bool upNow2 = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow2 = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow2 = keys[SDL_SCANCODE_RETURN];
        if (upNow2 && !upWas2 && menuSelection > 0)   menuSelection--;
        if (downNow2 && !downWas2 && menuSelection < 1) menuSelection++;
        if (enterNow2 && !enterWas2) {
            if (menuSelection == 0) {
                // PLAY AGAIN: restart current chapter
                bool wasNormal = isNormalPlay;
                resetGame();
                atStartScreen = false; atChapterSelect = false;
                isNormalPlay = wasNormal;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
            } else {
                // BACK TO MAIN MENU
                resetGame(); atStartScreen = true;
            }
        }
        upWas2 = upNow2; downWas2 = downNow2; enterWas2 = enterNow2;
        particleMgr.update();
        particleMgr.removeInactive();
    }

    void drawGameOverScreen() {
        SDL_Renderer* r = renderer.get();
        UIRenderer::drawHalfTransparentOverlay(r, 200);
        font.drawString(r, "GAME OVER", CENTER_X - 118, 140, 4);
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE:%-4d", score);
        int scoreW = (int)strlen(buf) * 6 * 4;
        font.drawString(r, buf, CENTER_X - scoreW/2, 210, 4);
        const char* items[2] = {"PLAY AGAIN", "BACK TO MAIN MENU"};
        const int MENU_Y0 = 340;
        for (int i = 0; i < 2; ++i) {
            int itemW = (int)strlen(items[i]) * 6 * 3;
            int itemX = CENTER_X - itemW / 2;
            int itemY = MENU_Y0 + i * 60;
            font.drawString(r, items[i], itemX, itemY, 3);
            if (i == menuSelection) {
                UIRenderer::drawMenuCursor(r, itemX - 20, itemY + 10, 8);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
            }
        }
        font.drawString(r, "W/S:select  ENTER:confirm", CENTER_X - 150, 490, 2);
    }

    // ======== MISSION COMPLETE ========
    void updateMissionComplete(const Uint8* keys) {
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        static bool entWas = false;
        if (enterNow && !entWas) { resetGame(); atStartScreen = true; }
        entWas = enterNow;
    }

    void drawMissionComplete() {
        SDL_Renderer* r = renderer.get();
        int fadeAlpha = defeatFadeTimer * 3;
        if (fadeAlpha > 220) fadeAlpha = 220;
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)fadeAlpha);
        SDL_Rect dr = {0, 0, WIN_WIDTH, WIN_HEIGHT};
        SDL_RenderFillRect(r, &dr);
        if (fadeAlpha > 160) {
            const int TR = 50, TG = 155, TB = 70;
            // Narration-style text box
            const char* title = "MISSION COMPLETE";
            int titleLen = (int)strlen(title);
            int titleW = titleLen * 18 + 50;
            int titleH = 21 + 40;
            int boxX = CENTER_X - titleW / 2, boxY = 200;
            // Background
            SDL_SetRenderDrawColor(r, 10, 25, 15, 220);
            SDL_Rect bgRect = {boxX, boxY, titleW, titleH};
            SDL_RenderFillRect(r, &bgRect);
            SDL_SetRenderDrawColor(r, TR, TG, TB, 180);
            SDL_RenderDrawRect(r, &bgRect);
            // Title text
            SDL_SetRenderDrawColor(r, TR, TG, TB, 255);
            int tx = boxX + 25;
            for (const char* p = title; *p; ++p) {
                if (*p != ' ') font.drawChar(r, *p, tx, boxY + 22, 3);
                tx += 18;
            }
            // Menu options
            const int MENU_Y = 310, MENU_GAP = 40;
            const char* items[2] = {"NEXT CHAPTER", "BACK TO MAIN MENU"};
            for (int i = 0; i < 2; ++i) {
                int iy = MENU_Y + i * MENU_GAP;
                SDL_SetRenderDrawColor(r, TR, TG, TB, (i == mcMenuSelection) ? 255 : 140);
                int itemW = (int)strlen(items[i]) * 12; // scale=2, charW=6*2
                int ix = CENTER_X - itemW / 2;
                font.drawString(r, items[i], ix, iy, 2);
                if (i == mcMenuSelection) {
                    SDL_SetRenderDrawColor(r, 255, 255, 0, 200);
                    SDL_RenderDrawLine(r, ix, iy + 20, ix + itemW, iy + 20);
                }
            }
        }
    }

    // ======== NARRATION ========
    void startChapterNarration() {
        narration.reset();
        int ch = chapterMgr.getCurrentIndex();
        switch (ch) {
            case 0:
                narration.queue("Stellar Calendar 24th.\nIn the deep space.\nThe \"Life\" base.");
                narration.queue("Martha aces her final test.\nShe's now a member of \"Huntress\".");
                narration.queue("When she is just off the plane.\nHeading for the rest area.\nAnnouncement sounds.");
                narration.queue("Tower: \"Hostile objects approaching!\nAll fighters scramble!\"");
                narration.queue("\"Repeat: Scramble! Scramble!\"");
                narration.queue("At the same moment,\na series of massive explosions behind her.\nShe turns --");
                narration.queue("All fighters become wreckage...");
                narration.queue("Martha runs back to the hangar.\nOnly one old trainer left.");
                narration.queue("Into the cockpit. Engines up.\nShe reports:\n\"I'm taking off. Flight code:21395\"");
                narration.queue("No response from the tower.\nOnly a sharp, harsh noise.");
                narration.queue("Martha heads into space.\nAgain and alone.");
                break;
            case 1:
                narration.queue("A hard-won victory. But the cost is plain.\nThe \"Life\" base lies scarred and broken.\nRepairs will take weeks.");
                narration.queue("The shockwave defense system,\npushed far beyond its limits,\nneeds a full overhaul\nbefore it can fire again.");
                narration.queue("The base's long-range comms array\nis destroyed.\nMartha must fly to the\n\"Moonwell\" R&D center,\nwith Bryssa in her backseat,\nto bring home new equipment.");
                narration.queue("Ally enters its standby mode.\nBryssa, exhausted, drifts off.\nThe old trainer's engine hums\na tired, weary vibration.");
                narration.queue("Space is terribly quiet...");
                narration.queue("......");
                narration.queue("............");
                narration.queue("After an eternity in the void,\nthe R&D center finally\ndrifts into Martha's comms range.");
                narration.queue("Martha opens the channel.\n\"Moonwell, Moonwell, this is Martha\nfrom \'Life\' base. Flight code 21395.\"");
                narration.queue("\"......\"");
                narration.queue("Silence.");
                narration.queue("\"Moonwell, Moonwell, 21395.\nRequesting landing clearance.\"");
                narration.queue("\"......\"");
                narration.queue("Nothing but still silence.");
                narration.queue("Martha has already flown to the center's gate.\nA massive vacuum door blocks her way.");
                break;
            case 2:
                narration.queue("CH.3  ENEMY FORTRESS");
                narration.queue("We have located the enemy stronghold.");
                narration.queue("Break through their defense line!");
                break;
            case 3:
                narration.queue("CH.4  ASTEROID BELT");
                narration.queue("Danger lurks among the asteroids.");
                narration.queue("Fly carefully and stay alert.");
                break;
            case 4:
                narration.queue("CH.5  FINAL ASSAULT");
                narration.queue("This is the final battle.");
                narration.queue("Give it everything you've got!");
                break;
        }
        narration.start();
        inNarration = true;
    }

    void updateNarration(const Uint8* keys) {
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        narration.update(enterNow);
        int cticks = narration.popTicks();
        while (cticks-- > 0) audio.sndTeletype();
        if (!narration.isActive()) inNarration = false;
        if (background) background->update();
        if (sideBg) sideBg->update();
    }

    void drawNarrationFrame() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0);
        renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        narration.draw(r, font);
        SDL_SetRenderDrawColor(r, 130, 130, 130, 255);
        font.drawString(r, "ENTER:continue", CENTER_X - 78, WIN_HEIGHT - 40, 2);
    }

};


// ============== MAIN ==============
int main() {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Huntress: Echoes from the Well",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WIN_WIDTH, WIN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); return 1; }

    Renderer renderer(window);
    AudioEngine audio;
    Game game(renderer, audio, window);
    game.run();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
