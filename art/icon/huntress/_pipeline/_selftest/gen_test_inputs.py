#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_test_inputs.py — 生成 _selftest 用的 5 张 1024² 测试概念图。

不是真实设计稿，只是管线自测用的「不同色相 + 不同形状」模拟素材：
    test_A  红楔   红黑径向底 + 米白三角楔
    test_B  蓝环   海军蓝底 + 青色圆环
    test_C  绿棱   深青底 + 青柠菱形
    test_D  紫涡   深紫底 + 品红/靛蓝螺旋
    test_E  金冠   暗金底 + 金色三尖冠

全部 2× 超采样绘制后缩到 1024，避免锯齿。输出到 _selftest/inputs/。
"""

from __future__ import annotations

import argparse
import math
import os
import sys

# Pillow 自举
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

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from mac_preview import linear_gradient  # noqa: E402

SIZE = 1024
SS = 2  # 超采样倍数


def _radial_bg(size: int, inner, outer) -> Image.Image:
    """中心亮、边缘暗的径向渐变底。"""
    rad = ImageOps.invert(Image.radial_gradient("L")).resize((size, size), Image.BICUBIC)
    bg = Image.composite(
        Image.new("RGB", (size, size), tuple(inner)),
        Image.new("RGB", (size, size), tuple(outer)),
        rad,
    )
    return bg.convert("RGBA")


def _vignette(img: Image.Image, strength: float = 0.24) -> Image.Image:
    """四角压暗（只作用 RGB，alpha 保持不透明 —— 概念稿是实心方图）。"""
    size = img.size[0]
    rad = Image.radial_gradient("L").resize((size, size), Image.BICUBIC)
    mask = rad.point(lambda v: int(v * strength))
    rgb = img.convert("RGB")
    dark = rgb.point(lambda v: int(v * 0.45))
    return ImageChops.composite(dark, rgb, mask).convert("RGBA")


def _polygon_mask(size: int, points) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    return mask


def _gradient_shape(size: int, points, top, bottom) -> Image.Image:
    grad = linear_gradient(size, size, [(0.0, tuple(top) + (255,)), (1.0, tuple(bottom) + (255,))])
    mask = _polygon_mask(size, points)
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    layer.paste(grad, (0, 0), mask)
    return layer


def _glow(size: int, points, color, blur: int = 46, alpha: int = 120) -> Image.Image:
    mask = _polygon_mask(size, points).filter(ImageFilter.GaussianBlur(blur))
    layer = Image.new("RGBA", (size, size), tuple(color[:3]) + (0,))
    layer.putalpha(mask.point(lambda v: int(v * alpha / 255)))
    return layer


def _art_a(size: int) -> Image.Image:
    s = size
    img = _radial_bg(s, (128, 24, 30), (18, 4, 7))
    tri = [(s * 0.50, s * 0.24), (s * 0.76, s * 0.74), (s * 0.24, s * 0.74)]
    img = Image.alpha_composite(img, _glow(s, tri, (255, 70, 60), blur=60, alpha=130))
    img = Image.alpha_composite(img, _gradient_shape(s, tri, (255, 238, 205), (255, 120, 110)))
    # 三角内部一条暗红阴影线
    d = ImageDraw.Draw(img)
    d.line((s * 0.50, s * 0.31, s * 0.66, s * 0.70), fill=(190, 40, 55, 120), width=int(s * 0.012))
    return _vignette(img)


def _art_b(size: int) -> Image.Image:
    s = size
    img = _radial_bg(s, (24, 66, 130), (4, 8, 20))
    box = (s * 0.24, s * 0.24, s * 0.76, s * 0.76)
    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse(box, outline=(120, 225, 255, 210), width=int(s * 0.10))
    glow = glow.filter(ImageFilter.GaussianBlur(24))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    d.ellipse(box, outline=(160, 240, 255, 255), width=int(s * 0.075))
    d.ellipse((s * 0.44, s * 0.44, s * 0.56, s * 0.56), fill=(225, 250, 255, 255))
    d.arc((s * 0.30, s * 0.30, s * 0.70, s * 0.70), 200, 340, fill=(230, 250, 255, 180),
          width=int(s * 0.012))
    return _vignette(img)


def _art_c(size: int) -> Image.Image:
    s = size
    img = _radial_bg(s, (16, 110, 92), (3, 16, 14))
    dia = [(s * 0.50, s * 0.17), (s * 0.83, s * 0.50), (s * 0.50, s * 0.83), (s * 0.17, s * 0.50)]
    img = Image.alpha_composite(img, _glow(s, dia, (100, 255, 150), blur=64, alpha=95))
    img = Image.alpha_composite(img, _gradient_shape(s, dia, (205, 255, 170), (30, 190, 120)))
    d = ImageDraw.Draw(img)
    inner = [(s * 0.50, s * 0.34), (s * 0.66, s * 0.50), (s * 0.50, s * 0.66), (s * 0.34, s * 0.50)]
    d.polygon(inner, fill=(10, 70, 55, 215))
    d.line((s * 0.50, s * 0.34, s * 0.66, s * 0.50), fill=(235, 255, 230, 180), width=int(s * 0.012))
    return _vignette(img)


def _art_d(size: int) -> Image.Image:
    s = size
    img = _radial_bg(s, (86, 26, 120), (10, 4, 20))
    # 阿基米德螺旋：颜色随进度从品红 → 紫 → 青，半径收在画布安全区内
    t_max = 6.0 * math.pi
    steps = 260
    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    prev = None
    for i in range(steps + 1):
        u = i / float(steps)
        t = t_max * u
        r = s * (0.035 + 0.395 * u)
        x = s * 0.5 + r * math.cos(t)
        y = s * 0.5 + r * math.sin(t)
        if prev is not None:
            color = (
                int(255 * (1 - u) + 40 * u),
                int(40 * (1 - u) + 190 * u),
                int(200 * (1 - u) + 255 * u),
                235,
            )
            d.line((prev[0], prev[1], x, y), fill=color,
                   width=max(2, int(s * (0.019 - 0.009 * u))))
        prev = (x, y)
    img = Image.alpha_composite(img, layer.filter(ImageFilter.GaussianBlur(1.4)))
    d = ImageDraw.Draw(img)
    d.ellipse((s * 0.42, s * 0.42, s * 0.58, s * 0.58), fill=(255, 240, 255, 235))
    d.ellipse((s * 0.47, s * 0.47, s * 0.53, s * 0.53), fill=(140, 60, 200, 255))
    return _vignette(img)


def _art_e(size: int) -> Image.Image:
    s = size
    img = _radial_bg(s, (120, 82, 18), (20, 12, 4))
    crown = [
        (s * 0.24, s * 0.70),
        (s * 0.24, s * 0.36),
        (s * 0.37, s * 0.52),
        (s * 0.50, s * 0.24),
        (s * 0.63, s * 0.52),
        (s * 0.76, s * 0.36),
        (s * 0.76, s * 0.70),
    ]
    img = Image.alpha_composite(img, _glow(s, crown, (255, 200, 90), blur=58, alpha=110))
    img = Image.alpha_composite(img, _gradient_shape(s, crown, (255, 245, 190), (255, 170, 40)))
    d = ImageDraw.Draw(img)
    d.rectangle((s * 0.24, s * 0.70, s * 0.76, s * 0.78), fill=(255, 200, 90, 255))
    for cx in (0.32, 0.50, 0.68):
        d.ellipse((s * (cx - 0.032), s * 0.725, s * (cx + 0.032), s * 0.787),
                  fill=(255, 240, 210, 235))
    d.line((s * 0.30, s * 0.63, s * 0.70, s * 0.63), fill=(160, 100, 20, 160), width=int(s * 0.010))
    return _vignette(img)


ARTS = {
    "test_A": ("红楔", _art_a),
    "test_B": ("蓝环", _art_b),
    "test_C": ("绿棱", _art_c),
    "test_D": ("紫涡", _art_d),
    "test_E": ("金冠", _art_e),
}


def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    p = argparse.ArgumentParser(description="生成 _selftest 用的 5 张 1024² 测试概念图")
    p.add_argument("--out-dir", default=os.path.join(here, "inputs"),
                   help="输出目录（默认 _selftest/inputs）")
    args = p.parse_args(argv)
    out_dir = os.path.abspath(os.path.expanduser(args.out_dir))
    os.makedirs(out_dir, exist_ok=True)

    big = SIZE * SS
    for name, (label, fn) in ARTS.items():
        art = fn(big).resize((SIZE, SIZE), Image.Resampling.LANCZOS)
        path = os.path.join(out_dir, name + ".png")
        art.save(path, format="PNG")
        print("  %-7s %-4s %s" % (name, label, path))

    # 错误路径用的 fixture（不属于 5 张设计稿）
    Image.new("RGB", (1024, 768), (40, 42, 48)).save(
        os.path.join(out_dir, "non_square.png"), format="PNG")
    with Image.open(os.path.join(out_dir, "test_A.png")) as a:
        a.convert("RGB").resize((512, 512), Image.Resampling.LANCZOS).save(
            os.path.join(out_dir, "test_A_512.png"), format="PNG")
    print("  fixtures non_square.png / test_A_512.png（错误路径检查用）")
    print("✅ 5 张测试输入已生成 → %s" % out_dir)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        raise SystemExit(130)
