# Asset Studio（素材工坊）— 规格文档

> 当前版本：**0.2.0**（2026-11-18 素材画廊重制：0.2 视觉系统 + 逐帧 / 真实波形 / 钉住对比 / 命令面板 / 深链 / 缓存管理）
>
> 一个独立的素材工坊：画廊式浏览全部美术素材 → 自动吸收主游戏新素材 → 在隔离草稿区 vibe coding 创作 → 满意后转正进主游戏。
>
> 它替代已休眠的 TEST 模式所承担的"草稿箱"职责；与存档系统互补（存档管"从任意节点测玩法"，工坊管"做素材"）。

---

## 0. 核心前提：你的素材不是图片，是 C++ 绘制代码

| 事实 | 数据 |
|------|------|
| 仓库内图片文件 | **0 张** |
| `SDL_RenderDrawLine` | 173 处 |
| `SDL_SetRenderDrawColor` | 187 处 |
| 加载图片的地方 | **0 处**（唯二 texture 是屏幕震动离屏缓冲） |
| 绘制入口总数 | **80 个**（`registry/scanner.py` 实测），横跨 7 大类 |
| 画布分辨率 | 固定 800×600 |

**推论**：不存在"素材文件"可以扫描。想要"自动读取新素材"，必须有一个**登记机制** + **源码扫描交叉检查**。

**好处**：素材是矢量代码，可任意缩放、可参数化、可做调色板实验，且渲染真值永远只有一个——游戏自己的代码。

---

## 1. 已验证的技术底座（PoC 已跑通）

| 能力 | 状态 | 验证方式 |
|------|------|----------|
| 无头渲染（无窗口/无显示器） | ✅ | `SDL_CreateSoftwareRenderer` + `SDL_CreateRGBSurfaceWithFormat` |
| 直接 include 游戏头文件 | ✅ | 需与 `main.cpp` 相同的前导 include（`<cmath>` 等必须先引入） |
| 调用游戏真实绘制代码 | ✅ | `NightElf elf; elf.setX(400); elf.draw(r);` 渲染出正确形状 |
| **颜色劫持（零侵入改色）** | ✅ | 工具自定义 `SDL_SetRenderDrawColor`，白→红映射成功，游戏代码零改动 |
| 生成图片 | ✅ | `SDL_SaveBMP` → `sips`/PIL 转 PNG（SDL2_image 未安装） |
| 动画导出 | ✅ | ffmpeg + PIL 12.2.0 已就位 |

### ⚠️ 两条重要约束

1. **素材在 800×600 画布里非常小**：NightElf 实测仅 **31×17 像素**。
   → 画廊缩略图必须**自动裁剪到内容包围盒 + 整数倍最近邻放大**，否则完全看不清。
2. **AI 侧读不了图片**（当前模型不支持图像输入）。
   → 工具必须内置**文本化预览通道**：字符画（ASCII）+ 像素级分析（包围盒/对称性/线宽/非空校验），
   保证 vibe coding 闭环可验证；需要审美判断时另派支持图像的模型审图。

---

## 2. 已确认的设计决策

| # | 决策点 | 结论 |
|---|--------|------|
| 1 | 技术形态 | **C++ 渲染核心 + Python 本地服务 + 网页画廊** |
| 2 | 素材发现 | **中央注册表 + 源码扫描交叉检查**（防漏网） |
| 3 | 编辑能力 | **预览 + 参数滑块 + 导出代码**（vibe coding 为主，滑块为辅） |
| 4 | 覆盖范围 | **全部 7 大类**（含背景与场景） |
| 5 | 转正流程 | **AI 把草稿改写成游戏内代码，你 review 后合并** |
| 6 | 预览内容 | 动画 GIF + 进度拖动；创作辅助叠加层；**真实场景做成可开关按钮** |
| 7 | 美术方向 | **保持线框风格，不同阵营不同颜色**（调色板系统） |
| 8 | 运行依赖 | C++ CLI 渲染器 + Python 服务（复用 `tools/` 既有惯例） |
| 9 | 草稿写法 | **绘制函数 + 参数结构体**（滑块直接绑定字段） |
| 10 | 交付策略 | **先跑通完整链路（试点 NightElf），再批量接入其余素材** |
| 11 | 草稿生命周期 | **进 git；转正后冻结归档**（保留调参历史） |
| 12 | 画廊组织 | **双轴分类（章节 × 类型）+ 搜索 + 标签收藏** |
| 13 | 范围边界 | 美术 + 音频；音频在 v0.1.4 接入（BGM / SFX 均可在画廊试听） |
| 14 | 已有素材接入 | **原样只读接入 + 按需克隆为草稿**（主游戏零改动） |
| 15 | 场景粒度 | **整体场景 + 可展开的零件树** |
| 16 | 派生能力 | **支持一键克隆/派生变体**（红蓝阵营版等） |

---

## 3. 架构

```
┌─ 主游戏 (v2.0.0/multiple code files/) ──────────────┐
│  零改动 · 零新增文件                                  │
│  （注册表放在工具侧，靠 include 游戏头文件工作）        │
└────────────────┬────────────────────────────────────┘
                 │ #include（单向依赖：工具知道游戏，游戏不知道工具）
┌────────────────▼────────────────────────────────────┐
│  assetstudio  (C++ 无头图片渲染器)                    │
│  · SDL 软件渲染器，无窗口                             │
│  · 颜色拦截层（调色板 / 阵营配色）                     │
│  · 渲染任意素材到任意状态、任意参数                     │
│  · 输出 PNG / 帧序列 / 字符画 / 像素分析报告           │
├─────────────────────────────────────────────────────┤
│  audiopreview (C++ 音频试听导出器)                    │
│  · 直接调用游戏 audio.h 的真实合成函数                 │
│  · 离线导出 BGM / SFX 为 44.1kHz 单声道 WAV           │
│  · 主游戏零改动，试听与游戏内声音始终同源               │
└────────────────┬────────────────────────────────────┘
                 │ 子进程调用 + 磁盘缓存
┌────────────────▼────────────────────────────────────┐
│  server.py  (本地服务)                                │
│  · 素材索引 API · 按需渲染 + 缓存 · GIF 合成 · 搜索    │
└────────────────┬────────────────────────────────────┘
                 │ http://127.0.0.1:PORT
┌────────────────▼────────────────────────────────────┐
│  网页画廊 (浏览器)                                    │
│  缩略图墙 · 双轴筛选 · 搜索 · 标签收藏                 │
│  · GIF 动画 + 进度拖动 · 参数滑块 · 叠加层             │
│  · 场景开关（有背景/无背景一键切换）                   │
└─────────────────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────┐
│  drafts/  草稿区  ← vibe coding 在这里创作            │
│  和主游戏完全隔离，改坏也影响不到游戏                  │
└─────────────────────────────────────────────────────┘
```

**单向依赖是关键**：主游戏完全不知道工坊存在，所以工坊永远不可能破坏游戏。

---

## 4. 目录结构（全部新增，主游戏目录不动）

```
tools/asset_studio/
├── SPEC.md                  ← 本文档
├── studio.sh                ← 一键启动（编译 + 起服务 + 开浏览器）
├── renderer/
│   ├── assetstudio.cpp      ← CLI 图片渲染器主程序
│   ├── audiopreview.cpp     ← BGM / SFX 离线试听导出器（调用游戏真实 audio.h）
│   ├── color_intercept.h    ← SDL_SetRenderDrawColor 拦截层（调色板）
│   ├── palette.h            ← 阵营配色预设
│   ├── preview_harness.h    ← 素材预览宿主（构造实例 + 摆状态 + 调用 draw）
│   └── inspect.h            ← 字符画 / 像素分析（供 AI 闭环验证）
├── registry/
│   ├── asset_registry.h     ← 中央素材注册表聚合入口
│   ├── audio_manifest.h     ← BGM / SFX 清单（网页登记 + 音频导出的单一真相源）
│   ├── reg_audio.h          ← 音频条目的画廊登记适配
│   └── scanner.py           ← 源码扫描，交叉检查 draw / snd* 是否漏登记
├── drafts/                  ← 草稿区（进 git）
│   ├── <draft_id>/
│   │   ├── draw.h           ← 绘制函数 + 参数结构体
│   │   └── meta.json        ← 名称/分类/参数范围/转正状态
│   └── ARCHIVE.md           ← 已转正冻结的草稿归档
├── server.py                ← 本地服务 + 渲染调度 + 缓存 + GIF 合成
├── web/                     ← 网页画廊（HTML/CSS/JS，无构建步骤）
├── cache/                   ← 渲染缓存（.gitignore）
└── Makefile
```

---

## 5. 素材注册表设计

每个素材登记一行，工具据此自动出现在画廊里：

```cpp
// 分类：双轴（章节 × 类型）
REGISTER_ASSET(
    .id       = "ch2.player.nightelf",
    .name     = "暗夜精灵号",
    .chapter  = CH2,
    .category = PLAYER,
    .tags     = {"战机", "三炮模式"},
    .params   = NightElfParams{},          // 参数结构体 → 网页自动生成滑块
    .states   = { {"单炮", &nightElfSingle}, {"三炮", &nightElfTriple} },
    .draw     = [](SDL_Renderer* r, const Params& p) {
        NightElf elf; elf.setX(400); elf.setY(300); elf.draw(r);   // 直接调游戏真代码
    }
);
```

**关键点**
- `draw` 是一个适配 lambda：负责构造实例、摆好状态、调用游戏真实 `draw()`。
  这样即使某些类的成员是 private（如 `Ch2SphereBoss`、`Ch2SkillOrb`、`Ch1Boss`）也能接入。
- `states` 声明状态切换器（如 NightElf 的单炮/三炮、敌人的入场进度、Boss 的阶段）。
- `params` 驱动网页滑块自动生成。

### 源码扫描交叉检查

`scanner.py` 扫描游戏源码里的 `void draw*(...)`，与注册表比对：
- 发现**未登记**的绘制入口 → 报警 + 自动生成注册项草稿
- 发现**注册表指向已消失**的入口 → 报警（防止游戏重构后注册表腐烂）

这满足"自动读取主游戏新增素材"，且不依赖脆弱的 C++ 解析。

---

## 6. 颜色与阵营调色板

工具自定义 `SDL_SetRenderDrawColor`，把游戏请求的颜色过一遍调色板：

```cpp
extern "C" int SDL_SetRenderDrawColor(SDL_Renderer* r, Uint8 cr,Uint8 cg,Uint8 cb,Uint8 ca) {
    static auto real = (Fn)dlsym(RTLD_NEXT, "SDL_SetRenderDrawColor");
    if (cr==255 && cg==255 && cb==255) return real(r, pal.r, pal.g, pal.b, ca);  // 白→阵营色
    return real(r, cr, cg, cb, ca);                                              // 其余放行
}
```

**已实测生效**：白 `255,255,255` → 红 `255,60,60`，游戏代码零改动。

配色预设：己方白 / 敌方红 / 技能绿 / 中立青 / 自定义。
额外好处：颜色滑块于是对**已有素材**也直接可用。

---

## 7. 草稿区与参数化

草稿是一对文件，和主游戏完全隔离：

```cpp
// drafts/enemy_spike/draw.h
struct SpikeParams {
    double noseLen  = 14;   // 滑块：0–40
    double wingSpan = 8;    // 滑块：0–24
    double lineWidth= 1;    // 滑块：1–4
    Rgb    color    = {255,255,255};
};
void drawSpike(SDL_Renderer* r, double x, double y, const SpikeParams& p);
```

- 网页滑块 → 改 `Params` 字段 → 重新渲染 → 实时看效果
- **一键克隆**：从任何素材（游戏里的或草稿里的）派生新草稿，自动参数化，改色改比例就能出阵营版本
- **导出代码**：把调好的参数值代回，生成可直接粘贴的 C++ 代码

---

## 8. 网页画廊功能（0.2 重制版）

| 区域 | 功能 |
|------|------|
| 缩略图墙 | 自动裁剪到内容 + 内容自适应最近邻放大（小素材填满卡片）；章节色条 / 类型徽标 / 帧数徽标 / 收藏星标；加载骨架屏；网格与列表双视图、三档密度 |
| 音频卡片 | 真实 WAV 波形（160 bin RMS 采样成条形）+ 中央播放按钮 + 播放进度线 + BGM/音效徽标与时长 |
| 筛选与排序 | 章节 × 类型双轴 + 标签 + 快捷筛选（只看收藏 / 有动画 / 可叠场景）+ 搜索；6 种排序；已选条件 chips 可逐项移除 |
| 详情页 | 双栏舞台：适配放大（小素材最多 10×）+ 额外缩放 / 拖拽平移；概览 / 参数 / 分析 / 源码四页签；状态切换器 + 参数滑块 + 元信息 + 相关素材 |
| 动画预览 | GIF 循环播放 + **逐帧时间轴**（真实逐帧 PNG，多帧并集包围盒保证尺寸不跳动）+ 播放/暂停 + 帧号 |
| 音频详情 | 真实波形画布 + 点击/拖动跳转 + 播放/暂停/循环/时间显示；音频信息页签；下载 WAV |
| 钉住对比 | 先把当前画面钉住，再改状态/参数，拖动分割线 A/B 对比（状态对比、场景开关对比都适用） |
| 场景与叠加层 | 真实场景上下文开关；网格 / 坐标轴 / 对称轴 / 包围盒可独立开关 |
| 收藏与历史 | 收藏、最近浏览（localStorage 持久化）；卡片星标、详情星标、侧栏“只看收藏”与统计 |
| 命令面板 | ⌘K / Ctrl+K 打开：模糊搜索素材 + 执行命令（未登记检查 / 重扫 / 清缓存 / 导出清单 / 视图切换 / 随机素材），↑↓ 选择、↵ 打开 |
| 深链 | `#a=<素材id>&s=<状态>`、`#/scan`、`#/help`；详情右上「分享链接」一键复制当前深链，浏览器前进/后退可用 |
| 快捷键 | `/` 搜索、`G`/`L` 视图、`←`/`→` 切换素材、`F` 收藏、`A` 动画、`S` 场景、`C` 对比、`1…9` 状态、`?` 快捷键表、`Esc` 关闭最上层 |
| 操作 | 下载 PNG / GIF / WAV、导出代码、克隆为草稿、保存草稿参数、查看源码、复制 ID / 源码 / 分析结果、导出当前列表清单 JSON |
| 零件关系 | 按素材 id 前缀推导父场景 / 子零件（门禁 → 真空门 / 扫描光束 / 脉冲光环），详情页直达 |
| 系统面板 | Hero 区渲染引擎状态（在线 / 构建时间 / 缓存大小）；「未登记检查」报告绘制与音频覆盖 + 渲染缓存统计与一键清理 |

---

## 9. 转正流程

```
草稿区（隔离）  →  你确认满意  →  AI 把草稿改写成游戏内对应类的代码
                              →  你 review diff  →  合并
                              →  草稿自动冻结并标记为「已转正」
```

转正时把调好的参数**代回游戏的硬编码常量**，保持主游戏原有代码风格。

---

## 10. 交付状态（已实现）

### 组件清单

| # | 组件 | 文件 | 状态 |
|---|------|------|------|
| 1 | 骨架 + Makefile + 一键启动 | `Makefile` `studio.sh` `.gitignore` | ✅ |
| 2 | 无头渲染器（含 `--verify-all` 批量自检） | `renderer/assetstudio.cpp` | ✅ |
| 3 | 颜色拦截层（零侵入改色） | `renderer/color_intercept.h` `renderer/palette.h` | ✅ |
| 4 | 极简 PNG 写出（zlib，无第三方依赖） | `renderer/png_writer.h` | ✅ |
| 5 | 字符画 + 像素分析通道 | `renderer/inspect.h` | ✅ |
| 6 | 预览宿主数据结构 | `renderer/preview_harness.h` | ✅ |
| 7 | 注册表公共辅助 | `registry/reg_common.h` | ✅ |
| 8 | 六类素材注册（共 45 个素材） | `registry/reg_{players,enemies,bosses,fx,hud,backgrounds}.h` | ✅ |
| 9 | 源码扫描交叉检查 | `registry/scanner.py` | ✅ |
| 10 | 草稿自动发现 | `drafts/gen_drafts.py` → `drafts/_generated.h` | ✅ |
| 11 | 本地服务 + 渲染缓存 + GIF 合成 | `server.py` | ✅ |
| 12 | 网页画廊 0.2（视觉系统 / 命令面板 / 逐帧 / 波形 / 对比 / 深链） | `web/index.html` `web/app.js` `web/style.css` | ✅ |
| 13 | 示范草稿（参数化克隆） | `drafts/nightelf_wide/` | ✅ |
| 14 | BGM / SFX 音频清单与离线导出 | `registry/audio_manifest.h` `reg_audio.h` `renderer/audiopreview.cpp` | ✅ |
| 15 | 逐帧渲染 / 波形 / 缓存 API | `renderer/assetstudio.cpp`（`--frame`）`server.py` | ✅ |
| 16 | 无头浏览器 UI 冒烟（零依赖 CDP 客户端） | `tests/ui_smoke.py` | ✅ |

### 素材覆盖

| 类别 | 数量 | 内容 |
|------|------|------|
| 玩家战机 | 5 | 训练机（透视/侧滚）、暗夜精灵号、德鲁伊号、参数化草稿 |
| 普通敌人 | 3 | 第一章外星飞船、第二章菱形敌、第二章弹幕敌 |
| Boss | 4 | TELAMONDO（+血条零件）、球体 Boss、MOONWELL WARDEN |
| 子弹弹幕 | 3 | 透视子弹（+Boss 光束）、侧滚子弹、敌弹 |
| 特效技能 | 7 | 爆炸、烟花、数字碎裂、冲击波、技能球、脉冲、准星 |
| HUD / UI / 字体 | 14 | 分数、爱心、双能量条、Boss 条、字库、对话行、4 个 UI 元件、浮动文字 |
| 背景 / 场景 | 11 | 第一章 5 个（星空×2/地面/基地/整体）、廊桥、门禁（+3 零件）、研究室 |
| 音频 / BGM | 57 | 5 条 BGM 试听 + 52 个游戏内 snd* 音效，全部由游戏 audio.h 真实合成 |
| **合计** | **104** | 可切换状态 27 个，可调参数（滑块）137 个 |

### 实测数据

| 指标 | 数值 |
|------|------|
| 图片批量自检 | **78 个用例 全部通过，0 空白 0 失败** |
| 音频批量自检 | **57 个 BGM / SFX 全部导出成功，0 失败**（`audiopreview --verify-all`） |
| 后端回归 | **13 项全部通过**（`python3 -m unittest discover -s tests`） |
| 逐帧预览 | 同一素材不同帧输出尺寸一致（多帧并集包围盒）；首次请求约 0.5–1s，之后命中缓存 |
| 波形数据 | 160 bin RMS，0..1 归一化；BGM / SFX 均验证非恒定 |
| UI 冒烟 | `tests/ui_smoke.py` 无头 Edge + 手写 WebSocket/CDP，**129 项全绿**（基线 33 + 0.2 扩展 96），15 张截图，JS 异常 / console.error / 坏图 / 同源 4xx 全 0 |
| 音频试听格式 | 44.1kHz / 单声道 / 16-bit WAV，按需导出并缓存 |
| 单次渲染耗时 | 60–70ms（含进程启动） |
| 24 帧动画渲染 | 170ms |
| 缓存命中 | 0.4ms（约 370 倍加速） |
| GIF 合成（12 帧 800×600） | 340ms，77KB |
| 主游戏改动 | **零**（仅新增 `tools/` 目录） |

### 未覆盖清单（34 个绘制入口）

| 文件 | 数量 | 原因 | 处理方式 |
|------|------|------|----------|
| `game.h` | 18 | 屏幕级绘制（开始画面/暂停菜单/Game Over/倒计时/分数 HUD…），需要完整 Game 实例 | 待你确认是否值得接入 |
| `save_menu.h` | 6 | 存读档界面 | 同上 |
| `renderer.h` | 4 | SDL 绘制的内部包装（drawPoint/drawLine/drawRect） | 不是素材，无需登记 |
| `ch2_background.h` | 4 | `drawSky/drawFloor/drawGlassPanels/drawPillars` **全是 private** | **需要改主游戏**（改成 public），待你确认 |
| `player.h` | 1 | `Player::draw` 是抽象基类的空实现 | 不是素材 |
| `dialogue.h` / `narration.h` | 2 | 对话框 / 旁白框，需要装载文本内容 | 可接入，工作量中等 |

### 接入一个素材的操作成本

1. 在对应 `reg_*.h` 里加一条 `reg(...)` 链式配置
2. 写一个适配函数：构造游戏实例 → 摆状态 → 调 `draw()`
3. `make`（会自动重新生成草稿入口）
4. `./assetstudio --verify-all` 确认它真的渲染出了东西

`scanner.py` 会告诉你还有哪些没登记、以及注册表有没有指向已消失的入口。

---

## 11. 建设过程中发现的问题

### 发现 1：暗夜精灵号的机身填充是上下不对称的

量化结果：

| | 内容像素 | 上下镜像对称 |
|---|---|---|
| 主游戏 `NightElf::draw()` | 180 | **83.7%** |
| 草稿 `half`（复刻现状） | 180 | **83.7%** |
| 草稿 `full`（对称填充） | 221 | **99.2%** |

中心线上方 103 像素、下方 62 像素（1.66 : 1）。

原因在 `player.h` 的填充循环——注释写着 `// Fill upper half with correct lower boundary`，
填充只覆盖中心线以上的半个三角，下半机身只有外轮廓。

作为侧视战机的"上方受光"明暗处理这是合理的；但这是工具第一次把它量化出来。
在画廊里打开 `draft.nightelf_wide`，切换 `上半填充` / `上下对称填充` 两种状态即可直接对比。

### 发现 2：绘制入口比预估多

最初用 `grep "void draw[A-Za-z]*("` 数出 45 个，扫描器实测 **80 个** ——
因为还漏掉了 `static void draw...`、`AimAssist::draw`、`BulletManager::draw`、
`DialogueSystem::draw`、`drawTextLine` 等形态。批量接入的工作量需按 80 计。

### 发现 3：素材在 800×600 画布里极小

NightElf 实测仅 **31×17 像素**。直接当缩略图完全看不清，
所以画廊的缩略图一律**自动裁剪到内容 + 最近邻放大**，
字符画在小素材上也走 **1 字符 = 1 像素**的无损模式（而不是降采样）。

### 发现 4：对称度必须分左右和上下两个方向

第一版只算左右镜像，对侧视战机（机头朝右）毫无意义。
现在同时报 `左右` 和 `上下`，俯视素材看前者、侧视素材看后者。

### 发现 5：`drawTextLine` 不自己设置绘制颜色

`Font::drawString()` 会自己 `SDL_SetRenderDrawColor(255,255,255,255)`，
但 `Font::drawChar()` **不会** —— 它只调 `SDL_RenderFillRect`，颜色完全靠调用方先设好。
`drawTextLine()`（对话文本）直接调 `drawChar`，所以继承调用方的颜色。

后果：工具里清屏用的背景色残留成了当前绘制色，文字被画成背景色，**完全看不见**
（诊断特征是"调色板拦截调用 0 次"）。游戏里之所以正常，是因为调用方碰巧先设了色。

**任何一处调用方忘了设色，那段文字就会隐身。** 建议在 `drawChar` 或 `drawTextLine`
开头补一句设色，消除这个隐患。

### 发现 6：`FloatingTextManager::draw()` 是空实现

```cpp
void draw(SDL_Renderer* renderer) const {
    for (const auto& ft : texts) {
        ...
        SDL_SetRenderDrawColor(renderer, r, g, b_, 255);
        // Use external font reference — handled in Game::draw
    }
}
```

它只设置颜色，**一个像素都不画**。真正画字的是 `Game::drawFloatingTexts()`。
这个 API 名字有误导性——看起来是渲染入口，实际是空转。
（工坊里已经改成忠实复刻 `Game::drawFloatingTexts` 的画法。）

### 发现 7：Warden 死亡瞬间机体立即消失

```cpp
void draw(SDL_Renderer* r) const {
    if (state == INACTIVE || state == DYING) return;   // ← 进入 DYING 就完全不画
```

`takeDamage()` 里血量归零会立刻 `state = DYING`，于是**Boss 机体在第一帧就消失**，
屏幕上只剩每 8 帧一次的爆炸粒子。如果本意是"机体一边爆炸一边消失"，
这里需要把 `DYING` 从提前返回里排除掉。

### 发现 8：第一章敌人有 60% 的存活时间只有 4~5 像素

透视缩放在地平线以上被钳死在 `0.17`：`scale = (depthBelow < 0) ? 0.17 : ...`，
而地平线以上的下降速度只有 `0.08` 倍（1.75 × 0.08 = 0.14 px/帧）。
实测尺寸时间线：

| 推进帧数 | 位置 y | 尺寸 |
|---|---|---|
| 55 | 60 | 5×9 |
| 1000 | 192 | 5×9 |
| 1300 | 262 | 11×17 |
| 1600 | 494 | 31×47 |
| ~1850 | — | 越过屏幕底部销毁 |

从入场到越过地平线要约 1000 帧（约 16 秒）。如果敌人在屏幕上看起来太小，
问题出在这里，而不是素材本身。

### 发现 9：两个类的绘制方法被 private 挡住

- `Ch2SphereBoss` 的 private 段从第 357 行开始，`bg / state / cells` 全在里面，
  只能通过 `init()` / `startEntering()` / `startAtCenter()` / `getState()` 这套公有 API 操作
  （工坊已按这套 API 实现，能正常预览 9 个状态）。
- `Ch2Background` 的 `drawSky / drawFloor / drawGlassPanels / drawPillars` 是 private，
  所以**廊桥目前无法拆零件预览**。要支持的话需要把这四个方法改成 public —— 属于主游戏改动。

### 发现 10（工坊自身 BUG）：`hidden` 属性被作者样式覆盖，弹窗永远可见

**症状**：打开画廊，整个页面被"未登记检查"弹窗盖住，正文空白，看不到缩略图墙。

**根因**：`hidden` 属性靠浏览器默认样式表的 `[hidden] { display: none }` 生效，
而**作者样式表里任何 `display` 声明都会无条件覆盖它**（优先于 UA 样式表）。
工坊的 `.detail` 类写了 `display: flex`，于是 `#detail` 和 `#scan-modal`
从页面加载起就永远可见；`#scan-modal` 在 DOM 里靠后，画在最上层。

**为什么正文是空的**：扫描按钮在弹窗下面，根本点不到，`showScan()` 从未执行。

**修复**：在 style.css 顶部加 `[hidden] { display: none !important; }`。

**教训**：任何依赖 `hidden` 属性显隐的元素，只要它自己的类里有 `display` 声明就会失效。
工坊的其他 `hidden` 元素（`#empty` / `#drafts-section` / 各 `*-wrap`）都没设 display，
所以当时只有这两个弹窗中招。

**顺带修的两个隐患**
- 网页资源（html/js/css）原本 `Cache-Control: public, max-age=86400`，
  导致改完前端后浏览器最多 24 小时看不到新版，**普通刷新也无效**。已改为 `no-store`。
- `showScan()` 没有 try/catch，请求失败会永远停在"正在扫描…"。已加错误提示。

**新增**：`#scan` 深链接（`http://127.0.0.1:8791/#scan` 直接打开未登记检查），
既方便收藏，也让无头浏览器能端到端验证弹窗。

### 架构修正：主游戏连一个文件都不用加

原计划在主游戏目录新增 `assets/asset_registry.h`。实际做法是**注册表放在工具侧**
（`registry/asset_registry.h`），靠 `-I` 指向游戏目录来 `#include` 游戏头文件。
结果：**主游戏零新增文件、零改动**，单向依赖从物理上杜绝了工坊弄坏游戏的可能。

---

## 12. 明确不做（第一期）

- 音效 / BGM / 语音画廊（已有 182 个 WAV，框架可复用，但不在第一期）
- 完整矢量几何编辑器（拖点/加点删点）—— 由"参数滑块 + 对话改代码"覆盖
- 把测试/调试能力（跳关、预设状态）搬进工坊 —— 这是存档系统 + `--mknodes` 的职责
- 改动主游戏任何现有文件

---

## 13. 默认决定（未专门讨论，按此执行，可随时推翻）

| 项 | 默认 |
|----|------|
| 工具名 | Asset Studio（素材工坊） |
| 安装位置 | `tools/asset_studio/` |
| 启动方式 | `tools/asset_studio/studio.sh`（编译 + 起服务 + 开浏览器） |
| 网页技术 | 纯 HTML/CSS/JS，无构建步骤（便于我直接改） |
| 缩略图策略 | 按需渲染 + 磁盘缓存 + 一键全量刷新 |
| 参数持久化 | 每个草稿一个 `meta.json` |
| 主游戏改动 | **零** |

---

## 14. 0.2 素材画廊重制（2026-11-18）

> 0.1.4 把音频塞进了旧界面，但信息密度、操作路径与视觉表达都不够。
> 0.2 的目标：把画廊从"能看的素材清单"升级成"能干活的美术工作台"。

### 视觉与信息架构

- 深空玻璃设计系统：星云 / 星点 / 网格背景、玻璃顶栏、章节配色、统一圆角与阴影层级
- 首屏改为「Hero 标题 + 四个统计卡 + 渲染引擎实时状态 + 侧栏分面 + 卡片墙」结构
- 卡片按内容自适应放大（31×17 的 NightElf 也能填满缩略图），并加章节色条、类型徽标、骨架屏
- 详情面板改为双栏舞台 + 四页签，操作固定在右下角；音频详情改为波形舞台
- 响应式：≤1024px 侧栏抽屉，≤860px 单列详情，`prefers-reduced-motion` 降级

### 新增后端接口

| 接口 | 作用 | 说明 |
|------|------|------|
| `GET /api/frame?id=&frame=N&frames=F` | 逐帧静态 PNG | 新增渲染器 `--frame N`；仍渲染 F 帧并沿用全帧并集包围盒，保证尺寸一致 |
| `GET /api/waveform?id=` | 真实波形 | 复用 `/api/audio` 的 WAV，标准库 `wave` + `array` 算 160 bin RMS |
| `GET /api/cache` | 缓存统计 | 文件数 / 字节数 / MB |
| `POST /api/cache/clear` | 清理缓存 | 只清 `CACHE_DIR` 直接子项并重建 |
| `GET /api/health` | 健康与版本 | 增加构建时间、缓存、Python / Pillow 版本 |

### 工程与验证

- `tests/test_server.py` 从 6 项扩到 13 项（frame / waveform / cache / health 契约）
- 新增 `tests/ui_smoke.py`：纯标准库实现最小 WebSocket + CDP 客户端，驱动无头 Edge 做 DOM 断言、可见性检查、截图与 JS 异常收集（129 项：基线 33 + 0.2 扩展 96）
- `make ui` 直接跑 UI 冒烟；`make check` / `--verify-all` / `audiopreview --verify-all` 保持全绿
- 主游戏仍 **零改动**：所有预览、逐帧、波形都来自游戏自己的绘制与合成代码

### 验证命令

```bash
cd tools/asset_studio
make                                   # 重建渲染器 + 音频试听器
./assetstudio --verify-all             # 78 项图片自检
./audiopreview --verify-all            # 57 项音频自检
python3 -m unittest discover -s tests -v   # 13 项后端回归
make ui                                # 无头浏览器 UI 冒烟（需服务在运行）
```

### 界面截图（`docs/screenshots/`）

| 文件 | 内容 |
|------|------|
| `gallery-main.png` | 首屏：Hero + 统计卡 + 渲染引擎状态 + 分面侧栏 + 卡片墙 |
| `gallery-detail.png` | 图片详情：舞台 + 真实场景叠加 + 概览页签 |
| `gallery-audio.png` | 音频详情：真实 WAV 波形 + 播放控制 |
| `gallery-command.png` | ⌘K 命令面板（模糊搜索 + 命令） |
| `gallery-scan.png` | 未登记检查（绘制 / 音频 / 缓存摘要 + 分类分组） |
| `gallery-mobile.png` | 430px 移动端布局（双列卡片 + 抽屉侧栏） |

## 15. NightElf 定稿与休眠德鲁伊家族（0.2.2，2026-11-18）

### 评审结果

- **C 案采用**为现役 NightElf：大后掠翼 + 翼尖双发 + 中央主发；名称仍为
  「暗夜精灵 / NightElf」，不使用「三叉戟」。
- **B 案**定稿为休眠资产「树妖」（`Dryad`）。
- **E 案**定稿为休眠资产「德鲁伊·幽光」（`DruidWraithlight`），作为原版
  `Druid` 的迭代型号；原版 `Druid` 的绘制、行为与画廊条目均保留不动。
- **A / D 案**清理，五案临时章节与概念稿文件删除。

### 游戏侧实现

- 新增 `v2.0.0/multiple code files/ship_art.h`：暗夜精灵家族共用 `Pen`
  画笔（局部多边形、对称上下双色填充、叶片尾焰、喷口、中轴座舱、引擎柔光）
  + `paintNightElf` / `paintDryad` / `paintDruidWraithlight` 三套绘制。
- `player.h`：`NightElf::draw()` 改为调用 `ShipArt::paintNightElf`；
  机头偏移 32 → 34；三炮炮口改为机鼻 `(34,0)` + 上下前翼 `(28,±6)`。
- 新增 `Dryad` / `DruidWraithlight` 两个 `[DORMANT]` 类，未接入 `Game`，
  不产生存档字段；原版 `Druid` 类保持原样。
- 无存档格式变化：`SAVE_FORMAT_VERSION` 不变。

### 画廊侧实现

- `registry/reg_players.h`：NightElf 条目改用新的 `player.h` 真值绘制；
  新增 `ch3.player.dryad`（树妖）与 `ch3.player.druid.wraithlight`（德鲁伊·幽光），
  均标 `休眠资产`；原 `ch3.player.druid` 保留。
- 删除 `registry/reg_nightelf_concepts.h` 与「暗夜精灵方案」临时章节；
  画廊总数 109 → 106。
- `docs/nightelf_final_sheet.png` 为定稿家族总览图。

### 验证

- 游戏：`make` 零警告；`./shooter --selftest` RESULT PASS（含 NightElf 换机/存档往返）。
- 工坊：`make`、`--verify-all`、无头浏览器 UI 冒烟（106 张素材）全绿。
- 原版 `Druid` 的绘制与画廊条目不参与任何改动。

### 后续

- 第三章若启用 `Dryad` / `DruidWraithlight`：先在 `Game` 中接入机体与切换，
  再按实际玩法决定是否需要新增存档字段（若新增，`SAVE_FORMAT_VERSION` +1）。

## 16. 训练机重设计五案双视角预览（0.2.3，2026-11-18）

### 任务

现役训练机（TrainingPlane / Ch2Trainer）是深色菱形 + 单圈描边；主美要求
重新设计并先看 **5 版方案预览**，每版都要有 **第一章透视** 与 **第二章侧滚**
两套配套样貌，选定后再移植。本轮不改主游戏。

### 双视角做法

- 每套方案只定义 **一张俯视平面图**（局部坐标 +x 右翼、-y 机头）：
  - 第一章：用游戏真实的 `perspLeft/perspWidth/HORIZON_Y` 和 `roll` 参数
    做透视投影；机身不滚、机翼跟随机翼横滚，与现役 `TrainingPlane::draw` 同规则。
  - 第二章：把同一张平面图按 `(x,y) -> (-y,x)` 旋转 90°，变成侧滚视图；
    与现役 `Ch2Trainer` 的"同一架飞机旋转"约定一致。
- 因此两章不是两套各画各的造型，而是同一架飞机的两个投影。

### 画廊呈现

- 新增临时章节「训练机方案」与 5 个资产 `concept.trainer.a..e`；
  每个资产两个状态：「第一章 · 透视」/「第二章 · 侧滚」，参数 `posX/posY/roll`。
- 默认显示第一章；第二章状态在详情「参数」页的「状态」里切换。
- 总览图：`docs/trainer_concepts_sheet.png`（5 列 × 上下两行双视角）。
- 画廊总数临时 106 → 111；选定后会移除 5 个概念稿，把选中方案移植到
  `TrainingPlane` / `Ch2Trainer` 两条现役资产上，总数回到 106。

### 五案

| 方案 | 名称 | 结构关键词 | 识别色 |
|------|------|------------|--------|
| A | 白鸥 GULL | 平直翼 + 翼尖副油箱 + 双发 + 双垂尾 | 琥珀 |
| B | 雨燕 SWIFT | 后掠翼 + 单发 + V 尾 | 冰蓝 |
| C | 蜻蜓 DRAGONFLY | 双尾撑 + 中央短舱 + 两台尾推 | 橙色 |
| D | 狮鹫 GRYPHON | 三角翼 + 前翼 + 单发 | 金色 |
| E | 信天翁 ALBATROSS | 飞翼 + 双子发 + 翼尖小翼 | 薄荷 |

### 验证

- 工坊 `--verify-all` 92 项通过；UI 冒烟 126/126（111 张素材）。
- 游戏 `TrainingPlane` / `Ch2Trainer` 零改动；选定后才进入游戏侧迁移。

## 17. 训练机定稿与碰撞盒优化（0.2.4，2026-11-18）

### 定稿

- **默认训练机 = B 方案「雨燕」**：`TrainingPlane` / `Ch2Trainer` 都改为调用
  `ShipArt::TrainerView + paintSwift`。两章仍共用同一张俯视平面图：
  第一章按透视投影，第二章按 `(x,y)->(-y,x)` 旋转 90°。
- 游戏内名称仍为 Trainer；素材画廊与补充剧情名为「训练机·雨燕」。
- **D 方案 = 休眠资产「训练机升级·狮鹫」**：`TrainerUpgrade`，默认第二章侧滚，
  `drawView(r,false)` 可出第一章透视；未接入 Game、不新增存档字段。
- A/C/E 概念稿、临时「训练机方案」章节与 `reg_trainer_concepts.h` 全部删除；
  画廊总数 111→107（移除 5 个概念稿、新增 1 个升级条目，训练机两条原条目就地更新）。

### 碰撞盒优化

- `Player` 新增水平胶囊碰撞：`getHitBack()/getHitFront()/getHitRadius()` +
  `hitByCircle(cx, cy, r)`；只让机身中段参与判定，翼尖、长机鼻尖端、尾焰不参与。
- 训练机 `Ch2Trainer`：8 / 9 / 4（默认训练机）。
- 暗夜精灵 `NightElf`：12 / 22 / 5。
- `Ch2ShooterBase` 的敌方子弹由 16px 玩家大圆改为胶囊（子弹半径 3）；
  `Ch2Boss` 的 Boss 子弹改为胶囊（半径 4）；Warden 机体碰撞改为
  Boss 圆 vs 玩家胶囊。
- 游戏 `--selftest` 新增 7 项 hitbox 回归：机身中段命中、翼尖/机鼻尖端不命中。

### 画廊

- `ch1.player.trainingplane` → 「训练机·雨燕」，直接调用新 `TrainingPlane::draw`；
  `ch2.player.trainer` → 「训练机·雨燕（侧滚）」，直接调用新 `Ch2Trainer::draw`。
- 新增 `ch2.player.trainerupgrade`「训练机升级·狮鹫」，双状态（第二章侧滚 / 第一章透视）。
- 两个 Ch2 玩家条目增加「显示碰撞盒」参数，绿色胶囊叠加层显示实际判定范围。
- 缩略图由真实 `draw()` 生成，并带构建时间参数，重建后不会显示旧图。

### 验证

- 游戏 `make` 零警告；`./shooter --selftest` RESULT PASS（含 7 项 hitbox 回归）。
- 工坊 `--verify-all` 84 项通过；UI 冒烟 126/126（107 张素材）。
- 无存档格式变化，旧存档兼容。
