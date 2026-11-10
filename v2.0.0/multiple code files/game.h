#pragma once

#include "font.h"
#include "renderer.h"
#include "audio.h"

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

#include "audio.h"
#include "bullets.h"
#include "constants.h"
#include "floating_text.h"
#include "font.h"
#include "particles.h"
#include "player.h"
#include "renderer.h"
#include "types.h"
#include "ch1/ch1_aliens.h"
#include "ch2/ch2_hud.h"
#include "save_menu.h"
#include "save_system.h"
#include <cstring>
#include <map>

// ============== Game 类 ==============

enum GamePhase { PHASE_PLAY, PHASE_BOSS_INTRO, PHASE_BOSS_FIGHT, PHASE_BOSS_PHASE2, PHASE_BOSS_DEFEAT };

// Chapter 2 scripted flow state machine (normal play):
// GATE → CORRIDOR → SPHERE → CHASE → LAB → BOSS → ENDED
enum Ch2Flow { C2_GATE, C2_CORRIDOR, C2_SPHERE, C2_CHASE, C2_LAB, C2_BOSS, C2_ENDED, C2_AIRLOCK };

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

    // Ch2 scripted flow state machine
    int ch2Flow;
    int ch2PhaseTimer;    // generic per-flow timer
    int ch2FadeTimer;     // white scene-transition fade (0=off, 1..29 active)
    bool ch2GateInitDone;
    bool ch2LabInitDone;
    Ch2GateScene gateScene;
    Ch2LabScene labScene;
    Ch2WardenBoss wardenBoss;
    int labUpgradeState;  // 0=approach, 1=upgrading animation, 2=done
    int labUpgradeTimer;
    bool ch2EpilogueStarted;
    // Ch2 scripted dialogue one-shot flags
    bool dGateQueued, dMoonwellQueued, dCorridorQueued, dSphereIntroQueued, dSphereActQueued;
    bool dChaseQueued, dOrbQueued, dPulseQueued, dLabQueued, dUpgradeQueued;
    bool dBossWarnQueued, dBossEnrageQueued;

    // Voice-over (narration / dialogue dubbing)
    int voiceLang;            // 0=中文 (default), 1=English, 2=OFF (letter pops only)
    std::map<uint32_t, std::string> voicePaths[2];  // [0]=zh [1]=en: hash → rel path (voice_manifest.txt)
    int lastNarrationPage;    // current narration page already voiced
    uint32_t lastDialogueHash;// hash of the dialogue line already voiced
    bool currentLineVoiced;   // current dialogue line has a real voice clip

    // Timing
    Uint32 lastTime;

    // ==== 存档系统（Save / Load）====
    SaveMenu saveMenu;
    SaveMeta slotMeta[SaveSystem::SLOT_COUNT];
    GameSettings settings;
    bool atSaveLoadScreen;     // 存档/读档/文件浏览器界面
    bool saveMenuFromPause;    // true=从暂停菜单进入（返回时回到暂停菜单）
    bool pendingAutoSave;      // 章节开始 / Ch2 流程段落切换时写 AUTO 槽
    uint32_t playFrames;       // 本局帧数（存档信息用）
    bool bkspWas;              // 文件浏览器返回上级目录的边沿检测
    bool devMode;              // --test：显示隐藏的 TEST 入口（[DORMANT — 仅开发构建]）
    int pendingSaveAction;     // 确认框待执行动作：0=无 1=存槽位 2=读槽位 3=另存路径 4=读文件
    int pendingSlot;
    std::string pendingPath;

    // Edge detection helpers for menus
    bool upWas, downWas, enterWas, escWas, leftWas, rightWas;
    bool pUpWas, pDownWas, pEnterWas, pLeftWas, pRightWas;   // 暂停菜单（改成员变量：读档后需要复位）
    bool f2Was;                // F2: CRT 后处理开关（纯视觉，不入档）
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
    Game(Renderer& r, AudioEngine& a, SDL_Window*, bool dev = false)
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
          ch2Flow(C2_GATE), ch2PhaseTimer(0), ch2FadeTimer(0),
          ch2GateInitDone(false), ch2LabInitDone(false),
          labUpgradeState(0), labUpgradeTimer(0), ch2EpilogueStarted(false),
          dGateQueued(false), dMoonwellQueued(false), dCorridorQueued(false),
          dSphereIntroQueued(false), dSphereActQueued(false),
          dChaseQueued(false), dOrbQueued(false), dPulseQueued(false),
          dLabQueued(false), dUpgradeQueued(false),
          dBossWarnQueued(false), dBossEnrageQueued(false),
          voiceLang(0), lastNarrationPage(-1), lastDialogueHash(0), currentLineVoiced(false),
          lastTime(0),
          atSaveLoadScreen(false), saveMenuFromPause(false), pendingAutoSave(false),
          playFrames(0), bkspWas(false), devMode(dev), pendingSaveAction(0), pendingSlot(0),
          upWas(false), downWas(false), enterWas(false), escWas(false),
          leftWas(false), rightWas(false),
          pUpWas(false), pDownWas(false), pEnterWas(false), pLeftWas(false), pRightWas(false),
          f2Was(false),
          autoSpawnPhase(0), autoSpawnQueued(0), autoSpawnTimer(0),
          autoSpawnWave3Reinf(0), autoSpawnScoreBase(0),
          autoSpawnAliveLast(0), autoSpawnKillsLast(0),
          lastShockwaveLevel(0),
          background(nullptr), sideBg(nullptr) {
        boss.setConfig(&chapterMgr.getConfig().bossConfig);
        background = new Ch1Background(chapterMgr.getConfig());
        sideBg = new Ch2Background();
        loadVoiceManifest();
        loadSettings();
        buildStartMenu();
    }

    // 启动时恢复 OPTIONS 设置（瞄准辅助 / 语音语言 / 音量与 EQ）
    void loadSettings() {
        settings.load();
        aimAssistOn = (settings.aimAssist != 0);
        voiceLang = settings.voiceLang;
        audio.setSoundState(settings.bgm, settings.sfx, settings.eqLow, settings.eqMid, settings.eqHigh);
    }

    // 设置变更后写回磁盘
    void storeSettings() {
        settings.aimAssist = aimAssistOn ? 1 : 0;
        settings.voiceLang = voiceLang;
        settings.bgm = audio.getBgmVolume();
        settings.sfx = audio.getSfxVolume();
        settings.eqLow = audio.getEqLow();
        settings.eqMid = audio.getEqMid();
        settings.eqHigh = audio.getEqHigh();
        settings.save();
    }

    // voice/voice_manifest.txt: "<lang> <crc32> <relpath>" per line
    void loadVoiceManifest() {
        FILE* f = fopen("voice/voice_manifest.txt", "rb");
        if (!f) return;
        char lang[16], hash[16], path[256];
        while (fscanf(f, "%15s %15s %255s", lang, hash, path) == 3) {
            uint32_t h = (uint32_t)strtoul(hash, nullptr, 16);
            int li = (strcmp(lang, "zh") == 0) ? 0 : 1;
            voicePaths[li][h] = path;
        }
        fclose(f);
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
        nightElf.reset();
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
        ch2Flow = C2_GATE; ch2PhaseTimer = 0; ch2FadeTimer = 0;
        ch2GateInitDone = false; ch2LabInitDone = false;
        gateScene.reset(); labScene.reset(); wardenBoss.reset();
        labUpgradeState = 0; labUpgradeTimer = 0; ch2EpilogueStarted = false;
        dGateQueued = false; dMoonwellQueued = false; dCorridorQueued = false;
        dSphereIntroQueued = false; dSphereActQueued = false;
        dChaseQueued = false; dOrbQueued = false; dPulseQueued = false;
        dLabQueued = false; dUpgradeQueued = false;
        dBossWarnQueued = false; dBossEnrageQueued = false;
        lastNarrationPage = -1; lastDialogueHash = 0; currentLineVoiced = false;
        autoSpawnPhase = 0; autoSpawnQueued = 0; autoSpawnTimer = 0;
        autoSpawnWave3Reinf = 0; autoSpawnScoreBase = 0;
        autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
        bossDefeatTimer = 0; defeatAlienTimer = 0; defeatReturnTimer = 0;
        defeatFWTimer = 0; defeatMCDelay = 0; defeatFadeTimer = 0;
        countdown = -1; countdownFrame = 0;
        soundCursor = 0;
        playFrames = 0;
        pendingAutoSave = false;
        atSaveLoadScreen = false;
        saveMenuFromPause = false;
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
            stepFrame(SDL_GetKeyboardState(NULL), escPressed, running);
            renderer.present();

            Uint32 now = SDL_GetTicks();
            Uint32 elapsed = now - lastTime;
            if (elapsed < 16) SDL_Delay(16 - elapsed);
            lastTime = SDL_GetTicks();
        }
        storeSettings();   // 退出前保存 OPTIONS 设置
    }

    // 单帧逻辑（run() 的主循环体；抽出来便于 --selftest 用合成按键驱动真实流程）
    void stepFrame(const Uint8* keys, bool escPressed, bool& running) {
        // 块作用域只是为了保留原 run() 循环体的缩进，不改任何逻辑
        {
            // F2 只切换 CRT 观感，不进入存档，也不影响任何 update 路径。
            bool f2Now = keys[SDL_SCANCODE_F2] != 0;
            if (f2Now && !f2Was) renderer.toggleCRT();
            f2Was = f2Now;

            bool wardenFight = chapterMgr.getConfig().isSideScrolling &&
                (wardenBoss.getState() == Ch2WardenBoss::FIGHT || wardenBoss.getState() == Ch2WardenBoss::ENRAGED);
            bool ch1BossFight = !chapterMgr.getConfig().isSideScrolling &&
                (phase == PHASE_BOSS_INTRO || phase == PHASE_BOSS_FIGHT ||
                 phase == PHASE_BOSS_PHASE2 || phase == PHASE_BOSS_DEFEAT);
            audio.setBossMusic(ch1BossFight || wardenFight);
            // 标题/章节/主菜单选项使用专门的慢速主题；暂停、读档、结算仍静音。
            // 从暂停进入 OPTIONS/SOUND 时不重新起标题曲，避免暂停被错误地"解冻"。
            bool startTheme = atStartScreen || atChapterSelect ||
                ((atOptionScreen || atSoundMenu) && !optionFromPause);
            bool bgmMuted = atTestSelect || atSaveLoadScreen || paused || gameOver || missionComplete ||
                ((atOptionScreen || atSoundMenu) && optionFromPause);
            audio.setStartBgm(startTheme);
            audio.setBgmOff(bgmMuted);
            audio.setCh2Bgm(chapterMgr.getConfig().isSideScrolling &&
                !atStartScreen && !atChapterSelect && !atTestSelect && !atOptionScreen && !atSoundMenu && !atSaveLoadScreen && !gameOver);
            int musicIntensity = 0;
            if (chapterMgr.getConfig().isSideScrolling) {
                if (ch2Flow == C2_CORRIDOR || ch2Flow == C2_SPHERE || ch2Flow == C2_BOSS)
                    musicIntensity = 1;
                if (ch2Flow == C2_CHASE || ch2Flow == C2_AIRLOCK || wardenFight)
                    musicIntensity = 2;
                if (wardenBoss.getState() == Ch2WardenBoss::ENRAGED) musicIntensity = 2;
            } else if (phase == PHASE_BOSS_INTRO || phase == PHASE_BOSS_FIGHT) {
                musicIntensity = 1;                 // demo boss layer 1
            } else if (phase == PHASE_BOSS_PHASE2 || phase == PHASE_BOSS_DEFEAT) {
                musicIntensity = 2;                 // demo boss layer 2
            } else if (phase == PHASE_PLAY && score >= 30) {
                musicIntensity = score >= 120 ? 2 : 1;
            }
            audio.setMusicIntensity(musicIntensity);
            // ======== Esc key global ========
            if (escPressed && inNarration) {
                // ESC skips entire opening narration
                narration.reset(); inNarration = false;
                audio.stopVoice();
            } else if (escPressed && !inNarration && !gameOver && !atStartScreen && !atTestSelect && !atChapterSelect
                && !atOptionScreen && !atSoundMenu && !atSaveLoadScreen && !missionComplete) {
                if (paused && countdown == -1) {
                    countdown = 3; countdownFrame = 0;
                } else {
                    paused = !paused;
                    pauseMenuSelection = 0;
                    if (paused) {
                        pauseHistoryFocused = false; dialogueSys.history.resetView();
                        audio.stopVoice();
                    }
                }
            }

            // 菜单、暂停和倒计时冻结世界；旁白由 updateNarration 单独推进背景。
            bool gameplayScreen = !atStartScreen && !atTestSelect && !atChapterSelect &&
                !atOptionScreen && !atSoundMenu && !atSaveLoadScreen && !gameOver && !inNarration;
            if (!paused && !atSaveLoadScreen && !atOptionScreen && !atSoundMenu && !gameOver && !inNarration) {
                if (background) background->update();
                if (gameplayScreen && sideBg) sideBg->update();
            }
            // TAB 跳过门禁，ESC 始终保留暂停/恢复功能（包括门禁存档）。
            if (gameplayScreen && !paused && chapterMgr.getConfig().isSideScrolling &&
                ch2Flow == C2_GATE && !gateScene.isOpen() && keys[SDL_SCANCODE_TAB]) {
                dialogueSys.reset(); audio.stopVoice();
                gateScene.skipToOpen();
                ch2GateInitDone = true;
                pulseSystem.energy = 0;
                dGateQueued = true; dMoonwellQueued = true;
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
            } else if (atSaveLoadScreen) {
                updateSaveLoadScreen(keys);
                drawSaveLoadScreen();
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
                playFrames++;
                updateGameplay(keys);
                // 章节起点 / Ch2 流程段落切换 → 写入 AUTO 槽（旁白阻塞结束后才落盘）
                if (pendingAutoSave && !inNarration && !gameOver && !missionComplete) {
                    pendingAutoSave = false;
                    std::string autoErr;
                    saveToPath(SaveSystem::slotPath(0), autoErr);
                }
                drawGameplayFrame();
                if (missionComplete) drawMissionComplete();
                if (paused && countdown >= 0) drawCountdown();
                else if (paused) drawPauseMenu();
            }
            // 统一 CRT/暗角后处理。所有界面都走同一出口，避免截图与实际画面不一致。
            renderer.composite();
        }
    }

    // ======== 开发自检（--selftest）========
    // 验证「暂停存档 → 继续玩一段（状态大幅改变）→ 读档」能回到与存档瞬间一致的状态与画面。
    // 每个场景检查：状态字节完全一致 / 画面像素一致（容差统计）/ 期间画面确实变化过 /
    //               文件元数据可读 / 读档后进入"暂停+菜单"状态。
    // 返回 0=PASS，1=FAIL；会在存档目录写 selftest_tmp.sav 并在结束时删除。
    bool renderToPixels(std::vector<uint32_t>& out) {
        SDL_Texture* tex = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);
        if (!tex) return false;
        SDL_SetRenderTarget(renderer.get(), tex);
        drawGameplayFrame();
        renderer.composite();
        out.assign((size_t)WIN_WIDTH * WIN_HEIGHT, 0);
        bool ok = SDL_RenderReadPixels(renderer.get(), NULL, SDL_PIXELFORMAT_ARGB8888,
                                      out.data(), WIN_WIDTH * 4) == 0;
        SDL_SetRenderTarget(renderer.get(), NULL);
        SDL_DestroyTexture(tex);
        return ok;
    }

    static uint32_t crcOfPixels(const std::vector<uint32_t>& px) {
        return saveCrc32((const unsigned char*)px.data(), px.size() * sizeof(uint32_t));
    }

    // 把某个界面画到离屏目标并导出 BMP（仅 --selftest 用于人工核对排版）
    bool captureBmp(const char* path) {
        std::vector<uint32_t> px((size_t)WIN_WIDTH * WIN_HEIGHT, 0);
        renderer.composite();
        if (SDL_RenderReadPixels(renderer.get(), NULL, SDL_PIXELFORMAT_ARGB8888,
                                 px.data(), WIN_WIDTH * 4) != 0) return false;
        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(px.data(), WIN_WIDTH, WIN_HEIGHT, 32, WIN_WIDTH * 4,
                                                     0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
        if (!surf) return false;
        bool ok = (SDL_SaveBMP(surf, path) == 0);
        SDL_FreeSurface(surf);
        return ok;
    }
    bool shotBegin() {
        SDL_Texture* tex = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);
        if (!tex) return false;
        SDL_SetRenderTarget(renderer.get(), tex);
        SDL_DestroyTexture(tex);   // 目标保持有效直到 resetTarget
        return true;
    }
    void shotEnd() { SDL_SetRenderTarget(renderer.get(), NULL); }

    // 界面截图组合（pause/存档界面需要游戏画面垫底）
    void shotStartScreen()   { drawStartScreen(); }
    void shotLoadScreen()    { drawSaveLoadScreen(); }
    void shotPauseMenu()     { drawGameplayFrame(); drawPauseMenu(); }
    void shotSaveFromPause() { drawGameplayFrame(); drawSaveLoadScreen(); }
    void shotBrowser()       { drawGameplayFrame(); drawSaveLoadScreen(); }

    void dumpScreens(const std::string& dir) {
        SaveSystem::ensureDir(dir);
        struct Shot { const char* file; void (Game::*fn)(); };
        const Shot shots[5] = {
            {"01_start.bmp",       &Game::shotStartScreen},
            {"02_load_slots.bmp",  &Game::shotLoadScreen},
            {"03_pause_menu.bmp",  &Game::shotPauseMenu},
            {"04_save_slots.bmp",  &Game::shotSaveFromPause},
            {"05_file_browser.bmp",&Game::shotBrowser}
        };
        // 主菜单
        resetGame(); atStartScreen = true; buildStartMenu();
        // 读档列表（主菜单路径）
        shotBegin(); refreshSlotMeta(); saveMenuFromPause = false; saveMenu.openLoad(); (this->*shots[0].fn)();
        captureBmp((dir + "/" + shots[0].file).c_str()); shotEnd();

        shotBegin(); refreshSlotMeta(); saveMenuFromPause = false;
        saveMenu.openLoad(); saveMenu.cursor = 1;
        (this->*shots[1].fn)();
        captureBmp((dir + "/" + shots[1].file).c_str()); shotEnd();

        // 暂停菜单 + 存档列表（第二章战斗中垫底）
        chapterMgr.selectChapter(1);
        resetGame(); atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        ch2Flow = C2_CHASE; dChaseQueued = true; score = 25;
        player = &nightElf; nightElf.reset();
        nightElf.setX(160); nightElf.setY(300);
        pulseSystem.unlocked = true; pulseSystem.energy = 18;
        dmMgr.spawnEnemy(); skillOrb.spawn(520.0, 220.0);
        std::vector<Uint8> noKeys(SDL_NUM_SCANCODES, 0);
        for (int i = 0; i < 150; ++i) updateGameplay(noKeys.data());
        shotBegin(); paused = true; pauseMenuSelection = 1;
        saveMenuFromPause = true; (this->*shots[2].fn)();
        captureBmp((dir + "/" + shots[2].file).c_str()); shotEnd();

        shotBegin(); refreshSlotMeta(); saveMenuFromPause = true;
        saveMenu.openSave(); saveMenu.cursor = 2;
        (this->*shots[3].fn)(); captureBmp((dir + "/" + shots[3].file).c_str()); shotEnd();

        shotBegin(); saveMenuFromPause = true;
        saveMenu.openBrowser(false); saveMenu.cursor = 2;
        (this->*shots[4].fn)(); captureBmp((dir + "/" + shots[4].file).c_str()); shotEnd();

        printf("[selftest] screenshots -> %s/0{1..5}_*.bmp\n", dir.c_str());
    }

    // 单个场景的存档往返验证
    int selfTestRoundTrip(const char* label, const std::vector<Uint8>& keys) {
        int failures = 0;
        std::vector<uint32_t> pxA;
        if (!renderToPixels(pxA)) { printf("[selftest] %-10s cannot read rendered pixels\n", label); return 1; }
        uint32_t hA = crcOfPixels(pxA);

        // 存档（序列化 + 打包 + 真实写盘 + 元数据读取）
        SaveArchive w(true);
        serializeAll(w);
        if (w.bad) { printf("[selftest] %-10s serialize error\n", label); return 1; }
        std::vector<unsigned char> payload = w.buf;
        std::vector<unsigned char> file = SaveSystem::pack(payload);
        std::string tmpPath = SaveSystem::baseDir() + "/selftest_tmp.sav";
        bool wrote = SaveSystem::writeBytes(tmpPath, file);
        SaveMeta meta; std::string merr;
        bool metaOk = wrote && SaveSystem::readMeta(tmpPath, meta, merr);
        if (!metaOk) { printf("[selftest] %-10s save-file/meta FAIL (%s)\n", label, merr.c_str()); failures++; }

        // 继续玩 180 帧，状态应显著变化
        for (int i = 0; i < 180; ++i) updateGameplay(keys.data());
        std::vector<uint32_t> pxMid;
        renderToPixels(pxMid);
        bool mutated = (crcOfPixels(pxMid) != hA);

        // 读档（真实读盘 + 校验 + 反序列化）
        std::vector<unsigned char> file2;
        SaveArchive r(false);
        std::string err;
        bool ok = SaveSystem::readBytes(tmpPath, file2) && SaveSystem::unpack(file2, r, err);
        if (!ok) { printf("[selftest] %-10s load FAIL (%s)\n", label, err.c_str()); remove(tmpPath.c_str()); return 1; }
        serializeAll(r);
        if (r.bad) { printf("[selftest] %-10s deserialize corrupt\n", label); remove(tmpPath.c_str()); return 1; }

        // 状态字节比对（SaveMeta.savedAt 是写档时的墙上时间，比较时跳过）
        SaveArchive w2(true);
        serializeAll(w2);
        std::vector<unsigned char> cmpA = payload, cmpB = w2.buf;
        for (size_t i = 8; i < 12 && i < cmpA.size() && i < cmpB.size(); ++i) { cmpA[i] = 0; cmpB[i] = 0; }
        bool sameBytes = (cmpA == cmpB);

        // 画面比对（技能球/能量条的呼吸光效用实时时钟，允许极小容差）
        std::vector<uint32_t> pxB;
        renderToPixels(pxB);
        int maxDiff = 0, over8 = 0;
        for (size_t i = 0; i < pxA.size() && i < pxB.size(); ++i) {
            for (int sh = 0; sh < 32; sh += 8) {
                int d = (int)((pxA[i] >> sh) & 0xFF) - (int)((pxB[i] >> sh) & 0xFF);
                if (d < 0) d = -d;
                if (d > maxDiff) maxDiff = d;
                if (d > 8) over8++;
            }
        }
        int totalCh = (int)pxA.size() * 4;
        double badPct = totalCh ? 100.0 * over8 / totalCh : 0.0;
        bool frameSame = (maxDiff <= 96 && badPct <= 1.0);

        printf("[selftest] %-10s bytes=%-3s frame=%-3s maxdiff=%d bad=%.3f%% mutated=%-3s meta=%-2s payload=%u\n",
               label, sameBytes ? "OK" : "BAD", frameSame ? "OK" : "BAD", maxDiff, badPct,
               mutated ? "yes" : "NO", metaOk ? "OK" : "BAD", (unsigned)payload.size());
        if (!sameBytes) {
            int shown = 0;
            for (size_t i = 0; i < cmpA.size() && i < cmpB.size() && shown < 6; ++i)
                if (cmpA[i] != cmpB[i]) { printf("[selftest]   diff@%zu: %02X vs %02X\n", i, cmpA[i], cmpB[i]); shown++; }
            failures++;
        }
        if (!frameSame)   failures++;
        if (!mutated)     failures++;

        // 读档收尾：应定格在"暂停 + 暂停菜单"，倒计时留给玩家 RESUME 触发
        finishLoad();
        if (!paused || countdown != -1 || !atStartScreen == false) failures++;
        remove(tmpPath.c_str());
        return failures;
    }

    int runSelfTest() {
        printf("[selftest] save/load round-trip verification\n");
        printf("[selftest] save dir: %s\n", SaveSystem::baseDir().c_str());
        std::vector<Uint8> keys(SDL_NUM_SCANCODES, 0);
        int failures = 0;
        if (!audio.runSynthesisSelfTest()) failures++;

        // ---- 场景 1：第二章追逐战（普敌 + 弹幕 + 技能球 + 粒子 + 脉冲波）----
        chapterMgr.selectChapter(1);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        bulletMgr.updateParams(1); shockwaveMgr.updateParams(1);
        ch2Flow = C2_CHASE; dChaseQueued = true;
        autoSpawnPhase = 2; autoSpawnQueued = 2; autoSpawnTimer = 0;
        score = 25;
        player = &nightElf; nightElf.reset();
        nightElf.setX(120); nightElf.setY(300);
        pulseSystem.unlocked = true; pulseSystem.energy = 15;
        dmMgr.spawnEnemy();
        skillOrb.spawn(520.0, 220.0);
        particleMgr.spawnExplosion(300, 300, 24);
        floatingTextMgr.spawn(300.0f, 280.0f, "SELFTEST");
        for (int i = 0; i < 240; ++i) {
            updateGameplay(keys.data());
            if (i == 60) pulseSystem.release((float)nightElf.getX(), (float)nightElf.getY(), particleMgr, audio);
            if (i == 120) { nightElf.setX(220); nightElf.setY(250); }
        }
        printf("[selftest] scene ch2-chase: aliens=%d danmaku=%d ebullets=%d pbullets=%d particles=%d\n",
               ch2AlienMgr.countLiving(), (int)dmMgr.getEnemies().size(),
               (int)ch2AlienMgr.getBullets().size() + (int)dmMgr.getBullets().size(),
               (int)bulletMgr.all().size(), (int)particleMgr.all().size());
        failures += selfTestRoundTrip("ch2-chase", keys);

        // ---- 场景 2：第二章门禁序列（对话打字中 + 门禁脉冲/扫描）----
        chapterMgr.selectChapter(1);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        player = &ch2Trainer; ch2Trainer.reset();
        for (int i = 0; i < 90; ++i) updateGameplay(keys.data());
        printf("[selftest] scene ch2-gate: flow=%d stage=%d dialogueActive=%d\n",
               ch2Flow, (int)gateScene.getStage(), dialogueSys.isActive() ? 1 : 0);
        failures += selfTestRoundTrip("ch2-gate", keys);

        // ---- 场景 3：第二章章节 Boss（Warden 弹幕循环 + 召唤增援）----
        chapterMgr.selectChapter(1);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        ch2Flow = C2_BOSS;
        dLabQueued = true; dUpgradeQueued = true; dBossWarnQueued = true;
        player = &nightElf; nightElf.reset();
        nightElf.setX(150); nightElf.setY(300);
        pulseSystem.unlocked = true; pulseSystem.energy = 20;
        autoSpawnPhase = 6;
        wardenBoss.startEntering();
        for (int i = 0; i < 420; ++i) {
            updateGameplay(keys.data());
            if (i % 60 == 0) { ch2PlayerHP = 3; ch2GameOver = false; gameOver = false; }
        }
        printf("[selftest] scene ch2-warden: bossState=%d hp=%d bossBullets=%d aliens=%d\n",
               (int)wardenBoss.getState(), wardenBoss.getHp(), (int)wardenBoss.bullets.size(),
               ch2AlienMgr.countLiving());
        failures += selfTestRoundTrip("ch2-warden", keys);

        // ---- 场景 4：第一章 Boss 战（透视背景 + 冲击波 + Telamondo）----
        chapterMgr.selectChapter(0);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        bulletMgr.updateParams(6); shockwaveMgr.updateParams(6);
        score = 200; enemiesEnabled = true;
        boss.setY(90); boss.hpRef() = 500; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
        boss.setActive(true); boss.enteringRef() = false;
        boss.phase2TriggeredRef() = true; boss.flashTimerRef() = 0;
        phase = PHASE_BOSS_FIGHT;
        boss.setCh1HealWavesEnabled(true);
        shockwaveMgr.setPending(true);
        player = &trainingPlane; trainingPlane.reset();
        alienMgr.spawnAlien(score); alienMgr.spawnAlien(score);
        for (int i = 0; i < 240; ++i) {
            updateGameplay(keys.data());
            boss.shakeTimerRef() = 0;    // 屏幕抖动会临时切换渲染目标，自检期间关掉
        }
        printf("[selftest] scene ch1-boss: bossHp=%d aliens=%d shockwaves=%d particles=%d\n",
               boss.getHp(), alienMgr.countAlive(), (int)shockwaveMgr.all().size(),
               (int)particleMgr.all().size());
        failures += selfTestRoundTrip("ch1-boss", keys);

        // ---- 场景 5：菜单接线（合成按键驱动真实 stepFrame）----
        failures += uiSelfTest();
        failures += ch2QaSelfTest();

        printf("[selftest] RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
        return failures == 0 ? 0 : 1;
    }

    // 第二章回归：真实单帧菜单输入 + 顺序推进七段流程（战斗注入命中，保持玩家存活）。
    int ch2QaSelfTest() {
        int failures = 0;
        auto check = [&failures](bool ok, const char* label) {
            printf("[ch2-qa] %s: %s\n", label, ok ? "PASS" : "FAIL");
            if (!ok) failures++;
        };
        std::vector<Uint8> k(SDL_NUM_SCANCODES, 0);
        SaveSystem::ensureDir("/tmp/sfss_ch2_qa");
        auto snap = [this](const char* name) {
            if (!shotBegin()) return;
            drawGameplayFrame();
            if (missionComplete) drawMissionComplete();
            captureBmp((std::string("/tmp/sfss_ch2_qa/") + name + ".bmp").c_str());
            shotEnd();
        };
        bool running = true;
        nodeResetCh2();
        stepFrame(k.data(), false, running);
        snap("01_gate");
        stepFrame(k.data(), true, running);
        check(paused && !gateScene.isOpen(), "gate ESC pauses without skipping");
        SaveArchive before; sideBg->visit(before);
        for (int i = 0; i < 20; ++i) stepFrame(k.data(), false, running);
        SaveArchive after; sideBg->visit(after);
        check(before.buf == after.buf, "pause freezes corridor background");
        stepFrame(k.data(), true, running);
        check(paused && countdown == 3 && !gateScene.isOpen(), "gate ESC resumes via countdown");
        for (int i = 0; i < 185; ++i) stepFrame(k.data(), false, running);
        check(!paused && !gateScene.isOpen(), "countdown returns to gate");
        openLoadScreen(true);
        k[SDL_SCANCODE_ESCAPE] = 1;
        stepFrame(k.data(), true, running);
        k[SDL_SCANCODE_ESCAPE] = 0;
        check(!gateScene.isOpen(), "load menu ESC does not skip gate");
        nodeResetCh2();
        k[SDL_SCANCODE_TAB] = 1;
        stepFrame(k.data(), false, running);
        k[SDL_SCANCODE_TAB] = 0;
        check(gateScene.isOpen() && pulseSystem.energy == 0, "TAB skips gate and consumes pulse energy");

        // 待刷增援的空场不能被误判为追逐战结束。
        nodeCh2ChaseBase(25);
        autoSpawnPhase = 6; autoSpawnQueued = 10; autoSpawnTimer = 1000;
        ch2PhaseTimer = 119;
        updateGameplay(k.data());
        check(ch2Flow == C2_CHASE, "queued reinforcements block lab transition");
        player->setX(WIN_WIDTH + 100);
        updateGameplay(k.data());
        check(player->getX() + player->getNoseOffset() < WIN_WIDTH, "side-scroll stays on screen");
        nightElf.setTripleFire(true); nightElf.setInvFrames(60);
        resetGame();
        check(!nightElf.isTripleFire() && nightElf.getInvFrames() == 0, "restart clears NightElf state");

        // 完整顺序流程：门禁演出不跳过；模拟命中经过游戏现有碰撞逻辑。
        // 沿途存下四张门禁阶段截图，便于人工核对开门/扫描动画（/tmp/sfss_ch2_qa/）。
        nodeResetCh2();
        bool gatePulse = nodeRunUntil(k, 8000, [this]{ return gateScene.getStage() == Ch2GateScene::PULSING
                                                            && !gateScene.groups.empty(); });
        snap("01b_gate_pulse");
        bool gateScan = nodeRunUntil(k, 4000, [this]{ return gateScene.getStage() == Ch2GateScene::SCANNING
                                                            && gateScene.beamTimer >= 150; });
        snap("01c_gate_scan");
        bool gateOpening = nodeRunUntil(k, 4000, [this]{ return gateScene.getStage() == Ch2GateScene::OPENING
                                                            && gateScene.doorOpen >= 0.35; });
        snap("01d_gate_open");
        bool gateReady = nodeRunUntil(k, 20000, [this]{ return gateScene.isFullyOpen() && !dialogueSys.isActive(); });
        snap("01e_gate_full");
        check(gatePulse, "gate pulse sequence reached");
        check(gateScan, "gate scan beam locked");
        check(gateOpening, "gate panels opening");
        check(gateReady, "natural gate dialogue/pulse/scan/open sequence");
        player->setX(660);
        // 淡出中段：验证白场是渐变而不是硬闪
        nodeRunUntil(k, 100, [this]{ return ch2FadeTimer >= 12; });
        snap("01f_gate_fade");
        bool corridor = nodeRunUntil(k, 100, [this]{ return ch2Flow == C2_CORRIDOR; });
        check(corridor && pendingAutoSave, "gate to corridor with autosave marker");
        // 转场中段：飞机从左侧飞入廊桥
        nodeRunUntil(k, 200, [this]{ return ch2FadeTimer >= 55; });
        snap("01g_gate_flyin");
        nodeRunUntil(k, 200, [this]{ return ch2FadeTimer == 0; });
        bool sphere = nodeRunUntil(k, 12000, [this]{ return ch2Flow == C2_SPHERE && sphereBoss.getState() == Ch2SphereBoss::FIGHT; });
        check(sphere, "corridor to sphere fight");
        snap("02_sphere");
        {
            int hp0 = sphereBoss.getHp(), on0 = sphereBoss.countOnSphere();
            int dropped = sphereBoss.hitDiamonds(3);
            check(dropped == 3 && sphereBoss.getHp() == hp0 - dropped
                  && sphereBoss.countOnSphere() == on0 - dropped,
                  "sphere drops 3 diamonds per hit with HP sync");
            snap("02a_sphere_3drop");
        }
        sphereBoss.takeDamage(sphereBoss.getMaxHp());
        bool ghost = nodeRunUntil(k, 1200,
            [this]{ return sphereBoss.getState() == Ch2SphereBoss::SHATTERING
                           && sphereBoss.getStateTimer() >= 55; });
        check(ghost, "sphere black body ghost phase");
        snap("02b_sphere_ghost");
        bool grounded = nodeRunUntil(k, 3000,
            [this]{ return sphereBoss.getState() == Ch2SphereBoss::SHAKING; });
        check(grounded, "sphere debris all grounded before shaking");
        snap("02c_sphere_shake");
        bool chase = nodeRunUntil(k, 6000, [this]{ return ch2Flow == C2_CHASE; });
        check(chase, "sphere shatter to chase");
        snap("03_chase");
        // 第一波从地面升起：确认玩家能看到"刚才散落的菱形"回来
        nodeRunUntil(k, 100, [this]{
            for (const auto& a : ch2AlienMgr.getAliens())
                if (a.active && a.entering && a.enterFrame > 12) return true;
            return false;
        });
        snap("03b_shards_return");
        auto hitAt = [this](double x, double y) {
            Ch1Bullet b = {};
            b.x = x; b.y = y; b.startX = x; b.startY = y;
            b.active = true; b.canDamage = true; b.sideScroll = true;
            bulletMgr.all().push_back(b);
        };
        bool lab = false;
        for (int i = 0; i < 18000 && ch2Flow == C2_CHASE; ++i) {
            nodeHealCh2();
            for (const auto& a : ch2AlienMgr.getAliens())
                if (a.active && !a.entering) hitAt(a.x, a.y);
            for (const auto& e : dmMgr.getEnemies())
                if (e.active && !e.entering && !e.defeated) hitAt(e.x, e.y);
            if (skillOrb.state == Ch2SkillOrb::FLOATING) hitAt(skillOrb.x, skillOrb.y);
            if (skillOrb.isCore()) {
                player->setX((int)skillOrb.x); player->setY((int)skillOrb.y);
                k[SDL_SCANCODE_LSHIFT] = 1;
            } else k[SDL_SCANCODE_LSHIFT] = 0;
            nodeStep(k.data());
        }
        k[SDL_SCANCODE_LSHIFT] = 0;
        if (ch2Flow == C2_AIRLOCK) {
            // 截一张红光锁定舱门的画面
            nodeRunUntil(k, 400, [this]{ return ch2PhaseTimer >= Ch2AirlockScene::LOCK_AT + 30; });
            snap("03b_airlock");
            bool airlockReady = nodeRunUntil(k, 500,
                [this]{ return Ch2AirlockScene::canPass(ch2PhaseTimer); });
            check(airlockReady, "corridor airlock scan and open");
            snap("03c_airlock_open");
            player->setX(725); player->setY(WIN_HEIGHT / 2);
            snap("03d_airlock_pass");
            nodeRunUntil(k, 400, [this]{ return ch2Flow == C2_LAB; });
        }
        lab = ch2Flow == C2_LAB;
        check(lab && pulseSystem.unlocked && score >= 30, "waves/orb/absorption/reinforcements to lab");
        if (lab) {
            // 转场中段：飞机从左侧飞入研究室（灯柱正在逐段通电）
            nodeRunUntil(k, 300, [this]{ return ch2FadeTimer >= 55; });
            snap("04b_lab_flyin");
            // 等淡入 + 左侧飞入结束，截到真正的研究室画面
            nodeRunUntil(k, 300, [this]{ return ch2FadeTimer == 0; });
            nodeStep(k.data());
            snap("04_lab");
            player->setX(Ch2LabScene::PARK_X); player->setY(Ch2LabScene::PARK_Y);
            bool bossReady = nodeRunUntil(k, 12000, [this]{ return ch2Flow == C2_BOSS; });
            check(bossReady && player == &nightElf, "lab upgrades to NightElf and starts Warden");
            nodeRunUntil(k, 1000, [this]{ return wardenBoss.getState() == Ch2WardenBoss::FIGHT; });
            snap("05_warden");
            wardenBoss.takeDamage(Ch2WardenBoss::ENRAGE_HP);
            check(wardenBoss.getState() == Ch2WardenBoss::ENRAGED, "Warden enrage threshold");
            wardenBoss.takeDamage(Ch2WardenBoss::MAX_HP);
            snap("06_warden_dying");
            nodeRunUntil(k, 1000, [this]{ return ch2Flow == C2_ENDED; });
            check(ch2Flow == C2_ENDED && inNarration && wardenBoss.isDefeated(), "Warden death enters epilogue");
            for (int i = 0; i < 12000 && inNarration; ++i) {
                k[SDL_SCANCODE_RETURN] = (i % 2 == 0);
                updateNarration(k.data());
            }
            k[SDL_SCANCODE_RETURN] = 0;
            nodeStep(k.data());
            check(missionComplete, "epilogue reaches mission complete");
            nodeRun(k, 80);
            snap("07_ended");
        }
        return failures;
    }

    // 菜单流程自检：主菜单→LOAD GAME→AUTO 槽→确认→读档；暂停菜单→RESUME→倒计时；
    // ESC 暂停→SAVE GAME→写槽位。用合成按键状态驱动 stepFrame（与 run() 完全同一条代码路径）。
    int uiSelfTest() {
        printf("[selftest] ui flow (synthetic keys)\n");
        int failures = 0;
        std::vector<Uint8> none(SDL_NUM_SCANCODES, 0);
        std::vector<Uint8> down(SDL_NUM_SCANCODES, 0);  down[SDL_SCANCODE_S] = 1;
        std::vector<Uint8> enter(SDL_NUM_SCANCODES, 0); enter[SDL_SCANCODE_RETURN] = 1;
        std::vector<Uint8> esck(SDL_NUM_SCANCODES, 0);  esck[SDL_SCANCODE_ESCAPE] = 1;
        bool running = true;
        auto step = [&](const std::vector<Uint8>& k, int n, bool escEvent = false) {
            for (int i = 0; i < n; ++i) stepFrame(k.data(), escEvent, running);
        };

        // 0) 先造一个已知存档（AUTO 槽），并记下分数
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        score = 77; ch2PlayerHP = 3;
        std::string werr;
        bool wroteAuto = saveToPath(SaveSystem::slotPath(0), werr);
        int savedScore = score;

        // 1) 主菜单：按 S 走到 LOAD GAME 并进入
        resetGame();
        atStartScreen = true;
        startMenuSelection = 0;
        step(none, 2);
        int loadIdx = -1;
        for (int i = 0; i < startItemCount; ++i) if (startItemId[i] == SI_LOAD) loadIdx = i;
        for (int i = 0; i < loadIdx; ++i) { step(down, 2); step(none, 1); }
        step(enter, 2); step(none, 1);
        bool inLoadScreen = atSaveLoadScreen && saveMenu.screen == SaveMenu::SCR_LOAD && !saveMenuFromPause;
        printf("[selftest] ui write-auto=%s  main menu -> LOAD GAME: %s\n",
               wroteAuto ? "ok" : "FAIL", inLoadScreen ? "ok" : "FAIL");
        if (!wroteAuto || !inLoadScreen) failures++;

        // 2) 选 AUTO 槽 → 确认框 → ENTER 确认 → 读档
        step(enter, 2); step(none, 1);
        bool confirmShown = saveMenu.confirmActive;
        step(enter, 2); step(none, 1);
        bool loaded = (!atSaveLoadScreen && paused && countdown == -1 && saveMenuFromPause);
        printf("[selftest] ui confirm-box=%s  loaded(paused)=%s  score=%d (expect %d)\n",
               confirmShown ? "ok" : "FAIL", loaded ? "ok" : "FAIL", score, savedScore);
        if (!confirmShown || !loaded || score != savedScore) failures++;

        // 3) 暂停菜单 RESUME → 3-2-1 倒计时 → 恢复游戏
        pauseMenuSelection = 0;
        step(enter, 2); step(none, 1);
        bool counting = (paused && countdown == 3);
        step(none, 150);
        bool resumed = (!paused && countdown == -1);
        printf("[selftest] ui countdown-start=%s  resumed=%s\n",
               counting ? "ok" : "FAIL", resumed ? "ok" : "FAIL");
        if (!counting || !resumed) failures++;

        // 4) 游玩中 ESC 暂停 → SAVE GAME → 写入 SLOT 1
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        score = 42;
        step(none, 2);
        step(none, 1, true);          // ESC 事件 → 暂停
        step(none, 2);
        bool pausedByEsc = paused;
        pauseMenuSelection = 1;       // SAVE GAME
        step(enter, 2); step(none, 1);
        bool inSaveScreen = atSaveLoadScreen && saveMenu.screen == SaveMenu::SCR_SAVE && saveMenuFromPause;
        step(enter, 2); step(none, 1);  // 选 SLOT 1（空 → 直接保存）
        SaveMeta m1; std::string merr;
        bool slotOk = SaveSystem::readMeta(SaveSystem::slotPath(1), m1, merr) && m1.score == 42;
        printf("[selftest] ui esc-pause=%s  save-screen=%s  slot1-score=%d\n",
               pausedByEsc ? "ok" : "FAIL", inSaveScreen ? "ok" : "FAIL", slotOk ? m1.score : -1);
        if (!pausedByEsc || !inSaveScreen || !slotOk) failures++;

        // 5) 文件浏览器：进入 → ESC 返回读档列表
        resetGame();
        atStartScreen = true;
        step(none, 2);
        openLoadScreen(false);
        step(none, 2);
        saveMenu.cursor = SaveSystem::SLOT_COUNT;    // BROWSE FILE...
        step(enter, 2); step(none, 1);
        bool inBrowser = (saveMenu.screen == SaveMenu::SCR_BROWSER) && !saveMenu.browser.rows.empty();
        step(esck, 2); step(none, 1);   // ESC 键 → 返回槽位列表（浏览器读的是按键状态，非事件）
        step(none, 2);
        bool backToList = (saveMenu.screen == SaveMenu::SCR_LOAD);
        printf("[selftest] ui browser-open=%s rows=%d  esc-back=%s\n",
               inBrowser ? "ok" : "FAIL", (int)saveMenu.browser.rows.size(), backToList ? "ok" : "FAIL");
        if (!inBrowser || !backToList) failures++;

        // 6) OPTIONS 设置持久化往返（写入 settings.dat 再读回）
        aimAssistOn = true; voiceLang = 2;
        audio.setSoundState(3, 9, -2, 4, 1);
        storeSettings();
        aimAssistOn = false; voiceLang = 0;
        audio.setSoundState(7, 7, 0, 0, 0);
        loadSettings();
        bool settingsOk = aimAssistOn && voiceLang == 2 && audio.getBgmVolume() == 3 &&
                          audio.getSfxVolume() == 9 && audio.getEqLow() == -2 &&
                          audio.getEqMid() == 4 && audio.getEqHigh() == 1;
        printf("[selftest] ui settings-persist=%s (aim=%d voice=%d bgm=%d sfx=%d eq=%d/%d/%d)\n",
               settingsOk ? "ok" : "FAIL", aimAssistOn ? 1 : 0, voiceLang, audio.getBgmVolume(),
               audio.getSfxVolume(), audio.getEqLow(), audio.getEqMid(), audio.getEqHigh());
        if (!settingsOk) failures++;

        // 7) 跨章节读档：第一章游玩中读取第二章存档（章节/背景/机体必须一起切换）
        chapterMgr.selectChapter(0);
        resetGame(); atStartScreen = false; isNormalPlay = true;
        for (int i = 0; i < 30; ++i) stepFrame(none.data(), false, running);
        chapterMgr.selectChapter(1);
        resetGame(); atStartScreen = false; isNormalPlay = true;
        player = &nightElf; nightElf.reset();
        ch2Flow = C2_CHASE; dChaseQueued = true;
        autoSpawnPhase = 1; autoSpawnQueued = 3; score = 33;
        for (int i = 0; i < 90; ++i) updateGameplay(none.data());
        saveToPath(SaveSystem::slotPath(2), werr);
        chapterMgr.selectChapter(0);
        resetGame(); atStartScreen = false; isNormalPlay = true;
        for (int i = 0; i < 30; ++i) stepFrame(none.data(), false, running);
        bool crossOk = loadFromPath(SaveSystem::slotPath(2), werr) &&
                       chapterMgr.getCurrentIndex() == 1 &&
                       chapterMgr.getConfig().isSideScrolling &&
                       player == (Player*)&nightElf && score == 33 && paused;
        paused = false;                      // 退出暂停，跑一段确认第二章分支不会崩
        for (int i = 0; i < 90; ++i) stepFrame(none.data(), false, running);
        std::vector<uint32_t> pxCross;
        bool crossRender = renderToPixels(pxCross);
        printf("[selftest] ui cross-chapter-load=%s render=%s chapter=%d side=%d\n",
               crossOk ? "ok" : "FAIL", crossRender ? "ok" : "FAIL",
               chapterMgr.getCurrentIndex(), chapterMgr.getConfig().isSideScrolling ? 1 : 0);
        if (!crossOk || !crossRender) failures++;

        // 8) 隐藏 TEST 入口（--test）只改变主菜单项，不影响 LOAD GAME
        bool savedDev = devMode;
        devMode = false; buildStartMenu();
        int normalCount = startItemCount;
        bool normalHasTest = false;
        for (int i = 0; i < startItemCount; ++i) if (startItemId[i] == SI_TEST) normalHasTest = true;
        devMode = true; buildStartMenu();
        bool devHasTest = false;
        for (int i = 0; i < startItemCount; ++i) if (startItemId[i] == SI_TEST) devHasTest = true;
        devMode = savedDev; buildStartMenu();
        bool menuOk = (normalCount == 5 && !normalHasTest && devHasTest);
        printf("[selftest] ui menu-items normal=%d(hidden-test=%s) dev-test=%s\n",
               normalCount, normalHasTest ? "shown" : "hidden", devHasTest ? "shown" : "MISSING");
        if (!menuOk) failures++;

        // 9) 列表最后一项 BACK → 回到主菜单
        resetGame();
        atStartScreen = true;
        step(none, 2);
        openLoadScreen(false);
        step(none, 2);
        saveMenu.cursor = saveMenu.loadRowCount() - 1;
        step(enter, 2); step(none, 1);
        bool backToMenu = (!atSaveLoadScreen && atStartScreen);
        printf("[selftest] ui list-back-to-menu=%s\n", backToMenu ? "ok" : "FAIL");
        if (!backToMenu) failures++;

        // 10) 旧 TEST 模式的对话历史预填充（抽成公共函数后仍可用）
        chapterMgr.selectChapter(0);
        resetGame();
        prepopulateCh1History(180);
        // 注：PH(scr) 的判定是 score > scr（与游戏内的触发条件一致），180 分不会触发 PH(180)
        bool histOk = (dialogueSys.history.size() > 0) && triggeredScores[160] &&
                      triggeredScores[0] && !triggeredScores[195];
        printf("[selftest] ui ch1-history-prefill=%s entries=%d\n",
               histOk ? "ok" : "FAIL", dialogueSys.history.size());
        if (!histOk) failures++;

        // 11) 端到端：用文件浏览器选中一个节点存档并读档（玩家读节点存档的实际路径）
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        score = 66; ch2PlayerHP = 3;
        std::string nodePath = SaveSystem::baseDir() + "/n99_uitest.sav";
        std::string nerr;
        bool nodeWrote = saveToPath(nodePath, nerr);
        score = 5;                               // 改脏，读档后应回到 66
        resetGame();
        atStartScreen = true;
        step(none, 2);
        openLoadScreen(false);
        step(none, 2);
        saveMenu.cursor = SaveSystem::SLOT_COUNT;         // BROWSE FILE...
        step(enter, 2); step(none, 1);
        int rowIdx = -1;
        for (int i = 0; i < (int)saveMenu.browser.rows.size(); ++i)
            if (saveMenu.browser.rows[i].label == "n99_uitest.sav") rowIdx = i;
        bool rowFound = (rowIdx >= 0);
        if (rowFound) {
            saveMenu.browser.cursor = rowIdx;
            saveMenu.browser.clampScroll();
        }
        step(enter, 2); step(none, 1);            // 选中文件 → 确认框
        bool fileConfirm = saveMenu.confirmActive;
        step(enter, 2); step(none, 1);            // ENTER = 读档
        bool fileLoaded = (!atSaveLoadScreen && paused && score == 66);
        printf("[selftest] ui browse-load-node: wrote=%s row=%s confirm=%s loaded=%s score=%d\n",
               nodeWrote ? "ok" : "FAIL", rowFound ? "ok" : "FAIL",
               fileConfirm ? "ok" : "FAIL", fileLoaded ? "ok" : "FAIL", score);
        if (!nodeWrote || !rowFound || !fileConfirm || !fileLoaded) failures++;
        remove(nodePath.c_str());

        // 12) 导出各界面截图，便于人工核对排版（无头渲染，不影响游戏）
        dumpScreens("/tmp/sfss_shots");

        resetGame();
        return failures;
    }


    // ======== 节点存档生成器（--mknodes）========
    // 目的：替代过时的 TEST 模式。每个节点先把游戏摆到该处状态（必要时空跑若干帧让其
    // 自然推进），再调用与手动存档完全相同的 saveToPath() 落盘 —— 生成的文件在
    // LOAD GAME → BROWSE FILE 里读取即可，等价于原来的跳关，但状态是完整合法的。
    // 维护方式：游戏加了新内容，只要往 nodeTable() 加一条 + 在 buildNodeState() 加一个
    // case，重跑 ./shooter --mknodes 就能刷新全部节点存档。
    struct NodeDef {
        const char* file;
        const char* title;
        const char* desc;
    };

    // UTF-8 显示宽度（中文/全角按 2 列算）——`%-26s` 按字节填充会让中文列错位
    static int dispWidth(const char* s) {
        int w = 0;
        for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
            if ((*p & 0xC0) == 0x80) continue;    // UTF-8 续字节
            w += (*p < 0x80) ? 1 : 2;
        }
        return w;
    }
    static std::string padTo(const std::string& s, int width) {
        int w = dispWidth(s.c_str());
        if (w >= width) return s;
        return s + std::string((size_t)(width - w), ' ');
    }

    static const int NODE_COUNT = 34;
    static const NodeDef* nodeTable() {
        static const NodeDef t[NODE_COUNT] = {
            // ---- 第一章：透视空战 ----
            {"n01_ch1_start.sav",        "第一章 · 开场",            "分数 0，开场对话中；对话结束敌人出现"},
            {"n02_ch1_score30.sav",      "第一章 · 分数 30",         "冲击波 Lv1（基地炮解锁）"},
            {"n03_ch1_score60.sav",      "第一章 · 分数 60",         "冲击波 Lv2"},
            {"n04_ch1_score90.sav",      "第一章 · 分数 90",         "冲击波 Lv3"},
            {"n05_ch1_score120.sav",     "第一章 · 分数 120",        "冲击波 Lv4 + 首都舰预警对话"},
            {"n06_ch1_score150.sav",     "第一章 · 分数 150",        "冲击波 Lv5"},
            {"n07_ch1_score180.sav",     "第一章 · 分数 180",        "冲击波 Lv6 满级 + 拦截警告"},
            {"n08_ch1_boss_intro.sav",   "第一章 · Boss 登场",       "分数 200：TELAMONDO 登场动画"},
            {"n09_ch1_boss_fight.sav",   "第一章 · Boss 一阶段",     "满血战斗 + 治疗波 + 小怪"},
            {"n10_ch1_boss_absorb.sav",  "第一章 · Boss 吸收阶段",   "二阶段：吸收小怪回血（蓝色吸收光束）"},
            {"n11_ch1_boss_phase2.sav",  "第一章 · Boss 二阶段",     "半血狂暴，纯战斗"},
            {"n12_ch1_boss_1hp.sav",     "第一章 · Boss 剩 1 血",    "快速验证击破演出"},
            {"n13_ch1_boss_defeat.sav",  "第一章 · Boss 击破",       "链式爆炸演出 → MISSION COMPLETE"},
            // ---- 第二章：侧滚廊桥 ----
            {"n14_ch2_start.sav",        "第二章 · 开场",            "门禁前，与 Bryssa 的开场对话中"},
            {"n15_ch2_gate_pulsing.sav", "第二章 · 门禁脉冲",        "Ally 脉冲解锁中，绿色能量条消耗"},
            {"n16_ch2_gate_scan.sav",    "第二章 · 门禁扫描",        "红色扫描光束锁定 + Moonwell AI 对话"},
            {"n17_ch2_gate_open.sav",    "第二章 · 门禁开启",        "大门已开，可向右飞入"},
            {"n18_ch2_corridor.sav",     "第二章 · 廊桥飞行",        "球体出现前的安静飞行段"},
            {"n19_ch2_sphere_enter.sav", "第二章 · 球体入场",        "球体 Boss 随滚动靠近中"},
            {"n20_ch2_sphere_fight.sav", "第二章 · 球体战斗",        "激活完成，橙色格子可输出"},
            {"n21_ch2_sphere_shatter.sav","第二章 · 球体碎裂",       "格子炸开 + 碎片物理演出"},
            {"n22_ch2_chase_wave1.sav",  "第二章 · 追逐战 第1波",    "自动出敌系统：3 只普敌"},
            {"n23_ch2_chase_wave3.sav",  "第二章 · 追逐战 第3波",    "5 只 + 逃跑补位增援，最激烈的一段"},
            {"n24_ch2_danmaku.sav",      "第二章 · 弹幕敌人",        "螺旋弹幕敌人出场"},
            {"n25_ch2_orb_shield.sav",   "第二章 · 技能球护罩",      "18 边形护罩阶段（打 18 下破罩）"},
            {"n26_ch2_orb_absorb.sav",   "第二章 · 技能球吸收",      "按住 Shift 吸收核心充能中"},
            {"n27_ch2_pulse_full.sav",   "第二章 · 脉冲满能量",      "绿色能量满，Shift 可放冲击波"},
            {"n28_ch2_nightelf_triple.sav","第二章 · 暗夜精灵三连发","白色能量满 → 三炮齐射模式"},
            {"n29_ch2_lab.sav",          "第二章 · 中央研究室",      "圆形研究室，暗夜精灵号停在基座"},
            {"n30_ch2_lab_upgrade.sav",  "第二章 · 研究室升级",      "升级动画中（白色粒子汇聚）"},
            {"n31_ch2_boss_intro.sav",   "第二章 · Warden 登场",     "章节 Boss MOONWELL WARDEN 降下"},
            {"n32_ch2_boss_fight.sav",   "第二章 · Warden 战斗",     "满血弹幕循环（放射/瞄准/螺旋）"},
            {"n33_ch2_boss_enraged.sav", "第二章 · Warden 狂暴",     "半血以下 ENRAGED，弹幕更密"},
            {"n34_ch2_boss_dying.sav",   "第二章 · Warden 击破",     "链式爆炸演出 → 结局旁白"}
        };
        return t;
    }

    // 第一章节点：按分数预填充"分数触发"的对话历史（旧 TEST 模式做法，测试菜单与节点生成器共用）
    void prepopulateCh1History(int score) {
        if (chapterMgr.getConfig().chapterNumber != 1 || score <= 0) return;
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

    // ---- 节点构建小工具 ----
    // 单帧推进：必须与主循环同序（背景先更新再 updateGameplay），否则侧滚背景不会滚动，
    // 球体 Boss 会永远卡在 ENTERING（背景停住 = 世界坐标不推进）。
    void nodeStep(const Uint8* keys) {
        if (background) background->update();
        if (sideBg) sideBg->update();
        updateGameplay(keys);
    }
    void nodeRun(const std::vector<Uint8>& keys, int frames) {
        for (int i = 0; i < frames; ++i) nodeStep(keys.data());
    }
    template <class Pred>
    bool nodeRunUntil(const std::vector<Uint8>& keys, int maxFrames, Pred done) {
        for (int i = 0; i < maxFrames; ++i) {
            nodeStep(keys.data());
            if (done()) return true;
        }
        return false;
    }
    void nodeResetCh1(int score) {
        chapterMgr.selectChapter(0);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        bulletMgr.updateParams(score / 30);
        shockwaveMgr.updateParams(score / 30);
        this->score = score;
        phase = PHASE_PLAY;
        if (score == 0) lastScore = -1;                     // 让分数 0 的开场对话照常触发
        else { lastScore = score; enemiesEnabled = true; shockwaveMgr.setPending(true); }
        prepopulateCh1History(score);
    }
    void nodeResetCh2() {
        chapterMgr.selectChapter(1);
        resetGame();
        atStartScreen = false; isNormalPlay = true;
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        bulletMgr.updateParams(1);
        shockwaveMgr.updateParams(1);
        player = &ch2Trainer;
        ch2Trainer.reset();
    }
    // 旧 TEST 模式的手工外星飞船布场（入场动画/无敌标记与正常出怪一致）
    void nodeSpawnCh1Aliens(int n) {
        for (int i = 0; i < n; ++i) {
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
    }
    void nodeHealCh2() { ch2PlayerHP = 3; ch2GameOver = false; gameOver = false; }

    // ---- 摆出第 i 个节点的状态 ----
    void buildNodeState(int i) {
        std::vector<Uint8> k(SDL_NUM_SCANCODES, 0);       // 空按键（玩家静止）
        std::vector<Uint8> fire(SDL_NUM_SCANCODES, 0);    // 按住射击
        fire[SDL_SCANCODE_SPACE] = 1;
        std::vector<Uint8> shift(SDL_NUM_SCANCODES, 0);   // 按住 Shift（技能球吸收）
        shift[SDL_SCANCODE_LSHIFT] = 1;

        switch (i) {
            // ---------- 第一章 ----------
            case 0:   // 开场（分数 0，开场对话中）
                nodeResetCh1(0);
                nodeRun(k, 70);
                break;
            case 1: case 2: case 3: case 4: case 5: case 6: {   // 分数 30/60/90/120/150/180
                static const int sc[6] = {30, 60, 90, 120, 150, 180};
                nodeResetCh1(sc[i - 1]);
                nodeRun(fire, 120);       // 打一会儿：画面里有子弹/敌机/冲击波
                break;
            }
            case 7:   // Boss 登场动画
                nodeResetCh1(200);
                boss.trigger();
                phase = PHASE_BOSS_INTRO;
                alienMgr.setAllInvincible();
                nodeSpawnCh1Aliens(5);
                nodeRun(k, 55);           // 停在下降途中
                break;
            case 8:   // Boss 一阶段（满血 + 治疗波）
                nodeResetCh1(200);
                boss.setY(90); boss.hpRef() = 1000; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true); boss.enteringRef() = false;
                boss.phase2TriggeredRef() = false; boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_FIGHT;
                boss.setCh1HealWavesEnabled(true);
                shockwaveMgr.setPending(true);
                nodeSpawnCh1Aliens(3);
                nodeRun(fire, 150);
                break;
            case 9:   // 二阶段 · 吸收小怪回血（旧 "BOSS PH.2" 的原型，带怪）
                nodeResetCh1(200);
                boss.setY(90); boss.hpRef() = 520; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true); boss.enteringRef() = false;
                boss.phase2TriggeredRef() = true; boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_PHASE2;
                boss.setCh1HealWavesEnabled(false);
                nodeSpawnCh1Aliens(5);
                nodeRun(k, 110);          // 吸收状态机跑起来（蓝色吸收光束）
                break;
            case 10:  // 二阶段 · 吸收结束后的半血持续战斗
                // 注：正常流程里 PHASE_BOSS_PHASE2 是"吸收小怪"阶段，吸收结束会回到
                // PHASE_BOSS_FIGHT（phase2Triggered 保持 true）——这里就是那个状态
                nodeResetCh1(200);
                boss.setY(90); boss.hpRef() = 500; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true); boss.enteringRef() = false;
                boss.phase2TriggeredRef() = true; boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_FIGHT;
                boss.setCh1HealWavesEnabled(true);
                shockwaveMgr.setPending(true);
                nodeSpawnCh1Aliens(2);
                nodeRun(fire, 90);
                break;
            case 11:  // Boss 剩 1 HP（验证击破流程）
                nodeResetCh1(200);
                boss.setY(90); boss.hpRef() = 1; boss.setMaxHp(1000); boss.bonusHpRef() = 0;
                boss.setActive(true); boss.enteringRef() = false;
                boss.phase2TriggeredRef() = true; boss.flashTimerRef() = 0;
                phase = PHASE_BOSS_FIGHT;
                boss.setCh1HealWavesEnabled(true);
                shockwaveMgr.setPending(true);
                nodeRun(k, 40);
                break;
            case 12:  // 击破演出
                nodeResetCh1(200);
                boss.setY(90); boss.hpRef() = 0; boss.bonusHpRef() = 0;
                boss.setActive(true); boss.enteringRef() = false;
                phase = PHASE_BOSS_DEFEAT;
                bossDefeatTimer = 0;
                nodeSpawnCh1Aliens(3);
                nodeRun(k, 70);           // 链式爆炸中
                break;

            // ---------- 第二章 ----------
            case 13:  // 开场：门禁前对话
                nodeResetCh2();
                nodeRun(k, 40);
                break;
            case 14:  // 门禁 · Ally 脉冲解锁
                nodeResetCh2();
                dialogueSys.reset();
                dGateQueued = true;
                nodeRunUntil(k, 900, [this]{ return gateScene.getStage() == Ch2GateScene::PULSING
                                                  && !gateScene.groups.empty(); });
                nodeRun(k, 10);
                break;
            case 15:  // 门禁 · 扫描锁定（Moonwell AI 对话）
                nodeResetCh2();
                dialogueSys.reset();
                dGateQueued = true;
                nodeRunUntil(k, 1600, [this]{ return gateScene.getStage() == Ch2GateScene::SCANNING; });
                nodeRun(k, 40);
                break;
            case 16:  // 门禁 · 大门开启
                nodeResetCh2();
                dialogueSys.reset();
                dGateQueued = true; dMoonwellQueued = true;
                gateScene.skipToOpen();
                player->setX(520); player->setY(WIN_HEIGHT / 2);
                nodeRun(k, 45);
                break;
            case 17:  // 廊桥飞行（球体出现前）
                nodeResetCh2();
                dialogueSys.reset();
                dGateQueued = true; dMoonwellQueued = true;
                gateScene.skipToOpen();
                player->setX(660); player->setY(WIN_HEIGHT / 2);   // 飞过 x=640 → 白淡入转场
                nodeRunUntil(k, 600, [this]{ return ch2Flow == C2_CORRIDOR; });
                dialogueSys.reset();
                nodeRun(k, 80);
                break;
            case 18:  // 球体 Boss · 滚动入场
                nodeCh2PreSphere(k);
                nodeRunUntil(fire, 900, [this]{ return sphereBossActive
                                                  && sphereBoss.getState() == Ch2SphereBoss::ENTERING; });
                nodeRun(k, 70);
                break;
            case 19:  // 球体 Boss · 激活完成可输出
                nodeCh2PreSphere(k);
                nodeCh2SphereToFight(k);
                nodeRun(fire, 80);
                break;
            case 20:  // 球体 Boss · 碎裂崩塌
                nodeCh2PreSphere(k);
                nodeCh2SphereToFight(k);
                for (int n = 0; n < 40 && sphereBoss.getState() == Ch2SphereBoss::FIGHT; ++n) {
                    sphereBoss.takeDamage(30);
                    nodeRun(k, 6);
                }
                nodeRunUntil(k, 600, [this]{ return sphereBoss.getState() == Ch2SphereBoss::SHATTERING; });
                nodeRun(k, 60);
                break;
            case 21:  // 追逐战 · 第 1 波
                nodeCh2ChaseBase(score);
                autoSpawnPhase = 1; autoSpawnQueued = 3; autoSpawnTimer = 0;
                autoSpawnScoreBase = score; autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
                nodeRun(fire, 110);
                break;
            case 22:  // 追逐战 · 第 3 波 + 增援
                nodeCh2ChaseBase(score);
                autoSpawnPhase = 5; autoSpawnQueued = 5; autoSpawnTimer = 0;
                autoSpawnScoreBase = score; autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
                nodeRun(fire, 240);
                break;
            case 23:  // 弹幕敌人（螺旋弹幕）
                nodeCh2ChaseBase(score);
                dmMgr.spawnEnemy();
                nodeRun(fire, 120);
                break;
            case 24:  // 技能球 · 护罩阶段
                nodeCh2ChaseBase(score);
                skillOrb.spawn(540.0, 220.0);
                pulseOrbDropped = true;
                nodeRun(fire, 70);
                break;
            case 25:  // 技能球 · 吸收充能中
                nodeCh2ChaseBase(score);
                skillOrb.spawn(540.0, 220.0);
                pulseOrbDropped = true;
                for (int n = 0; n < 18; ++n) skillOrb.registerHit(particleMgr, audio);   // 直接打掉护罩
                pulseSystem.energy = 0;            // 吸收前提：绿条必须是空的
                pulseSystem.draining = false;
                player->setX((int)skillOrb.x - 70); player->setY((int)skillOrb.y);
                nodeRun(shift, 70);
                nodeHealCh2();
                break;
            case 26:  // 脉冲能量满
                nodeCh2ChaseBase(score);
                pulseSystem.unlocked = true;
                pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;
                ch2AlienMgr.forceSpawn(); ch2AlienMgr.forceSpawn(); ch2AlienMgr.forceSpawn();
                dmMgr.spawnEnemy();
                nodeRun(k, 55);
                nodeHealCh2();
                break;
            case 27:  // 暗夜精灵号 · 白色能量满（三连发）
                nodeCh2ChaseBase(score);
                player = &nightElf;
                nightElf.reset();
                nightElf.setX(140); nightElf.setY(300);
                pulseSystem.unlocked = true;
                pulseSystem.energy = 12;
                nightElfEnergy.reset();
                nightElfEnergy.setEnergy(NightElfEnergy::MAX_ENERGY);
                nightElfEnergy.checkTripleTrigger();
                ch2AlienMgr.forceSpawn(); ch2AlienMgr.forceSpawn(); ch2AlienMgr.forceSpawn();
                nodeRun(fire, 70);
                nodeHealCh2();
                break;
            case 28:  // 中央研究室（抵达）
                nodeCh2LabBase();
                nodeRun(k, 45);
                break;
            case 29:  // 研究室 · 升级动画中
                nodeCh2LabBase();
                player->setX(Ch2LabScene::PARK_X); player->setY(Ch2LabScene::PARK_Y);
                nodeRunUntil(k, 300, [this]{ return labUpgradeState == 1; });
                nodeRun(k, 25);
                break;
            case 30:  // Warden 登场
                nodeCh2BossBase(false);
                nodeRun(k, 55);
                break;
            case 31:  // Warden 战斗
                nodeCh2BossBase(false);
                nodeCh2WardenToFight(k);
                nodeRun(fire, 110);
                nodeHealCh2();
                break;
            case 32:  // Warden 狂暴
                nodeCh2BossBase(false);
                nodeCh2WardenToFight(k);
                wardenBoss.hp = Ch2WardenBoss::ENRAGE_HP + 5;
                wardenBoss.takeDamage(10);                     // → ENRAGED
                nodeRun(fire, 90);
                nodeHealCh2();
                break;
            case 33:  // Warden 击破演出
                nodeCh2BossBase(false);
                nodeCh2WardenToFight(k);
                wardenBoss.hp = 5;
                wardenBoss.takeDamage(10);                     // → DYING（未到结局旁白）
                nodeRun(k, 40);
                nodeHealCh2();
                break;
        }
    }

    // 第二章公共底座：门禁前 → 廊桥 → 球体出现（供球体/追逐/技能球节点复用）
    void nodeCh2PreSphere(const std::vector<Uint8>& keys) {
        nodeResetCh2();
        dialogueSys.reset();
        dGateQueued = true; dMoonwellQueued = true;
        gateScene.skipToOpen();
        player->setX(660); player->setY(WIN_HEIGHT / 2);   // 飞过 x=640 → 白淡入转场
        nodeRunUntil(keys, 600, [this]{ return ch2Flow == C2_CORRIDOR; });
        dialogueSys.reset();
        dChaseQueued = true;
    }
    void nodeCh2SphereToFight(const std::vector<Uint8>& keys) {
        nodeRunUntil(keys, 2400, [this]{ return sphereBossActive
                                            && sphereBoss.getState() == Ch2SphereBoss::FIGHT; });
    }
    // 第二章追逐战底座：跳过门禁与球体，直接进入追逐段
    void nodeCh2ChaseBase(int sc) {
        nodeResetCh2();
        dialogueSys.reset();
        dGateQueued = true; dMoonwellQueued = true; dSphereIntroQueued = true;
        dSphereActQueued = true; dChaseQueued = true;
        gateScene.skipToOpen();
        sphereBossActive = false;
        sphereBoss.reset();
        ch2Flow = C2_CHASE;
        autoSpawnPhase = 0; autoSpawnQueued = 0; autoSpawnTimer = 0;
        score = sc;
        player = &nightElf;
        nightElf.reset();
        nightElf.setX(140); nightElf.setY(300);
        pulseSystem.unlocked = true;
        pulseSystem.energy = 8;
        autoSpawnScoreBase = score; autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
    }
    void nodeCh2LabBase() {
        nodeResetCh2();
        dialogueSys.reset();
        dGateQueued = true; dMoonwellQueued = true; dSphereIntroQueued = true;
        dSphereActQueued = true; dChaseQueued = true; dLabQueued = false;
        gateScene.skipToOpen();
        sphereBossActive = false;
        sphereBoss.reset();
        ch2Flow = C2_LAB;
        ch2LabInitDone = false;
        autoSpawnPhase = 0;
        score = 25;
        player = &ch2Trainer;
        ch2Trainer.reset();
        player->setX(300); player->setY(Ch2LabScene::PARK_Y);
        pulseSystem.unlocked = true;
        pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;
    }
    void nodeCh2BossBase(bool keepDialogue) {
        nodeResetCh2();
        if (!keepDialogue) dialogueSys.reset();
        dGateQueued = true; dMoonwellQueued = true; dSphereIntroQueued = true;
        dSphereActQueued = true; dChaseQueued = true; dLabQueued = true;
        dUpgradeQueued = true; dBossWarnQueued = true;
        gateScene.skipToOpen();
        sphereBossActive = false;
        sphereBoss.reset();
        ch2Flow = C2_BOSS;
        score = 25;
        player = &nightElf;
        nightElf.reset();
        nightElf.setX(150); nightElf.setY(300);
        pulseSystem.unlocked = true;
        pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;
        autoSpawnPhase = 6;                 // 右侧能量墙重新开启
        wardenBoss.startEntering();
    }
    void nodeCh2WardenToFight(const std::vector<Uint8>& keys) {
        nodeRunUntil(keys, 1200, [this]{ return wardenBoss.getState() == Ch2WardenBoss::FIGHT; });
    }

    // ---- 节点状态自校验：确认 buildNodeState() 真的把游戏摆到了该节点 ----
    // 返回 nullptr 表示通过，否则返回失败原因（写进日志，避免"生成成功但节点内容不对"）
    const char* nodeVerify(int i, char* buf, size_t n) {
        switch (i) {
            case 0:  return (score == 0 && dialogueSys.isActive()) ? nullptr : "expect ch1 opening dialogue";
            case 1:  return (score == 30)  ? nullptr : "expect score 30";
            case 2:  return (score == 60)  ? nullptr : "expect score 60";
            case 3:  return (score == 90)  ? nullptr : "expect score 90";
            case 4:  return (score == 120) ? nullptr : "expect score 120";
            case 5:  return (score == 150) ? nullptr : "expect score 150";
            case 6:  return (score == 180) ? nullptr : "expect score 180";
            case 7:  if (phase == PHASE_BOSS_INTRO) return nullptr;
                     snprintf(buf, n, "expect INTRO (phase=%d)", (int)phase); return buf;
            case 8:  if (phase == PHASE_BOSS_FIGHT) return nullptr;
                     snprintf(buf, n, "expect FIGHT (phase=%d hp=%d)", (int)phase, boss.getHp()); return buf;
            case 9:  if (phase == PHASE_BOSS_PHASE2) return nullptr;
                     snprintf(buf, n, "expect PHASE2 absorb (phase=%d hp=%d)", (int)phase, boss.getHp()); return buf;
            case 10: if (phase == PHASE_BOSS_FIGHT && boss.isPhase2Triggered()) return nullptr;
                     snprintf(buf, n, "expect phase2 fight (phase=%d p2=%d hp=%d)", (int)phase,
                              boss.isPhase2Triggered() ? 1 : 0, boss.getHp()); return buf;
            case 11: if (phase == PHASE_BOSS_FIGHT && boss.getHp() <= 1) return nullptr;
                     snprintf(buf, n, "expect boss hp1 (phase=%d hp=%d)", (int)phase, boss.getHp()); return buf;
            case 12: if (phase == PHASE_BOSS_DEFEAT) return nullptr;
                     snprintf(buf, n, "expect DEFEAT (phase=%d)", (int)phase); return buf;
            case 13: return (ch2Flow == C2_GATE && dialogueSys.isActive()) ? nullptr : "expect gate dialogue";
            case 14: if (gateScene.getStage() == Ch2GateScene::PULSING) return nullptr;
                     snprintf(buf, n, "expect PULSING (stage=%d groups=%d)", (int)gateScene.getStage(), (int)gateScene.groups.size()); return buf;
            case 15: if (gateScene.getStage() == Ch2GateScene::SCANNING) return nullptr;
                     snprintf(buf, n, "expect SCANNING (stage=%d)", (int)gateScene.getStage()); return buf;
            case 16: if (gateScene.isOpen()) return nullptr;
                     snprintf(buf, n, "expect OPEN (stage=%d door=%.2f)", (int)gateScene.getStage(), gateScene.doorOpen); return buf;
            case 17: if (ch2Flow == C2_CORRIDOR && !sphereBossActive) return nullptr;
                     snprintf(buf, n, "expect CORRIDOR (flow=%d sphereActive=%d)", ch2Flow, sphereBossActive ? 1 : 0); return buf;
            case 18: if (sphereBossActive && sphereBoss.getState() == Ch2SphereBoss::ENTERING) return nullptr;
                     snprintf(buf, n, "expect sphere ENTERING (active=%d state=%d flow=%d)", sphereBossActive ? 1 : 0, (int)sphereBoss.getState(), ch2Flow); return buf;
            case 19: if (sphereBoss.getState() == Ch2SphereBoss::FIGHT) return nullptr;
                     snprintf(buf, n, "expect sphere FIGHT (state=%d flow=%d)", (int)sphereBoss.getState(), ch2Flow); return buf;
            case 20: if (sphereBoss.getState() >= Ch2SphereBoss::SHATTERING &&
                         sphereBoss.getState() <= Ch2SphereBoss::SHAKING) return nullptr;
                     snprintf(buf, n, "expect sphere SHATTER (state=%d)", (int)sphereBoss.getState()); return buf;
            case 21: if (ch2Flow == C2_CHASE && ch2AlienMgr.countLiving() > 0) return nullptr;
                     snprintf(buf, n, "expect wave1 aliens (flow=%d alive=%d)", ch2Flow, ch2AlienMgr.countLiving()); return buf;
            case 22: if (ch2Flow == C2_CHASE && ch2AlienMgr.countLiving() >= 3) return nullptr;
                     snprintf(buf, n, "expect wave3 aliens (flow=%d alive=%d)", ch2Flow, ch2AlienMgr.countLiving()); return buf;
            case 23: if (!dmMgr.getEnemies().empty()) return nullptr;
                     snprintf(buf, n, "expect danmaku enemy (n=%d)", (int)dmMgr.getEnemies().size()); return buf;
            case 24: if (skillOrb.state == Ch2SkillOrb::FLOATING) return nullptr;
                     snprintf(buf, n, "expect orb FLOATING (state=%d shield=%d)", (int)skillOrb.state, skillOrb.shieldHp); return buf;
            case 25: if (skillOrb.state == Ch2SkillOrb::ABSORBING && pulseSystem.energy > 0) return nullptr;
                     snprintf(buf, n, "expect orb ABSORBING (state=%d timer=%d energy=%d)", (int)skillOrb.state, skillOrb.absorbTimer, pulseSystem.energy); return buf;
            case 26: if (pulseSystem.isFull()) return nullptr;
                     snprintf(buf, n, "expect pulse FULL (energy=%d unlocked=%d)", pulseSystem.energy, pulseSystem.unlocked ? 1 : 0); return buf;
            case 27: if (player == (Player*)&nightElf && nightElfEnergy.isTripleActive()) return nullptr;
                     snprintf(buf, n, "expect nightelf TRIPLE (nightelf=%d triple=%d)", player == (Player*)&nightElf ? 1 : 0, nightElfEnergy.isTripleActive() ? 1 : 0); return buf;
            case 28: if (ch2Flow == C2_LAB && labUpgradeState == 0) return nullptr;
                     snprintf(buf, n, "expect LAB arrival (flow=%d upgrade=%d)", ch2Flow, labUpgradeState); return buf;
            case 29: if (ch2Flow == C2_LAB && labUpgradeState == 1) return nullptr;
                     snprintf(buf, n, "expect LAB upgrade anim (flow=%d upgrade=%d t=%d)", ch2Flow, labUpgradeState, labUpgradeTimer); return buf;
            case 30: if (wardenBoss.getState() == Ch2WardenBoss::ENTERING) return nullptr;
                     snprintf(buf, n, "expect warden ENTERING (state=%d)", (int)wardenBoss.getState()); return buf;
            case 31: if (wardenBoss.getState() == Ch2WardenBoss::FIGHT) return nullptr;
                     snprintf(buf, n, "expect warden FIGHT (state=%d hp=%d)", (int)wardenBoss.getState(), wardenBoss.getHp()); return buf;
            case 32: if (wardenBoss.getState() == Ch2WardenBoss::ENRAGED) return nullptr;
                     snprintf(buf, n, "expect warden ENRAGED (state=%d hp=%d)", (int)wardenBoss.getState(), wardenBoss.getHp()); return buf;
            case 33: if (wardenBoss.getState() == Ch2WardenBoss::DYING && !wardenBoss.isDefeated()) return nullptr;
                     snprintf(buf, n, "expect warden DYING (state=%d defeated=%d)", (int)wardenBoss.getState(), wardenBoss.isDefeated() ? 1 : 0); return buf;
        }
        return "unknown node";
    }

    // ---- 生成全部节点存档 ----
    int generateNodeSaves(const std::string& dir) {
        SaveSystem::ensureDir(dir);
        const NodeDef* table = nodeTable();
        std::string readme;
        readme += "STAR FOX SPACE SHOOTER — 节点存档说明\n";
        readme += "=====================================\n\n";
        readme += "这些存档由 ./shooter --mknodes 自动生成，等价于以前的 TEST 模式跳关，\n";
        readme += "但状态是完整合法的（含飞行中的子弹/敌人/对话进度）。\n\n";
        readme += "读取方式：主菜单 → LOAD GAME → BROWSE FILE... → 选 saves/ 里的对应文件 →\n";
        readme += "确认读档 → 画面停在存档瞬间 → 选 RESUME → 3-2-1 倒计时后继续。\n\n";
        readme += "游戏更新后想刷新全部节点：在源码目录执行 ./shooter --mknodes\n\n";
        readme += "小技巧：把某个 nXX_*.sav 复制/改名为 slot1.sav ~ slot6.sav，它就会直接出现在\n";
        readme += "        LOAD GAME 的槽位列表里（不用每次翻文件夹）。AUTO 槽请留给自动存档。\n\n";
        readme += "注意：读档会覆盖当前进度，读之前游戏会二次确认。\n\n";
        readme += "----------------------------------------------------------------\n";
        int okCount = 0, failCount = 0;
        for (int i = 0; i < NODE_COUNT; ++i) {
            buildNodeState(i);
            // 1) 状态自校验：确认摆到了目标节点
            char vbuf[160] = {0};
            const char* vfail = nodeVerify(i, vbuf, sizeof(vbuf));
            // 2) 落盘
            std::string path = dir + "/" + table[i].file;
            std::string err;
            bool ok = saveToPath(path, err);
            // 3) 回读摘要：确认文件可读且章节/分数与预期一致
            SaveMeta chk; std::string merr;
            bool metaOk = ok && SaveSystem::readMeta(path, chk, merr);
            if (ok && metaOk && !vfail) okCount++; else failCount++;
            std::string flags = std::string(vfail ? "STATE-BAD " : "state-ok ") +
                                (ok ? (metaOk ? "file-ok" : "META-BAD") : "WRITE-FAIL");
            printf("[nodes] %2d/%-2d %s%s%s\n", i + 1, NODE_COUNT,
                   padTo(table[i].file, 30).c_str(), padTo(table[i].title, 24).c_str(),
                   (flags + (vfail ? ("  <- " + std::string(vfail)) : "")).c_str());
            if (!ok && !err.empty()) printf("[nodes]        write error: %s\n", err.c_str());
            readme += padTo(table[i].file, 30) + padTo(table[i].title, 24) + table[i].desc + "\n";
        }
        // ---- 回读验证：逐个真正读档 + 渲染一帧 + 复检状态（确保文件拿来就能用）----
        int loadOk = 0, loadFail = 0;
        printf("[nodes] reload check:\n");
        for (int i = 0; i < NODE_COUNT; ++i) {
            std::string path = dir + "/" + table[i].file;
            std::string lerr;
            bool loaded = loadFromPath(path, lerr);
            char lbuf[160] = {0};
            const char* lvfail = loaded ? nodeVerify(i, lbuf, sizeof(lbuf)) : "load failed";
            std::vector<uint32_t> px;
            bool rendered = loaded && renderToPixels(px);
            // 画面非空校验：读档后那一帧必须真的有内容（防止"能读但黑屏"）
            int ink = 0;
            for (size_t k = 0; k < px.size(); ++k) if ((px[k] & 0x00FFFFFFu) != 0) ink++;
            double inkPct = px.empty() ? 0.0 : 100.0 * ink / (double)px.size();
            bool hasInk = (inkPct > 0.05);
            if (!hasInk) lvfail = "frame is blank";
            if (loaded && rendered && hasInk && !lvfail) loadOk++;
            else {
                loadFail++;
                printf("[nodes]   RELOAD-BAD %-28s %s%s%s\n", table[i].file,
                       loaded ? "" : ("load: " + lerr).c_str(),
                       lvfail ? lvfail : "", rendered ? "" : " render failed");
            }
            printf("[nodes]   reload %2d %s ink=%.1f%%\n", i + 1, padTo(table[i].file, 30).c_str(), inkPct);
        }
        printf("[nodes] reload: %d ok, %d bad\n", loadOk, loadFail);
        failCount += loadFail;

        readme += "----------------------------------------------------------------\n";
        char tail[256];
        snprintf(tail, sizeof(tail), "\n生成结果：成功 %d 个，失败 %d 个（含回读验证）。\n", okCount, failCount);
        readme += tail;
        FILE* f = fopen((dir + "/NODES_README.txt").c_str(), "wb");
        if (f) { fwrite(readme.c_str(), 1, readme.size(), f); fclose(f); }
        printf("[nodes] %d ok, %d failed -> %s\n", okCount, failCount, dir.c_str());
        printf("[nodes] 说明文件: %s/NODES_README.txt\n", dir.c_str());
        return failCount == 0 ? 0 : 1;
    }


private:
    // ======== START SCREEN ========
    // 菜单项：正常构建 = PLAY / CHAPTER / LOAD GAME / OPTIONS / EXIT
    //           --test 开发构建额外保留 TEST（[DORMANT — 激活条件：命令行参数 --test]）
    enum StartItem { SI_PLAY, SI_CHAPTER, SI_TEST, SI_LOAD, SI_OPTIONS, SI_EXIT, SI_COUNT_MAX };
    int startItemCount;
    int startItemId[6];

    void buildStartMenu() {
        int n = 0;
        startItemId[n++] = SI_PLAY;
        startItemId[n++] = SI_CHAPTER;
        if (devMode) startItemId[n++] = SI_TEST;      // [DORMANT — 仅 --test 构建可见]
        startItemId[n++] = SI_LOAD;
        startItemId[n++] = SI_OPTIONS;
        startItemId[n++] = SI_EXIT;
        startItemCount = n;
    }
    static const char* startItemLabel(int id) {
        switch (id) {
            case SI_PLAY:    return "PLAY";
            case SI_CHAPTER: return "CHAPTER";
            case SI_TEST:    return "TEST";
            case SI_LOAD:    return "LOAD GAME";
            case SI_OPTIONS: return "OPTIONS";
            default:         return "EXIT";
        }
    }

    void updateStartScreen(const Uint8* keys, bool& running) {
        static bool sJustEntered = true;
        if (startItemCount == 0) buildStartMenu();
        bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];

        if (sJustEntered) {
            upWas = upNow; downWas = downNow; enterWas = enterNow;
            sJustEntered = false;
        }
        if (upNow && !upWas && startMenuSelection > 0) { startMenuSelection--; audio.sndMenuMove(); }
        if (downNow && !downWas && startMenuSelection < startItemCount - 1) { startMenuSelection++; audio.sndMenuMove(); }
        if (enterNow && !enterWas) {
            audio.sndMenuConfirm();
            int act = startItemId[startMenuSelection];
            if (act == SI_PLAY) {
                resetGame(); atStartScreen = false;
                isNormalPlay = true;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                startChapterNarration();
                pendingAutoSave = true;      // 章节起点写入 AUTO 槽
                sJustEntered = true;
            } else if (act == SI_CHAPTER) {
                atStartScreen = false; atChapterSelect = true;
                chapterSelection = 0; sJustEntered = true;
            } else if (act == SI_TEST) {
                atStartScreen = false; atTestSelect = true;
                testScoreSelection = 0; testChapterSelection = 0;
                testAtChapterSelect = true; sJustEntered = true;
            } else if (act == SI_LOAD) {
                refreshSlotMeta();
                atStartScreen = false; atSaveLoadScreen = true;
                saveMenuFromPause = false; saveMenu.openLoad();
                upWas = downWas = enterWas = escWas = true;
                sJustEntered = true;
            } else if (act == SI_OPTIONS) {
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

        ArtFX::BlendGuard guard(r);
        // 顶部/底部仪器框：给极简标题画面一个"驾驶舱终端"的舞台。
        ArtFX::bracket(r, 30, 24, WIN_WIDTH - 60, WIN_HEIGHT - 48, 26, 72, 190, 116, 120);

        // 标题：深色投影 + 双层荧光，形成明确的视觉中心。
        int titleW = 8 * 6 * 5;
        int titleX = CENTER_X - titleW / 2;
        font.drawStringColor(r, "STAR FOX", titleX + 3, 61, 5, 14, 56, 39, 220);
        font.drawStringColor(r, "STAR FOX", titleX, 58, 5, 232, 248, 238, 255);
        int subW = 13 * 6 * 3;
        int subX = CENTER_X - subW / 2;
        font.drawStringColor(r, "SPACE SHOOTER", subX, 116, 3, 184, 255, 207, 235);

        // 分隔线中央留一个菱形准星。
        SDL_SetRenderDrawColor(r, 54, 168, 108, 180);
        SDL_RenderDrawLine(r, 196, 166, 389, 166);
        SDL_RenderDrawLine(r, 411, 166, 604, 166);
        SDL_Point dia[4] = {{400, 161}, {406, 166}, {400, 171}, {394, 166}};
        SDL_SetRenderDrawColor(r, 142, 255, 184, 220);
        SDL_RenderDrawLines(r, dia, 4);
        SDL_RenderDrawLine(r, dia[3].x, dia[3].y, dia[0].x, dia[0].y);

        // 右侧缓慢旋转的战术准星，增加菜单层的纵深与生命感。
        {
            int cx = 688, cy = 366;
            int tick = (int)(SDL_GetTicks() / 40);
            for (int ring = 0; ring < 2; ++ring) {
                int rr = 42 + ring * 13;
                int alpha = ring == 0 ? 48 : 30;
                int px = cx + rr, py = cy;
                for (int i = 1; i <= 48; ++i) {
                    double a = 2.0 * M_PI * i / 48.0 + tick * 0.004 * (ring ? -1 : 1);
                    int nx = cx + (int)(rr * std::cos(a));
                    int ny = cy + (int)(rr * std::sin(a));
                    if (i % 6) {
                        SDL_SetRenderDrawColor(r, 54, 184, 118, (Uint8)alpha);
                        SDL_RenderDrawLine(r, px, py, nx, ny);
                    }
                    px = nx; py = ny;
                }
            }
            SDL_SetRenderDrawColor(r, 90, 232, 151, 75);
            SDL_RenderDrawLine(r, cx - 68, cy, cx - 18, cy);
            SDL_RenderDrawLine(r, cx + 18, cy, cx + 68, cy);
            SDL_RenderDrawLine(r, cx, cy - 68, cx, cy - 18);
            SDL_RenderDrawLine(r, cx, cy + 18, cx, cy + 68);
            ArtFX::starGlint(r, cx, cy, 2, 110, 255, 172, 110);
        }

        if (startItemCount == 0) buildStartMenu();
        const int MENU_Y0 = 200, GAP = 45;
        int maxW = 0;
        for (int i = 0; i < startItemCount; ++i) {
            int w = (int)strlen(startItemLabel(startItemId[i])) * 6 * 4;
            if (w > maxW) maxW = w;
        }
        // 一整块切角玻璃菜单底板：选中项在其内部形成一层更亮的浮起面板。
        int menuPanelW = maxW + 112;
        int menuPanelH = (startItemCount - 1) * GAP + 62;
        ArtFX::techPanel(r, CENTER_X - menuPanelW / 2, MENU_Y0 - 16,
                         menuPanelW, menuPanelH, 3, 11, 12, 47, 150, 98, 235);
        for (int i = 0; i < startItemCount; ++i) {
            const char* label = startItemLabel(startItemId[i]);
            int itemW = (int)strlen(label) * 6 * 4;
            int itemX = CENTER_X - itemW / 2;
            int itemY = MENU_Y0 + i * GAP;
            bool sel = (i == startMenuSelection);
            if (sel) {
                ArtFX::techPanel(r, CENTER_X - maxW / 2 - 30, itemY - 8,
                                 maxW + 60, 42, 4, 18, 17, 84, 234, 148, 245);
                ArtFX::bracket(r, CENTER_X - maxW / 2 - 38, itemY - 13,
                               maxW + 76, 52, 12, 126, 255, 176, 145);
            }
            font.drawStringColor(r, label, itemX, itemY, 4,
                                 sel ? 238 : 130, sel ? 255 : 167, sel ? 245 : 158,
                                 sel ? 255 : 185);
            if (sel) {
                UIRenderer::drawMenuCursor(r, itemX - 42, itemY + 14, 12);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 33, itemW);
            }
        }

        // 底部主导航提示 + 几何状态灯。保留功能提示，不添加装饰性小字。
        SDL_SetRenderDrawColor(r, 34, 92, 68, 120);
        SDL_RenderDrawLine(r, 64, 486, 736, 486);
        const char* help = "W/S SELECT   ENTER CONFIRM   F2 CRT";
        int helpW = (int)strlen(help) * 6 * 2;
        font.drawStringColor(r, help, CENTER_X - helpW / 2, 500, 2, 118, 183, 149, 220);
        font.drawStringColor(r, "Ver 1.2.28", 55, WIN_HEIGHT - 52, 2, 83, 126, 106, 170);
        SDL_SetRenderDrawColor(r, 83, 238, 141, 195);
        SDL_Rect l1 = {54, 204, 3, 3}; SDL_RenderFillRect(r, &l1);
        SDL_SetRenderDrawColor(r, 238, 164, 76, 160);
        SDL_Rect l2 = {742, 410, 3, 3}; SDL_RenderFillRect(r, &l2);
    }

    // ======== CHAPTER SCREEN ========
    void updateChapterScreen(const Uint8* keys) {
        static bool cJustEntered = true;
        MenuKeys mk(keys);
        if (cJustEntered) { upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc; cJustEntered=false; }
        if (mk.up && !upWas)    { int cs = chapterSelection; while (cs > 0) { cs--; if (chapterMgr.isUnlocked(cs)) { chapterSelection = cs; audio.sndMenuMove(); break; } } }
        if (mk.down && !downWas) { int cs = chapterSelection; while (cs < 4) { cs++; if (chapterMgr.isUnlocked(cs)) { chapterSelection = cs; audio.sndMenuMove(); break; } } }
        if (mk.enter && !enterWas) {
            audio.sndMenuConfirm();
            if (chapterMgr.isUnlocked(chapterSelection)) {
                chapterMgr.selectChapter(chapterSelection);
                resetGame(); atStartScreen = false; atChapterSelect = false;
                isNormalPlay = true;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                startChapterNarration();
                pendingAutoSave = true;      // 章节起点写入 AUTO 槽
            }
        }
        if (mk.esc && !escWas) { audio.sndMenuBack(); atChapterSelect = false; atStartScreen = true; cJustEntered = true; }
        upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc;
    }

    void drawChapterScreen() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        ArtFX::BlendGuard guard(r);
        ArtFX::bracket(r, 30, 24, WIN_WIDTH - 60, WIN_HEIGHT - 48, 24, 64, 188, 118, 112);
        const char* title = "SELECT CHAPTER";
        int tw = (int)strlen(title) * 6 * 4;
        font.drawStringColor(r, title, CENTER_X - tw / 2, 50, 4, 228, 247, 237, 255);
        SDL_SetRenderDrawColor(r, 49, 166, 106, 185);
        SDL_RenderDrawLine(r, CENTER_X - 180, 98, CENTER_X + 180, 98);

        const char* chLabels[5] = {"CHAPTER 1", "CHAPTER 2", "CHAPTER 3", "CHAPTER 4", "CHAPTER 5"};
        const int Y0 = 150, GAP = 56;
        ArtFX::techPanel(r, 92, 128, WIN_WIDTH - 184, 294,
                         3, 11, 12, 47, 150, 98, 235);
        for (int i = 0; i < 5; ++i) {
            bool locked = !chapterMgr.isUnlocked(i);
            bool sel = (i == chapterSelection);
            int itemW = (int)strlen(chLabels[i]) * 6 * 3;
            int itemX = CENTER_X - itemW / 2;
            int itemY = Y0 + i * GAP;
            if (sel) {
                ArtFX::techPanel(r, 108, itemY - 8, WIN_WIDTH - 216, 42,
                                 4, 18, 17, 84, 234, 148, 245);
                ArtFX::bracket(r, 100, itemY - 13, WIN_WIDTH - 200, 52,
                               12, 126, 255, 176, 145);
            } else {
                SDL_SetRenderDrawColor(r, 36, 88, 65, 70);
                SDL_RenderDrawLine(r, 128, itemY + 35, WIN_WIDTH - 128, itemY + 35);
            }
            font.drawStringColor(r, chLabels[i], itemX, itemY, 3,
                                 locked ? 86 : (sel ? 228 : 165),
                                 locked ? 103 : (sel ? 255 : 195),
                                 locked ? 94 : (sel ? 194 : 178),
                                 locked ? 145 : 230);
            if (locked) {
                // 不用装饰性小字“LOCKED”，以简洁锁形图标表达锁定状态。
                int lx = 704, ly = itemY + 4;
                SDL_SetRenderDrawColor(r, 150, 76, 68, 190);
                SDL_Rect body = {lx, ly + 6, 11, 9};
                SDL_RenderDrawRect(r, &body);
                SDL_RenderDrawLine(r, lx + 3, ly + 6, lx + 3, ly + 2);
                SDL_RenderDrawLine(r, lx + 8, ly + 6, lx + 8, ly + 2);
                SDL_RenderDrawLine(r, lx + 3, ly + 2, lx + 8, ly + 2);
                SDL_SetRenderDrawColor(r, 235, 133, 110, 210);
                SDL_Rect key = {lx + 5, ly + 9, 2, 4};
                SDL_RenderFillRect(r, &key);
            }
            if (sel) {
                UIRenderer::drawMenuCursor(r, itemX - 30, itemY + 10, 10);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 26, itemW);
            }
        }
        const char* hint = "W/S:SELECT   ENTER:START   ESC:BACK";
        int hw = (int)strlen(hint) * 12;
        font.drawStringColor(r, hint, CENTER_X - hw / 2, 492, 2, 112, 172, 139, 218);
    }

    // ======== TEST SCREEN ========
    // [DORMANT — 激活条件：命令行参数 --test（由 main.cpp 解析后传入 devMode）]
    // 原开发用测试模式：直接跳转到预设状态（章节/分数/Boss 阶段/Ch2 流程段落）。
    // 正常构建不显示入口，主菜单该项位置由 LOAD GAME 取代；逻辑保持可用，便于开发时跳关复现。
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
            int maxSel = isCh2 ? 5 : 9;
            if (mk.esc && !escWas) { testAtChapterSelect = true; tJustEntered = true; }
            if (mk.up && !upWas && testScoreSelection > 0)     testScoreSelection--;
            if (mk.down && !downWas && testScoreSelection < maxSel) testScoreSelection++;
        if (mk.enter && !enterWas) {
            int savedSel = testScoreSelection;
            if (isCh2) {
                // Chapter 2 sub-menu: six entry points
                resetGame(); atStartScreen = false; atTestSelect = false;
                isNormalPlay = false;
                alienMgr.applyChapterConfig(chapterMgr.getConfig());
                bulletMgr.updateParams(0);
                shockwaveMgr.updateParams(0);
                dmFireCooldown = 0;
                if (savedSel == 0) {
                    // Option 0: Full sphere boss entry (entrance animation → fight → debris → combat)
                    ch2Flow = C2_SPHERE;
                    dSphereIntroQueued = true; dSphereActQueued = true; dChaseQueued = true;
                    sphereBoss.init(sideBg, player);
                    sphereBossActive = true;
                    sphereBoss.startEntering();
                } else if (savedSel == 1) {
                    // Option 1: Skip to combat (boss done, auto-spawn wave 1)
                    ch2Flow = C2_CHASE;
                    dChaseQueued = true;
                    autoSpawnPhase = 1; autoSpawnQueued = 3; autoSpawnTimer = 0;
                } else if (savedSel == 2) {
                    // Option 2: Score 25 + first danmaku spawn (for pulse orb testing)
                    ch2Flow = C2_CHASE;
                    dChaseQueued = true;
                    score = 25; autoSpawnPhase = 6; autoSpawnScoreBase = 0;
                    dmMgr.spawnEnemy();
                } else if (savedSel == 3) {
                    // Option 3: Gate sequence (opening scene: dialogue → pulses → scan → door)
                    // resetGame() already set ch2Flow = C2_GATE — nothing more needed
                } else if (savedSel == 4) {
                    // Option 4: NightElf lab (touch the prototype to upgrade)
                    ch2Flow = C2_LAB;
                    dLabQueued = true; dUpgradeQueued = true;
                    sideBg->setSpeed(0);
                    pulseSystem.unlocked = true;
                    pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;
                } else {
                    // Option 5: Warden boss fight (NightElf + pulse ready)
                    ch2Flow = C2_BOSS;
                    dLabQueued = true; dUpgradeQueued = true; dBossWarnQueued = true;
                    player = &nightElf;
                    nightElf.reset();
                    nightElf.setX(100); nightElf.setY(WIN_HEIGHT / 2);
                    pulseSystem.unlocked = true;
                    pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;
                    autoSpawnPhase = 6;   // energy wall active
                    wardenBoss.startEntering();
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
            prepopulateCh1History(score);   // 按分数预填充对话历史（与节点生成器共用）
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
                const char* labels[6] = {"SPHERE BOSS FULL", "COMBAT ONLY", "PULSE ORB TEST",
                                         "GATE SEQUENCE", "NIGHTELF LAB", "WARDEN BOSS"};
                const int MENU_Y0 = 140, GAP = 52;
                for (int i = 0; i < 6; ++i) {
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

    // ======== SAVE / LOAD ========
    // 存档原理：暂停时把"当前这一帧的全部游戏状态"按固定顺序序列化写入文件；
    // 读档时反序列化回同一批成员，然后强制进入 paused 状态——
    // 于是重绘出来的就是存档瞬间那一帧，玩家按 RESUME → 3-2-1 倒计时 → 从原状态继续。
    // 所有可变量都在 serializeAll() 里列出；字段顺序即字节顺序。
    int playerPlaneIndex() const {
        if (player == (Player*)&nightElf) return 1;
        if (player == (Player*)&ch2Trainer) return 0;
        return 2;   // TrainingPlane（第一章）
    }

    SaveMeta buildMeta() const {
        SaveMeta m;
        m.savedAt = (uint32_t)time(nullptr);
        m.playFrames = playFrames;
        m.chapterIdx = chapterMgr.getCurrentIndex();
        m.setTitle(chapterMgr.getConfig().title);
        m.score = score;
        m.baseHP = baseHP;
        m.playerHP = ch2PlayerHP;
        m.flow = chapterMgr.getConfig().isSideScrolling ? ch2Flow : -1;
        m.plane = playerPlaneIndex();
        m.valid = true;
        return m;
    }

    // 读档：先按存档切章节（决定 isSideScrolling / 背景与难度配置），再恢复其余状态
    void applyMetaToChapter(const SaveMeta& meta) {
        int idx = meta.chapterIdx;
        if (idx < 0) idx = 0;
        if (idx > 4) idx = 4;
        chapterMgr.selectChapter(idx);
        boss.setConfig(&chapterMgr.getConfig().bossConfig);
        alienMgr.applyChapterConfig(chapterMgr.getConfig());
        if (background) { delete background; background = new Ch1Background(chapterMgr.getConfig()); }
    }

    // ---- 全量状态序列化（写=输出缓冲，读=还原成员）----
    template <class Ar> void serializeAll(Ar& ar) {
        SaveMeta meta;
        if (ar.writing) meta = buildMeta();
        ar.ioObj(meta);
        if (ar.bad) return;
        if (!ar.writing) applyMetaToChapter(meta);

        // ---- 玩家机体（visit 是模板无法虚分派，按机体编号显式分发）----
        if (ar.writing) {
            if (meta.plane == 1)      ar.ioObj(nightElf);
            else if (meta.plane == 0) ar.ioObj(ch2Trainer);
            else                      ar.ioObj(trainingPlane);
        } else {
            if (meta.plane == 1)      { ar.ioObj(nightElf);      player = &nightElf; }
            else if (meta.plane == 0) { ar.ioObj(ch2Trainer);    player = &ch2Trainer; }
            else                      { ar.ioObj(trainingPlane); player = &trainingPlane; }
        }

        // ---- 全局标量 ----
        ar.ioNum(score); ar.ioNum(baseHP); ar.ioNum(difficultyTimer);
        ar.ioEnum(phase);
        ar.ioBool(gameOver); ar.ioBool(aimAssistOn);
        ar.ioBool(ch1DialogueDone); ar.ioBool(bossPhase2DialogueTriggered);
        ar.ioBytes(triggeredScores, sizeof(triggeredScores));
        ar.ioNum(baseFireTimer); ar.ioNum(lastScore); ar.ioBool(pauseHistoryFocused);
        ar.ioBool(enemiesEnabled); ar.ioBool(isNormalPlay);
        ar.ioNum(wallFlashTimer); ar.ioNum(wallContactY); ar.ioNum(wallAnimFrame);
        ar.ioNum(menuSelection); ar.ioNum(mcMenuSelection);
        // Ch2
        ar.ioNum(ch2PlayerHP); ar.ioBool(ch2GameOver); ar.ioNum(dmFireCooldown);
        ar.ioBool(sphereBossActive); ar.ioNum(playerHitCount); ar.ioNum(tripleBeepCounter);
        ar.ioBool(pulseOrbDropped); ar.ioBool(shiftWas);
        ar.ioNum(ch2Flow); ar.ioNum(ch2PhaseTimer); ar.ioNum(ch2FadeTimer);
        ar.ioBool(ch2GateInitDone); ar.ioBool(ch2LabInitDone);
        ar.ioNum(labUpgradeState); ar.ioNum(labUpgradeTimer); ar.ioBool(ch2EpilogueStarted);
        ar.ioBool(dGateQueued); ar.ioBool(dMoonwellQueued); ar.ioBool(dCorridorQueued);
        ar.ioBool(dSphereIntroQueued); ar.ioBool(dSphereActQueued);
        ar.ioBool(dChaseQueued); ar.ioBool(dOrbQueued); ar.ioBool(dPulseQueued);
        ar.ioBool(dLabQueued); ar.ioBool(dUpgradeQueued);
        ar.ioBool(dBossWarnQueued); ar.ioBool(dBossEnrageQueued);
        // 自动出敌调度
        ar.ioNum(autoSpawnPhase); ar.ioNum(autoSpawnQueued); ar.ioNum(autoSpawnTimer);
        ar.ioNum(autoSpawnWave3Reinf); ar.ioNum(autoSpawnScoreBase);
        ar.ioNum(autoSpawnAliveLast); ar.ioNum(autoSpawnKillsLast);
        ar.ioNum(lastShockwaveLevel);
        // Ch1 Boss 击破演出
        ar.ioNum(bossDefeatTimer); ar.ioNum(defeatAlienTimer); ar.ioNum(defeatReturnTimer);
        ar.ioNum(defeatFWTimer); ar.ioNum(defeatMCDelay); ar.ioNum(defeatFadeTimer);
        ar.ioBool(missionCompleteShown); ar.ioBool(missionComplete);
        // 语音/旁白
        ar.ioNum(voiceLang); ar.ioNum(lastNarrationPage); ar.ioNum(lastDialogueHash);
        ar.ioBool(currentLineVoiced); ar.ioBool(inNarration);
        ar.ioNum(playFrames);

        // ---- 子系统 ----
        ar.ioObj(bulletMgr);
        ar.ioObj(alienMgr);
        ar.ioObj(particleMgr);
        ar.ioObj(shockwaveMgr);
        ar.ioObj(boss);
        ar.ioObj(floatingTextMgr);
        ar.ioObj(narration);
        ar.ioObj(dialogueSys);
        ar.ioObj(ch2AlienMgr);
        ar.ioObj(dmMgr);
        ar.ioObj(sphereBoss);
        ar.ioObj(nightElfEnergy);
        ar.ioObj(pulseSystem);
        ar.ioObj(skillOrb);
        ar.ioObj(gateScene);
        ar.ioObj(labScene);
        ar.ioObj(wardenBoss);
        ar.ioObj(*background);
        ar.ioObj(*sideBg);
    }

    void refreshSlotMeta() {
        for (int i = 0; i < SaveSystem::SLOT_COUNT; ++i) {
            std::string path = SaveSystem::slotPath(i);
            SaveMeta m;
            std::string err;
            if (SaveSystem::readMeta(path, m, err)) {
                m.broken = false;
                slotMeta[i] = m;
            } else if (SaveSystem::fileExists(path)) {
                // 文件在但读不出：损坏或版本不符 → 列表里明确提示，不要显示成"空槽位"
                m.broken = true;
                slotMeta[i] = m;
            } else {
                slotMeta[i] = SaveMeta();
            }
        }
    }

    bool saveToPath(const std::string& path, std::string& err) {
        if (inNarration) { err = "CANNOT SAVE DURING NARRATION"; return false; }
        SaveArchive ar(true);
        serializeAll(ar);
        if (ar.bad) { err = "SERIALIZE FAILED"; return false; }
        std::vector<unsigned char> file = SaveSystem::pack(ar.buf);
        if (!SaveSystem::writeBytes(path, file)) { err = "WRITE FAILED"; return false; }
        return true;
    }

    bool loadFromPath(const std::string& path, std::string& err) {
        std::vector<unsigned char> file;
        if (!SaveSystem::readBytes(path, file)) { err = "CANNOT READ FILE"; return false; }
        SaveArchive ar(false);
        // 版本 / 长度 / CRC 校验全部通过后才动游戏状态
        if (!SaveSystem::unpack(file, ar, err)) return false;
        serializeAll(ar);
        if (ar.bad) { err = "SAVE DATA CORRUPTED"; return false; }
        finishLoad();
        return true;
    }

    // 读档收尾：定格在"暂停 + 暂停菜单"，玩家 RESUME 后走 3-2-1 倒计时
    void finishLoad() {
        paused = true;
        countdown = -1; countdownFrame = 0;
        pauseMenuSelection = 0;
        pauseHistoryFocused = false;
        dialogueSys.history.resetView();
        atStartScreen = false; atChapterSelect = false; atTestSelect = false;
        atOptionScreen = false; atSoundMenu = false; atSaveLoadScreen = false;
        saveMenuFromPause = true;
        gameOver = false;
        pendingAutoSave = false;
        // 读档瞬间按住的键不允许触发菜单/对话跳过
        upWas = downWas = enterWas = escWas = leftWas = rightWas = true;
        pUpWas = pDownWas = pEnterWas = pLeftWas = pRightWas = true;
        bkspWas = true;
        shiftWas = true; shiftJustPressed = false;
        dialogueSys.suppressEnter();
        narration.suppressEnter();
        if (sphereBossActive) {
            sphereBoss.init(sideBg, player);   // 重新绑定读档后的机体指针
            sphereBoss.normalizeHpToCells();   // 旧档 maxHp=500 → 剩余菱形数
        }
        audio.stopVoice();
    }

    // ---- 存/读档界面 ----
    // 统一入口：必须清掉其它界面标志，否则单帧分派会继续走旧界面（例如还停在主菜单）
    void beginSaveLoadScreen(bool fromPause) {
        atStartScreen = false; atChapterSelect = false; atTestSelect = false;
        atOptionScreen = false; atSoundMenu = false;
        atSaveLoadScreen = true;
        saveMenuFromPause = fromPause;
        upWas = downWas = enterWas = escWas = true;
        bkspWas = true;
    }
    void openSaveScreen(bool fromPause) {
        refreshSlotMeta();
        beginSaveLoadScreen(fromPause);
        saveMenu.openSave();
    }
    void openLoadScreen(bool fromPause) {
        refreshSlotMeta();
        beginSaveLoadScreen(fromPause);
        saveMenu.openLoad();
    }

    void doSaveTo(const std::string& path) {
        std::string err;
        if (saveToPath(path, err)) {
            refreshSlotMeta();
            saveMenu.setStatus("GAME SAVED");
            atSaveLoadScreen = false;
            if (saveMenuFromPause) paused = true;
            else atStartScreen = true;
        } else {
            saveMenu.setStatus("SAVE FAILED: " + err);
        }
    }

    void doLoadFrom(const std::string& path) {
        std::string err;
        if (loadFromPath(path, err)) return;      // finishLoad() 已切到暂停画面
        saveMenu.setStatus("LOAD FAILED: " + err);
    }

    void updateSaveLoadScreen(const Uint8* keys) {
        bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        bool escNow = keys[SDL_SCANCODE_ESCAPE];
        bool bkspNow = keys[SDL_SCANCODE_BACKSPACE];

        bool upP = upNow && !upWas, downP = downNow && !downWas;
        bool enterP = enterNow && !enterWas, escP = escNow && !escWas;
        bool bkspP = bkspNow && !bkspWas;
        upWas = upNow; downWas = downNow; enterWas = enterNow;
        escWas = escNow; bkspWas = bkspNow;

        saveMenu.tickStatus();
        int act = saveMenu.update(upP, downP, enterP, escP, bkspP);

        switch (act) {
            case SaveMenu::ACT_PICK_LOAD: {
                int slot = saveMenu.pickedSlot;
                if (!slotMeta[slot].valid && !slotMeta[slot].broken) { saveMenu.setStatus("SLOT IS EMPTY"); break; }
                pendingSaveAction = 2; pendingSlot = slot;
                saveMenu.askConfirm(slotMeta[slot].broken
                                    ? std::string("THIS FILE MAY BE INCOMPATIBLE. LOAD ANYWAY?")
                                    : std::string("LOAD ") + SaveSystem::slotLabel(slot) +
                                      "? CURRENT PROGRESS WILL BE LOST");
                break;
            }
            case SaveMenu::ACT_PICK_SAVE: {
                int slot = saveMenu.pickedSlot;
                if (slotMeta[slot].valid) {
                    pendingSaveAction = 1; pendingSlot = slot;
                    saveMenu.askConfirm(std::string("OVERWRITE ") + SaveSystem::slotLabel(slot) + "?");
                } else {
                    doSaveTo(SaveSystem::slotPath(slot));
                }
                break;
            }
            case SaveMenu::ACT_BROWSE_LOAD: saveMenu.openBrowser(false); break;
            case SaveMenu::ACT_BROWSE_SAVE: saveMenu.openBrowser(true); break;
            case SaveMenu::ACT_LOAD_FILE:
                if (!SaveSystem::fileExists(saveMenu.pickedPath)) { saveMenu.setStatus("FILE NOT FOUND"); break; }
                pendingSaveAction = 4; pendingPath = saveMenu.pickedPath;
                saveMenu.askConfirm("LOAD THIS FILE? CURRENT PROGRESS WILL BE LOST");
                break;
            case SaveMenu::ACT_SAVE_FILE:
                if (SaveSystem::fileExists(saveMenu.pickedPath)) {
                    pendingSaveAction = 3; pendingPath = saveMenu.pickedPath;
                    saveMenu.askConfirm("OVERWRITE EXISTING FILE?");
                } else {
                    doSaveTo(saveMenu.pickedPath);
                }
                break;
            case SaveMenu::ACT_CONFIRM:
                if (pendingSaveAction == 1)      doSaveTo(SaveSystem::slotPath(pendingSlot));
                else if (pendingSaveAction == 2) doLoadFrom(SaveSystem::slotPath(pendingSlot));
                else if (pendingSaveAction == 3) doSaveTo(pendingPath);
                else if (pendingSaveAction == 4) doLoadFrom(pendingPath);
                pendingSaveAction = 0;
                break;
            case SaveMenu::ACT_CANCEL:
                pendingSaveAction = 0;
                break;
            case SaveMenu::ACT_BACK:
                if (saveMenu.screen == SaveMenu::SCR_BROWSER) {
                    if (saveMenu.browser.saveMode) saveMenu.openSave();
                    else saveMenu.openLoad();
                } else {
                    atSaveLoadScreen = false;
                    if (saveMenuFromPause) { paused = true; countdown = -1; }
                    else atStartScreen = true;
                }
                break;
            default: break;
        }
    }

    void drawSaveLoadScreen() {
        if (saveMenuFromPause) drawGameplayFrame();    // 暂停中的原画面作为背景
        if (saveMenu.screen == SaveMenu::SCR_BROWSER) saveMenu.drawBrowser(renderer.get(), font);
        else saveMenu.drawSlots(renderer.get(), font, slotMeta, saveMenuFromPause);
    }

    // ======== OPTIONS SCREEN ========
    void updateOptionScreen(const Uint8* keys) {
        static bool oJustEntered = true;
        MenuKeys mk(keys);
        if (oJustEntered) { upWas=mk.up; downWas=mk.down; enterWas=mk.enter; escWas=mk.esc; oJustEntered=false; }
        if (mk.up && !upWas && optionCursor > 0)    { optionCursor--; audio.sndMenuMove(); }
        if (mk.down && !downWas && optionCursor < 2) { optionCursor++; audio.sndMenuMove(); }
        if (mk.enter && !enterWas) {
            audio.sndMenuConfirm();
            if (optionCursor == 0) { aimAssistOn = !aimAssistOn; storeSettings(); }
            else if (optionCursor == 1) { voiceLang = (voiceLang + 1) % 3; storeSettings(); }   // 0=中文 1=English 2=OFF
            else if (optionCursor == 2) { atSoundMenu = true; oJustEntered = true; }
        }
        if (mk.esc && !escWas) {
            audio.sndMenuBack();
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
        ArtFX::BlendGuard guard(r);
        ArtFX::bracket(r, 30, 24, WIN_WIDTH - 60, WIN_HEIGHT - 48, 24, 64, 188, 118, 112);
        const char* title = "OPTIONS";
        int tw = (int)strlen(title) * 6 * 4;
        font.drawStringColor(r, title, CENTER_X - tw / 2, 42, 4, 228, 247, 237, 255);
        SDL_SetRenderDrawColor(r, 49, 166, 106, 185);
        SDL_RenderDrawLine(r, CENTER_X - 180, 84, CENTER_X + 180, 84);

        const char* labels[3] = {"AIM ASSIST", "VOICE LANG", "SOUND"};
        const int Y0 = 134, GAP = 76;
        ArtFX::techPanel(r, CENTER_X - 300, 112, 600, 218,
                         3, 11, 12, 47, 150, 98, 235);
        for (int i = 0; i < 3; ++i) {
            int ly = Y0 + i * GAP;
            int lx = CENTER_X - 230;
            bool sel = (i == optionCursor);
            if (sel) {
                ArtFX::techPanel(r, CENTER_X - 284, ly - 11, 568, 46,
                                 4, 18, 17, 84, 234, 148, 245);
                ArtFX::bracket(r, CENTER_X - 292, ly - 15, 584, 54, 12, 126, 255, 176, 145);
            }
            font.drawStringColor(r, labels[i], lx, ly, 3,
                                 sel ? 224 : 158, sel ? 255 : 192, sel ? 237 : 180, 240);
            if (i == 0) {
                int tx = CENTER_X + 120;
                ArtFX::techPanel(r, tx, ly - 3, 70, 28, 3, 13, 12, 65, 204, 121);
                font.drawStringColor(r, aimAssistOn ? "ON" : "OFF", tx + 17, ly + 3, 2,
                                     aimAssistOn ? 126 : 216, aimAssistOn ? 255 : 116,
                                     aimAssistOn ? 171 : 94, 245);
            } else if (i == 1) {
                const char* vlLabels[3] = {"ZH", "EN", "OFF"};
                int tx = CENTER_X + 120;
                ArtFX::techPanel(r, tx, ly - 3, 70, 28, 3, 13, 12, 65, 204, 121);
                font.drawStringColor(r, vlLabels[voiceLang], tx + 22, ly + 3, 2, 168, 230, 255, 240);
            } else {
                font.drawStringColor(r, ">", CENTER_X + 130, ly, 3, 118, 244, 157, 240);
            }
            if (sel) {
                int itemW = (int)strlen(labels[i]) * 6 * 3;
                UIRenderer::drawMenuCursor(r, lx - 22, ly + 10, 10);
                UIRenderer::drawMenuUnderline(r, lx, ly + 25, itemW);
            }
        }
        const char* hint = "W/S:SELECT   ENTER:CONFIRM   ESC:BACK";
        int hw = (int)strlen(hint) * 12;
        font.drawStringColor(r, hint, CENTER_X - hw / 2, 494, 2, 112, 172, 139, 218);
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
        if (mk.up && !upWas && soundCursor > 0)    { soundCursor--; audio.sndMenuMove(); }
        if (mk.down && !downWas && soundCursor < 5) { soundCursor++; audio.sndMenuMove(); }
        if (mk.esc && !escWas)  { audio.sndMenuBack(); atSoundMenu = false; sJustEntered = true; }
        if (mk.enter && !enterWas && soundCursor == 5) { audio.sndMenuBack(); atSoundMenu = false; sJustEntered = true; }

        bool changed = false;
        if (leftNow && !leftWas) {
            if (soundCursor == 0) { audio.adjBgmVolume(-1); changed = true; }
            if (soundCursor == 1) { audio.adjSfxVolume(-1); changed = true; }
            if (soundCursor == 2) { audio.adjEqLow(-1); changed = true; }
            if (soundCursor == 3) { audio.adjEqMid(-1); changed = true; }
            if (soundCursor == 4) { audio.adjEqHigh(-1); changed = true; }
        }
        if (rightNow && !rightWas) {
            if (soundCursor == 0) { audio.adjBgmVolume(1); changed = true; }
            if (soundCursor == 1) { audio.adjSfxVolume(1); changed = true; }
            if (soundCursor == 2) { audio.adjEqLow(1); changed = true; }
            if (soundCursor == 3) { audio.adjEqMid(1); changed = true; }
            if (soundCursor == 4) { audio.adjEqHigh(1); changed = true; }
        }
        if (changed) { audio.sndMenuMove(); storeSettings(); }   // 音量/EQ 改动立即持久化
        upWas=mk.up; downWas=mk.down; escWas=mk.esc; enterWas=mk.enter;
        leftWas=leftNow; rightWas=rightNow;
    }

    void drawSoundMenu() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0); renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        ArtFX::BlendGuard guard(r);
        ArtFX::bracket(r, 30, 24, WIN_WIDTH - 60, WIN_HEIGHT - 48, 24, 64, 188, 118, 112);
        const char* title = "SOUND";
        int tw = (int)strlen(title) * 6 * 4;
        font.drawStringColor(r, title, CENTER_X - tw / 2, 40, 4, 228, 247, 237, 255);
        SDL_SetRenderDrawColor(r, 49, 166, 106, 185);
        SDL_RenderDrawLine(r, CENTER_X - 180, 84, CENTER_X + 180, 84);

        const char* labels[6] = {"BGM VOL", "SFX VOL", "EQ LOW", "EQ MID", "EQ HIGH", "BACK"};
        const int Y0 = 120, GAP = 54;
        ArtFX::techPanel(r, CENTER_X - 306, 102, 612, 332,
                         3, 11, 12, 47, 150, 98, 235);
        for (int i = 0; i < 6; ++i) {
            int ly = Y0 + i * GAP;
            int lx = CENTER_X - 225;
            bool sel = (i == soundCursor);
            if (sel) {
                ArtFX::techPanel(r, CENTER_X - 288, ly - 10, 576, 42,
                                 4, 18, 17, 84, 234, 148, 245);
                ArtFX::bracket(r, CENTER_X - 296, ly - 14, 592, 50, 12, 126, 255, 176, 145);
            }
            font.drawStringColor(r, labels[i], lx, ly, 3,
                                 sel ? 224 : 158, sel ? 255 : 192, sel ? 237 : 180, 240);
            int sx = CENTER_X + 50, sw = 160;
            char vbuf[8] = {0};
            if (i < 5) {
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
                font.drawStringColor(r, vbuf, sx + sw + 14, ly, 2,
                                     sel ? 190 : 123, sel ? 255 : 174, sel ? 213 : 148, 235);
            } else {
                font.drawStringColor(r, ">", sx, ly, 3, sel ? 124 : 68, sel ? 255 : 148,
                                     sel ? 167 : 111, 235);
            }
            if (sel) {
                int itemW = (int)strlen(labels[i]) * 6 * 3;
                UIRenderer::drawMenuCursor(r, lx - 22, ly + 10, 10);
                UIRenderer::drawMenuUnderline(r, lx, ly + 25, itemW);
            }
        }
        const char* hint = "W/S:SELECT   A/D:ADJUST   ENTER/ESC:BACK";
        int hw = (int)strlen(hint) * 12;
        font.drawStringColor(r, hint, CENTER_X - hw / 2, 494, 2, 112, 172, 139, 218);
    }

    // ======== GAMEPLAY UPDATE ========
    void updateGameplay(const Uint8* keys) {
        bool isSide = chapterMgr.getConfig().isSideScrolling;
        if (isSide) {
            // ======== Chapter 2 side-scrolling update ========
            // 转场期间冻结玩家输入与位置：飞机位置由 updateCh2Transition() 接管
            if (ch2FadeTimer <= 0) {
                bool moveLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
                bool moveRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
                bool moveUp    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
                bool moveDown  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
                int px = player->getX(), py = player->getY();
                if (moveLeft)  px -= 6;
                if (moveRight) px += 6;
                if (moveUp)    py -= 6;
                if (moveDown)  py += 6;
                if (px < 10) px = 10;
                if (py < 10) py = 10;
                if (py > WIN_HEIGHT - 16) py = WIN_HEIGHT - 16;
                if (py < 16) py = 16;
                if (px > WIN_WIDTH - player->getNoseOffset() - 4)
                    px = WIN_WIDTH - player->getNoseOffset() - 4;
                // Gate scene: the closed door blocks the plane; once it opens,
                // the plane may fly right into the doorway
                if (ch2Flow == C2_GATE) {
                    double maxGateX = (gateScene.isOpen() || gateScene.doorOpen > 0.5) ? 700.0 : 560.0;
                    if (px > maxGateX) px = maxGateX;
                }
                // 室内门禁：门未开时停在舱壁前，开门后才允许穿过去
                if (ch2Flow == C2_AIRLOCK) {
                    double maxAirX = Ch2AirlockScene::canPass(ch2PhaseTimer) ? 730.0 : 630.0;
                    if (px > maxAirX) px = maxAirX;
                }

                int noseX = px + player->getNoseOffset();
                // Energy wall only during combat (after auto-spawn begins)
                if (autoSpawnPhase > 0 && noseX > 642) {
                    px = 642 - player->getNoseOffset();
                    wallFlashTimer = 28; wallContactY = py; wallAnimFrame++;
                } else if (wallFlashTimer > 0) { wallFlashTimer--; wallAnimFrame++; }

                player->setX(px); player->setY(py);
            }

            bool shoot = (ch2FadeTimer <= 0) && keys[SDL_SCANCODE_SPACE];

            // Shooting (disabled during the gate scene)
            if (dmFireCooldown > 0) dmFireCooldown--;
            if (ch2Flow != C2_GATE && ch2Flow != C2_AIRLOCK && ch2Flow != C2_ENDED && shoot && dmFireCooldown <= 0) {
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

            // ==== Ch2 scripted flow state machine ====
            switch (ch2Flow) {
                case C2_GATE:     updateCh2Gate(); break;
                case C2_CORRIDOR: updateCh2Corridor(); break;
                case C2_SPHERE:   updateCh2Sphere(); break;
                case C2_CHASE:    updateCh2Chase(keys); break;
                case C2_LAB:      updateCh2Lab(); break;
                case C2_BOSS:     updateCh2Boss(); break;
                case C2_AIRLOCK:  updateCh2Airlock(); break;
                case C2_ENDED:    updateCh2Ended(keys); break;
            }

            // ==== Scene transition: fade out → switch → fade in + left fly-in ====
            if (ch2FadeTimer > 0) updateCh2Transition();

            // 研究室的灯/全息动画在 C2_LAB / C2_BOSS / C2_ENDED 持续播放
            if (ch2Flow == C2_LAB || ch2Flow == C2_BOSS || ch2Flow == C2_ENDED)
                labScene.update();

            // ==== Enemies + invincibility (combat flows only) ====
            if (ch2Flow == C2_CHASE || ch2Flow == C2_BOSS) {
                player->updateInvFrames();
                ch2AlienMgr.update(bulletMgr, particleMgr, audio, score, *player, floatingTextMgr, playerHitCount);
                dmMgr.update(bulletMgr, particleMgr, audio, score, *player, floatingTextMgr, playerHitCount);
                if (ch2GameOver) gameOver = true;
            } else {
                player->updateInvFrames();
            }

            // ==== NightElf white energy (post-upgrade) ====
            if (player == &nightElf) {
                nightElf.setTripleFire(nightElfEnergy.isTripleActive());
                nightElfEnergy.update(playerHitCount);
                if (nightElfEnergy.justEnteredTriple()) {
                    audio.sndTripleOn();
                    floatingTextMgr.spawn((float)player->getX(), (float)(player->getY() - 26),
                                          "TRIPLE FIRE!", 255, 255, 255);
                    floatingTextMgr.spawn((float)player->getX() + 1, (float)(player->getY() - 27),
                                          "TRIPLE FIRE!", 0, 0, 0);
                }
                if (nightElfEnergy.isTripleActive()) {
                    int tt = nightElfEnergy.getTripleTimer();
                    if (tt > 0 && tt <= NightElfEnergy::COUNTDOWN_START && tt % 30 == 0)
                        audio.sndTripleCountdown();
                }
            }

            // ==== Shift input + pulse release (any flow, unlocked only) ====
            bool shiftNow = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            shiftJustPressed = (shiftNow && !shiftWas);
            shiftWas = shiftNow;
            if (pulseSystem.unlocked && pulseSystem.isFull() && shiftJustPressed) {
                pulseSystem.release((float)player->getX(), (float)player->getY(), particleMgr, audio);
            }

            // ==== Pulse energy from hits + pulse update + collisions ====
            pulseSystem.addEnergy(playerHitCount);
            playerHitCount = 0;
            pulseSystem.update();
            pulseSystem.collideWithBullets(
                const_cast<std::vector<Ch2EnemyBullet>&>(ch2AlienMgr.getBullets()), particleMgr,
                &playerHitCount);
            pulseSystem.collideWithBullets(
                const_cast<std::vector<Ch2EnemyBullet>&>(dmMgr.getBullets()), particleMgr,
                &playerHitCount);
            pulseSystem.collideWithAliens(ch2AlienMgr, particleMgr, audio, score, playerHitCount);
            pulseSystem.collideWithDanmaku(dmMgr, particleMgr, audio, score, playerHitCount);

            // ==== Dialogue (shared update + character sounds) ====
            updateDialogueCommon();

            floatingTextMgr.update();
            particleMgr.update();
            particleMgr.removeInactive();
            if (aimAssistOn) drawAimAssistSide(true);
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
            updateDialogueCommon();

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
        if (aimAssistOn) drawAimAssist(true);
    }

    // ======== DIALOGUE COMMON (both chapters) ========
    // Plays the dubbed voice clip for a text line (manifest lookup by CRC32).
    // Returns true when a clip was found & started (missing/off → false).
    bool playVoiceForText(const std::string& text) {
        if (voiceLang >= 2) return false;   // OFF: keep letter pops, no voice
        if (text.empty()) return false;
        uint32_t h = AudioEngine::voiceCrc32(text.c_str());
        std::map<uint32_t, std::string>::const_iterator it = voicePaths[voiceLang].find(h);
        if (it == voicePaths[voiceLang].end()) return false;
        audio.stopVoice();   // cut the previous line's clip (fast page-flipping)
        return audio.playVoice(it->second.c_str());
    }

    void updateDialogueCommon() {
        dialogueSys.update(false);  // no ENTER skip for in-game dialogue

        // Voice hook: play the dubbed clip when the current line changes
        if (dialogueSys.isActive()) {
            const std::string& txt = dialogueSys.currentText();
            uint32_t h = AudioEngine::voiceCrc32(txt.c_str());
            if (h != lastDialogueHash) {
                lastDialogueHash = h;
                currentLineVoiced = playVoiceForText(txt);
            }
        } else {
            lastDialogueHash = 0;
            currentLineVoiced = false;
        }

        int ticks = dialogueSys.popTicks();
        const std::string& spk = dialogueSys.currentSpeaker();
        while (ticks-- > 0) {
            if (currentLineVoiced) continue;   // real voice clip → skip synthetic blips
            if (spk.find("Ally") != std::string::npos) audio.sndAllyTalk();
            else if (spk.find("Bryssa") != std::string::npos) audio.sndBryssaTalk();
            else if (spk.find("Tower") != std::string::npos) audio.sndTowerTalk();
            else if (spk.find("Martha") != std::string::npos) audio.sndMarthaTalk();
            else if (spk.find("Moonwell") != std::string::npos) audio.sndMoonwellTalk();
            else if (spk.empty()) audio.sndSystemTalk();
            else audio.sndTeletype();
        }
    }

    // ======== CH2 FLOW: GATE (opening scene at Moonwell's vacuum door) ========
    // 流程段落统一入口：切换段落时顺带标记自动存档（结局段不覆盖 AUTO 槽）
    void enterCh2Flow(int f) {
        if (ch2Flow == f) return;
        ch2Flow = f;
        if (f != C2_ENDED) pendingAutoSave = true;
    }

    // ======== CH2 场景切换：淡出 → 换场 → 淡入 + 飞机从左侧飞入 ========
    // 复用已入档的 ch2FadeTimer，不新增存档字段：
    //   1..22 淡出；24 换场；25..46 淡入；30..96 飞机从左侧飞入
    // 淡出色：门禁/廊桥用白（门外是强光），追逐/研究室用黑（进入室内）。
    enum {
        CH2_FADE_OUT_END = 22,
        CH2_SWITCH_AT    = 24,
        CH2_FADE_IN_END  = 46,
        CH2_FLY_START    = 30,
        CH2_FLY_END      = 96
    };

    bool ch2TransitionActive() const { return ch2FadeTimer > 0; }
    bool ch2FadeIsWhite() const { return ch2Flow == C2_GATE || ch2Flow == C2_CORRIDOR; }

    int ch2TransitionAlpha() const {
        int t = ch2FadeTimer;
        if (t <= 0) return 0;
        if (t <= CH2_FADE_OUT_END) return 255 * t / CH2_FADE_OUT_END;
        if (t <= CH2_SWITCH_AT) return 255;
        if (t <= CH2_FADE_IN_END)
            return 255 * (CH2_FADE_IN_END - t) / (CH2_FADE_IN_END - CH2_SWITCH_AT);
        return 0;
    }

    int ch2EntryX() const {
        // 廊桥从左侧飞入到偏左位置；研究室停在暗夜精灵战机前方
        if (ch2Flow == C2_LAB || ch2Flow == C2_BOSS || ch2Flow == C2_ENDED) return 300;
        return 170;
    }

    void beginCh2Transition() {
        if (ch2FadeTimer <= 0) {
            ch2FadeTimer = 1;
            audio.sndTransitionSwell();
        }
    }

    // 在最暗的一帧完成换场：重置新场景、把飞机放到左侧画面外
    void ch2ApplySceneSwitch() {
        if (ch2Flow == C2_GATE) {
            sideBg->reset();
            player->setX(-40); player->setY(WIN_HEIGHT / 2);
            enterCh2Flow(C2_CORRIDOR);
            ch2PhaseTimer = 0;
            if (!dCorridorQueued) {
                dCorridorQueued = true;
                dialogueSys.queueDialogue("Martha", "We're in. The whole facility is silent.");
                dialogueSys.queueDialogue("Bryssa", "It feels like no one has been here for years.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Main power is online. No life signs detected.");
                dialogueSys.queueDialogue("Martha", "Stay sharp. We came here for the comms equipment.");
                dialogueSys.start();
            }
        } else if (ch2Flow == C2_CHASE || ch2Flow == C2_AIRLOCK) {
            ch2AlienMgr.clearBullets();
            dmMgr.clearBullets();
            player->setX(-40); player->setY(WIN_HEIGHT / 2);
            enterCh2Flow(C2_LAB);
            ch2PhaseTimer = 0;
            ch2LabInitDone = false;
            floatingTextMgr.spawn((float)CENTER_X + 1, 160.0f, "CENTRAL LAB", 0, 0, 0);
            floatingTextMgr.spawn((float)CENTER_X, 159.0f, "CENTRAL LAB", 255, 255, 255);
        }
    }

    // 每帧推进转场：换场在最暗帧执行，之后飞机从左侧缓动飞入
    void updateCh2Transition() {
        ch2FadeTimer++;
        if (ch2FadeTimer == CH2_SWITCH_AT) ch2ApplySceneSwitch();

        // 门禁段：淡出时让飞机继续驶进门内（输入已冻结，由转场接管）
        if (ch2Flow == C2_GATE && ch2FadeTimer < CH2_SWITCH_AT && player) {
            int nx = player->getX() + 6;
            if (nx > 770) nx = 770;
            player->setX(nx);
            player->setY(WIN_HEIGHT / 2);
        }

        // 换场后：从画面左侧外缓动飞入，末段减速
        if (ch2FadeTimer >= CH2_FLY_START && ch2FadeTimer <= CH2_FLY_END) {
            double t = (ch2FadeTimer - CH2_FLY_START) /
                       double(CH2_FLY_END - CH2_FLY_START);
            double e = 1.0 - std::pow(1.0 - t, 3.0);
            double x0 = -40.0, x1 = (double)ch2EntryX();
            player->setX((int)(x0 + (x1 - x0) * e));
            player->setY(WIN_HEIGHT / 2);
        }
        if (ch2FadeTimer >= CH2_FLY_END) {
            ch2FadeTimer = 0;
            player->setX(ch2EntryX());
            player->setY(WIN_HEIGHT / 2);
        }
    }

    // 转场遮罩：白色/黑色淡入淡出（显式开 BLEND，alpha 才生效）
    void drawCh2TransitionOverlay() {
        int fa = ch2TransitionAlpha();
        if (fa <= 0) return;
        SDL_Renderer* r = renderer.get();
        SDL_BlendMode prev;
        SDL_GetRenderDrawBlendMode(r, &prev);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        if (ch2FadeIsWhite()) SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)fa);
        else SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)fa);
        SDL_Rect fr = {0, 0, WIN_WIDTH, WIN_HEIGHT};
        SDL_RenderFillRect(r, &fr);
        SDL_SetRenderDrawBlendMode(r, prev);
    }

    void updateCh2Gate() {
        if (!ch2GateInitDone) {
            ch2GateInitDone = true;
            pulseSystem.energy = Ch2PulseSystem::MAX_ENERGY;   // HUD energy bar starts full
            if (!dGateQueued) {
                dGateQueued = true;
                dialogueSys.queueDialogue("Bryssa", "Are we there? ... Why did we stop?");
                dialogueSys.queueDialogue("Martha", "'Moonwell' ... no response at all.");
                dialogueSys.queueDialogue("Bryssa", "So we can't get in?");
                dialogueSys.queueDialogue("Martha", "Hold on. I'll have Ally try to link into their system.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "... ?");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Accessing Moonwell system.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Connecting. Please wait.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "All their devices are offline. But the facility still has power.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "... Preparing to emit a strong pulse signal. Forcefully waking up nearby hardware.");
                dialogueSys.start();
            }
        }

        // Dialogue finished → Ally starts the pulse sequence
        if (gateScene.getStage() == Ch2GateScene::DIALOGUE && !dialogueSys.isActive()) {
            gateScene.startPulsing();
        }

        // Pulse groups: each drains 1/5 of the full energy bar
        int emitted = gateScene.update(player->getX(), player->getY(), audio);
        if (emitted > 0) {
            pulseSystem.energy -= Ch2PulseSystem::MAX_ENERGY / 5;
            if (pulseSystem.energy < 0) pulseSystem.energy = 0;
        }

        // Scan starts → Moonwell AI speaks
        if (gateScene.getStage() == Ch2GateScene::SCANNING && !dMoonwellQueued && !dialogueSys.isActive()) {
            dMoonwellQueued = true;
            dialogueSys.queueDialogue("Moonwell (ai)", "Scanning code. Verifying ID.");
            dialogueSys.queueDialogue("Moonwell (ai)", "Flight code verified, 21395. Cargo confirmed. No obvious threats. You may proceed.");
            dialogueSys.queueDialogue("Moonwell (ai)", "Welcome, Martha. Ally.");
            dialogueSys.start();
        }

        // Door fully open → fly right through the doorway → start scene transition
        if (gateScene.isOpen() && player->getX() > 640 && ch2FadeTimer <= 0) {
            beginCh2Transition();
        }
    }

    // ======== CH2 FLOW: CORRIDOR (silent flight, then the sphere appears) ========
    void updateCh2Corridor() {
        // Wait for the corridor dialogue to end, then a short beat
        if (!dialogueSys.isActive()) {
            ch2PhaseTimer++;
        }
        if (ch2PhaseTimer >= 150 && !sphereBossActive) {
            sphereBoss.init(sideBg, player);
            sphereBossActive = true;
            sphereBoss.startEnteringAt(sideBg->getScrollX());  // appears ahead of current scroll
            audio.sndSphereAppear();
            enterCh2Flow(C2_SPHERE);
            ch2PhaseTimer = 0;
        }
    }

    // ======== CH2 FLOW: SPHERE (blue sphere boss: enter → activate → fight → shatter) ========
    void updateCh2Sphere() {
        if (sphereBossActive) {
            if (sphereBoss.getState() == Ch2SphereBoss::ENTERING)
                sphereBoss.syncScreenPos(sideBg->getScrollX());
            // First sighting dialogue
            if (!dSphereIntroQueued && sphereBoss.getState() == Ch2SphereBoss::ENTERING
                && !dialogueSys.isActive()) {
                dSphereIntroQueued = true;
                dialogueSys.queueDialogue("Bryssa", "Martha... what is THAT?");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Massive object ahead! Slowing down!");
                dialogueSys.queueDialogue("Martha", "Brace yourselves.");
                dialogueSys.start();
            }
            sphereBoss.update(&audio);
            double ts = sphereBoss.getBgTargetSpeed();
            if (ts >= 0 && sideBg) sideBg->setSpeed(ts);
        }
        // Activation dialogue (may arrive while intro dialogue still plays)
        if (sphereBossActive && !dSphereActQueued
            && (sphereBoss.getState() == Ch2SphereBoss::ACTIVATING || sphereBoss.getState() == Ch2SphereBoss::FIGHT)
            && !dialogueSys.isActive()) {
            dSphereActQueued = true;
            dialogueSys.queueDialogue("Ally (ai copilot)", "Its surface is shifting. Blue to orange!");
            dialogueSys.queueDialogue("Martha", "Orange means vulnerable. Open fire!");
            dialogueSys.start();
        }
        // Bullets vs sphere (only in FIGHT)
        if (sphereBossActive && sphereBoss.getState() == Ch2SphereBoss::FIGHT) {
            for (auto& b : bulletMgr.all()) {
                if (!b.active || !b.canDamage) continue;
                double dx = b.x - sphereBoss.getCx();
                double dy = b.y - sphereBoss.getCy();
                if (dx*dx + dy*dy < sphereBoss.getRadius() * sphereBoss.getRadius()) {
                    b.active = false;
                    int dropped = sphereBoss.hitDiamonds(3);
                    if (dropped > 0) {
                        particleMgr.spawnExplosion(b.x, b.y, 2 + dropped);
                        audio.sndSphereCrystalHit();
                    }
                }
            }
        }
        // Shatter sequence finished → chase begins (auto-spawn wave 1)
        if (sphereBossActive && sphereBoss.getState() == Ch2SphereBoss::DONE) {
            audio.sndShardSwarm();
            if (autoSpawnPhase == 0) {
                autoSpawnPhase = 1; autoSpawnQueued = 3; autoSpawnTimer = 0;
                autoSpawnScoreBase = score;  // record baseline for kill counting
                autoSpawnAliveLast = 0; autoSpawnKillsLast = 0;
            }
            enterCh2Flow(C2_CHASE);
            floatingTextMgr.spawn((float)CENTER_X + 1, 154.0f, "SHARDS INBOUND", 0, 0, 0);
            floatingTextMgr.spawn((float)CENTER_X, 153.0f, "SHARDS INBOUND", 255, 100, 70);
            if (!dChaseQueued) {
                dChaseQueued = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "The shards — they're coming back around! Incoming!");
                dialogueSys.start();
            }
        }
    }

    // ======== CH2 FLOW: CHASE (auto-spawn waves → danmaku → pulse orb → lab) ========
    void updateCh2Chase(const Uint8* keys) {
        // ==== Auto-spawn wave system ====
        // Spawn pump: processes any queued aliens (initial waves + escape replacements + reinforcements)
        if (autoSpawnQueued > 0) {
            if (autoSpawnTimer > 0) autoSpawnTimer--;
            if (autoSpawnTimer <= 0) {
                if (autoSpawnPhase == 1) {
                    ch2AlienMgr.forceSpawnFromShards();
                    audio.sndShardLaunch();
                } else {
                    ch2AlienMgr.forceSpawn();
                    audio.sndAlienWarp();
                }
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
                audio.sndAlienWarp();
                autoSpawnPhase = 6;
            }
        }

        // ==== Shift input (orb absorption uses it; release handled in common) ====
        bool shiftNow = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

        // Skill orb: spawn on first danmaku defeat (after defeat animation ends)
        if (!pulseOrbDropped && !pulseSystem.unlocked) {
            for (const auto& de : dmMgr.getEnemies()) {
                if (de.defeated && de.defeatTimer == 1) { // last frame of defeat animation
                    skillOrb.spawn(de.x, de.y);
                    pulseOrbDropped = true;
                    audio.sndTripleOn();
                    break;
                }
            }
        }
        // Orb drop dialogue
        if (pulseOrbDropped && !dOrbQueued && !dialogueSys.isActive()) {
            dOrbQueued = true;
            dialogueSys.queueDialogue("Ally (ai copilot)", "That one dropped something! Break the shell, grab the core!");
            dialogueSys.start();
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
                    audio.sndAbsorbCharge();
                }
                // Continuing absorption: each frame tick timer + fill energy
                if (skillOrb.state == Ch2SkillOrb::ABSORBING && shiftNow && nearPlayer) {
                    skillOrb.tickAbsorb();
                    if (skillOrb.absorbTimer % 10 == 0) pulseSystem.addAbsorbEnergy();
                    if (skillOrb.absorbTimer % 30 == 0) audio.sndAbsorbCharge();
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
                        audio.sndTripleOn();
                    }
                }
                // Energy drain: -3 per frame until 0
                if (pulseSystem.isDraining()) {
                    pulseSystem.drainTick();
                }
            }
        }

        // Pulse unlock dialogue
        if (pulseSystem.unlocked && !dPulseQueued && !dialogueSys.isActive()) {
            dPulseQueued = true;
            dialogueSys.queueDialogue("Ally (ai copilot)", "Pulse skill online! Full bar + SHIFT to release.");
            dialogueSys.start();
        }

        // ==== Transition to the central lab ====
        // Pulse unlocked + waves done + field clear → fly into the research center
        bool danmakuGone = true;
        for (const auto& de : dmMgr.getEnemies())
            if (de.active || de.defeated) { danmakuGone = false; break; }
        if (pulseSystem.unlocked && autoSpawnPhase == 6
            && autoSpawnQueued == 0 && ch2AlienMgr.countLiving() == 0 && danmakuGone
            && !dialogueSys.isActive()) {
            ch2PhaseTimer++;
            if (ch2PhaseTimer >= 120 && ch2FadeTimer <= 0) {
                ch2PhaseTimer = 0;
                enterCh2Flow(C2_AIRLOCK);   // 廊桥尽头的室内门禁
            }
        } else {
            ch2PhaseTimer = 0;
        }
    }

    // ======== CH2 FLOW: AIRLOCK (indoor security door at the corridor's end) ========
    // 扫描 → 锁定 → 开门；开门后玩家自己飞过舱门，再进入研究室转场。
    // 计时/进度全部由已入档的 ch2PhaseTimer 推导，不新增存档字段。
    void updateCh2Airlock() {
        if (ch2PhaseTimer == 0) {
            sideBg->setSpeed(0);       // 廊桥停在舱壁前
            autoSpawnPhase = 0;        // 关掉隐形能量墙，允许飞向舱门
            ch2AlienMgr.clearBullets();
            dmMgr.clearBullets();
        }
        int prev = ch2PhaseTimer;
        ch2PhaseTimer++;
        if (prev < Ch2AirlockScene::BOOT_END && ch2PhaseTimer >= Ch2AirlockScene::BOOT_END)
            audio.sndTripleCountdown();
        if (prev < Ch2AirlockScene::LOCK_AT && ch2PhaseTimer >= Ch2AirlockScene::LOCK_AT)
            audio.sndTripleOn();
        if (prev < Ch2AirlockScene::VERIFY_AT && ch2PhaseTimer >= Ch2AirlockScene::VERIFY_AT)
            audio.sndTripleOn();
        if (prev < Ch2AirlockScene::OPEN_START && ch2PhaseTimer >= Ch2AirlockScene::OPEN_START)
            audio.sndShockwave();

        if (Ch2AirlockScene::canPass(ch2PhaseTimer) &&
            player->getX() > 720 && ch2FadeTimer <= 0) {
            beginCh2Transition();
        }
    }

    // ======== CH2 FLOW: LAB (NightElf prototype → plane upgrade) ========
    void updateCh2Lab() {
        if (!ch2LabInitDone) {
            ch2LabInitDone = true;
            labUpgradeState = 0; labUpgradeTimer = 0;
            autoSpawnPhase = 0;              // energy wall off inside the lab
            if (sideBg) sideBg->setSpeed(0); // corridor comes to a stop
            labScene.reset();
            if (!dLabQueued) {
                dLabQueued = true;
                dialogueSys.queueDialogue("Bryssa", "Look! The center of the lab...");
                dialogueSys.queueDialogue("Martha", "A fighter. A real one.");
                dialogueSys.queueDialogue("Ally (ai copilot)", "Prototype identified: NightElf. Touch it to transfer control.");
                dialogueSys.start();
            }
        }

        // Park the NightElf prototype on its pedestal (until the player takes it)
        if (labUpgradeState == 0) {
            nightElf.setX(Ch2LabScene::PARK_X);
            nightElf.setY(Ch2LabScene::PARK_Y);
        }

        if (labUpgradeState == 0) {
            // Player flies to touch the prototype
            double dx = player->getX() - Ch2LabScene::PARK_X;
            double dy = player->getY() - Ch2LabScene::PARK_Y;
            if (dx*dx + dy*dy < 80.0 * 80.0) {
                labUpgradeState = 1;
                labUpgradeTimer = 0;
                audio.sndTransitionSwell();
            }
        } else if (labUpgradeState == 1) {
            // Upgrade animation: white particles converge on the prototype
            labUpgradeTimer++;
            if (labUpgradeTimer % 18 == 0) audio.sndAbsorbCharge();
            if (labUpgradeTimer % 3 == 0) {
                double angle = (rand() % 6283) / 1000.0;
                double dist = 40 + rand() % 90;
                double sx = Ch2LabScene::PARK_X + std::cos(angle) * dist;
                double sy = Ch2LabScene::PARK_Y + std::sin(angle) * dist * 0.6;
                particleMgr.spawnWhiteParticle(sx, sy,
                    (Ch2LabScene::PARK_X - sx) * 0.06, (Ch2LabScene::PARK_Y - sy) * 0.06,
                    15 + rand() % 15);
            }
            if (labUpgradeTimer >= 60) {
                // Swap planes: trainer → NightElf (position carried over)
                labUpgradeState = 2;
                labUpgradeTimer = 0;
                int tx = player->getX(), ty = player->getY();
                player = &nightElf;
                nightElf.setX(tx); nightElf.setY(ty);
                nightElf.setTripleFire(false);
                nightElfEnergy.reset();
                tripleBeepCounter = 0;
                floatingTextMgr.spawn((float)tx + 1, (float)(ty - 25), "NIGHTELF ONLINE", 0, 0, 0);
                floatingTextMgr.spawn((float)tx, (float)(ty - 26), "NIGHTELF ONLINE", 255, 255, 255);
                audio.sndTripleOn();
                if (!dUpgradeQueued) {
                    dUpgradeQueued = true;
                    dialogueSys.queueDialogue("Ally (ai copilot)", "NightElf online. Welcome aboard, Martha.");
                    dialogueSys.queueDialogue("Ally (ai copilot)", "Chain hits to charge white energy. Full bar = triple fire!");
                    dialogueSys.start();
                }
            }
        } else {
            // Upgrade done → the Warden blocks the exit
            labUpgradeTimer++;
            if (!dBossWarnQueued && !dialogueSys.isActive()) {
                dBossWarnQueued = true;
                dialogueSys.queueDialogue("Ally (ai copilot)", "Massive energy signature! It's blocking the exit!");
                dialogueSys.queueDialogue("Martha", "Then we go through it.");
                dialogueSys.start();
                audio.sndBossEntrance();
                wardenBoss.startEntering();
                autoSpawnPhase = 6;   // energy wall back on
                enterCh2Flow(C2_BOSS);
            }
        }
    }

    // ======== CH2 FLOW: BOSS (MOONWELL WARDEN fight) ========
    void updateCh2Boss() {
        // Minion summons from the boss
        if (wardenBoss.wantsMinion()) {
            wardenBoss.clearMinionRequest();
            if (ch2AlienMgr.countLiving() < 2) {
                ch2AlienMgr.forceSpawn();
                audio.sndAlienWarp();
            }
        }

        wardenBoss.update(*player, ch2PlayerHP, ch2GameOver, particleMgr, audio, floatingTextMgr);
        if (ch2GameOver) gameOver = true;

        Ch2WardenBoss::State bs = wardenBoss.getState();

        // Player bullets vs boss bullets
        for (auto& b : bulletMgr.all()) {
            if (!b.active || !b.canDamage) continue;
            for (auto& eb : wardenBoss.bullets) {
                if (!eb.active) continue;
                if (std::abs(b.x - eb.x) < 10 && std::abs(b.y - eb.y) < 10) {
                    b.active = false; b.canDamage = false; eb.hp--;
                    if (eb.hp <= 0) {
                        eb.active = false;
                        particleMgr.spawnExplosion(eb.x, eb.y, 4);
                        audio.sndCrystalCrush();
                        playerHitCount += 1;   // destroying an enemy bullet grants 1 energy
                    }
                    break;
                }
            }
        }

        // Player bullets vs boss body
        if (bs == Ch2WardenBoss::FIGHT || bs == Ch2WardenBoss::ENRAGED) {
            for (auto& b : bulletMgr.all()) {
                if (!b.active || !b.canDamage) continue;
                double dx = b.x - wardenBoss.getX(), dy = b.y - wardenBoss.getY();
                if (dx*dx + dy*dy < wardenBoss.getRadius() * wardenBoss.getRadius()) {
                    b.active = false; b.canDamage = false;
                    wardenBoss.takeDamage(1, &audio);
                    playerHitCount += 2;
                    audio.sndBossHit();
                    particleMgr.spawnExplosion(b.x, b.y, 4);
                }
            }
        }

        // Pulse vs boss bullets + boss body
        pulseSystem.collideWithBullets(wardenBoss.bullets, particleMgr, &playerHitCount);
        for (auto& w : pulseSystem.waves) {
            if (!w.active) continue;
            if (wardenBoss.lastHitByPulse == w.id) continue;
            if (bs != Ch2WardenBoss::FIGHT && bs != Ch2WardenBoss::ENRAGED) continue;
            double dx = wardenBoss.getX() - w.x, dy = wardenBoss.getY() - w.y;
            if (dx*dx + dy*dy < (w.radius + wardenBoss.getRadius()) * (w.radius + wardenBoss.getRadius())) {
                wardenBoss.lastHitByPulse = w.id;
                wardenBoss.takeDamage(1, &audio);
                playerHitCount += 2;
                particleMgr.spawnExplosion(wardenBoss.getX(), wardenBoss.getY(), 4);
                audio.sndShockwaveHit();
            }
        }

        // Enrage dialogue
        if (bs == Ch2WardenBoss::ENRAGED && !dBossEnrageQueued && !dialogueSys.isActive()) {
            dBossEnrageQueued = true;
            dialogueSys.queueDialogue("Ally (ai copilot)", "It's enraged! Watch the patterns!");
            dialogueSys.start();
        }

        // Boss destroyed → clear the field and roll the epilogue narration
        if (wardenBoss.isDefeated() && !ch2EpilogueStarted) {
            ch2EpilogueStarted = true;
            enterCh2Flow(C2_ENDED);   // epilogue narration runs, then mission complete
            ch2AlienMgr.clearAll();
            dmMgr.clearAll();
            wardenBoss.clearBullets();
            if (sideBg) sideBg->setSpeed(0);
            startCh2Epilogue();
        }
    }

    // ======== CH2 FLOW: ENDED (epilogue narration → mission complete menu) ========
    void updateCh2Ended(const Uint8* keys) {
        if (inNarration) return;   // epilogue narration is running (blocking)

        if (!missionComplete) {
            missionComplete = true;
            defeatFadeTimer = 0;
            if (isNormalPlay) {
                int cur = chapterMgr.getCurrentIndex();
                if (cur < 4 && !chapterMgr.isUnlocked(cur + 1))
                    chapterMgr.unlockChapter(cur + 1);
            }
        }
        defeatFadeTimer++;

        // Mission complete menu: NEXT CHAPTER / BACK TO MAIN MENU
        static bool upWasM2 = false, downWasM2 = false, enterWasM2 = false;
        bool upNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        if (upNow && !upWasM2 && mcMenuSelection > 0) mcMenuSelection--;
        if (downNow && !downWasM2 && mcMenuSelection < 1) mcMenuSelection++;
        if (enterNow && !enterWasM2) {
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
                    pendingAutoSave = true;      // 新章节起点写入 AUTO 槽
                } else {
                    resetGame(); atStartScreen = true;
                }
            } else {
                resetGame(); atStartScreen = true;
            }
        }
        upWasM2 = upNow; downWasM2 = downNow; enterWasM2 = enterNow;
    }

    // ======== CH2 EPILOGUE (blocking narration after the Warden falls) ========
    void startCh2Epilogue() {
        dialogueSys.reset(); audio.stopVoice();
        narration.reset();
        lastNarrationPage = -1;
        narration.queue("The Warden falls.\nThe lab falls silent.");
        narration.queue("The long-range comms unit is found intact.\nBryssa loads it into the trainer's hold.");
        narration.queue("Martha takes one last look at Moonwell.\nThe engines roar to life.");
        narration.queue("Home. The \"Life\" base is waiting.");
        narration.start();
        inNarration = true;
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
                        pendingAutoSave = true;      // 新章节起点写入 AUTO 槽
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
                if (ch2Flow == C2_GATE) {
                    // Gate scene: dedicated full-screen deep space + vacuum door.
                    // 不再复用第一章只覆盖上半屏的星空。
                    sideBg->drawGateBackdrop(renderer.get());
                    gateScene.drawDoor(renderer.get());
                } else if (ch2Flow == C2_LAB || ch2Flow == C2_BOSS || ch2Flow == C2_ENDED) {
                    // 研究室的圆厅一直保留到 Warden 战与击破演出
                    labScene.draw(renderer.get());
                } else {
                    sideBg->draw(renderer.get());
                    if (ch2Flow == C2_AIRLOCK)
                        Ch2AirlockScene::draw(renderer.get(), ch2PhaseTimer);
                }
            } else if (background) {
                background->drawBackground(renderer.get());
                background->drawBase(renderer.get());
            }

            if (!isSide) {
                for (const auto& sw : shockwaveMgr.all()) if (sw.active) shockwaveMgr.draw(renderer.get());
            }
            particleMgr.draw(renderer.get());
            if (!isSide) {
                dialogueSys.draw(renderer.get(), font);
                alienMgr.draw(renderer.get());
                bulletMgr.draw(renderer.get());
            }
            if (isSide) {
                if (sphereBossActive) sphereBoss.draw(renderer.get());
                if (ch2Flow == C2_BOSS || (ch2Flow == C2_ENDED && !wardenBoss.isDefeated()))
                    wardenBoss.draw(renderer.get());
                bulletMgr.draw(renderer.get());
                player->draw(renderer.get());
                if (ch2Flow == C2_GATE) {
                    gateScene.drawScanBeam(renderer.get(), player->getX(), player->getY());
                    gateScene.drawRings(renderer.get(), player->getX(), player->getY());
                }
                if (ch2Flow == C2_AIRLOCK) {
                    Ch2AirlockScene::drawScan(renderer.get(), ch2PhaseTimer,
                                              player->getX(), player->getY());
                }
                if (ch2Flow == C2_GATE) {
                    font.drawString(renderer.get(), "TAB:skip gate  ESC:pause", 16, WIN_HEIGHT - 22, 2);
                }
                drawWallFlash();
                ch2AlienMgr.drawEnemy(renderer.get());
                ch2AlienMgr.drawBullets(renderer.get());
                dmMgr.drawEnemy(renderer.get());
                dmMgr.drawBullets(renderer.get());
                if (ch2Flow == C2_BOSS) wardenBoss.drawBullets(renderer.get());
                // Parked NightElf prototype on its pedestal (before the upgrade)
                if (ch2Flow == C2_LAB && labUpgradeState == 0) nightElf.draw(renderer.get());
                // Pulse waves on top of enemies
                pulseSystem.draw(renderer.get());
                skillOrb.draw(renderer.get());
                // Ch2 HUD: all aligned to rightEdge = WIN_WIDTH - 10
                HUDBase::drawStatusPanel(renderer.get(), WIN_WIDTH - 10, 4, 60);
                HUDBase::drawScore(renderer.get(), font, score, WIN_WIDTH - 10, 10);
                HUDBase::drawHPHearts(renderer.get(), font, ch2PlayerHP, 3, WIN_WIDTH - 10, 28);
                float eFill = pulseSystem.getFill();
                HUDBase::drawEnergyBar(renderer.get(), WIN_WIDTH - 10, 46, 10*14, 6,
                    eFill, pulseSystem.isFull());
                // NightElf white energy bar (under the green bar, post-upgrade)
                if (player == &nightElf) {
                    int wMode = nightElfEnergy.isTripleActive() ? 2
                              : (nightElfEnergy.isCharging() ? 0 : 1);
                    HUDBase::drawEnergyBarWhite(renderer.get(), WIN_WIDTH - 10, 54, 10*14, 6,
                        nightElfEnergy.getFill(), wMode);
                }
                // Sphere boss HP bar (above the sphere, FIGHT only)
                if (ch2Flow == C2_SPHERE && sphereBossActive
                    && (sphereBoss.getState() == Ch2SphereBoss::FIGHT
                        || sphereBoss.getState() == Ch2SphereBoss::SHATTERING)) {
                    HUDBase::drawBossBar(renderer.get(), font, "???",
                        sphereBoss.getHp(), sphereBoss.getMaxHp(),
                        (int)sphereBoss.getCx() - 100,
                        (int)(sphereBoss.getCy() - sphereBoss.getRadius() - 30), 200, 10);
                }
                // Warden boss HP bar
                if (ch2Flow == C2_BOSS) wardenBoss.drawHPBar(renderer.get(), font);
                if (aimAssistOn) drawAimAssistSide();
                dialogueSys.draw(renderer.get(), font);
                // Scene-transition fade (white for gate/corridor, black for lab)
                drawCh2TransitionOverlay();
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
        HUDBase::drawStatusPanel(r, WIN_WIDTH - 10, 4, 54);
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

    void drawAimAssistSide(bool updateOnly = false) {
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

        // Check the Warden boss body (chapter boss fight)
        if (ch2Flow == C2_BOSS &&
            (wardenBoss.getState() == Ch2WardenBoss::FIGHT || wardenBoss.getState() == Ch2WardenBoss::ENRAGED)) {
            double t = wardenBoss.getX() - px;
            if (t > 0) {
                double lateral = std::fabs(wardenBoss.getY() - py);
                if (lateral < wardenBoss.getRadius() && t < bestT) {
                    bestT = t; snapX = wardenBoss.getX(); snapY = wardenBoss.getY();
                }
            }
        }

        if (updateOnly) { player->aimAssist.update(bestT < 999.0); return; }
        double drawX = (bestT < 999.0) ? snapX : aimX;
        double drawY = (bestT < 999.0) ? snapY : aimY;
        player->aimAssist.draw(r, drawX, drawY, 3, 2);
    }

    void drawAimAssist(bool updateOnly = false) {
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

        if (updateOnly) { player->aimAssist.update(bestT < 999.0); return; }
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
                if (upNow && !pUpWas && pauseMenuSelection > 0)     { pauseMenuSelection--; audio.sndMenuMove(); }
                if (downNow && !pDownWas && pauseMenuSelection < 6) { pauseMenuSelection++; audio.sndMenuMove(); }
                if (enterNow && !pEnterWas) {
                    audio.sndMenuConfirm();
                    if (pauseMenuSelection == 0)      { countdown = 3; countdownFrame = 0; }
                    else if (pauseMenuSelection == 1) { openSaveScreen(true); }
                    else if (pauseMenuSelection == 2) { openLoadScreen(true); }
                    else if (pauseMenuSelection == 3) { resetGame(); paused = false; pendingAutoSave = true; }
                    else if (pauseMenuSelection == 4) { paused = false; atOptionScreen = true; optionFromPause = true; }
                    else if (pauseMenuSelection == 5) { resetGame(); atStartScreen = true; paused = false; }
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
        ArtFX::BlendGuard guard(r);

        // 左右两块独立仪器面板：菜单和通讯记录互不干扰，同时压住背景高频线条。
        ArtFX::techPanel(r, 26, 82, 368, 384, 3, 10, 11, 52, 168, 106);
        ArtFX::techPanel(r, 406, 82, 368, 384, 3, 10, 11, 52, 148, 172);
        ArtFX::bracket(r, 20, 76, 380, 396, 12, 96, 236, 142, 115);
        ArtFX::bracket(r, 400, 76, 380, 396, 12, 96, 182, 214, 115);

        // === Left half: menu ===
        bool menuFocus = !pauseHistoryFocused;
        font.drawStringColor(r, "PAUSED", 50, 100, 3, 226, 248, 238, 255);
        SDL_SetRenderDrawColor(r, 45, 143, 91, 130);
        SDL_RenderDrawLine(r, 48, 134, 372, 134);
        const char* items[7] = {"RESUME", "SAVE GAME", "LOAD GAME", "RESTART",
                                "OPTIONS", "BACK TO MAIN MENU", "EXIT"};
        const int MENU_Y0 = 150, GAP = 38;
        for (int i = 0; i < 7; ++i) {
            int itemW = (int)strlen(items[i]) * 6 * 3;
            int itemX = 50;
            int itemY = MENU_Y0 + i * GAP;
            bool sel = menuFocus && i == pauseMenuSelection;
            if (sel) {
                ArtFX::techPanel(r, 36, itemY - 6, 342, 34,
                                 4, 18, 17, 84, 234, 148, 245);
                ArtFX::bracket(r, 30, itemY - 10, 354, 42, 10, 126, 255, 176, 145);
                UIRenderer::drawMenuCursor(r, itemX - 20, itemY + 10, 8);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
            }
            font.drawStringColor(r, items[i], itemX, itemY, 3,
                                 sel ? 230 : 158, sel ? 255 : 190, sel ? 241 : 178, 235);
        }
        font.drawStringColor(r, "A/D:SWITCH  W/S:MENU  ENTER:CONFIRM", 42, 480, 2,
                             108, 166, 134, 218);

        // === Right half: dialogue history ===
        const int RX = 420;
        font.drawStringColor(r, "HISTORY DIALOGUE", RX, 100, 3, 186, 226, 242, 250);
        SDL_SetRenderDrawColor(r, 49, 117, 152, 115);
        SDL_RenderDrawLine(r, RX, 136, 756, 136);

        auto& hist = dialogueSys.history;
        int total = (int)hist.size();
        if (total == 0) {
            font.drawStringColor(r, "NO TRANSMISSIONS", RX + 8, 184, 2,
                                 86, 122, 133, 185);
        } else {
            const int CH_W = 12, CH_H = 14;
            const int RY_TOP = 152;
            const int LINE_GAP = 14;
            const int TR = 54, TG = 177, TB = 123;
            const int SR = 164, SG = 218, SB = 240;

            int bottomIdx = total - 1 - dialogueSys.history.scroll;
            if (bottomIdx < 0) { bottomIdx = 0; dialogueSys.history.scroll = total - 1; }
            int maxSlot = (total >= 3) ? 2 : total - 1;
            int fSlot = dialogueSys.history.focusSlot;
            if (fSlot > maxSlot) fSlot = maxSlot;

            const int HIST_WRAP = 27;
            int curY = RY_TOP;
            for (int s = 0; s <= maxSlot; ++s) {
                int idx = bottomIdx - (maxSlot - s);
                if (idx < 0 || idx >= total) continue;
                auto& hl = hist[idx];

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

                std::vector<std::string> wlines;
                for (auto& line : hl.lines) {
                    std::string wr = line;
                    while ((int)wr.length() > HIST_WRAP) {
                        int brk = HIST_WRAP;
                        while (brk > 0 && wr[brk] != ' ') brk--;
                        if (brk == 0) brk = HIST_WRAP;
                        wlines.push_back(wr.substr(0, brk));
                        wr = wr.substr(brk + 1);
                    }
                    if (!wr.empty()) wlines.push_back(wr);
                }
                int wNum = (int)wlines.size();
                int itemH = wNum * CH_H + (wNum - 1) * 4;
                if (!hl.speaker.empty()) itemH += CH_H + 2;
                int itemY = curY;
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
    }

    void drawCountdown() {
        SDL_Renderer* r = renderer.get();
        UIRenderer::drawHalfTransparentOverlay(r, 160);
        double progress = countdownFrame / 45.0;
        if (progress > 1.0) progress = 1.0;
        double eased = 1.0 - std::pow(1.0 - progress, 2.5);
        char digit = '0' + countdown;

        // 准星式倒计时环，进度向右收束，增强"重新进入战斗"的节奏。
        int rr = 92 + (int)(eased * 18.0);
        for (int ring = 0; ring < 2; ++ring) {
            int radius = rr + ring * 9;
            int alpha = ring == 0 ? 115 : 55;
            SDL_Point prev = {CENTER_X + radius, WIN_HEIGHT / 2};
            for (int i = 1; i <= 48; ++i) {
                double a = 2.0 * M_PI * i / 48.0;
                SDL_Point next = {CENTER_X + (int)(radius * std::cos(a)),
                                  WIN_HEIGHT / 2 + (int)(radius * std::sin(a))};
                if (i <= (int)(48 * (1.0 - progress)) || ring == 1) {
                    SDL_SetRenderDrawColor(r, ring == 0 ? 94 : 44, ring == 0 ? 238 : 132,
                                           ring == 0 ? 145 : 94, (Uint8)alpha);
                    SDL_RenderDrawLine(r, prev.x, prev.y, next.x, next.y);
                }
                prev = next;
            }
        }
        // 四向刻度。
        for (int k = 0; k < 4; ++k) {
            double a = k * M_PI / 2.0;
            int x0 = CENTER_X + (int)((rr + 7) * std::cos(a));
            int y0 = WIN_HEIGHT / 2 + (int)((rr + 7) * std::sin(a));
            int x1 = CENTER_X + (int)((rr + 17) * std::cos(a));
            int y1 = WIN_HEIGHT / 2 + (int)((rr + 17) * std::sin(a));
            SDL_SetRenderDrawColor(r, 122, 255, 172, 165);
            SDL_RenderDrawLine(r, x0, y0, x1, y1);
        }

        int mainScale = 3 + (int)(eased * 13);
        for (int i = 2; i >= 0; --i) {
            double t = (double)(i + 1) / 3.0;
            int gs = mainScale + (int)(t * eased * 25.0);
            int alpha = (int)(150.0 * (1.0 - t) * (1.0 - eased * 0.4));
            if (alpha < 8) continue;
            int gw = 5 * gs, gh = 7 * gs;
            int gx = CENTER_X - gw / 2, gy = WIN_HEIGHT / 2 - gh / 2;
            SDL_SetRenderDrawColor(r, (Uint8)(alpha / 3), (Uint8)alpha, (Uint8)(alpha / 2), 255);
            font.drawChar(r, digit, gx, gy, gs);
        }
        int dw = 5 * mainScale, dh = 7 * mainScale;
        int dx = CENTER_X - dw / 2, dy = WIN_HEIGHT / 2 - dh / 2;
        font.drawStringColor(r, "RESUMING", CENTER_X - 48, WIN_HEIGHT / 2 + 82, 2, 173, 224, 191, 220);
        SDL_SetRenderDrawColor(r, 225, 255, 236, 255);
        font.drawChar(r, digit, dx, dy, mainScale);
    }

    // ======== GAME OVER ========
    void updateGameOverScreen(const Uint8* keys, bool&) {
        static bool upWas2 = false, downWas2 = false, enterWas2 = false;
        bool upNow2 = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        bool downNow2 = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        bool enterNow2 = keys[SDL_SCANCODE_RETURN];
        if (upNow2 && !upWas2 && menuSelection > 0)   { menuSelection--; audio.sndMenuMove(); }
        if (downNow2 && !downWas2 && menuSelection < 1) { menuSelection++; audio.sndMenuMove(); }
        if (enterNow2 && !enterWas2) {
            audio.sndMenuConfirm();
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
        ArtFX::techPanel(r, 142, 116, 516, 300, 8, 13, 12, 184, 60, 54);
        ArtFX::bracket(r, 134, 108, 532, 316, 16, 236, 96, 78, 170);
        int titleW = 9 * 6 * 4;
        font.drawStringColor(r, "GAME OVER", CENTER_X - titleW / 2, 140, 4, 255, 116, 96, 255);
        SDL_SetRenderDrawColor(r, 162, 56, 51, 180);
        SDL_RenderDrawLine(r, CENTER_X - 160, 186, CENTER_X + 160, 186);
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE:%-4d", score);
        int scoreW = (int)strlen(buf) * 6 * 3;
        font.drawStringColor(r, buf, CENTER_X - scoreW/2, 216, 3, 226, 239, 232, 240);
        const char* items[2] = {"PLAY AGAIN", "BACK TO MAIN MENU"};
        const int MENU_Y0 = 312;
        for (int i = 0; i < 2; ++i) {
            int itemW = (int)strlen(items[i]) * 6 * 3;
            int itemX = CENTER_X - itemW / 2;
            int itemY = MENU_Y0 + i * 60;
            bool sel = i == menuSelection;
            if (sel) {
                ArtFX::techPanel(r, CENTER_X - itemW / 2 - 34, itemY - 7, itemW + 68, 36,
                                 5, 18, 15, 176, 56, 51);
                UIRenderer::drawMenuCursor(r, itemX - 20, itemY + 10, 8);
                UIRenderer::drawMenuUnderline(r, itemX, itemY + 24, itemW);
            }
            font.drawStringColor(r, items[i], itemX, itemY, 3,
                                 sel ? 244 : 155, sel ? 236 : 171, sel ? 223 : 163, 235);
        }
        const char* hint = "W/S:SELECT   ENTER:CONFIRM";
        int hw = (int)strlen(hint) * 12;
        font.drawStringColor(r, hint, CENTER_X - hw / 2, 474, 2, 201, 139, 121, 220);
    }

    // ======== MISSION COMPLETE ========
    void updateMissionComplete(const Uint8* keys) {
        bool enterNow = keys[SDL_SCANCODE_RETURN];
        static bool entWas = false;
        if (enterNow && !entWas) { audio.sndMenuConfirm(); resetGame(); atStartScreen = true; }
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
            ArtFX::techPanel(r, 146, 118, 508, 314, 8, 18, 14, 74, 214, 130);
            ArtFX::bracket(r, 138, 110, 524, 330, 18, 146, 255, 180, 165);
            const char* title = "MISSION COMPLETE";
            int titleW = (int)strlen(title) * 6 * 3;
            font.drawStringColor(r, title, CENTER_X - titleW / 2, 148, 3, 168, 255, 193, 255);
            SDL_SetRenderDrawColor(r, 105, 216, 145, 175);
            SDL_RenderDrawLine(r, CENTER_X - 160, 192, CENTER_X + 160, 192);
            char scoreBuf[48];
            snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %-4d", score);
            int sw = (int)strlen(scoreBuf) * 6 * 2;
            font.drawStringColor(r, scoreBuf, CENTER_X - sw / 2, 238, 2, 226, 240, 232, 235);
            const int MENU_Y = 300, MENU_GAP = 44;
            const char* items[2] = {"NEXT CHAPTER", "BACK TO MAIN MENU"};
            for (int i = 0; i < 2; ++i) {
                int iy = MENU_Y + i * MENU_GAP;
                bool sel = i == mcMenuSelection;
                int itemW = (int)strlen(items[i]) * 6 * 2;
                int ix = CENTER_X - itemW / 2;
                if (sel) {
                    ArtFX::techPanel(r, CENTER_X - itemW / 2 - 28, iy - 7, itemW + 56, 34,
                                     5, 18, 15, 65, 218, 132);
                    UIRenderer::drawMenuCursor(r, ix - 20, iy + 8, 8);
                    UIRenderer::drawMenuUnderline(r, ix, iy + 20, itemW);
                }
                font.drawStringColor(r, items[i], ix, iy, 2,
                                     sel ? 236 : 154, sel ? 255 : 196,
                                     sel ? 219 : 177, 235);
            }
            const char* hint = "W/S:SELECT   ENTER:CONFIRM";
            int hw = (int)strlen(hint) * 12;
            font.drawStringColor(r, hint, CENTER_X - hw / 2, 402, 2, 111, 174, 137, 215);
        }
    }

    // ======== NARRATION ========
    void startChapterNarration() {
        narration.reset();
        lastNarrationPage = -1;   // first page will trigger its voice clip
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
        // Voice hook: play the dubbed clip when the page changes
        int page = narration.getCurLine();
        if (page >= 0 && page != lastNarrationPage) {
            lastNarrationPage = page;
            const std::string& raw = narration.currentRawText();
            if (!raw.empty()) playVoiceForText(raw);
        }
        if (!narration.isActive()) {
            if (lastNarrationPage >= 0) { lastNarrationPage = -1; audio.stopVoice(); }
            inNarration = false;
        }
        if (background) background->update();
        if (sideBg) sideBg->update();
    }

    void drawNarrationFrame() {
        SDL_Renderer* r = renderer.get();
        renderer.setColor(0, 0, 0);
        renderer.clear();
        if (background) background->drawStarsFullscreen(r);
        narration.draw(r, font);
        const char* hint = "ENTER : CONTINUE";
        int hw = (int)strlen(hint) * 12;
        font.drawStringColor(r, hint, CENTER_X - hw / 2, WIN_HEIGHT - 42, 2,
                             116, 184, 147, 220);
        SDL_SetRenderDrawColor(r, 40, 116, 76, 100);
        SDL_RenderDrawLine(r, CENTER_X - hw / 2 - 22, WIN_HEIGHT - 27,
                           CENTER_X + hw / 2 + 22, WIN_HEIGHT - 27);
    }

};
