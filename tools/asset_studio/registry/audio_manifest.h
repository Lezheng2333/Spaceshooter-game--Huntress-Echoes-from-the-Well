#pragma once

// ============================================================
// 音频素材清单（BGM / SFX）
//
// 主游戏没有音频文件：所有声音都在 audio.h 里实时合成。
// 这里用 X-macro 做单一真相源：
//   · reg_audio.h   → 把条目注册进画廊，供 --list / 网页展示
//   · audiopreview.cpp → 按同一份 id 调游戏真实合成代码导出 WAV
//
// BGM 的 key 是简短模式名（title/ch1/ch2/boss/upgrade）；
// SFX 的 key 是 AudioEngine 的真实方法名（由 audiopreview 直接调用）。
//
// 字段：id, 显示名, 章节, key, 试听秒数, 分组标签, 描述, 源码定位
// ============================================================

#define AUDIO_BGM_ASSETS(X) \
    X("audio.bgm.title",   "标题主题 · Moonwell Terminal", "shared", "title",   16.0, "标题 / 菜单", \
      "缓慢、空灵的空间站主题，带钟声、低音脉冲与四段和声呼吸。", \
      "v2.0.0/multiple code files/audio.h:361 (AudioEngine::renderMusicSample)") \
    X("audio.bgm.ch1",     "第一章 BGM · First Flight 92 BPM", "ch1", "ch1",      16.0, "第一章", \
      "92 BPM 的 demo 移植乐句：pad 和声、琶音、低音与强度分层。", \
      "v2.0.0/multiple code files/audio.h:181 (AudioEngine::scheduleDemoStep)") \
    X("audio.bgm.ch2",     "第二章 BGM · Deep-space Arpeggio", "ch2", "ch2",      16.0, "第二章", \
      "C# 大调深空环境曲：旋律、低音、冷色 pad，强度越高琶音与脉冲越明显。", \
      "v2.0.0/multiple code files/audio.h:391 (AudioEngine::renderMusicSample)") \
    X("audio.bgm.boss",    "Boss 战 BGM · 132 BPM", "shared", "boss",              8.0, "Boss 战", \
      "132 BPM 高压战斗乐句，失真低音、铜管式和弦与高频节奏层。", \
      "v2.0.0/multiple code files/audio.h:232 (AudioEngine::scheduleDemoStep)") \
    X("audio.bgm.upgrade", "NightElf 升级主题 · Fanfare", "ch2", "upgrade",        5.0, "升级演出", \
      "NightElf 获得新装备时的 C# 大调短促 fanfare，终段五音上行。", \
      "v2.0.0/multiple code files/audio.h:304 (AudioEngine::scheduleUpgradeStep)")

#define AUDIO_SFX_ASSETS(X) \
    X("audio.sfx.menu_move",            "菜单移动", "shared", sndMenuMove,         0.08, "菜单 UI", \
      "菜单焦点移动的短促高频提示音。", "v2.0.0/multiple code files/audio.h:729") \
    X("audio.sfx.menu_confirm",         "菜单确认", "shared", sndMenuConfirm,      0.12, "菜单 UI", \
      "确认选择的上扬双音，带一点空气感。", "v2.0.0/multiple code files/audio.h:732") \
    X("audio.sfx.menu_back",            "菜单返回", "shared", sndMenuBack,         0.10, "菜单 UI", \
      "返回上一层的短促下滑提示音。", "v2.0.0/multiple code files/audio.h:736") \
    X("audio.sfx.menu_denied",          "菜单拒绝", "shared", sndMenuDenied,       0.12, "菜单 UI", \
      "不可用操作的低声拒绝音。", "v2.0.0/multiple code files/audio.h:739") \
    X("audio.sfx.shoot",                "玩家射击", "shared", sndShoot,            0.10, "战斗通用", \
      "玩家开火的三层合成音：快速下滑主音、高频尾音与低频推力。", "v2.0.0/multiple code files/audio.h:742") \
    X("audio.sfx.enemy_shoot",          "敌人射击", "shared", sndEnemyShoot,      0.12, "战斗通用", \
      "敌方弹幕发射时的短促下扫警报。", "v2.0.0/multiple code files/audio.h:747") \
    X("audio.sfx.hit",                  "命中", "shared", sndHit,                  0.10, "战斗通用", \
      "普通命中的方波冲击与高频碎裂。", "v2.0.0/multiple code files/audio.h:751") \
    X("audio.sfx.explosion_small",      "小型爆炸", "shared", sndExplosionSmall,    0.13, "战斗通用", \
      "小型敌机爆炸：噪声、方波与低频轰鸣的轻量组合。", "v2.0.0/multiple code files/audio.h:755") \
    X("audio.sfx.explosion_big",        "大型爆炸", "shared", sndExplosionBig,      0.18, "战斗通用", \
      "大型爆炸：更长的噪声尾音、快速下扫和深低频冲击。", "v2.0.0/multiple code files/audio.h:760") \
    X("audio.sfx.shockwave",            "第一章冲击波", "ch1", sndShockwave,        0.50, "第一章 Boss", \
      "D 小调低频能量脉冲，根音、五度、八度加轻微空气噪声。", "v2.0.0/multiple code files/audio.h:766") \
    X("audio.sfx.shockwave_hit",        "冲击波命中", "ch1", sndShockwaveHit,      0.12, "第一章 Boss", \
      "冲击波擦中敌机时的高频确认音。", "v2.0.0/multiple code files/audio.h:775") \
    X("audio.sfx.boss_heal",            "Boss 治疗", "ch1", sndBossHeal,            0.23, "第一章 Boss", \
      "Boss 吸收能量回血时的上扬双音。", "v2.0.0/multiple code files/audio.h:779") \
    X("audio.sfx.boss_absorb",          "Boss 吸收", "ch1", sndBossAbsorb,          0.28, "第一章 Boss", \
      "Boss 吸取玩家能量的低沉下扫与噪声。", "v2.0.0/multiple code files/audio.h:783") \
    X("audio.sfx.boss_entrance",        "Boss 入场", "ch1", sndBossEntrance,        0.84, "第一章 Boss", \
      "TELAMONDO 登场的多层低频轰鸣、金属噪声与压迫感尾音。", "v2.0.0/multiple code files/audio.h:787") \
    X("audio.sfx.base_damage",          "基地受损", "ch1", sndBaseDamage,          0.20, "第一章 Boss", \
      "基地被击中的短促低频冲击。", "v2.0.0/multiple code files/audio.h:794") \
    X("audio.sfx.boss_hit",             "Boss 受击", "shared", sndBossHit,          0.12, "战斗通用", \
      "Boss 装甲被命中的金属撞击与低频闷响。", "v2.0.0/multiple code files/audio.h:795") \
    X("audio.sfx.shake",                "屏幕震动", "shared", sndShake,             0.17, "战斗通用", \
      "重击附加的次低频震动与空气噪声。", "v2.0.0/multiple code files/audio.h:800") \
    X("audio.sfx.crystal_crush",        "水晶碎裂", "shared", sndCrystalCrush,     0.10, "战斗通用", \
      "暖三角波主体的柔和玻璃碎裂音。", "v2.0.0/multiple code files/audio.h:804") \
    X("audio.sfx.villain_talk",         "反派通讯", "ch1", sndVillainTalk,          0.09, "角色通讯", \
      "反派无线电通讯的高频雷达啾声与低频底噪。", "v2.0.0/multiple code files/audio.h:811") \
    X("audio.sfx.teletype",             "打字机", "shared", sndTeletype,            0.08, "菜单 UI", \
      "旁白/对白逐字弹出的机械键盘式软嗒声，四种音高轮换。", "v2.0.0/multiple code files/audio.h:815") \
    X("audio.sfx.triple_on",            "三炮模式开启", "ch2", sndTripleOn,          0.10, "玩法反馈", \
      "NightElf 三炮模式开启的清脆金属双音。", "v2.0.0/multiple code files/audio.h:825") \
    X("audio.sfx.triple_countdown",     "三炮倒计时", "ch2", sndTripleCountdown,    0.06, "玩法反馈", \
      "三炮模式结束前的短促提示音。", "v2.0.0/multiple code files/audio.h:827") \
    X("audio.sfx.ally_talk",            "AI 副驾通讯", "shared", sndAllyTalk,       0.08, "角色通讯", \
      "AI 副驾的高频正弦消息提示。", "v2.0.0/multiple code files/audio.h:828") \
    X("audio.sfx.tower_talk",           "塔台通讯", "shared", sndTowerTalk,        0.08, "角色通讯", \
      "塔台 AI 的中频扫频消息提示。", "v2.0.0/multiple code files/audio.h:829") \
    X("audio.sfx.bryssa_talk",          "Bryssa 通讯", "shared", sndBryssaTalk,    0.08, "角色通讯", \
      "人类通讯的中高频方波提示。", "v2.0.0/multiple code files/audio.h:830") \
    X("audio.sfx.system_talk",          "系统消息", "shared", sndSystemTalk,       0.07, "角色通讯", \
      "系统消息的纯正弦短提示。", "v2.0.0/multiple code files/audio.h:831") \
    X("audio.sfx.martha_talk",          "Martha 通讯", "shared", sndMarthaTalk,    0.08, "角色通讯", \
      "驾驶员 Martha 的中低频方波通讯提示。", "v2.0.0/multiple code files/audio.h:832") \
    X("audio.sfx.moonwell_talk",        "Moonwell AI 通讯", "shared", sndMoonwellTalk, 0.08, "角色通讯", \
      "Moonwell AI 的下行正弦消息提示。", "v2.0.0/multiple code files/audio.h:833") \
    X("audio.sfx.player_hit",           "玩家受击", "shared", sndPlayerHit,         0.17, "战斗通用", \
      "玩家重伤时的低频冲击、噪声与机壳震动组合。", "v2.0.0/multiple code files/audio.h:834") \
    X("audio.sfx.sphere_appear",        "球体 Boss 出现", "ch2", sndSphereAppear,    1.03, "第二章·球体", \
      "巨型球体从深空压近时的超低频长鸣与谐波。", "v2.0.0/multiple code files/audio.h:842") \
    X("audio.sfx.sphere_activate_tick", "球体单元激活", "ch2", sndSphereActivateTick, 0.10, "第二章·球体", \
      "球体表面单元逐个亮起时的六音轮换电子 Tick。", "v2.0.0/multiple code files/audio.h:848") \
    X("audio.sfx.sphere_lock_on",       "球体锁定", "ch2", sndSphereLockOn,        0.38, "第二章·球体", \
      "球体锁定玩家时的低频充能、上行四音与压迫尾音。", "v2.0.0/multiple code files/audio.h:855") \
    X("audio.sfx.sphere_crystal_hit",   "球体晶体命中", "ch2", sndSphereCrystalHit, 0.12, "第二章·球体", \
      "击碎球体表面晶体时的玻璃高频与低频落点。", "v2.0.0/multiple code files/audio.h:861") \
    X("audio.sfx.ghost_drift",          "幽灵船漂移", "ch2", sndGhostDrift,        1.63, "第二章·幽灵船", \
      "幽灵船漂过时的长滑音、低频轰鸣与高频幽灵尾音。", "v2.0.0/multiple code files/audio.h:866") \
    X("audio.sfx.ghost_vanish",         "幽灵船消散", "ch2", sndGhostVanish,       0.84, "第二章·幽灵船", \
      "幽灵船向上消散的三层上扫与低频退场。", "v2.0.0/multiple code files/audio.h:872") \
    X("audio.sfx.debris_land",          "碎片落地", "ch2", sndDebrisLand,          0.10, "第二章·碎片", \
      "菱形残骸落地时的短促撞击与高频碎屑。", "v2.0.0/multiple code files/audio.h:877") \
    X("audio.sfx.debris_rumble",        "碎片轰鸣", "ch2", sndDebrisRumble,        0.23, "第二章·碎片", \
      "碎片堆移动时的低频隆隆声。", "v2.0.0/multiple code files/audio.h:881") \
    X("audio.sfx.shard_launch",         "碎片发射", "ch2", sndShardLaunch,         0.12, "第二章·碎片", \
      "地面碎片升空发射时的快速上扫与短促高音。", "v2.0.0/multiple code files/audio.h:885") \
    X("audio.sfx.shard_swarm",          "碎片风暴", "ch2", sndShardSwarm,          0.80, "第二章·碎片", \
      "大群碎片同时升空时的低频上升气流与噪声风暴。", "v2.0.0/multiple code files/audio.h:891") \
    X("audio.sfx.boss_enrage",          "Boss 暴怒", "ch2", sndBossEnrage,          0.82, "第二章·Warden", \
      "Warden 进入暴怒阶段的低频咆哮与上行噪声。", "v2.0.0/multiple code files/audio.h:896") \
    X("audio.sfx.shield_hit",           "护罩命中", "ch2", sndShieldHit,           0.10, "第二章·Warden", \
      "打在护罩上的高频能量反射与低频确认音。", "v2.0.0/multiple code files/audio.h:901") \
    X("audio.sfx.absorb_charge",        "吸收充能", "ch2", sndAbsorbCharge,        0.10, "玩法反馈", \
      "技能球吸收过程中的四音轮换充能 Tick。", "v2.0.0/multiple code files/audio.h:906") \
    X("audio.sfx.alien_warp",           "异星折跃", "ch2", sndAlienWarp,           0.10, "第二章·追逐", \
      "第二章敌机折跃进入时的短促上扫。", "v2.0.0/multiple code files/audio.h:912") \
    X("audio.sfx.alien_escape",         "异星撤离", "ch2", sndAlienEscape,         0.12, "第二章·追逐", \
      "敌机脱离时的下降式尾音。", "v2.0.0/multiple code files/audio.h:916") \
    X("audio.sfx.warden_burst",         "Warden 爆发", "ch2", sndWardenBurst,       0.28, "第二章·Warden", \
      "Warden 弹幕爆发时的低频推力与高速上扫。", "v2.0.0/multiple code files/audio.h:919") \
    X("audio.sfx.warden_aim",           "Warden 瞄准", "ch2", sndWardenAim,         0.18, "第二章·Warden", \
      "Warden 锁定目标时的雷达般双音上扫。", "v2.0.0/multiple code files/audio.h:923") \
    X("audio.sfx.warden_spiral",        "Warden 螺旋弹幕", "ch2", sndWardenSpiral,  0.34, "第二章·Warden", \
      "螺旋弹幕放出时的上下扫频与噪声尾音。", "v2.0.0/multiple code files/audio.h:927") \
    X("audio.sfx.transition_swell",     "转场渐强", "ch2", sndTransitionSwell,     0.70, "转场", \
      "章节转场时的低频渐强、上行噪声与空间膨胀感。", "v2.0.0/multiple code files/audio.h:931") \
    X("audio.sfx.upgrade_start",        "升级开始", "ch2", sndUpgradeStart,        0.60, "第二章·升级", \
      "NightElf 升级演出开始时的低频充能与上行噪声。", "v2.0.0/multiple code files/audio.h:938") \
    X("audio.sfx.upgrade_charge",       "升级充能", "ch2", sndUpgradeCharge,       0.20, "第二章·升级", \
      "升级过程中的四音轮换充能提示。", "v2.0.0/multiple code files/audio.h:943") \
    X("audio.sfx.upgrade_burst",        "升级爆闪", "ch2", sndUpgradeBurst,        0.70, "第二章·升级", \
      "升级完成瞬间的低频爆闪、噪声扫频与高频金光。", "v2.0.0/multiple code files/audio.h:949") \
    X("audio.sfx.upgrade_complete",     "升级完成", "ch2", sndUpgradeComplete,     0.50, "第二章·升级", \
      "四音上行的完成 chime 与低频落地音。", "v2.0.0/multiple code files/audio.h:955")
