# HUNTRESS — 128×128 艺术设定海报

两幅 128×128 像素艺术海报，使用 Aseprite 1.3 的原生图层与 Lua 脚本绘制，
不依赖任何外部图片素材。

## 文件

| 文件 | 内容 |
|------|------|
| `trainer_swift_128.png` / `.aseprite` | 训练机「雨燕 / Swift」——单发、V 尾、琥珀训练标记 |
| `nightelf_128.png` / `.aseprite` | 暗夜精灵 / NightElf——翼尖双发 + 中央主发、金色鸭翼、青色座舱 |
| `build_posters.lua` | 两幅海报的 Aseprite Lua 生成脚本 |

两幅画均为 128×128 RGBA PNG；`.aseprite` 保留完整 8 层：

`BG · DEEP SPACE` → `NEBULA` → `STARFIELD` → `WORLD` → `LIGHT` → `SHIP` → `ENGINE GLOW` → `FRAME + TITLE`

## 画面

- **训练机**：深蓝绿星云 + 琥珀色带环气态巨行星；雨燕以斜俯视姿态从画面中下方掠向
  右上，单发尾焰、琥珀训练条纹和外翼 V 尾；底部 `SWIFT / TRAINER / TR-01`。
- **暗夜精灵**：紫罗兰深空 + 带陨坑的银白月面与 Moonwell 光柱；暗夜精灵从右下掠向左上，
  翼尖双发和中央主发共三点尾焰，金色鸭翼/背脊与青色座舱；底部
  `NIGHTELF / TRIMOTOR / NE-02`。

两幅都采用同一套 128×128 画框、HUNTRESS 铭牌和 5×5 像素标题字。

## 重新生成

需要 Aseprite CLI。Steam 安装路径示例：

```bash
ASEPRITE="$HOME/Library/Application Support/Steam/steamapps/common/Aseprite/Aseprite.app/Contents/MacOS/aseprite"
"$ASEPRITE" -b --script art/posters/build_posters.lua --script-param out=art/posters/
```

脚本会覆盖同名的 `.aseprite` 与 `.png`。修改 `build_posters.lua` 里的
`drawSwift()` / `drawNightElf()` 即可调整舰体；`spaceBg()` / `planet()` / `frame()`
分别控制星空、天体与画框。

> 这两张是游戏世界观下的艺术设定海报，不是游戏内实时绘制素材；舰体造型是
> 对训练机 B「雨燕」与 NightElf C 的 3/4 斜俯视再演绎。
