#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""contact_sheet.py — 5 张 1024² 概念图 → 一张 ~2200 宽的 macOS 呈现展示板。

三行结构（与需求一一对应）:
    第一行  5 张方形原图缩略（约 400 px）
    第二行  5 张 macOS Big Sur squircle 预览（约 300 px，824 内容区 + 100 边距）
    第三行  每张图实际 128 px 与 64 px 小尺寸并排（检查缩小后的识别度）

深灰背景、浅色文字标签、细网格分隔。预览几何/抗锯齿复用 mac_preview.py。

用法:
    python3 contact_sheet.py --inputs a.png b.png c.png d.png e.png \
        --labels "A 红楔" "B 蓝环" "C 绿棱" "D 紫涡" "E 金冠" \
        --out contact_sheet.png
"""

from __future__ import annotations

import argparse
import os
import sys

# Pillow 自举（DSH 自带 Python）；与 mac_preview.py 同一套逻辑
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

from PIL import Image, ImageDraw, ImageFilter  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mac_preview import (linear_gradient, load_font,  # noqa: E402
                         preview_from_image, text_size)

# ---------------------------------------------------------------------------
# 视觉常量
# ---------------------------------------------------------------------------
BG_TOP = (27, 28, 33)
BG_BOTTOM = (16, 17, 21)
INK = (238, 239, 243)
DIM = (143, 148, 158)
FAINT = (97, 102, 112)
ACCENT = (150, 163, 255)
GRID = (255, 255, 255, 16)
HAIR = (255, 255, 255, 26)
PLATE = (255, 255, 255, 7)

MARGIN = 64
COL_GAP = 22
HEADER_H = 96
ROW_CAP_H = 40
CELL_LABEL_H = 36
ROW_GAP = 34
FOOTER_H = 64
SMALL_GAP = 26
SMALL_CAP_H = 24


def _open_square(path: str) -> Image.Image:
    with Image.open(path) as im:
        im.load()
        im = im.convert("RGBA")
    if im.width != im.height:
        raise ValueError("%s 不是正方形（%d×%d）" % (os.path.basename(path), im.width, im.height))
    return im


def _paste_thumb(canvas: Image.Image, art: Image.Image, box: int, xy) -> None:
    thumb = art.resize((box, box), Image.Resampling.LANCZOS)
    canvas.alpha_composite(thumb, xy)


def _draw_border(overlay: Image.Image, xy, size: int, color=HAIR) -> None:
    x, y = xy
    ImageDraw.Draw(overlay).rectangle(
        (x - 1, y - 1, x + size, y + size), outline=color, width=1
    )


def build_sheet(inputs, labels, *, width=2200, title="ICON REDESIGN · CONTACT SHEET",
                preview_shadow=False, preview_inner=True) -> Image.Image:
    n = len(inputs)
    if n < 1:
        raise ValueError("至少需要 1 张输入图")
    if n > 6:
        raise ValueError("最多 6 张（需求为 5 张；更多请拆成多张展示板）")
    if width < 1200:
        raise ValueError("--width 至少 1200，当前 %d" % width)

    # 列宽按「至少 5 列」的尺寸算：1–4 张时保持和 5 张板相同的列宽，整组居中，
    # 避免单张图被拉成 2000px 宽。
    layout_cols = max(n, 5)
    col_w = (width - 2 * MARGIN - (layout_cols - 1) * COL_GAP) / float(layout_cols)
    grid_w = n * col_w + (n - 1) * COL_GAP
    x_off = (width - grid_w) / 2.0
    thumb = int(round(col_w))
    preview = int(min(round(col_w - 84), 300))
    small_h = 128 + SMALL_CAP_H
    height = int(
        MARGIN
        + HEADER_H
        + ROW_CAP_H + CELL_LABEL_H + thumb
        + ROW_GAP
        + ROW_CAP_H + preview
        + ROW_GAP
        + ROW_CAP_H + small_h
        + FOOTER_H
    )

    canvas = linear_gradient(width, height, [(0.0, BG_TOP), (1.0, BG_BOTTOM)])
    # 顶部一层很淡的冷色光，避免整板死平
    glow = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((-width * 0.25, -height * 0.55, width * 1.25, height * 0.45),
               fill=(120, 140, 255, 14))
    glow = glow.filter(ImageFilter.GaussianBlur(90))
    canvas = Image.alpha_composite(canvas, glow)

    overlay = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)

    # 字体
    f_title = load_font(30, "Semibold")
    f_sub = load_font(15, "Regular")
    f_brand = load_font(13, "Semibold")
    f_cap = load_font(14, "Semibold")
    f_label = load_font(21, "Medium")
    f_note = load_font(13, "Regular")
    f_tiny = load_font(12, "Regular")

    d = ImageDraw.Draw(canvas)
    y = MARGIN

    # ---------------- header ----------------
    d.text((MARGIN, y + 4), title, font=f_title, fill=INK)
    d.text(
        (MARGIN, y + 50),
        "%d 张 1024² 概念稿 · macOS Big Sur 图标呈现预览 · "
        "824 内容区 / 185.4 圆角 / 100 边距 / 4× 超采样" % n,
        font=f_sub, fill=DIM,
    )
    brand = "HUNTRESS · ECHOES FROM THE WELL"
    d.text((width - MARGIN, y + 8), brand, font=f_brand, fill=FAINT, anchor="rt")
    d.text((width - MARGIN, y + 52), "%d × %d px" % (width, height), font=f_tiny, fill=FAINT,
           anchor="rt")
    od.line((MARGIN, y + HEADER_H - 22, width - MARGIN, y + HEADER_H - 22), fill=GRID, width=1)
    y += HEADER_H

    def row_caption(index: str, text: str) -> None:
        nonlocal y
        d.text((MARGIN, y + 8), index, font=f_cap, fill=ACCENT)
        iw, _ = text_size(d, index, f_cap)
        d.text((MARGIN + iw + 12, y + 8), text, font=f_cap, fill=DIM)
        y += ROW_CAP_H

    def col_x(i: int) -> int:
        return int(round(x_off + i * (col_w + COL_GAP)))

    # ---------------- row 1: 原图 ----------------
    row_caption("01", "原图 · 1024 × 1024（概念稿，未套壳）")
    row1_top = y
    for i, art in enumerate(inputs):
        x = col_x(i)
        d.text((x, y), labels[i], font=f_label, fill=INK)
        d.text((x + thumb, y + 4), "1024²", font=f_tiny, fill=FAINT, anchor="rt")
        _paste_thumb(canvas, art, thumb, (x, y + CELL_LABEL_H))
        _draw_border(overlay, (x, y + CELL_LABEL_H), thumb)
    y += CELL_LABEL_H + thumb

    # ---------------- row 2: macOS 预览 ----------------
    y += ROW_GAP // 2
    od.line((MARGIN, y, width - MARGIN, y), fill=GRID, width=1)
    y += ROW_GAP - ROW_GAP // 2
    row_caption("02", "macOS 预览 · 824 内容区 + 100 边距 + 185.4 圆角（Big Sur 规范）")
    row2_top = y
    previews = []
    for i, art in enumerate(inputs):
        pv = preview_from_image(art, inner=preview_inner, shadow=preview_shadow)
        previews.append(pv)
        small = pv.resize((preview, preview), Image.Resampling.LANCZOS)
        x = int(round(col_x(i) + (col_w - preview) / 2.0))
        plate = (x - 12, y - 8, x + preview + 12, y + preview + 8)
        od.rounded_rectangle(plate, radius=18, fill=PLATE)
        canvas.alpha_composite(small, (x, y))
        _draw_border(overlay, (x, y), preview, color=(255, 255, 255, 20))
    y += preview

    # ---------------- row 3: 小尺寸 ----------------
    y += ROW_GAP // 2
    od.line((MARGIN, y, width - MARGIN, y), fill=GRID, width=1)
    y += ROW_GAP - ROW_GAP // 2
    row_caption("03", "实际小尺寸 · 128 px 与 64 px（检查缩到 Dock / Finder 后的识别度）")
    row3_top = y
    small_128 = 128
    small_64 = 64
    block_w = small_128 + SMALL_GAP + small_64
    for i, pv in enumerate(previews):
        x = int(round(col_x(i) + (col_w - block_w) / 2.0))
        s128 = pv.resize((small_128, small_128), Image.Resampling.LANCZOS)
        s64 = pv.resize((small_64, small_64), Image.Resampling.LANCZOS)
        canvas.alpha_composite(s128, (x, y))
        canvas.alpha_composite(s64, (x + small_128 + SMALL_GAP, y + (small_128 - small_64) // 2))
        d.text((x + small_128 // 2, y + small_128 + 4), "128", font=f_tiny, fill=FAINT,
               anchor="mt")
        d.text((x + small_128 + SMALL_GAP + small_64 // 2,
                y + (small_128 + small_64) // 2 + 4), "64", font=f_tiny, fill=FAINT,
               anchor="mt")
    y += small_h

    # ---------------- 网格 / footer ----------------
    # 竖向细网格线（列间）
    for i in range(1, n):
        gx = int(round(x_off + i * (col_w + COL_GAP) - COL_GAP / 2.0))
        od.line((gx, row1_top - 10, gx, row3_top + small_h - 6), fill=GRID, width=1)
    od.line((MARGIN, y + 22, width - MARGIN, y + 22), fill=GRID, width=1)

    canvas = Image.alpha_composite(canvas, overlay)
    d = ImageDraw.Draw(canvas)
    feat = "shadow=on" if preview_shadow else "shadow=off"
    d.text(
        (MARGIN, y + 38),
        "generated by contact_sheet.py · squircle mask 4× supersampled · %s · inner=%s"
        % (feat, "on" if preview_inner else "off"),
        font=f_note, fill=FAINT,
    )
    d.text((width - MARGIN, y + 38), "Big Sur: 824 / 185.4 / 100",
           font=f_note, fill=FAINT, anchor="rt")

    return canvas, {"thumb": thumb, "preview": preview, "col_w": col_w, "height": height}


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="contact_sheet.py",
        description="5 张 1024² 概念图 → 三行 macOS 展示板（宽约 2200）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--inputs", nargs="+", required=True, metavar="PNG",
                   help="输入方形 PNG 列表（建议 5 张，1024²）")
    p.add_argument("--labels", nargs="+", default=None, metavar="TEXT",
                   help="每张图的标签，如 'A 红楔'；数量必须与 --inputs 一致")
    p.add_argument("--out", default="contact_sheet.png", help="输出 PNG 路径")
    p.add_argument("--width", type=int, default=2200, help="展示板宽度（px）")
    p.add_argument("--title", default="ICON REDESIGN · CONTACT SHEET", help="左上角标题")
    p.add_argument("--shadow", action="store_true", help="macOS 预览带柔和投影")
    p.add_argument("--no-inner", action="store_true", help="关闭预览内高光/压暗")
    p.add_argument("--quiet", action="store_true", help="不打印成功信息")
    return p


def main(argv=None) -> int:
    args = _parser().parse_args(argv)
    inputs = [os.path.abspath(os.path.expanduser(p)) for p in args.inputs]
    for path in inputs:
        if not os.path.isfile(path):
            sys.stderr.write("error: 输入文件不存在: %s\n" % path)
            return 2
    labels = args.labels
    if labels is None:
        labels = [os.path.splitext(os.path.basename(p))[0] for p in inputs]
    if len(labels) != len(inputs):
        sys.stderr.write(
            "error: --labels 数量（%d）必须与 --inputs 数量（%d）一致\n"
            % (len(labels), len(inputs))
        )
        return 2

    try:
        arts = [_open_square(p) for p in inputs]
        sheet, meta = build_sheet(
            arts, labels, width=args.width, title=args.title,
            preview_shadow=args.shadow, preview_inner=not args.no_inner,
        )
    except (ValueError, OSError) as exc:
        sys.stderr.write("error: 生成展示板失败: %s\n" % exc)
        return 2

    out = os.path.abspath(os.path.expanduser(args.out))
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    sheet.convert("RGB").save(out, format="PNG")
    if not args.quiet:
        print("✅ contact_sheet: %s — %d×%d px" % (out, sheet.width, sheet.height))
        print("   %d 列 · 原图 %d px · macOS 预览 %d px · 小尺寸 128 + 64"
              % (len(inputs), meta["thumb"], meta["preview"]))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        raise SystemExit(130)
