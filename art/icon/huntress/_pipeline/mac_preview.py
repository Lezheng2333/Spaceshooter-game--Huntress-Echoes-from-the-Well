#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mac_preview.py — 把一张方形概念图套进 macOS Big Sur 图标几何，输出 RGBA PNG 预览。

几何规范（Big Sur / macOS 11+ 图标画布）:
    画布          1024 × 1024，透明背景
    内容区        824 × 824，居中，四边各留 100 px
    圆角半径      185.4 px（= 824 × 0.225）
    抗锯齿        4× 超采样蒙版（4096 → 1024 BOX 面积平均降采样，等价 4×4 SSAA）

可选效果:
    --shadow      柔和投影（导出展示图/Contact Sheet 用）
    --inner       极淡的顶部内高光 + 底部压暗，模拟 macOS 图标的体积感
    --shape squircle
                  连续圆角（superellipse n≈5）近似 Apple 实际 squircle；
                  默认 rounded 即大厂模板常用的 185.4 圆角矩形规范。

本文件同时作为该管线的共享模块：contact_sheet.py / dock_preview.py 通过
`from mac_preview import build_mask, preview_from_image, load_font` 复用蒙版、
预览与字体工具。

用法:
    python3 mac_preview.py <input.png> <output.png> [--shadow] [--inner]
                           [--shape {rounded,squircle}]

只用标准库 + Pillow，不需要网络 / npm。
"""

from __future__ import annotations

import argparse
import math
import os
import sys
from typing import Callable

# ---------------------------------------------------------------------------
# Pillow 自举：优先当前解释器；若当前解释器没有 Pillow，自动切到 DSH 自带 Python
# ---------------------------------------------------------------------------
_DSH_PY = os.environ.get("DSH_PY") or (
    "/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3"
)


def _bootstrap_pillow() -> None:
    try:
        import PIL  # noqa: F401

        return
    except ImportError:
        pass
    if os.path.isfile(_DSH_PY) and os.path.realpath(sys.executable) != os.path.realpath(_DSH_PY):
        os.execv(_DSH_PY, [_DSH_PY, os.path.abspath(__file__), *sys.argv[1:]])
    sys.exit(
        "error: 需要 Pillow（当前解释器 %s 未安装）。\n"
        "       请改用 DSH 自带 Python 运行：\n"
        "       %s %s ..." % (sys.executable, _DSH_PY, os.path.basename(__file__))
    )


_bootstrap_pillow()

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont  # noqa: E402

# ---------------------------------------------------------------------------
# Big Sur 图标几何常量
# ---------------------------------------------------------------------------
CANVAS = 1024          # 画布边长
MARGIN = 100           # 内容区四周留白
CONTENT = 824          # 内容区边长 = 1024 - 2 × 100
RADIUS = 185.4         # 圆角半径 = 824 × 0.225（Big Sur 规范值）
SUPERSAMPLE = 4        # 蒙版超采样倍数
SQUIRCLE_EXP = 5.0     # --shape squircle 的 superellipse 指数
SHAPES = ("rounded", "squircle")

_MASK_CACHE: "dict[tuple, Image.Image]" = {}


# ---------------------------------------------------------------------------
# 蒙版
# ---------------------------------------------------------------------------
def _squircle_points(x0: float, y0: float, x1: float, y1: float, r: float,
                     exp: float = SQUIRCLE_EXP, steps: int = 96):
    """按 superellipse |u|^n + |v|^n = 1 生成连续圆角矩形的轮廓点（顺时针）。"""
    r = max(1.0, float(r))
    n = max(2.0, float(exp))
    k = 2.0 / n
    pts = [(x0 + r, y0)]

    def arc(cx: float, cy: float, sx: float, sy: float, t0: float, t1: float):
        for i in range(steps + 1):
            t = t0 + (t1 - t0) * i / steps
            u = math.cos(t) ** k
            v = math.sin(t) ** k
            pts.append((cx + sx * r * u, cy + sy * r * v))

    # 右上角: (x1-r, y0) -> (x1, y0+r)
    arc(x1 - r, y0 + r, 1.0, -1.0, math.pi / 2, 0.0)
    # 右下角: (x1, y1-r) -> (x1-r, y1)
    arc(x1 - r, y1 - r, 1.0, 1.0, 0.0, math.pi / 2)
    # 左下角: (x0+r, y1) -> (x0, y1-r)
    arc(x0 + r, y1 - r, -1.0, 1.0, math.pi / 2, 0.0)
    # 左上角: (x0, y0+r) -> (x0+r, y0)
    arc(x0 + r, y0 + r, -1.0, -1.0, 0.0, math.pi / 2)
    return pts


def build_mask(canvas: int = CANVAS, margin: int = MARGIN, content: int = CONTENT,
               radius: float = RADIUS, shape: str = "rounded",
               ss: int = SUPERSAMPLE) -> Image.Image:
    """返回 canvas×canvas 的 L 蒙版（内容区外为 0，内部为 255，边缘 4× 超采样抗锯齿）。"""
    if shape not in SHAPES:
        raise ValueError("未知 shape=%r（可选：%s）" % (shape, ", ".join(SHAPES)))
    key = (canvas, margin, content, round(float(radius), 3), shape, ss)
    cached = _MASK_CACHE.get(key)
    if cached is not None:
        return cached

    big = Image.new("L", (canvas * ss, canvas * ss), 0)
    x0, y0 = margin * ss, margin * ss
    x1, y1 = (margin + content) * ss - 1, (margin + content) * ss - 1
    draw = ImageDraw.Draw(big)
    if shape == "rounded":
        draw.rounded_rectangle((x0, y0, x1, y1), radius=float(radius) * ss, fill=255)
    else:
        pts = _squircle_points(x0, y0, x1, y1, float(radius) * ss, exp=SQUIRCLE_EXP)
        draw.polygon(pts, fill=255)

    # 4× 超采样用 BOX（面积平均）降采样：等价于 4×4 SSAA，
    # 直线边缘保持 1px 内锐利，圆角处得到干净的覆盖率抗锯齿。
    mask = big.resize((canvas, canvas), Image.Resampling.BOX)
    _MASK_CACHE[key] = mask
    return mask


# ---------------------------------------------------------------------------
# 内容合成
# ---------------------------------------------------------------------------
def _fit_content(art: Image.Image, content: int) -> Image.Image:
    if art.width != art.height:
        raise ValueError(
            "输入必须是正方形图片（当前 %d × %d）" % (art.width, art.height)
        )
    art = art.convert("RGBA")
    if art.size != (content, content):
        art = art.resize((content, content), Image.Resampling.LANCZOS)
    return art


def _vertical_layer(content: int, rgb: "tuple[int, int, int]",
                    alpha_at: Callable[[float], float]) -> Image.Image:
    """生成 content×content 的纯色层，alpha 只随行变化（用于内高光/压暗）。"""
    data = bytearray()
    for i in range(content):
        t = i / float(content - 1) if content > 1 else 0.0
        a = max(0, min(255, int(round(alpha_at(t)))))
        data += bytes(rgb + (a,)) * content
    return Image.frombytes("RGBA", (content, content), bytes(data))


def _add_inner_lighting(icon: Image.Image, mask: Image.Image,
                        margin: int, content: int) -> Image.Image:
    """顶部内高光 + 底部压暗（都被 squircle 蒙版裁切，只作用在图标内容上）。"""
    cmask = mask.crop((margin, margin, margin + content, margin + content))

    def top_alpha(t: float) -> float:
        # 顶部最亮，约 50% 高度处淡出
        return 30.0 * max(0.0, 1.0 - t / 0.50) ** 2.4

    def bottom_alpha(t: float) -> float:
        # 底部约 8%，从 42% 高度开始渐入
        return 22.0 * max(0.0, (t - 0.42) / 0.58) ** 2.0

    out = icon
    for rgb, fn in (((255, 255, 255), top_alpha), ((0, 0, 0), bottom_alpha)):
        layer = _vertical_layer(content, rgb, fn)
        layer.putalpha(ImageChops.multiply(layer.getchannel("A"), cmask))
        out = _composite_at(out, layer, margin)
    return out


def _composite_at(base: Image.Image, layer: Image.Image, xy: int) -> Image.Image:
    """把与内容区同尺寸的层 alpha-composite 到画布指定位置。"""
    pad = Image.new("RGBA", base.size, (0, 0, 0, 0))
    pad.paste(layer, (xy, xy))
    return Image.alpha_composite(base, pad)


def _shadow_layer(mask: Image.Image, blur: float, opacity: float,
                  offset: float) -> Image.Image:
    """由 squircle 蒙版生成柔和黑色投影层（画布同尺寸，RGBA）。"""
    a = mask.point(lambda v: int(round(v * opacity)))
    a = a.filter(ImageFilter.GaussianBlur(blur))
    if abs(offset) > 1e-6:
        shifted = Image.new("L", mask.size, 0)
        shifted.paste(a, (0, int(round(offset))))
        a = shifted
    layer = Image.new("RGBA", mask.size, (0, 0, 0, 0))
    layer.putalpha(a)
    return layer


def preview_from_image(art: Image.Image, *, shadow: bool = False, inner: bool = False,
                       shape: str = "rounded", radius: float = RADIUS,
                       margin: int = MARGIN, content: int = CONTENT,
                       ss: int = SUPERSAMPLE,
                       shadow_blur: float = 14.0, shadow_opacity: float = 0.30,
                       shadow_offset: float = 6.0) -> Image.Image:
    """把一张方形 PIL 图转成 1024×1024 的 macOS 图标预览（RGBA，不改原图）。"""
    if margin < 0 or content <= 0 or margin + content > CANVAS:
        raise ValueError(
            "几何参数越界: margin=%s content=%s（需 margin>=0, content>0, margin+content<=%d）"
            % (margin, content, CANVAS)
        )
    if not (0 < radius <= content / 2.0):
        raise ValueError("radius 需在 (0, content/2] 内，当前 %s" % radius)
    if not (0.0 <= shadow_opacity <= 1.0):
        raise ValueError("shadow_opacity 需在 [0,1] 内，当前 %s" % shadow_opacity)

    mask = build_mask(CANVAS, margin, content, radius, shape, ss)
    content_mask = mask.crop((margin, margin, margin + content, margin + content))
    art = _fit_content(art, content)

    icon = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    icon.paste(art, (margin, margin), content_mask)

    if inner:
        icon = _add_inner_lighting(icon, mask, margin, content)
    if shadow:
        icon = Image.alpha_composite(
            _shadow_layer(mask, shadow_blur, shadow_opacity, shadow_offset), icon
        )
    return icon


def preview_from_path(path: str, **kwargs) -> Image.Image:
    """读取图片文件并生成预览（原文件只读，不会被修改）。"""
    with Image.open(path) as im:
        im.load()
        return preview_from_image(im, **kwargs)


# ---------------------------------------------------------------------------
# 字体工具（contact_sheet.py / dock_preview.py 复用）
#
# macOS 上没有单一的 "SF Pro + 中文" 字体文件：SFNS.ttf 能渲染拉丁但不含中文字形
# （Pillow 会画出豆腐块）。所以这里优先用 PingFang SC / Hiragino Sans GB 这类
# 同时覆盖拉丁与简体中文的系统字体，按 weight 选 TTC 内的对应字面。
# ---------------------------------------------------------------------------
import glob  # noqa: E402  (放在此处以便文件顶部依赖最小)

_WEIGHT_ALIASES = {
    "Regular": ("Regular", "W3", "Book", "Normal"),
    "Medium": ("Medium", "W3", "Regular", "Book"),
    "Semibold": ("Semibold", "W6", "Bold", "Medium", "Regular"),
    "Bold": ("Bold", "W6", "Semibold", "Heavy", "Medium"),
    "Heavy": ("Heavy", "Bold", "W6", "Semibold"),
}

_TTC_FACE_CACHE = {}
_FONT_CACHE = {}


def _font_paths():
    """按优先级返回 (path, 允许的 family 列表)。PingFang 在 macOS 上可能是
    AssetsV2 里的按需字体资产，路径带 hash，所以用 glob 找。"""
    paths = []
    for pattern in (
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/PingFang.ttc",
        "/System/Library/AssetsV2/com_apple_MobileAsset_Font*/*/AssetData/PingFang.ttc",
    ):
        paths.extend(sorted(glob.glob(pattern)))
    paths = [p for p in paths if os.path.isfile(p)]
    entries = [(p, ("PingFang SC", "PingFang TC", "PingFang HK")) for p in paths]
    entries += [
        ("/System/Library/Fonts/Hiragino Sans GB.ttc", ("Hiragino Sans GB",)),
        ("/System/Library/Fonts/Supplemental/Arial Unicode.ttf", ("Arial Unicode MS",)),
        ("/System/Library/Fonts/SFNS.ttf", ("System Font", "SF Pro")),
    ]
    return [(p, fam) for p, fam in entries if os.path.isfile(p)]


def _ttc_faces(path):
    faces = _TTC_FACE_CACHE.get(path)
    if faces is None:
        faces = []
        for idx in range(32):
            try:
                face = ImageFont.truetype(path, 12, index=idx)
            except OSError:
                break
            family, style = face.getname()
            faces.append((idx, family or "", style or ""))
        _TTC_FACE_CACHE[path] = faces
    return faces


def load_font(size: int, weight: str = "Regular") -> ImageFont.FreeTypeFont:
    """加载同时覆盖拉丁与中文的系统字体，并尽量匹配 weight。

    weight 可选 Regular / Medium / Semibold / Bold / Heavy。
    """
    size = max(1, int(size))
    aliases = _WEIGHT_ALIASES.get(weight, _WEIGHT_ALIASES["Regular"])
    key = (size, aliases)
    cached = _FONT_CACHE.get(key)
    if cached is not None:
        return cached

    for path, families in _font_paths():
        faces = _ttc_faces(path)
        if faces:
            for want in aliases:
                for idx, family, style in faces:
                    if family in families and style == want:
                        font = ImageFont.truetype(path, size, index=idx)
                        _FONT_CACHE[key] = font
                        return font
        # 非 TTC / 单字面文件（Arial Unicode、SFNS）兜底
        try:
            font = ImageFont.truetype(path, size)
        except OSError:
            continue
        if weight != "Regular":
            for name in (weight, weight.capitalize()):
                try:
                    font.set_variation_by_name(name)
                    break
                except (OSError, ValueError, AttributeError):
                    pass
        _FONT_CACHE[key] = font
        return font

    try:
        font = ImageFont.load_default(size)
    except TypeError:  # Pillow < 10.1
        font = ImageFont.load_default()
    _FONT_CACHE[key] = font
    return font


def text_size(draw: ImageDraw.ImageDraw, text: str,
              font: ImageFont.FreeTypeFont) -> "tuple[int, int]":
    """返回文本 (宽, 高)，兼容 Pillow 新旧 API。"""
    box = draw.textbbox((0, 0), text, font=font)
    return box[2] - box[0], box[3] - box[1]


# ---------------------------------------------------------------------------
# 通用渐变（contact_sheet.py / dock_preview.py 复用）
# ---------------------------------------------------------------------------
def linear_gradient(w: int, h: int, stops) -> Image.Image:
    """竖向线性渐变 RGBA 图。stops = [(位置0..1, (r,g,b[,a])), ...]，位置需递增。"""
    if not stops:
        raise ValueError("stops 不能为空")
    data = bytearray()
    rgba_stops = []
    for pos, color in stops:
        if len(color) == 3:
            color = tuple(color) + (255,)
        rgba_stops.append((float(pos), tuple(int(c) for c in color)))
    rgba_stops.sort(key=lambda item: item[0])
    for y in range(h):
        t = y / float(h - 1) if h > 1 else 0.0
        c = rgba_stops[-1][1]
        for i in range(len(rgba_stops) - 1):
            p0, c0 = rgba_stops[i]
            p1, c1 = rgba_stops[i + 1]
            if p0 <= t <= p1:
                u = 0.0 if p1 == p0 else (t - p0) / (p1 - p0)
                c = tuple(int(round(c0[k] + (c1[k] - c0[k]) * u)) for k in range(4))
                break
        if t < rgba_stops[0][0]:
            c = rgba_stops[0][1]
        data += bytes(c) * w
    return Image.frombytes("RGBA", (w, h), bytes(data))


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="mac_preview.py",
        description="方形概念图 → macOS Big Sur 图标预览（1024² RGBA PNG）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("input", help="输入方形 PNG（1024×1024 最佳，其他方形会自动缩放）")
    p.add_argument("output", help="输出 PNG 路径（RGBA，不会写回输入文件）")
    p.add_argument("--shadow", action="store_true", help="加柔和投影（展示图用）")
    p.add_argument("--inner", action="store_true", help="加顶部内高光 + 底部压暗")
    p.add_argument("--shape", choices=SHAPES, default="rounded",
                   help="rounded=185.4 圆角矩形（规范值）; squircle=连续圆角近似 Apple 形状")
    p.add_argument("--radius", type=float, default=RADIUS, help="圆角半径（px，1024 画布坐标）")
    p.add_argument("--margin", type=int, default=MARGIN, help="内容区留白（px）")
    p.add_argument("--content", type=int, default=CONTENT, help="内容区边长（px）")
    p.add_argument("--super-sample", type=int, default=SUPERSAMPLE, dest="ss",
                   help="蒙版超采样倍数（>=1）")
    p.add_argument("--shadow-blur", type=float, default=14.0, help="投影模糊半径（px）")
    p.add_argument("--shadow-opacity", type=float, default=0.30, help="投影不透明度 0..1")
    p.add_argument("--shadow-offset", type=float, default=6.0, help="投影向下偏移（px）")
    p.add_argument("--quiet", action="store_true", help="不打印成功信息")
    return p


def main(argv: "list[str] | None" = None) -> int:
    args = _parser().parse_args(argv)

    src = os.path.abspath(os.path.expanduser(args.input))
    dst = os.path.abspath(os.path.expanduser(args.output))
    if not os.path.isfile(src):
        sys.stderr.write("error: 输入文件不存在: %s\n" % src)
        return 2
    if os.path.realpath(src) == os.path.realpath(dst):
        sys.stderr.write("error: 输出路径与输入相同，拒绝覆盖原图: %s\n" % dst)
        return 2
    if args.ss < 1:
        sys.stderr.write("error: --super-sample 必须 >= 1\n")
        return 2

    try:
        art = preview_from_path(
            src,
            shadow=args.shadow,
            inner=args.inner,
            shape=args.shape,
            radius=args.radius,
            margin=args.margin,
            content=args.content,
            ss=args.ss,
            shadow_blur=args.shadow_blur,
            shadow_opacity=args.shadow_opacity,
            shadow_offset=args.shadow_offset,
        )
    except (ValueError, OSError) as exc:
        sys.stderr.write("error: 生成预览失败: %s\n" % exc)
        return 2

    out_dir = os.path.dirname(dst)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    if not dst.lower().endswith(".png"):
        sys.stderr.write("warning: 输出扩展名不是 .png，仍按 PNG(RGBA) 写出\n")
    art.save(dst, format="PNG")

    if not args.quiet:
        w, h = art.size
        print(
            "✅ mac_preview: %s\n"
            "   %d×%d RGBA · 内容 %d×%d @ (%d, %d) · 半径 %.1f · shape=%s · 蒙版 %d× 超采样"
            "\n   options: shadow=%s inner=%s"
            % (dst, w, h, args.content, args.content, args.margin, args.margin,
               args.radius, args.shape, args.ss,
               "on" if args.shadow else "off", "on" if args.inner else "off")
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        raise SystemExit(130)
