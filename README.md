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
| F2 | CRT scanline overlay on/off / 开关 CRT 扫描线后处理 |

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

[Download v1.2.27](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.27) (latest)

[Download v1.2.26](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.26)

[Download v1.2.25](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.25)

[Download v1.2.24](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.24)

[Download v1.2.23](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.23)

[Download v1.2.21](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.21)

[Download v1.2.20](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.20)

[Download v1.2.19](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.19)

[Download v1.2.18](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.18)

[Download v1.2.17](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.17)

Download the `.zip`, unzip, and run `Shooter ver1.2.27.app`.

下载 `.zip` 后解压，直接运行 `Shooter ver1.2.27.app`。

## What's New / 最近更新 (v1.2.27)

- **全画面表现层强化**：统一 CRT 扫描线 + 暗角，`F2` 可开关；标题、章节、选项、暂停、存读档、对话与旁白的终端化 UI。
- **第一章美术重做**：深空星云/行星/远景前哨、装甲基地与机库灯光、玩家机/敌机/弹幕/冲击波细节，以及分层 Telamondo 堡垒 Boss。
- **第二章细节强化**：Warden 能量躯干与核心眼、敌方弹尾迹、脉冲环、技能球、廊桥速度尘埃，以及球体 Boss 幽灵退场的音画衔接。
- **音乐与音效**：第一章 BGM 全面移植 demo 的 92 BPM CH1 / 132 BPM Boss 乐句与三层强度；重做打字机、基地光波，并补齐球体激活/幽灵飘走/碎片落地·震动·起飞、Warden 狂暴与弹幕模式、技能球护罩/吸收、敌机入场/逃逸等细节音。
- **兼容性**：玩法、碰撞、输入、生成和流程状态机未改；未新增游戏存档字段，`SAVE_FORMAT_VERSION` 不变，读档同帧像素回归 `maxdiff=0`。
- **QA**：`make` 零警告；`--selftest` 全绿（存档字节/像素、Ch1 Boss、第二章 25 项流程、音频压力巡检）；Asset Studio `--verify-all` 78/78。

## Presentation Pass / 表现层强化（v1.2.27）

- 全局 CRT 扫描线 + 暗角后处理，按 `F2` 开关；标题、菜单、战斗、对话与存档界面统一为同一套终端视觉语言。
- 第一章重绘深空、行星、基地、玩家机、敌机、Telamondo 堡垒、弹道/爆炸/冲击波；第二章强化 Warden、敌方弹、脉冲环、技能球与廊桥速度尘埃。
- 第一章 BGM 全面切换为 demo 的 92 BPM CH1 / 132 BPM Boss 乐句与三层强度编排；第二章与标题保留各自氛围层。全部音效继续纯代码实时合成，并扩展三角波、锯齿波、窄脉冲与低通噪声音色。
- 重做打字机、基地光波与第二章细节音：球体登场/激活/幽灵飘走/碎片落地/地面震动/碎片起飞、Warden 狂暴与弹幕模式提示、技能球护罩/吸收、敌机入场/逃逸等。
- 本轮只改表现层：碰撞、输入、生成、流程状态机与存档字段均未改动；所有新增战斗动画由已存档状态或纯常量推导，读档同帧像素仍与存档时一致。
- QA：`make` 零警告，`./shooter --selftest` 全绿（含存档字节/像素一致性、第二章 25 项、音频压力巡检）；Asset Studio `--verify-all` 78/78。

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
└── v2.0.0/                                            # 当前开发主线 (v1.2.27)
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
        ├── Shooter ver1.2.26.app/ + .zip                # v1.2.26
        └── Shooter ver1.2.27.app/ + .zip                # v1.2.27 (最新)
```

## License

MIT
