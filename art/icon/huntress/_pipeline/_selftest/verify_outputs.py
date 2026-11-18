#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_outputs.py — 自测产物的机器校验（尺寸 / 透明 / 几何 / 布局 / icns）。

由 run_selftest.sh 最后一步调用：
    python3 verify_outputs.py _selftest/out
任何一项 FAIL 都以非零码退出。
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile

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

from PIL import Image, ImageChops, ImageStat  # noqa: E402

PASS = 0
FAIL = 0


def check(name: str, ok: bool, detail: str = "") -> None:
    global PASS, FAIL
    if ok:
        PASS += 1
        print("  ✅ %s%s" % (name, ("  ·  " + detail) if detail else ""))
    else:
        FAIL += 1
        print("  ❌ %s%s" % (name, ("  ·  " + detail) if detail else ""))


def load(path: str):
    return Image.open(path).convert("RGBA")


def mean_rgb(img: Image.Image) -> "tuple[float, float, float]":
    st = ImageStat.Stat(img.convert("RGB"))
    return tuple(st.mean[:3])


def region_mean(img: Image.Image, box) -> "tuple[float, float, float]":
    return mean_rgb(img.crop(box))


def dist(a, b) -> float:
    return sum((a[i] - b[i]) ** 2 for i in range(3)) ** 0.5


def blank_std(img: Image.Image) -> float:
    return sum(ImageStat.Stat(img.convert("RGB")).stddev) / 3.0


def check_render(out: str) -> None:
    print("\n[1] render.sh 产出")
    p2 = os.path.join(out, "render_2x.png")
    p1 = os.path.join(out, "render_1x.png")
    check("render_2x.png 存在", os.path.isfile(p2))
    check("render_1x.png 存在", os.path.isfile(p1))
    if not (os.path.isfile(p2) and os.path.isfile(p1)):
        return
    im2, im1 = load(p2), load(p1)
    check("2x 输出 1024×1024", im2.size == (1024, 1024), "%dx%d" % im2.size)
    check("1x 输出 1024×1024", im1.size == (1024, 1024), "%dx%d" % im1.size)
    check("2x 非空图（stddev>6）", blank_std(im2) > 6, "stddev=%.1f" % blank_std(im2))
    diff = ImageChops.difference(im2.convert("RGB"), im1.convert("RGB"))
    md = sum(ImageStat.Stat(diff).mean) / 3.0
    check("2x 降采样与 1x 直出一致（平均差<8）", md < 8, "mean_diff=%.2f" % md)


def check_preview(out: str) -> None:
    print("\n[2] mac_preview.py 产出")
    plain = os.path.join(out, "preview_A_plain.png")
    shadow = os.path.join(out, "preview_A_shadow_inner.png")
    squircle = os.path.join(out, "preview_A_squircle.png")
    for label, path in (("plain", plain), ("shadow_inner", shadow), ("squircle", squircle)):
        check("preview_A_%s.png 存在" % label, os.path.isfile(path))
    if not all(os.path.isfile(p) for p in (plain, shadow, squircle)):
        return

    im = load(plain)
    check("画布 1024×1024 RGBA", im.size == (1024, 1024) and im.mode == "RGBA",
          "%dx%d %s" % (im.size[0], im.size[1], im.mode))
    a = im.getchannel("A")
    check("四角完全透明", a.getpixel((0, 0)) == 0 and a.getpixel((1023, 0)) == 0
          and a.getpixel((0, 1023)) == 0 and a.getpixel((1023, 1023)) == 0,
          "corner alpha=%d" % a.getpixel((0, 0)))
    check("中心不透明", a.getpixel((512, 512)) == 255, "alpha=%d" % a.getpixel((512, 512)))
    # 内容区边界: 100..923 为内容，98/925 为外部
    check("内容区左边界 x=100 不透明", a.getpixel((100, 512)) >= 250,
          "alpha=%d" % a.getpixel((100, 512)))
    check("内容区外 x=98 透明", a.getpixel((98, 512)) <= 5,
          "alpha=%d" % a.getpixel((98, 512)))
    check("内容区底边 y=923 不透明", a.getpixel((512, 923)) >= 250,
          "alpha=%d" % a.getpixel((512, 923)))
    check("内容区外 y=925 透明", a.getpixel((512, 925)) <= 5,
          "alpha=%d" % a.getpixel((512, 925)))
    check("圆角裁切：内容区内侧角 (108,108) 透明", a.getpixel((108, 108)) <= 10,
          "alpha=%d" % a.getpixel((108, 108)))
    bbox = a.getbbox()
    check("有效内容 bbox ≈ (100,100,924,924)", abs(bbox[0] - 100) <= 2 and abs(bbox[1] - 100) <= 2
          and abs(bbox[2] - 924) <= 2 and abs(bbox[3] - 924) <= 2, "bbox=%s" % (bbox,))

    # 投影
    sh = load(shadow)
    sa = sh.getchannel("A")
    check("--shadow 在图标下方产生投影", sa.getpixel((512, 945)) > 4,
          "alpha(512,945)=%d" % sa.getpixel((512, 945)))
    check("--shadow 不污染画布四角", sa.getpixel((2, 2)) <= 2 and sa.getpixel((1021, 1021)) <= 2,
          "corner=%d" % sa.getpixel((2, 2)))
    diff_mean = sum(ImageStat.Stat(
        ImageChops.difference(sh.convert("RGB"), im.convert("RGB"))).mean) / 3.0
    check("--shadow 与 plain 有可见差异", diff_mean > 0.5, "mean_diff=%.2f" % diff_mean)

    # 内高光：顶部条带比 plain 更亮
    top_plain = region_mean(im, (300, 102, 724, 132))
    top_inner = region_mean(sh, (300, 102, 724, 132))
    check("--inner 顶部内高光提亮", top_inner[0] > top_plain[0] + 0.2,
          "%.2f > %.2f" % (top_inner[0], top_plain[0]))

    # squircle 形状
    sq = load(squircle)
    check("--shape squircle 四角比 rounded 更饱满", sq.getchannel("A").getpixel((140, 140)) > 200,
          "alpha(140,140)=%d" % sq.getchannel("A").getpixel((140, 140)))
    check("squircle 仍保留 100 边距", sq.getchannel("A").getpixel((512, 102)) >= 250,
          "alpha(512,102)=%d" % sq.getchannel("A").getpixel((512, 102)))


def check_contact(out: str) -> None:
    print("\n[3] contact_sheet.py 产出（5 张图排版）")
    path = os.path.join(out, "contact_sheet.png")
    check("contact_sheet.png 存在", os.path.isfile(path))
    if not os.path.isfile(path):
        return
    im = load(path)
    check("宽度 2200", im.width == 2200, "%d×%d" % im.size)
    check("高度在合理区间 1100..1500", 1100 <= im.height <= 1500, "高度=%d" % im.height)
    check("非空图", blank_std(im) > 4, "stddev=%.1f" % blank_std(im))
    check("深灰背景（角落亮度<70）", sum(region_mean(im, (0, 0, 40, 40))) / 3 < 70,
          "corner=%.1f" % (sum(region_mean(im, (0, 0, 40, 40))) / 3))

    # 第一行 5 张缩略图应呈现 5 种不同色相
    margin, gap, n = 64, 22, 5
    col_w = (2200 - 2 * margin - (n - 1) * gap) / float(n)
    thumb = int(round(col_w))
    y = margin + 96 + 40 + 36  # header + row1 caption + cell label
    cy = y + thumb // 2
    means = []
    for i in range(n):
        cx = int(round(margin + i * (col_w + gap) + col_w / 2.0))
        means.append(region_mean(im, (cx - 90, cy - 90, cx + 90, cy + 90)))
    pairs = [(i, j) for i in range(n) for j in range(i + 1, n)]
    distinct = sum(1 for i, j in pairs if dist(means[i], means[j]) > 30)
    check("第一行 5 张缩略色相可区分", distinct >= 6,
          "%d/%d 对颜色距离>30" % (distinct, len(pairs)))

    # 第三行 128px 小图标应存在（非背景像素）
    y3 = y + thumb + 34 + 40 + 300 + 34 + 40
    small_means = []
    for i in range(n):
        cx = int(round(margin + i * (col_w + gap) + col_w / 2.0))
        small_means.append(region_mean(im, (cx - 60, y3 + 30, cx + 60, y3 + 90)))
    check("第三行 5 个小尺寸图标可见", all(sum(m) / 3 > 24 for m in small_means),
          "亮度 min=%.1f" % min(sum(m) / 3 for m in small_means))


def check_dock(out: str) -> None:
    print("\n[4] dock_preview.py 产出")
    path = os.path.join(out, "dock_preview.png")
    check("dock_preview.png 存在", os.path.isfile(path))
    if not os.path.isfile(path):
        return
    im = load(path)
    check("尺寸 2000×520", im.size == (2000, 520), "%d×%d" % im.size)
    check("非空图", blank_std(im) > 4, "stddev=%.1f" % blank_std(im))

    # Dock 玻璃条可见（dock 内亮度高于背景）
    dock_pad = region_mean(im, (480, 300, 1540, 470))
    wall = region_mean(im, (100, 80, 600, 160))
    check("Dock 毛玻璃条可见（亮度高于壁纸）", sum(dock_pad) > sum(wall),
          "dock=%.1f wall=%.1f" % (sum(dock_pad) / 3, sum(wall) / 3))

    # 按脚本默认几何推算 5 个彩色图标 + 2 个灰色占位
    icon, gap, ph = 128, 22, 2
    pad_x = int(round(icon * 0.22))
    slots = 5 + ph
    dock_w = slots * icon + (slots - 1) * gap + 2 * pad_x
    dock_x = (2000 - dock_w) // 2
    dock_y = 520 - 54 - (icon + 2 * int(round(icon * 0.21)))
    x0 = dock_x + pad_x
    y0 = dock_y + int(round(icon * 0.21))
    sat = []
    for i in range(slots):
        cx = x0 + i * (icon + gap) + icon // 2
        cy = y0 + icon // 2
        m = region_mean(im, (cx - 22, cy - 22, cx + 22, cy + 22))
        sat.append(max(m) - min(m))
    check("5 个彩色图标在 Dock 中可见", all(s > 22 for s in sat[:5]),
          "饱和度=%s" % ["%.0f" % s for s in sat[:5]])
    check("2 个灰色占位图标为中性灰", all(s < 14 for s in sat[5:]),
          "饱和度=%s" % ["%.0f" % s for s in sat[5:]])


def check_icns(out: str) -> None:
    print("\n[5] make_icns.sh 产出")
    path = os.path.join(out, "test_A.icns")
    check("test_A.icns 存在", os.path.isfile(path))
    if not os.path.isfile(path):
        return
    size = os.path.getsize(path)
    with open(path, "rb") as fh:
        magic = fh.read(4)
    check("icns magic 正确", magic == b"icns", "magic=%r" % magic)
    check("icns 体积合理（>50 KB）", size > 50 * 1024, "%d bytes" % size)

    tmp = tempfile.mkdtemp(prefix="icns_verify_")
    try:
        iconset = os.path.join(tmp, "verify.iconset")
        rc = subprocess.run(["/usr/bin/iconutil", "-c", "iconset", "-o", iconset, path],
                            capture_output=True)
        check("iconutil 可回读 icns", rc.returncode == 0, rc.stderr.decode("utf-8", "replace").strip())
        if rc.returncode == 0:
            reps = sorted(f for f in os.listdir(iconset) if f.endswith(".png"))
            check("回读含 10 个尺寸 rep", len(reps) == 10, "%d 个" % len(reps))
            biggest = 0
            for name in reps:
                with Image.open(os.path.join(iconset, name)) as im:
                    biggest = max(biggest, im.width)
            check("包含 1024 像素（512@2x）", biggest == 1024, "max=%d" % biggest)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    out = os.path.abspath(argv[0]) if argv else os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "out"))
    print("verify_outputs.py — 校验目录: %s" % out)
    check_render(out)
    check_preview(out)
    check_contact(out)
    check_dock(out)
    check_icns(out)

    total = PASS + FAIL
    print("\n%s  %d/%d 项通过" % ("✅ ALL PASS" if FAIL == 0 else "❌ 有失败项", PASS, total))
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        raise SystemExit(130)
