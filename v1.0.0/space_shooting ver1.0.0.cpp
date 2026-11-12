#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>

// ============== 音效系统 ==============
SDL_AudioDeviceID audioDev = 0;
int bgmVolume = 7;            // 背景音乐音量 1-10
int sfxVolume = 7;            // 音效音量 1-10
int eqLow  = 0;               // 低频增益 -5 ~ +5
int eqMid  = 0;               // 中频增益 -5 ~ +5
int eqHigh = 0;               // 高频增益 -5 ~ +5
bool bossMusicOn = false;     // Boss战BGM切换标志

struct ActiveSound {
    float freq, sweepEnd;
    int totalSamples, samplesLeft;
    float volume;
    int type;   // 0=sine, 1=square, 2=noise, 3=sweep
    int band;   // 0=low, 1=mid, 2=high
    float phase;
};
std::vector<ActiveSound> activeSnd;

// BGM 状态
float bgmPhase = 0.0f;
float bgmPulsePhase = 0.0f;
int bgmStepCounter = 0;
int bgmNoteIndex = 0;
float bgmNoteFreq = 0.0f;
int bgmNoteLen = 0;
// 太空射击 BGM 音符序列 (低频旋律)
const float BGM_MELODY[32] = {
    110,130,146,174,196,174,146,130,  110,130,146,174,196,220,196,174,
    130,146,174,196,220,196,174,146,  110,130,146,174,196,174,146,130
};
const int BGM_NOTE_DUR[32] = {
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,12,12,
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,16,16
};
// Boss战 BGM (更快、更紧迫)
const float BOSS_MELODY[16] = {
    196,220,261,294,330,294,261,220,  261,294,330,392,330,294,261,220
};
const int BOSS_DUR[16] = {
    6,6,4,4,6,6,4,4,  6,4,4,6,8,6,4,8
};

void audioCB(void*, Uint8* stream, int len) {
    float* buf = (float*)stream;
    int n = len / (int)sizeof(float);
    memset(buf, 0, len);

    // === BGM：根据Boss状态切换 ===
    bool bossFight = bossMusicOn;
    const float* melodyTbl = bossFight ? BOSS_MELODY : BGM_MELODY;
    const int*   durTbl    = bossFight ? BOSS_DUR    : BGM_NOTE_DUR;
    int noteCount = bossFight ? 16 : 32;
    int tickLen   = bossFight ? 450 : 800;

    for (int i = 0; i < n; ++i) {
        // 旋律音符
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
        float melodyVol = bossFight ? 0.035f : 0.025f;
        float melody = sinf(bgmPhase * 2.0f * M_PI) * env * melodyVol;

        // 低频贝斯 (Boss战时更强、更快脉动)
        float bassFreq = bossFight ? 65.0f : 55.0f;
        float pulseSpeed = bossFight ? 0.0015f : 0.0005f;
        bgmPulsePhase += bassFreq / 44100.0f;
        if (bgmPulsePhase > 2.0f) bgmPulsePhase -= 2.0f;
        float pulse = 0.5f + 0.5f * sinf(bgmStepCounter * pulseSpeed);
        float bassVol = bossFight ? 0.025f : 0.018f;
        float bass = sinf(bgmPulsePhase * 2.0f * M_PI) * pulse * bassVol;

        buf[i] = (melody + bass) * (bgmVolume / 5.0f);
        bgmStepCounter++;
    }

    // === SFX ===
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
            if (s.type == 3)
                freq = s.freq + (s.sweepEnd - s.freq) * t;

            s.phase += freq / 44100.0f;
            if (s.phase > 2.0f) s.phase -= 2.0f;

            float smp = 0.0f;
            if (s.type == 2)
                smp = ((rand() % 2000) / 1000.0f - 1.0f);
            else if (s.type == 1)
                smp = (sinf(s.phase * 2.0f * M_PI) > 0.0f ? 1.0f : -1.0f) * 0.5f;
            else
                smp = sinf(s.phase * 2.0f * M_PI);

            buf[i] += smp * env * s.volume * (sfxVolume / 7.0f) * eqGain[s.band];
            s.samplesLeft--;
        }
    }
    // 钳制
    for (int i = 0; i < n; ++i) {
        if (buf[i] > 0.9f) buf[i] = 0.9f;
        if (buf[i] < -0.9f) buf[i] = -0.9f;
    }
    // 清理已完成音效
    activeSnd.erase(std::remove_if(activeSnd.begin(), activeSnd.end(),
        [](const ActiveSound& s){ return s.samplesLeft <= 0; }), activeSnd.end());
}

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

// 预定义音效  (band: 0=low, 1=mid, 2=high)
void sndShoot()     { playSound(1200, 600, 28, 0.30f, 3, 2); }  // 激光枪：高频快速下行
void sndHit()       { playSound(350, 0, 35, 0.35f, 1, 1); }
void sndExplosionSmall() { playSound(80, 0, 80, 0.40f, 2, 0); }
void sndExplosionBig()   { playSound(50, 0, 180, 0.55f, 2, 0); }
void sndShockwave() { playSound(60, 150, 250, 0.50f, 3, 0); }
void sndShockwaveHit()   { playSound(100, 0, 60, 0.40f, 0, 1); }
void sndBossHeal()  { playSound(300, 600, 120, 0.35f, 3, 1); }
void sndBossAbsorb() { playSound(200, 80, 200, 0.40f, 3, 0); }
void sndBossEntrance() {                        // 震慑力入场：多层和弦
    playSound(60, 25, 700, 0.55f, 3, 0);
    playSound(150, 60, 550, 0.40f, 3, 0);
    playSound(280, 160, 350, 0.28f, 3, 0);
    playSound(180, 90, 500, 0.30f, 1, 1);
    playSound(40, 0, 120, 0.45f, 2, 0);
}
void sndBaseDamage() { playSound(45, 0, 150, 0.45f, 2, 0); }
void sndBossHit() {                             // 低沉金属碰撞
    playSound(280, 0, 28, 0.32f, 1, 2);        // 中低金属ping (低于外星sndHit的350Hz)
    playSound(80, 0, 35, 0.45f, 1, 0);         // 深沉碰撞体
}
void sndShake()   { playSound(35, 0, 100, 0.35f, 2, 0); }  // 震动低音

// ============== 窗口 & 透视常量 ==============
const int WIN_WIDTH = 800;
const int WIN_HEIGHT = 600;
const int CENTER_X = WIN_WIDTH / 2;
const int HORIZON_Y = 200;
const double HORIZON_LEFT  = WIN_WIDTH * 0.25;  // 地平线处50%宽度
const double HORIZON_RIGHT = WIN_WIDTH * 0.75;

// ============== 透视边界辅助 ==============
inline double perspLeft(double y) {
    return HORIZON_LEFT  * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspRight(double y) {
    return WIN_WIDTH - HORIZON_LEFT * (WIN_HEIGHT - y) / (WIN_HEIGHT - HORIZON_Y);
}
inline double perspWidth(double y) {
    return perspRight(y) - perspLeft(y);
}

// ============== 飞机参数 ==============
int planeX = CENTER_X;
int planeY = WIN_HEIGHT - 80;
const int PLANE_HALF_WIDTH = 15;
double rollAngle = 0.0;
double rollTarget = 0.0;           // 翻滚目标角度（2π的整数倍）
const double ROLL_SPEED = 0.314;   // 翻滚速度（20帧一圈，10帧半圈）
int lastMoveDir = 0;               // 上一帧移动方向 -1/0/1

// ============== 游戏状态 ==============
int score = 0;
bool gameOver = false;
bool paused = false;
bool atStartScreen = true;    // 开始界面
bool atTestSelect = false;    // 测试模式-分数选择
bool atOptionScreen = false;  // 选项界面
bool atSoundMenu = false;     // 声音子菜单
bool optionFromPause = false; // 从暂停进入选项
bool aimAssistOn = false;     // 瞄准辅助开关（默认关闭）
int optionCursor = 0;         // 选项界面光标位置
bool optionJustEntered = true; // 刚进入选项界面，防按键穿透
int menuSelection = 0;        // game over 菜单选项
int pauseMenuSelection = 0;   // 暂停菜单选项
int startMenuSelection = 0;   // 开始界面选项
int testScoreSelection = 0;   // 测试分数选项索引
int countdown = -1;
int countdownFrame = 0;
int difficultyTimer = 0;
int baseHP = 10;
int shockwavePending = 0;     // >0时下一帧立刻释放冲击波
int lastShockwaveLevel = 0;   // 上一次冲击波等级，检测升级
const int BASE_MAX_HP = 10;

// ============== Boss 阶段 ==============
enum GamePhase { PHASE_PLAY, PHASE_BOSS_INTRO, PHASE_BOSS_FIGHT, PHASE_BOSS_PHASE2, PHASE_BOSS_DEFEAT };
GamePhase phase = PHASE_PLAY;
int shakeTimer = 0;
int shakeX = 0, shakeY = 0;
int absorbTimer = 0;
int absorbIndex = 0;
int absorbCooldown = 0;    // 上次吸收完成后的冷却帧数
int absorbState = 0;       // 0=空闲找目标, 1=光束飞行中, 2=螺旋吸收中, 3=冷却中
int absorbTargetIdx = -1;  // 当前吸收目标索引
int postAbsorbTimer = 0;   // 吸收完毕后震动计时
double snapProgress = 0.0;  // 瞄准辅助吸附过渡（0=方形, 1=仅白点）
double bossX = CENTER_X;       // Boss 当前X坐标
double bossMoveTime = 0;       // Boss移动计时器（∞轨迹）
int bossFlashTimer = 0;        // Boss受击高亮倒计时
bool bossPhase2Triggered = false;  // 50%血量事件是否已触发
int bossDefeatTimer = 0;           // Boss战败动画计时器
int defeatAlienTimer = 0;          // 逐个引爆外星飞船间隔计时
int defeatReturnTimer = 0;         // 飞机回中/前飞计时
int defeatFWTimer = 0;             // 烟花计时
int defeatMCDelay = 0;             // MISSION COMPLETE 延迟
int defeatFadeTimer = 0;           // 结束画面淡入计时
bool missionCompleteShown = false; // MISSION COMPLETE 是否已弹出
bool missionComplete = false;      // 结束画面弹窗

// ============== Boss治疗红波 ==============
struct HealWave {
    double radius;
    int id;          // 唯一编号，防止同一波多次命中同一飞船
    bool active;
};
std::vector<HealWave> healWaves;
int healWaveTimer = 0;
bool healWavesEnabled = false;   // Phase2完成后才启用治疗红波
int nextHealWaveID = 0;

// ============== 子弹 ==============
struct Bullet {
    double x, y, dx, dy;
    double startX, startY;
    bool active;
    bool canDamage;
    bool blueBeam;               // Boss吸收光束（蓝色慢速）
    int beamTargetIndex;         // 目标外星飞船索引
};
std::vector<Bullet> bullets;
int fireCooldown = 0;
const int BASE_FIRE_DELAY = 7;
const double BASE_BULLET_SPEED = 60.0;
const double BULLET_RANGE = 370.0;   // 固定飞行距离，不依赖地平线

// 根据冲击波等级计算当前射速和弹速（每级+10%）
inline int currentFireDelay() {
    int lv = score / 30;
    if (lv < 1) lv = 1;
    double factor = std::pow(0.9, lv - 1);   // 每级减少10%间隔
    int d = (int)(BASE_FIRE_DELAY * factor);
    return d < 3 ? 3 : d;
}
inline double currentBulletSpeed() {
    int lv = score / 30;
    if (lv < 1) lv = 1;
    return BASE_BULLET_SPEED * std::pow(1.1, lv - 1);  // 每级增加10%速度
}
inline int currentShockwaveDamage() {
    int lv = score / 30;
    if (lv < 1) lv = 1;
    return 3 + (lv - 1);   // 30分=3, 60分=4, 90分=5 ...
}
inline int currentShockwaveInterval() {
    // 冲击波自动释放间隔（帧数）：lv1=10s, lv2=8s, lv3=6s, lv4=4s, lv5=2s, lv6=1s
    int lv = score / 30;
    if (lv < 1) lv = 1;
    int sec = 12 - lv * 2;
    if (sec < 1) sec = 1;
    return sec * 60;
}

// ============== 外星飞船 ==============
struct Alien {
    double y;               // 当前 y
    double t;               // 当前透视线参数
    double startT;          // 突袭起始 t（侧面入场用）
    double targetT;         // 突袭终点 t
    double enterStartY;     // 突袭起始 y（顶部入场用）
    double enterTargetY;    // 突袭终点 y（顶部入场用）
    bool entering;          // 是否还在突袭动画中
    bool enterFromTop;      // true=顶部突入, false=侧面突入
    bool enterFromBoss;     // true=从Boss中心向外突袭
    int enterFrame;         // 突袭已过帧数
    int enterDuration;      // 突袭总帧数
    int invincibleFrames;   // >0或-1(入场中)=无敌，被击中不扣血只消弹
    int lastHitBySW;        // 上次命中本飞船的冲击波ID，防止同一冲击波多次扣血
    int lastHealHit;        // 上次治疗波ID，防止同一波重复治疗
    bool beingAbsorbed;     // 正在被Boss吸收动画中
    int absorbFrame, absorbDuration;
    double absorbStartX, absorbStartY;
    int hp, maxHp;
    bool active;
};
std::vector<Alien> aliens;
int spawnTimer = 0;
double alienBaseSpeed = 1.75;

inline int currentSpawnInterval() {
    // 基础间隔随分数递减：0分→110帧, 150分→15帧（每秒3-5个）
    int base = 110 - score * 95 / 150;
    if (base < 12) base = 12;
    // 随时间进一步缩短
    base -= (difficultyTimer / 720) * 5;
    if (base < 8) base = 8;
    return base;
}
const int ALIEN_MIN_HP = 3;
const int ALIEN_MAX_HP = 5;

// ============== 爆炸粒子 ==============
struct Particle {
    double x, y, vx, vy;
    int life;
    bool active;
    bool whiteParticle;   // 白色粒子（倒计时碎裂专用）
    bool greenParticle;   // 绿色粒子（冲击波释放特效）
    bool redParticle;     // 红色粒子（Boss吸收特效）
};
std::vector<Particle> particles;

// ============== 冲击波 ==============
struct Shockwave {
    double y;        // 当前位置（拱底 y 坐标）
    int id;          // 唯一编号（防止同一冲击波多次命中同一飞船）
    bool active;
};
int nextShockwaveID = 0;
std::vector<Shockwave> shockwaves;
int shockwaveTimer = 0;       // 距下次释放的帧计数
int shockwaveKills = 0;       // 冲击波累计击杀（每3个+1分）

// ============== Boss ==============
struct Boss {
    double y;
    int hp, maxHp, bonusHp;
    bool active, entering;
    int enterFrame, enterDuration;
    int lastHitBySW;     // 上次命中的冲击波ID
};
Boss boss;

// ============== 星空 ==============
struct Star { float x, y; float phase; float twinkleSpeed; float driftSpeed; };
std::vector<Star> stars;

void updateStars() {
    for (auto& s : stars) {
        s.phase += s.twinkleSpeed;
        if (s.phase > 2.0f * M_PI) s.phase -= 2.0f * M_PI;
        s.x += s.driftSpeed;
        if (s.x < -10) s.x = WIN_WIDTH + 10;
        if (s.x > WIN_WIDTH + 10) s.x = -10;
    }
}
void drawStars();

// ============== 浮动文字（升级提示） ==============
struct FloatingText {
    float x, y;
    int life, totalLife;
    char text[32];
    int r, g, b;
};
std::vector<FloatingText> floatingTexts;

void spawnFloatingText(float x, float y, const char* txt, int r=50, int g=255, int b_=80) {
    FloatingText ft;
    ft.x = x; ft.y = y;
    ft.life = 90; ft.totalLife = 90;
    ft.r = r; ft.g = g; ft.b = b_;
    snprintf(ft.text, sizeof(ft.text), "%s", txt);
    floatingTexts.push_back(ft);
}
void updateFloatingTexts() {
    for (auto& ft : floatingTexts) {
        ft.y -= 0.7f;  // 上浮
        ft.life--;
    }
    floatingTexts.erase(std::remove_if(floatingTexts.begin(), floatingTexts.end(),
        [](const FloatingText& ft){ return ft.life <= 0; }), floatingTexts.end());
}
void drawFloatingTexts();

// ============== 全局渲染器 ==============
SDL_Renderer* renderer = nullptr;

// ============== 5x7 位图字体 ==============
struct FontChar { unsigned char rows[7]; };
FontChar FONT[128];

void initFont() {
    auto setChar = [](char c, const char* bits) {
        for (int r = 0; r < 7; ++r)
            FONT[(int)c].rows[r] = (unsigned char)bits[r];
    };
    // 数字 0-9
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
    // 大写字母 A-Z
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
    // 小写字母 a-z
    setChar('a', "\x00\x00\x0E\x01\x0F\x11\x0F");
    setChar('b', "\x10\x10\x1E\x11\x11\x11\x0E");
    setChar('c', "\x00\x00\x0E\x10\x10\x11\x0E");
    setChar('d', "\x01\x01\x0F\x11\x11\x11\x0F");
    setChar('e', "\x00\x00\x0E\x11\x1F\x10\x0E");
    setChar('f', "\x02\x04\x0E\x04\x04\x04\x0E");
    setChar('g', "\x00\x00\x0F\x11\x0F\x01\x0E");
    setChar('h', "\x10\x10\x16\x19\x11\x11\x11");
    setChar('i', "\x04\x00\x0C\x04\x04\x04\x0E");
    setChar('j', "\x02\x00\x06\x02\x02\x12\x0C");
    setChar('k', "\x10\x10\x12\x14\x18\x14\x12");
    setChar('l', "\x0C\x04\x04\x04\x04\x04\x0E");
    setChar('m', "\x00\x00\x1B\x15\x15\x11\x11");
    setChar('n', "\x00\x00\x1E\x11\x11\x11\x11");
    setChar('o', "\x00\x00\x0E\x11\x11\x11\x0E");
    setChar('p', "\x00\x00\x1E\x11\x11\x1E\x10");
    setChar('r', "\x00\x00\x16\x19\x10\x10\x10");
    setChar('s', "\x00\x00\x0E\x10\x0E\x01\x1E");
    setChar('t', "\x08\x08\x1C\x08\x08\x09\x06");
    setChar('u', "\x00\x00\x11\x11\x11\x13\x0D");
    setChar('v', "\x00\x00\x11\x11\x0A\x0A\x04");
    setChar('w', "\x00\x00\x11\x15\x15\x1B\x11");
    setChar('x', "\x00\x00\x11\x0A\x04\x0A\x11");
    setChar('y', "\x00\x00\x11\x11\x0F\x01\x0E");
    setChar('z', "\x00\x00\x1F\x02\x04\x08\x1F");
    // 符号
    setChar(' ', "\x00\x00\x00\x00\x00\x00\x00");
    setChar('!', "\x04\x04\x04\x04\x00\x00\x04");
    setChar('*', "\x00\x0A\x1F\x1F\x0E\x04\x00");
    setChar('+', "\x00\x00\x04\x0E\x04\x00\x00");
    setChar('-', "\x00\x00\x00\x1F\x00\x00\x00");
    setChar('/', "\x01\x02\x02\x04\x08\x08\x10");
    setChar(':', "\x00\x00\x04\x00\x00\x04\x00");
    setChar('.', "\x00\x00\x00\x00\x00\x00\x04");
}

// ============== 函数声明 ==============
void resetGame();
void drawBackground();
void drawBase();
void drawPlane();
void addBullet();
void updateBullets();
void drawBullet(const Bullet& b);
void spawnAlien();
void updateAliens();
void drawAlien(const Alien& a);
void spawnExplosion(double x, double y, int count);
void updateParticles();
void drawParticle(const Particle& p);
void drawChar(char c, int x, int y, int scale);
void drawString(const char* str, int x, int y, int scale);
void drawGameOver();
void drawPauseMenu();
void drawCountdown();
void spawnDigitShatter(char digit, int digitScale);
void drawScoreHUD();
void spawnShockwave();
void updateShockwaves();
void drawShockwave(const Shockwave& sw);
void triggerBoss();
void drawBossHP();
void drawBossBody();
void spawnAlienFromBoss();
void drawCircularShockwave();
void drawAimAssist();
void drawStartScreen();
void drawTestSelect();
void drawOptionScreen();
void drawSoundMenu(int cursor);

// ============== 主函数 ==============
int main(int argc, char* argv[]) {
    srand((unsigned)time(nullptr));
    initFont();

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
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    // 音频初始化
    {
        SDL_AudioSpec want;
        SDL_memset(&want, 0, sizeof(want));
        want.freq = 44100;
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 1024;
        want.callback = audioCB;
        audioDev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
    }

    for (int i = 0; i < 100; ++i)
        stars.push_back({(float)(rand() % WIN_WIDTH), (float)(rand() % HORIZON_Y),
                         (rand() % 628) / 100.0f, 0.01f + (rand() % 40) / 1000.0f,
                         (rand() % 30 - 15) / 200.0f});

    bool running = true;
    SDL_Event e;
    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                if (!gameOver && !atStartScreen && !atTestSelect) {
                    if (paused && countdown == -1) {
                        countdown = 3; countdownFrame = 0;
                    } else {
                        paused = !paused;
                        pauseMenuSelection = 0;
                    }
                }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(NULL);
        bossMusicOn = (phase == PHASE_BOSS_FIGHT && boss.active);
        updateStars();

        // ======== 开始界面 ========
        if (atStartScreen) {
            static bool sUpWas = false, sDownWas = false, sEnterWas = false;
            bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
            if (upNow && !sUpWas)     startMenuSelection = (startMenuSelection - 1 + 4) % 4;
            if (downNow && !sDownWas) startMenuSelection = (startMenuSelection + 1) % 4;
            if (enterNow && !sEnterWas) {
                if (startMenuSelection == 0) { atStartScreen = false; }
                else if (startMenuSelection == 1) { atStartScreen = false; atTestSelect = true; }
                else if (startMenuSelection == 2) { atStartScreen = false; atOptionScreen = true; optionFromPause = false; }
                else                              running = false;
            }
            sUpWas = upNow; sDownWas = downNow; sEnterWas = enterNow;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            drawStars();
            drawStartScreen();
            SDL_RenderPresent(renderer);
            continue;
        }

        // ======== 测试模式-分数选择 ========
        // ======== 选项界面 ========
        if (atOptionScreen && !atSoundMenu) {
            static bool oUpWas = false, oDownWas = false, oEnterWas = false;
            static bool oLWas = false, oRWas = false, oEscWas = false;
            bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool leftNow  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
            bool escNow   = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
            if (optionJustEntered) {
                oUpWas = upNow; oDownWas = downNow; oEnterWas = enterNow;
                oLWas = leftNow; oRWas = rightNow; oEscWas = escNow;
                optionJustEntered = false;
            }

            if (upNow && !oUpWas)    optionCursor = (optionCursor - 1 + 2) % 2;
            if (downNow && !oDownWas) optionCursor = (optionCursor + 1) % 2;
            if (enterNow && !oEnterWas) {
                if (optionCursor == 0) aimAssistOn = !aimAssistOn;
                else if (optionCursor == 1) { atSoundMenu = true; optionJustEntered = true; }
            }
            if (escNow && !oEscWas) {
                optionCursor = 0;
                optionJustEntered = true;
                atOptionScreen = false;
                if (optionFromPause) paused = true;
                else atStartScreen = true;
            }

            oUpWas = upNow; oDownWas = downNow; oEnterWas = enterNow;
            oLWas = leftNow; oRWas = rightNow; oEscWas = escNow;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            drawStars();
            drawOptionScreen();
            SDL_RenderPresent(renderer);
            continue;
        }

        // ======== 声音子菜单 ========
        if (atSoundMenu) {
            static bool sUpWas = false, sDownWas = false, sEscWas = false, sEnterWas = false;
            static bool sLWas = false, sRWas = false;
            static int  soundCursor = 0;
            static bool sJustEntered = true;
            bool upNow    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool leftNow  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            bool rightNow = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            bool escNow   = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
            bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];

            if (sJustEntered) {
                sUpWas = upNow; sDownWas = downNow; sEscWas = escNow; sEnterWas = enterNow;
                sLWas = leftNow; sRWas = rightNow;
                sJustEntered = false;
            }
            if (upNow && !sUpWas)    soundCursor = (soundCursor - 1 + 6) % 6;
            if (downNow && !sDownWas) soundCursor = (soundCursor + 1) % 6;
            if (escNow && !sEscWas)  { atSoundMenu = false; optionJustEntered = true; sJustEntered = true; }
            if (enterNow && !sEnterWas && soundCursor == 5) { atSoundMenu = false; optionJustEntered = true; sJustEntered = true; }

            auto adjVal = [&](int& val, int lo, int hi) {
                if (leftNow && !sLWas)  { if (val > lo) val--; }
                if (rightNow && !sRWas) { if (val < hi) val++; }
            };
            switch (soundCursor) {
                case 0: adjVal(bgmVolume, 1, 10); break;
                case 1: adjVal(sfxVolume, 1, 10); break;
                case 2: adjVal(eqLow,  -5, 5); break;
                case 3: adjVal(eqMid,  -5, 5); break;
                case 4: adjVal(eqHigh, -5, 5); break;
            }

            sUpWas = upNow; sDownWas = downNow; sEscWas = escNow; sEnterWas = enterNow;
            sLWas = leftNow; sRWas = rightNow;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            drawStars();
            drawSoundMenu(soundCursor);
            SDL_RenderPresent(renderer);
            continue;
        }

        if (atTestSelect) {
            const int testScores[6] = {30, 60, 90, 120, 150, 180};
            static bool tUpWas = false, tDownWas = false, tEnterWas = false;
            static bool justEntered = true;
            bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];
            bool escNow   = keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE];
            if (escNow) { atTestSelect = false; atStartScreen = true; justEntered = true; continue; }
            if (justEntered) {
                tUpWas = upNow; tDownWas = downNow; tEnterWas = enterNow;
                justEntered = false;
            }
            if (upNow && !tUpWas)     testScoreSelection = (testScoreSelection - 1 + 9) % 9;
            if (downNow && !tDownWas) testScoreSelection = (testScoreSelection + 1) % 9;
            if (enterNow && !tEnterWas) {
                atTestSelect = false;
                justEntered = true;
                if (testScoreSelection < 6) {
                    score = testScores[testScoreSelection];
                    shockwavePending = 1;
                } else if (testScoreSelection == 6) {
                    // 200 BOSS：直接进入Boss登场动画
                    score = 200;
                    triggerBoss();
                    for (int i = 0; i < 5; ++i) {
                        Alien a;
                        a.targetT = 0.15 + (rand() % 700) / 1000.0;
                        a.t = a.targetT;
                        a.y = 120.0 + (rand() % 180);
                        a.entering = false;
                        a.enterFromTop = false;
                        a.enterFromBoss = false;
                        a.invincibleFrames = -1;
                        a.lastHitBySW = -1; a.lastHealHit = -1;
                        a.absorbFrame = 0; a.absorbDuration = 0;
                        a.absorbStartX = 0; a.absorbStartY = 0;
                        a.hp = ALIEN_MIN_HP + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
                        a.maxHp = a.hp;
                        a.beingAbsorbed = false;
                        a.active = true;
                        aliens.push_back(a);
                    }
                } else if (testScoreSelection == 7) {
                    // BOSS PH.2：直接进入Boss二阶段吸收动画
                    score = 200;
                    boss.y = 90;
                    boss.hp = 500; boss.maxHp = 1000; boss.bonusHp = 0;
                    boss.active = true;
                    boss.entering = false;
                    boss.lastHitBySW = -1;
                    bossX = CENTER_X;
                    bossMoveTime = 0;
                    bossFlashTimer = 0;
                    bossPhase2Triggered = true;
                    phase = PHASE_BOSS_PHASE2;
                    shakeTimer = 0;
                    absorbTimer = 0;
                    absorbIndex = 0;
                    absorbCooldown = 0;
                    absorbState = 0;
                    healWavesEnabled = false;
                    postAbsorbTimer = 0;
                    for (int i = 0; i < 5; ++i) {
                        Alien a;
                        a.targetT = 0.15 + (rand() % 700) / 1000.0;
                        a.t = a.targetT;
                        a.y = 120.0 + (rand() % 180);
                        a.entering = false;
                        a.enterFromTop = false;
                        a.enterFromBoss = false;
                        a.invincibleFrames = -1;
                        a.lastHitBySW = -1; a.lastHealHit = -1;
                        a.absorbFrame = 0; a.absorbDuration = 0;
                        a.absorbStartX = 0; a.absorbStartY = 0;
                        a.hp = ALIEN_MIN_HP + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
                        a.maxHp = a.hp;
                        a.beingAbsorbed = false;
                        a.active = true;
                        aliens.push_back(a);
                    }
                } else {
                    // BOSS 1HP：快速检验战败流程
                    score = 200;
                    boss.y = 90;
                    boss.hp = 1; boss.maxHp = 1000; boss.bonusHp = 0;
                    boss.active = true;
                    boss.entering = false;
                    boss.lastHitBySW = -1;
                    bossX = CENTER_X;
                    bossMoveTime = 0;
                    bossFlashTimer = 0;
                    bossPhase2Triggered = true;
                    phase = PHASE_BOSS_FIGHT;
                    healWavesEnabled = true;
                    shockwavePending = 1;
                }
            }
            tUpWas = upNow; tDownWas = downNow; tEnterWas = enterNow;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            drawStars();
            drawTestSelect();
            SDL_RenderPresent(renderer);
            continue;
        }

        if (!gameOver && !paused && phase != PHASE_BOSS_DEFEAT) {
            // Boss 触发检测
            if (score >= 200 && phase == PHASE_PLAY) {
                triggerBoss();
            }

            // 难度增长（Boss战期间暂停）
            if (phase == PHASE_PLAY) {
                difficultyTimer++;
                if (difficultyTimer % 720 == 0) {
                    alienBaseSpeed += 0.4;
                }
            }

            bool moveLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
            bool moveRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
            bool moveUp    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool moveDown  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

            if (moveLeft)  planeX -= 7;
            if (moveRight) planeX += 7;
            // 限定在透视线范围内
            double minX = perspLeft(planeY) + 18;
            double maxX = perspRight(planeY) - 18;
            if (planeX < minX) planeX = (int)minX;
            if (planeX > maxX) planeX = (int)maxX;

            if (moveUp)   planeY -= 6;
            if (moveDown) planeY += 6;
            if (planeY < 220) planeY = 220;
            if (planeY > WIN_HEIGHT - 30) planeY = WIN_HEIGHT - 30;

            int curDir = 0;
            if (moveRight && !moveLeft) curDir = 1;
            else if (moveLeft && !moveRight) curDir = -1;
            // 静止→移动 或 变向时触发完整一圈翻滚
            if (curDir != 0 && lastMoveDir != curDir)
                rollTarget += (curDir > 0 ? 2.0 * M_PI : -2.0 * M_PI);
            lastMoveDir = curDir;
            // 平滑动画到目标角度
            double diff = rollTarget - rollAngle;
            if (std::fabs(diff) > 0.005) {
                if (std::fabs(diff) < ROLL_SPEED) rollAngle = rollTarget;
                else rollAngle += (diff > 0 ? ROLL_SPEED : -ROLL_SPEED);
            }

            // 射击和冲击波（Boss入场/Phase2期间禁用）
            bool canShoot = true;  // 任何阶段都可以射击
            if (canShoot) {
                if (keys[SDL_SCANCODE_SPACE] && fireCooldown == 0) {
                    addBullet();
                    fireCooldown = currentFireDelay();
                }
                if (fireCooldown > 0) fireCooldown--;

                if (phase == PHASE_BOSS_FIGHT) {
                    if (spawnTimer <= 0) {
                        spawnAlienFromBoss();
                        spawnTimer = currentSpawnInterval() + (rand() % 20);
                    }
                    spawnTimer--;
                } else if (phase == PHASE_PLAY) {
                    if (spawnTimer <= 0) {
                        spawnAlien();
                        spawnTimer = currentSpawnInterval() + (rand() % 30);
                    }
                    spawnTimer--;
                }

                if (score >= 30 && (phase == PHASE_PLAY || phase == PHASE_BOSS_FIGHT)) {
                    int curLv = score / 30;
                    if (curLv > lastShockwaveLevel) {
                        lastShockwaveLevel = curLv;
                        shockwavePending = 1;
                        shockwaveTimer = 0;
                        if (curLv < 6)
                            spawnFloatingText((float)planeX, (float)(planeY - 30), "LEVEL UP!");
                    }
                    if (shockwavePending) {
                        shockwavePending = 0;
                        spawnShockwave();
                        shockwaveTimer = 0;
                    } else {
                        shockwaveTimer++;
                        if (shockwaveTimer >= currentShockwaveInterval()) {
                            shockwaveTimer = 0;
                            spawnShockwave();
                        }
                    }
                }
            }

            updateBullets();
            if (phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2) updateAliens();
            updateParticles();
            updateShockwaves();
            updateFloatingTexts();

            // ======== Boss ∞字移动 ========
            if (phase == PHASE_BOSS_FIGHT || phase == PHASE_BOSS_PHASE2) {
                bossMoveTime += 0.025;
                bossX = CENTER_X + std::sin(bossMoveTime) * 140.0;
                boss.y = 90.0 + std::sin(bossMoveTime * 2.0) * std::cos(bossMoveTime) * 25.0;
                if (bossFlashTimer > 0) bossFlashTimer--;
            }
            // Boss治疗红波（仅Boss战中，每5秒释放一次）
            if (phase == PHASE_BOSS_FIGHT && boss.active && healWavesEnabled) {
                healWaveTimer++;
                if (healWaveTimer >= 420) {
                    healWaveTimer = 0;
                    HealWave hw; hw.radius = 15; hw.id = nextHealWaveID++; hw.active = true;
                    healWaves.push_back(hw);
                }
                // 更新治疗波
                for (auto& hw : healWaves) {
                    if (!hw.active) continue;
                    hw.radius += 4.0;
                    if (hw.radius > 700) hw.active = false;
                }
                // 治疗波碰撞外星飞船（穿过所有飞船，每只+20HP）
                for (auto& hw : healWaves) {
                    if (!hw.active) continue;
                    for (auto& a : aliens) {
                        if (!a.active || a.entering || a.beingAbsorbed) continue;
                        if (a.lastHealHit == hw.id) continue;
                        double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                        double dist = std::sqrt((ax - bossX) * (ax - bossX) + (a.y - boss.y) * (a.y - boss.y));
                        if (std::abs(dist - hw.radius) < 25.0) {
                            a.lastHealHit = hw.id;
                            sndBossHeal();
                            boss.bonusHp += 30;
                            int barEndX = CENTER_X - 200 + (int)((double)(boss.hp + boss.bonusHp) / boss.maxHp * 400);
                            if (barEndX > WIN_WIDTH) barEndX = WIN_WIDTH;
                            for (int i = 0; i < 10; ++i) {
                                Particle p;
                                p.x = ax + (rand() % 10 - 5);
                                p.y = a.y + (rand() % 10 - 5);
                                p.vx = (barEndX - p.x) / 60.0 + (rand() % 10 - 5) / 10.0;
                                p.vy = (16 - p.y) / 30.0 + (rand() % 10 - 5) / 10.0;
                                p.life = 30 + rand() % 15;
                                p.active = true;
                                p.whiteParticle = false;
                                p.greenParticle = false;
                                p.redParticle = true;
                                particles.push_back(p);
                            }
                        }
                    }
                }
                healWaves.erase(std::remove_if(healWaves.begin(), healWaves.end(),
                    [](const HealWave& hw){ return !hw.active; }), healWaves.end());
            }
            // 50%血量触发Phase2
            int totalBossHP = boss.hp + boss.bonusHp;
            if (phase == PHASE_BOSS_FIGHT && !bossPhase2Triggered && boss.active &&
                totalBossHP <= (boss.maxHp + boss.bonusHp) / 2) {
                bossPhase2Triggered = true;
                phase = PHASE_BOSS_PHASE2;
                shakeTimer = 120;  // 2秒震动
                absorbTimer = 0;
                absorbIndex = 0;
                for (auto& a : aliens) { a.invincibleFrames = -1; }
            }
            // Phase2处理（类似Intro）
            if (phase == PHASE_BOSS_PHASE2) {
                if (shakeTimer > 0) {
                    shakeTimer--;
                    shakeX = (rand() % 12) - 6;
                    shakeY = (rand() % 12) - 6;
                    if (shakeTimer % 8 == 0) sndShake();
                } else { shakeX = shakeY = 0; }
                // 外星飞船减速+入场
                for (auto& a : aliens) {
                    if (!a.active) continue;
                    if (a.entering) {
                        a.enterFrame++;
                        double raw = (double)a.enterFrame / a.enterDuration;
                        double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                        if (raw >= 1.0) { eased = 1.0; a.entering = false; a.invincibleFrames = 3; }
                        if (a.enterFromBoss) {
                            a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                            a.t = a.startT + (a.targetT - a.startT) * eased;
                        } else if (a.enterFromTop) {
                            a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                        } else {
                            a.t = a.startT + (a.targetT - a.startT) * eased;
                        }
                    } else {
                        double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                        double sf = (depthBelow < 0) ? 0.08 : 0.08 + 0.92 * depthBelow;
                        a.y += alienBaseSpeed * sf * 0.2;
                    }
                    // 基地碰撞检测：无敌飞船撞基地即爆炸
                    {
                        double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                        double dx = (ax - CENTER_X) / (WIN_WIDTH / 2.0 - 15.0);
                        double dy = (WIN_HEIGHT - a.y) / 75.0;
                        if (dx*dx + dy*dy < 1.0) {
                            spawnExplosion(ax, a.y, 35);
                            a.active = false;
                        }
                    }
                }
                // 无活跃飞船则跳过吸收，直接进入Boss战
                bool anyAlive = false;
                for (auto& aa : aliens) if (aa.active) { anyAlive = true; break; }
                if (!anyAlive && absorbTimer >= 0) {
                    for (auto& aa : aliens) aa.invincibleFrames = 0;
                    postAbsorbTimer = 30;
                    absorbTimer = -1;
                    healWavesEnabled = true;
                    absorbState = 0;
                }
                // 震动结束后逐只吸收（状态机）
                if (shakeTimer == 0 && absorbTimer >= 0) {
                    if (absorbCooldown > 0) absorbCooldown--;
                    if (absorbState == 0) {
                        // 空闲：找下一个未吸收的飞船
                        int idx = -1;
                        for (int i = 0; i < (int)aliens.size(); ++i) {
                            if (aliens[i].active && !aliens[i].beingAbsorbed) { idx = i; break; }
                        }
                        if (idx >= 0) {
                            Alien& a = aliens[idx];
                            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                            Bullet beam;
                            beam.x = bossX; beam.y = boss.y;
                            double bdx = ax - bossX, bdy = a.y - boss.y;
                            double blen = std::sqrt(bdx*bdx + bdy*bdy);
                            if (blen > 1.0) { beam.dx = bdx / blen; beam.dy = bdy / blen; }
                            else { beam.dx = 0; beam.dy = -1; }
                            beam.startX = bossX; beam.startY = boss.y;
                            beam.active = true; beam.canDamage = false;
                            beam.blueBeam = true; beam.beamTargetIndex = idx;
                            bullets.push_back(beam);
                            absorbTargetIdx = idx;
                            absorbState = 1;
                        } else if (absorbCooldown <= 0) {
                            // 全部吸收完毕 → 震动后进入Boss战
                            for (auto& aa : aliens) {
                                if (aa.active) { boss.bonusHp += 50; aa.active = false; }
                            }
                            for (auto& aa : aliens) aa.invincibleFrames = 0;
                            postAbsorbTimer = 30;
                            absorbTimer = -1;
                            healWavesEnabled = true;
                            absorbState = 0;
                        }
                    }
                    // 状态1安全网：光束丢失目标 → 冷却
                    if (absorbState == 1) {
                        bool beamAlive = false;
                        for (auto& bb : bullets)
                            if (bb.active && bb.blueBeam && bb.beamTargetIndex == absorbTargetIdx)
                                { beamAlive = true; break; }
                        if (!beamAlive) { absorbState = 3; absorbCooldown = 18; }
                    }
                    // 状态2→3：螺旋完成，进入冷却
                    if (absorbState == 2 && absorbTargetIdx >= 0) {
                        if (!aliens[absorbTargetIdx].active || !aliens[absorbTargetIdx].beingAbsorbed) {
                            absorbState = 3; absorbCooldown = 18;
                        }
                    }
                    if (absorbState == 3 && absorbCooldown <= 0) absorbState = 0;
                }
                // 吸收完毕震动后进入Boss战
                if (postAbsorbTimer > 0) {
                    postAbsorbTimer--;
                    shakeX = (rand() % 10) - 5; shakeY = (rand() % 10) - 5;
                    if (postAbsorbTimer % 6 == 0) sndShake();
                    if (postAbsorbTimer <= 0) { shakeX = shakeY = 0; phase = PHASE_BOSS_FIGHT; }
                }
                // 吸收动画更新
                for (auto& a : aliens) {
                    if (!a.active || !a.beingAbsorbed) continue;
                    a.absorbFrame++;
                    double raw = (double)a.absorbFrame / a.absorbDuration;
                    if (raw >= 1.0) {
                        raw = 1.0; a.active = false;
                        boss.bonusHp += 50;
                        absorbCooldown = 18;  // 0.3秒冷却 bossFlashTimer = 8;
                        int barEndX = CENTER_X - 200 + (int)((double)(boss.hp + boss.bonusHp) / boss.maxHp * 400);
                        if (barEndX > WIN_WIDTH) barEndX = WIN_WIDTH;
                        for (int i = 0; i < 12; ++i) {
                            Particle p;
                            p.x = bossX + (rand() % 30 - 15);
                            p.y = boss.y + (rand() % 20 - 10);
                            p.vx = (barEndX - p.x) / 60.0 + (rand() % 10 - 5) / 10.0;
                            p.vy = (16 - p.y) / 60.0 + (rand() % 10 - 5) / 10.0;
                            p.life = 60 + rand() % 20;
                            p.active = true;
                            p.whiteParticle = false;
                            p.greenParticle = false;
                            p.redParticle = true;
                            particles.push_back(p);
                        }
                    } else {
                        double eased = 1.0 - std::pow(1.0 - raw, 2.0);
                        double startDist = std::sqrt(
                            (a.absorbStartX - bossX) * (a.absorbStartX - bossX) +
                            (a.absorbStartY - boss.y) * (a.absorbStartY - boss.y));
                        double startAngle = std::atan2(a.absorbStartY - boss.y,
                                                        a.absorbStartX - bossX);
                        double angle = startAngle + raw * M_PI * 5.0;
                        double radius = startDist * (1.0 - eased);
                        a.y = boss.y + std::sin(angle) * radius;
                        double tx = bossX + std::cos(angle) * radius;
                        a.t = (tx - perspLeft(a.y)) / perspWidth(a.y);
                    }
                }
            }

            // ======== Boss 入场/震动/吸收 ========
            if (phase == PHASE_BOSS_INTRO) {
                // Boss 入场动画
                if (boss.entering) {
                    boss.enterFrame++;
                    double raw = (double)boss.enterFrame / boss.enterDuration;
                    if (raw > 1.0) raw = 1.0;
                    double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                    boss.y = -300.0 + (90.0 + 300.0) * eased;
                    if (raw >= 1.0) {
                        boss.entering = false;
                        shakeTimer = 60;
                        sndBossEntrance();
                    }
                }
                // 画面震动
                if (shakeTimer > 0) {
                    shakeTimer--;
                    shakeX = (rand() % 10) - 5;
                    shakeY = (rand() % 10) - 5;
                    if (shakeTimer % 8 == 0) sndShake();
                } else { shakeX = shakeY = 0; }
                // 外星飞船：入场动画 + 20%速度
                for (auto& a : aliens) {
                    if (!a.active) continue;
                    if (a.entering) {
                        a.enterFrame++;
                        double raw = (double)a.enterFrame / a.enterDuration;
                        double eased = 1.0 - std::pow(1.0 - raw, 3.0);
                        if (raw >= 1.0) { eased = 1.0; a.entering = false; a.invincibleFrames = 3; }
                        if (a.enterFromTop)
                            a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                        else
                            a.t = a.startT + (a.targetT - a.startT) * eased;
                    } else {
                        double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                        double sf = (depthBelow < 0) ? 0.08 : 0.08 + 0.92 * depthBelow;
                        a.y += alienBaseSpeed * sf * 0.2;
                    }
                    // 基地碰撞检测：无敌飞船撞基地即爆炸
                    {
                        double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                        double dx = (ax - CENTER_X) / (WIN_WIDTH / 2.0 - 15.0);
                        double dy = (WIN_HEIGHT - a.y) / 75.0;
                        if (dx*dx + dy*dy < 1.0) {
                            spawnExplosion(ax, a.y, 35);
                            a.active = false;
                        }
                    }
                }
                // 无活跃飞船则跳过吸收，直接进入Boss战
                { bool anyAlive = false;
                for (auto& aa : aliens) if (aa.active) { anyAlive = true; break; }
                if (!anyAlive && absorbTimer >= 0) {
                    for (auto& aa : aliens) aa.invincibleFrames = 0;
                    postAbsorbTimer = 30;
                    absorbTimer = -1;
                    absorbState = 0;
                } }
                // 吸收外星飞船（震动结束后逐只吸收-状态机）
                if (!boss.entering && shakeTimer == 0 && absorbTimer >= 0) {
                    if (absorbCooldown > 0) absorbCooldown--;
                    if (absorbState == 0) {
                        int idx = -1;
                        for (int i = 0; i < (int)aliens.size(); ++i) {
                            if (aliens[i].active && !aliens[i].beingAbsorbed) { idx = i; break; }
                        }
                        if (idx >= 0) {
                            Alien& a = aliens[idx];
                            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                            Bullet beam;
                            beam.x = bossX; beam.y = boss.y;
                            double bdx = ax - bossX, bdy = a.y - boss.y;
                            double blen = std::sqrt(bdx*bdx + bdy*bdy);
                            if (blen > 1.0) { beam.dx = bdx / blen; beam.dy = bdy / blen; }
                            else { beam.dx = 0; beam.dy = -1; }
                            beam.startX = bossX; beam.startY = boss.y;
                            beam.active = true; beam.canDamage = false;
                            beam.blueBeam = true; beam.beamTargetIndex = idx;
                            bullets.push_back(beam);
                            absorbTargetIdx = idx;
                            absorbState = 1;
                        } else if (absorbCooldown <= 0) {
                            for (auto& aa : aliens) {
                                if (aa.active) { boss.bonusHp += 50; aa.active = false; }
                            }
                            for (auto& aa : aliens) aa.invincibleFrames = 0;
                            postAbsorbTimer = 30;
                            absorbTimer = -1;
                            absorbState = 0;
                        }
                    }
                    // 状态1安全网：光束丢失目标 → 冷却
                    if (absorbState == 1) {
                        bool beamAlive = false;
                        for (auto& bb : bullets)
                            if (bb.active && bb.blueBeam && bb.beamTargetIndex == absorbTargetIdx)
                                { beamAlive = true; break; }
                        if (!beamAlive) { absorbState = 3; absorbCooldown = 18; }
                    }
                    if (absorbState == 2 && absorbTargetIdx >= 0) {
                        if (!aliens[absorbTargetIdx].active || !aliens[absorbTargetIdx].beingAbsorbed) {
                            absorbState = 3; absorbCooldown = 18;
                        }
                    }
                    if (absorbState == 3 && absorbCooldown <= 0) absorbState = 0;
                }
                // 吸收完毕震动后进入Boss战
                if (postAbsorbTimer > 0) {
                    postAbsorbTimer--;
                    shakeX = (rand() % 10) - 5; shakeY = (rand() % 10) - 5;
                    if (postAbsorbTimer % 6 == 0) sndShake();
                    if (postAbsorbTimer <= 0) { shakeX = shakeY = 0; phase = PHASE_BOSS_FIGHT; }
                }
                // 吸收动画更新
                for (auto& a : aliens) {
                    if (!a.active || !a.beingAbsorbed) continue;
                    a.absorbFrame++;
                    double raw = (double)a.absorbFrame / a.absorbDuration;
                    if (raw >= 1.0) {
                        raw = 1.0;
                        a.active = false;
                        boss.bonusHp += 50;
                        // 粒子向血条末端汇聚
                        int barEndX = CENTER_X - 200 + (int)((double)(boss.hp + boss.bonusHp) / boss.maxHp * 400);
                        if (barEndX > WIN_WIDTH) barEndX = WIN_WIDTH;
                        for (int i = 0; i < 12; ++i) {
                            Particle p;
                            p.x = bossX + (rand() % 30 - 15);
                            p.y = boss.y + (rand() % 20 - 10);
                            p.vx = (barEndX - p.x) / 60.0 + (rand() % 10 - 5) / 10.0;
                            p.vy = (16 - p.y) / 60.0 + (rand() % 10 - 5) / 10.0;
                            p.life = 60 + rand() % 20;
                            p.active = true;
                            p.whiteParticle = false;
                            p.greenParticle = false;
                            p.redParticle = true;
                            particles.push_back(p);
                        }
                    } else {
                        double eased = 1.0 - std::pow(1.0 - raw, 2.0);
                        double startDist = std::sqrt(
                            (a.absorbStartX - bossX) * (a.absorbStartX - bossX) +
                            (a.absorbStartY - boss.y) * (a.absorbStartY - boss.y));
                        double startAngle = std::atan2(a.absorbStartY - boss.y,
                                                        a.absorbStartX - bossX);
                        double angle = startAngle + raw * M_PI * 5.0;
                        double radius = startDist * (1.0 - eased);
                        a.y = boss.y + std::sin(angle) * radius;
                        double tx = bossX + std::cos(angle) * radius;
                        a.t = (tx - perspLeft(a.y)) / perspWidth(a.y);
                    }
                }
            }

            // 碰撞检测
            for (auto& b : bullets) {
                if (!b.active || !b.canDamage) continue;
                for (auto& a : aliens) {
                    if (!a.active) continue;
                    double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                    double dx = b.x - ax;
                    double dy = b.y - a.y;
                    double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
                    double alienScale = (depthBelow < 0) ? 0.17
                        : 0.17 + 0.83 * depthBelow;
                    if (alienScale < 0.14) alienScale = 0.14;
                    if (alienScale > 1.0) alienScale = 1.0;
                    double hitRadius = 22.0 * alienScale + 8.0;
                    if (dx*dx + dy*dy < hitRadius * hitRadius) {
                        b.active = false;
                        // 无敌中的飞船：消弹但不扣血
                        if (a.invincibleFrames != 0) {
                            spawnExplosion(b.x, b.y, 3);
                            break;
                        }
                        a.hp--;
                        sndHit();
                        if (a.hp <= 0) {
                            a.active = false;
                            spawnExplosion(ax, a.y, 22);
                            sndExplosionBig();
                            score++;
                        } else {
                            spawnExplosion(b.x, b.y, 4);
                        }
                        break;
                    }
                }
            }
            // 立即清除已命中子弹，防止继续飞行
            bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                [](const Bullet& b){ return !b.active; }), bullets.end());

            // 冲击波-外星飞船碰撞（每个冲击波对每艘飞船只命中一次）
            const double BASE_B = 75.0;
            for (auto& sw : shockwaves) {
                if (!sw.active) continue;
                double progress = (WIN_HEIGHT - sw.y) / WIN_HEIGHT;
                if (progress > 1.0) progress = 1.0;
                double swA = perspWidth(sw.y) / 2.0;
                double swB = BASE_B * swA / (perspWidth(WIN_HEIGHT) / 2.0);
                for (auto& a : aliens) {
                    if (!a.active) continue;
                    if (a.invincibleFrames != 0) continue;
                    if (a.lastHitBySW == sw.id) continue;  // 已被本冲击波命中过
                    double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                    double ddx = (ax - CENTER_X) / swA;
                    double ddy = (sw.y - a.y) / swB;
                    if (ddx*ddx + ddy*ddy < 1.0) {
                        a.lastHitBySW = sw.id;
                        sndShockwaveHit();
                        a.hp -= currentShockwaveDamage();
                        spawnExplosion(ax, a.y, 4);
                        if (a.hp <= 0) {
                            a.active = false;
                            spawnExplosion(ax, a.y, 22);
                            sndExplosionBig();
                            shockwaveKills++;
                            if (shockwaveKills >= 3) {
                                shockwaveKills -= 3;
                                score++;
                            }
                        }
                    }
                }
            }

            // ======== 蓝色光束命中检测 ========
            for (auto& b : bullets) {
                if (!b.active || !b.blueBeam) continue;
                if (b.beamTargetIndex < 0 || b.beamTargetIndex >= (int)aliens.size()) continue;
                Alien& a = aliens[b.beamTargetIndex];
                if (!a.active || a.beingAbsorbed) { b.active = false; continue; }
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                double dx = b.x - ax, dy = b.y - a.y;
                if (dx*dx + dy*dy < 28.0 * 28.0) {
                    b.active = false;
                    // 蓝色受击特效
                    for (int i = 0; i < 12; ++i) {
                        Particle p;
                        p.x = ax + (rand()%14-7); p.y = a.y + (rand()%14-7);
                        p.vx = (rand()%30-15)/6.0; p.vy = (rand()%30-15)/6.0;
                        p.life = 15+rand()%10;
                        p.active = true;
                        p.whiteParticle = true;
                        p.greenParticle = false; p.redParticle = false;
                        particles.push_back(p);
                    }
                    // 开始螺旋吸收
                    a.beingAbsorbed = true;
                    sndBossAbsorb();
                    a.absorbFrame = 0;
                    a.absorbDuration = 70;
                    a.absorbStartX = ax;
                    a.absorbStartY = a.y;
                    absorbState = 2;
                }
            }

            // ======== Boss受击检测 ========
            if (boss.active && phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2 && phase != PHASE_BOSS_DEFEAT) {
                double bossHitR = 55.0;
                // 子弹命中
                for (auto& b : bullets) {
                    if (!b.active || !b.canDamage) continue;
                    double dx = b.x - bossX, dy = b.y - boss.y;
                    if (dx*dx + dy*dy < bossHitR * bossHitR) {
                        b.active = false;
                        bossFlashTimer = 5;
                        sndBossHit();
                        spawnExplosion(b.x, b.y, 4);
                        // 优先扣bonusHp
                        if (boss.bonusHp > 0) { boss.bonusHp--; }
                        else { boss.hp--; }
                        if (boss.hp <= 0) { phase = PHASE_BOSS_DEFEAT; bossDefeatTimer = 0; }
                    }
                }
                // 冲击波命中
                for (auto& sw : shockwaves) {
                    if (!sw.active) continue;
                    if (boss.lastHitBySW == sw.id) continue;
                    double dx = bossX - CENTER_X, dy = boss.y - sw.y;
                    double swA = perspWidth(sw.y) / 2.0;
                    double swB = 75.0 * swA / (perspWidth(WIN_HEIGHT) / 2.0);
                    double ddx = dx / swA, ddy = dy / swB;
                    if (ddx*ddx + ddy*ddy < 1.0 || (std::abs(dx) < swA && std::abs(dy) < swB*0.8)) {
                        boss.lastHitBySW = sw.id;
                        bossFlashTimer = 8;
                        sndBossHit();
                        spawnExplosion(bossX, boss.y, 10);
                        int dmg = currentShockwaveDamage();
                        if (boss.bonusHp > 0) {
                            int d = dmg < boss.bonusHp ? dmg : boss.bonusHp;
                            boss.bonusHp -= d; dmg -= d;
                        }
                        if (dmg > 0) boss.hp -= dmg;
                        if (boss.hp <= 0) { phase = PHASE_BOSS_DEFEAT; bossDefeatTimer = 0; }
                    }
                }
            }

            bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                [](const Bullet& b){ return !b.active; }), bullets.end());
            aliens.erase(std::remove_if(aliens.begin(), aliens.end(),
                [](const Alien& a){ return !a.active; }), aliens.end());
            particles.erase(std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p){ return !p.active; }), particles.end());
            shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(),
                [](const Shockwave& sw){ return !sw.active; }), shockwaves.end());

        } else if (phase == PHASE_BOSS_DEFEAT) {
            // ======== Boss 战败多阶段动画 ========
            bossDefeatTimer++;
            updateParticles();
            updateFloatingTexts();

            // 阶段0 (0-300帧=5秒): Boss持续爆炸
            if (bossDefeatTimer < 300) {
                if (bossDefeatTimer % 6 == 0)
                    spawnExplosion(bossX + (rand()%90-45), boss.y + (rand()%55-27), 10);
                if (bossDefeatTimer % 20 == 0)
                    spawnExplosion(bossX + (rand()%120-60), boss.y + (rand()%70-35), 20);
            }
            // Boss最终大爆炸
            if (bossDefeatTimer == 300) {
                spawnExplosion(bossX, boss.y, 120);
                sndExplosionBig();
                boss.active = false;
                defeatAlienTimer = 0;
            }

            // 阶段1 (300帧后): 每秒引爆一个外星飞船
            if (bossDefeatTimer > 300 && boss.active == false) {
                defeatAlienTimer++;
                if (defeatAlienTimer >= 60) {
                    defeatAlienTimer = 0;
                    for (auto& a : aliens) {
                        if (a.active) {
                            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                            spawnExplosion(ax, a.y, 22);
                            sndExplosionBig();
                            a.active = false;
                            break;
                        }
                    }
                }
            }

            // 阶段2: 外星飞船全灭后60帧弹出黄色MISSION COMPLETE
            bool anyAlien = false;
            for (auto& aa : aliens) if (aa.active) anyAlien = true;
            if (!anyAlien && bossDefeatTimer > 300 && !missionCompleteShown) {
                defeatMCDelay++;
                if (defeatMCDelay >= 60) {
                    missionCompleteShown = true;
                    spawnFloatingText((float)planeX, (float)(planeY - 30),
                                      "MISSION COMPLETE!", 255, 255, 50);
                }
            }

            // 阶段3: MISSION COMPLETE后120帧 → 飞机非线性回中
            if (missionCompleteShown) {
                defeatReturnTimer++;
                if (defeatReturnTimer >= 120 && defeatReturnTimer < 200) {
                    double t = (defeatReturnTimer - 120) / 80.0;
                    double eased = t * t * (3.0 - 2.0 * t); // smoothstep
                    planeX = planeX + (int)((CENTER_X - planeX) * eased * 0.25);
                    if (std::abs(planeX - CENTER_X) < 2) planeX = CENTER_X;
                }
                // 阶段4: 到位后飞机快速前飞（无翻转）
                if (defeatReturnTimer == 200) {
                    rollAngle = 0; rollTarget = 0;
                }
                if (defeatReturnTimer >= 200) {
                    planeY -= 7;
                }
                // 阶段5: 飞机飞出后烟花
                if (planeY < -50) {
                    defeatFWTimer++;
                    if (defeatFWTimer % 8 == 0) {
                        for (int k = 0; k < 12; ++k) {
                            Particle p;
                            double angle = (rand() % 360) * M_PI / 180.0;
                            double speed = 2.5 + (rand() % 500) / 100.0;
                            p.x = CENTER_X + (rand() % 240 - 120);
                            p.y = WIN_HEIGHT - (rand() % 40);
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
                    // 烟花2秒后结束画面
                    if (defeatFWTimer >= 120) {
                        missionComplete = true;
                        defeatFadeTimer++;
                    }
                }
            }

            if (missionComplete) {
                if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE]) {
                    resetGame(); atStartScreen = true;
                }
            }
            particles.erase(std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p){ return !p.active; }), particles.end());
            shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(),
                [](const Shockwave& sw){ return !sw.active; }), shockwaves.end());

        } else if (paused) {
            // ======== 倒计时进行中 ========
            if (countdown >= 0) {
                countdownFrame++;
                updateParticles();
                particles.erase(std::remove_if(particles.begin(), particles.end(),
                    [](const Particle& p){ return !p.active; }), particles.end());
                if (countdownFrame >= 45) {
                    countdownFrame = 0;
                    // 数字碎裂消散（基于字体位图的粒子）
                    spawnDigitShatter('0' + countdown, 18);
                    countdown--;
                    if (countdown <= 0) {
                        paused = false;
                        countdown = -1;
                        if (score >= 30) shockwavePending = 1;
                    }
                }
            } else {
                // ======== 暂停菜单 ========
                static bool pUpWas = false, pDownWas = false, pEnterWas = false;
                bool upNow   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
                bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
                bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];

                if (upNow && !pUpWas)     pauseMenuSelection = (pauseMenuSelection - 1 + 5) % 5;
                if (downNow && !pDownWas) pauseMenuSelection = (pauseMenuSelection + 1) % 5;
                if (enterNow && !pEnterWas) {
                    if (pauseMenuSelection == 0)      { countdown = 3; countdownFrame = 0; }
                    else if (pauseMenuSelection == 1) { resetGame(); paused = false; }
                    else if (pauseMenuSelection == 2) { paused = false; atOptionScreen = true; optionFromPause = true; }
                    else if (pauseMenuSelection == 3) { resetGame(); atStartScreen = true; paused = false; }
                    else                              running = false;
                }
                pUpWas = upNow;
                pDownWas = downNow;
                pEnterWas = enterNow;
            }

        } else {
            // ======== 游戏结束菜单 ========
            static bool upWasDown = false, downWasDown = false, enterWasDown = false;
            bool upNow2   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
            bool downNow2 = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
            bool enterNow2 = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE];

            if (upNow2 && !upWasDown)   menuSelection = (menuSelection - 1 + 2) % 2;
            if (downNow2 && !downWasDown) menuSelection = (menuSelection + 1) % 2;
            if (enterNow2 && !enterWasDown) {
                if (menuSelection == 0) resetGame();
                else running = false;
            }
            upWasDown = upNow2;
            downWasDown = downNow2;
            enterWasDown = enterNow2;

            updateParticles();
            particles.erase(std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p){ return !p.active; }), particles.end());
        }

        bool useShake = (shakeTimer > 0);
        SDL_Texture* shakeTex = nullptr;
        if (useShake) {
            shakeTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);
            SDL_SetRenderTarget(renderer, shakeTex);
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (!gameOver) {
            drawBackground();
            drawBase();
            for (const auto& sw : shockwaves) if (sw.active) drawShockwave(sw);
            for (const auto& p : particles) if (p.active) drawParticle(p);
            for (const auto& a : aliens)  if (a.active) drawAlien(a);
            for (const auto& b : bullets) if (b.active) drawBullet(b);
            drawPlane();
            drawFloatingTexts();
            if (aimAssistOn) drawAimAssist();
            if (boss.active) {
                if (phase == PHASE_BOSS_PHASE2) drawCircularShockwave();
                // 治疗红波（地面圆形在斜上方视角的透视投影）
                for (const auto& hw : healWaves) {
                    if (!hw.active) continue;
                    SDL_SetRenderDrawColor(renderer, 255, 60, 60, 80);
                    const int SEG = 60;
                    SDL_Point prev;
                    for (int i = 0; i <= SEG; ++i) {
                        double angle = 2.0 * M_PI * i / SEG;
                        double sy = boss.y + std::sin(angle) * hw.radius;
                        double rx = hw.radius * perspWidth(sy) / perspWidth(boss.y);
                        int sx = (int)(bossX + std::cos(angle) * rx);
                        if (i > 0)
                            SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, (int)sy);
                        prev = {sx, (int)sy};
                    }
                }
                drawBossBody();
                drawBossHP();
            }
            drawScoreHUD();
            if (paused && countdown >= 0) drawCountdown();
            else if (paused) drawPauseMenu();
            // Boss战败后平滑渐入结束画面
            if (missionComplete) {
                int fadeAlpha = defeatFadeTimer * 3;
                if (fadeAlpha > 220) fadeAlpha = 220;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, (unsigned char)fadeAlpha);
                SDL_Rect dr = {0, 0, WIN_WIDTH, WIN_HEIGHT};
                SDL_RenderFillRect(renderer, &dr);
                if (fadeAlpha > 160) {
                    // 黄色文字
                    SDL_SetRenderDrawColor(renderer, 255, 255, 50, 255);
                    int cx1 = CENTER_X - 204;
                    for (const char* p = "MISSION COMPLETE"; *p; ++p) {
                        if (*p != ' ') drawChar(*p, cx1, 220, 4);
                        cx1 += 6 * 4;
                    }
                    int cx2 = CENTER_X - 48;
                    for (const char* p = "EXIT"; *p; ++p) {
                        if (*p != ' ') drawChar(*p, cx2, 340, 4);
                        cx2 += 6 * 4;
                    }
                    int ex = CENTER_X - 70, ey = 350;
                    SDL_RenderDrawLine(renderer, ex, ey, ex+12, ey+7);
                    SDL_RenderDrawLine(renderer, ex, ey, ex+12, ey-7);
                    SDL_RenderDrawLine(renderer, ex+12, ey-7, ex+12, ey+7);
                    SDL_RenderDrawLine(renderer, CENTER_X - 48, 374, CENTER_X + 48, 374);
                    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                    drawString("ENTER:return to title", CENTER_X - 150, 490, 2);
                }
            }
        } else {
            drawBackground();
            drawBase();
            for (const auto& p : particles) if (p.active) drawParticle(p);
            drawGameOver();
        }

        if (useShake) {
            SDL_SetRenderTarget(renderer, NULL);
            SDL_Rect dst = {shakeX, shakeY, WIN_WIDTH, WIN_HEIGHT};
            SDL_RenderCopy(renderer, shakeTex, NULL, &dst);
            SDL_DestroyTexture(shakeTex);
        }
        SDL_RenderPresent(renderer);
        Uint32 now = SDL_GetTicks();
        Uint32 elapsed = now - lastTime;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        lastTime = SDL_GetTicks();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_Quit();
    return 0;
}

// ============== 游戏重置 ==============
void resetGame() {
    score = 0;
    gameOver = false;
    atStartScreen = true;
    atTestSelect = false;
    atOptionScreen = false;
    menuSelection = 0;
    startMenuSelection = 0;
    testScoreSelection = 0;
    difficultyTimer = 0;
    lastShockwaveLevel = 0;
    shockwavePending = 0;
    spawnTimer = 0;
    alienBaseSpeed = 1.75;
    baseHP = BASE_MAX_HP;
    planeX = CENTER_X;
    planeY = WIN_HEIGHT - 80;
    rollAngle = 0.0;
    rollTarget = 0.0;
    lastMoveDir = 0;
    fireCooldown = 0;
    bullets.clear();
    aliens.clear();
    particles.clear();
    shockwaves.clear();
    shockwaveTimer = 0;
    shockwaveKills = 0;
    phase = PHASE_PLAY;
    boss.active = false;
    boss.entering = false;
    bossX = CENTER_X;
    bossMoveTime = 0;
    bossFlashTimer = 0;
    bossPhase2Triggered = false;
    bossDefeatTimer = 0;
    defeatAlienTimer = 0;
    defeatReturnTimer = 0;
    defeatFWTimer = 0;
    defeatMCDelay = 0;
    defeatFadeTimer = 0;
    missionCompleteShown = false;
    missionComplete = false;
    healWaves.clear();
    healWaveTimer = 0;
    healWavesEnabled = false;
    shakeTimer = 0;
    absorbTimer = 0;
    absorbIndex = 0;
    absorbCooldown = 0;
    postAbsorbTimer = 0;
}

// ============== 字体 ==============
void drawChar(char c, int x, int y, int scale) {
    FontChar& fc = FONT[(int)c];
    for (int row = 0; row < 7; ++row) {
        unsigned char bits = fc.rows[row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1 << (4 - col))) {
                SDL_Rect r = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }
}
void drawString(const char* str, int x, int y, int scale) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int cx = x;
    for (const char* p = str; *p; ++p) {
        if (*p == ' ') { cx += 6 * scale; continue; }
        drawChar(*p, cx, y, scale);
        cx += 6 * scale;
    }
}
void drawFloatingTexts() {
    for (auto& ft : floatingTexts) {
        float t = (float)ft.life / ft.totalLife;
        if (t < 0.05f) continue;
        int r = (int)(ft.r * t);
        int g = (int)(ft.g * t);
        int b_ = (int)(ft.b * t);
        SDL_SetRenderDrawColor(renderer, r, g, b_, 255);
        int cx = (int)(ft.x - strlen(ft.text) * 6 * 3 / 2);
        for (const char* p = ft.text; *p; ++p) {
            if (*p != ' ') drawChar(*p, cx, (int)ft.y, 3);
            cx += 6 * 3;
        }
    }
}

void drawScoreHUD() {
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE %d", score);
    drawString(buf, WIN_WIDTH - 160, 10, 2);
    for (int i = 0; i < BASE_MAX_HP; ++i) {
        int hx = WIN_WIDTH - 20 - i * 14;
        SDL_SetRenderDrawColor(renderer, (i < baseHP) ? 255 : 70, 20, 20, 255);
        drawChar('*', hx, 28, 2);
    }
    // 绿色能量条（得分进度 0→30，满30冲击波升级，始终显示）
    {
        const int EBAR_X = WIN_WIDTH - 20 - (BASE_MAX_HP - 1) * 14;
        const int EBAR_W = BASE_MAX_HP * 14;
        const int EBAR_Y = 46;
        const int EBAR_H = 6;
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_Rect ebg = {EBAR_X, EBAR_Y, EBAR_W, EBAR_H};
        SDL_RenderFillRect(renderer, &ebg);
        int lev = score / 30;
        int fillW;
        if (lev >= 6) {
            fillW = EBAR_W;  // 180分后常绿满条
        } else {
            fillW = (score % 30) * EBAR_W / 30;
        }
        if (fillW > 0) {
            int green = (lev >= 6) ? 255 : (lev >= 1 ? 180 : 100);
            SDL_SetRenderDrawColor(renderer, 30, green, 50, 255);
            SDL_Rect er = {EBAR_X, EBAR_Y, fillW, EBAR_H};
            SDL_RenderFillRect(renderer, &er);
        }
    }
}
void drawGameOver() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    for (int yy = 0; yy < WIN_HEIGHT; yy += 3) {
        SDL_Rect r = {0, yy, WIN_WIDTH, 2};
        SDL_RenderFillRect(renderer, &r);
    }
    drawString("GAME OVER", CENTER_X - 118, 140, 4);
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE %d", score);
    int scoreW = (int)strlen(buf) * 6 * 4;
    drawString(buf, CENTER_X - scoreW/2, 210, 4);

    const int MENU_Y0 = 340;
    const char* items[2] = {"PLAY AGAIN", "EXIT"};
    for (int i = 0; i < 2; ++i) {
        int itemW = (int)strlen(items[i]) * 6 * 3;
        int itemX = CENTER_X - itemW / 2;
        int itemY = MENU_Y0 + i * 60;
        drawString(items[i], itemX, itemY, 3);
        if (i == menuSelection) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = itemX - 20, ay = itemY + 10;
            SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay+5);
            SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay-5);
            SDL_RenderDrawLine(renderer, ax+8, ay-5, ax+8, ay+5);
            SDL_RenderDrawLine(renderer, itemX, itemY + 24, itemX + itemW, itemY + 24);
        }
    }
    drawString("W/S:select  ENTER:confirm", CENTER_X - 150, 490, 2);
}

// ============== 开始界面 ==============
void drawStartScreen() {
    drawString("HUNTRESS", CENTER_X - 96, 70, 4);
    drawString("ECHOES FROM THE WELL", CENTER_X - 117, 120, 3);
    // 装饰线
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, CENTER_X - 200, 140, CENTER_X + 200, 140);
    SDL_RenderDrawLine(renderer, CENTER_X - 200, 142, CENTER_X + 200, 142);

    const int MENU_Y0 = 240;
    const int GAP = 60;
    const char* items[4] = {"PLAY", "TEST", "OPTIONS", "EXIT"};
    for (int i = 0; i < 4; ++i) {
        int itemW = (int)strlen(items[i]) * 6 * 4;
        int itemX = CENTER_X - itemW / 2;
        int itemY = MENU_Y0 + i * GAP;
        drawString(items[i], itemX, itemY, 4);
        if (i == startMenuSelection) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = itemX - 30, ay = itemY + 14;
            SDL_RenderDrawLine(renderer, ax, ay, ax+12, ay+7);
            SDL_RenderDrawLine(renderer, ax, ay, ax+12, ay-7);
            SDL_RenderDrawLine(renderer, ax+12, ay-7, ax+12, ay+7);
            SDL_RenderDrawLine(renderer, itemX, itemY + 32, itemX + itemW, itemY + 32);
        }
    }

    drawString("W/S:select  ENTER:confirm", CENTER_X - 150, 490, 2);
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    drawString("Ver 1.0.0", 15, WIN_HEIGHT - 30, 2);
}

// ============== 测试模式-分数选择 ==============
void drawTestSelect() {
    drawString("SELECT SCORE", CENTER_X - 144, 50, 4);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, CENTER_X - 180, 90, CENTER_X + 180, 90);

    const char* testLabels[9] = {"30", "60", "90", "120", "150", "180", "200 BOSS", "BOSS PH.2", "BOSS 1HP"};
    const int MENU_Y0 = 130;
    const int GAP = 48;
    for (int i = 0; i < 9; ++i) {
        int itemW = (int)strlen(testLabels[i]) * 6 * 3;
        int itemX = CENTER_X - itemW / 2;
        int itemY = MENU_Y0 + i * GAP;
        drawString(testLabels[i], itemX, itemY, 3);
        if (i == testScoreSelection) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = itemX - 24, ay = itemY + 10;
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay+5);
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay-5);
            SDL_RenderDrawLine(renderer, ax+10, ay-5, ax+10, ay+5);
            SDL_RenderDrawLine(renderer, itemX, itemY + 24, itemX + itemW, itemY + 24);
        }
    }
}

// ============== 选项界面 ==============
void drawOptionScreen() {
    drawString("OPTIONS", CENTER_X - 84, 40, 4);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, CENTER_X - 180, 78, CENTER_X + 180, 78);

    const char* labels[2] = {"AIM ASSIST", "SOUND"};
    const int Y0 = 130, GAP = 70;

    for (int i = 0; i < 2; ++i) {
        int ly = Y0 + i * GAP;
        int lx = CENTER_X - 120;
        if (i == optionCursor) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = lx - 22, ay = ly + 7;
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay+5);
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay-5);
            SDL_RenderDrawLine(renderer, ax+10, ay-5, ax+10, ay+5);
        }
        drawString(labels[i], lx, ly, 3);

        if (i == 0) {
            SDL_SetRenderDrawColor(renderer, aimAssistOn ? 100 : 200, aimAssistOn ? 255 : 60, 100, 255);
            SDL_Rect tg = {CENTER_X + 80, ly - 2, 56, 26};
            SDL_RenderFillRect(renderer, &tg);
            drawString(aimAssistOn ? "ON" : "OFF", CENTER_X + 88, ly + 4, 2);
        } else {
            drawString(">", CENTER_X + 80, ly, 3);
        }
        // 黄色下划线
        if (i == optionCursor) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int itemW = (int)strlen(labels[i]) * 6 * 3;
            SDL_RenderDrawLine(renderer, lx, ly + 24, lx + itemW, ly + 24);
        }
    }

    drawString("W/S:select  ENTER:confirm  ESC:back", CENTER_X - 216, 490, 2);
}

// ============== 声音子菜单 ==============
void drawSoundMenu(int cursor) {
    drawString("SOUND", CENTER_X - 60, 40, 4);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, CENTER_X - 180, 78, CENTER_X + 180, 78);

    const char* labels[6] = {"BGM VOL", "SFX VOL", "EQ LOW", "EQ MID", "EQ HIGH", "BACK"};
    const int Y0 = 110, GAP = 48;

    auto drawSlider = [](int x, int y, int w, int val, int lo, int hi, bool sym) {
        int range = hi - lo;
        int fillW = (val - lo) * w / range;
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect bg = {x, y, w, 12};
        SDL_RenderFillRect(renderer, &bg);
        if (sym) {
            int zeroX = x + w / 2;
            if (fillW >= w / 2) {
                SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
                SDL_Rect fg = {zeroX, y, fillW - w/2, 12};
                SDL_RenderFillRect(renderer, &fg);
            } else {
                SDL_SetRenderDrawColor(renderer, 220, 100, 100, 255);
                SDL_Rect fg = {zeroX + fillW - w/2, y, w/2 - fillW, 12};
                SDL_RenderFillRect(renderer, &fg);
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, zeroX, y, zeroX, y + 12);
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
            SDL_Rect fg = {x, y, fillW, 12};
            SDL_RenderFillRect(renderer, &fg);
        }
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_RenderDrawRect(renderer, &bg);
    };

    for (int i = 0; i < 6; ++i) {
        int ly = Y0 + i * GAP;
        int lx = CENTER_X - 120;
        if (i == cursor) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = lx - 22, ay = ly + 7;
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay+5);
            SDL_RenderDrawLine(renderer, ax, ay, ax+10, ay-5);
            SDL_RenderDrawLine(renderer, ax+10, ay-5, ax+10, ay+5);
        }
        drawString(labels[i], lx, ly, 3);
        // 黄色下划线
        if (i == cursor) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int itemW = (int)strlen(labels[i]) * 6 * 3;
            SDL_RenderDrawLine(renderer, lx, ly + 24, lx + itemW, ly + 24);
        }

        int sx = CENTER_X + 20, sw = 160;
        char vbuf[8];
        switch (i) {
            case 0: snprintf(vbuf, sizeof(vbuf), "%d", bgmVolume);
                    drawSlider(sx, ly+6, sw, bgmVolume, 1, 10, false); break;
            case 1: snprintf(vbuf, sizeof(vbuf), "%d", sfxVolume);
                    drawSlider(sx, ly+6, sw, sfxVolume, 1, 10, false); break;
            case 2: snprintf(vbuf, sizeof(vbuf), "%+d", eqLow);
                    drawSlider(sx, ly+6, sw, eqLow, -5, 5, true); break;
            case 3: snprintf(vbuf, sizeof(vbuf), "%+d", eqMid);
                    drawSlider(sx, ly+6, sw, eqMid, -5, 5, true); break;
            case 4: snprintf(vbuf, sizeof(vbuf), "%+d", eqHigh);
                    drawSlider(sx, ly+6, sw, eqHigh, -5, 5, true); break;
        }
        if (i < 5) drawString(vbuf, sx + sw + 10, ly, 2);
    }

    drawString("W/S:select  A/D:adjust  ENTER/ESC:back", CENTER_X - 232, 490, 2);
}

// ============== 暂停菜单 ==============
void drawPauseMenu() {
    // 半透明遮罩
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    for (int yy = 0; yy < WIN_HEIGHT; yy += 3) {
        SDL_Rect r = {0, yy, WIN_WIDTH, 2};
        SDL_RenderFillRect(renderer, &r);
    }
    // 标题
    drawString("PAUSED", CENTER_X - 72, 150, 3);

    const int MENU_Y0 = 210;
    const int GAP = 45;
    const char* items[5] = {"RESUME", "RESTART", "OPTIONS", "BACK TO MAIN MENU", "EXIT"};
    for (int i = 0; i < 5; ++i) {
        int itemW = (int)strlen(items[i]) * 6 * 3;
        int itemX = CENTER_X - itemW / 2;
        int itemY = MENU_Y0 + i * GAP;
        drawString(items[i], itemX, itemY, 3);
        if (i == pauseMenuSelection) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int ax = itemX - 20, ay = itemY + 10;
            SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay+5);
            SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay-5);
            SDL_RenderDrawLine(renderer, ax+8, ay-5, ax+8, ay+5);
            SDL_RenderDrawLine(renderer, itemX, itemY + 24, itemX + itemW, itemY + 24);
        }
    }
    drawString("W/S:select  ENTER:confirm  ESC:resume", CENTER_X - 228, 490, 2);
}

// ============== 倒计时画面 ==============
void drawCountdown() {
    // 暗色遮罩
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    for (int yy = 0; yy < WIN_HEIGHT; yy += 3) {
        SDL_Rect r = {0, yy, WIN_WIDTH, 2};
        SDL_RenderFillRect(renderer, &r);
    }

    double progress = countdownFrame / 45.0;
    if (progress > 1.0) progress = 1.0;
    // 缓出曲线：开头冲得快
    double eased = 1.0 - std::pow(1.0 - progress, 2.5);

    char digit = '0' + countdown;
    // 数字高速扩张：3→18
    int mainScale = 3 + (int)(eased * 15);

    // 3层光环：同中心、逐层放大、高对比透明度
    for (int i = 2; i >= 0; --i) {
        double t = (double)(i + 1) / 3.0;          // 0.33, 0.67, 1.0
        int gs = mainScale + (int)(t * eased * 28.0);
        int alpha = (int)(180.0 * (1.0 - t) * (1.0 - eased * 0.4));
        if (alpha < 8) continue;
        int gw = 5 * gs, gh = 7 * gs;
        int gx = CENTER_X - gw / 2, gy = WIN_HEIGHT / 2 - gh / 2;
        SDL_SetRenderDrawColor(renderer, (unsigned char)alpha, (unsigned char)alpha, (unsigned char)alpha, 255);
        drawChar(digit, gx, gy, gs);
    }

    // 主体白色数字
    int dw = 5 * mainScale, dh = 7 * mainScale;
    int dx = CENTER_X - dw / 2, dy = WIN_HEIGHT / 2 - dh / 2;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawChar(digit, dx, dy, mainScale);
}

// ============== 数字碎裂粒子（基于字体位图，白色大碎片）==============
void spawnDigitShatter(char digit, int digitScale) {
    FontChar& fc = FONT[(int)digit];
    int dw = 5 * digitScale, dh = 7 * digitScale;
    int dx = CENTER_X - dw / 2, dy = WIN_HEIGHT / 2 - dh / 2;

    for (int row = 0; row < 7; ++row) {
        unsigned char bits = fc.rows[row];
        for (int col = 0; col < 5; ++col) {
            if (!(bits & (1 << (4 - col)))) continue;
            double px = dx + col * digitScale + digitScale / 2.0;
            double py = dy + row * digitScale + digitScale / 2.0;
            // 每个笔画像素爆 4~6 个白色碎片
            int n = 4 + rand() % 3;
            for (int k = 0; k < n; ++k) {
                Particle p;
                p.x = px + (rand() % (digitScale + 1)) - digitScale / 2.0;
                p.y = py + (rand() % (digitScale + 1)) - digitScale / 2.0;
                double angle = (rand() % 360) * M_PI / 180.0;
                double speed = 2.0 + (rand() % 500) / 100.0;   // 2~7 px/frame
                p.vx = std::cos(angle) * speed;
                p.vy = std::sin(angle) * speed;
                p.life = 22 + rand() % 24;   // 更长寿命
                p.active = true;
                p.whiteParticle = true;
                particles.push_back(p);
            }
        }
    }
}

// ============== 背景：地平线处透视线跨50%宽 ==============
void drawStars() {
    for (const auto& s : stars) {
        float bright = 0.45f + 0.55f * std::fabs(std::sin(s.phase));
        int b = (int)(bright * 200);
        if (b < 60) b = 60;
        SDL_SetRenderDrawColor(renderer, b, b, b, 255);
        SDL_RenderDrawPoint(renderer, (int)s.x, (int)s.y);
    }
}

void drawBackground() {
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(renderer, 0, HORIZON_Y, WIN_WIDTH, HORIZON_Y);

    drawStars();

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    const int NUM_RADIATE = 17;
    for (int i = 0; i < NUM_RADIATE; ++i) {
        double t = (double)i / (NUM_RADIATE - 1);
        int bx = (int)(t * WIN_WIDTH);
        int hx = (int)(HORIZON_LEFT + t * (HORIZON_RIGHT - HORIZON_LEFT));
        SDL_RenderDrawLine(renderer, bx, WIN_HEIGHT, hx, HORIZON_Y);
    }

    SDL_SetRenderDrawColor(renderer, 45, 45, 45, 255);
    const int NUM_DEPTH = 16;
    for (int i = 0; i < NUM_DEPTH; ++i) {
        double ratio = (double)(i + 1) / (NUM_DEPTH + 1);
        double dy = HORIZON_Y + (WIN_HEIGHT - HORIZON_Y) * (1.0 - ratio) * (1.0 - ratio);
        SDL_RenderDrawLine(renderer, 0, (int)dy, WIN_WIDTH, (int)dy);
    }
}

// ============== 圆拱形基地 ==============
void drawBase() {
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
    SDL_RenderDrawLine(renderer, CENTER_X - (int)A, WIN_HEIGHT,
                       CENTER_X + (int)A, WIN_HEIGHT);
}

// ============== 飞机（机头对准消失点 + 翻滚动画）==============
void drawPlane() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // 机头方向：指向透视线消失点
    double tPlane = (planeX - perspLeft(planeY)) / perspWidth(planeY);
    if (tPlane < 0.0) tPlane = 0.0; if (tPlane > 1.0) tPlane = 1.0;
    double hx = perspLeft(HORIZON_Y) + tPlane * perspWidth(HORIZON_Y);
    double hdx = hx - planeX;
    double hdy = HORIZON_Y - planeY;
    // 飞机超过地平线后固定机头朝上，防止翻转
    if (hdy > 0) { hdx = 0; hdy = -1; }
    double hlen = std::sqrt(hdx*hdx + hdy*hdy);
    if (hlen < 0.01) { hdx = 0; hdy = -1; hlen = 1; }
    double hnx = hdx / hlen;            // 机头方向X
    double hny = hdy / hlen;            // 机头方向Y

    double c = std::cos(rollAngle), s = std::sin(rollAngle);

    // 仅机头旋转（机身用）
    auto hr = [&](int lx, int ly) -> SDL_Point {
        return {planeX + (int)(lx * (-hny) - ly * hnx),
                planeY + (int)(lx * hnx  + ly * (-hny))};
    };

    // 机头旋转 + 翻滚（机翼用）
    auto hrr = [&](int lx, int ly) -> SDL_Point {
        double rx = lx * c;
        double ry = ly + lx * s * 0.40;
        return {planeX + (int)(rx * (-hny) - ry * hnx),
                planeY + (int)(rx * hnx  + ry * (-hny))};
    };

    // 机身三角形（仅旋转）
    SDL_Point nose = hr(0, -12), leftB = hr(-10, 6), rightB = hr(10, 6);
    SDL_Point body[3] = {nose, leftB, rightB};
    SDL_RenderDrawLines(renderer, body, 3);
    SDL_RenderDrawLine(renderer, leftB.x, leftB.y, rightB.x, rightB.y);
    // 左翼（旋转+翻滚）
    SDL_Point lw1 = hrr(-6, 0), lw2 = hrr(-14, 4);
    SDL_RenderDrawLine(renderer, lw1.x, lw1.y, lw2.x, lw2.y);
    // 右翼（旋转+翻滚）
    SDL_Point rw1 = hrr(6, 0),  rw2 = hrr(14, 4);
    SDL_RenderDrawLine(renderer, rw1.x, rw1.y, rw2.x, rw2.y);
    // 尾翼（仅旋转）
    SDL_Point tail1 = hr(0, 6), tail2 = hr(0, 12);
    SDL_RenderDrawLine(renderer, tail1.x, tail1.y, tail2.x, tail2.y);
}

// ============== 子弹：固定飞行距离，方向随飞机上下移动而改变 ==============
void addBullet() {
    double tPlane = (planeX - perspLeft(planeY)) / perspWidth(planeY);
    if (tPlane < 0.0) tPlane = 0.0;
    if (tPlane > 1.0) tPlane = 1.0;
    double spread = (rand() % 40 - 20) / 1000.0;  // ±0.02 in t-space
    double tBullet = tPlane + spread;
    if (tBullet < 0.0) tBullet = 0.0;
    if (tBullet > 1.0) tBullet = 1.0;

    // 目标点：沿透视线，距发射点固定距离 BULLET_RANGE
    double ty = planeY - BULLET_RANGE;
    double tx = perspLeft(ty) + tBullet * perspWidth(ty);

    double dirX = tx - planeX;
    double dirY = ty - planeY;
    double len = std::sqrt(dirX * dirX + dirY * dirY);
    if (len < 0.001) return;
    Bullet b;
    b.x = (double)planeX;
    b.y = (double)planeY;
    b.startX = planeX;
    b.startY = planeY;
    b.dx = dirX / len;
    b.dy = dirY / len;
    b.active = true;
    b.canDamage = true;
    b.blueBeam = false;
    b.beamTargetIndex = -1;
    bullets.push_back(b);
    sndShoot();
}

void updateBullets() {
    for (auto& b : bullets) {
        if (!b.active) continue;
        if (b.blueBeam) {
            // 追踪目标飞船当前位置
            if (b.beamTargetIndex >= 0 && b.beamTargetIndex < (int)aliens.size()) {
                Alien& ta = aliens[b.beamTargetIndex];
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
        // 飞行超过85%射程后失去伤害
        if (remainRatio < 0.20) b.canDamage = false;
        // 速度随剩余距离衰减：近处快、远处慢
        double speed = currentBulletSpeed() * remainRatio * remainRatio;
        b.x += b.dx * speed;
        b.y += b.dy * speed;
        if (b.x < -20 || b.x > WIN_WIDTH + 20 || b.y < -30 || b.y > WIN_HEIGHT + 10)
            b.active = false;
    }
}

void drawBullet(const Bullet& b) {
    double dist = std::sqrt((b.x - b.startX) * (b.x - b.startX) +
                            (b.y - b.startY) * (b.y - b.startY));
    double remainRatio = 1.0 - dist / BULLET_RANGE;
    if (remainRatio < 0.0) remainRatio = 0.0;
    double bodyLen = 16.0 * remainRatio;
    if (bodyLen < 1.5) bodyLen = 1.5;

    // 失效子弹迅速变透明（从85%射程处开始渐隐）
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
    if (b.blueBeam) {
        SDL_SetRenderDrawColor(renderer, 60, 160, 255, (unsigned char)alpha);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 90, 90, (unsigned char)trailAlpha);
    }
    SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)trx, (int)try_);
    if (b.blueBeam)
        SDL_SetRenderDrawColor(renderer, 100, 200, 255, (unsigned char)alpha);
    else
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, (unsigned char)alpha);
    SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)hx, (int)hy);
}

// ============== 冲击波 ==============
void spawnShockwave() {
    Shockwave sw;
    sndShockwave();
    sw.y = WIN_HEIGHT;
    sw.id = nextShockwaveID++;
    sw.active = true;
    shockwaves.push_back(sw);
    // 基地释放能量特效：绿色粒子覆盖整个圆拱顶
    const double BA = WIN_WIDTH / 2.0 - 15.0;
    const double BB = 75.0;
    for (int i = 0; i < 40; ++i) {
        Particle p;
        // 在拱顶椭圆上随机取点
        double t = (rand() % 1000) / 1000.0;
        double sx = CENTER_X + BA * (2.0 * t - 1.0);
        double ratio = (sx - CENTER_X) / BA;
        if (ratio > 1.0) ratio = 1.0;
        if (ratio < -1.0) ratio = -1.0;
        double sy = WIN_HEIGHT - BB * std::sqrt(1.0 - ratio * ratio);
        p.x = sx + (rand() % 10 - 5);
        p.y = sy + (rand() % 10 - 5);
        // 沿拱顶法线方向向外散射
        double nx = (sx - CENTER_X) / BA;
        double ny = (sy - WIN_HEIGHT) / BB;
        double nlen = std::sqrt(nx*nx + ny*ny);
        if (nlen < 0.01) { nx = 0; ny = -1; }
        else { nx /= nlen; ny /= nlen; }
        double speed = 1.5 + (rand() % 350) / 100.0;
        p.vx = nx * speed + (rand() % 40 - 20) / 20.0;
        p.vy = ny * speed + (rand() % 40 - 20) / 20.0;
        p.life = 20 + rand() % 25;
        p.active = true;
        p.whiteParticle = false;
        p.greenParticle = true;
        particles.push_back(p);
    }
}

void updateShockwaves() {
    for (auto& sw : shockwaves) {
        if (!sw.active) continue;
        double progress = (WIN_HEIGHT - sw.y) / WIN_HEIGHT;
        if (progress > 1.0) progress = 1.0;
        double speed = 2.0 * (1.0 + progress * 5.0);
        sw.y -= speed;
        if (sw.y < -30) sw.active = false;
    }
}

void drawShockwave(const Shockwave& sw) {
    const double BASE_B = 75.0;
    double progress = (WIN_HEIGHT - sw.y) / WIN_HEIGHT;
    if (progress > 1.0) progress = 1.0;
    if (progress < 0.0) progress = 0.0;

    // 快速渐隐（三次方）：底部255 → 中段就很淡
    int alpha = (int)(255.0 * std::pow(1.0 - progress, 3.0));
    if (alpha < 8) alpha = 8;
    if (alpha > 255) alpha = 255;

    // 宽度沿透视线收敛：越往上越窄
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

// ============== Boss 系统 ==============
void triggerBoss() {
    phase = PHASE_BOSS_INTRO;
    boss.y = -300;
    boss.hp = 1000;
    boss.maxHp = 1000;
    boss.bonusHp = 0;
    boss.active = true;
    boss.lastHitBySW = -1;
    boss.entering = true;
    boss.enterFrame = 0;
    boss.enterDuration = 150;   // 2.5秒入场
    // 场上外星飞船变慢变无敌
    for (auto& a : aliens) {
        a.invincibleFrames = -1;
    }
    absorbTimer = 0;
    absorbIndex = 0;
    absorbCooldown = 0;
    postAbsorbTimer = 0;
}

void drawBossHP() {
    if (!boss.active) return;
    const int BAR_W = 400, BAR_H = 14;
    const int BAR_X = CENTER_X - BAR_W / 2, BAR_Y = 8;
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_Rect bg = {BAR_X, BAR_Y, BAR_W, BAR_H};
    SDL_RenderFillRect(renderer, &bg);

    // 本体血量按比例，bonusHp叠加到血条右侧
    int baseW = (int)((double)boss.hp / boss.maxHp * BAR_W);
    if (baseW > BAR_W) baseW = BAR_W;
    int bonusW = (int)((double)boss.bonusHp / boss.maxHp * BAR_W);
    int bonusStart = BAR_X + baseW;
    int totalW = baseW + bonusW;
    if (totalW < BAR_W) totalW = BAR_W;

    // 基础红色
    SDL_SetRenderDrawColor(renderer, 220, 30, 30, 255);
    SDL_Rect baseR = {BAR_X, BAR_Y, baseW, BAR_H};
    SDL_RenderFillRect(renderer, &baseR);
    // 额外浅红
    if (boss.bonusHp > 0 && bonusW > 0) {
        SDL_SetRenderDrawColor(renderer, 255, 140, 140, 255);
        SDL_Rect bonusR = {bonusStart, BAR_Y, bonusW, BAR_H};
        SDL_RenderFillRect(renderer, &bonusR);
    }
    // 边框：跟随总血量伸缩，不低于BAR_W
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_Rect border = {BAR_X-1, BAR_Y-1, totalW+2, BAR_H+2};
    SDL_RenderDrawRect(renderer, &border);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    drawChar('B', BAR_X - 40, BAR_Y + 1, 2);
    drawChar('O', BAR_X - 28, BAR_Y + 1, 2);
    drawChar('S', BAR_X - 16, BAR_Y + 1, 2);
    drawChar('S', BAR_X - 4, BAR_Y + 1, 2);
}

void drawBossBody() {
    if (!boss.active || boss.y < -100) return;
    double scale = 0.5 + 0.5 * ((boss.y + 100) / 200.0);  // 入场渐显
    if (scale > 1.0) scale = 1.0;
    int s = (int)(80 * scale);
    if (s < 20) s = 20;
    int x = (int)bossX, y = (int)boss.y;
    // 巨大菱形飞船（受击时高亮）
    if (bossFlashTimer > 0)
        SDL_SetRenderDrawColor(renderer, 255, 200, 200, 255);
    else
        SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
    SDL_Point pts[4] = {
        {x, y - s},
        {x + s * 2 / 3, y},
        {x, y + s / 2},
        {x - s * 2 / 3, y}
    };
    SDL_RenderDrawLines(renderer, pts, 4);
    SDL_RenderDrawLine(renderer, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
    // 中心横线和竖线
    SDL_RenderDrawLine(renderer, x - s*2/3, y, x + s*2/3, y);
    SDL_RenderDrawLine(renderer, x, y - s, x, y + s/2);
    // 外层护盾环
    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 100);
    int r = (int)(s * 0.9);
    for (int i = 0; i < 2; ++i) {
        int rr = r + i * 10;
        // 简易六边形环
        SDL_Point ring[6] = {
            {x, y - rr},
            {x + rr*3/4, y - rr/2},
            {x + rr*3/4, y + rr/2},
            {x, y + rr},
            {x - rr*3/4, y + rr/2},
            {x - rr*3/4, y - rr/2}
        };
        SDL_RenderDrawLines(renderer, ring, 6);
        SDL_RenderDrawLine(renderer, ring[5].x, ring[5].y, ring[0].x, ring[0].y);
    }
}

// ============== Boss突袭生成外星飞船 ==============
void spawnAlienFromBoss() {
    Alien a;
    a.startT = (bossX - perspLeft(boss.y)) / perspWidth(boss.y);
    if (a.startT < 0.1) a.startT = 0.1;
    if (a.startT > 0.9) a.startT = 0.9;
    a.targetT = 0.08 + (rand() % 840) / 1000.0;
    a.t = a.startT;
    a.enterStartY = boss.y;
    a.enterTargetY = boss.y + 50.0 + (rand() % 180);
    a.y = boss.y;
    a.enterFromBoss = true;
    a.enterFromTop = false;
    a.entering = true;
    a.enterFrame = 0;
    a.enterDuration = 16 + rand() % 18;
    a.invincibleFrames = -1;
    a.lastHitBySW = -1;
    a.lastHealHit = -1;
    a.beingAbsorbed = false;
    a.absorbFrame = 0; a.absorbDuration = 0;
    a.absorbStartX = 0; a.absorbStartY = 0;
    int hpBonus = (score >= 60) ? (score / 30 - 1) * 2 : 0;
    a.hp = ALIEN_MIN_HP + hpBonus + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
    a.maxHp = a.hp;
    a.active = true;
    aliens.push_back(a);
}

// ============== Phase2 透视椭圆冲击波动画 ==============
void drawCircularShockwave() {
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
        double sy = boss.y + std::sin(angle) * radius;
        double rx = radius * perspWidth(sy) / perspWidth(boss.y);
        int sx = (int)(bossX + std::cos(angle) * rx);
        if (i > 0)
            SDL_RenderDrawLine(renderer, prev.x, prev.y, sx, (int)sy);
        prev = {sx, (int)sy};
    }
}

// ============== 瞄准辅助 ==============
void drawAimAssist() {
    double tPlane = (planeX - perspLeft(planeY)) / perspWidth(planeY);
    if (tPlane < 0.0) tPlane = 0.0; if (tPlane > 1.0) tPlane = 1.0;
    double ty = planeY - BULLET_RANGE;
    double tx = perspLeft(ty) + tPlane * perspWidth(ty);
    double dx = tx - planeX, dy = ty - planeY;
    double fullDist = std::sqrt(dx*dx + dy*dy);
    if (fullDist < 1.0) return;
    // 默认位置：飞行距离的40%
    double defaultDist = fullDist * 0.40;
    double ex = planeX + dx / fullDist * defaultDist;
    double ey = planeY + dy / fullDist * defaultDist;

    // 有效射程终点：飞行距离的80%
    double effEndDist = fullDist * 0.80;
    double effEndX = planeX + dx / fullDist * effEndDist;
    double effEndY = planeY + dy / fullDist * effEndDist;

    // 有效射程全域搜索，用实际碰撞判定（弹道到达alien.y时的x偏移）
    double bestT = 999.0;
    double snapX = ex, snapY = ey;
    if (fabs(dy) > 0.001) {
        for (auto& a : aliens) {
            if (!a.active || a.entering || a.invincibleFrames != 0) continue;
            double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
            double t = (a.y - planeY) / dy;
            if (t < 0.0 || t > 0.80) continue;
            double hitX = planeX + dx * t;
            double dist = std::fabs(hitX - ax);
            double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
            double alienScale = (depthBelow < 0) ? 0.17 : 0.17 + 0.83 * depthBelow;
            if (alienScale < 0.14) alienScale = 0.14;
            double hitR = 22.0 * alienScale + 8.0;
            if (dist < hitR * 0.5 && t < bestT) { bestT = t; snapX = ax; snapY = a.y; }
        }
        // Boss吸附
        if (boss.active && phase != PHASE_BOSS_INTRO && phase != PHASE_BOSS_PHASE2) {
            double bt = (boss.y - planeY) / dy;
            if (bt > 0.0 && bt < 0.80) {
                double hitX = planeX + dx * bt;
                if (std::fabs(hitX - bossX) < 28.0 && bt < bestT) {
                    bestT = bt; snapX = bossX; snapY = boss.y;
                }
            }
        }
    }

    if (bestT < 999.0) snapProgress += 0.05; else snapProgress -= 0.12;
    if (snapProgress > 1.0) snapProgress = 1.0;
    if (snapProgress < 0.0) snapProgress = 0.0;

    // 锁定后瞬移到判定点，方形缩小动画
    double drawX = (bestT < 999.0) ? snapX : ex;
    double drawY = (bestT < 999.0) ? snapY : ey;

    // 方形随吸附进度向内缩小
    double ss = 8.0 * (1.0 - snapProgress);
    if (ss > 0.5) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, (unsigned char)(140 * (1.0 - snapProgress)));
        int sx = (int)(drawX - ss), sy = (int)(drawY - ss);
        int side = (int)(ss * 2);
        int gapPx = side * 6 / 10;
        if (gapPx < 2) gapPx = 2;
        int edgePx = (side - gapPx) / 2;
        if (edgePx < 1) edgePx = 1;
        if (side >= 3) {
            SDL_RenderDrawLine(renderer, sx, sy, sx+edgePx, sy);
            SDL_RenderDrawLine(renderer, sx+edgePx+gapPx, sy, sx+side, sy);
            SDL_RenderDrawLine(renderer, sx, sy+side, sx+edgePx, sy+side);
            SDL_RenderDrawLine(renderer, sx+edgePx+gapPx, sy+side, sx+side, sy+side);
            SDL_RenderDrawLine(renderer, sx, sy, sx, sy+edgePx);
            SDL_RenderDrawLine(renderer, sx, sy+edgePx+gapPx, sx, sy+side);
            SDL_RenderDrawLine(renderer, sx+side, sy, sx+side, sy+edgePx);
            SDL_RenderDrawLine(renderer, sx+side, sy+edgePx+gapPx, sx+side, sy+side);
        }
    }
    // 白点始终绘制，吸附时稍大
    int dotSize = (snapProgress > 0.5) ? 5 : 3;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect dot = {(int)(drawX - dotSize/2), (int)(drawY - dotSize/2), dotSize, dotSize};
    SDL_RenderFillRect(renderer, &dot);
}

// ============== 外星飞船：从侧面或顶部突袭入场 ==============
void spawnAlien() {
    Alien a;
    a.targetT = 0.15 + (rand() % 700) / 1000.0;  // 0.15~0.85
    a.enterFrame = 0;
    a.enterDuration = 20 + rand() % 25;           // 20~44帧
    a.entering = true;
    a.enterFromBoss = false;
    a.invincibleFrames = -1;
    a.lastHitBySW = -1;
    a.lastHealHit = -1;
    a.beingAbsorbed = false;
    a.absorbFrame = 0;
    a.absorbDuration = 0;
    a.absorbStartX = 0;
    a.absorbStartY = 0;
    int hpBonus = (score >= 60) ? (score / 30 - 1) * 2 : 0;
    a.hp = ALIEN_MIN_HP + hpBonus + rand() % (ALIEN_MAX_HP - ALIEN_MIN_HP + 1);
    a.maxHp = a.hp;
    a.active = true;

    int entryType = rand() % 3;  // 0=左侧, 1=右侧, 2=顶部
    if (entryType < 2) {
        // 侧面突入
        a.enterFromTop = false;
        a.enterStartY = 0; a.enterTargetY = 0;
        a.y = 30.0 + (rand() % 60);
        a.startT = (entryType == 0) ? -0.15 : 1.15;
        a.t = a.startT;
    } else {
        // 顶部突入：从屏幕上方冲下来
        a.enterFromTop = true;
        a.enterStartY = -40.0 - (rand() % 30);
        a.enterTargetY = 30.0 + (rand() % 60);
        a.y = a.enterStartY;
        a.startT = a.targetT;
        a.t = a.targetT;
    }
    aliens.push_back(a);
}

void updateAliens() {
    const double A = WIN_WIDTH / 2.0 - 15.0;
    const double B = 75.0;
    for (auto& a : aliens) {
        if (!a.active) continue;

        if (a.entering) {
            a.enterFrame++;
            double raw = (double)a.enterFrame / a.enterDuration;
            double eased = 1.0 - std::pow(1.0 - raw, 3.0);
            if (raw >= 1.0) {
                raw = 1.0;
                eased = 1.0;
                a.entering = false;
                a.invincibleFrames = 3;
                double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
                spawnExplosion(ax, a.y, 8);
            }
            if (a.enterFromBoss) {
                // Boss突袭：y和t同时从Boss位置向外辐射
                a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
                a.t = a.startT + (a.targetT - a.startT) * eased;
            } else if (a.enterFromTop) {
                // 顶部入场：y 从上方冲下来，t 不变
                a.y = a.enterStartY + (a.enterTargetY - a.enterStartY) * eased;
            } else {
                // 侧面入场：t 从边界冲入，y 不变
                a.t = a.startT + (a.targetT - a.startT) * eased;
            }
        }

        if (!a.entering) {
            if (a.invincibleFrames > 0) a.invincibleFrames--;
            // 入场完成 → 沿透视线缓慢下降
            double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
            double speedFactor = (depthBelow < 0)
                ? 0.08
                : 0.08 + 0.92 * depthBelow;
            a.y += alienBaseSpeed * speedFactor;
        }

        if (a.y > WIN_HEIGHT + 30) { a.active = false; continue; }

        double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
        double dx = (ax - CENTER_X) / A;
        double dy = (WIN_HEIGHT - a.y) / B;
        if (dx*dx + dy*dy < 1.0) {
            spawnExplosion(ax, a.y, 35);
            a.active = false;
            if (a.invincibleFrames != 0) {
                // Boss吸收阶段：无敌飞船碰基地直接消失，基地不扣血
            } else {
                baseHP--;
                sndBaseDamage();
                if (baseHP <= 0) { gameOver = true; menuSelection = 0; }
            }
        }
    }
}

void drawAlien(const Alien& a) {
    double ax = perspLeft(a.y) + a.t * perspWidth(a.y);
    double depthBelow = (a.y - HORIZON_Y) / (WIN_HEIGHT - HORIZON_Y);
    double scale = (depthBelow < 0) ? 0.17 : 0.17 + 0.83 * depthBelow;
    if (scale < 0.14) scale = 0.14;
    if (scale > 1.0)  scale = 1.0;
    int s = (int)(28.0 * scale);
    if (s < 3) s = 3;

    // 突袭中：运动轨迹残影（科幻感）
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
    if (inv) {
        r = 100; g = 180; b = 255;   // 无敌=蓝色
    } else {
        r = 255;
        g = (int)(150 * hpRatio + 80 * (1 - hpRatio));
        b = (int)(100 * hpRatio + 30 * (1 - hpRatio));  // 可受击=红色(满血亮红→残血暗红)
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    int ix = (int)ax, iy = (int)a.y;
    SDL_Point pts[4] = {
        {ix, iy - s},
        {ix + s * 2 / 3, iy},
        {ix, iy + s},
        {ix - s * 2 / 3, iy}
    };
    SDL_RenderDrawLines(renderer, pts, 4);
    SDL_RenderDrawLine(renderer, pts[3].x, pts[3].y, pts[0].x, pts[0].y);
    SDL_RenderDrawLine(renderer, ix - s*2/3, iy, ix + s*2/3, iy);
}

// ============== 爆炸粒子 ==============
void spawnExplosion(double x, double y, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
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
void updateParticles() {
    for (auto& p : particles) {
        if (!p.active) continue;
        p.x += p.vx; p.y += p.vy;
        p.vx *= 0.96; p.vy *= 0.96;
        p.life--;
        if (p.life <= 0) p.active = false;
    }
}
void drawParticle(const Particle& p) {
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
