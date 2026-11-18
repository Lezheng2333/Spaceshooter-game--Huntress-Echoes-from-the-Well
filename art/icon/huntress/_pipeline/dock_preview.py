#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dock_preview.py — 用 1–5 张概念图生成「仿 macOS Dock」展示图（约 2000×520）。

用途：让主设计直观看到图标缩到 Dock 尺寸（128 px 槽位）后的效果，
以及与灰色占位图标相邻时的比例关系。

实现要点（无外部依赖，只用 Pillow）：
  * 深色壁纸 = 竖向渐变 + 两团柔光 + 细噪点（模拟桌面背景）
  * Dock 条 = 圆角深色玻璃：背景模糊 → 半透明竖向渐变 → 顶部内高光 /
    底部压暗 → 1px 亮边 + 投影
  * 图标 = mac_preview.py 的 Big Sur squircle 预览（含 100 边距），缩到 128 px，
    带轻微投影；占位图标为灰色 squircle + "?" 字形

用法:
    python3 dock_preview.py a.png b.png c.png d.png e.png --out dock_preview.png
    python3 dock_preview.py a.png --placeholders 2
"""

from __future__ import annotations

import argparse
import os
import random
import sys

# Pillow 自举（DSH 自带 Python）
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
    sys.exit("error: 需要 Pillow，请用 DSH 自带 Python 运行：%s %s ..."
             % (_DSH_PY, os.path.basename(__file__)))


_bootstrap_pillow()

from PIL import Image, ImageChops, ImageDraw, ImageFilter  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mac_preview import linear_gradient, load_font, preview_from_image, preview_from_path  # noqa: E402

DOCK_RADIUS = 34
DOCK_BOTTOM_MARGIN = 54
DOCK_MIN_SIDE_MARGIN = 90


def _radial_glow(size, center, radii, color) -> Image.Image:
    """椭圆径向光斑（高斯模糊近似），返回整幅 RGBA 层。"""
    width, height = size
    cx, cy = center
    rx, ry = radii
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(max(rx, ry) * 0.55))
    layer = Image.new("RGBA", size, tuple(color[:3]) + (0,))
    layer.putalpha(mask.point(lambda v: int(v * color[3] / 255)))
    return layer


def _place_icon(base: Image.Image, icon: Image.Image, xy) -> None:
    """把 128 px 图标带一层很小的柔和投影贴到画布上。"""
    x, y = xy
    alpha = icon.getchannel("A").point(lambda v: int(v * 0.34))
    shadow = Image.new("RGBA", icon.size, (0, 0, 0, 0))
    shadow.putalpha(alpha.filter(ImageFilter.GaussianBlur(7)))
    base.alpha_composite(shadow, (x, y + 4))
    base.alpha_composite(icon, (x, y))


def _placeholder(size: int = 128) -> Image.Image:
    """灰色占位图标：Big Sur squircle + "?" 字形。"""
    art = linear_gradient(
        824, 824, [(0.0, (132, 137, 147, 255)), (1.0, (82, 86, 95, 255))]
    )
    glyph = Image.new("RGBA", (824, 824), (0, 0, 0, 0))
    ImageDraw.Draw(glyph).text(
        (412, 412), "?", font=load_font(400, "Semibold"), anchor="mm",
        fill=(255, 255, 255, 165),
    )
    art = Image.alpha_composite(art, glyph)
    preview = preview_from_image(art, inner=True, shadow=False)
    return preview.resize((size, size), Image.Resampling.LANCZOS)


def _wallpaper(width: int, height: int) -> Image.Image:
    canvas = linear_gradient(
        width, height,
        [(0.0, (17, 21, 34, 255)), (0.55, (9, 11, 18, 255)), (1.0, (4, 5, 10, 255))],
    )
    glows = [
        ((width * 0.16, height * 0.06), (width * 0.48, height * 0.60), (40, 130, 155, 52)),
        ((width * 0.90, height * 0.00), (width * 0.42, height * 0.60), (112, 72, 175, 44)),
        ((width * 0.55, height * 1.04), (width * 0.66, height * 0.44), (28, 52, 112, 62)),
    ]
    for center, radii, color in glows:
        canvas = Image.alpha_composite(canvas, _radial_glow((width, height), center, radii, color))

    # 极淡星点，避免纯渐变太平
    stars = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stars)
    rng = random.Random(7)
    for _ in range(int(width * height / 9000)):
        x = rng.uniform(0, width)
        y = rng.uniform(0, height * 0.72)
        r = rng.choice((0.6, 0.8, 1.0, 1.4))
        a = rng.randint(14, 48)
        sd.ellipse((x - r, y - r, x + r, y + r), fill=(220, 230, 255, a))
    canvas = Image.alpha_composite(canvas, stars)

    # 噪点（玻璃质感的一部分）
    noise = Image.effect_noise((width, height), 6).convert("RGBA")
    noise.putalpha(Image.new("L", (width, height), 9))
    return Image.alpha_composite(canvas, noise)


def _glass_dock(canvas: Image.Image, box) -> None:
    """在 canvas 上就地绘制圆角毛玻璃 Dock 条。"""
    dx, dy, dw, dh = box
    dock_mask = Image.new("L", (dw, dh), 0)
    ImageDraw.Draw(dock_mask).rounded_rectangle(
        (0, 0, dw - 1, dh - 1), radius=DOCK_RADIUS, fill=255
    )

    def composite_clipped(layer: Image.Image) -> None:
        """把 (dw×dh) 的 RGBA 层按圆角蒙版 alpha 混合进 canvas。"""
        layer = layer.copy()
        layer.putalpha(ImageChops.multiply(layer.getchannel("A"), dock_mask))
        canvas.alpha_composite(layer, (dx, dy))

    # 1) 底部投影
    shadow = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(shadow).rounded_rectangle(
        (dx + 4, dy + 14, dx + dw - 4, dy + dh + 12), radius=DOCK_RADIUS, fill=165
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(26))
    sh_layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    sh_layer.putalpha(shadow)
    canvas.alpha_composite(sh_layer)

    # 2) 背景模糊（毛玻璃）：把 Dock 区域背后的壁纸模糊后裁进圆角
    region = canvas.crop((dx, dy, dx + dw, dy + dh)).filter(ImageFilter.GaussianBlur(16))
    composite_clipped(region)

    # 3) 半透明深色渐变 + 顶部内高光 + 底部压暗
    tint = linear_gradient(
        dw, dh,
        [
            (0.00, (120, 132, 162, 72)),
            (0.16, (74, 80, 98, 104)),
            (0.55, (42, 45, 58, 138)),
            (1.00, (20, 21, 28, 184)),
        ],
    )
    composite_clipped(tint)
    highlight = linear_gradient(
        dw, dh,
        [
            (0.00, (255, 255, 255, 88)),
            (0.07, (255, 255, 255, 34)),
            (0.30, (255, 255, 255, 0)),
            (1.00, (255, 255, 255, 0)),
        ],
    )
    composite_clipped(highlight)
    darken = linear_gradient(
        dw, dh,
        [(0.0, (0, 0, 0, 0)), (0.86, (0, 0, 0, 0)), (1.0, (0, 0, 0, 42))],
    )
    composite_clipped(darken)

    # 4) 亮边：顶部最亮，往下渐隐
    edge = Image.new("L", (dw, dh), 0)
    ImageDraw.Draw(edge).rounded_rectangle(
        (0, 0, dw - 1, dh - 1), radius=DOCK_RADIUS, outline=255, width=1
    )
    vfade = linear_gradient(
        dw, dh,
        [(0.0, (255, 255, 255, 255)), (0.45, (255, 255, 255, 54)),
         (1.0, (255, 255, 255, 38))],
    )
    edge = ImageChops.multiply(edge, vfade.getchannel("A"))
    border = Image.new("RGBA", (dw, dh), (255, 255, 255, 0))
    border.putalpha(edge.point(lambda v: int(v * 0.72)))
    composite_clipped(border)

    # 5) 顶部一条更亮的细线
    top_line = Image.new("RGBA", (dw, dh), (255, 255, 255, 0))
    ImageDraw.Draw(top_line).line(
        (DOCK_RADIUS, 1, dw - DOCK_RADIUS - 1, 1), fill=(255, 255, 255, 84), width=1
    )
    composite_clipped(top_line)


def build_dock(icon_paths, *, width=2000, height=520, icon_size=128, gap=22,
               placeholders=2, title="DOCK PREVIEW", show_title=True,
               inner=True) -> Image.Image:
    n = len(icon_paths)
    if n < 1:
        raise ValueError("至少需要 1 张输入图")
    if n > 5:
        raise ValueError("最多 5 张（需求为 1–5 张，另有灰色占位参照）")
    if placeholders < 0:
        raise ValueError("placeholders 不能为负")
    icon_size = int(icon_size)
    if icon_size < 32 or icon_size > 256:
        raise ValueError("icon-size 需在 32..256 之间")

    slots = n + int(placeholders)
    pad_x = int(round(icon_size * 0.22))
    pad_y = int(round(icon_size * 0.21))
    dock_w = slots * icon_size + (slots - 1) * gap + 2 * pad_x
    dock_h = icon_size + 2 * pad_y
    if dock_w + 2 * DOCK_MIN_SIDE_MARGIN > width:
        raise ValueError(
            "画布太窄：需要至少 %d px 宽（当前 %d）；请减小 --icon-size / --gap 或增大 --width"
            % (dock_w + 2 * DOCK_MIN_SIDE_MARGIN, width)
        )
    if height < dock_h + DOCK_BOTTOM_MARGIN + 120:
        raise ValueError(
            "画布太矮：需要至少 %d px 高（当前 %d）"
            % (dock_h + DOCK_BOTTOM_MARGIN + 120, height)
        )

    canvas = _wallpaper(width, height)
    dock_x = (width - dock_w) // 2
    dock_y = height - DOCK_BOTTOM_MARGIN - dock_h
    box = (dock_x, dock_y, dock_w, dock_h)
    _glass_dock(canvas, box)

    # 图标
    icons = []
    for path in icon_paths:
        preview = preview_from_path(path, inner=inner, shadow=False)
        icons.append(preview.resize((icon_size, icon_size), Image.Resampling.LANCZOS))
    placeholder_icon = _placeholder(icon_size)
    icons.extend([placeholder_icon] * int(placeholders))

    x0 = dock_x + pad_x
    y0 = dock_y + pad_y
    for i, icon in enumerate(icons):
        _place_icon(canvas, icon, (x0 + i * (icon_size + gap), y0))

    # 文案
    if show_title:
        f_title = load_font(26, "Semibold")
        f_sub = load_font(14, "Regular")
        f_dim = load_font(13, "Regular")
        canvas_overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        od = ImageDraw.Draw(canvas_overlay)
        od.text((48, 34), title, font=f_title, fill=(234, 237, 244, 235))
        od.text(
            (48, 72),
            "%d 个图标 + %d 个灰色占位 · Dock 槽位 %d px（Big Sur 内容区 %.0f px）"
            " · squircle 4× 超采样" % (n, int(placeholders), icon_size, icon_size * 824 / 1024.0),
            font=f_sub, fill=(156, 164, 179, 225),
        )
        od.text((width - 48, 40), "%d × %d px" % (width, height), font=f_dim,
                fill=(126, 133, 148, 215), anchor="rt")
        canvas = Image.alpha_composite(canvas, canvas_overlay)

    return canvas


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="dock_preview.py",
        description="1–5 张概念图 → 仿 macOS Dock 展示图（约 2000×520）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("inputs", nargs="+", metavar="PNG",
                   help="1–5 张方形 PNG（建议 1024²）")
    p.add_argument("--out", default="dock_preview.png", help="输出 PNG 路径")
    p.add_argument("--width", type=int, default=2000, help="画布宽")
    p.add_argument("--height", type=int, default=520, help="画布高")
    p.add_argument("--icon-size", type=int, default=128, help="Dock 图标槽位尺寸")
    p.add_argument("--gap", type=int, default=22, help="图标间距")
    p.add_argument("--placeholders", type=int, default=2, help="灰色占位图标数量（0 可关闭）")
    p.add_argument("--title", default="DOCK PREVIEW", help="左上角标题")
    p.add_argument("--no-title", action="store_true", help="不画标题/说明文字")
    p.add_argument("--no-inner", action="store_true", help="关闭图标内高光/压暗")
    p.add_argument("--quiet", action="store_true", help="不打印成功信息")
    return p


def main(argv=None) -> int:
    args = _parser().parse_args(argv)
    paths = [os.path.abspath(os.path.expanduser(p)) for p in args.inputs]
    for path in paths:
        if not os.path.isfile(path):
            sys.stderr.write("error: 输入文件不存在: %s\n" % path)
            return 2

    try:
        canvas = build_dock(
            paths,
            width=args.width, height=args.height,
            icon_size=args.icon_size, gap=args.gap,
            placeholders=args.placeholders, title=args.title,
            show_title=not args.no_title, inner=not args.no_inner,
        )
    except (ValueError, OSError) as exc:
        sys.stderr.write("error: 生成 Dock 预览失败: %s\n" % exc)
        return 2

    out = os.path.abspath(os.path.expanduser(args.out))
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    canvas.convert("RGB").save(out, format="PNG")
    if not args.quiet:
        print("✅ dock_preview: %s — %d×%d px · %d 图标 + %d 占位 · 槽位 %d px"
              % (out, canvas.width, canvas.height, len(paths),
                 max(0, args.placeholders), args.icon_size))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        raise SystemExit(130)
