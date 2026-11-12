#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "types.h"
#include "constants.h"
#include "font.h"
#include "renderer.h"
#include "audio.h"
#include "floating_text.h"
#include "dialogue.h"
#include "narration.h"
#include "particles.h"
#include "aim_assist.h"
#include "player.h"
#include "bullets.h"
#include "ch1/ch1_shockwave.h"
#include "ch1/ch1_aliens.h"
#include "ch1/ch1_boss.h"
#include "ch1/ch1_background.h"
#include "ch2/ch2_sphere_boss.h"
#include "ch2/ch2_background.h"
#include "ch2/ch2_shooter_base.h"
#include "ch2/ch2_hud.h"
#include "ch2/ch2_danmaku.h"
#include "ch2/ch2_aliens.h"
#include "ch2/ch2_skill_orb.h"
#include "ch2/ch2_pulse.h"
#include "ch2/ch2_gate.h"
#include "ch2/ch2_airlock.h"
#include "ch2/ch2_lab.h"
#include "ch2/ch2_boss.h"
#include "chapter_manager.h"
#include "ui.h"
#include "game.h"

// ============== MAIN ==============
int main(int argc, char** argv) {
    srand((unsigned)time(nullptr));

    // 开发用开关（不影响正常游玩）：
    //   --test      主菜单显示隐藏的 TEST 入口（[DORMANT] 测试模式）
    //   --selftest  存档/读档自检后退出（无窗口，返回 0=PASS）
    //   --mknodes   生成/刷新节点存档到 saves/（等价于旧 TEST 模式的跳关）
    bool devMode = false, selfTest = false, mkNodes = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--test") == 0) devMode = true;
        else if (strcmp(argv[i], "--selftest") == 0) selfTest = true;
        else if (strcmp(argv[i], "--mknodes") == 0) mkNodes = true;
    }
    // 自检把存档目录重定向到临时目录，避免覆盖玩家真实存档
    if (selfTest) setenv("HUNTRESS_SAVE_DIR", "/tmp/sfss_selftest", 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(GAME_TITLE,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WIN_WIDTH, WIN_HEIGHT,
                                          (selfTest || mkNodes) ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    if (!window) { SDL_Quit(); return 1; }

    Renderer renderer(window);
    AudioEngine audio;
    Game game(renderer, audio, window, devMode);

    int rc = 0;
    if (selfTest) rc = game.runSelfTest();
    else if (mkNodes) rc = game.generateNodeSaves(SaveSystem::baseDir());
    else game.run();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
