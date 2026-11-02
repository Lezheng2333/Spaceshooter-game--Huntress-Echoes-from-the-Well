#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Asset Studio 本地服务

职责：
  · 把构建好的 C++ 渲染器包装成 HTTP API
  · 按需渲染 + 磁盘缓存（首次 ~60ms，之后直接读缓存）
  · 用 PIL 把帧序列合成为 GIF 动画
  · 提供源码片段、扫描报告、草稿读写

零第三方依赖（除 GIF 合成用 PIL，缺失时自动降级为逐帧轮播）。

启动：python3 server.py [--port 8791] [--open]
"""

import os
import sys
import io
import re
import json
import time
import hashlib
import shutil
import subprocess
import threading
import argparse
import webbrowser
import importlib.util
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = os.path.dirname(os.path.abspath(__file__))
RENDERER = os.path.join(HERE, "assetstudio")
CACHE_DIR = os.path.join(HERE, "cache")
DRAFTS_DIR = os.path.join(HERE, "drafts")
WEB_DIR = os.path.join(HERE, "web")
GAME_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "v2.0.0", "multiple code files"))

_cache_lock = threading.Lock()
_render_lock = threading.Lock()      # C++ 渲染器每次起进程，避免并发轰炸
_assets_cache = {"t": 0, "data": None, "build": None}


# ============================================================
# 渲染器调用
# ============================================================
def run_renderer(args, timeout=60):
    return subprocess.run([RENDERER] + args, capture_output=True, text=True,
                          timeout=timeout, cwd=HERE)


def get_assets(force=False):
    """读取素材注册表（带 3 秒缓存，避免画廊频繁刷新时反复起进程）"""
    now = time.time()
    build = os.stat(RENDERER).st_mtime_ns
    if not force and _assets_cache["data"] and _assets_cache["build"] == build and now - _assets_cache["t"] < 3:
        return _assets_cache["data"]
    r = run_renderer(["--list"])
    if r.returncode != 0:
        raise RuntimeError(r.stderr or "渲染器 --list 失败")
    data = json.loads(r.stdout)
    data["gifAvailable"] = importlib.util.find_spec("PIL") is not None
    _assets_cache["data"] = data
    _assets_cache["t"] = now
    _assets_cache["build"] = build
    return data


def cache_key(parts):
    h = hashlib.sha1()
    h.update(json.dumps(parts, sort_keys=True, ensure_ascii=False).encode("utf-8"))
    return h.hexdigest()[:16]


def build_render_args(q, out_path, want_frames=None):
    """把查询参数翻译成渲染器命令行"""
    args = ["--render", q["id"][0]]
    if out_path:
        args += ["--out", out_path]
    if q.get("state"):
        args += ["--state", q["state"][0]]

    # 参数：param.<key>=<value>
    for k, v in q.items():
        if k.startswith("param."):
            args += ["--param", "%s=%s" % (k[6:], v[0])]

    # 调色板
    args += ["--palette", q.get("palette", ["none"])[0]]
    # 背景
    if q.get("bg"):
        args += ["--bg", q["bg"][0]]
    # 缩放
    if q.get("zoom"):
        args += ["--zoom", q["zoom"][0]]
    # 场景上下文
    if q.get("scene", ["0"])[0] == "1":
        args += ["--scene"]
        if q.get("scenedim"):
            args += ["--scene-dim", q["scenedim"][0]]
    # 叠加层
    for flag in ("grid", "axes", "symmetry", "bbox"):
        if q.get(flag, ["0"])[0] == "1":
            args += ["--" + flag]
    # 动画帧
    if want_frames:
        args += ["--frames", str(want_frames)]
    return args


# ============================================================
# GIF 合成
# ============================================================
def make_gif(frame_dir, out_path, fps=12, zoom=1):
    try:
        from PIL import Image
    except ImportError:
        return False
    files = sorted(f for f in os.listdir(frame_dir) if f.endswith(".png"))
    if len(files) < 2:
        return False
    imgs = [Image.open(os.path.join(frame_dir, f)).convert("RGB") for f in files]
    if zoom > 1:
        imgs = [im.resize((im.width * zoom, im.height * zoom), Image.NEAREST) for im in imgs]
    w = max(i.width for i in imgs)
    h = max(i.height for i in imgs)
    imgs = [i if (i.width, i.height) == (w, h) else i.resize((w, h), Image.NEAREST) for i in imgs]
    duration = max(20, int(1000 / max(1, fps)))
    imgs[0].save(out_path, save_all=True, append_images=imgs[1:],
                 duration=duration, loop=0, optimize=True)
    return True


# ============================================================
# 克隆草稿时使用的 C++ 模板（命名令牌替换，避免 % 与花括号冲突）
# ============================================================
DRAFT_TEMPLATE = """#pragma once

// ============================================================
// 草稿：__TITLE__ 的派生变体
//   来源素材: __SRCASSET__
//   源位置  : __SRCLOC__
//   克隆状态: __STATE____TUNED__
//
// 这里和主游戏完全隔离 —— 改坏也影响不到游戏。
// 本文件需要定义 __FN__()，
// 它由 make 自动生成的 drafts/_generated.h 调用。
//
// 参数结构体里的每个字段都会在画廊里自动变成一个滑块。
// ============================================================

#include "preview_harness.h"

struct __SYM__Params {
    // TODO: 把源素材绘制代码里的硬编码常量搬到这里
    // 例：double noseLen = 30;   → 画廊自动出现「机头长度」滑块
    double scale = 1.0;
};

inline void draw___SYM__(DrawCtx& c) {
    __SYM__Params p;
    p.scale = c.P("scale", 1.0);

    // TODO: 把源素材的绘制代码粘贴到这里，并把常量替换成 p.xxx
    // 源位置：__SRCLOC__
    (void)c; (void)p;
}

inline void __FN__() {
    reg("draft.__DRAFTID__", "__TITLE__ · 变体", "draft", "misc")
        .describe("从 __TITLE__ 派生的草稿变体。")
        .from("drafts/__SYM__/draw.h")
        .tag("草稿")
        .param("scale", "整体缩放", 0.5, 3.0, 1.0, 0.1)
        .drawFn(draw___SYM__);
}
"""


# ============================================================
# 草稿
# ============================================================
def list_drafts():
    out = []
    if not os.path.isdir(DRAFTS_DIR):
        return out
    for name in sorted(os.listdir(DRAFTS_DIR)):
        d = os.path.join(DRAFTS_DIR, name)
        if not os.path.isdir(d) or name.startswith(".") or name.startswith("_"):
            continue
        meta_path = os.path.join(d, "meta.json")
        meta = {}
        if os.path.isfile(meta_path):
            try:
                with open(meta_path, encoding="utf-8") as f:
                    meta = json.load(f)
            except (OSError, ValueError):
                meta = {"error": "meta.json 解析失败"}
        meta.setdefault("id", name)
        meta["dir"] = name
        meta["files"] = sorted(f for f in os.listdir(d) if not f.startswith("."))
        out.append(meta)
    return out


def save_draft_params(draft_id, params, state=None):
    # 客户端传素材 id；按 meta.id 找目录，避免把 draft.xxx 当成目录名。
    draft = next((d for d in list_drafts() if draft_id in (d["id"], d["dir"])), None)
    if not draft:
        return False
    d = os.path.join(DRAFTS_DIR, draft["dir"])
    asset = next((a for a in get_assets()["assets"] if a["id"] == draft["id"]), None)
    if not asset or not isinstance(params, dict):
        raise ValueError("草稿尚未编译或参数格式错误")
    specs = {p["key"]: p for p in asset.get("params", [])}
    tuned = {}
    import math
    for key, value in params.items():
        if key not in specs or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError("无效草稿参数: " + key)
        p = specs[key]
        tuned[key] = max(p["min"], min(p["max"], value))
    if state is not None and state not in [s["key"] for s in asset.get("states", [])] + [""]:
        raise ValueError("无效素材状态")
    meta_path = os.path.join(d, "meta.json")
    meta = {}
    if os.path.isfile(meta_path):
        with open(meta_path, encoding="utf-8") as f:
            meta = json.load(f)
    meta.setdefault("tuned", {}).update(tuned)
    if state is not None:
        meta["state"] = state
    meta["tunedAt"] = time.strftime("%Y-%m-%d %H:%M:%S")
    tmp = meta_path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)
    os.replace(tmp, meta_path)
    return True


# ============================================================
# 源码片段
# ============================================================
def read_source(source_field, context=4):
    """source 形如 'v2.0.0/multiple code files/player.h:215 (NightElf::draw)'"""
    m = re.match(r"^(.*?):(\d+)", source_field or "")
    if not m:
        return None
    rel, line = m.group(1), int(m.group(2))
    # 相对仓库根解析
    root = os.path.normpath(os.path.join(HERE, "..", ".."))
    for cand in (os.path.join(root, rel), os.path.join(GAME_DIR, os.path.basename(rel)),
                 os.path.join(root, rel.replace("multiple code files/", "v2.0.0/multiple code files/"))):
        if os.path.isfile(cand):
            try:
                with open(cand, encoding="utf-8", errors="replace") as f:
                    lines = f.read().split("\n")
            except OSError:
                continue
            # 行号随源码改动会漂移；优先按注册表中的 Class::draw 名称重新定位。
            token = re.search(r"\(([A-Za-z_]\w*)::(draw\w*)", source_field)
            if token:
                text = "\n".join(lines)
                cls = re.search(r"\bclass\s+" + re.escape(token[1]) + r"\b[^;{]*\{", text)
                if cls:
                    method = re.search(r"^\s*(?:(?:static|virtual|inline)\s+)*void\s+" +
                                       re.escape(token[2]) + r"\s*\(", text[cls.end():], re.M)
                    if method:
                        line = text.count("\n", 0, cls.end() + method.start()) + 1
                        while line < len(lines) and not lines[line - 1].strip():
                            line += 1
            start = max(0, line - 1 - 2)
            end = min(len(lines), line - 1 + context + 12)
            return {
                "file": rel,
                "line": line,
                "code": "\n".join("%5d | %s" % (i + 1, lines[i]) for i in range(start, end)),
            }
    return None


# ============================================================
# HTTP 处理
# ============================================================
class Handler(BaseHTTPRequestHandler):
    server_version = "AssetStudio/0.1"

    def log_message(self, fmt, *args):
        if "--verbose" in sys.argv:
            BaseHTTPRequestHandler.log_message(self, fmt, *args)

    # ---------- 工具 ----------
    def send_json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def send_bytes(self, data, ctype, code=200, cache=True):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "public, max-age=86400" if cache else "no-store")
        self.end_headers()
        self.wfile.write(data)

    def send_file(self, path, ctype, cache=False):
        # 网页自身资源（html/js/css）默认不缓存：工坊的前端是开发面，
        # 改完刷新就该看到新的，否则会拿着 24 小时前的旧 JS 排查"功能坏了"。
        # 渲染出来的图片仍走长缓存（URL 带全部参数，等于内容寻址）。
        try:
            data = open(path, "rb").read()
        except OSError:
            self.send_error(404, "not found")
            return
        self.send_bytes(data, ctype, cache=cache)

    def err(self, msg, code=400):
        self.send_json({"error": msg}, code)

    def do_GET(self):
        u = urlparse(self.path)
        path, q = u.path, parse_qs(u.query)
        try:
            if path in ("/", "/index.html"):
                return self.send_file(os.path.join(WEB_DIR, "index.html"), "text/html; charset=utf-8")
            if path.startswith("/web/"):
                rel = path[len("/web/"):]
                if ".." in rel:
                    return self.err("非法路径", 403)
                ctype = {"js": "application/javascript; charset=utf-8",
                         "css": "text/css; charset=utf-8",
                         "html": "text/html; charset=utf-8"}.get(rel.rsplit(".", 1)[-1], "text/plain")
                return self.send_file(os.path.join(WEB_DIR, rel), ctype)

            if path == "/api/assets":
                return self.send_json(get_assets(force=q.get("force", ["0"])[0] == "1"))
            if path == "/api/health":
                return self.send_json({"ok": True, "renderer": os.path.isfile(RENDERER)})
            if path == "/api/drafts":
                return self.send_json({"drafts": list_drafts()})
            if path == "/api/scan":
                return self.api_scan()
            if path == "/api/analysis":
                return self.api_analysis(q)
            if path == "/api/source":
                return self.api_source(q)
            if path == "/api/png":
                return self.api_png(q)
            if path == "/api/gif":
                return self.api_gif(q)
            if path == "/api/authored":
                return self.api_authored(q)
            return self.err("未知路径: %s" % path, 404)
        except subprocess.TimeoutExpired:
            return self.err("渲染超时", 504)
        except Exception as e:  # noqa
            import traceback
            traceback.print_exc()
            return self.err("服务端异常: %s" % e, 500)

    def do_POST(self):
        u = urlparse(self.path)
        try:
            n = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(n).decode("utf-8")) if n else {}
        except (ValueError, UnicodeDecodeError):
            return self.err("请求体不是合法 JSON")
        if u.path == "/api/draft/save":
            try:
                ok = save_draft_params(body.get("draftId", ""), body.get("params", {}), body.get("state"))
            except ValueError as e:
                return self.err(str(e))
            return self.send_json({"ok": ok}, 200 if ok else 404)
        if u.path == "/api/clone":
            return self.api_clone(body)
        return self.err("未知路径", 404)

    # ---------- API 实现 ----------
    def api_scan(self):
        r = subprocess.run([sys.executable, os.path.join(HERE, "registry", "scanner.py"), "--json"],
                           capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            return self.err(r.stderr or "扫描失败", 500)
        return self.send_json(json.loads(r.stdout))

    def api_analysis(self, q):
        if "id" not in q:
            return self.err("缺少 id")
        args = build_render_args(q, None)
        args += ["--analysis", "--ascii"]
        r = run_renderer(args)
        if r.returncode != 0:
            return self.err(r.stderr or "渲染失败", 500)
        return self.send_json({"text": r.stdout})

    def api_source(self, q):
        if "id" not in q:
            return self.err("缺少 id")
        for a in get_assets()["assets"]:
            if a["id"] == q["id"][0]:
                return self.send_json(read_source(a.get("source", "")) or {"error": "找不到源码位置"})
        return self.err("找不到素材", 404)

    def _render_cached(self, q, kind):
        """kind = 'png' 或 'gif'；返回缓存文件路径"""
        asset = None
        for a in get_assets()["assets"]:
            if a["id"] == q.get("id", [""])[0]:
                asset = a
                break
        if not asset:
            raise RuntimeError("找不到素材: %s" % q.get("id"))

        params = {k[6:]: v[0] for k, v in q.items() if k.startswith("param.")}
        overlays = {f: q.get(f, ["0"])[0] for f in ("grid", "axes", "symmetry", "bbox")}
        key_parts = {
            "id": asset["id"], "kind": kind,
            "state": q.get("state", [""])[0],
            "params": params, "overlays": overlays,
            "palette": q.get("palette", ["none"])[0],
            "bg": q.get("bg", [""])[0],
            "scene": q.get("scene", ["0"])[0],
            "scenedim": q.get("scenedim", [""])[0],
            "zoom": q.get("zoom", [""])[0],
            "frames": q.get("frames", [str(asset.get("frames") or 12)])[0],
            "fps": q.get("fps", ["12"])[0],
            "build": os.stat(RENDERER).st_mtime_ns,
            "v": 4,     # 改这里可以强制刷新所有缓存
        }
        key = cache_key(key_parts)
        ext = "png" if kind == "png" else "gif"
        cache_path = os.path.join(CACHE_DIR, "%s.%s" % (key, ext))

        # 清理同名残留目录：多帧渲染时渲染器会把 --out 当目录用，
        # 若之前以多帧渲染过同一路径，会留下 <key>.png/ 目录让 isfile 永远为假
        if os.path.isdir(cache_path):
            shutil.rmtree(cache_path, ignore_errors=True)

        if os.path.isfile(cache_path) and os.path.getsize(cache_path) > 0:
            return cache_path, asset

        frames = None
        if kind == "gif":
            frames = max(2, min(120, int(q.get("frames", [str(asset.get("frames") or 12)])[0])))
        else:
            # 静态图固定 1 帧：否则默认帧数 >1 的素材（如背景）会被当目录输出
            frames = 1

        with _render_lock:
            if os.path.isfile(cache_path) and os.path.getsize(cache_path) > 0:
                return cache_path, asset
            if os.path.isdir(cache_path):
                shutil.rmtree(cache_path, ignore_errors=True)
            if kind == "png":
                with _cache_lock:
                    args = build_render_args(q, cache_path, want_frames=frames)
                    r = run_renderer(args)
                if r.returncode != 0 or not os.path.isfile(cache_path):
                    raise RuntimeError(r.stderr or "渲染失败")
            else:
                frame_dir = os.path.join(CACHE_DIR, "frames_" + key)
                shutil.rmtree(frame_dir, ignore_errors=True)
                os.makedirs(frame_dir, exist_ok=True)
                args = build_render_args(q, frame_dir, want_frames=frames)
                r = run_renderer(args)
                if r.returncode != 0:
                    raise RuntimeError(r.stderr or "渲染失败")
                fps = max(1, min(60, int(q.get("fps", ["12"])[0])))
                if not make_gif(frame_dir, cache_path, fps=fps, zoom=1):
                    raise RuntimeError("GIF 合成失败（PIL 缺失或帧数不足）")
        return cache_path, asset

    def api_clone(self, body):
        """把素材派生为一个独立草稿目录（不动主游戏，也不动注册表）"""
        aid = body.get("id", "")
        if not aid:
            return self.err("缺少 id")

        # 目录名：id 的末段 + _variant，重名自动加序号
        base = re.sub(r"[^A-Za-z0-9_]", "_", aid.split(".")[-1]) + "_variant"
        name, n = base, 1
        while os.path.isdir(os.path.join(DRAFTS_DIR, name)):
            n += 1
            name = "%s%d" % (base, n)
        d = os.path.join(DRAFTS_DIR, name)
        os.makedirs(d, exist_ok=True)

        fn = "register_" + re.sub(r"[^A-Za-z0-9_]", "_", name)
        sym = re.sub(r"[^A-Za-z0-9_]", "_", name)
        src = body.get("source", "") or "(未记录)"
        params = body.get("params", {}) or {}
        title = body.get("name", aid)
        state = body.get("state", "")

        # 用命名令牌替换，避免 % 格式化与 C++ 花括号互相干扰
        draw_h = DRAFT_TEMPLATE
        for token, value in (
            ("__TITLE__", title), ("__SRCASSET__", aid), ("__SRCLOC__", src),
            ("__FN__", fn), ("__SYM__", sym), ("__DRAFTID__", name.lower()),
        ):
            draw_h = draw_h.replace(token, value)
        tuned_note = ("\n// 克隆时调好的参数: " + ", ".join("%s=%s" % (k, v) for k, v in params.items())) if params else ""
        draw_h = draw_h.replace("__TUNED__", tuned_note)
        draw_h = draw_h.replace("__STATE__", state or "(默认)")

        with open(os.path.join(d, "draw.h"), "w", encoding="utf-8") as f:
            f.write(draw_h)

        meta = {
            "id": "draft." + name.lower(),
            "title": body.get("name", aid) + " · 变体",
            "dir": name,
            "status": "draft",
            "derivedFrom": aid,
            "clonedAt": time.strftime("%Y-%m-%d %H:%M:%S"),
            "tuned": params,
            "state": body.get("state", ""),
            "note": "在画廊里调好的参数已记录在 tuned 字段；转正时由 AI 写回游戏代码。",
        }
        with open(os.path.join(d, "meta.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f, ensure_ascii=False, indent=2)

        return self.send_json({"ok": True, "dir": name, "file": "drafts/%s/draw.h" % name})

    def api_png(self, q):
        if "id" not in q:
            return self.err("缺少 id")
        path, _ = self._render_cached(q, "png")
        return self.send_file(path, "image/png")

    def api_gif(self, q):
        if "id" not in q:
            return self.err("缺少 id")
        path, _ = self._render_cached(q, "gif")
        return self.send_file(path, "image/gif")

    def api_authored(self, q):
        """把当前参数导出成可直接粘贴的 C++ 代码"""
        if "id" not in q:
            return self.err("缺少 id")
        asset = None
        for a in get_assets()["assets"]:
            if a["id"] == q["id"][0]:
                asset = a
                break
        if not asset:
            return self.err("找不到素材", 404)
        state = q.get("state", [asset["states"][0]["key"] if asset["states"] else ""])[0]
        params = {k[6:]: v[0] for k, v in q.items() if k.startswith("param.")}
        lines = ["// %s  (%s)" % (asset["name"], asset["id"]),
                 "// 状态: %s" % (state or "(默认)")]
        if params:
            lines.append("// 调好的参数：")
            for k, v in sorted(params.items()):
                lines.append("//   %s = %s" % (k, v))
        lines.append("//")
        lines.append("// 转正时把上面的参数代回游戏里对应的绘制代码（保持原有代码风格）。")
        if asset.get("source"):
            lines.append("// 目标位置: %s" % asset["source"])
        return self.send_json({"code": "\n".join(lines), "asset": asset["id"],
                               "state": state, "params": params})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8791)
    ap.add_argument("--open", action="store_true", help="启动后自动打开浏览器")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.makedirs(CACHE_DIR, exist_ok=True)
    os.makedirs(DRAFTS_DIR, exist_ok=True)

    if not os.path.isfile(RENDERER):
        print("✗ 找不到渲染器 %s" % RENDERER)
        print("  请先执行:  make")
        return 1

    srv = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = "http://127.0.0.1:%d/" % args.port
    print("=" * 60)
    print("  Asset Studio 素材工坊")
    print("=" * 60)
    print("  地址: %s" % url)
    print("  游戏目录: %s" % GAME_DIR)
    print("  缓存目录: %s" % CACHE_DIR)
    print("  按 Ctrl+C 停止")
    print()
    if args.open:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n已停止")
    return 0


if __name__ == "__main__":
    sys.exit(main())
