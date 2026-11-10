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

[Download v1.2.28](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.28) (latest)

[Download v1.2.27](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.27)

[Download v1.2.26](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.26)

[Download v1.2.25](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.25)

[Download v1.2.24](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.24)

[Download v1.2.23](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.23)

[Download v1.2.21](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.21)

[Download v1.2.20](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.20)

[Download v1.2.19](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.19)

[Download v1.2.18](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.18)

[Download v1.2.17](https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v1.2.17)

Download the `.zip`, unzip, and run `Shooter ver1.2.28.app`.

下载 `.zip` 后解压，直接运行 `Shooter ver1.2.28.app`。

## What's New / 最近更新 (v1.2.28)

- **叙事字体重制**：吸收 demo 的程序化 stroke font，章节开场旁白与角色对白正文全部改用新字体；角色名继续保持原 5×7 像素字体。
- **无边框角色对白**：移除对话面板、外框、切角、光标和装饰标签，只保留角色名与发言正文，并用低透明度文字阴影保证可读性。
- **菜单视觉统一**：标题、章节、OPTIONS、SOUND、暂停、存读档、GAME OVER、MISSION COMPLETE 与章前旁白统一为切角熏黑玻璃 HUD；加入冷色渐变、细扫描纹理、双层细轮廓和双层角线，移除所有装饰性小字。
- **第二章门禁独立背景**：新增全屏深空场景，使用少量慢速导航星、静态微尘和低对比度遥远星体，下半屏不再复用第一章的空白背景。
- **星光降噪**：第一章战斗星数约减半、闪烁对比度降低、星相推进变慢，十字星芒改为极少数出现，减少对瞄准和弹幕读取的干扰。
- **NightElf 充能调整**：直接命中敌人 +2 能量，玩家子弹摧毁敌方子弹 +1 能量（Pulse 清弹不回能）；白色能量条上限 50 → 100，纯命中充能时间保持不变；绿色 Pulse 条上限仍为 30，冲击波伤害保持 1。
- **冲击波回复与技能教学**：每次实际释放 Pulse 冲击波回复 1 HP，最高不超过 3 点；技能球核心可吸收时画面下方出现呼吸式 SHIFT 提示；吸收完成后弹出暂停游戏世界的技能说明卡。
- **像素爱心血量**：第二章玩家生命从红色菱形改回早期版本风格的像素爱心，并加入暗红描边、亮红填充和左上高光。
- **Warden 难度调整**：暗夜精灵阶段的 Warden 子弹发射密度整体降低约 20%；环形弹、瞄准弹间隔与螺旋齐射均已调整，弹速、伤害和模式顺序不变。
- **同时包含 v1.2.27 表现层强化**：全局 CRT/暗角、终端化 UI、第一章与第二章美术重制、demo 的 92 BPM CH1 / 132 BPM Boss BGM，以及球体、幽灵、碎片、Warden、技能球等细节音效。
- **兼容性 / QA**：未新增游戏存档字段，`SAVE_FORMAT_VERSION` 不变；`make -Wall -Wextra` 零警告，`--selftest` 全绿，Asset Studio `--verify-all` 78/78。

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
└── v2.0.0/                                            # 当前开发主线 (v1.2.28)
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
        ├── Shooter ver1.2.27.app/ + .zip                # v1.2.27
        └── Shooter ver1.2.28.app/ + .zip                # v1.2.28 (最新)
```

## License

MIT
