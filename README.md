# STAR FOX SPACE SHOOTER

> 星际火狐 极简版 — A 2D space shooting game built with C++ and SDL2.

## Gameplay / 玩法

Control your starfighter in a massive space, shoot down alien ships, survive waves of enemies, and defeat the boss.

在无垠空间中操控星际战机，击落外星飞船，在敌潮中生存，击败关底 Boss。

| Key / 按键 | Action / 操作 |
|---|---|
| W/A/S/D | Move / 移动 |
| Space / 空格 | Shoot / 射击 |
| Shift | Pulse / 脉冲冲击波（获得技能球后） |
| ESC | Pause / 暂停 |

暂停菜单：`RESUME` / `SAVE GAME` / `LOAD GAME` / `RESTART` / `OPTIONS` / `BACK TO MAIN MENU` / `EXIT`

任意时刻 ESC 暂停即可存档；下次启动在主菜单选 `LOAD GAME` 读档，画面会停在你存档那一刻（连飞行中的子弹与弹幕都原样恢复），按 `RESUME` 倒计时 3-2-1 后继续战斗。

Save at any moment from the pause menu, then pick `LOAD GAME` from the main menu next time — the game resumes on the exact frame you saved (bullets in flight included).

## Asset Studio / 素材工坊 v0.1.3

素材工坊用于浏览游戏美术、切换状态、调整参数、预览动画、派生草稿和保存草稿参数。源码随本仓库提供。

macOS 先安装 Xcode Command Line Tools、SDL2 和 Python 3：

```bash
xcode-select --install
brew install sdl2
```

在终端进入你下载或克隆的项目根目录，执行：

```bash
bash tools/asset_studio/studio.sh
```

脚本会自动编译、启动本地服务并打开浏览器。若未自动打开，请访问 [http://127.0.0.1:8791/](http://127.0.0.1:8791/)。
**使用期间保留终端窗口，结束时按 Control + C；下次重新执行启动命令。**

动画 GIF 需要启动服务的 Python 环境安装 Pillow（`python3 -m pip install Pillow`）；缺少时自动使用 PNG 预览。
点击素材卡片可切换状态和调参数；草稿素材的「保存草稿参数」会保存参数与状态，下次打开恢复。
端口冲突可运行 `bash tools/asset_studio/studio.sh --port 8792`，然后访问相应端口。

QA：游戏 `--selftest` 包含第二章 25 项回归、七段主流程与廊桥室内门禁检查（战斗注入命中、维持玩家存活）；
工坊运行 `python3 -m unittest discover -s tools/asset_studio/tests -v`。

## Download / 下载

> macOS 11.0+

[Download v1.2.26](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.26) (latest)

[Download v1.2.25](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.25)

[Download v1.2.24](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.24)

[Download v1.2.23](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.23)

[Download v1.2.21](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.21)

[Download v1.2.20](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.20)

[Download v1.2.19](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.19)

[Download v1.2.18](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.18)

[Download v1.2.17](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.17)

Download the `.zip`, unzip, and run `Shooter ver1.2.26.app`.

下载 `.zip` 后解压，直接运行 `Shooter ver1.2.26.app`。

## What's New / 最近更新 (v1.2.26)

- 廊桥美术二轮：实心行星/大气/云带/风暴、银河/星云/星团、月球/环站/碎石/航船，顶部桁架与灯具/应急灯/体积光，实心立柱/湿地倒影/光池/舱盖/跑道灯。
- 廊桥尽头新增室内小型真空门禁：侧置扫描头、七段编号、双门板；红光从慢闪逐渐锁定常亮，验证后直接开门放行。
- 开场大门红光扫描延长，锁定玩家后由慢闪过渡到常亮，再转绿验证。
- 球体 Boss：每发子弹随机打落 3 个菱形，血条与掉落数量严格同步；异化排布（扭曲网格/三暗坑/深色本体/约 7.5% 微颤）。
- 球体 Boss 死亡演出：黑球扭曲成幽灵飘走，随后散落残骸按旧版灰白清晰节奏震动/飞出，再从地面升起追击。
- 存档格式未变（兼容 v1.2.23–v1.2.25 存档与节点存档）；`--selftest` 第二章 25 项回归全绿；Asset Studio v0.1.3 源码随仓库提供。

## Build from Source / 从源码编译

```bash
cd "v2.0.0/multiple code files" && make
```
或手动：
```bash
cd "v2.0.0/multiple code files" && clang++ -std=c++11 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -D_THREAD_SAFE main.cpp -o shooter -L/opt/homebrew/lib -lSDL2
```

Requires SDL2: `brew install sdl2`

开发用运行参数：

```bash
./shooter              # 正常游玩
./shooter --test       # 主菜单显示隐藏的 TEST 入口（跳关/预设状态调试）
./shooter --selftest   # 存档系统自检（无窗口，0=PASS）
```

## Project Structure / 项目结构

```
├── README.md
├── DEVELOPMENT_LOG.md
├── CLAUDE.md
├── v1.0.0/                                            # v1.0.0 稳定归档
│   ├── space_shooting ver1.0.0.cpp
│   ├── Shooter ver1.0.0.app/
│   └── Shooter ver1.0.0.zip
└── v2.0.0/                                            # 当前开发主线 (v1.2.26)
    ├── multiple code files/                            # ★ 当前开发源码（多文件架构）
    │   ├── main.cpp                                    # 入口 + #include 所有头文件
    │   ├── game.h                                      # Game 类与流程/存档/QA
    │   ├── save_system.h  save_menu.h                  # 存档格式/IO + 存读档界面
    │   ├── types.h  constants.h                        # 数据结构和常量
    │   ├── font.h  renderer.h  audio.h                 # 共享基础设施
    │   ├── player.h  bullets.h  particles.h            # 玩家/子弹/粒子
    │   ├── dialogue.h  narration.h  aim_assist.h       # 对话/旁白/瞄准
    │   ├── floating_text.h  chapter_manager.h  ui.h    # 浮动文字/章节/UI
    │   ├── ch1/                                        # Ch1 系统 (4 个文件)
    │   │   ├── ch1_shockwave.h  ch1_aliens.h
    │   │   ├── ch1_boss.h  ch1_background.h
    │   └── ch2/                                        # Ch2 系统 (12 个文件)
    │       ├── ch2_background.h  ch2_shooter_base.h
    │       ├── ch2_danmaku.h  ch2_aliens.h
    │       ├── ch2_sphere_boss.h  ch2_hud.h
    │       ├── ch2_skill_orb.h  ch2_pulse.h
    │       ├── ch2_gate.h  ch2_airlock.h  ch2_lab.h  ch2_boss.h
    │       └── Makefile
    ├── space_shooting ver1.2.20 single-file archive.cpp # v1.2.20 单文件存档
    ├── space_shooting ver2.0.0 deprecated.cpp           # 原始代码 (已废弃)
    └── Release Version/                                 # 发布版本
        ├── Shooter ver1.1.0.app/ + .zip                 # v1.1.0
        ├── Shooter ver1.2.2.app/ + .zip                 # v1.2.2
        ├── Shooter ver1.2.4.app/ + .zip                 # v1.2.4
        ├── Shooter ver1.2.5.app/ + .zip                 # v1.2.5
        ├── Shooter ver1.2.7.app/ + .zip                 # v1.2.7
        ├── Shooter ver1.2.8.app/ + .zip                 # v1.2.8
        ├── Shooter ver1.2.9.app/ + .zip                 # v1.2.9
        ├── Shooter ver1.2.10.app/ + .zip                # v1.2.10
        ├── Shooter ver1.2.11.app/ + .zip                # v1.2.11
        ├── Shooter ver1.2.14.app/ + .zip                # v1.2.14
        ├── Shooter ver1.2.15.app/ + .zip                # v1.2.15
        ├── Shooter ver1.2.16.app/ + .zip                # v1.2.16
        ├── Shooter ver1.2.17.app/ + .zip                # v1.2.17
        ├── Shooter ver1.2.18.app/ + .zip                # v1.2.18
        ├── Shooter ver1.2.19.app/ + .zip                # v1.2.19
        ├── Shooter ver1.2.20.app/ + .zip                # v1.2.20
        ├── Shooter ver1.2.21.app/ + .zip                # v1.2.21
        ├── Shooter ver1.2.23.app/ + .zip                # v1.2.23
        ├── Shooter ver1.2.24.app/ + .zip                # v1.2.24
        ├── Shooter ver1.2.25.app/ + .zip                # v1.2.25
        └── Shooter ver1.2.26.app/ + .zip                # v1.2.26 (最新)
```

## License

MIT
