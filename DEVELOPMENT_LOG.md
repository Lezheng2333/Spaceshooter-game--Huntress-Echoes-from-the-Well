================================================================
  SPACE SHOOTER — 开发日志
================================================================

Ver 0.0.1 | 2026-05-06
----------------------------------------------------------------

  Ver 0.0.1 | 项目初始化 + 透视优化
    - 创建 SDL2 窗口（800x600），主游戏循环

    - 星空背景：随机生成星点，原地正弦闪烁

    - 优化子弹飞行轨迹，使其符合空间透视学原理

    - 重构背景场景构造，景深更自然

    - 解决子弹"被远端中心点吸入"的视觉问题

  Ver 0.0.2 | 飞机上下移动 + 左右翻滚倾斜
    - 新增上下移动（W/S），原先仅支持左右（A/D）

    - 左右移动时飞机向移动方向自然翻滚倾斜

  Ver 0.0.3 | 外星飞船透视优化 + 子弹散射调整
    - 外星飞船放大，移动遵循空间透视原理

    - 子弹散射减小，弹道整体路径符合玩家预期

    - 透视消失点上移，与地平线相交区域占画面宽度 80%

    - 地平线以上纯星空，透视线不再上延

    - 外星飞船从星空顶端透视线范围内刷新

    - 外星飞船初始速度减半，60 秒后才逐渐加速

----------------------------------------------------------------

  Ver 0.1.0 | 外星飞船系统 + 基地 + 游戏结束
    - 外星飞船：体积较大、移动较慢、从上向下沿透视线飞行

    - 被击中 5-10 次爆炸，简易爆炸粒子特效（红/白色）

    - 玩家分数 +1，随时间推移飞船数量增加、速度变快

    - 圆拱形基地覆盖画面底部，飞船触碰→玩家失败

    - 失败画面放大最终得分，"退出程序"/"再来一次"选项

  Ver 0.1.1 | 基地血量 + 子弹优化
    - 基地 10 点 HP，HUD 用 10 颗小爱心展示

    - 飞船碰基地扣 1 HP，HP=0→GAME OVER

    - 子弹 85% 射程后失效并变透明，解决滞留子弹秒杀问题

    - 外星飞船刷新后沿透视线直接移动（轨迹简化，更容易命中）

  Ver 0.1.2 | 暂停系统 + 3 秒倒计时
    - ESC 暂停，菜单：RESUME / RESTART / EXIT

    - 恢复游戏时 3 秒倒计时（画面中央巨大白色数字）

    - 倒计时特效：数字"从画面内向外蹦出"的裸眼 3D 感

    - 更大透明版数字从本体扩散+爆炸白色粒子碎片消散

  Ver 0.1.3 | 外星飞船入场无敌 + 顶部突袭
    - 飞船蓝色登场阶段+完成后 3 帧无敌

    - 无敌期间子弹碰撞→子弹消失，飞船不受伤害

    - 新增从屏幕顶端直接突袭入场的刷新方式

    - 外星飞船无敌=蓝色，可受击=红色

----------------------------------------------------------------

  Ver 0.2.0 | 冲击波系统
    - 绿色冲击波沿透视线扩散，快速渐隐

    - 基地释放时绿色特效覆盖圆拱顶部

    - 冲击波升级：每级+1 伤害，子弹+10%数量/速度

    - 外星飞船血量随机 3-5（等级提升范围也提升）

    - 飞船触碰冲击波每帧只判定一次伤害

    - 60 分后外星飞船刷新明显加快

----------------------------------------------------------------

  Ver 0.3.0 | Boss 系统
    - 分数 ≥200 触发 Boss 登场动画

    - Boss 状态机：INTRO → FIGHT → PHASE2 → DEFEAT

    - 场上飞船移速降 20% 无敌，玩家不能射击但可移动

    - 基地冲击波停止，基地无敌

    - 超大型 Boss 从顶部降下，10000 HP，宽血条

    - 登场完毕→画面震动 1 秒→逐个吸收外星飞船

    - Boss 战期间飞船从 Boss 中心向外突袭释放

    - Boss 左右移动（∞形路径），受击高亮+命中爆炸

    - HP<50% 触发 Phase2

  Ver 0.3.1 | Boss 吸收动画
    - 蓝色光束从 Boss 射向飞船→击中→蓝色受击特效

    - 飞船超大半螺旋收缩吸入 Boss

    - 逐个吸收（上一个完成 0.3 秒后再下一个）

    - 吸收形成淡红色血条：等比例压缩本体红条长度

    - 光束未命中→飞出画面→冷却→重新发射

    - BUGFIX: 最后一个飞船吸收被跳过修复

    - BUGFIX: 吸收完成后未进入 Boss 战修复

  Ver 0.3.2 | Boss 治疗红波
    - Phase2 后每 7 秒释放透视椭圆形红色冲击波

    - 碰到一个外星飞船→Boss 回 30 HP（多次调优: 5→10→20→50→30）

    - 红色粒子飞向血条末端，移动时间 1 秒

    - 50% 血量后红波叠加到血条末端（不压缩本体）

    - Boss 登场/Phase2 动画期间 Boss 无敌

    - BUGFIX: "bos"→"BOSS" 名称修正

  Ver 0.3.3 | Boss 血条优化
    - 血条宽度缩减为画面 50%

    - 淡红色超出边框时动态重绘边框

    - 血量减至本体红线时边框停止缩短

----------------------------------------------------------------

  Ver 0.4.0 | 菜单系统 + 测试模式
    - 开始界面：PLAY / TEST / OPTIONS / EXIT

    - 黄色箭头+黄色下划线指示当前选项

    - 测试模式：选择初始分数（30/60/90/120/150/180）

    - 选择后直接进入对应游戏状态

    - 测试模式后续新增：分数 200（直达 Boss）、Boss 二阶段、
      Boss 1HP，默认创建 5 个测试敌人
    - OPTIONS 菜单：AIM ASSIST 开关（绿色 ON/红色 OFF）

    - 暂停菜单增加 OPTIONS 和"BACK TO MAIN MENU"

    - 所有菜单黄色下划线统一、操作提示语居中（y=490）

    - 版本号左下角显示

    - BUGFIX: "confirm" 缺 'f' 修复

    - BUGFIX: 菜单闪烁、Back 无响应修复

    - BUGFIX: 瞄准辅助默认关闭状态修复

    - BUGFIX: 暂停 ESC 行为与 resume 统一（+3 秒倒数）

----------------------------------------------------------------

  Ver 0.5.0 | 瞄准辅助系统
    - 默认位置：子弹飞行距离 40%（有效射程 80%的一半）

    - 外观：正方形（四边中间 60%断开）+ 中央白色瞄准点

    - 有效射程内有敌人→方形缩小、白点吸附到碰撞判定点

    - 多敌人优先吸附最近的（先碰撞的）

    - 吸附过程平滑过渡（不直接消失），延长缩小时间

    - 对 Boss 同样有效

    - BUGFIX: 吸附目标命中率修复

----------------------------------------------------------------

  Ver 0.6.0 | 音频系统
    - SDL 音频回调，44100Hz 浮点采样

    - 射击声：密特罗德风格高频扫频

    - Boss 登场音效：5 层和弦，震慑力强

    - Boss 受击音效：低沉哀鸣

    - 外星飞船受击音效：低频+噪声

    - BGM：8bit 旋律（32 音符）+ 低频贝斯 + ADSR 包络

    - Bass 脉动效果（LFO 调制）

    - Boss 战自动切换 BGM（更快 BPM + 更高音 + 更强 bass）

    - 音量控制：BGM 1-10 / SFX 1-10

    - EQ 三段均衡：低频/中频/高频 各 -5~+5

  Ver 0.6.1 | SOUND 子菜单
    - Options 内新增 SOUND 子菜单入口

    - BGM 音量 / SFX 音量 / EQ 低中高频 独立调节

    - ESC 返回上级菜单

----------------------------------------------------------------

  Ver 0.7.0 | Boss 战败完整序列
    - Boss 爆炸 5 秒：小爆炸(每 6 帧)+大爆炸(每 20 帧)

    - 最终大爆炸→逐个消灭残存飞船(60 帧间隔)

    - 黄色"MISSION COMPLETE"大字弹出

    - 飞机 smoothstep 缓动回中加速

    - 飞机快速直线飞出画面（无翻滚）

    - 烟花系统：彩色粒子底部喷射

    - 烟花 2 秒后淡入结束画面

    - 结束标题："STAR FOX SPACE SHOOTER"

    - BUGFIX: Boss 血条见底游戏不结束修复

    - BUGFIX: 结束界面黑屏修复

    - BUGFIX: MISSION COMPLETE 截断(text[16]→[32])

    - BUGFIX: 飞机飞离上下翻转(地平线以上机头朝上)

----------------------------------------------------------------

  Ver 0.8.0 | 飞机翻滚动画重设计
    - 仅移动方向改变时触发 360° 完整翻滚

    - 静止→移动首帧也触发翻滚

    - smoothstep 缓动，20 帧一圈

    - 飞机机头始终对齐透视消失点（heading direction）

    - 机翼随翻滚旋转+倾斜（双层变换：旋转+roll）

  Ver 0.8.1 | 透视椭圆 + 字体 + 界面打磨
    - 治疗红波改为透视圆形（perspEllipse）

    - Phase2 圆形冲击波改为透视圆形

    - 新增 12 个字符：/ - + . b d h j k w z

    - 窗口标题："星际火狐 极简版"

    - 游戏内标题："STAR FOX SPACE SHOOTER"

    - 暂停页面/Option 缺失字母补全

    - 版本号字体放大

----------------------------------------------------------------

  Ver 0.9.0 | 冲击波能量系统重设计
    - 从"命中次数触发"改为"得分累积触发"

    - 1 分 = 1 点能量，满 30 升级冲击波

    - 30 分首次升级：10 秒间隔自动释放

    - 60 分：8 秒；90 分：6 秒；120 分：4 秒

    - 150 分:2 秒；≥180 分:1 秒+能量条常绿永满

    - "Level UP"绿色浮动文字(飞机上方弹出)

    - BUGFIX: 180 分后不再触发 Level UP 特效

  Ver 0.9.1 | macOS App 封装 + 兼容性修复
    - 封装为 macOS .app bundle，内置 SDL2 动态库

    - 代码签名，兼容 macOS 11.0+

    - 支持 Apple Silicon (M1/M4) 原生运行

    - 压缩为 .zip 分发

================================================================
  Ver 1.0.0 | 2026-05-09
  v0.x.x 全部内容的稳定归档
  源代码：space_shooting ver1.0.0.cpp
  封装：Shooter ver1.0.0.app / Shooter ver1.0.0.zip
  最低系统要求：macOS 11.0
================================================================

Ver 1.0.1 | 2026-05-08
----------------------------------------------------------------

  Ver 1.0.1 | CHAPTER 篇章选择系统
    - 开始界面新增 CHAPTER 选项（位于 PLAY 下方第二项）

    - 点击进入 Chapter 1-5 选择界面

    - Chapter 1 可选（包含 v1.0.0 全部游戏内容）

    - Chapter 2-5 灰色锁定预留

  Ver 1.0.2 | 测试模式菜单优化
    - 删除测试模式菜单底部的操作提示语

  Ver 1.0.3 | BUGFIX 批量修复
    - MISSION COMPLETE 文字截断修复（FloatingText.text[16]→[32]）

    - 飞机飞出画面上下翻转修复（地平线以上机头强制朝上）

    - 修复同时应用于 v1.0.0 源码

  Ver 1.0.4 | 外星飞船碰撞判定优化
    - 增大碰撞判定半径：22.0*scale+8.0 → 28.0*scale+12.0

    - 已随透视缩放（远小近大），但整体偏小，尤其在近处

    - 命中范围增大 33-44%，改善近距离射击体验

    - 项目交接 + README 更新：Claude 新会话接手开发；版本体系明确
      （v1.0.0 归档 v0.x.x，v2.0.0 将归档 v1.x.x）；编译命令/项目结构
      指向正确文件名

  Ver 1.0.4-refactored | 2026-05-10  (平行分支 — 完成后转正为开发主线)
    - 创建新源码文件：space_shooting ver2.0.0 refactored.cpp (单文件, 20个类)

    - 可执行文件：shooter_refactored (与原始 shooter 并列)

    - 原始代码/可执行文件完好保留

    - OOP 重构 — 组合优于继承、数据驱动（ChapterConfig）、函数指针替代虚函数

    - 核心类：Renderer / AudioEngine / Player / BulletManager / AlienManager /
      ParticleManager / ShockwaveManager / Boss+BossConfig / ChapterConfig+
      ChapterManager+Background / Font+UIRenderer+MenuStateMachine / Game
    - Boss 吸收状态机去重：Intro 和 Phase2 共享 updateAbsorbStateMachine()
      和 updateAbsorbAnimations()
    - Alien 多态：int alienType tag + 函数指针，不引入虚函数

    - ChapterConfig 数据驱动：5 个章节预配置差异化参数（生成速度/血量/Boss
      属性/背景色/星星数量），Chapter 2-5 预留解锁机制
    - 7-Phase 渐进重构：Phase 1-2 基础提取 → Phase 3 Boss去重 → Phase 4
      章节系统 → Phase 5 UI整合 → Phase 6 Game主控 → Phase 7 调试
    - main() 从 ~800行缩减至 15行

    - 编译修复：static const double→constexpr / AbsorbState 提升 public /
      Background 成员指针化 / soundCursor 同步 / ESC 事件边沿触发
    - BUGFIX: 逐行比对原始代码，共发现并修复 22 个 bug
        Bug 1  星空不更新 (background->update 未调用)
        Bug 2  冲击波缺绿色粒子 (spawn 未生成拱顶特效)
        Bug 3  外星 HP 不随分数增长 (spawn 缺 score 参数)
        Bug 4  gameOver menuSelection 未重置
        Bug 5  resetGame 缺 10 个状态重置
        Bug 6  Boss 冲击波碰撞 lastHitBySW 不写回
        Bug 7  Boss 冲击波碰撞 flashTimer 未设
        Bug 8  alienScale 缺 >1.0 钳制
        Bug 9  PLAY atStartScreen 顺序错误
        Bug 10 Chapter 选择后回开始界面
        Bug 11 Test 模式同 Bug 10
        Bug 12 菜单界面背景绘制错误 — 所有菜单画了透视线而非星星(drawStart/
          Chapter/Test/Option/Sound 五个方法)
        Bug 13 能量条 X 坐标偏移 — 公式从 -10*14-14 改回 (BASE_MAX_HP-1)*14
        Bug 14 LEVEL UP 浮动文字不消失 — floatingTextMgr.update() 从未调用
        Bug 15 Test 模式完全不可用 — resetGame() 清空状态；BOSS 模式 Boss 属
          性设置不完整(y/hp/maxHp/active 缺失)；BOSS 1HP 不应创建测试外星
        Bug 16 冲击波参数 double-divide — BulletManager/ShockwaveManager
          的 updateParams(score/30) 内部又 scoreLevel/30，lv 始终 clamp 到 1
        Bug 17 GameOver 界面空格键触发确认 — 改为仅 SDL_SCANCODE_RETURN
        Bug 18 Boss 一阶段释放治疗红波 — updateAbsorbStateMachine() 被 Intro
          和 Phase2 共用，healWavesEnabled=true 在 Intro 吸收完成后错误触发
        Bug 19 Boss 战败序列断裂 — updateBossDefeat() 在 frame 300 最终爆炸处
          缺少 boss.setActive(false) 和 defeatAlienTimer=0；缺少
          floatingTextMgr.update() 调用
        Bug 20 冲击波击杀后参数不更新 — collideWithAlien() 内 score++ 后未调用
          bulletMgr/shockwaveMgr.updateParams()，参数停滞在旧等级
        Bug 21 结束界面不显示 — missionComplete 被单独 else-if 分支拦截，
          updateBossDefeat() 不再执行，defeatFadeTimer 停止递增，
          遮罩透明度卡在 3/255 不可见
        Bug 22 Boss动画阶段外星飞船未全部变蓝 — alienMgr.update() 在入场
          完成时将 invincibleFrames 从 -1 覆盖为 3，3帧后 decrement 到 0
          变红；修复为 update() 后重新 setAllInvincible()
    - 当前状态：22 个 bug 全部修复，编译零错误零警告，运行正常

    - 待验证：全流程功能测试（Play/Boss/Test/菜单/暂停/GameOver）

    - 全量设计审计 (2026-05-10): 对照开发日志 v0.0.1 至 v1.0.4 全部版本，
      逐项比对原始代码与重构代码的游戏设计要素，确认以下全部一致：
      透视系统(3函数) / 星空闪烁 / WASD+翻滚 / 外星缩放 / 菱形外形 /
      圆拱基地 / 爆炸粒子 / 10HP星号 / 子弹85%失效 / 暂停+倒计时碎裂 /
      蓝无敌红受击 / 绿色冲击波渐隐 / Alien HP 3-5 / per-frame唯一命中 /
      Boss 200分触发 / 五阶段状态机(INTRO→FIGHT→PHASE2→DEFEAT) /
      HP 1000 / ∞形移动 / 蓝色光束吸收+螺旋收缩 / 治疗红波420帧30HP /
      血条400px / 5项开始菜单 / 黄色光标 / 9项测试模式 / 瞄准辅助 /
      44100Hz音频+全部SFX / BGM 32+16音符Boss切换 / 音量EQ滑条 /
      Boss 5秒爆炸+链式清场 / MISSION COMPLETE / smoothstep回中 /
      烟花2秒淡入 / 飞机上翻修复 / 方向改变翻滚 / 机头对齐消失点 /
      治疗红波+Phase2冲击波透视圆形 / 12新增字符 / 能量条得分驱动 /
      180分常绿 / Chapter 1-5选择 / 碰撞半径28*scale+12
    - 重构转正确认：OOP 重构版（Ver 1.0.4-refactored）经过 22 个 bug 修复
      及全量设计审计，已与原始版功能完全一致。后续所有开发将基于重构版进行，
      原始代码保留为历史参考

----------------------------------------------------------------

  Ver 1.1.0 | 2026-05-10  (重构转正 — 首个稳定版本)
    - 重构版（OOP，20个类）正式转为开发主线，原始代码标记为 deprecated

    - 代码文件：space_shooting ver2.0.0 developing.cpp

    - 封装：Shooter ver1.1.0.app / Shooter ver1.1.0.zip

    - macOS 11.0+ (Apple Silicon 原生)

    - 后续所有开发将基于此版本继续

  Ver 1.1.1 | 音频系统优化 — BGM/SFX 音量重新平衡
    - BGM 基频翻倍（旋律 0.025→0.045 / 贝斯 0.018→0.030；
      Boss 旋律 0.035→0.055），17 个 SFX 统一降低 33-40%
      （0.30-0.55→0.18-0.35），主音量从线性除法(x/5, x/7)
      改为平方曲线(x²/70)，每格幅度显著增大，BGM 不再被淹没

  Ver 1.1.2 | Test 模式菜单层级重构
    - Test 模式改为二级菜单：进入后先选 Chapter（1-5）再选测试目标，
      Chapter 1 开放全部 9 项测试（30/60/90/120/150/180/200BOSS/
      BOSS PH.2/BOSS 1HP），Chapter 2-5 灰色锁定预留；ESC 从分数
      选择层返回 Chapter 选择层，再按 ESC 返回开始界面
    - BUGFIX: 边沿检测变量 upWas/downWas/enterWas/escWas 被错误
      放在 else 分支内部，Chapter 选择层从不更新导致光标乱跑并无法
      点击进入 Chapter 1；移到方法作用域层级修复

----------------------------------------------------------------

  Ver 1.2.0 | Chapter 2 侧滚背景系统
    - ChapterConfig 新增 isSideScrolling 字段，Chapter 2 设为 true

    - 新增 SideScrollingBackground 类：三图层视差滚动（远0.25x/中0.55x/近1.0x）

    - 程序化地形生成，随镜头滚动动态生成前方元素、自动清理视野外元素

    - 群山地形：蓝色天空渐变，三层三角形山峰剪影，雪冠，松树点缀

    - 非洲草原地形：橙黄暖色天空，起伏丘陵，猴面包树（粗干+分枝+叶簇）、
      金合欢树
    - 赛博都市地形：深紫暗色天空，三层矩形建筑剪影，霓虹窗户灯光
      （青/洋红/黄/绿/橙），近处高楼霓虹边框+屋顶天线/水塔
    - 地形自动轮转：每约 18 秒切换一种，切换时天空 200px 渐变过渡

    - 数字键 1/2/3 手动锁定地形，0 恢复自动循环，左上角显示地形名称和模式

    - Test 模式开放 Chapter 2（跳过分数选择，直接进入侧滚背景演示）

    - Chapter 2 飞机侧视图绘制（类似战斗机剪影），WASD 移动、空格水平射击

    - 侧滚模式暂停/恢复功能正常

  Ver 1.2.1 | Chapter 2 背景系统迭代优化
    - 画风统一为镂空线条风格（与第一章一致），移除所有大色块填充

    - 天空改为黑色布满闪烁星空（400 颗，12% 十字芒星），三层地形共享星
      空背景，星空以极慢视差（0.06x）独立滚动
    - 飞机恢复第一章三角形+机翼+尾翼形状，中心线改水平（机头朝右），
      无透视效果；侧滚模式暂时移除射击系统
    - 山峰改为独立三角形轮廓：清晰尖峰+凹曲线边坡（t^1.8），边缘分形噪
      声（±2px），内部等高线纹理（2-3 条水平弧形），水墨画风格疏密有致
    - 山峰生成改为簇群式：每簇 1-3 座，22% 概率主峰（高度×1.5），簇间距
      240-380 单位留白；远山淡灰绿（g=110）雾状隐现，中山中绿（g=75），
      近山深绿（g=48）带松树
    - 草原草地改为世界坐标定位的淡黄色细线，以 0.30x 慢速视差滚动；
      灌木用放射状短弧线，猴面包树用粗干轮廓+分枝+横纹纹理
    - 赛博都市重构为"呼吸方块"：浮动于画面底部 20% 区域，非线性 cubic
      sin 缩放（±15%），方块内部 2×2/3×3/4×4 霓虹方格矩阵（五色+
      白晕双线），光源般闪烁
    - 场景过渡重设计（400 单位过渡带）：山脉→草原时山峰逐级缩小变缓
      （密度²），草原从小草(d>0.05)→灌木(d>0.35)→猴面包树(d>0.65)
      梯次出现；城市呼吸方块从 d>0.1 逐渐成长，举一反三
    - 草原地面细草线和城市地面霓虹光晕短线均受密度控制，过渡带边缘自然
      稀疏
    - 修复山脊连续性（sampleRidge 自动 wrap）、草原草地视差滚动、呼吸方
      块底部定位等多项问题

  Ver 1.2.2 | 廊桥背景系统重设计 + OOP 代码优化
    - 删除全部山峰/草原/霓虹方块随机地形代码（~450 行），SideScrolling-
      Background 类完全重写为廊桥背景
    - 星空系统保留：400 颗闪烁星+十字芒星，视差降至 0.015（几乎静止）

    - 金属地面：深灰底色(22,24,30)，5 条纵向接缝轨道以二次方透视间距排列
      （近疏远密），每条接缝为固定 y 位置的短横线，随廊桥同步向左滚动
    - 金属支柱：世界坐标等距排列（间距 330 单位），深灰主体(55,55,65)+
      两侧亮边(蓝灰 80/90/120)+底部三角支撑，每 5 根一根加粗主支柱，顶部
      向消失点 VP(CENTER_X, -60) 略微倾斜（0.85 系数）
    - 玻璃面板：每对支柱之间一面大型玻璃，Minecraft 风格设计——四边淡蓝白
      边框+四个角 L 形高光+内部横纵纹理线+中央微光对角线，无外边框，玻璃
      边缘贴合支柱倾斜角度呈梯形，完整出画后才消失
    - 地面与玻璃墙呈 120° 夹角，地板接缝消失点 VP_FLOOR_X 与支柱消失点
      VP_X 分属不同平面
    - 廊桥以 3.5 px/帧快速左移，支柱约 1.5 秒出现一根

    - OOP 优化：移除空 public:/private: 声明，移除地形系统遗留的三个 stub
      方法（setForcedTerrain/getForcedTerrain/getCurrentTerrain），提取
      pruneDoubleVec 模板消除接缝裁剪重复代码
    - 删除地形切换数字键控制（1/2/3/0）和 HUD 地形名称显示

    - BUGFIX: 修复支柱/玻璃在画面左端提前消失问题（裁剪边界从 -200 扩至
      -500，绘制边界扩至 ±100-120px）
    - BUGFIX: 修复地面接缝"桌面弹球"式上下弹跳问题（改为固定 y 位置的水平
      滚动短横线，不再根据屏幕 x 动态计算 y）
    - BUGFIX: 修复玻璃面板依赖的支柱被提前清理导致玻璃半途消失的问题

  Ver 1.2.3 | 章节解锁系统 + Chapter 2 程序化 BGM
    - 新增章节顺序解锁机制：正常游玩模式下，通关当前章节自动解锁下一章
      （第一章默认解锁，依次解锁至第五章），TEST 模式不受此限制
    - Chapter 2 专属程序化背景音乐系统：在 AudioEngine 中新增独立的 BGM
      模式，通过 volatile bool ch2Bgm 标志在主线程和音频线程间同步状态
    - 进入 Chapter 2（侧滚廊桥）时自动切换 Ch2 BGM，返回菜单/退出时切回
      普通 BGM；Boss 战时自动切回 Boss BGM（ch2Bgm && !bossFight）
    - Ch2 BGM 基于用户提供的 32-bit WAV 音频文件进行 FFT 频谱分析后手工
      转录：126 个旋律音符（16 分音符精度，250ms/音符）+ 64 个 Bass 音符
      （8 分音符精度），形成 32 秒循环
    - 音乐特征：C# 大调，~60 BPM，缓慢空灵的 Pad 质感；和弦进行 C#→A#m
      →F→C#→D#m→F→D→C# 循环；旋律以高音"闪烁"音色（G#4/A#5/C6）交
      替和弦内声部；Bass 以低音和声基础（C#2/A#1/F2/D#2/G#2）跟随和弦
    - 旋律使用 Pad 包络（柔起音 15%、满延留 80%、缓释放 12%），Bass 使
      用温暖延留包络（平滑起音、85%延留、缓释放 10%），模拟氛围持续音
    - 章节选择画面光标优化：W/S 上下移动时自动跳过未解锁的灰色章节，仅在
      已解锁章节之间循环
    - BUGFIX: ch2Bgm 标志加 volatile 关键字解决音频线程读到主线程旧缓存
      值的问题（导致进入 Chapter 2 后仍然播放普通 BGM）

  Ver 1.2.4 | 射击手感优化 + 第二章飞机重设计 + 能量罩系统
    - 外星飞船爆炸音效重设计：从低频轰鸣改为高频短促碎裂感——小爆炸用
      600Hz 噪声(35ms)+1400Hz 正弦"啪"声(16ms)，大爆炸分层叠加
      （500+200Hz 噪声 + 1800→400Hz 扫频 22ms），模拟碾碎芝麻的脆裂声
    - 碰撞箱深度自适应优化：alienScale 从 0.17+0.83*depth 改回 v1.0.0
      的 0.08+0.92*depth，hitRadius=28*scale+10，远处从 ~17px 缩至 ~12px
      （接近 v1.0.0 手感），近处保持 ~38px；瞄准辅助吸附半径与子弹碰撞
      半径完全同步，确保子弹能打中的敌人一定被瞄准辅助锁定
    - 第二章飞机可飞行范围扩大至全屏（仅留 10px 四周边距），右侧限制在
      x≤630（HUD 左侧边缘），避免遮挡分数显示
    - 第二章飞机造型重设计：上翼完整（远侧，透视缩窄至 55%）、下翼带可
      见缺口（内段+分离翼尖）；机身远侧透视缩窄（上腰 6px vs 下腰 10px），
      飞机平面与廊桥地面平行
    - 新增右侧能量罩隐形墙系统（x=642，机头尖端最右位置）：飞机触碰时触发
      持续动画特效——屏障核心白热线（青白辉光，上下衰减）+ 5 层青蓝渐变
      内层辉光向右释放 + 锯齿状闪电分支动态闪烁 + 22 个白色能量火花粒子；
      wallAnimFrame 帧计数器驱动动画持续播放，wallContactY 跟随飞机 y 坐标
    - Boss 名称系统：BossConfig 新增 name 字段，第一章 Boss 更名为
      TELAMONDO，显示在血量条左侧（自适应名称长度）；第二至五章 Boss 配置
      暂用 Ch1 占位，待后续各章专属设计
    - 开始画面版本号同步至 Ver 1.2.4

    - BUGFIX: 子弹命中后未消失问题——碰撞时同时设置 active=false 和
      canDamage=false 双重标记，增加冗余安全网

  Ver 1.2.5 | 第二章弹幕敌人系统 + 瞄准辅助 + 音效优化
    - 新增 DanmakuManager 类（OOP 封装）：弹幕敌人状态机（入场→无敌射击→
      可受击→循环），螺旋弹幕图案（参考尼尔机械纪元），入场蓝色残影特效，
      受击/击败动画（震动+爆炸粒子），紫色弹幕子弹可被玩家子弹击碎
    - Chapter 2 玩家子弹系统：新增 Bullet::sideScroll 标志，侧滚模式子弹
      恒定速度向右飞行（dx=10），不受 Chapter 1 射程衰减限制；轻微散射偏移
      (±0.16) 提升射击手感但不失精准度
    - Chapter 2 瞄准辅助系统：drawAimAssistSide() 水平射线投射+十字准星，
      敌人吸附半径 24px、子弹吸附半径 16px，与碰撞判定完全同步；无射程限制
      （区别于 Chapter 1 的 85% 射程限制）
    - 第二章 HUD：右上角 3 颗心心显示玩家 HP（初始 3），复用第一章基地受击
      减血逻辑；能级条与第一章样式一致；HUD 元素不重叠
    - 弹幕敌人透视绘制：根据摄像机位置进行深度透视变换（远小近大），菱形/
      楔形线框造型，与第一章敌机风格统一
    - 音效系统优化：
      - 新增 sndCrystalCrush()：方波扫频 2200→500Hz(24ms) + 噪音冲击 800Hz
        (18ms) + 高频正弦闪光 3600→1200Hz(10ms)，与 sndShoot/sndHit 的
        电子游戏风格统一（方波 8-bit 感+噪音纹理+水晶玻璃顶端）
      - 新增 sndPlayerHit()：三层沉重受击音效（低频 80Hz+中频 200Hz 方波+
        超低频 40Hz 轰鸣），表现玩家机体受损的重量感
    - 视觉特效：玩家受伤时头顶飘出红色立体 "HP -1" 字样（带阴影深度感），
      浮动上升后渐隐消失
    - 弹幕敌人击败后现存弹幕继续飞行（仍可被玩家击碎），不受敌人消失影响

    - BUGFIX: Chapter 2 子弹不可见（dx 过大导致一帧飞出屏幕）

    - BUGFIX: 弹幕敌人被击败后爆炸特效不触发（defeatTimer 逻辑修复）

    - BUGFIX: 右上角血量显示不正确（Chapter 1/2 HUD 互斥条件修复）

    - BUGFIX: 瞄准辅助吸附到碰撞范围外的目标（吸附半径与碰撞半径同步收紧）

  Ver 1.2.6 | Chapter 2 普敌系统 + 玩家无敌帧 + HUD 优化
    - 新增 Ch2AlienManager：第二章普通外星飞船（Ch2 普敌），OOP 封装
      - 造型：上下两个三角形拼成菱形 + 水平分割线（与第一章敌机画法一致，代码复用）
      - 登场：从任意边界（上下左右）cubic ease-out 突袭入场，目标位置在隐形墙右侧
        空间（x:640-780），入场无敌帧 3 帧 + 蓝色粒子拖尾（与第一章一致）
      - 移动：缓慢左移 0.35 px/帧（= 廊桥背景滚动速度 3.5 的 10%）
      - 射击：无敌结束后立即向玩家位置发射 5 发点射（每发间隔 8 帧），之后每 3 秒
        一轮；水晶子弹速度 1.5、hp=3（与弹幕敌人子弹参数一致）
      - 血量 10，无血条；被击败触发第一章爆炸特效；离开画面左边界后直接消失
      - 子弹继承 Ch2ShooterBase 特性：敌人离场/被击败后子弹继续飞行直至出画
    - 玩家无敌帧系统：PlayerBase 新增 invFrames 字段（所有章节通用）
      - 受伤后 1 秒（60 帧）无敌，期间飞机闪烁（每 4 帧切换可见/不可见）
      - 无敌期间玩家飞机与敌人子弹不发生碰撞
    - HUD 系统重构：提取 HUDBase 类（静态 drawScore/drawHPHearts/drawEnergyBar）
      - SCORE 右对齐到画面右边缘，格式 "SCORE:%-4d"，数字后补空格保持定宽
      - SCORE / HP 心心 / 能量条三者右端对齐（rightEdge = WIN_WIDTH - 10）
    - 优化：Chapter 2 玩家子弹缩短拖尾（12px→6px），飞行速度增加 10%（dx: 10→11）

    - 优化：弹幕敌人前后移动速度降低 42%，登场拖尾改为 5 个小型蓝色粒子点

    - BUGFIX: Ch2 普敌登场后不发射子弹（fireVolleyTimer 状态机修复）

    - BUGFIX: Ch2 普敌离场后子弹停止飞行（离场时标记 defeated 使子弹继续更新）

    - BUGFIX: 第二章右上角 HUD 混乱（SCORE/HP/能量条缺失或重叠）

  Ver 1.2.7 | 代码重构：全量命名规范化 + 继承体系 + 文件编排
    - 全量类命名规范化：Chapter 1 专用类加 Ch1 前缀（12 个），Chapter 2 专用类加
      Ch2 前缀（7 个），两章共享基础设施保持无前缀（14 个）
    - 继承体系设计（消除重复代码）：
      - BulletBase（x,y,dx,dy,active）→ Ch1Bullet（+透视射程衰减字段）
      - EnemyData（active,entering,defeated,hp,maxHp,enterFrame,enterDuration,
        invincibleFrames）→ Ch1Alien / Ch2DanmakuEnemy / Ch2Alien
      - Ch2ShooterBase（bullets,playerHP,gameOver,updateBullets,drawBullets）
        → Ch2DanmakuManager / Ch2AlienManager（子弹碰撞/飞行/绘制逻辑消除 ~80 行重复）
      - PlayerBase（x,y,rollAngle,rollTarget,invFrames）→ Ch1Player
        （+handleInput/getT），invFrames 所有后续章节通用
      - HUDBase（drawScore/drawHPHearts/drawEnergyBar）→ Ch1/Ch2 统一调用
    - 文件结构重编排：共享数据 → Ch1 数据 → Ch2 数据 → 共享基础设施 → Ch1 系统
      → Ch2 系统 → 游戏框架（Game/main），父类在前子类在后
    - 移除 Ch2ShooterBase 中的 drawHUD（HUD 不属于敌人系统，已移至 HUDBase）

    - 禁用弹幕敌人（DanmakuManager）为 Ch2 普敌测试让路，代码保留后续恢复

  Ver 1.2.8 | Chapter 2 测试模式增强 + 多敌人同屏 + 瞄准辅助组件化
    - 测试模式新增按键控制（仅 TEST 模式可用）：
      - 按 1：刷新一个 Ch2 普敌（边沿触发，按一下只刷一个）
      - 按 2：刷新一个弹幕敌人（入场 x 随机范围 400-630，画面中央到隐形墙之间）
      - 画面左上角显示 "press 1/2" 提示文字
      - 能量条暂时冻结（细节后续设计），分数正常累计（普敌 1 分，弹幕 5 分）
    - Ch2AlienManager 向量化重构：单敌人 → std::vector<Ch2Alien>，支持多个普敌同屏

    - Ch2DanmakuManager 向量化重构：单敌人 → std::vector<Ch2DanmakuEnemy>，
      支持多个弹幕敌人同屏，重新启用 update/draw
    - 瞄准辅助组件化（AimAssist）：合并为单一类嵌入 PlayerBase，每个玩家实例自带
      瞄准辅助状态（snapProgress + update + draw），后续章节直接复用
    - PlayerBase 提取：x/y/rollAngle/rollTarget/lastMoveDir/invFrames/aimAssist，
      Ch1Player 继承后添加 handleInput/getT
    - 优化 Ch2 普敌入场：只会从画面四条边刷新（startX/y 均在边缘外），突袭到隐形墙
      右侧空间（targetX: 640-780）；右侧入口修正为 x=810（真正画面外）
    - 优化 Ch2 弹幕敌人入场：增加右下角刷新位置，随机从右上角或右下角入场

    - BUGFIX: 按键 1/2 每帧重复触发 → 添加边沿检测（keyNow && !keyWas）

    - BUGFIX: 弹幕敌人入场阶段 leg 字段未初始化导致绘制异常

  Ver 1.2.9 | 玩家飞机继承体系 + NightElf 三炮战机 + 多枪系统
    - Player 基类虚方法体系：draw / handleInput / getGunCount / getGunOffset，
      每种飞机子类自行实现绘制、操控和枪口布局
    - TrainingPlane（Chapter 1 训练机，原 Ch1Player）：透视三角机身 + 机翼线 +
      尾翼，单炮布局，保留 getT() 透视定位能力
    - NightElf（Chapter 2 暗夜精灵）：30° 锐角机头 + 120° 内凹燕尾（以机翼线
      为对称轴翻转），上半机身扫描线填充，3 炮布局（机头尖端 + 上/下翼尖平行
      线前端），侧滚模式火力三倍
    - Druid（第三章备用设计）：双尾翼 boomerang 造型 + 中心导航线，代码已预留，
      后续章节激活
    - 多枪射击系统：getGunCount() 返回枪数，getGunOffset(idx) 返回每把枪相对
      坐标，射击循环遍历所有枪口，addBulletSideScrollAt 支持任意枪位发射
    - Game 类重构：player 从具体子类改为 Player* 指针，根据章节动态切换
      TrainingPlane/NightElf；drawPlane()/drawPlaneFlat() 移入各子类 draw()
    - Ch2ShooterBase/enemy manager 参数泛化：Ch1Player& → Player&，任意飞机
      均可与第二章敌人系统交互
    - 无敌闪烁逻辑下沉：从 Game::drawPlaneFlat 移入各子类 draw()，每架飞机自
      行处理 invFrames 闪烁遮蔽

  Ver 1.2.10 | 代码质量提升 + 敌人互斥 + 碰撞优化 + Bug 修复
    - 代码重构：MenuKeys 辅助结构消除 4 个屏幕处理器的按键读取样板代码；
      Player::resetState() 消除三架子机 reset() 重复；Ch2ShooterBase::
      computeHPColor() 消除两处 HP 颜色计算重复；消除全部 11 个编译器 warning
    - 敌人互斥系统：弹幕敌人 baseY 垂直分散(200-360)；入场目标间距检测(≥80px，
      最多5次重试)；per-enemy moveSpeed 错开振荡节奏
    - 玩家受击优化：同一帧只扣一滴血（命中后 break），触发无敌闪烁

    - 隐形墙碰撞优化：机头位置检测（Player::getNoseOffset()虚方法），NightElf
      机头不再越过隐形墙
    - Game Over 界面重构：删除 EXIT，新增 BACK TO MAIN MENU，PLAY AGAIN 改为
      重新进入当前章节
    - BUGFIX: 瞄准辅助遗漏弹幕敌人 — drawAimAssistSide() 新增 dmMgr 敌人本体
      和子弹检测
    - BUGFIX: 玩家受伤 HUD 心心不减少 — Ch2ShooterBase 的 playerHP/gameOver
      改为引用，两个 enemy manager 共享同一份状态
    - BUGFIX: 暂停倒计时粒子/数字错位 — spawnDigitShatter 从切换帧改到每数字
      第一帧触发
    - BUGFIX: 弹幕敌人入场后瞬移 — 用 per-enemy moveSpeed(0.005~0.009)替代
      随机 movePhase，入场结束从 0 开始平滑振荡
    - BUGFIX: 倒计时结束暂停菜单闪一帧 — drawPauseMenu 加 paused 条件

    - BUGFIX: 测试模式 Chapter 1 显示飞机2 — 分数选择确认时加 resetGame()调用

  Ver 1.2.11 | 旁白对话系统 + 字体增强 + 星空全屏
    - 旁白系统（NarrationSystem）：章节开场/结尾剧情讲解，文本框暗绿配色
      （夜精灵色系），打字机逐字出现 + Mario easeOutBack 弹出特效，按 ENTER
      逐行推进，结束后自动进入游戏。PLAY/CHAPTER SELECT 触发，测试模式跳过
    - 队友对话子系统：游戏内画面左侧弹出，打字机出现 + 停留 3.3 秒 +
      easeOut 非线性上浮渐隐消失，自动提行（38字符/行），弹出时电报音效
      （sndTeletype: 2400→1800Hz 科幻终端提示音；sndVillainTalk 反派音效保留）
    - 对话绘制层级：置于粒子层之后、敌人/子弹层之前，可被敌人遮挡

    - 对话位置位于 Chapter 1 地平线（HORIZON_Y=200）

    - 字体库扩充：新增 24 个键盘可打符号（逗号、撇号、引号、问号、括号、
      比较符、数学符号等），消除旁白中标点乱码
    - 字体渲染升级：drawChar/drawString 支持浮点缩放（SDL_FRect+
      SDL_RenderFillRectF），新增 mul 参数支持整数倍像素渲染（1x/2x/3x等）
    - 星空全屏优化：菜单/旁白界面星空布满全屏（drawStarsFullscreen），
      游戏场景保持天空区域限定绘制
    - 第一章 30 分时触发队友对话测试（三句 + 一句长句验证自动提行）

  Ver 1.2.12 | 第一章剧情旁白 + 对话系统完善 + 代码质量
    - 第一章开幕旁白剧本：Martha 故事线（Stellar Calendar 24th / Life 基地
      遇袭 / 塔台通讯中断），9 个文本框，支持 \n 换行+自动折行(36字符/行)，
      文本框高度随行数动态增加
    - 中心旁白增加多行渲染：Line 结构改为 vlines 向量，按 \n 拆分后每行
      独立折行，draw() 根据 maxLineLen 和 numLines 计算动态框尺寸
    - 对话系统增加说话人标签（speaker 字段）：Ally / Tower，名字用浅黄绿色
      (180,200,160) 渲染于内容上方，同一说话人连续发言时名字保持不动仅内容切换，
      说完后名字+内容一起上浮消失
    - 对话触发改为分数跨阈值检测（lastScore < 阈值 && score >= 阈值），
      测试模式选高分不会触发之前所有对话；得分 15 新增塔台通讯恢复提示
    - 对话显示停留时间缩短至约 2 秒（200→120 帧）

    - 章节旁白打字机增加电报音效（popCTicks + sndTeletype）

    - 基地燃烧特效：每 4 帧随机位置橙黄火焰粒子，每 30 帧大型爆燃，
      每 6 帧白色烟雾上升，贴合剧情中基地遇袭残骸场景
    - 字体 g 字型优化：碗部扩至 4 行(0x0E/11/11/0E)，降部保持右缘单像素，
      底部 2 像素钩脚(0x06)
    - 代码质量优化：
      - Game 增加 ~Game() 析构函数释放 background/sideBg 堆内存
      - FloatingTextManager::clear() / Ch1AlienManager::pushAlien() 封装
      - Ch1Boss::triggerPhase2() 封装 Phase2 状态切换（替代 10 行 Ref 操作）
      - Ch1ParticleManager → ParticleManager / Ch1BulletManager → BulletManager
        重命名（Ch1/Ch2 共享去掉 Ch1 前缀）
    - BUGFIX: 测试模式任意分数选入皆为 30 分 — resetGame() 清零
      testScoreSelection，修复为先保存 savedSel 再取分
    - BUGFIX: 对话队列重复触发 — startDialogue() 增加 erase 已播放条目逻辑

  Ver 1.2.13 | 暂停页对话历史 + 对话系统完善 + 开场流程优化
    - 暂停页面重新设计：左半菜单左对齐，A/D 切换焦点；右半对话历史日志，
      默认显示最近 3 条（100%-50%-30% 透明度），焦点移动→边界滚动，
      每次打开自动刷新至最新，自动折行（30字符/行）
    - 对话系统完善：说话人更新为 Ally / Bryssa / Tower，
      新增 8 个分数触发点（0/3/15/20/30/40/50/61），13 条对话内容
    - 对话历史记录：每条对话上浮消失后写入 history，pauseHistoryFocused 切换

    - 开场流程优化：旁白结束后立即弹出 0 分对话，敌人等对话结束后才刷新
      （enemiesEnabled 门控）；ESC 跳过全部开幕旁白直接进入游戏
    - ENTER 不再跳过游戏内对话（仅旁白可用 ENTER 推进）

    - SPACE 键仅用于射击，所有确认/跳过操作改为纯 ENTER

    - 所有菜单光标/焦点改为边界钳制（不再回绕）

    - 测试模式新增 0 分选项

    - 字体优化：双引号收紧(0x0A)、小写 g 多轮调整定型
      (0x00/0E/11/0F/01/11/1F)
    - BUGFIX: 历史对话 DLine::historyRecorded 未初始化导致记录丢失

    - BUGFIX: 历史对话符号错误（idx 上下索引反号）

    - BUGFIX: 2条对话时无法显示两条 — maxScroll 调整

  Ver 1.2.14 | OOP 重构 + Bug 修复
    - OOP 重构：NarrationSystem 拆分为 3 个独立类
      （NarrationSystem 中心旁白 / DialogueSystem 游戏对话 / DialogueHistory 历史记录），
      DialogueSystem 组合 DialogueHistory，职责清晰
    - 对话触发标志优化：8 个 dTrigXX 布尔值替换为 triggeredScores[64] 数组，
      新增对话只需添加对应分数索引
    - 提取共享绘制函数 drawTextLine()，消除中心旁白/对话/历史三处重复绘制代码

    - 暂停页历史滚动逻辑封装至 DialogueHistory::moveUp()/moveDown()/resetView()

    - BUGFIX: Boss 蓝色追踪弹变成白色横弹 — addBossBeam() 未初始化 sideScroll=false

  Ver 1.2.15 | 对话系统扩充 + 第一章章节守卫 + 测试模式历史预填充
    - 对话系统大幅扩充：
      - 说话人名称更新：Ally (ai copilot) / Tower (ai) / Bryssa from Tower
      - 新增 13 个分数触发点（55/70/80/90/105/120/160/180/195/210），
        覆盖从系统自检到 Telamondo 主力舰逼近的完整叙事弧线
      - Score 3 对话拆为两句，speaker 由 Bryssa 改为 Ally
      - Score 20/61 第二句 speaker 由 Tower 改为 Bryssa from Tower
      - Score 50 移除 Ally 第二句；Score 61 第二句加 "Watch out!"
      - Boss 半血进入二阶段时触发新对话：
        "Telamondo can absorb energy!" 三句
      - 开幕旁白 Flight code B295 → 21395
    - 第一章章节守卫：所有 Ch1 对话触发包裹在 chapterNumber==1 检查中，
      第二章 score 从 0 重新计数不会触发第一章对话；
      enemiesEnabled 在非第一章直接启用
    - 测试模式历史预填充：从任意分数进入测试，低于该分数的所有对话
      直接写入暂停页历史日志，方便回顾完整对话链；
      PH() 宏精简重复代码
    - Boss Phase2 对话触发统一：新增 bossPhase2DialogueTriggered 标志，
      非分数触发归入统一对话区块，所有对话代码集中管理
    - triggeredScores 数组扩容：64 → 256，支持 200+ 分数对话

    - 字体 g 字型重新设计：(0x00/0F/11/0F/01/19/0F) 顶部留空、碗部饱满、降部左勾

  Ver 1.2.16 | 角色对话音效 + Boss 震爆音量修复
    - 角色专用对话音效：
      - Ally (ai copilot): 高频正弦 blip（3200→2200Hz），AI 助理感
      - Tower (ai): 中低频 noise sweep（800→400Hz），塔台无线电感
      - Bryssa from Tower: 中高频方波 sweep（1500→600Hz），人类通讯官
      - 系统消息 (空 speaker): 纯正弦 ping（1000Hz），系统提示
      - 开幕旁白保持原 sndTeletype() 音效不受影响
      - DialogueSystem 新增 currentSpeaker() 方法，按角色名自动匹配音效
    - 对话历史即时写入：pop-in 动画结束后立即写入 history，
      不再等到整条对话上浮消失才记录
    - BUGFIX: Bryssa 对话匹配到 Tower 音效 — "Bryssa from Tower" 包含 "Tower"
      导致 Bryssa 优先匹配错误，调整 if-else 顺序修复
    - BUGFIX: Boss 二阶段震动/爆炸音效过大、不受音量控制 —
      sndShake 音量 0.22→0.12、时长 100→80ms、触发间隔放宽；
      sndExplosionBig 三层音量分别降低（0.19/0.14/0.10 → 0.12/0.09/0.07）

  Ver 1.2.17 | 结束页面重设计 + 透视速度 + 第二章开幕旁白 + BGM优化
    - 结束页面（Mission Complete）重设计：
      - MISSION COMPLETE 用旁白风格墨绿文本框展示
      - 两个菜单选项居中显示：NEXT CHAPTER / BACK TO MAIN MENU
      - 光标改为黄色下划线；NEXT CHAPTER 直接进入下一章开幕旁白
    - 第一章透视移动优化：
      - 移动速度随高度缩放（speedFactor = perspWidth(y)/perspWidth(400)）
      - 靠近地平线速度慢（~0.7x），靠近下端速度快（~1.3x），中间保持默认
    - 暂停页面优化：右半区域新增 "HISTORY DIALOGUE" 白色标题，与 PAUSED 同高

    - 开始页面和暂停页面取消 BGM；预留 setStartBgm() 接口

    - 第二章开幕旁白完整重写（12页），文学性氛围感大幅提升

    - 对话单行最大长度 38→20 字符，紧凑显示

    - 开幕旁白细节修正：Flight code B295→21395，Huntress 措辞优化

    - 字体库补充小写 q（p 的镜像对称），所有 ASCII 可打印字符完整

    - BUGFIX: 第一章子弹横飞 — 透视移动中 x 重赋值与 getT() 不一致，
      简化为独立 x/y 移动+速度缩放，移除透视线保持逻辑
    - BUGFIX: 第二章背景玻璃在画面内刷新 — genAhead 预生成边距 300→600，
      星点 wrap 边距 200→400

  Ver 1.2.18 | 球体Boss战 + 夜精灵能量条 + 对话修复
    - 新增 Ch2SphereBoss 球体Boss类：蓝色六角密铺菱形球体，7 状态机
      （入场→激活→战斗→碎裂→震动→突袭飞散→结束），固定种子乱序碎裂，
      菱形碎片物理（重力/落地/透视散落），碰撞检测（圆形判定）
    - 球体入场动画：右侧滚动入场，背景随距离减速至静止

    - 激活动画：菱形从下到上蓝→橙随机窗口逐个激活，~160 帧完成

    - 受击碎片弹出：每发子弹命中弹出 1 个菱形，HP=500，HP=0 进入碎裂流程

    - 碎裂→震动→突袭全流程：全菱形碎裂弹出后连锁震动传播(fade-in振幅→飞出)，
      碎片向随机方向飞散消失
    - 测试模式 Chapter 2 直接进入球体 Boss 入场流程（替代手动刷怪）

    - 新增 NightElfEnergy 白色能量条系统：50 次命中蓄满，0.5s 无命中衰减，
      蓄满触发 15 秒 triple-fire 三炮模式，最后 3 秒加速倒计时提示音
    - 新增 sndTripleOn(crisp metallic ping) / sndTripleCountdown(short beep)

    - 白色能量条 HUD 绘制在绿色能量条下方，triple 脉冲/充能/衰减三色态

    - NightElf 枪数降至 1（triple-fire 激活时覆盖为 3 炮）

    - 能量命中追踪链：manager 更新+球体碰撞→统一汇总→energy.update→命中清零

    - 对话触发防级联：每个触发点同步设置 lastScore=score，帧级仅在无对话时更新

    - 对话内容精简：移除 Score 40/90/210 部分台词，Score 70 措辞修正

    - BGM 静音范围扩大：章节选择/测试选择/设置/音效菜单/任务完成全部静音

    - BUGFIX: 对话级联触发 — lastScore 在各触发点同步更新+帧级加对话活跃守卫

    - BUGFIX: 历史焦点需按两次上键 — focusSlot 在 resetView/moveUp/moveDown
      中钳制至 maxSlot
    - BUGFIX: NEXT CHAPTER 回到开始画面 — resetGame()后设置 atStartScreen=false

    - BUGFIX: 部分菜单未静音 — BGM off 条件覆盖全部非游戏界面

  Ver 1.2.19 | 自动出敌系统 + Ch2训练机 + 测试双入口
    - 新增自动出敌系统（autoSpawnPhase 状态机，从球体Boss DONE后触发）：
      - Wave 1: 3只普敌，全部消灭+score≥3→Wave 2
      - Wave 2: 5只普敌，全部消灭+score≥8→Wave 3
      - Wave 3: 初始5只+4轮增援（每消灭3只补3只）+飞出即补→弹幕敌人
    - 全部波次统一逃逸检测（帧间delta公式）：敌人飞出画面立刻补充，
      aliveLast/aliveKills last快照+queued计入防重复检测
    - 统一spawn pump：queue-driven，不限phase，逃逸替补和增援均在0.2s间隔生成

    - 波次过渡使用分数门槛（真实击杀），飞出≠击杀不推进波次

    - 新增 Ch2Trainer 横版训练机（与Ch1 TrainingPlane同一飞机侧视角度，1:1尺寸），
      Ch2当前使用trainer单发基础射击，NightElf类和NightElfEnergy系统搁置保留
    - 第二章测试模式双入口：SPHERE BOSS FULL（完整Boss入场+动画+战斗）/
      COMBAT ONLY（跳过Boss直接进入自动出敌波次）
    - 删除 press 1/2 手动刷怪代码和key1Was/key2Was边沿检测变量

    - 新增 Ch2AlienManager::countLiving() 统计存活普敌数

    - BUGFIX: autoSpawnPhase=2/4逃逸替补入队后spawn pump未执行 —
      将spawn pump从phase-gated改为queue-driven（if autoSpawnQueued>0）
    - BUGFIX: Wave 3逃逸检测无限刷新 — expectedAlive公式累积重复计算，
      改为帧间delta（aliveLost-killsGained）

  Ver 1.2.20 | 脉冲技能系统 + 技能球获取流程
    - 新增 Ch2PulseSystem 脉冲技能系统：
      - 绿色能量条与白色能量条等长（10*14px），30格容量，命中敌人+1格
      - 能量满后HUD呼吸动画（亮绿↔暗绿），按一次Shift即释放全屏白色冲击波
      - 冲击波双层同心圆扩散（8px/帧），碰撞清屏弹幕+对敌人1伤害
      - 碰撞ID机制（Ch1Shockwave同款）：每波唯一ID，lastHitByPulse防重复伤害
    - 新增 Ch2SkillOrb 技能球获取流程：
      - 击败第一个弹幕敌人→动画结束后原地生成漂浮技能球
      - 技能球缓慢漂浮（边界反弹），包裹18边形白色保护罩（18次命中击碎）
      - 子弹命中护罩有碎裂粒子特效+音效，一次性全部碎开向外飞散渐隐
      - 护罩碎后显现金黄色发光核心（多层羽化边缘），按住Shift靠近吸收
      - 吸收持续5秒，期间粒子从核心飞向玩家，松开Shift能量快速流失归零
      - 吸收完成解锁脉冲技能，同时刷10只普敌供玩家体验
    - 吸收期间绿色能量条同步增长（每10帧+1格，300帧刚好满30）

    - 测试模式新增第三入口 PULSE ORB TEST（分数25+弹幕敌人，直接测试技能球流程）

    - 隐形能量墙限制条件优化：仅在战斗阶段（autoSpawnPhase>0）生效

    - 清理死代码：charge蓄力机制残留（charging/chargeTimer/shakePhase）、
      key1Was/key2Was手动刷怪变量残留
    - BUGFIX: 能量满后按Shift无反应 — release()残留charge守卫条件导致脉冲从不释放

    - BUGFIX: 吸收时能量条不增长 — getFill()的unlocked门控在吸收期间锁0%

    - BUGFIX: 吸收时能量条不增长重新修复 — getFill()彻底移除unlocked门控

  Ver 1.2.21 | 代码架构重构：单文件→28个多文件模块化
    - 全量代码重构：从6633行单文件拆分为28个头文件+1个main.cpp的结构化项目
    - 文件结构映射：
      - types.h — 全部18个struct（Star/FloatingText/BossConfig/ChapterConfig/FontChar/ActiveSound/
        MenuItem/Ch1Particle/BulletBase/Ch1Bullet/EnemyData/Ch1Alien/Ch1Shockwave/Ch1HealWave/
        Ch2EnemyBullet/Ch2PulseWave/Ch2DanmakuEnemy/Ch2Alien/ShieldDebris）
      - constants.h — WIN_WIDTH/WIN_HEIGHT/CENTER_X/HORIZON_Y/perspLeft/perspRight/perspWidth
      - font.h / renderer.h / audio.h — Font+Renderer+AudioEngine（共享基础设施）
      - floating_text.h — FloatingTextManager
      - dialogue.h — DialogueHistory + DialogueSystem + drawTextLine
      - narration.h — NarrationSystem
      - particles.h — ParticleManager（spawnExplosion/spawnWhiteParticle/spawnGreenParticle等）
      - aim_assist.h — AimAssist组件
      - player.h — Player基类 + TrainingPlane/Ch2Trainer/NightElf[DORMANT]/Druid[DORMANT]
      - bullets.h — BulletManager（addBullet/addBulletSideScrollAt/update/draw）
      - chapter_manager.h — ChapterManager（章节配置/解锁/切换）
      - ui.h — UIRenderer + MenuStateMachine + MenuKeys
      - game.h — Game类（2144行，含全部状态机+全部方法）
      - main.cpp — #include所有头文件 + main()
      - ch1/ — Ch1ShockwaveManager / Ch1AlienManager / Ch1Boss / Ch1Background（4个文件）
      - ch2/ — Ch2Background / Ch2ShooterBase / Ch2DanmakuManager / Ch2AlienManager /
        Ch2SphereBoss / Ch2PulseSystem+Ch2PulseWave / Ch2SkillOrb / HUDBase（8个文件）
    - 全部实现内联在头文件中（#pragma once，单翻译单元编译）

    - 新编译命令：cd "v2.0.0/multiple code files" && make

    - 休眠资产[DORMANT]标注规范：NightElf（门禁序列激活后）/
      NightElfEnergy（NightElf联动）/Druid（Chapter 3）/
      setStartBgm() — 均为完整保留的未激活代码
    - 旧单文件存档为 v2.0.0/space_shooting ver1.2.20 single-file archive.cpp

    - 零行逻辑修改，纯剪切粘贴重构

  Ver 1.2.22 | 第二章完整流程整合：门禁序列 + NightElf升级 + 章节Boss + 结局
    - 新增 Ch2GateScene 门禁场景（新文件 ch2/ch2_gate.h，316行）：
      - 画面右侧巨大真空门（双面板+上下监视扫描头+指示灯），镂空线稿风格
      - Ally 脉冲解锁序列：以飞机为圆心的5层同心圆脉冲组，每2秒一组共5组，
        每组消耗绿色能量条 1/5（满格30→0），最外圈圆最亮形成渐变
      - 门指示灯亮起→窄扇形红色扫描光束锁定教练机→Moonwell AI 三段对话
      - 大门双面板缓缓分开，玩家驾驶飞机向右飞入门内（白色淡入转场）
      - ESC 一键跳过整段门禁演出（直接开门）；门禁阶段禁用射击
    - 新增 Ch2LabScene 中央研究室场景（新文件 ch2/ch2_lab.h，103行）：
      - 圆形研究室（圆墙+径向肋条+旋转雷达光扫+顶光锥），廊桥背景停止滚动
      - 暗夜精灵号停放在基座上（底座呼吸光环+光柱），"CENTRAL LAB"白色大字
    - 新增 Ch2WardenBoss 章节Boss（新文件 ch2/ch2_boss.h，350行）：
      - 名称 "MOONWELL WARDEN"，350 HP，175 HP 半血狂暴（ENRAGED）
      - 造型：大菱形核心+8枚环绕碎片+4片旋转刀刃，镂空线稿风格
      - 三种弹幕模式循环：放射弹幕/瞄准三连发/螺旋弹幕，狂暴后加速加密
      - 周期性召唤2只普敌增援；水晶子弹可被玩家子弹击碎；Boss战自动切Boss BGM
      - 击败流程：DYING 链式爆炸→清场→结局旁白→MISSION COMPLETE
    - 第二章流程状态机（Ch2Flow 七段）：
      门禁GATE→廊桥CORRIDOR→球体SPHERE→追逐CHASE→研究室LAB→Boss BOSS→结局ENDED
      - 廊桥飞行对话结束后，球体Boss在任意滚动距离前方1200单位处生成
        （新增 startEnteringAt，修正世界坐标与滚动距离的耦合）
      - 脉冲解锁+波次清场后自动进入中央研究室
      - 触碰暗夜精灵号→粒子汇聚升级动画→飞机切换为 NightElf
      - NightElf 激活：白色能量条（绿条下方），命中50次充能→15秒三炮模式
        （triple-fire），最后3秒倒计时提示音，"TRIPLE FIRE!"浮动文字
      - 升级后 Warden 拦截去路→击败→清场→结局旁白（4页）→MISSION COMPLETE
      - NEXT CHAPTER 解锁第三章并直接进入其开幕旁白
    - 第二章剧情对话全部接入（12组触发点，含新说话人 Martha / Moonwell (ai)）：
      门前 Bryssa/Martha/Ally 对话→Moonwell AI 扫描对话→廊桥对话→球体遭遇/
      激活→追逐提示→技能球提示→脉冲解锁提示→研究室发现→升级讲解→Boss警报→狂暴警告
    - 新增角色语音音效：sndMarthaTalk / sndMoonwellTalk
    - 测试模式第二章扩展为6入口：SPHERE BOSS FULL / COMBAT ONLY / PULSE ORB TEST /
      GATE SEQUENCE / NIGHTELF LAB / WARDEN BOSS
    - HUD 扩展：白色能量条三色态（充能白/衰减灰/三发脉冲青）+ 通用Boss血条
      （球体Boss血条"???"显示于球体上方，按剧本补全）
    - 瞄准辅助吸附章节Boss；脉冲释放移至公共区（Boss战/研究室同样可用）；
      Boss/球体命中计能量（白色能量条与绿色能量条联动）
    - 无头集成测试验证全流程（门禁→结局 全阶段自动机 + Ch1 回归冒烟，
      GATE/SPHERE/CHASE/LAB/BOSS/ENDED 各阶段状态断言全部通过）
    - BUGFIX: 门禁阶段飞机被活动范围钳制无法进门 — 开门后放宽活动范围至门内
    - BUGFIX: Boss死亡后流程卡死 — 结局旁白触发时同步切换 C2_ENDED
    - BUGFIX: 研究室升级后飞机被吸回基座 — 停泊位置赋值限定在触碰前
    - BUGFIX: 场景转场白屏卡死 — 淡出计时器移入公共区统一推进

  Ver 1.2.22 (补充) | 第一/二章旁白对话双语语音配音 (VOICE OVER,版本号不变)
    - AudioEngine 新增语音播放通道:playVoice()/stopVoice()/voicePlaying()
      - 支持 8/16-bit int 与 32-bit float WAV(任意采样率/声道),
        加载时归一化为 mono 44.1kHz float,线性重采样
      - 与 SFX 通道独立混音(固定增益 0.9),语音不受 BGM 静音影响;
        播放/停止均在 SDL_LockAudioDevice 保护下操作(回调线程安全)
      - 新增 voiceCrc32()(标准 CRC-32/IEEE,与 Python zlib.crc32 逐字节一致)
    - 语音挂钩:旁白页切换/对话行切换时自动播放配音
      - 文本 → 文件映射走 voice/voice_manifest.txt(<lang> <crc32> <相对路径>,
        启动时加载),文本改动自动映射新文件,缺失文件静默跳过
      - 旁白打字机字母弹出音效(sndTeletype)始终保留,与配音同时播放
      - 对话行有真实语音时自动屏蔽合成"电报音"音效(无声文件则保留原音效)
      - ESC 跳过旁白 / 暂停 / 旁白结束 时自动停止语音
    - 设置新增 VOICE LANG 开关(OPTIONS 菜单第 2 项),三态循环:
      0=中文(ZH,默认)/ 1=English(EN)/ 2=OFF(关闭配音,仅保留字母弹出音效)
    - 音频文件按章节+角色分文件夹命名:
      voice/<zh|en>/ch<N>/nar_XX.wav(旁白页)+ dlg_<角色>_XX.wav(对话句),
      角色缩写 narrator/martha/bryssa/ally/tower/moonwell/system
    - 语音资源管线 tools/voice_pipeline.py:
      - collect:从 game.h 自动提取全部旁白页+对话行并按章节归属(第一章对话
        位于 updateGameplay,第二章位于 updateCh2Gate~updateBossDefeat 区间),
        C 转义还原原文,内置 104 条中英对照表(100% 覆盖)→ voice_script.json
      - generate:按"角色→音色"映射合成双语 WAV(edge-tts 在线 / piper 离线 /
        gpt-sovits API 三种后端),--chapters 按章节过滤,6 线程并行+失败重试,
        纯标点句("……")自动跳过(无声停顿),结尾自动生成 voice_manifest.txt
    - 已生成第一+二章全部语音(182 个文件,34MB,22050Hz 16bit mono):
      第一章 11 页旁白+34 句对话;第二章 15 页开幕旁白+4 页结局旁白+32 句剧情对话;
      角色音色映射:旁白 zh-Yunyang/en-Christopher;Martha zh-Xiaoxiao/en-Jenny;
      Ally zh-Yunxi/en-Guy;Tower zh-Yunjian/en-Brian;Bryssa zh-Xiaoyi/en-Sonia;
      Moonwell zh-Yunjian(慢速)/en-Andrew;系统 zh-Yunxi/en-Aria
    - 无头测试验证:manifest 加载/zh·en·OFF 三态/第一章第二章语音命中/
      标点静默/选项三态循环,全部通过
    - BUGFIX: 语音通道推入无锁保护 — 与音频回调存在数据竞争,补
      SDL_LockAudioDevice 包裹

  Ver 1.2.23 | 测试模式退役：全量存档 / 读档回到存档瞬间同一帧 / 自由读档与另存为
    - 存档系统上线，TEST 测试模式改为隐藏入口（--test），主菜单原位置换成 LOAD GAME

    - 新增存档系统（新文件 save_system.h）：
      - 文件格式：magic "SFSSAVE0" + 格式版本 + 游戏版本 + payload 长度 + CRC32 + payload
      - 写入采用 临时文件 + rename，中途失败不会留下半个存档
      - payload 最前端是 SaveMeta 摘要（章节/章节名/分数/血量/流程段落/游戏版本/时间戳），
        读档列表只解析摘要，不反序列化整份状态

    - 全量状态快照（Game::serializeAll()，字段顺序即存档字节顺序）：
      - 玩家三架子机（教练机/Ch2Trainer/暗夜精灵号，含无敌帧、滚转角、瞄准辅助进度）、
        飞行中的玩家子弹（透视弹道与侧滚弹道）、基地血量、分数、难度计时、章节与
        Ch1 剧情触发位图 triggeredScores[256]
      - Ch1：外星飞船（含入场动画/被吸收状态）、Boss TELAMONDO 全状态（阶段/吸收状态机/
        治疗波）、冲击波（含 pending/lastLevel，避免读档瞬间白放一发）、粒子、浮动文字、星空
      - Ch2：门禁场景（阶段/脉冲环组/开门进度/扫描光束）、球体Boss（六角密铺格子激活与
        碎裂顺序表/碎片物理）、普敌与弹幕敌人（含螺旋角/发射计时）、敌方子弹、技能球
        （护罩/吸收进度/碎片）、绿色脉冲能量与冲击波环、白色能量条（含三连发剩余时间）、
        Warden Boss（弹幕模式循环/召唤计时）、研究室计时、廊桥滚动与星星/立柱/地板缝
      - 对话系统（队列/打字机已显示字数/淡出位移/历史记录/暂停菜单滚动焦点）、
        旁白系统（当前页/逐字进度）、流程状态机 ch2Flow 与全部 *Queued 一次性标记、
        自动出敌调度、章节 Boss 击破演出计时

    - 读档语义（按需求定制）：读档后定格在"暂停 + 暂停菜单"，画面就是存档瞬间那一帧；
      玩家选 RESUME → 3-2-1 倒计时 → 从原状态继续
      - 读档后强制 countdown=-1，避免"读档即自动倒计时"，一定要玩家自己退菜单
      - 复位所有按键边沿检测与对话/旁白的 ENTER 抑制，防止读档瞬间被按住的键跳过内容
      - 跨章节读档：按存档章节重建章节配置与星空背景、重新绑定 Boss 配置与机体指针

    - 存档界面（新文件 save_menu.h）：
      - 暂停菜单新增 SAVE GAME / LOAD GAME（原 5 项 → 7 项）
      - 槽位：AUTO（自动槽）+ SLOT 1-6，列表显示章节/流程段落/分数/血量/存档版本/时间
      - 空槽位、损坏或版本不符的槽位分别显示 EMPTY / UNREADABLE，可读档时给出精确失败原因
      - 覆盖槽位、覆盖文件、读档覆盖当前进度都有 ENTER/ESC 二次确认
      - 游戏内迷你文件浏览器（自由读档 + 另存为任意路径）：W/S 移动、ENTER 进入目录或选中、
        BACKSPACE 返回上级、ESC 返回；另存为首行是自动命名的 [ NEW FILE ]

    - 自动存档：每章开始（PLAY/CHAPTER/下一章/重开）、Ch2 每次流程段落切换写入 AUTO 槽
      （旁白阻塞结束后才落盘；结局段落不覆盖 AUTO 槽）

    - 存档位置：优先 ./saves/（开发时在可执行文件旁，与 voice/ 同级），
      不可写则自动回退 ~/Library/Application Support/StarFoxSpaceShooter/saves/
      （发布成 .app 后从 Finder 启动时 cwd=/，且写进 bundle 会破坏签名）；
      环境变量 SFSS_SAVE_DIR 可强制指定

    - OPTIONS 设置持久化：瞄准辅助 / VOICE LANG / BGM·SFX 音量 / 三段 EQ 存入
      saves/settings.dat，改动即落盘，退出前再存一次，启动时自动恢复

    - 版本与结构规则：存档只按格式版本把关（SAVE_FORMAT_VERSION）+ CRC32 校验，
      游戏版本号仅作显示诊断，因此小版本更新不会作废玩家存档；
      任何改动 serializeAll 字段顺序/类型的修改都必须把格式版本 +1

    - 开发自检 ./shooter --selftest（无窗口，返回码 0=PASS），把"存档→继续玩→读档"
      的等价性做成可回归验证：
      - 4 个状态场景（第二章追逐战 / 第二章门禁对话 / 章节Boss Warden / 第一章 Telamondo）
        各跑 180 帧改变状态后读档，比对"重新序列化的状态字节"与"重绘画面的像素"
      - 结果：4 个场景状态字节完全一致，门禁/Boss/第一章三个场景画面逐像素一致（差异 0），
        追逐战仅技能球呼吸光效存在 ±8/255 的实时钟差异
      - 菜单接线自检（合成按键驱动真实单帧逻辑）：主菜单→LOAD GAME→AUTO 槽→确认→读档→
        RESUME 倒计时→恢复；ESC 暂停→SAVE GAME→写槽位；文件浏览器进出；跨章节读档；
        设置持久化往返；BACK 返回主菜单
      - 自检把存档目录重定向到 /tmp，不会污染玩家真实存档
      - 同时导出 5 张界面截图（/tmp/sfss_shots/01_start ~ 05_file_browser.bmp），
        便于人工核对排版（已核对：读档列表 9 行、存档列表 8 行、暂停菜单 7 项、
        文件浏览器 4 行，右边距 ≥28px 无越界/重叠）

    - 代码结构调整：Game::run() 的单帧逻辑抽成 stepFrame()（主循环只做事件轮询/限帧/
      present），使菜单与存档流程可以被合成按键自动化验证

    - 测试模式保留但隐藏：./shooter --test 才显示 TEST 入口，代码标注
      [DORMANT — 激活条件：命令行参数 --test]，便于开发时跳关复现

    - BUGFIX: SaveMeta.chapterTitle 字符串尾部未清零导致存档字节不确定（无法做逐字节
      校验），改为写入前整块 memset

    - BUGFIX: 从暂停菜单以外的路径打开存/读档界面时其它界面标志未清空，单帧分派会继续
      停在旧界面（例如主菜单），统一收进 beginSaveLoadScreen()

  Ver 1.2.23 (补充) | 节点存档生成器：用存档文件取代 TEST 模式的跳关功能（版本号不变）
    - 新增开发命令 ./shooter --mknodes：一条命令把游戏摆到 34 个测试节点并生成对应存档文件

    - 节点存档（saves/nXX_*.sav，共 34 个，含 saves/NODES_README.txt 清单说明）：
      - 第一章 13 个：开场 / 分数 30·60·90·120·150·180（冲击波 Lv1~Lv6）/
        Boss 登场 / 一阶段战斗 / 吸收小怪回血阶段 / 二阶段半血持续战斗 /
        Boss 剩 1 血 / 击破演出
      - 第二章 21 个：开场 / 门禁脉冲·扫描·开门 / 廊桥飞行 / 球体入场·战斗·碎裂 /
        追逐战第 1·3 波 / 弹幕敌人 / 技能球护罩·吸收 / 脉冲满能量 / 暗夜精灵三连发 /
        中央研究室 / 研究室升级 / Warden 登场·战斗·狂暴·击破
      - 读取方式：主菜单 LOAD GAME → BROWSE FILE... → 选文件 → 确认 → 定格在存档那一帧 →
        RESUME 倒计时后继续；也可把 nXX_*.sav 改名为 slotN.sav 直接进槽位列表

    - 生成时每个节点都要过三道校验，全部通过才算成功（避免"生成成功但节点内容不对"）：
      状态自校验（流程枚举/阶段/坐标是否真的到位）、文件回读校验（摘要可读）、
      真正读档并渲染一帧的空白校验（墨迹覆盖率必须 > 0.05%）

    - 与旧 TEST 模式的差异：节点存档是完整合法的游戏状态（含飞行中的子弹/敌人/对话进度），
      TEST 模式则改为 --test 隐藏入口保留；节点生成器成为跳关功能的新维护点

    - 维护方式：游戏加了新内容 → nodeTable() 加一行、buildNodeState() 加一个 case、
      nodeVerify() 加一条校验，重跑 ./shooter --mknodes 即可刷新全部节点存档，
      不会再出现测试模式长期失修的情况

    - 顺带重构：旧 TEST 模式中"按分数预填充第一章对话历史"的代码抽成
      prepopulateCh1History()，测试菜单与节点生成器共用，避免两处逻辑各走一边

    - BUGFIX: 节点空跑只调用 updateGameplay()，而侧滚背景的推进在主循环 stepFrame() 里，
      导致背景不滚动、球体 Boss 永远卡在 ENTERING —— 节点推进改为与主循环同序
      （先更新背景再 updateGameplay）

  Ver 1.2.23 (补充) | 开始开发配套工具 Asset Studio 素材工坊（独立目录 tools/asset_studio）
    - 立项：把 TEST 模式退役后失去的"素材草稿箱"补回来 —— 存档系统管"从任意节点测玩法"，
      工坊管"做素材"，两者互补；附属独立开发日志 tools/asset_studio/DEVELOPMENT_LOG.md

    - 工具直接调用游戏真实绘制代码渲染（本作素材是 C++ 绘制代码而非图片文件），
      并自定义 SDL_SetRenderDrawColor 覆盖动态库中的同名符号，实现零侵入换阵营配色

    - 主游戏零改动：素材注册表放在工具侧、靠 include 游戏头文件工作，单向依赖，
      游戏完全不知道工坊存在

    - 入口 tools/asset_studio/studio.sh，画廊 http://127.0.0.1:8791/

  Ver 1.2.24 | 第二章流程与画面 QA 修复（2026-11-02）
    - 新增第二章 18 项回归检查，随 --selftest 执行：门禁暂停/恢复、背景冻结、增援等待、机体重置及门禁到结局七段顺序流程

    - 本轮验证：-Wall -Wextra 编译零警告、--selftest 全部通过、34 个节点生成与回读通过

    - QA 战斗通过注入命中并维持玩家存活验证推进与碰撞，不作为难度平衡或人工通关结论；七阶段截图输出 /tmp/sfss_ch2_qa/

    - 门禁操作改为 ESC 暂停/恢复、TAB 跳过演出，左下角显示提示；跳过演出同步消耗门禁脉冲能量，支持门禁阶段暂停存档

    - Warden 死亡演出保留逐渐衰暗的机体；入场平滑移至战斗位置，Boss 名称移到血条下方，避免与血条重叠

    - 第二章对话移至战斗实体之后绘制，避免被敌机与弹幕遮挡；结局旁白开始时清理战斗对话与语音

    - 编译检查启用 -Wall -Wextra，清理现有 7 条未使用参数/变量与初始化顺序警告；存档字段布局保持不变，格式版本仍为 1

    - 发布封装内置第一/二章 182 个中英语音文件；新增启动器定位资源并将存档写入用户目录；SDL2 从稳定归档提取以保留 macOS 11 兼容目标

    - BUGFIX: 球体随机碎裂顺序与数组遍历顺序不同，游标越过未处理单元后永久卡在 SHATTERING — 每次选取仍在球体上的最小碎裂序号，中心单元光照计算同时避免除零

    - BUGFIX: 门禁 ESC 分支抢走暂停、读档界面的返回键 — 跳过演出限定为实际游玩中的 TAB，菜单与倒计时不再误开门

    - BUGFIX: 暂停/倒计时/存读档界面仍推进廊桥滚动 — 冻结世界背景，旁白背景只由旁白更新一次

    - BUGFIX: 追逐战空场时忽略待刷增援而提前进研究室 — 转场同时检查 autoSpawnQueued，进入研究室清理遗留敌弹

    - BUGFIX: 第二章非战斗段可向右飞出屏幕、重开后 NightElf 沿用旧无敌帧/三炮状态 — 补屏幕边界钳制并完整重置机体

    - BUGFIX: 瞄准辅助在 draw 中推进吸附动画，反复重绘改变暂停/读档画面 — 仅在玩法更新时推进吸附状态

    - BUGFIX: 技能球呼吸颜色超过 255 后转换 Uint8 溢出，局部突然变暗 — 输出前钳制颜色范围
