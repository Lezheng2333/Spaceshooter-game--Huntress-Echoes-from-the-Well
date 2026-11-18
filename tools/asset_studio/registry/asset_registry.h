#pragma once

// ============================================================
// 中央素材注册表 —— 聚合入口
//
// 每个素材登记一项，工具据此自动出现在画廊里。
// 按类别拆到各 reg_*.h，新增素材时改对应文件即可。
//
// 主游戏零改动：本文件靠 include 游戏头文件工作，游戏完全不知道工坊存在。
// scanner.py 会扫描游戏源码交叉检查有没有漏登记的绘制入口。
// ============================================================

#include "reg_common.h"

// ---- 各类别素材（各文件自行 include 需要的游戏头文件）----
#include "reg_players.h"
#include "reg_enemies.h"
#include "reg_bosses.h"
#include "reg_fx.h"
#include "reg_hud.h"
#include "reg_backgrounds.h"
#include "reg_audio.h"

// ---- 草稿区入口（由 drafts/gen_drafts.py 生成，勿手改）----
#include "_generated.h"

// ============================================================
// 注册入口
// ============================================================
inline void buildAssetRegistry() {
    static bool built = false;
    if (built) return;
    built = true;

    registerAllPlayers();      // 玩家战机 6 架
    registerAllEnemies();      // 普通敌人 3 种
    registerAllBosses();       // Boss 3 个
    registerAllFx();           // 子弹 / 粒子 / 技能球 / 脉冲 / 冲击波
    registerAllHud();          // HUD / UI / 字体 / 准星
    registerAllBackgrounds();  // 背景 / 场景
    registerAllAudio();        // BGM / 音效

    registerDrafts();          // 草稿区
}
