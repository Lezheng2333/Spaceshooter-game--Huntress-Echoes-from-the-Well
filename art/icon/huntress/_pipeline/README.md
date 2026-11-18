# macOS 图标渲染 / 预览 / 导出管线

把 `art/icon/huntress/` 的 **1024×1024 方形概念稿**变成 macOS 展示图：
Big Sur squircle 预览 → 5 图展示板 → 仿 Dock 预览 → 可用的 `.icns`。

- **只读设计稿**：所有脚本只读取输入 PNG/HTML，不会修改原文件（自测里用哈希确认过）。
- **无 npm / 无网络依赖**：渲染用本机 Microsoft Edge 无头模式，图像处理只用 Pillow。
- **会自动找 Python**：当前解释器没有 Pillow 时，脚本会自动切到 DSH 自带 Python
  （`DSH_PY` 环境变量可覆盖）。

## 最短路径（TL;DR）

```bash
cd "art/icon/huntress/_pipeline"
PY=/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3

./render.sh ../draft_1_wedge.html out/d1.png 1024 2                # HTML → 1024² PNG
"$PY" mac_preview.py out/d1.png out/d1_mac.png --shadow --inner     # → macOS squircle
"$PY" contact_sheet.py --inputs out/d1.png ... --labels ... --out out/sheet.png
"$PY" dock_preview.py out/d1.png ... --out out/dock_preview.png
"$PY" mac_preview.py out/d1.png out/d1_icns.png --inner && ./make_icns.sh out/d1_icns.png out/AppIcon.icns
```

- Big Sur 几何：1024² 画布 / 824² 内容区 / 每边 100 留白 / 圆角 185.4（= 824×0.225），4× 超采样蒙版。
- 自测：`./_selftest/run_selftest.sh`（约 7 s，43/43 PASS，日志 `_selftest/last_run.log`）。
- 详细参数 / 规范说明 / 故障排查见下方。

---

## 目录

```
_pipeline/
├── render.sh            HTML/SVG → PNG（Edge 无头 2x 截图 + Pillow LANCZOS 降采样）
├── mac_preview.py       方形图 → macOS Big Sur squircle 预览（1024² RGBA PNG）
├── contact_sheet.py     5 张图 → 三行展示板（宽 2200）
├── dock_preview.py      1–5 张图 → 仿 macOS Dock 展示图（2000×520）
├── make_icns.sh         PNG → .icns（iconset + iconutil）
├── README.md
└── _selftest/
    ├── gen_test_inputs.py   生成 5 张测试概念图 + 错误路径 fixture
    ├── render_test.html     render.sh 测试稿（1024² HTML）
    ├── run_selftest.sh      一键跑通 2–6 号交付
    ├── verify_outputs.py    43 项机器校验
    ├── inputs/              自测输入（Pillow 绘制）
    ├── out/                 自测产物（预览 / 展示板 / Dock / icns）
    └── last_run.log         最近一次自测完整日志
```

---

## 已验证环境（本机 2026-11-18）

```
macOS            27.0.1 (arm64)
Microsoft Edge   157.0.4341.60
                 /Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge
DSH Python       3.12.14 + Pillow 12.3.0
                 /Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3
系统工具         /usr/bin/iconutil、/usr/bin/sips
```

已验证的 Edge 无头渲染命令（脚本内部就是这套参数）：

```bash
"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
  --headless=new --no-sandbox --disable-gpu --disable-software-rasterizer \
  --disable-dev-shm-usage --no-first-run --no-default-browser-check \
  --user-data-dir=/tmp/<临时profile> \
  --force-device-scale-factor=2 --window-size=1024,1024 \
  --screenshot=/tmp/out.png "file:///abs/path/page.html"
# → 2048×2048 PNG（Edge 支持 --force-device-scale-factor）
```

附加事实：

- 加 `--default-background-color=00000000` 可得到 **透明背景 RGBA** 截图
  （`render.sh` 用 `RENDER_TRANSPARENT=1` 开启）。
- HTML 路径里含空格/中文也没问题：`render.sh` 会把绝对路径转成正确的
  `file://` 百分号编码 URL。
- `sips -z N N in.png --out out.png` 会保留 alpha 通道（`make_icns.sh` 依赖这点）。

---

## 推荐工作流（5 张概念稿 → 展示图 + icns）

```bash
cd "art/icon/huntress/_pipeline"
PY=/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3
mkdir -p out

# ① HTML/SVG 概念稿 → 1024² PNG（Edge 2x 渲染后 LANCZOS 降到 1024）
./render.sh ../draft_1_wedge.html      out/draft_1_wedge.png      1024 2
./render.sh ../draft_2_echo.html       out/draft_2_echo.png       1024 2
./render.sh ../draft_3_monogram.html   out/draft_3_monogram.png   1024 2
./render.sh ../draft_4_hunt.html       out/draft_4_hunt.png       1024 2
./render.sh ../draft_5_well.html       out/draft_5_well.png       1024 2

# ② → macOS squircle 预览（展示用：投影 + 内高光；原图不改）
for f in draft_1_wedge draft_2_echo draft_3_monogram draft_4_hunt draft_5_well; do
  "$PY" mac_preview.py "out/$f.png" "out/${f}_mac.png" --shadow --inner
done

# ③ 展示板（第一行原图 / 第二行 macOS 预览 / 第三行 128 + 64 小尺寸）
"$PY" contact_sheet.py \
  --inputs out/draft_1_wedge.png out/draft_2_echo.png out/draft_3_monogram.png \
           out/draft_4_hunt.png out/draft_5_well.png \
  --labels "01 白线红楔" "02 回声阵列" "03 H / 黑玻璃" "04 猎杀" "05 井壳" \
  --out out/contact_sheet.png

# ④ 仿 Dock 预览（让用户直观看 Dock 里的效果）
"$PY" dock_preview.py \
  out/draft_1_wedge.png out/draft_2_echo.png out/draft_3_monogram.png \
  out/draft_4_hunt.png out/draft_5_well.png --out out/dock_preview.png

# ⑤ 导出 macOS 图标（.icns 里不要放投影，`--inner` 即可）
"$PY" mac_preview.py out/draft_1_wedge.png out/draft_1_wedge_icns.png --inner
./make_icns.sh out/draft_1_wedge_icns.png out/AppIcon.icns
```

> 想加投影版 icns 也可以，但 macOS 图标规范本身不带投影，Finder/Dock 会自动加，
> 建议 icns 用 `--inner`（或什么都不加）的版本。

---

## 脚本用法

### 1. `render.sh` — Edge 无头渲染

```bash
./render.sh <input.html|input.svg> <output.png> [size=1024] [scale=2]
```

行为：

1. 输入转绝对路径 → `file://` 百分号编码 URL；
2. 每次用独立的 `mktemp` profile 启动 Edge，退出自动清理（`RENDER_KEEP_TMP=1` 可保留）；
3. 默认 `scale=2`：`--window-size=1024,1024 --force-device-scale-factor=2` 截图得到
   2048×2048，再用 Pillow LANCZOS 缩到 1024×1024（比 1x 直出边缘更干净）；
4. Edge 若不支持该参数（截图尺寸不是 2048）会明确警告并退回 1x，仍输出 1024²；
5. 有前台看门狗（默认 90 s，`RENDER_TIMEOUT` 可改）。

环境变量：`EDGE_BIN`、`DSH_PY`、`RENDER_TIMEOUT`、`RENDER_TRANSPARENT=1`、
`RENDER_KEEP_TMP=1`。示例：

```bash
./render.sh ../draft_1_wedge.html out/d1.png            # 默认 1024 / 2x
./render.sh ../draft_1_wedge.html out/d1_1x.png 1024 1  # 1x 直出
RENDER_TRANSPARENT=1 ./render.sh logo.svg out/logo.png  # 透明背景
```

### 2. `mac_preview.py` — macOS Big Sur 图标预览

```bash
python3 mac_preview.py <input.png> <output.png> [--shadow] [--inner]
                       [--shape rounded|squircle]
```

| 选项 | 默认 | 说明 |
|---|---|---|
| `--shadow` | 关 | 柔和投影（展示图用；画布 1024² 足够容纳） |
| `--inner` | 关 | 顶部内高光 + 底部压暗，模拟图标体积 |
| `--shape` | `rounded` | `rounded`=185.4 圆角矩形规范值；`squircle`=连续圆角近似 Apple 形状 |
| `--radius` | 185.4 | 圆角半径（1024 画布坐标） |
| `--margin` / `--content` | 100 / 824 | 内容区留白 / 边长 |
| `--super-sample` | 4 | 蒙版超采样倍数 |
| `--shadow-blur/opacity/offset` | 14 / 0.30 / 6 | 投影参数 |
| `--quiet` | — | 不打印成功信息 |

- 输出固定 1024×1024 RGBA PNG，透明背景，**不改原图**；
- 非正方形输入会报错退出；其他方形尺寸会自动缩放到 824 内容区；
- 输入输出路径相同会被拒绝（保护概念稿）。

```bash
"$PY" mac_preview.py out/d1.png out/d1_mac.png --shadow --inner
"$PY" mac_preview.py out/d1.png out/d1_squircle.png --shape squircle --inner
```

### 3. `contact_sheet.py` — 三行展示板

```bash
python3 contact_sheet.py --inputs a.png b.png c.png d.png e.png \
    --labels "A 红楔" "B 蓝环" "C 绿棱" "D 紫涡" "E 金冠" \
    --out contact_sheet.png
```

- 宽度默认 **2200**；5 张时高度 **1297**；支持 1–6 张（超出请拆板）；
- 第一行：5 张原图缩略（397 px，约 400）；第二行：macOS 预览（300 px，含 824 内容区）；
  第三行：每张 128 px 与 64 px 并排（检查 Dock/Finder 下的识别度）；
- 深灰背景、浅色标签、细网格；`--shadow` 给第二行预览加投影，
  `--no-inner` 关闭内高光；`--labels` 数量必须与输入一致（否则报错）；
- 不传 `--labels` 时用文件名作为标签。

### 4. `dock_preview.py` — 仿 macOS Dock

```bash
python3 dock_preview.py a.png b.png c.png d.png e.png --out dock_preview.png
```

- 画布 **2000×520**：深色壁纸（渐变 + 柔光 + 星点 + 噪点） +
  圆角毛玻璃 Dock 条（背景模糊 → 半透明渐变 → 顶部内高光 → 亮边 → 投影）；
- 图标槽位 128 px（真实 macOS 图标含 100/1024 边距，所以可见内容约 103 px），
  间距 22；默认追加 **2 个灰色占位图标做比例参照**（`--placeholders 0` 可关）；
- 选项：`--icon-size`、`--gap`、`--placeholders`、`--title`/`--no-title`、
  `--no-inner`、`--width`/`--height`、`--out`。

### 5. `make_icns.sh` — 打包 .icns

```bash
./make_icns.sh <1024.png> <out.icns>
```

- 临时生成 `AppIcon.iconset`（10 档：16/32/64/128/256/512/1024，即 16–512 的 1x+2x），
  再 `iconutil -c icns` 打包；默认保留 alpha、不联网；
- 校验：源图必须正方形且 ≥1024（512@2x 需要 1024 源图），每档 PNG 尺寸逐一核对，
  输出 magic 必须是 `icns`，最后用 `iconutil -c iconset` 回读确认；
- 源图没有 alpha 时会警告（直接打包不透明概念稿会变成白底方块）；
- 任何一步失败都会打印明确原因（`sips`/`iconutil` 哪一步、输出路径等）。

---

## macOS squircle 参数（Big Sur 规范）

| 参数 | 值 | 说明 |
|---|---|---|
| 画布 | 1024 × 1024 | 透明背景 |
| 内容区 | 824 × 824 | 居中，x = y = 100 |
| 四边留白 | 100 | `1024 − 824 = 200`，每边 100 |
| 圆角半径 | **185.4** | `824 × 0.225 = 185.4` |

`mac_preview.py` 已按该规范实现并验证：

- 默认 `--shape rounded` 使用上述精确值：内容区 `824`、`margin=100`、`radius=185.4`；
- 蒙版在 **4096² 超采样**（4×）绘制，再用 BOX 面积平均降采样到 1024² —— 等价 4×4 SSAA，
  直边保持 1 px 内锐利、圆角得到干净的覆盖率抗锯齿（无 LANCZOS 振铃）；
- 自测机器校验：alpha bbox 精确为 `(100, 100, 924, 924)`，四角 alpha=0，
  内容区外 1 px（98 / 925）为 0，中心与边线内部为 255；
- 注意：Apple 实际图标蒙版是**连续圆角（squircle，G2 曲率平滑）**，不是正圆角弧。
  行业内广泛使用的 Big Sur 模板（Sketch/Figma）就是本脚本默认的 185.4 圆角矩形。
  如果希望更接近 Apple 原生形状，用 `--shape squircle`：superellipse 指数 n≈5，
  保持同样的 824 内容区 / 100 边距 / 185.4 圆角包围盒（自测中角部 (140,140) alpha=255，
  而圆角版为 0，两者区别可见）；
- `--inner` / `--shadow` 只是展示效果，不属于 Big Sur 画布规范；`.icns` 建议不带投影。

---

## 自测（43/43 通过，2026-11-18）

一键复跑：

```bash
cd "art/icon/huntress/_pipeline"
./_selftest/run_selftest.sh      # 约 6.5 s，无网络
# 完整日志：_selftest/last_run.log
# 产物：    _selftest/out/
```

结果摘要：

| 步骤 | 产物 | 结果 |
|---|---|---|
| `render.sh` | `render_2x.png` / `render_1x.png` | 均 1024×1024；2x 降采样 vs 1x 直出平均差 **0.51**；空图检查 stddev 66 |
| `mac_preview.py` | `preview_A_plain.png` / `_shadow_inner.png` / `_squircle.png` | 1024² RGBA；bbox **(100,100,924,924)**；投影/内高光/squircle 差异均可见 |
| `contact_sheet.py` | `contact_sheet.png` **2200×1297** | 5 列排版；第一行 10/10 对颜色可区分；第三行 128/64 小图可见 |
| `dock_preview.py` | `dock_preview.png` **2000×520** | Dock 玻璃条亮度 51.1 > 壁纸 39.1；5 个彩色图标饱和度 71–123，2 个灰色占位 12 |
| `make_icns.sh` | `test_A.icns` **509 KB** | magic=`icns`；10 个尺寸 rep；回读含 1024 px（512@2x） |
| 错误路径 | — | 非方形输入 / 512 源图 / 标签数不匹配 / 非 HTML 输入 4 个用例均按预期报错；输入文件哈希前后一致（未被修改） |

另：在当前的 5 张真实设计稿（`../draft_*.html`）上做过**只读冒烟测试**：
`render.sh` → `mac_preview --shadow --inner` → `contact_sheet`（2200×1297）→
`dock_preview`（2000×520）全部正常（测试产物只写到 `/tmp`，未污染仓库）。

---

## 故障排查

| 现象 | 处理 |
|---|---|
| `找不到可执行的 Edge` | `EDGE_BIN="/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" ./render.sh ...` |
| `需要 Pillow` / 自动切换失败 | 用 DSH Python：`PY=.../python3`，或用 `DSH_PY` 指定 |
| 渲染结果空白/尺寸不对 | 确认页面 `html,body{width:1024px;height:1024px}`；页面不要依赖网络资源；`RENDER_KEEP_TMP=1` 保留现场看 Edge 日志 |
| 需要透明背景 | `RENDER_TRANSPARENT=1 ./render.sh ...` |
| `iconutil 转换失败` | 确认源图是正方形 PNG 且 ≥1024；`--super-sample` 等不影响的预览参数 |
| 中文显示成方框 | 脚本按 PingFang SC → Hiragino Sans GB → Arial Unicode → SFNS 自动选字体；如系统都没有，会退回默认字体 |

---

## 约束与边界

- 仅标准库 + **Pillow**（不需要 numpy / npm / 网络）；
- `mac_preview.py` 输出画布固定 1024²；更小尺寸由展示板/Dock 脚本 LANCZOS 缩放得到；
- `contact_sheet.py` 设计目标 5 张（支持 1–6）；`dock_preview.py` 支持 1–5 张；
- Bash 脚本兼容 macOS 自带 **Bash 3.2**（不依赖 bash 4 语法）；
- 脚本不会写入 `art/icon/huntress/` 根目录（设计稿目录），产出请放 `_pipeline/` 或指定路径。
