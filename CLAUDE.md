# STAR FOX SPACE SHOOTER — Claude Code 备忘录

## 项目信息

- **语言**：C++11, SDL2
- **源码**：`v2.0.0/multiple code files/`（33 个头文件 + 1 个 main.cpp = 34 个文件）\
  旧单文件存档：`v2.0.0/space_shooting ver1.2.20 single-file archive.cpp`
- **编译命令**：
  ```bash
  cd "v2.0.0/multiple code files" && make
  ```
  或手动：
  ```bash
  cd "v2.0.0/multiple code files" && clang++ -std=c++11 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -D_THREAD_SAFE main.cpp -o shooter -L/opt/homebrew/lib -lSDL2
  ```
- **开发用运行参数**：
  ```bash
  ./shooter              # 正常游玩
  ./shooter --test       # 主菜单显示隐藏的 TEST 入口（跳关/预设状态调试）
  ./shooter --selftest   # 存档系统自检（无窗口，0=PASS；存档写 /tmp，不动玩家存档）
  ./shooter --mknodes    # 生成/刷新 34 个节点存档到 saves/（替代旧 TEST 模式的跳关）
  ```
- **SDL2 安装**：`brew install sdl2`
- **GitHub 仓库**：`Lezheng2333/starfox_spaceshooter`

## 配套工具：Asset Studio 素材工坊

- **进入网址**：**http://127.0.0.1:8791/**　（忘记网址时就查这里）
- **位置**：`tools/asset_studio/`（独立于游戏源码，**主游戏零改动**）
- **启动**（自动编译 + 起服务 + 开浏览器）：
  ```bash
  tools/asset_studio/studio.sh
  ```
- **用途**：浏览 / 预览 / 创作游戏全部美术素材（47 个），
  补回 TEST 模式退役后失去的"素材草稿箱"；存档系统管"从任意节点测玩法"，工坊管"做素材"
- **常用命令**：
  ```bash
  cd tools/asset_studio
  ./assetstudio --verify-all          # 批量自检（所有素材 × 所有状态）
  python3 registry/scanner.py         # 未登记检查（源码绘制入口 vs 注册表）
  make server                         # 编译 + 起服务
  ```
- **文档**：`SPEC.md`（设计与建设过程中的发现）、`DEVELOPMENT_LOG.md`（独立开发日志，从 0.1.0 起）
- **关键前提**：本作素材是 C++ 绘制代码而非图片文件，工具直接调用游戏真实绘制代码渲染，
  以保证预览与游戏画面永不漂移

## 上下文恢复

- 每次对话开始或 compact 之后，必须阅读 `DEVELOPMENT_LOG.md` 了解最新开发进展和最近版本变更
- 阅读本文件中的"当前开发状态"了解正在进行的任务和下一步计划

## 当前开发状态

### 当前焦点
- **当前发布版本 v1.2.27：全方向美术/音乐/音效/特效表现层强化（2026-11-09）**
  - 第一章 BGM 已移植 demo 的 92 BPM CH1 / 132 BPM Boss 乐句与三层强度；打字机、基地光波、Ch2 球体/碎片/Warden/技能球细节音已重做
  - 玩法、碰撞、输入、生成、流程状态机未改；未新增存档字段，SAVE_FORMAT_VERSION 不变
  - 发布包：`v2.0.0/Release Version/Shooter ver1.2.27.app` + `.zip`；Asset Studio v0.1.3
- **2026-11-07 v1.2.25 音频优化**：BGM/音效继续使用纯代码实时合成；三套乐谱安全淡变、流程强度分层、对白 ducking、48 声部与柔性限幅已接入，`--selftest` 包含音频渲染压力巡检。
- **2026-11-07 v1.2.25 第二章门禁演出升级**：真空大门改为分层硬表面（装甲带/倒角八边形门洞/门柱肩部/夹具/状态灯柱），扫描与开门重做（重启灯柱、红束断续充能锁定、验证转绿、夹具解锁+门板缓动、纵深通道）；动画全部由既有存档字段推导，存档格式未变。
- **2026-11-07 v1.2.25 第二章场景转场与背景美术**：门禁→廊桥、追逐→研究室改为淡出/淡入 + 飞机从左侧飞入（复用 `ch2FadeTimer`，无存档格式变化）；廊桥加窗外行星/远景桁架/吊灯/地面光池/前景栏杆，研究室改为完整圆厅（观景窗/气闸/出口/基座全息），Warden 战留在圆厅内。
- **2026-11-08 v1.2.25 (补充) 廊桥美术细节强化**：实心行星与大气/云带/风暴、银河/星云/星团/空间站/碎石/航船；顶部桁架、灯具/应急灯/体积光；实心立柱与波纹倒影；湿地光池/光楔/舱盖/跑道灯；纯绘制，存档格式未变。
- **2026-11-08 v1.2.25 (补充) 廊桥室内门禁 + 红光锁定**：新增 Ch2AirlockScene（廊桥尽头整高双门板小型真空门禁 + 侧置扫描头/七段编号/状态灯），流程追加 C2_AIRLOCK；开场大门红光延长并加入「慢闪 → 锁定后渐常亮」；室内门禁复用同一套红光锁定，开门后飞过 x>720 进入研究室。
- **2026-11-08 v1.2.25 (补充) 球体 Boss 三连掉落 + 异化排布**：每发子弹打落 3 个菱形，maxHp=菱形总数、`hitDiamonds()` 同步扣血；碎裂后等全部落地再震动；深色本体/扭曲网格/三暗坑/约 7.5% 微颤；地面残骸升起的橙色第一波 + SHARDS INBOUND 衔接追逐。
- **2026-11-08 v1.2.25 (补充) 球体 Boss 幽灵退场 + 旧版震动回归**：HP 归零后黑球扭曲成幽灵（130 帧，拉长/尾须/苍白轮廓/余影/竖缝暗坑）向上飘走淡出；随后恢复改版前的灰白清晰菱形震动/随机飞出（无橙色光晕、拖尾、地面能量环，SHAKING 不显示血条）。
- 本轮继续完善第二章与素材工坊；第三章暂不扩展。第二章门禁改为 ESC 暂停、TAB 跳过演出。
- `--selftest` 新增第二章 25 项回归及门禁→廊桥室内门禁→研究室的顺序流程；阶段截图 `/tmp/sfss_ch2_qa/`。
- 工坊后端回归：`python3 -m unittest discover -s tools/asset_studio/tests -v`（GIF/像素比较需 Pillow）。
- **第二章已完整贯通** — 七段主流程：门禁GATE→廊桥CORRIDOR→球体SPHERE→追逐CHASE→(室内门禁AIRLOCK)→研究室LAB→Boss BOSS→结局ENDED
- Ch2 组件全貌：门禁真空门/扫描演出（ch2_gate.h）、廊桥室内门禁（ch2_airlock.h）、圆形研究室（ch2_lab.h）、章节Boss Warden（ch2_boss.h）、球体Boss（三连掉落/HP 同步/幽灵退场/旧版震动/异化排布）、自动出敌、pulse技能、技能球、NightElf升级+白色能量条三炮模式
- Ch2 飞机切换：门禁/廊桥/球体/追逐段用 Ch2Trainer，研究室触碰暗夜精灵号后切换 NightElf
- **存档系统已上线** — 测试模式退役为隐藏入口（--test），主菜单改为 LOAD GAME；
  暂停存档/读档回到存档瞬间同一帧 + AUTO 自动存档 + 存档界面 + 自由读档文件浏览器 + 设置持久化
- **后续规划：Chapter 3（敌人要塞）** — 当前仍优先第二章与工坊 QA，第三章待本轮稳定后设计

### 开发路线
```
✅ 第二章全部主线：球体Boss ✅ 自动出敌 ✅ pulse技能 ✅ NightElf升级 ✅ 门禁序列 ✅ 章节Boss ✅ 剧情串联
✅ 存档系统：暂停存档/读档回同一帧/AUTO 自动存档/自由读档/设置持久化（--selftest 全绿）
→ Chapter 3：Druid激活 + 敌人要塞章节设计（第三章旁白目前为占位）
```

### 已知脆弱点（修改前必须理解上下文）
- **球体碎裂**：数组顺序与 shatterOrder 不同，必须寻找仍在球体上的最小碎裂序号，不能用数组遍历配累加游标过滤，否则可能永久卡关。
- **Ch2 球体 Boss（Ch2SphereBoss）**：`maxHp` 现在等于菱形总数，子弹只走 `hitDiamonds(3)`（返回实际掉落数并扣同数 HP），不要回到 `takeDamage(1)+popDiamonds(1)`；SHATTERING 先播 130 帧黑球幽灵（`drawGhost`），等 `allGrounded && stateTimer>=GHOST_END` 才进 SHAKING；SHAKING 的 `drawDebris` 必须保持改版前灰白 8px 清晰菱形，不要再加橙色光晕/拖尾/地面能量环，HUD 血条在 SHAKING 隐藏；追逐第一波只在 `autoSpawnPhase==1` 用 `forceSpawnFromShards()`（橙色/地面升起），否则会丢衔接；旧档用 `normalizeHpToCells()`。
- **瞄准辅助**：吸附进度只在 updateGameplay 中推进；drawAimAssist/Side 默认只绘制，避免暂停与读档重绘改变状态。
- **门禁按键**：ESC 统一暂停/倒计时，TAB 只在门禁实际游玩时跳过；菜单返回不能改变门禁状态。
- **Ch1 子弹横飞 Bug**：`addBullet()` 中 `Ch1Bullet.sideScroll` 必须显式设 `false`，否则垃圾值会随机触发 Ch2 弹道
- **对话级联触发**：每个分数触发的 dialogue 区块内必须同步设置 `lastScore = score`，帧级 `lastScore` 更新仅在 `!dialogueSys.isActive()` 时执行
- **Ch1 移动速度缩放**：`TrainingPlane::handleInput` 中的 y 钳制顺序必须在 x 钳制之后（先夹 y 再夹 x），否则影响 `getT()` 返回的子弹方向
- **Ch2 流程状态机**：`ch2Flow`（Ch2Flow 枚举）在 `updateGameplay` 的 Ch2 分支中 switch 分发；
  结局旁白触发时必须同步 `ch2Flow = C2_ENDED`，否则 MISSION COMPLETE 永不出现
- **Ch2 廊桥室内门禁（AIRLOCK）**：`C2_AIRLOCK` 必须保持在 Ch2Flow 枚举末尾（旧档 flow=0..6 数值不能变）；
  门禁阶段/开门进度全部由既有 `ch2PhaseTimer` 推导，不新增存档字段；`updateCh2Airlock()` 在 timer==0 停背景/关能量墙，
  玩家 x>720 且 `Ch2AirlockScene::canPass` 才 `beginCh2Transition()`，换场由 `ch2ApplySceneSwitch` 的 `C2_CHASE || C2_AIRLOCK` 进研究室
- **Ch2 门禁飞机活动范围**：门关闭时 x≤560，开门（doorOpen>0.5）后放宽至 x≤700，
  玩家穿过 x=640 触发白色淡出转场进入廊桥
- **Ch2 转场计时**：`ch2FadeTimer` 现在是转场总计时（1..22 淡出 / 24 换场 / 30..96 左侧飞入），
  换场与飞机位置由 `updateCh2Transition()` 接管；不要在别处直接改它，也不要绕过 `beginCh2Transition()`
- **Ch2 研究室绘制**：`C2_LAB / C2_BOSS / C2_ENDED` 统一画 `labScene`（完整圆厅，不再叠廊桥），
  `labScene.update()` 在公共 Ch2 段落里推进；读档/截图像素对比依赖这个更新顺序
- **存档字段顺序即字节顺序**：任何新增"会被存档的可变状态"必须同时
  ①在对应类的 `visit()` 里按固定位置补上 ②改动 serializeAll 字段顺序/类型时把
  `SAVE_FORMAT_VERSION` +1。游戏版本号（SAVE_GAME_VERSION）只作显示，不作废玩家存档
- **私有成员也要能存档**：各类的 `visit()` 是成员模板（因此能访问 private/protected）；
  指针成员（Ch1Boss.cfg / Ch2SphereBoss.bg / playerRef）一律不入档，读档后由 Game 重新绑定
- **读档必须复位输入边沿**：`finishLoad()` 里把 upWas/downWas/enterWas/escWas/bkspWas/shiftWas
  与 DialogueSystem/NarrationSystem 的 enterWas 全部置真，否则读档瞬间按住的键会跳过对话或误触发菜单
- **进入存/读档界面必须走 `beginSaveLoadScreen()`**：它会清掉 atStartScreen/atChapterSelect 等
  其它界面标志；直接改 atSaveLoadScreen 会让单帧分派继续停在旧界面
- **读档后固定进暂停态**：`finishLoad()` 强制 `paused=true, countdown=-1`，
  倒计时只能由玩家在暂停菜单选 RESUME 触发（需求：读档先看到存档那一帧，手动退菜单再继续）
- **Ch2 流程段落切换统一走 `enterCh2Flow()`**：它负责在切换时标记 AUTO 自动存档；
  直接赋值 `ch2Flow = ...` 会漏掉自动存档（TEST 预设入口除外）

## 代码规范

- 多文件架构：33 个头文件（全部 `#pragma once`）+ 1 个 `main.cpp`
- 每个文件对应一个系统/类，文件即文档
- 全部实现内联在头文件中（单翻译单元编译，无 .cpp 对应文件除了 main.cpp）
- OOP 设计：组合优于继承，静态方法替代虚函数
- 命名：Chapter 1 专用类加 `Ch1` 前缀，Chapter 2 加 `Ch2` 前缀，共享类无前缀
- **休眠代码标注**：所有未激活的类/函数用 `[DORMANT — 激活条件]` 注释标注
- **文件映射**：
  ```
  types.h = 全部 struct          constants.h = WIN_WIDTH + persp函数
  font.h   = Font                renderer.h = Renderer
  audio.h  = AudioEngine         floating_text.h = FloatingTextManager
  dialogue.h = DialogueHistory + DialogueSystem + drawTextLine
  narration.h = NarrationSystem  particles.h = ParticleManager
  aim_assist.h = AimAssist       player.h = Player + 全部4架子机
  bullets.h = BulletManager      chapter_manager.h = ChapterManager
  ui.h = UIRenderer + MenuStateMachine + MenuKeys
  save_system.h = SaveArchive + SaveMeta + SaveSystem + FileBrowser + GameSettings（存档格式/IO/设置）
  save_menu.h = SaveMenu（读档列表/存档列表/文件浏览器/确认框 UI）
  ch1/ = Ch1Shockwave/Alien/Boss/Background
  ch2/ = Ch2Background + ShooterBase + Danmaku + Aliens + SphereBoss + SkillOrb + Pulse + HUD + Gate + Airlock + Lab + WardenBoss
  game.h = Game类（含全部状态机+update/draw+存档序列化)    main.cpp = main()
  ```
- **存档序列化约定**：每个会被存档的类都实现 `template<class Ar> void visit(Ar& ar)`，
  字段顺序即存档字节顺序；`Game::serializeAll()` 按固定顺序串起全部子系统

## 完整文件结构（v1.2.23 多文件架构）

### 编译入口

```
main.cpp            (82 lines)  #include 所有头文件 + main()（--test / --selftest / --mknodes 参数解析）
```

### 根目录: 共享基础 (19 files)

| 文件 | 行数 | 内容 |
|------|------|------|
| `types.h` | 207 | 全部 18 个 struct + 2 个 enum（每个 struct 内含存档 visit()） |
| `constants.h` | 19 | WIN_WIDTH/WIN_HEIGHT/CENTER_X/HORIZON_Y + perspLeft/Right/Width |
| `font.h` | 153 | Font — 5x7 位图字体渲染 |
| `renderer.h` | 37 | Renderer — SDL 渲染器封装 |
| `audio.h` | 485 | AudioEngine — 实时音频合成 + BGM + 全部 snd* 函数 |
| `floating_text.h` | 38 | FloatingTextManager |
| `dialogue.h` | 273 | DialogueHistory + DialogueSystem + drawTextLine |
| `narration.h` | 157 | NarrationSystem — 中心开幕旁白 |
| `particles.h` | 140 | ParticleManager — 粒子特效管理 |
| `aim_assist.h` | 43 | AimAssist — 瞄准辅助组件 |
| `player.h` | 299 | Player + TrainingPlane + Ch2Trainer + NightElf + Druid[DORMANT] |
| `bullets.h` | 192 | BulletManager — addBullet/addBulletSideScrollAt/update/draw |
| `chapter_manager.h` | 70 | ChapterManager — 章节配置/解锁/切换 |
| `ui.h` | 185 | UIRenderer + MenuStateMachine + MenuKeys |
| `save_system.h` | 533 | SaveArchive（读写双端序列化）+ SaveMeta + SaveSystem（路径/CRC/文件 IO）+ FileBrowser + GameSettings |
| `save_menu.h` | 274 | SaveMenu — 读档列表/存档列表/迷你文件浏览器/确认框/提示条 |
| `game.h` | 4592 | Game 类 — 全部状态机 + 全部 update/draw + 存档序列化 + 存/读档界面接线 |

### ch1/ 子目录: Chapter 1 战斗系统 (4 files)

| 文件 | 行数 | 内容 |
|------|------|------|
| `ch1_shockwave.h` | 199 | Ch1ShockwaveManager + ID 碰撞机制 |
| `ch1_aliens.h` | 237 | Ch1AlienManager — 透视外星飞船管理 |
| `ch1_boss.h` | 435 | Ch1Boss — TELAMONDO（二阶段+治疗波+吸收） |
| `ch1_background.h` | 125 | Ch1Background — 星空+地平线+基地 |

### ch2/ 子目录: Chapter 2 战斗系统 (12 files)

| 文件 | 行数 | 内容 |
|------|------|------|
| `ch2_background.h` | 1380 | Ch2Background — 侧滚廊桥背景（深空/行星/桁架/灯具/湿地倒影/前景栏杆） |
| `ch2_shooter_base.h` | 160 | Ch2ShooterBase + NightElfEnergy（白色能量条） |
| `ch2_hud.h` | 86 | HUDBase — drawScore/drawHPHearts/drawEnergyBar/drawEnergyBarWhite/drawBossBar |
| `ch2_danmaku.h` | 224 | Ch2DanmakuManager — 螺旋弹幕敌人 |
| `ch2_aliens.h` | 191 | Ch2AlienManager — 普敌管理（菱形造型，四边突袭；球体碎裂后橙色/地面升起的第一波） |
| `ch2_sphere_boss.h` | 771 | Ch2SphereBoss — 菱形球体 Boss（每发掉 3 个/HP 同步/异化排布/幽灵退场/旧版震动），7 状态机 |
| `ch2_skill_orb.h` | 211 | Ch2SkillOrb — 漂浮技能球 + 18边形护罩 + 吸收 |
| `ch2_pulse.h` | 203 | Ch2PulseSystem — 绿色能量条 + Shift 单按释放冲击波 |
| `ch2_gate.h` | 1089 | Ch2GateScene — 门禁场景（真空门造型+重启扫描演出+红光锁定/常亮+脉冲解锁+开门动画） |
| `ch2_airlock.h` | 430 | Ch2AirlockScene — 廊桥尽头室内小型真空门禁（双门板/侧置扫描头/红光锁定/验证开门） |
| `ch2_lab.h` | 487 | Ch2LabScene — 中央研究室（完整圆厅/观景窗/气闸/出口/基座全息） |
| `ch2_boss.h` | 360 | Ch2WardenBoss — 章节Boss MOONWELL WARDEN（4 状态机+弹幕） |

### 休眠资产 (grep `[DORMANT]` 查找)

| 资产 | 文件 | 激活条件 |
|------|------|------|
| Druid | player.h | Chapter 3 开发时激活 |
| setStartBgm() | audio.h | 开始画面专属 BGM 设计完成后 |
| TEST 测试模式（updateTestScreen/drawTestScreen + 预设状态跳转） | game.h | 命令行 `./shooter --test`（主菜单才显示 TEST 项） |

### 关键设计决策

- **Player 切换**：`resetGame()` 中根据 `isSideScrolling` 分支选择 player 指针指向的实例
- **Ch2 移动**：在 `updateGameplay` 中直接读键移动，不走 `handleInput()`
- **Ch2 射击**：`addBulletSideScrollAt(Player&)` 支持任意枪位，dx=11 恒定速度
- **Ch1 射击**：`addBullet(TrainingPlane&)` 依赖 `getT()` 做透视弹道
- **Ch1 子弹横飞 Bug 根因**：`sideScroll` 未初始化，垃圾值随机触发 Ch2 弹道 — 修复在 `addBullet()` 中显式设 `sideScroll=false`
- **碰撞 ID 机制**：Ch1Shockwave 用 `lastHitBySW == sw.id`，Ch2Pulse 用 `lastHitByPulse == w.id`，每波唯一，每敌人只受一次伤害
- **Ch2 流程状态机**：`ch2Flow`（Ch2Flow 七段枚举）在 `updateGameplay` 的 Ch2 分支 switch 分发，
  对话更新/脉冲释放/白色能量等共享逻辑放公共区；Boss 死亡时同步 `ch2Flow = C2_ENDED` 再触发结局旁白
- **Ch2 球体Boss 入场**：正常流程用 `startEnteringAt(scrollX)`（世界坐标=当前滚动+1200），
  测试模式用 `startEntering()`（依赖滚动从 0 开始）
- **Ch2 章节Boss**：Ch2WardenBoss 独立类（ENTERING→FIGHT→ENRAGED→DYING），
  弹幕三模式循环+召唤普敌增援；Boss 受击计入 playerHitCount（白/绿能量条联动）
- **语音系统（VOICE OVER）**：旁白页/对话行切换时播放配音；文本→文件映射由
  `voice/voice_manifest.txt`（`<lang> <crc32(英文原文)> <相对路径>`，启动时加载）提供，
  文本改动自动映射新文件，缺失文件静默降级（对话保留合成电报音）；
  旁白打字机字母弹出音效（sndTeletype）始终保留；`voiceLang` 三态
  （0=中文默认/1=English/2=OFF 只留字母音效）在 OPTIONS 菜单循环切换；
  语音生成管线 `tools/voice_pipeline.py`（collect 提取 game.h 台词+按章节归属
  +校验中英翻译 → generate 按角色音色合成并生成 manifest，
  `--chapters 1,2` 按章节过滤，edge-tts/piper/gpt-sovits 后端）
- **语音资源现状**：第一+二章已生成（182 个 WAV，34MB），目录结构
  `voice/<zh|en>/ch<N>/nar_XX.wav + dlg_<角色>_XX.wav`；
  第三~五章占位旁白未生成（`generate --chapters 3,4,5` 可补）；
  发布 .app 时需一并打包 voice/ 目录
- **存档系统（Ver 1.2.23）**：暂停时把整帧状态按固定顺序序列化写盘（含飞行中的子弹/敌人/粒子/
  对话打字进度/背景滚动），读档反序列化后强制 `paused=true`，因此重绘出的就是存档那一帧，
  玩家在暂停菜单选 RESUME 才走 3-2-1 倒计时继续
  - 每个状态类实现 `visit(Ar&)`（成员模板，可访问 private）；指针成员不入档，读档后重新绑定
  - 文件结构：magic "SFSSAVE0" + 格式版本 + 游戏版本 + payload 长度 + CRC32 + payload；
    payload 首部是 SaveMeta 摘要（章节/分数/血量/流程段落/时间），列表只读摘要
  - 槽位：AUTO + SLOT 1-6；另可进入游戏内文件浏览器自由读档 / 另存为任意路径
  - 自动存档点：每章开始 + Ch2 每次 `enterCh2Flow()` 段落切换（结局段除外）
  - 存档目录 ./saves/，不可写回退 ~/Library/Application Support/StarFoxSpaceShooter/saves/，
    环境变量 SFSS_SAVE_DIR 可强制指定（--selftest 用它写到 /tmp）
  - 布局规则：改动 serializeAll 字段顺序/类型必须 +1 SAVE_FORMAT_VERSION（只按格式版本拒绝旧档）
- **节点存档生成器（--mknodes，Ver 1.2.23 补充）**：TEST 模式跳关功能的替代方案，
  把 34 个测试节点直接生成为存档文件（saves/nXX_*.sav），读档即等于跳到该节点
  - 三段式维护：nodeTable()（清单）+ buildNodeState()（怎么摆）+ nodeVerify()（怎么验）；
    加节点 = 各加一条，然后重跑 ./shooter --mknodes 刷新全部存档
  - 生成时必须过三道校验：状态自校验 / 文件回读 / 读档渲染非空（防"生成成功但节点不对"）
  - 节点空跑必须走 nodeStep()（先 background/sideBg update 再 updateGameplay），
    与主循环 stepFrame() 同序；只调 updateGameplay 会让侧滚背景不滚动（球体 Boss 卡 ENTERING）
- **只移动不改逻辑**：重构全部是剪切粘贴，零行逻辑修改

## OOP 重构后检查清单

每次 OOP 重构（拆分/合并类、重命名、提取方法）完成后，必须逐项检查：
1. 编译零错误零警告
2. 运行 `./shooter` 快速启动，确认不崩溃
3. 逐项核对受影响的功能点（用 `grep -n` 查找所有调用方，确认每个调用点已更新）
4. 全局搜索旧方法名/旧类名，确保无残留引用
5. 涉及战斗/流程/存档的改动，跑 `./shooter --selftest`（4 个战斗场景的存档往返 +
   菜单接线 + 跨章节读档 + 设置持久化，全部必须 PASS）

## 开发日志格式规范

`DEVELOPMENT_LOG.md` 遵循以下格式规则：

1. **标题行**：`  Ver X.X.X | 简短中文概括`（**行首 2 空格缩进**，不超过一行）；
   标题行下面**直接跟第一个条目**，不要空行、不要 `----` 分隔线
   - 只有里程碑大版本（0.1.0 / 1.1.0 / 1.2.0 这类）才用 `Ver X | 日期` + `----` 分隔线做章节大标题
2. **条目**：全部使用 `- ` 开头，4 空格缩进。**每个条目之间必须用空行分隔**（确保 Markdown 预览模式下条目正确换行）。续行用 6+ 空格缩进紧跟父条目；子条目用 6 空格 + `- `。
3. **顺序**：新功能/优化/enhancement 在前，**BUGFIX 统一在最后**
4. **BUGFIX 格式**：`- BUGFIX: 问题描述 + 修复方法`，与其他条目同级缩进
5. 每个条目尽量控制在一行内，避免不必要的多行展开
6. 版本号不变的小补充（如仅补语音资源、仅补开发工具）用 `  Ver X.X.X (补充) | ...` 标题，与上一版本共用版本号

## 发布流程 (/release 技能)

每次发布新版本时，按以下步骤操作：

1. **开发日志**：在 `DEVELOPMENT_LOG.md` 末尾写入新版本条目（格式参照已有条目）
2. **源码版本号**：修改 `font.drawString(r, "Ver X.X.X", ...)` 中的版本号
3. **README 更新**：
   - 下载链接：新增最新版本，保留历史版本
   - What's New：用人话简述最近更新（下次发版直接替换内容）
   - 项目结构：更新版本号和 Release Version 目录
4. **运行发布脚本**：
   ```bash
   .claude/skills/release.sh <version> <title> <notes>
   ```
   脚本自动完成：编译 → .app 封装 → 签名 → zip → git commit/push → gh release

### 发布脚本示例
```bash
.claude/skills/release.sh 1.2.9 \
  "Ver 1.2.9: 新功能描述" \
  "## 更新内容\n- 功能A\n- 功能B"
```
