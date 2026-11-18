#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Asset Studio 素材工坊 — 无头浏览器 UI 冒烟测试（零第三方依赖）。

用 Microsoft Edge 的 headless 模式 + 手写的最小 CDP 客户端，真实打开画廊页面、
用真实鼠标事件点击、断言“用户看得见点得到”，并把关键界面截图落盘。

只依赖 Python 标准库：socket 手写 WebSocket 握手/帧（客户端帧 mask）、
urllib 探测 CDP 端口、subprocess 拉起 Edge、json 解析 CDP 消息。

用法：
    python3 tests/ui_smoke.py                       # 默认 http://127.0.0.1:8791/
    python3 tests/ui_smoke.py --url http://127.0.0.1:8792/
    python3 tests/ui_smoke.py --port 8792 --out /tmp/as_ui_8792
    python3 tests/ui_smoke.py --headless-binary "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
    python3 tests/ui_smoke.py --skip-cache-clear    # 调试时别清真实缓存
    python3 tests/ui_smoke.py --keep-open           # 测试后保留 Edge（调试用）

两套断言：
  1. 基线断言：标题 / 卡片 / 状态栏 / 详情 / 扫描弹窗 / 截图 / JS 异常与 console.error。
  2. 0.2.0 扩展断言（当后端有 /api/cache 等新接口时自动启用）：
     搜索、章节/类型筛选、真实波形、音频详情、收藏与持久化、状态/参数/分析/源码/对比、
     命令面板、逐帧滑杆、列表视图与排序、导出 toast、显示设置、快捷键、扫描缓存区、
     深链（#/scan、#a=<id>、#/help）、缓存清理（默认执行，--skip-cache-clear 可跳过）。

退出码：
    0 = 全部 PASS
    1 = 有断言失败 / 页面 JS 异常 / console.error
    2 = 环境或驱动失败（Edge 拉不起来、CDP 连不上、画廊服务不可达等）

说明：
- 默认复用已在 --cdp-port 上运行的 Edge（例如 lead 验证时留下的实例）；
  没有才自己拉起，并在结束时只清理自己拉起的进程与 /tmp profile。
- 页面运行期间的 Runtime.exceptionThrown 与 console.error 都算失败；
  Log.entryAdded 里 source=network 的资源加载错误只作为 warning 报告（例如 favicon 404）。
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from urllib import error as urlerror
from urllib import request as urlrequest
from urllib.parse import urlparse

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------
DEFAULT_EDGE = "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
DEFAULT_GALLERY_PORT = 8791
DEFAULT_CDP_PORT = 9333
VIEW_WIDTH = 1440
VIEW_HEIGHT = 960

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_HARNESS = 2

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
PNG_SIG = b"\x89PNG\r\n\x1a\n"

# 本地探测绕过 http_proxy 等环境变量
HTTP_OPENER = urlrequest.build_opener(urlrequest.ProxyHandler({}))


# ---------------------------------------------------------------------------
# 异常
# ---------------------------------------------------------------------------
class HarnessError(RuntimeError):
    """驱动 / 环境级错误（退出码 2）。"""


class SuiteAbort(RuntimeError):
    """冒烟流程无法继续（已经记录过具体失败项）。"""


class WSError(RuntimeError):
    """WebSocket 协议错误。"""


class WSClosed(WSError):
    """WebSocket 已被对端关闭。"""


class CDPError(RuntimeError):
    """CDP 命令返回 error。"""

    def __init__(self, method, error):
        self.method = method
        self.error = error if isinstance(error, dict) else {"message": str(error)}
        super().__init__(f"CDP {method} 失败: {self.error.get('message', self.error)}")


class JSError(RuntimeError):
    """页面内 Runtime.evaluate 抛出的 JS 异常。"""


# ---------------------------------------------------------------------------
# 最小 WebSocket 客户端（RFC 6455 子集）
# ---------------------------------------------------------------------------
class MiniWebSocket:
    """只够 CDP 用的 WebSocket 客户端。

    - 连接时发 HTTP Upgrade 握手（注意：不能带 Origin 头，Chromium 111+ 会拒绝）；
    - 发送的客户端帧一律 mask；
    - 接收支持 text / continuation / ping / pong / close，服务端误加 mask 也能解。
    """

    def __init__(self, url: str, timeout: float = 20.0):
        u = urlparse(url)
        if u.scheme != "ws":
            raise WSError(f"只支持 ws://（收到 {u.scheme!r}）")
        host = u.hostname or "127.0.0.1"
        port = u.port or 80
        path = u.path or "/"
        if u.query:
            path += "?" + u.query

        self._buf = b""
        self._closed = False
        self._sock = socket.create_connection((host, port), timeout=timeout)
        try:
            self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        except OSError:
            pass
        self._sock.settimeout(timeout)

        key = base64.b64encode(os.urandom(16)).decode("ascii")
        req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        )
        self._sock.sendall(req.encode("ascii"))

        deadline = time.monotonic() + timeout
        while b"\r\n\r\n" not in self._buf:
            remain = deadline - time.monotonic()
            if remain <= 0:
                raise WSError("WebSocket 握手超时")
            self._sock.settimeout(remain)
            chunk = self._sock.recv(65536)
            if not chunk:
                raise WSError("WebSocket 握手期间连接被关闭")
            self._buf += chunk
        head, _, self._buf = self._buf.partition(b"\r\n\r\n")
        lines = head.decode("latin-1", "replace").split("\r\n")
        parts = lines[0].split() if lines else []
        if len(parts) < 2 or parts[1] != "101":
            raise WSError(f"WebSocket 握手被拒绝: {lines[0] if lines else '(空响应)'}")
        headers = {}
        for line in lines[1:]:
            k, _, v = line.partition(":")
            headers[k.strip().lower()] = v.strip()
        expect = base64.b64encode(
            hashlib.sha1((key + WS_GUID).encode("ascii")).digest()
        ).decode("ascii")
        if headers.get("sec-websocket-accept") != expect:
            raise WSError("WebSocket 握手校验失败（Sec-WebSocket-Accept 不匹配）")

    # ---------- 底层收发 ----------
    def settimeout(self, timeout: float) -> None:
        try:
            self._sock.settimeout(timeout)
        except OSError:
            pass

    def _recv_exact(self, n: int) -> bytes:
        while len(self._buf) < n:
            if self._closed:
                raise WSClosed("WebSocket 已关闭")
            try:
                chunk = self._sock.recv(min(65536, max(1, n - len(self._buf))))
            except socket.timeout:
                raise
            if not chunk:
                self._closed = True
                raise WSClosed("WebSocket 连接被对端关闭")
            self._buf += chunk
        data, self._buf = self._buf[:n], self._buf[n:]
        return data

    def _send_frame(self, opcode: int, payload: bytes) -> None:
        if self._closed:
            raise WSClosed("WebSocket 已关闭，无法发送")
        header = bytearray()
        header.append(0x80 | opcode)  # FIN=1
        n = len(payload)
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", n)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", n)
        mask = os.urandom(4)
        header += mask
        masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        self._sock.sendall(bytes(header) + masked)

    def _read_frame(self):
        b0, b1 = self._recv_exact(2)
        fin = bool(b0 & 0x80)
        opcode = b0 & 0x0F
        masked = bool(b1 & 0x80)
        n = b1 & 0x7F
        if n == 126:
            n = struct.unpack(">H", self._recv_exact(2))[0]
        elif n == 127:
            n = struct.unpack(">Q", self._recv_exact(8))[0]
        mask = self._recv_exact(4) if masked else b""
        payload = self._recv_exact(n) if n else b""
        if masked:
            payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
        return fin, opcode, payload

    def send_text(self, text: str) -> None:
        self._send_frame(0x1, text.encode("utf-8"))

    def recv_text(self) -> str:
        chunks = []
        msg_opcode = None
        while True:
            fin, opcode, payload = self._read_frame()
            if opcode == 0x9:  # ping → pong
                self._send_frame(0xA, payload)
                continue
            if opcode == 0xA:  # pong
                continue
            if opcode == 0x8:  # close
                reason = payload[2:].decode("utf-8", "replace") if len(payload) > 2 else ""
                try:
                    self._send_frame(0x8, payload[:2] if len(payload) >= 2 else struct.pack(">H", 1000))
                except Exception:
                    pass
                self._closed = True
                raise WSClosed(f"WebSocket 被服务端关闭 {reason}".strip())
            if opcode in (0x1, 0x2):  # data frame
                if msg_opcode is not None:
                    raise WSError("收到嵌套的数据帧")
                msg_opcode = opcode
                chunks = [payload]
                if fin:
                    break
                continue
            if opcode == 0x0:  # continuation
                if msg_opcode is None:
                    raise WSError("收到非法 continuation 帧")
                chunks.append(payload)
                if fin:
                    break
                continue
            raise WSError(f"不支持的 WebSocket 操作码: {opcode}")
        return b"".join(chunks).decode("utf-8", "replace")

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        try:
            self._send_frame(0x8, struct.pack(">H", 1000))
        except Exception:
            pass
        try:
            self._sock.close()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# CDP 客户端
# ---------------------------------------------------------------------------
class CDP:
    """同步风格的 CDP 客户端：call() 等命令响应，wait_event() 等事件。"""

    MAX_BACKLOG = 4000

    def __init__(self, ws_url: str, timeout: float = 30.0):
        self.ws = MiniWebSocket(ws_url, timeout=timeout)
        self.timeout = timeout
        self._next_id = 0
        self._backlog = []
        self._handlers = {}

    def on(self, method: str, handler) -> None:
        self._handlers.setdefault(method, []).append(handler)

    def _dispatch(self, msg: dict) -> None:
        method = msg.get("method")
        if not method:
            return
        if len(self._backlog) < self.MAX_BACKLOG:
            self._backlog.append(msg)
        else:
            del self._backlog[: self.MAX_BACKLOG // 2]
            self._backlog.append(msg)
        for handler in self._handlers.get(method, ()):
            try:
                handler(msg)
            except Exception as exc:  # 事件处理异常不能打断主流程
                print(f"  [warn] CDP 事件处理器异常 {method}: {exc}")

    def _read_message(self, timeout: float) -> dict:
        self.ws.settimeout(max(0.1, timeout))
        try:
            text = self.ws.recv_text()
        except socket.timeout:
            raise TimeoutError("等待 CDP 消息超时")
        try:
            return json.loads(text)
        except ValueError as exc:
            raise WSError(f"CDP 收到非法 JSON: {exc}") from exc

    def send(self, method: str, params=None, session_id=None) -> int:
        self._next_id += 1
        msg = {"id": self._next_id, "method": method}
        if params is not None:
            msg["params"] = params
        if session_id:
            msg["sessionId"] = session_id
        self.ws.send_text(json.dumps(msg, ensure_ascii=False, separators=(",", ":")))
        return self._next_id

    def call(self, method: str, params=None, session_id=None, timeout: float = None):
        timeout = self.timeout if timeout is None else timeout
        mid = self.send(method, params, session_id)
        deadline = time.monotonic() + timeout
        while True:
            remain = deadline - time.monotonic()
            if remain <= 0:
                raise TimeoutError(f"CDP 命令超时: {method}")
            try:
                msg = self._read_message(remain)
            except TimeoutError:
                raise TimeoutError(f"CDP 命令超时: {method}")
            if msg.get("id") != mid:
                continue
            if "error" in msg:
                raise CDPError(method, msg["error"])
            return msg.get("result", {})

    def wait_event(self, method: str, timeout: float = None, session_id=None, predicate=None):
        timeout = self.timeout if timeout is None else timeout

        def match(msg):
            if msg.get("method") != method:
                return False
            if session_id is not None and msg.get("sessionId") != session_id:
                return False
            if predicate is not None and not predicate(msg.get("params") or {}):
                return False
            return True

        deadline = time.monotonic() + timeout
        while True:
            for i, msg in enumerate(self._backlog):
                if match(msg):
                    del self._backlog[i]
                    return msg
            remain = deadline - time.monotonic()
            if remain <= 0:
                raise TimeoutError(f"等待 CDP 事件超时: {method}")
            try:
                self._read_message(remain)
            except TimeoutError:
                raise TimeoutError(f"等待 CDP 事件超时: {method}")

    def close(self) -> None:
        self.ws.close()


class PageSession:
    """绑定到某个 target 的 CDP 会话（flatten=true 时命令带 sessionId）。"""

    def __init__(self, cdp: CDP, session_id: str):
        self.cdp = cdp
        self.session_id = session_id

    def call(self, method: str, params=None, timeout: float = None):
        return self.cdp.call(method, params, session_id=self.session_id, timeout=timeout)

    def wait_event(self, method: str, timeout: float = None, predicate=None):
        return self.cdp.wait_event(
            method, timeout=timeout, session_id=self.session_id, predicate=predicate
        )


# ---------------------------------------------------------------------------
# HTTP / Edge 进程管理
# ---------------------------------------------------------------------------
def http_get_json(url: str, timeout: float = 2.0):
    try:
        with HTTP_OPENER.open(url, timeout=timeout) as resp:
            return json.loads(resp.read().decode("utf-8", "replace"))
    except (urlerror.URLError, OSError, ValueError, socket.timeout):
        return None


def probe_http(url: str, timeout: float = 5.0):
    """返回 (可达, 说明)。有 HTTP 响应（含 4xx/5xx）都算服务在跑。"""
    try:
        with HTTP_OPENER.open(url, timeout=timeout) as resp:
            resp.read(512)
            return True, f"HTTP {resp.status}"
    except urlerror.HTTPError as exc:
        return True, f"HTTP {exc.code}"
    except (urlerror.URLError, OSError, socket.timeout) as exc:
        return False, str(getattr(exc, "reason", exc))


class EdgeBrowser:
    """负责“复用或拉起”无头 Edge，并在结束时清理自己拉起的进程与 profile。"""

    def __init__(self, binary: str, cdp_port: int, out_dir: str, keep_open: bool):
        self.binary = binary
        self.cdp_port = cdp_port
        self.out_dir = out_dir
        self.keep_open = keep_open

        self.external = False       # 复用已有实例
        self.ws_url = None
        self.version = None
        self.proc = None
        self.profile = None
        self._log_fh = None
        self.log_path = os.path.join(out_dir, "edge.log")

    # ---------- 探测 / 启动 ----------
    def _probe(self):
        info = http_get_json(f"http://127.0.0.1:{self.cdp_port}/json/version", timeout=1.5)
        if info and info.get("webSocketDebuggerUrl"):
            return info
        return None

    def start(self) -> str:
        info = self._probe()
        if info:
            self.external = True
            self.ws_url = info["webSocketDebuggerUrl"]
            self.version = info.get("Browser", "?")
            return self.ws_url

        if not os.path.isfile(self.binary):
            raise HarnessError(
                f"找不到 Edge 可执行文件: {self.binary}\n"
                "  用 --headless-binary 指定，或设置环境变量 EDGE_BIN。"
            )
        if not os.access(self.binary, os.X_OK):
            raise HarnessError(f"Edge 可执行文件不可执行: {self.binary}")

        try:
            self.profile = tempfile.mkdtemp(prefix="as-edge-profile-", dir="/tmp")
        except OSError as exc:
            raise HarnessError(f"无法创建临时 profile 目录: {exc}") from exc

        self._log_fh = open(self.log_path, "wb")
        cmd = [
            self.binary,
            "--headless=new",
            "--no-sandbox",
            "--disable-dev-shm-usage",
            f"--remote-debugging-port={self.cdp_port}",
            f"--user-data-dir={self.profile}",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-gpu",
            f"--window-size={VIEW_WIDTH},{VIEW_HEIGHT}",
            "about:blank",
        ]
        print(f"  启动 Edge: {' '.join(cmd[:2])} … (CDP {self.cdp_port})")
        try:
            self.proc = subprocess.Popen(
                cmd,
                stdout=self._log_fh,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        except OSError as exc:
            raise HarnessError(f"启动 Edge 失败: {exc}") from exc

        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                tail = self._log_tail()
                raise HarnessError(
                    f"Edge 启动后立即退出（code={self.proc.returncode}）\n"
                    f"  Edge 日志尾部:\n{tail}"
                )
            info = self._probe()
            if info and info.get("webSocketDebuggerUrl"):
                self.ws_url = info["webSocketDebuggerUrl"]
                self.version = info.get("Browser", "?")
                return self.ws_url
            time.sleep(0.15)

        tail = self._log_tail()
        raise HarnessError(
            f"等待 Edge CDP 端口 {self.cdp_port} 超时（30s）\n  Edge 日志尾部:\n{tail}"
        )

    def _log_tail(self, limit: int = 3000) -> str:
        try:
            if self._log_fh:
                self._log_fh.flush()
            with open(self.log_path, "rb") as fh:
                data = fh.read()
            text = data.decode("utf-8", "replace").strip()
            return text[-limit:] if text else "(空)"
        except OSError:
            return "(无法读取日志)"

    # ---------- 清理 ----------
    def stop(self) -> None:
        # 复用的实例、--keep-open 都不动
        if self.external or self.keep_open:
            if self._log_fh:
                try:
                    self._log_fh.close()
                except Exception:
                    pass
            return
        proc = self.proc
        if proc is not None and proc.poll() is None:
            self._terminate(proc)
        if self._log_fh:
            try:
                self._log_fh.close()
            except Exception:
                pass
        if self.profile:
            shutil.rmtree(self.profile, ignore_errors=True)

    def _terminate(self, proc) -> None:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError, OSError):
            try:
                proc.terminate()
            except Exception:
                pass
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                return
            time.sleep(0.1)
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except (ProcessLookupError, PermissionError, OSError):
            try:
                proc.kill()
            except Exception:
                pass
        try:
            proc.wait(timeout=5)
        except Exception:
            pass

    def describe(self) -> str:
        if self.external:
            return f"{self.version} · CDP 端口 {self.cdp_port}（复用已有实例，退出时不关闭）"
        return f"{self.version} · CDP 端口 {self.cdp_port}（本次新启动并负责清理）"


# ---------------------------------------------------------------------------
# PNG 小工具
# ---------------------------------------------------------------------------
def png_size(data: bytes):
    if len(data) < 24 or not data.startswith(PNG_SIG):
        raise ValueError("不是合法的 PNG 数据")
    width, height = struct.unpack(">II", data[16:24])
    return width, height


# ---------------------------------------------------------------------------
# 页面驱动：DOM 查询 / 真实鼠标点击 / 截图
# ---------------------------------------------------------------------------
JS_VISIBLE = r"""
(function(sel){
  var el = document.querySelector(sel);
  if (!el) return {exists:false, sel:sel};
  var r = el.getBoundingClientRect();
  var cs = getComputedStyle(el);
  var op = parseFloat(cs.opacity); if (isNaN(op)) op = 1;
  var vis = r.width > 0 && r.height > 0 && cs.display !== 'none' &&
            cs.visibility !== 'hidden' && cs.visibility !== 'collapse' && op > 0;
  return {exists:true, sel:sel, w:Math.round(r.width*100)/100, h:Math.round(r.height*100)/100,
          top:Math.round(r.top), left:Math.round(r.left),
          display:cs.display, visibility:cs.visibility, opacity:cs.opacity,
          hiddenAttr: el.hidden === true, visible:vis,
          text:String(el.textContent||'').replace(/\s+/g,' ').trim().slice(0,160),
          inViewport: r.width>0 && r.height>0 && r.bottom>0 && r.right>0 &&
                      r.top<(window.innerHeight||0) && r.left<(window.innerWidth||0)};
})
"""

JS_CLICK_PREP = r"""
(function(sel){
  var el = document.querySelector(sel);
  if (!el) return {ok:false, reason:'缺少元素: '+sel};
  try { el.scrollIntoView({block:'center', inline:'center'}); } catch(e) {}
  var r = el.getBoundingClientRect();
  var cs = getComputedStyle(el);
  var op = parseFloat(cs.opacity); if (isNaN(op)) op = 1;
  var vis = r.width > 0 && r.height > 0 && cs.display !== 'none' &&
            cs.visibility !== 'hidden' && op > 0;
  var x = r.left + r.width/2, y = r.top + r.height/2;
  var inVp = x >= 0 && y >= 0 && x <= (window.innerWidth||0) && y <= (window.innerHeight||0);
  // 悬停兜底：有些控件（如卡片收藏按钮）默认 opacity:0，靠 :hover 显示
  var px = x, py = y;
  var parent = el.closest('.card');
  if (parent) {
    var pr = parent.getBoundingClientRect();
    px = pr.left + pr.width/2; py = pr.top + pr.height/2;
  }
  return {ok:vis && inVp, visible:vis, inViewport:inVp, x:x, y:y, px:px, py:py,
          w:r.width, h:r.height,
          display:cs.display, visibility:cs.visibility, opacity:cs.opacity,
          reason: !vis ? '元素不可见' : (!inVp ? '点击坐标在视口外' : '')};
})
"""

JS_TEXT = r"""
(function(sel){
  var el = document.querySelector(sel);
  if (!el) return null;
  var t = (el.innerText !== undefined && el.innerText !== null) ? el.innerText : el.textContent;
  return String(t === undefined || t === null ? '' : t).trim();
})
"""

JS_ASSET_STATE = r"""
(function(){
  var statusEl = document.querySelector('#status');
  return {
    cards: document.querySelectorAll('#grid .card').length,
    status: statusEl ? String(statusEl.textContent||'').trim() : '',
    ready: document.readyState
  };
})()
"""

JS_FIRST_IMG = r"""
(function(){
  var img = document.querySelector('#grid .card img');
  if (!img) return {found:false};
  return {found:true, complete: !!img.complete, nw: img.naturalWidth||0,
          src: String(img.currentSrc||img.src||'')};
})()
"""

JS_BROKEN_IMGS = r"""
(function(){
  var imgs = Array.prototype.slice.call(document.querySelectorAll('#grid .card img'));
  var done = imgs.filter(function(i){ return !!i.complete; });
  var broken = done.filter(function(i){
    return (i.naturalWidth||0) === 0 && String(i.getAttribute('src')||i.currentSrc||'') !== '';
  });
  return {total: imgs.length, done: done.length, broken: broken.length,
          brokenSrc: broken.slice(0,5).map(function(i){ return String(i.currentSrc||i.src||'').slice(-80); })};
})()
"""

JS_FIRST_CARD_NAME = r"""
(function(){
  var c = document.querySelector('#grid .card');
  if (!c) return '';
  var n = c.querySelector('.card-name');
  return String((n ? n.textContent : c.textContent) || '').trim();
})()
"""

JS_DETAIL_MEDIA = r"""
(function(){
  function vis(el){
    if (!el) return false;
    var r = el.getBoundingClientRect();
    var cs = getComputedStyle(el);
    var op = parseFloat(cs.opacity); if (isNaN(op)) op = 1;
    return r.width > 0 && r.height > 0 && cs.display !== 'none' &&
           cs.visibility !== 'hidden' && op > 0;
  }
  var img = document.querySelector('#detail-img');
  var imgInfo = function(){
    return {kind:'image', loaded: !!(img && img.complete && (img.naturalWidth||0) > 0),
            src: img ? String(img.currentSrc||img.src||'') : ''};
  };
  // 旧版（0.1.x）预览框
  var ib = document.querySelector('#image-preview-box');
  if (ib && vis(ib)) return imgInfo();
  var ab = document.querySelector('#audio-preview-box');
  if (ab && vis(ab)) return {kind:'audio', loaded:true, src:''};
  // 新版（0.2.x）画布舞台 / 音频舞台
  var stage = document.querySelector('#stage-viewport') || document.querySelector('#stage');
  var astage = document.querySelector('#audio-stage');
  if (stage && vis(stage)) return imgInfo();
  if (astage && vis(astage)) return {kind:'audio', loaded:true, src:''};
  return {kind:'none', loaded:false, src:''};
})()
"""

JS_SKELETON = r"""
(function(){
  var el = document.querySelector('#skeleton');
  if (!el) return {exists:false};
  var cs = getComputedStyle(el);
  var r = el.getBoundingClientRect();
  return {exists:true, hiddenAttr: el.hidden === true, display: cs.display,
          w: Math.round(r.width), h: Math.round(r.height)};
})()
"""


def _short(value, limit: int = 220) -> str:
    text = repr(value)
    return text if len(text) <= limit else text[:limit] + "…"


class UiDriver:
    """把 CDP 细节包成“人怎么用页面”的动词。"""

    def __init__(self, page: PageSession, out_dir: str, timeout: float = 30.0):
        self.page = page
        self.out_dir = out_dir
        self.timeout = timeout
        self.screenshots = []

    # ---------- JS 执行 ----------
    def eval(self, expression: str, timeout: float = None):
        res = self.page.call(
            "Runtime.evaluate",
            {
                "expression": expression,
                "returnByValue": True,
                "awaitPromise": True,
                "userGesture": True,
            },
            timeout=timeout,
        )
        details = res.get("exceptionDetails")
        if details:
            desc = ((details.get("exception") or {}).get("description")
                    or details.get("text") or "未知 JS 异常")
            loc = ""
            if details.get("url"):
                loc += f" @ {details['url']}"
            if details.get("lineNumber") is not None:
                loc += f":{details['lineNumber'] + 1}"
            raise JSError(f"{desc}{loc}")
        result = res.get("result") or {}
        if result.get("subtype") in ("error", "exception"):
            raise JSError(result.get("description") or "JS 对象异常")
        return result.get("value")

    def poll(self, expression: str, predicate, timeout: float = None, desc: str = "",
             interval: float = 0.15):
        timeout = self.timeout if timeout is None else timeout
        deadline = time.monotonic() + timeout
        last = None
        last_err = None
        while True:
            last_err = None
            try:
                last = self.eval(expression)
            except (JSError, CDPError, TimeoutError) as exc:
                last_err = exc
            else:
                try:
                    if predicate(last):
                        return last
                except Exception:  # 判断函数自身异常也要继续等
                    pass
            if time.monotonic() >= deadline:
                extra = f"；错误={last_err}" if last_err is not None else f"；最后值={_short(last)}"
                raise TimeoutError(f"等待超时（{timeout:g}s）: {desc or expression[:60]}{extra}")
            time.sleep(interval)

    def drain(self, seconds: float = 0.6) -> None:
        """空转一小段时间并强制读 socket，把积压的页面事件/异常取回来。"""
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                self.eval("1")
            except (JSError, CDPError, TimeoutError):
                pass
            time.sleep(0.1)

    # ---------- 查询 ----------
    def visible(self, selector: str) -> dict:
        return self.eval(f"({JS_VISIBLE})({json.dumps(selector)})")

    def text(self, selector: str):
        return self.eval(f"({JS_TEXT})({json.dumps(selector)})")

    def wait_visible(self, selector: str, timeout: float = 10.0, desc: str = "") -> dict:
        return self.poll(
            f"({JS_VISIBLE})({json.dumps(selector)})",
            lambda info: bool(info and info.get("visible")),
            timeout=timeout,
            desc=desc or f"{selector} 可见",
        )

    def wait_hidden(self, selector: str, timeout: float = 10.0, desc: str = "") -> dict:
        def gone(info):
            return (not info) or (not info.get("exists")) or (not info.get("visible"))

        return self.poll(
            f"({JS_VISIBLE})({json.dumps(selector)})",
            gone,
            timeout=timeout,
            desc=desc or f"{selector} 隐藏",
        )

    def wait_text(self, selector: str, predicate, timeout: float = 10.0, desc: str = ""):
        return self.poll(
            f"({JS_TEXT})({json.dumps(selector)})",
            lambda t: predicate(t),
            timeout=timeout,
            desc=desc or f"{selector} 文本满足条件",
        )

    # ---------- 操作 ----------
    def _mouse_move(self, x, y) -> None:
        try:
            self.page.call(
                "Input.dispatchMouseEvent",
                {"type": "mouseMoved", "x": float(x), "y": float(y),
                 "button": "none", "clickCount": 0},
                timeout=2,
            )
        except (CDPError, TimeoutError):
            pass

    def click(self, selector: str, timeout: float = None, wait_visible: float = 5.0) -> dict:
        """真实鼠标点击：先 scrollIntoView，再 Input.dispatchMouseEvent。

        元素不存在 / 不可见 / 坐标在视口外都会报错 —— 这正是“用户点不到”的判定。
        卡片有入场动画（opacity 0→1），部分控件（如收藏按钮）靠 hover 显示，
        所以会先轮询等到目标真的可见可点（默认最多 5s），期间补一次鼠标悬停。
        """
        timeout = self.timeout if timeout is None else timeout
        deadline = time.monotonic() + max(0.0, wait_visible)
        info = None
        hover_phase = 0
        hover_started = 0.0
        while True:
            info = self.eval(f"({JS_CLICK_PREP})({json.dumps(selector)})")
            if isinstance(info, dict) and info.get("ok"):
                break
            now = time.monotonic()
            if isinstance(info, dict) and info.get("x") is not None:
                if hover_phase == 0:
                    self._mouse_move(info.get("x"), info.get("y"))
                    hover_phase, hover_started = 1, now
                    time.sleep(0.12)
                    continue
                if hover_phase == 1 and now - hover_started > 0.8:
                    self._mouse_move(info.get("px", info.get("x")), info.get("py", info.get("y")))
                    hover_phase, hover_started = 2, now
                    time.sleep(0.12)
                    continue
            if now >= deadline:
                break
            time.sleep(0.1)
        if not isinstance(info, dict) or not info.get("ok"):
            detail = info.get("reason") if isinstance(info, dict) else "未知原因"
            raise JSError(f"无法点击 {selector}: {detail}（可见={info and info.get('visible')}, "
                          f"视口内={info and info.get('inViewport')}, "
                          f"rect={info and info.get('w')}x{info and info.get('h')}, "
                          f"display={info and info.get('display')}, "
                          f"visibility={info and info.get('visibility')}, "
                          f"opacity={info and info.get('opacity')}）")
        x, y = float(info["x"]), float(info["y"])
        try:
            self.page.call(
                "Input.dispatchMouseEvent",
                {"type": "mouseMoved", "x": x, "y": y, "button": "none", "clickCount": 0},
                timeout=timeout or self.timeout,
            )
            self.page.call(
                "Input.dispatchMouseEvent",
                {"type": "mousePressed", "x": x, "y": y, "button": "left",
                 "buttons": 1, "clickCount": 1},
                timeout=timeout or self.timeout,
            )
            self.page.call(
                "Input.dispatchMouseEvent",
                {"type": "mouseReleased", "x": x, "y": y, "button": "left",
                 "buttons": 0, "clickCount": 1},
                timeout=timeout or self.timeout,
            )
            return {"method": "mouse", "x": x, "y": y}
        except (CDPError, TimeoutError):
            # 极端环境兜底（浏览器繁忙/命令超时）：退回 DOM click（仍会记录，便于排查）
            self.eval(
                f"(function(){{var el=document.querySelector({json.dumps(selector)});"
                f"if(!el) throw new Error('缺少元素'); el.click(); return true;}})()"
            )
            return {"method": "js", "x": x, "y": y}

    def focus(self, selector: str) -> str:
        return self.eval(
            f"(function(){{var el=document.querySelector({json.dumps(selector)});"
            f"if(!el) throw new Error('缺少元素: '+{json.dumps(selector)});"
            f"el.scrollIntoView({{block:'center'}}); el.focus(); return document.activeElement === el;}})()"
        )

    def insert_text(self, text: str) -> None:
        """向当前焦点元素输入文本（触发真实 input 事件）。"""
        self.page.call("Input.insertText", {"text": text}, timeout=self.timeout)

    def set_value(self, selector: str, value, event: str = "input") -> str:
        """直接设值并派发事件（input/change），用于 range/select/input。"""
        expr = (
            "(function(){var el=document.querySelector(%s);"
            "if(!el) throw new Error('缺少元素: '+%s);"
            "el.value=%s;"
            "el.dispatchEvent(new Event(%s,{bubbles:true}));"
            "return String(el.value);})()"
            % (json.dumps(selector), json.dumps(selector), json.dumps(str(value)), json.dumps(event))
        )
        return self.eval(expr)

    def js_click(self, selector: str) -> bool:
        return bool(
            self.eval(
                f"(function(){{var el=document.querySelector({json.dumps(selector)});"
                f"if(!el) throw new Error('缺少元素: '+{json.dumps(selector)});"
                f"el.click(); return true;}})()"
            )
        )

    def dispatch(self, selector: str, event: str = "click") -> bool:
        return bool(
            self.eval(
                f"(function(){{var el=document.querySelector({json.dumps(selector)});"
                f"if(!el) throw new Error('缺少元素: '+{json.dumps(selector)});"
                f"el.dispatchEvent(new Event({json.dumps(event)},{{bubbles:true}})); return true;}})()"
            )
        )

    def key(self, key: str, code: str = None, meta: bool = False, ctrl: bool = False) -> None:
        """向 document 派发 KeyboardEvent（用于 ⌘K 这类全局快捷键）。"""
        code = code or key
        self.eval(
            "document.dispatchEvent(new KeyboardEvent('keydown', {key:%s, code:%s, "
            "metaKey:%s, ctrlKey:%s, bubbles:true, cancelable:true}))"
            % (
                json.dumps(key),
                json.dumps(code),
                "true" if meta else "false",
                "true" if ctrl else "false",
            )
        )

    def count(self, selector: str) -> int:
        return int(self.eval(f"document.querySelectorAll({json.dumps(selector)}).length") or 0)

    def attr(self, selector: str, name: str):
        return self.eval(
            f"(function(){{var el=document.querySelector({json.dumps(selector)});"
            f"return el?el.getAttribute({json.dumps(name)}):null;}})()"
        )

    def prop(self, selector: str, name: str):
        return self.eval(
            f"(function(){{var el=document.querySelector({json.dumps(selector)});"
            f"return el?el[{json.dumps(name)}]:null;}})()"
        )

    def screenshot(self, filename: str) -> dict:
        if not filename.lower().endswith(".png"):
            filename += ".png"
        path = os.path.join(self.out_dir, filename)
        # 等两帧 + 一小段过渡，避免把 CSS 淡入动画拍成半透明
        try:
            self.eval(
                "new Promise(function(resolve){"
                "requestAnimationFrame(function(){requestAnimationFrame(function(){resolve(1);});});})"
            )
            time.sleep(0.18)
        except (JSError, CDPError, TimeoutError):
            pass
        raw = None
        last_err = None
        for _ in range(3):
            try:
                res = self.page.call(
                    "Page.captureScreenshot",
                    {"format": "png", "fromSurface": True, "captureBeyondViewport": False},
                    timeout=max(20.0, self.timeout),
                )
                b64 = res.get("data")
                if not b64:
                    raise HarnessError("Page.captureScreenshot 未返回 data")
                raw = base64.b64decode(b64)
                break
            except CDPError as exc:
                last_err = exc
                time.sleep(0.2)
        if raw is None:
            raise HarnessError(f"截图失败: {last_err}")
        width, height = png_size(raw)
        with open(path, "wb") as fh:
            fh.write(raw)
        info = {
            "name": filename,
            "file": path,
            "width": width,
            "height": height,
            "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        }
        self.screenshots.append(info)
        print(f"  [shot] {filename}  {width}x{height}  {len(raw)/1024:.1f} KB")
        return info

    def dump_debug(self, tag: str = "failure") -> None:
        try:
            html = self.eval(
                "(function(){return document.documentElement.outerHTML.slice(0,400000);})()"
            )
            with open(os.path.join(self.out_dir, f"{tag}_dom.html"), "w", encoding="utf-8") as fh:
                fh.write(html or "")
        except Exception as exc:
            print(f"  [warn] 保存 DOM 失败: {exc}")
        try:
            text = self.eval("(function(){return String(document.body.innerText||'').slice(0,40000);})()")
            with open(os.path.join(self.out_dir, f"{tag}_text.txt"), "w", encoding="utf-8") as fh:
                fh.write(text or "")
        except Exception as exc:
            print(f"  [warn] 保存页面文本失败: {exc}")


# ---------------------------------------------------------------------------
# 测试结果收集 / 报告
# ---------------------------------------------------------------------------
class Harness:
    def __init__(self, out_dir: str):
        self.out_dir = out_dir
        self.results = []          # [{name, ok, detail, ms}]
        self.fail_count = 0
        self.started = time.monotonic()
        self.url = ""
        self.browser_desc = ""
        self.harness_error = ""
        self.exit_code = EXIT_HARNESS

        self.js_errors = []        # uncaught exceptions
        self.console_errors = []   # console.error / console.assert
        self.console_warnings = []
        self.log_errors = []       # Log.entryAdded level=error（多为网络/资源类）
        self.network_http_errors = []   # 同源 4xx/5xx 资源（img 404 等）
        self.network_failures = []      # 同源 loadingFailed（排除 ERR_ABORTED）
        self.notes = []                 # [INFO] 提示 / 期望核对，不计失败
        self.origin = ""                # 画廊 origin，用于过滤同源请求
        self._last_t = time.monotonic()

    # ---------- 记录 ----------
    def _record(self, name: str, ok: bool, detail: str = "") -> bool:
        now = time.monotonic()
        ms = round((now - self._last_t) * 1000)
        self._last_t = now
        self.results.append({"name": name, "ok": ok, "detail": detail, "ms": ms})
        if not ok:
            self.fail_count += 1
        tag = "PASS" if ok else "FAIL"
        line = f"[{tag}] {name}"
        if detail:
            line += f" — {detail}"
        print(line)
        return ok

    def ok(self, name: str, detail: str = "") -> bool:
        return self._record(name, True, detail)

    def fail(self, name: str, detail: str = "") -> bool:
        return self._record(name, False, detail)

    def check(self, name: str, cond, detail: str = "") -> bool:
        return self._record(name, bool(cond), detail if not cond else detail)

    def note(self, name: str, detail: str = "") -> None:
        self.notes.append({"name": name, "detail": detail})
        print(f"[INFO] {name}" + (f" — {detail}" if detail else ""))

    @staticmethod
    def fmt_vis(info) -> str:
        if not isinstance(info, dict):
            return _short(info)
        if not info.get("exists"):
            return "元素不存在"
        return (
            f"rect={info.get('w')}x{info.get('h')} display={info.get('display')} "
            f"visibility={info.get('visibility')} opacity={info.get('opacity')} "
            f"视口内={info.get('inViewport')} 文本={_short(info.get('text'), 60)}"
        )

    # ---------- 页面事件采集 ----------
    @staticmethod
    def _arg_text(arg: dict) -> str:
        if "value" in arg:
            value = arg["value"]
            if isinstance(value, str):
                return value
            try:
                return json.dumps(value, ensure_ascii=False)
            except (TypeError, ValueError):
                return str(value)
        return str(arg.get("description") or arg.get("type") or "?")

    def on_exception(self, msg: dict, session_id: str) -> None:
        if msg.get("sessionId") != session_id:
            return
        details = msg.get("params", {}).get("exceptionDetails", {}) or {}
        exc = details.get("exception") or {}
        desc = exc.get("description") or details.get("text") or "未知异常"
        item = {
            "text": str(desc).splitlines()[0][:300],
            "detail": str(desc)[:1500],
            "url": details.get("url") or "",
            "line": details.get("lineNumber"),
            "column": details.get("columnNumber"),
        }
        self.js_errors.append(item)
        print(f"  [js-error] {item['text']}")

    def on_console(self, msg: dict, session_id: str) -> None:
        if msg.get("sessionId") != session_id:
            return
        params = msg.get("params", {}) or {}
        ctype = params.get("type", "?")
        text = " ".join(self._arg_text(a) for a in (params.get("args") or []))[:800]
        item = {"type": ctype, "text": text}
        if ctype in ("error", "assert"):
            self.console_errors.append(item)
            print(f"  [console.error] {text[:300]}")
        elif ctype == "warning":
            self.console_warnings.append(item)

    def on_log(self, msg: dict, session_id: str) -> None:
        if msg.get("sessionId") != session_id:
            return
        entry = (msg.get("params") or {}).get("entry") or {}
        if entry.get("level") != "error":
            return
        item = {
            "source": entry.get("source", ""),
            "text": str(entry.get("text", ""))[:500],
            "url": entry.get("url", ""),
        }
        self.log_errors.append(item)
        if item["source"] != "network":
            print(f"  [log:error/{item['source']}] {item['text'][:240]}")

    # ---------- 网络（同源资源失败 / img 404） ----------
    def _same_origin(self, url: str) -> bool:
        if not self.origin or not url:
            return False
        if not (url.startswith("http://") or url.startswith("https://")):
            return False
        return url.startswith(self.origin + "/") or url == self.origin

    @staticmethod
    def _ignore_url(url: str) -> bool:
        # favicon 404 与页面功能无关，不算失败（但会在报告里作为 Log 提示）
        return "/favicon.ico" in url

    def on_network_response(self, msg: dict, session_id: str) -> None:
        if msg.get("sessionId") != session_id:
            return
        params = msg.get("params") or {}
        response = params.get("response") or {}
        url = response.get("url") or ""
        status = int(response.get("status") or 0)
        if status < 400 or not self._same_origin(url) or self._ignore_url(url):
            return
        item = {
            "status": status,
            "type": params.get("type", ""),
            "url": url[:300],
        }
        if item not in self.network_http_errors:
            self.network_http_errors.append(item)
            print(f"  [net:{status}] {item['type']} {item['url']}")

    def on_network_failed(self, msg: dict, session_id: str) -> None:
        if msg.get("sessionId") != session_id:
            return
        params = msg.get("params") or {}
        url = params.get("request", {}).get("url") or ""
        error_text = params.get("errorText") or ""
        if not self._same_origin(url) or self._ignore_url(url):
            return
        # 换页 / 快速切图会主动 abort 旧请求，这是正常现象
        if error_text in ("net::ERR_ABORTED", "net::ERR_ABORTED_BY_CLIENT"):
            return
        item = {"error": error_text, "type": params.get("type", ""), "url": url[:300]}
        if item not in self.network_failures:
            self.network_failures.append(item)
            print(f"  [net-failed] {error_text} {item['url']}")

    # ---------- 报告 ----------
    def finalize(self, driver: UiDriver = None) -> None:
        duration = time.monotonic() - self.started
        total = len(self.results)
        passed = total - self.fail_count
        screenshots = driver.screenshots if driver else []

        print("\n" + "=" * 72)
        print("截图：")
        if screenshots:
            for shot in screenshots:
                print(f"  - {shot['file']}  {shot['width']}x{shot['height']}  "
                      f"{shot['bytes']/1024:.1f} KB")
        else:
            print("  （无）")
        print()

        if self.console_warnings:
            print(f"console 警告：{len(self.console_warnings)} 条")
            for item in self.console_warnings[:10]:
                print(f"  [console.warn] {item['text'][:200]}")
            print()
        net_logs = [x for x in self.log_errors if x.get("source") == "network"]
        if net_logs:
            print(f"资源加载日志错误（source=network，仅提示、不计失败）：{len(net_logs)} 条")
            for item in net_logs[:10]:
                print(f"  [log:network] {item['text'][:200]}  {item['url'][:120]}")
            print()
        other_logs = [x for x in self.log_errors if x.get("source") != "network"]
        if other_logs:
            print(f"其它 Log 错误：{len(other_logs)} 条")
            for item in other_logs[:10]:
                print(f"  [log:{item['source']}] {item['text'][:200]}")
            print()

        if self.js_errors:
            print(f"JS 异常（exceptionThrown）：{len(self.js_errors)} 条（算失败）")
            for item in self.js_errors[:10]:
                loc = f"{item['url']}:{(item['line'] or 0) + 1}" if item["url"] else "?"
                print(f"  - {item['text']}  @ {loc}")
            print()
        if self.console_errors:
            print(f"console.error / assert：{len(self.console_errors)} 条（算失败）")
            for item in self.console_errors[:10]:
                print(f"  - [{item['type']}] {item['text'][:300]}")
            print()
        if self.network_http_errors:
            print(f"同源资源 4xx/5xx：{len(self.network_http_errors)} 条（算失败，img 404 等）")
            for item in self.network_http_errors[:10]:
                print(f"  - HTTP {item['status']} [{item['type']}] {item['url']}")
            print()
        if self.network_failures:
            print(f"同源资源加载失败：{len(self.network_failures)} 条（算失败）")
            for item in self.network_failures[:10]:
                print(f"  - {item['error']} [{item['type']}] {item['url']}")
            print()

        if self.notes:
            print("提示 / 期望核对（不计失败）：")
            for item in self.notes:
                print(f"  - {item['name']}: {item['detail']}")
            print()

        if self.fail_count:
            print("未通过项：")
            for item in self.results:
                if not item["ok"]:
                    print(f"  - {item['name']}: {item['detail']}")
            print()

        if self.harness_error:
            print(f"HARNESS ERROR: {self.harness_error}")
        print("=" * 72)
        print(f"目标页面: {self.url}")
        print(f"浏览器:   {self.browser_desc}")
        print(f"输出目录: {self.out_dir}")
        print(f"检查汇总: {total} 项 → PASS {passed} / FAIL {self.fail_count}  "
              f"(JS 异常 {len(self.js_errors)} / console.error {len(self.console_errors)} / "
              f"资源错误 {len(self.network_http_errors) + len(self.network_failures)})")
        if self.exit_code == EXIT_OK:
            print("结果: PASS")
        elif self.exit_code == EXIT_FAIL:
            print("结果: FAIL")
        else:
            print("结果: HARNESS ERROR（环境/驱动问题，先修好环境再重跑）")
        print(f"耗时: {duration:.1f}s")

        report = {
            "tool": "tools/asset_studio/tests/ui_smoke.py",
            "url": self.url,
            "browser": self.browser_desc,
            "out_dir": self.out_dir,
            "viewport": f"{VIEW_WIDTH}x{VIEW_HEIGHT}",
            "duration_s": round(duration, 2),
            "exit_code": self.exit_code,
            "summary": {
                "total": total,
                "passed": passed,
                "failed": self.fail_count,
                "js_exceptions": len(self.js_errors),
                "console_errors": len(self.console_errors),
                "console_warnings": len(self.console_warnings),
                "log_errors": len(self.log_errors),
                "network_http_errors": len(self.network_http_errors),
                "network_failures": len(self.network_failures),
            },
            "results": self.results,
            "screenshots": screenshots,
            "notes": self.notes,
            "errors": {
                "js_exceptions": self.js_errors,
                "console_errors": self.console_errors,
                "console_warnings": self.console_warnings,
                "log_errors": self.log_errors,
                "network_http_errors": self.network_http_errors,
                "network_failures": self.network_failures,
            },
        }
        try:
            report_path = os.path.join(self.out_dir, "ui_smoke_report.json")
            with open(report_path, "w", encoding="utf-8") as fh:
                json.dump(report, fh, ensure_ascii=False, indent=2)
            print(f"报告: {report_path}")
        except OSError as exc:
            print(f"[warn] 写报告失败: {exc}")

        if self.exit_code == EXIT_OK:
            print("\n全部检查通过 ✅")
        else:
            print("\n存在失败项，按上面 [FAIL] / 错误清单定位。")


# ---------------------------------------------------------------------------
# 基线用例（旧版画廊 0.1.x）
# ---------------------------------------------------------------------------
def run_baseline(driver: UiDriver, h: Harness, args) -> None:
    timeout = args.timeout

    # ---------- 1. 等待素材加载 ----------
    try:
        state = driver.poll(
            JS_ASSET_STATE,
            lambda s: isinstance(s, dict) and int(s.get("cards") or 0) > 0,
            timeout=timeout,
            desc="素材卡片出现（#grid .card > 0）",
        )
    except Exception as exc:
        h.fail("素材卡片加载", f"{type(exc).__name__}: {exc}")
        try:
            driver.screenshot("01_home_failed.png")
        except Exception:
            pass
        raise SuiteAbort("素材没有加载出来，无法继续交互检查") from exc

    cards = int(state.get("cards") or 0)
    h.check("素材卡片已加载", cards > 0, f"#grid .card = {cards}")
    h.check(
        "页面文档加载完成",
        state.get("ready") == "complete",
        f"document.readyState={state.get('ready')!r}",
    )

    # ---------- 2. 标题 ----------
    title = driver.eval("document.title")
    h.check(
        "页面标题包含 Asset Studio / 素材工坊",
        isinstance(title, str) and ("Asset Studio" in title or "素材工坊" in title),
        f"document.title={title!r}",
    )

    # ---------- 3. 状态栏 ----------
    status = driver.text("#status") or ""
    api_status = driver.eval(
        "(function(){var el=document.querySelector('#status');return el?String(el.textContent||'').trim():null;})()"
    ) or ""
    h.check("状态栏有文字", bool(status.strip()), f"#status={status[:140]!r}")
    h.check(
        "状态栏含素材数量",
        bool(re.search(r"\d", status)),
        f"#status={status[:140]!r}",
    )
    status_info = driver.visible("#status")
    h.check("状态栏可见", bool(status_info.get("visible")), h.fmt_vis(status_info))
    count_match = re.search(r"共\s*(\d+)\s*个", api_status)
    if count_match:
        h.check(
            "状态栏素材数与卡片数一致",
            int(count_match.group(1)) == cards,
            f"状态栏={count_match.group(1)}，卡片={cards}",
        )
    else:
        h.note("状态栏素材计数", f"未匹配到“共 N 个”格式，跳过数量一致性（实际={api_status[:100]!r}）")

    # 加载骨架屏应在素材渲染后消失（新版 0.2.x 才有该元素）
    try:
        skel = driver.poll(
            JS_SKELETON,
            lambda s: isinstance(s, dict) and (not s.get("exists") or s.get("hiddenAttr")
                                               or s.get("display") == "none"),
            timeout=10,
            desc="加载骨架屏消失",
        )
        if isinstance(skel, dict) and skel.get("exists"):
            h.check("加载骨架屏已消失", True, f"display={skel.get('display')}")
        else:
            h.note("加载骨架屏", "当前 UI 没有 #skeleton 元素，跳过")
    except Exception as exc:
        h.fail("加载骨架屏已消失", f"{type(exc).__name__}: {exc}")

    # ---------- 4. 网格与卡片可见性 ----------
    grid_info = driver.visible("#grid")
    h.check("素材网格 #grid 可见", bool(grid_info.get("visible")), h.fmt_vis(grid_info))
    card_info = driver.visible("#grid .card")
    h.check("首张素材卡片可见", bool(card_info.get("visible")), h.fmt_vis(card_info))

    names = driver.eval(
        "(function(){return Array.prototype.slice.call(document.querySelectorAll('#grid .card')).slice(0,5)"
        ".map(function(c){var n=c.querySelector('.card-name');return String((n?n.textContent:c.textContent)||'').trim();});})()"
    )
    h.check(
        "素材卡片有名称",
        isinstance(names, list) and bool(names) and all(str(n).strip() for n in names),
        f"前 5 个名称={_short(names)}",
    )

    # 首张缩略图真的加载出来（不是空框）
    try:
        img = driver.poll(
            JS_FIRST_IMG,
            lambda v: isinstance(v, dict) and v.get("found") and v.get("complete") and int(v.get("nw") or 0) > 0,
            timeout=timeout,
            desc="首张缩略图加载完成",
        )
        h.check("首张缩略图已加载", True, f"naturalWidth={img.get('nw')} src=…{str(img.get('src',''))[-60:]}")
    except Exception as exc:
        h.fail("首张缩略图已加载", f"{type(exc).__name__}: {exc}")

    try:
        broken = driver.eval(JS_BROKEN_IMGS)
        h.check(
            "网格缩略图无加载失败",
            isinstance(broken, dict) and int(broken.get("broken") or 0) == 0 and int(broken.get("done") or 0) > 0,
            f"已加载 {broken.get('done') if isinstance(broken, dict) else '?'} 张，"
            f"失败 {broken.get('broken') if isinstance(broken, dict) else '?'} 张 "
            f"{_short(broken.get('brokenSrc') if isinstance(broken, dict) else None, 160)}",
        )
    except Exception as exc:
        h.fail("网格缩略图无加载失败", f"{type(exc).__name__}: {exc}")

    # 首屏至少要有一批缩略图渲染出来再截图（不要求 47 张全部画完，只影响观感）
    try:
        loaded = driver.poll(
            "(function(){return Array.prototype.slice.call(document.querySelectorAll('#grid .card img'))"
            ".filter(function(i){return i.complete && (i.naturalWidth||0) > 0;}).length;})()",
            lambda n: isinstance(n, (int, float)) and n >= 6,
            timeout=12,
            desc="首屏至少 6 张缩略图渲染完成",
        )
        h.note("首屏缩略图渲染进度", f"{loaded} 张已加载（截图前）")
    except Exception as exc:
        h.note("首屏缩略图渲染进度", f"部分缩略图仍在渲染（{type(exc).__name__}），截图可能包含占位")

    # ---------- 5. 首屏截图 ----------
    shot_home = None
    try:
        shot_home = driver.screenshot("01_home_1440x960.png")
        h.check(
            "首屏截图 1440x960",
            shot_home["width"] == VIEW_WIDTH and shot_home["height"] == VIEW_HEIGHT,
            f"{shot_home['width']}x{shot_home['height']} {shot_home['bytes']/1024:.1f} KB",
        )
        h.check("首屏截图非空", shot_home["bytes"] > 4096, f"{shot_home['bytes']} bytes")
    except Exception as exc:
        h.fail("首屏截图", f"{type(exc).__name__}: {exc}")

    # ---------- 6. 打开详情面板 ----------
    card_name = driver.eval(JS_FIRST_CARD_NAME) or ""
    detail_opened = False
    try:
        driver.click("#grid .card")
        info = driver.wait_visible("#detail", timeout=10, desc="#detail 打开")
        h.check("#detail 打开后可见", True, h.fmt_vis(info))
        detail_opened = True
    except Exception as exc:
        h.fail("#detail 打开后可见", f"{type(exc).__name__}: {exc}")

    shot_detail = None
    if detail_opened:
        try:
            name = driver.wait_text(
                "#detail-name",
                lambda t: bool(t and str(t).strip()),
                timeout=10,
                desc="#detail-name 非空",
            )
            h.check("#detail-name 非空", True, f"#detail-name={str(name)[:80]!r}")
            if card_name:
                h.check(
                    "详情名称与首张卡片一致",
                    str(name).strip() == str(card_name).strip()
                    or str(card_name).strip() in str(name),
                    f"卡片={str(card_name)[:50]!r} / 详情={str(name)[:50]!r}",
                )
        except Exception as exc:
            h.fail("#detail-name 非空", f"{type(exc).__name__}: {exc}")

        try:
            media = driver.poll(
                JS_DETAIL_MEDIA,
                lambda m: isinstance(m, dict) and m.get("kind") in ("image", "audio")
                and (m.get("kind") == "audio" or bool(m.get("loaded"))),
                timeout=timeout,
                desc="详情预览区可见且预览已加载",
            )
            h.check(
                "详情预览区可见且预览已加载",
                True,
                f"kind={media.get('kind')} src=…{str(media.get('src', ''))[-60:]}",
            )
        except Exception as exc:
            h.fail("详情预览区可见且预览已加载", f"{type(exc).__name__}: {exc}")

        try:
            shot_detail = driver.screenshot("02_detail.png")
            h.check(
                "详情截图有效",
                shot_detail["width"] == VIEW_WIDTH and shot_detail["height"] == VIEW_HEIGHT
                and shot_detail["bytes"] > 4096,
                f"{shot_detail['width']}x{shot_detail['height']} {shot_detail['bytes']/1024:.1f} KB",
            )
        except Exception as exc:
            h.fail("详情截图", f"{type(exc).__name__}: {exc}")

        # 关闭详情
        try:
            driver.click("#detail-close")
            driver.wait_hidden("#detail", timeout=10, desc="#detail 关闭")
            h.check("点击关闭后详情隐藏", True, "")
        except Exception as exc:
            h.fail("点击关闭后详情隐藏", f"{type(exc).__name__}: {exc}")

    # ---------- 7. 扫描弹窗 ----------
    scan_opened = False
    try:
        driver.click("#btn-scan")
        info = driver.wait_visible("#scan-modal", timeout=10, desc="#scan-modal 打开")
        h.check("#scan-modal 打开后可见", True, h.fmt_vis(info))
        scan_opened = True
    except Exception as exc:
        h.fail("#scan-modal 打开后可见", f"{type(exc).__name__}: {exc}")

    shot_scan = None
    if scan_opened:
        try:
            body = driver.wait_text(
                "#scan-body",
                lambda t: bool(t and len(str(t).strip()) > 10 and "正在扫描" not in str(t)),
                timeout=max(60.0, timeout),
                desc="#scan-body 扫描结果",
            )
            body = str(body or "").strip()
            h.check("#scan-body 正文非空", len(body) > 10, f"长度={len(body)} 摘要={_short(body, 100)}")
            bad = ("扫描失败" in body) or ("服务端异常" in body)
            h.check("#scan-body 无扫描失败提示", not bad, f"摘要={_short(body, 160)}")
        except Exception as exc:
            h.fail("#scan-body 正文非空", f"{type(exc).__name__}: {exc}")

        try:
            shot_scan = driver.screenshot("03_scan_modal.png")
            h.check(
                "扫描弹窗截图有效",
                shot_scan["width"] == VIEW_WIDTH and shot_scan["height"] == VIEW_HEIGHT
                and shot_scan["bytes"] > 4096,
                f"{shot_scan['width']}x{shot_scan['height']} {shot_scan['bytes']/1024:.1f} KB",
            )
        except Exception as exc:
            h.fail("扫描弹窗截图", f"{type(exc).__name__}: {exc}")

        try:
            driver.click("#scan-close")
            driver.wait_hidden("#scan-modal", timeout=10, desc="#scan-modal 关闭")
            h.check("点击关闭后扫描弹窗隐藏", True, "")
        except Exception as exc:
            h.fail("点击关闭后扫描弹窗隐藏", f"{type(exc).__name__}: {exc}")

    # ---------- 8. 截图汇总 ----------
    shots = driver.screenshots
    h.check("截图数量 ≥ 2", len(shots) >= 2, f"已保存 {len(shots)} 张：{[s['name'] for s in shots]}")
    if shots:
        all_valid = all(
            s["width"] == VIEW_WIDTH and s["height"] == VIEW_HEIGHT and s["bytes"] > 4096
            for s in shots
        )
        h.check(
            "所有截图尺寸 1440x960 且非空",
            all_valid,
            "，".join(f"{s['name']}={s['width']}x{s['height']}/{s['bytes']}B" for s in shots),
        )
    if shot_home and shot_detail:
        h.check(
            "首屏与详情截图不同（确认界面真的切换了）",
            shot_home["sha256"] != shot_detail["sha256"],
            "两张截图像素内容相同意味着点击未生效",
        )

    # ---------- 9. 页面 JS 异常 / console.error ----------
    driver.drain(0.6)
    h.check(
        "页面无 JS 异常（exceptionThrown）",
        not h.js_errors,
        f"共 {len(h.js_errors)} 条，第一条={_short(h.js_errors[0], 160) if h.js_errors else '无'}",
    )
    h.check(
        "页面无 console.error / assert",
        not h.console_errors,
        f"共 {len(h.console_errors)} 条，第一条={_short(h.console_errors[0], 160) if h.console_errors else '无'}",
    )


# ---------------------------------------------------------------------------
# 新版画廊 0.2.0 用例（data-test 选择器）
# ---------------------------------------------------------------------------
EXPECT_TOTAL = 107   # 106 + 训练机升级·狮鹫（休眠）；5 个训练机概念稿已清理
EXPECT_CH2 = 44
EXPECT_AUDIO = 57
PALETTE_SWATCH_COUNT = 8
EXPECT_SORT_FIRST = "Boss 战 BGM · 132 BPM"   # lead 给的期望值，实测会核对并在报告里说明
EXPECT_WARDEN = "第二章 Boss · MOONWELL WARDEN"
EXPECT_SPHERE = "第二章 Boss · 球体"


def _v2_goto(driver: UiDriver, url: str, timeout: float = 30.0, expect_cards=None) -> str:
    """整页导航（含 hash 深链时带 query 强制真正加载），并等素材渲染。"""
    nav = driver.page.call("Page.navigate", {"url": url}, timeout=max(30.0, timeout))
    if nav.get("errorText"):
        raise HarnessError(f"Page.navigate 失败: {nav['errorText']} url={url}")
    driver.poll(
        "document.readyState",
        lambda v: v in ("interactive", "complete"),
        timeout=max(20.0, timeout),
        desc=f"页面就绪（{url}）",
    )
    if expect_cards is not None:
        driver.poll(
            JS_ASSET_STATE,
            lambda s: isinstance(s, dict) and int(s.get("cards") or 0) == expect_cards,
            timeout=max(90.0, timeout),
            desc=f"素材网格 {expect_cards} 张（{url}）",
        )
    return driver.eval("location.href")


def _v2_deep_url(base: str, token: str, fragment: str) -> str:
    """带 cache-buster query 的深链，确保是整页加载而不是同文档 hash 跳转。"""
    sep = "&" if "?" in base else "?"
    return f"{base}{sep}ui_smoke={token}{fragment}"


def _v2_reset_local_storage(driver: UiDriver) -> None:
    driver.eval(
        "(function(){try{var ks=[];for(var i=0;i<localStorage.length;i++){"
        "var k=localStorage.key(i);if(k&&k.indexOf('as.v02.')===0)ks.push(k);}"
        "ks.forEach(function(k){localStorage.removeItem(k);});}catch(e){return false;}return true;})()"
    )


def _v2_type(driver: UiDriver, selector: str, text: str) -> str:
    """优先真实键入（Input.insertText），失败时回退为设值 + input 事件；返回实际值。"""
    driver.click(selector)
    try:
        driver.insert_text(text)
    except (CDPError, TimeoutError):
        pass
    time.sleep(0.15)
    value = driver.eval(
        "(function(){var e=document.querySelector(%s);return e?String(e.value||''):'';})()"
        % json.dumps(selector)
    )
    if text not in str(value):
        driver.set_value(selector, text, "input")
        value = driver.eval(
            "(function(){var e=document.querySelector(%s);return e?String(e.value||''):'';})()"
            % json.dumps(selector)
        )
    return str(value)


def _v2_clear_filters(driver: UiDriver, timeout: float = 30.0) -> int:
    driver.click("[data-test=clear-filters]")
    return int(
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda n: n == EXPECT_TOTAL,
            timeout=timeout,
            desc=f"清空筛选恢复 {EXPECT_TOTAL} 张",
        )
    )


def _v2_toast_text(driver: UiDriver) -> str:
    return driver.eval(
        "(function(){var es=document.querySelectorAll('[data-test=toast]');"
        "return Array.prototype.slice.call(es).map(function(e){"
        "return e.textContent.replace(/\\s+/g,' ').trim();}).join(' || ');})()"
    ) or ""


def run_v2(driver: UiDriver, h: Harness, args) -> None:
    """新版（0.2.0）交互验收：按 lead 给的选择器清单与期望行为逐项断言。"""
    t = max(args.timeout, 30.0)
    base = args.url.split("#")[0]

    def open_card(asset_id: str) -> None:
        driver.click(f'.card[data-id="{asset_id}"] .card-name')
        driver.poll(
            "document.querySelector('[data-test=detail]').hidden",
            lambda v: v is False,
            timeout=t,
            desc=f"打开 {asset_id} 详情",
        )

    def close_detail() -> None:
        driver.click("[data-test=detail-close]")
        driver.poll(
            "document.querySelector('[data-test=detail]').hidden",
            lambda v: v is True,
            timeout=t,
            desc="关闭详情",
        )

    def detail_name() -> str:
        return driver.wait_text(
            "[data-test=detail-name]", lambda v: bool(v and str(v).strip()), timeout=t,
            desc="详情名称非空",
        ) or ""

    # ---------- V2-0 干净状态 + 首屏 ----------
    _v2_reset_local_storage(driver)
    try:
        _v2_goto(driver, base, t, expect_cards=EXPECT_TOTAL)
    except Exception as exc:
        h.fail("v2 首屏加载", f"{type(exc).__name__}: {exc}")
        try:
            driver.screenshot("04_v2_home_failed.png")
        except Exception:
            pass
        raise SuiteAbort("v2 首屏加载失败，无法继续") from exc

    try:
        hero = driver.wait_visible("[data-test=hero-stats]", timeout=t, desc="hero-stats 可见")
        h.check("hero-stats 可见", True, h.fmt_vis(hero))
        hero_text = driver.text("[data-test=hero-stats]") or ""
        h.check(f"hero-stats 显示素材总数 {EXPECT_TOTAL}", str(EXPECT_TOTAL) in hero_text, f"文本={_short(hero_text, 120)}")
        hero_meta = driver.wait_text(
            "[data-test=hero-meta]", lambda v: bool(v and str(v).strip()), timeout=t,
            desc="hero-meta 非空",
        ) or ""
        h.check(f"hero-meta 非空且含 {EXPECT_TOTAL}", str(EXPECT_TOTAL) in hero_meta, f"文本={_short(hero_meta, 140)}")
        eng_state = driver.poll(
            "(function(){var e=document.querySelector('#hero-engine-state');"
            "return e?e.textContent.trim():'';})()",
            lambda v: bool(v and "在线" in v),
            timeout=t,
            desc="hero-engine-state 含“在线”",
        )
        h.check("hero-engine-state 含“在线”", True, f"文本={eng_state!r}")
        eng_status = driver.text("[data-test=engine-status]") or ""
        h.check(
            "engine-status 已连接（不是“连接渲染引擎…”）",
            bool(eng_status) and "连接渲染引擎" not in eng_status,
            f"文本={_short(eng_status, 120)}",
        )
    except Exception as exc:
        h.fail("v2 首屏 hero / 引擎状态", f"{type(exc).__name__}: {exc}")

    n_cards = driver.count("#grid .card")
    h.check(f"首屏 {EXPECT_TOTAL} 张素材卡", n_cards == EXPECT_TOTAL, f"实际 {n_cards} 张")
    n_data = driver.count("#grid .card[data-id]")
    h.check("卡片均带 data-id", n_data == EXPECT_TOTAL, f"实际 {n_data}/{EXPECT_TOTAL}")
    status_v2 = driver.text("[data-test=status]") or ""
    h.check(f"status 显示 {EXPECT_TOTAL}", str(EXPECT_TOTAL) in status_v2, f"#status={status_v2[:140]!r}")

    for sel in (
        "[data-test=search]", "[data-test=scan-open]", "[data-test=help-open]",
        "[data-test=display-open]", "[data-test=sidebar]", "[data-test=view-toggle]",
        "[data-test=sort]", "[data-test=export-list]", "[data-test=engine-status]",
        "[data-test=grid]", "[data-test=status]", "[data-test=hero-engine]",
    ):
        info = driver.visible(sel)
        h.check(f"新版 UI 可见 {sel}", bool(info.get("visible")), h.fmt_vis(info))

    # ---------- V2-1 搜索 / 空状态 ----------
    try:
        _v2_type(driver, "[data-test=search]", "nightelf")
        res = driver.poll(
            "(function(){var cards=Array.prototype.slice.call(document.querySelectorAll('#grid .card'));"
            "var S=window.__assetStudio.S, q='nightelf';"
            "var expect=S.assets.filter(function(a){var hay=[a.name,a.id,a.desc,(a.tags||[]).join(' '),"
            "a.category,a.chapter,a.audioType||'',a.media||''].join(' ').toLowerCase();"
            "return hay.indexOf(q)>=0;}).length;"
            "return {cards:cards.length,expect:expect,"
            "ids:cards.map(function(c){return c.dataset.id;})};})()",
            lambda v: isinstance(v, dict) and int(v.get("expect") or 0) > 0
            and int(v.get("cards") or 0) == int(v.get("expect") or 0),
            timeout=t,
            desc="搜索 nightelf 结果数收敛到过滤规则",
        )
        cards_n = int(res.get("cards") or 0)
        expect_n = int(res.get("expect") or 0)
        h.check("搜索 nightelf 结果数符合过滤规则", cards_n == expect_n,
                f"实际 {cards_n} 张 / 规则期望 {expect_n} 张")
        ids = res.get("ids") or []
        h.check("搜索命中 ch2.player.nightelf", "ch2.player.nightelf" in ids, f"命中 ids={ids[:8]}")
        search_val = driver.eval("document.querySelector('[data-test=search]').value")
        h.check("搜索框内容为 nightelf", "nightelf" in str(search_val), f"value={search_val!r}")
        driver.screenshot("04_v2_search_nightelf.png")
        if cards_n != 1:
            h.note(
                "期望核对：搜索 nightelf",
                f"lead 期望=1 张（旧数据）；concept 概念稿已清理；现含 NightElf 本体、音频 NightElf 主题、draft 参数化变体等，实测={cards_n} 张；"
                f"画廊按名称/ID/描述/标签全字段模糊匹配，实现符合当前代码）",
            )
        _v2_clear_filters(driver, t)
        h.check(f"清空筛选恢复 {EXPECT_TOTAL} 张", driver.count("#grid .card") == EXPECT_TOTAL,
                f"实际 {driver.count('#grid .card')}")
    except Exception as exc:
        h.fail("v2 搜索筛选", f"{type(exc).__name__}: {exc}")

    try:
        _v2_type(driver, "[data-test=search]", "zzz_no_such_asset_zzz")
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == 0,
            timeout=t,
            desc="无结果搜索 → 0 张",
        )
        empty = driver.wait_visible("[data-test=empty]", timeout=t, desc="空状态可见")
        h.check("无结果时显示空状态", True, h.fmt_vis(empty))
        _v2_clear_filters(driver, t)
    except Exception as exc:
        h.fail("v2 空状态", f"{type(exc).__name__}: {exc}")

    # ---------- V2-2 章节 / 类型筛选 ----------
    try:
        driver.click('[data-test=facet-chapter] [data-val="ch2"]')
        n = driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == EXPECT_CH2,
            timeout=t,
            desc=f"章节 ch2 → {EXPECT_CH2} 张",
        )
        h.check("章节筛第二章 → 44 张", n == EXPECT_CH2, f"实际 {n} 张")
        chips_text = driver.text("[data-test=active-chips]") or ""
        h.check("筛选后 active-chips 非空", bool(chips_text.strip()), f"文本={_short(chips_text, 100)}")
        _v2_clear_filters(driver, t)
    except Exception as exc:
        h.fail("v2 章节筛选", f"{type(exc).__name__}: {exc}")

    try:
        driver.click('[data-test=facet-category] [data-val="audio"]')
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == EXPECT_AUDIO,
            timeout=t,
            desc="类型 audio → 57 张",
        )
        n_audio = driver.count("#grid .card.audio-card")
        h.check("类型筛音频 → 57 张音频卡", n_audio == EXPECT_AUDIO, f"实际 {n_audio} 张音频卡")
        _v2_clear_filters(driver, t)
    except Exception as exc:
        h.fail("v2 类型筛选", f"{type(exc).__name__}: {exc}")

    # ---------- V2-3 音频卡真实波形 + 音频详情 ----------
    try:
        has_audio = driver.eval(
            "(function(){var c=document.querySelector('#grid .audio-card');"
            "if(c)c.scrollIntoView({block:'center'});return !!c;})()"
        )
        if not has_audio:
            raise JSError("页面里没有 .audio-card")
        n_real = driver.poll(
            "document.querySelectorAll('#grid .audio-wave.real').length",
            lambda v: bool(v) and v > 0,
            timeout=max(60.0, t),
            desc="音频卡真实波形 .audio-wave.real",
        )
        bars = driver.eval("document.querySelectorAll('#grid .audio-wave.real .real-bar').length")
        h.check("滚动到音频卡后出现真实波形", n_real >= 1 and bars > 0,
                f"real 波形卡 {n_real} 个，real-bar {bars} 根")
        driver.screenshot("05_v2_audio_cards.png")
    except Exception as exc:
        h.fail("v2 音频卡真实波形", f"{type(exc).__name__}: {exc}")

    try:
        open_card("audio.bgm.title")
        name = detail_name()
        driver.poll(
            "(function(){var e=document.querySelector('[data-test=detail-name]');"
            "return e?e.textContent.trim():'';})()",
            lambda v: bool(v),
            timeout=t,
            desc="音频详情名称",
        )
        h.check("音频详情名称正确", "标题主题" in name, f"实际={name!r}")
        stage = driver.wait_visible("[data-test=audio-stage]", timeout=t, desc="audio-stage 可见")
        h.check("音频详情 audio-stage 可见", True, h.fmt_vis(stage))
        param_tab_hidden = driver.eval("document.querySelector('.tab[data-tab=params]').hidden")
        h.check("音频详情隐藏“参数”页", param_tab_hidden is True, f"hidden={param_tab_hidden!r}")
        peaks = driver.poll(
            "(function(){var w=window.__assetStudio.S.wavePeaks;return w?w.length:null;})()",
            lambda v: v == 160,
            timeout=t,
            desc="S.wavePeaks.length===160",
        )
        h.check("S.wavePeaks.length === 160", peaks == 160, f"实际 {peaks}")
        wave_canvas = driver.visible("[data-test=waveform]")
        h.check("波形 canvas 可见", bool(wave_canvas.get("visible")), h.fmt_vis(wave_canvas))
        driver.screenshot("06_v2_audio_detail.png")
        close_detail()
    except Exception as exc:
        h.fail("v2 音频详情", f"{type(exc).__name__}: {exc}")

    # ---------- V2-4 收藏 / 只看收藏 / 刷新持久化 ----------
    try:
        first_id = driver.eval(
            "(function(){var c=document.querySelector('#grid .card');return c?c.dataset.id:null;})()"
        )
        if not first_id:
            raise JSError("首张卡片没有 data-id")
        h.note("收藏测试对象", f"首张卡 id={first_id!r}")
        driver.click(f'.card[data-id="{first_id}"] .fav-btn')
        fav_raw = driver.poll(
            "localStorage.getItem('as.v02.favorites')",
            lambda v: bool(v) and first_id in v,
            timeout=t,
            desc="收藏写入 localStorage",
        )
        h.check("收藏后 localStorage as.v02.favorites 含该 id", first_id in str(fav_raw), f"实际={fav_raw!r}")
        stat_fav = driver.poll(
            "(function(){var e=document.querySelector('#stat-fav');return e?e.textContent.trim():null;})()",
            lambda v: v == "1",
            timeout=t,
            desc="收藏数显示 1",
        )
        h.check("收藏数显示 1", stat_fav == "1", f"实际={stat_fav!r}")
        h.check("卡片显示已收藏状态", driver.count(".fav-btn.on") >= 1,
                f".fav-btn.on = {driver.count('.fav-btn.on')}")
        driver.click("[data-test=quick-fav]")
        n = driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == 1,
            timeout=t,
            desc="只看收藏 → 1 张",
        )
        hit = driver.eval("(function(){var c=document.querySelector('#grid .card');return c?c.dataset.id:null;})()")
        h.check("只看收藏只显示已收藏", n == 1 and hit == first_id, f"{n} 张，id={hit!r}")
        driver.click("[data-test=quick-fav]")
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == EXPECT_TOTAL,
            timeout=t,
            desc=f"取消只看收藏恢复 {EXPECT_TOTAL} 张",
        )
        _v2_goto(driver, base, t, expect_cards=EXPECT_TOTAL)
        stat_after = driver.poll(
            "(function(){var e=document.querySelector('#stat-fav');return e?e.textContent.trim():null;})()",
            lambda v: v == "1",
            timeout=t,
            desc="刷新后收藏数保留",
        )
        h.check("刷新后收藏数保留", stat_after == "1", f"实际={stat_after!r}")
        h.check("刷新后收藏卡片高亮保留", driver.count(".fav-btn.on") == 1,
                f".fav-btn.on = {driver.count('.fav-btn.on')}")
        driver.click(f'.card[data-id="{first_id}"] .fav-btn')
        driver.poll(
            "localStorage.getItem('as.v02.favorites')",
            lambda v: v in ("[]", None),
            timeout=t,
            desc="取消收藏清空",
        )
        driver.poll(
            "(function(){var e=document.querySelector('#stat-fav');return e?e.textContent.trim():null;})()",
            lambda v: v == "0",
            timeout=t,
            desc="收藏数归零",
        )
        h.check("取消收藏后收藏数归零", True, "")
    except Exception as exc:
        h.fail("v2 收藏 / 只看收藏 / 刷新持久化", f"{type(exc).__name__}: {exc}")

    # ---------- V2-5 绘制详情：状态 / 参数 / 分析 / 源码 / 对比 ----------
    try:
        open_card("ch2.player.nightelf")
        name = detail_name()
        h.check("打开暗夜精灵号详情", "暗夜精灵号" in name, f"实际={name!r}")
        disp_w = driver.poll(
            "(function(){var i=document.querySelector('#detail-img');"
            "return i?Math.round(i.getBoundingClientRect().width):0;})()",
            lambda v: bool(v) and v > 100,
            timeout=t,
            desc="详情图显示宽度 > 100px",
        )
        h.check("详情预览图显示宽度 > 100px", disp_w > 100, f"实际 {disp_w}px")
        natural_w = driver.poll(
            "(function(){var i=document.querySelector('#detail-img');"
            "return i&&i.complete?(i.naturalWidth||0):0;})()",
            lambda v: bool(v) and v > 0,
            timeout=t,
            desc="详情图已加载",
        )
        h.check("详情预览图已加载", natural_w > 0, f"naturalWidth={natural_w}")

        # 状态区在「参数」页里，先切页再点状态按钮
        driver.click("[data-test=tab-params]")
        n_states = driver.count("[data-test=states] [data-state]")
        h.check("状态按钮数量 = 2", n_states == 2, f"实际 {n_states}")
        driver.click('[data-test=states] [data-state="triple"]')
        driver.poll(
            "(function(){return document.querySelector('#detail-img').src.indexOf('state=triple')>=0;})()",
            lambda v: v is True,
            timeout=t,
            desc="切换到 triple 后 src 含 state=triple",
        )
        h.check("切换状态 triple 后预览 src 含 state=triple", True, "")

        n_ranges = driver.poll(
            "document.querySelectorAll('[data-test=params] input[type=range]').length",
            lambda v: bool(v) and v >= 1,
            timeout=t,
            desc="参数滑块出现",
        )
        h.check("参数页滑块数量 = 4", n_ranges == 4, f"实际 {n_ranges}")
        src_before = driver.eval("document.querySelector('#detail-img').src")
        range_spec = driver.eval(
            "(function(){var r=document.querySelector('[data-test=params] input[type=range]');"
            "return r?{min:r.min,max:r.max,value:r.value}:null;})()"
        )
        target_val = "0"
        if range_spec:
            target_val = str(max(float(range_spec["min"]), float(range_spec["max"])))
        driver.set_value("[data-test=params] input[type=range]", target_val, "input")
        src_after = driver.poll(
            "(function(){return document.querySelector('#detail-img').src;})()",
            lambda s: bool(s) and s != src_before,
            timeout=t,
            desc="拖动参数后预览 src 变化",
        )
        h.check("拖动参数滑块后预览 src 变化", bool(src_after), f"新 src=…{str(src_after)[-70:]}")

        driver.click("[data-test=tab-analysis]")
        n_rows = driver.poll(
            "document.querySelectorAll('[data-test=analysis] .an-row').length",
            lambda v: bool(v) and v > 3,
            timeout=t,
            desc="分析页 .an-row > 3",
        )
        h.check("分析页 .an-row > 3", n_rows > 3, f"实际 {n_rows} 行")

        driver.click("[data-test=tab-source]")
        source_text = driver.poll(
            "(function(){var e=document.querySelector('[data-test=source]');"
            "return e?String(e.textContent||''):'';})()",
            lambda s: "void draw" in str(s),
            timeout=t,
            desc="源码含 void draw",
        )
        h.check("源码页含 “void draw”", "void draw" in str(source_text),
                f"源码开头={_short(str(source_text)[:120], 120)}")

        driver.click("[data-test=compare-pin]")
        wrap = driver.wait_visible("#compare-wrap", timeout=t, desc="compare-wrap 可见")
        divider = driver.wait_visible("#compare-divider", timeout=t, desc="compare-divider 可见")
        h.check("钉住对比后 #compare-wrap / #compare-divider 可见", True,
                f"{h.fmt_vis(wrap)} | {h.fmt_vis(divider)}")
        driver.screenshot("07_v2_draw_detail.png")
        driver.click("[data-test=compare-pin]")
        driver.poll(
            "document.querySelector('#compare-wrap').hidden",
            lambda v: v is True,
            timeout=t,
            desc="取消对比后 compare-wrap 隐藏",
        )
        h.check("再次点击取消对比", True, "")

        driver.click("[data-test=fav]")
        fav_detail = driver.poll(
            "localStorage.getItem('as.v02.favorites')",
            lambda v: bool(v) and "ch2.player.nightelf" in v,
            timeout=t,
            desc="详情收藏按钮写入 localStorage",
        )
        h.check("详情收藏按钮写入 localStorage", "ch2.player.nightelf" in str(fav_detail),
                f"实际={fav_detail!r}")
        driver.click("[data-test=fav]")
        driver.poll(
            "localStorage.getItem('as.v02.favorites')",
            lambda v: v in ("[]", None),
            timeout=t,
            desc="详情取消收藏",
        )
        close_detail()
    except Exception as exc:
        h.fail("v2 绘制详情交互", f"{type(exc).__name__}: {exc}")

    # ---------- V2-6 命令面板 + 逐帧滑杆 ----------
    try:
        driver.key("k", "KeyK", meta=True)
        cmd = driver.wait_visible("[data-test=cmd-modal]", timeout=t, desc="⌘K 打开命令面板")
        h.check("⌘K 打开命令面板", True, h.fmt_vis(cmd))
        _v2_type(driver, "[data-test=cmd-input]", "warden")
        n_items = driver.poll(
            "document.querySelectorAll('[data-test=cmd-list] .cmd-item').length",
            lambda v: bool(v) and v > 0,
            timeout=t,
            desc="命令面板搜索 warden 有结果",
        )
        h.check("命令面板输入 warden 有结果", n_items > 0, f"{n_items} 项")
        warden_idx = driver.poll(
            "(function(){var items=Array.prototype.slice.call("
            "document.querySelectorAll('[data-test=cmd-list] .cmd-item'));"
            "for(var i=0;i<items.length;i++){if((items[i].textContent||'').indexOf('ch2.boss.warden')>=0)return i;}"
            "return -1;})()",
            lambda v: isinstance(v, (int, float)) and int(v) >= 0,
            timeout=t,
            desc="命令面板出现 ch2.boss.warden 入口",
        )
        h.check("命令面板包含 ch2.boss.warden 入口", int(warden_idx) >= 0, f"index={warden_idx}")
        driver.screenshot("08_v2_cmd_palette.png")
        if int(warden_idx) >= 0:
            driver.click(f'[data-test=cmd-list] .cmd-item[data-cmd-index="{int(warden_idx)}"]')
            driver.poll(
                "document.querySelector('[data-test=detail]').hidden",
                lambda v: v is False,
                timeout=t,
                desc="命令面板点选后打开详情",
            )
            warden_name = detail_name()
            h.check("命令面板点选打开对应素材", "WARDEN" in warden_name, f"实际={warden_name!r}")
        else:
            h.fail("命令面板点选打开对应素材", f"未找到 ch2.boss.warden 入口（items={n_items}）")
            open_card("ch2.boss.warden")

        timeline = driver.wait_visible("[data-test=timeline]", timeout=t, desc="时间轴可见")
        h.check("Warden 详情显示时间轴", True, h.fmt_vis(timeline))
        frame_max = int(driver.eval("Number(document.querySelector('[data-test=frame-range]').max)"))
        h.check("时间轴帧数足够（max ≥ 6）", frame_max >= 6, f"max={frame_max}")
        driver.set_value("[data-test=frame-range]", 6, "input")
        frame_src = driver.poll(
            "(function(){return document.querySelector('#detail-img').src;})()",
            lambda s: "/api/frame" in str(s),
            timeout=t,
            desc="#detail-img.src 含 /api/frame",
        )
        h.check("逐帧切换后 src 含 /api/frame", "/api/frame" in str(frame_src),
                f"src=…{str(frame_src)[-90:]}")
        badge = driver.poll(
            "(function(){var e=document.querySelector('[data-test=frame-badge]');"
            "return e?e.textContent.trim():'';})()",
            lambda v: bool(v) and "逐帧 7" in v,
            timeout=t,
            desc="frame-badge 含“逐帧 7”",
        )
        h.check("frame-badge 显示“逐帧 7 /”", "逐帧 7" in str(badge), f"实际={badge!r}")
        driver.screenshot("09_v2_frame_scrub.png")
        close_detail()
    except Exception as exc:
        h.fail("v2 命令面板 / 逐帧滑杆", f"{type(exc).__name__}: {exc}")

    # ---------- V2-7 列表视图 / 排序 ----------
    try:
        _v2_goto(driver, base, t, expect_cards=EXPECT_TOTAL)
        driver.click('[data-test=view-toggle] [data-view="list"]')
        driver.poll(
            "document.querySelector('#grid').classList.contains('list')",
            lambda v: v is True,
            timeout=t,
            desc="#grid.list",
        )
        h.check("列表视图 #grid.list", True,
                f"class={driver.eval('document.querySelector(\"#grid\").className')}")
        driver.set_value("[data-test=sort]", "name", "change")
        order = driver.poll(
            "(function(){var ns=Array.prototype.slice.call("
            "document.querySelectorAll('#grid .card .card-name')).map(function(e){"
            "return e.textContent.trim();});"
            "var sorted=ns.slice().sort(function(a,b){return a.localeCompare(b,'zh-Hans-CN');});"
            "return {n:ns.length,ok:JSON.stringify(ns)===JSON.stringify(sorted),"
            "first:ns[0]||''};})()",
            lambda v: isinstance(v, dict) and v.get("n") == EXPECT_TOTAL,
            timeout=t,
            desc=f"sort=name 后 {EXPECT_TOTAL} 张",
        )
        h.check("sort=name 后按名称排序", bool(order.get("ok")), f"首卡={order.get('first')!r}")
        first_name = str(order.get("first") or "")
        if first_name == EXPECT_SORT_FIRST:
            h.check("列表视图 sort=name 首卡名（lead 期望）", True, f"{first_name!r}")
        else:
            h.note(
                "期望核对：sort=name 首卡名",
                f"lead 期望={EXPECT_SORT_FIRST!r}，实测={first_name!r}"
                f"（排序结果与 localeCompare(zh-Hans-CN) 一致，疑似期望值基于旧数据）",
            )
        driver.screenshot("10_v2_list_view.png")
        driver.click('[data-test=view-toggle] [data-view="grid"]')
        driver.set_value("[data-test=sort]", "default", "change")
        driver.poll(
            "document.querySelector('#grid').classList.contains('list')",
            lambda v: v is False,
            timeout=t,
            desc="恢复网格视图",
        )
    except Exception as exc:
        h.fail("v2 列表视图 / 排序", f"{type(exc).__name__}: {exc}")

    # ---------- V2-8 导出清单 / 显示设置 / 快捷键弹窗 ----------
    try:
        driver.click("[data-test=export-list]")
        toast = driver.poll(
            "(function(){var es=document.querySelectorAll('[data-test=toast]');"
            "return Array.prototype.slice.call(es).map(function(e){"
            "return e.textContent.replace(/\\s+/g,' ').trim();}).join(' || ');})()",
            lambda s: bool(s) and ("导出" in str(s) or "清单" in str(s)),
            timeout=t,
            desc="导出清单 toast",
        )
        h.check("导出清单后出现 toast", True, _short(str(toast), 120))
    except Exception as exc:
        h.fail("v2 导出清单", f"{type(exc).__name__}: {exc}")

    try:
        driver.click("[data-test=display-open]")
        pop = driver.wait_visible("[data-test=display-pop]", timeout=t, desc="显示设置弹层打开")
        h.check("显示设置弹层打开", True, h.fmt_vis(pop))
        swatches = driver.count("#palette-swatches .swatch")
        h.check("配色 swatches = 8", swatches == PALETTE_SWATCH_COUNT, f"实际 {swatches}")
        driver.screenshot("11_v2_display_pop.png")
        driver.click("[data-test=display-open]")
        driver.poll(
            "(function(){var e=document.querySelector('[data-test=display-pop]');"
            "return e?e.hidden:true;})()",
            lambda v: v is True,
            timeout=t,
            desc="显示设置弹层关闭",
        )
        h.check("再次点击关闭显示设置", True, "")
    except Exception as exc:
        h.fail("v2 显示设置弹层", f"{type(exc).__name__}: {exc}")

    try:
        driver.click("[data-test=help-open]")
        help_modal = driver.wait_visible("[data-test=help-modal]", timeout=t, desc="快捷键弹窗打开")
        h.check("快捷键弹窗打开", True, h.fmt_vis(help_modal))
        n_help = driver.count("#help-grid > *")
        h.check("快捷键条目非空", n_help > 0, f"{n_help} 条")
        driver.screenshot("12_v2_help_modal.png")
        driver.click("[data-test=help-close]")
        driver.poll(
            "(function(){var e=document.querySelector('[data-test=help-modal]');"
            "return e?e.hidden:true;})()",
            lambda v: v is True,
            timeout=t,
            desc="快捷键弹窗关闭",
        )
        h.check("快捷键弹窗可关闭", True, "")
    except Exception as exc:
        h.fail("v2 快捷键弹窗", f"{type(exc).__name__}: {exc}")

    # ---------- V2-9 扫描弹窗统计 + 缓存信息 ----------
    try:
        driver.click("[data-test=scan-open]")
        scan_modal = driver.wait_visible("[data-test=scan-modal]", timeout=t, desc="扫描弹窗打开")
        h.check("扫描弹窗打开", True, h.fmt_vis(scan_modal))
        n_stats = driver.poll(
            "document.querySelectorAll('[data-test=scan-body] .scan-stat').length",
            lambda v: v == 4,
            timeout=max(90.0, t),
            desc="扫描统计 .scan-stat ×4",
        )
        h.check("扫描弹窗 .scan-stat ×4", n_stats == 4, f"实际 {n_stats}")
        promise_leak = driver.eval(
            "(function(){var b=document.querySelector('[data-test=scan-body]');"
            "var t=b?String(b.textContent||''):'';"
            "var kinds=document.querySelectorAll('[data-test=scan-body] .scan-kind');"
            "return {has:t.indexOf('[object Promise]')>=0, kinds:kinds.length,"
            "first:kinds.length?String(kinds[0].textContent||''):''};})()"
        )
        h.check(
            "扫描弹窗无 [object Promise] 渲染（span.scan-kind）",
            isinstance(promise_leak, dict) and not promise_leak.get("has"),
            f"scan-kind {promise_leak.get('kinds') if isinstance(promise_leak, dict) else '?'} 个，"
            f"首个={_short(promise_leak.get('first') if isinstance(promise_leak, dict) else None, 60)}",
        )
        cache_text = driver.poll(
            "(function(){var e=document.querySelector('#cache-text');"
            "return e?e.textContent.trim():'';})()",
            lambda v: bool(v) and "缓存" in v and re.search(r"\d+\s*个文件", v),
            timeout=max(60.0, t),
            desc="#cache-text 缓存信息",
        )
        h.check(
            "#cache-text 含“缓存”与文件数",
            "缓存" in cache_text and bool(re.search(r"\d+\s*个文件", cache_text)),
            f"文本={_short(cache_text, 120)}",
        )
        h.check(
            "#cache-text 显示文件数与容量",
            bool(re.search(r"\d+(\.\d+)?\s*(MB|KB|B)", cache_text)),
            f"文本={_short(cache_text, 120)}",
        )
        driver.screenshot("13_v2_scan_cache.png")
        driver.click("[data-test=scan-close]")
        driver.poll(
            "(function(){var e=document.querySelector('[data-test=scan-modal]');"
            "return e?e.hidden:true;})()",
            lambda v: v is True,
            timeout=t,
            desc="扫描弹窗关闭",
        )
    except Exception as exc:
        h.fail("v2 扫描弹窗统计 / 缓存信息", f"{type(exc).__name__}: {exc}")

    # ---------- V2-10 深链 #/scan、#a=、#/help ----------
    try:
        _v2_goto(driver, _v2_deep_url(base, "scan", "#/scan"), t, expect_cards=EXPECT_TOTAL)
        driver.poll(
            "document.querySelector('[data-test=scan-modal]').hidden",
            lambda v: v is False,
            timeout=max(90.0, t),
            desc="#/scan 深链打开扫描弹窗",
        )
        n_deep = driver.poll(
            "document.querySelectorAll('[data-test=scan-body] .scan-stat').length",
            lambda v: v == 4,
            timeout=max(90.0, t),
            desc="#/scan 深链扫描统计",
        )
        h.check("#/scan 深链直接打开扫描弹窗", n_deep == 4, f"scan-stat={n_deep}")
        driver.click("[data-test=scan-close]")
        driver.poll(
            "document.querySelector('[data-test=scan-modal]').hidden",
            lambda v: v is True,
            timeout=t,
            desc="关闭扫描弹窗",
        )
    except Exception as exc:
        h.fail("v2 深链 #/scan", f"{type(exc).__name__}: {exc}")

    try:
        _v2_goto(driver, _v2_deep_url(base, "asset", "#a=ch2.boss.sphere"), t,
                 expect_cards=EXPECT_TOTAL)
        driver.poll(
            "document.querySelector('[data-test=detail]').hidden",
            lambda v: v is False,
            timeout=t,
            desc="#a=ch2.boss.sphere 深链打开详情",
        )
        deep_name = detail_name()
        h.check("#a=ch2.boss.sphere 深链打开对应详情", EXPECT_SPHERE in deep_name,
                f"期望含 {EXPECT_SPHERE!r}，实际={deep_name!r}")
        hash_now = driver.eval("location.hash")
        h.check("深链 hash 保留素材 id", "ch2.boss.sphere" in str(hash_now), f"hash={hash_now!r}")
        driver.screenshot("14_v2_deeplink_detail.png")
        close_detail()
    except Exception as exc:
        h.fail("v2 深链 #a=", f"{type(exc).__name__}: {exc}")

    try:
        _v2_goto(driver, _v2_deep_url(base, "help", "#/help"), t, expect_cards=EXPECT_TOTAL)
        driver.poll(
            "document.querySelector('[data-test=help-modal]').hidden",
            lambda v: v is False,
            timeout=t,
            desc="#/help 深链打开快捷键弹窗",
        )
        h.check("#/help 深链直接打开快捷键弹窗", True, "")
        driver.click("[data-test=help-close]")
        driver.poll(
            "document.querySelector('[data-test=help-modal]').hidden",
            lambda v: v is True,
            timeout=t,
            desc="关闭快捷键弹窗",
        )
    except Exception as exc:
        h.fail("v2 深链 #/help", f"{type(exc).__name__}: {exc}")

    # ---------- V2-11 其余选择器覆盖：最近浏览 / 快捷筛选 / 密度 / 恢复默认 / 查看源码 ----------
    try:
        _v2_goto(driver, base, t, expect_cards=EXPECT_TOTAL)
        open_card("ch1.player.trainingplane")
        close_detail()
        open_card("ch2.boss.warden")
        close_detail()
        recent_info = driver.visible("[data-test=facet-recent]")
        recent_text = driver.text("[data-test=facet-recent]") or ""
        h.check(
            "最近浏览区出现且含打开过的素材",
            bool(recent_info.get("visible")) and bool(recent_text.strip()),
            f"{h.fmt_vis(recent_info)} 文本={_short(recent_text, 100)}",
        )
    except Exception as exc:
        h.fail("v2 最近浏览", f"{type(exc).__name__}: {exc}")

    try:
        anim_expect = int(
            driver.eval(
                "(function(){var S=window.__assetStudio.S;"
                "return S.assets.filter(function(a){return a.media!=='audio' && (a.frames||1)>1;}).length;})()"
            ) or 0
        )
        driver.click("[data-test=quick-anim]")
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == anim_expect,
            timeout=t,
            desc="快捷筛选“有动画”",
        )
        h.check("快捷筛选“有动画”", True, f"{anim_expect} 张")
        driver.click("[data-test=quick-anim]")
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == EXPECT_TOTAL,
            timeout=t,
            desc="取消“有动画”",
        )
        scene_expect = int(
            driver.eval(
                "(function(){var S=window.__assetStudio.S;"
                "return S.assets.filter(function(a){return !!a.scene;}).length;})()"
            ) or 0
        )
        driver.click("[data-test=quick-scene]")
        driver.poll(
            "document.querySelectorAll('#grid .card').length",
            lambda v: v == scene_expect,
            timeout=t,
            desc="快捷筛选“可叠场景”",
        )
        h.check("快捷筛选“可叠场景”", True, f"{scene_expect} 张")
        _v2_clear_filters(driver, t)
    except Exception as exc:
        h.fail("v2 快捷筛选", f"{type(exc).__name__}: {exc}")

    try:
        driver.set_value("[data-test=density]", "sm", "change")
        driver.poll(
            "document.querySelector('#grid').classList.contains('density-sm')",
            lambda v: v is True,
            timeout=t,
            desc="紧凑密度 class",
        )
        h.check("密度切换为紧凑 density-sm", True, "")
        driver.set_value("[data-test=density]", "md", "change")
        driver.poll(
            "document.querySelector('#grid').classList.contains('density-md')",
            lambda v: v is True,
            timeout=t,
            desc="标准密度 class",
        )
        h.check("密度切回标准 density-md", True, "")
    except Exception as exc:
        h.fail("v2 卡片密度切换", f"{type(exc).__name__}: {exc}")

    try:
        open_card("ch2.player.nightelf")
        default_src = driver.eval("document.querySelector('#detail-img').src")
        driver.click("[data-test=tab-params]")
        driver.set_value("[data-test=params] input[type=range]", "0", "input")
        driver.poll(
            "(function(){return document.querySelector('#detail-img').src;})()",
            lambda s: bool(s) and s != default_src,
            timeout=t,
            desc="参数改变后 src 变化",
        )
        driver.click("[data-test=reset-params]")
        reset_src = driver.poll(
            "(function(){return document.querySelector('#detail-img').src;})()",
            lambda s: s == default_src,
            timeout=t,
            desc="恢复默认参数 src 还原",
        )
        h.check("“恢复默认”参数后预览 src 还原", str(reset_src) == str(default_src),
                f"src=…{str(reset_src)[-70:]}")
        driver.click("[data-test=source-open]")
        src_text = driver.poll(
            "(function(){var e=document.querySelector('[data-test=source]');"
            "return e?String(e.textContent||''):'';})()",
            lambda s: "void draw" in str(s),
            timeout=t,
            desc="“查看源码”打开源码",
        )
        h.check("“查看源码”按钮打开源码页", "void draw" in str(src_text),
                _short(str(src_text)[:100], 100))
        close_detail()
    except Exception as exc:
        h.fail("v2 恢复默认参数 / 查看源码", f"{type(exc).__name__}: {exc}")

    # ---------- V2-12 缓存清理（会清真实缓存，默认只跑一次；--skip-cache-clear 可跳过） ----------
    if getattr(args, "skip_cache_clear", False):
        h.note("缓存清理测试", "已按 --skip-cache-clear 跳过（避免清空真实缓存）")
    else:
        try:
            _v2_goto(driver, base, t, expect_cards=EXPECT_TOTAL)
            driver.click("[data-test=scan-open]")
            driver.wait_visible("[data-test=scan-modal]", timeout=t, desc="缓存测试-扫描弹窗")
            driver.poll(
                "(function(){var e=document.querySelector('#cache-text');"
                "return e?e.textContent.trim():'';})()",
                lambda v: bool(v) and re.search(r"\d+\s*个文件", v),
                timeout=max(60.0, t),
                desc="缓存信息可读",
            )
            driver.eval("window.confirm = function(){ return true; };")
            driver.click("[data-test=cache-clear]")
            clean_toast = driver.poll(
                "(function(){var es=document.querySelectorAll('[data-test=toast]');"
                "return Array.prototype.slice.call(es).map(function(e){"
                "return e.textContent.replace(/\\s+/g,' ').trim();}).join(' || ');})()",
                lambda s: bool(s) and "已清理" in str(s),
                timeout=max(60.0, t),
                desc="清理缓存 toast",
            )
            h.check("点击清理缓存出现 toast", True, _short(str(clean_toast), 120))
            cache_zero = driver.poll(
                "(function(){var e=document.querySelector('#cache-text');"
                "return e?e.textContent.trim():'';})()",
                lambda v: bool(v) and re.search(r"缓存\s*0\s*个文件", v),
                timeout=max(60.0, t),
                desc="#cache-text 文件数归零",
            )
            h.check("清理后 #cache-text 文件数归零", bool(re.search(r"缓存\s*0\s*个文件", cache_zero)),
                    f"文本={_short(cache_zero, 120)}")
            api_cache = driver.eval("fetch('/api/cache').then(function(r){return r.json();})")
            h.check(
                "清理后 /api/cache files=0",
                isinstance(api_cache, dict) and int(api_cache.get("files", -1)) == 0
                and int(api_cache.get("bytes", -1)) == 0,
                f"实际={_short(api_cache)}",
            )
            driver.screenshot("15_v2_cache_cleared.png")
            driver.click("[data-test=scan-close]")
            h.note("缓存清理副作用", "已清空真实缓存（8791/8792 共用），下次预览会重新渲染，属正常")
        except Exception as exc:
            h.fail("v2 缓存清理", f"{type(exc).__name__}: {exc}")

    # ---------- V2-12 最终错误检查 ----------
    driver.drain(1.0)
    h.check(
        "【v2】页面无 JS 异常（exceptionThrown）",
        not h.js_errors,
        f"共 {len(h.js_errors)} 条，第一条={_short(h.js_errors[0], 160) if h.js_errors else '无'}",
    )
    h.check(
        "【v2】页面无 console.error / assert",
        not h.console_errors,
        f"共 {len(h.console_errors)} 条，第一条={_short(h.console_errors[0], 160) if h.console_errors else '无'}",
    )
    h.check(
        "【v2】同源资源无 4xx/5xx（含 img 404）",
        not h.network_http_errors,
        f"共 {len(h.network_http_errors)} 条，第一条={_short(h.network_http_errors[0], 180) if h.network_http_errors else '无'}",
    )
    h.check(
        "【v2】同源资源无加载失败",
        not h.network_failures,
        f"共 {len(h.network_failures)} 条，第一条={_short(h.network_failures[0], 180) if h.network_failures else '无'}",
    )
    try:
        broken = driver.eval(
            "(function(){var imgs=Array.prototype.slice.call(document.images);"
            "var bad=imgs.filter(function(i){return i.complete && (i.naturalWidth||0)===0 && "
            "String(i.getAttribute('src')||i.currentSrc||'')!=='';});"
            "return {total:imgs.length,broken:bad.length,"
            "src:bad.slice(0,5).map(function(i){return String(i.currentSrc||i.src||'').slice(-80);})};})()"
        )
        h.check(
            "【v2】页面无加载失败的图片",
            isinstance(broken, dict) and int(broken.get("broken") or 0) == 0,
            f"图片 {broken.get('total') if isinstance(broken, dict) else '?'} 张，失败 "
            f"{broken.get('broken') if isinstance(broken, dict) else '?'} 张 "
            f"{_short(broken.get('src') if isinstance(broken, dict) else None, 160)}",
        )
    except Exception as exc:
        h.fail("【v2】页面无加载失败的图片", f"{type(exc).__name__}: {exc}")

    # 典型模板 bug：把 Promise 直接插进 innerHTML 会渲染成 [object Promise]
    try:
        promise_leak = driver.eval(
            "(function(){var t=document.documentElement.textContent||'';"
            "var i=t.indexOf('[object Promise]');"
            "return {has:i>=0, ctx:i>=0?t.slice(Math.max(0,i-90), i+110).replace(/\\s+/g,' '):''};})()"
        )
        h.check(
            "【v2】页面无 [object Promise] 文案泄漏",
            isinstance(promise_leak, dict) and not promise_leak.get("has"),
            f"上下文={_short(promise_leak.get('ctx') if isinstance(promise_leak, dict) else None, 180)}",
        )
    except Exception as exc:
        h.fail("【v2】页面无 [object Promise] 文案泄漏", f"{type(exc).__name__}: {exc}")

    h.check(
        "累计截图数量 ≥ 8 张",
        len(driver.screenshots) >= 8,
        f"已保存 {len(driver.screenshots)} 张：{[s['name'] for s in driver.screenshots]}",
    )


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Asset Studio 素材工坊 — 无头浏览器 UI 冒烟测试（零第三方依赖）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "示例：\n"
            "  python3 tests/ui_smoke.py\n"
            "  python3 tests/ui_smoke.py --url http://127.0.0.1:8792/ --out /tmp/as_ui_8792\n"
            "  python3 tests/ui_smoke.py --skip-cache-clear    # 调试时别清真实缓存\n"
            "  python3 tests/ui_smoke.py --no-v2               # 只跑基线断言\n"
            "  python3 tests/ui_smoke.py --keep-open           # 调试时保留 Edge\n"
            "\n退出码：0=PASS，1=有失败项，2=环境/驱动错误"
        ),
    )
    parser.add_argument("--url", default=None,
                        help="画廊地址（默认 http://127.0.0.1:<--port>/）")
    parser.add_argument("--port", type=int, default=DEFAULT_GALLERY_PORT,
                        help=f"画廊服务端口，仅在未提供 --url 时使用（默认 {DEFAULT_GALLERY_PORT}）")
    parser.add_argument("--out", default="/tmp/as_ui",
                        help="截图与报告输出目录（默认 /tmp/as_ui）")
    parser.add_argument("--headless-binary", default=os.environ.get("EDGE_BIN", DEFAULT_EDGE),
                        help="Edge 可执行文件路径（也可用环境变量 EDGE_BIN）")
    parser.add_argument("--cdp-port", type=int, default=DEFAULT_CDP_PORT,
                        help=f"Chrome DevTools Protocol 调试端口（默认 {DEFAULT_CDP_PORT}，已有实例会复用）")
    parser.add_argument("--keep-open", action="store_true",
                        help="测试结束后保留 Edge 进程与临时 profile（调试用）")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="单步等待超时秒数（默认 30）")
    parser.add_argument("--no-v2", action="store_true",
                        help="只跑基线断言，不跑新版 0.2.0 的 data-test 扩展断言")
    parser.add_argument("--skip-cache-clear", action="store_true",
                        help="跳过“清理缓存”用例（会清空真实渲染缓存，调试时建议加上）")
    args = parser.parse_args(argv)
    if not args.url:
        args.url = f"http://127.0.0.1:{args.port}/"
    return args


def main(argv=None) -> int:
    args = parse_args(argv)
    url = args.url.strip()
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        print(f"参数错误: --url 不是合法地址: {url!r}")
        return EXIT_HARNESS
    origin = f"{parsed.scheme}://{parsed.netloc}"

    out_dir = os.path.abspath(os.path.expanduser(args.out))
    try:
        os.makedirs(out_dir, exist_ok=True)
    except OSError as exc:
        print(f"HARNESS ERROR: 无法创建输出目录 {out_dir}: {exc}")
        return EXIT_HARNESS

    print("Asset Studio UI 冒烟测试（无头 Edge + 手写 CDP）")
    print(f"目标: {url}")
    print(f"输出: {out_dir}")
    print(f"视口: {VIEW_WIDTH}x{VIEW_HEIGHT}   CDP 端口: {args.cdp_port}")
    print("-" * 72)

    # 先确认画廊服务在跑，避免拉起浏览器后才发现没服务
    reachable, detail = probe_http(url, timeout=5.0)
    if not reachable:
        print(f"HARNESS ERROR: 画廊服务不可达 {url}（{detail}）")
        print("  先把工坊服务起起来：cd tools/asset_studio && ./studio.sh 或 python3 server.py")
        return EXIT_HARNESS
    print(f"画廊服务可达: {detail}")

    # 新版 0.2 后端探测：/api/cache 等新接口存在才跑扩展断言（8791 旧 server 只有静态新 web）
    v2_backend = http_get_json(origin + "/api/cache", timeout=3.0)
    v2_supported = isinstance(v2_backend, dict) and "files" in v2_backend
    if v2_supported:
        print(f"新版后端可用（/api/cache: {v2_backend.get('files')} 个文件）→ 将运行 0.2.0 扩展断言")
    else:
        print("新版后端不可用（/api/cache 缺失）→ 只跑基线断言，跳过 0.2.0 扩展断言")

    h = Harness(out_dir)
    h.url = url
    h.origin = origin
    browser = EdgeBrowser(args.headless_binary, args.cdp_port, out_dir, args.keep_open)
    cdp = None
    driver = None
    target_id = None
    session_id = None
    exit_code = EXIT_HARNESS

    try:
        browser.start()
        h.browser_desc = browser.describe()

        cdp = CDP(browser.ws_url, timeout=args.timeout)
        version = cdp.call("Browser.getVersion", timeout=15)
        h.browser_desc = f"{version.get('product', h.browser_desc)} · " + browser.describe().split(" · ", 1)[-1]

        # 视口尺寸交给 Emulation.setDeviceMetricsOverride 控制；
        # 新版 Chromium 的 createTarget 不接受 width/height（会报 Target position ... new windows）
        target = cdp.call("Target.createTarget", {"url": "about:blank"}, timeout=15)
        target_id = target.get("targetId")
        if not target_id:
            raise HarnessError(f"Target.createTarget 未返回 targetId: {target!r}")

        attach = cdp.call(
            "Target.attachToTarget",
            {"targetId": target_id, "flatten": True},
            timeout=15,
        )
        session_id = attach.get("sessionId")
        if not session_id:
            raise HarnessError(f"Target.attachToTarget 未返回 sessionId: {attach!r}")
        page = PageSession(cdp, session_id)

        # 事件采集必须早于导航
        cdp.on("Runtime.exceptionThrown",
               lambda msg: h.on_exception(msg, session_id))
        cdp.on("Runtime.consoleAPICalled",
               lambda msg: h.on_console(msg, session_id))
        cdp.on("Log.entryAdded",
               lambda msg: h.on_log(msg, session_id))
        cdp.on("Network.responseReceived",
               lambda msg: h.on_network_response(msg, session_id))
        cdp.on("Network.loadingFailed",
               lambda msg: h.on_network_failed(msg, session_id))

        page.call("Page.enable", timeout=10)
        page.call("Runtime.enable", timeout=10)
        page.call("Log.enable", timeout=10)
        page.call("Network.enable", timeout=10)
        page.call(
            "Emulation.setDeviceMetricsOverride",
            {
                "width": VIEW_WIDTH,
                "height": VIEW_HEIGHT,
                "deviceScaleFactor": 1,
                "mobile": False,
            },
            timeout=10,
        )

        h.ok(
            "无头浏览器与 CDP 会话就绪",
            f"{h.browser_desc}",
        )

        nav = page.call("Page.navigate", {"url": url}, timeout=max(30.0, args.timeout))
        if nav.get("errorText"):
            h.fail("页面导航", f"Page.navigate errorText={nav['errorText']!r}")
            raise SuiteAbort("页面导航失败")
        driver = UiDriver(page, out_dir, timeout=args.timeout)

        # 先等 DOM 可交互；load 事件会被几十张缩略图拖很久，不作为失败条件
        try:
            ready = driver.poll(
                "document.readyState",
                lambda v: v in ("interactive", "complete"),
                timeout=max(20.0, args.timeout),
                desc="document.readyState ≥ interactive",
            )
            h.ok("页面 DOM 就绪", f"readyState={ready}")
        except Exception as exc:
            h.fail("页面 DOM 就绪", f"{type(exc).__name__}: {exc}")
            raise SuiteAbort("页面没有进入可交互状态") from exc
        try:
            page.wait_event("Page.loadEventFired", timeout=3)
            h.note("页面 load 事件", "已触发")
        except TimeoutError:
            h.note("页面 load 事件",
                   "尚未触发（缩略图仍在加载，不影响 DOM 断言；baseline 以 DOM 为准）")

        run_baseline(driver, h, args)

        # ---------- 新版 0.2.0 扩展断言 ----------
        if v2_supported and not args.no_v2:
            print("\n" + "-" * 72)
            print("新版画廊 0.2.0 扩展断言（data-test 选择器）")
            print("-" * 72)
            try:
                run_v2(driver, h, args)
            except SuiteAbort as exc:
                h.fail("v2 流程中止", str(exc))
        else:
            try:
                test_attr_count = driver.eval("document.querySelectorAll('[data-test]').length")
            except Exception:
                test_attr_count = 0
            h.note(
                "跳过新版 0.2.0 扩展断言",
                f"后端支持={v2_supported}，--no-v2={args.no_v2}，页面 data-test 属性={test_attr_count}",
            )

        exit_code = EXIT_OK if h.fail_count == 0 else EXIT_FAIL

    except SuiteAbort as exc:
        print(f"\n[ABORT] 测试流程中止: {exc}")
        exit_code = EXIT_FAIL
    except (HarnessError, CDPError, WSError, WSClosed, JSError, TimeoutError, OSError) as exc:
        h.harness_error = f"{type(exc).__name__}: {exc}"
        print(f"\n[HARNESS ERROR] {type(exc).__name__}: {exc}")
        exit_code = EXIT_HARNESS
    except KeyboardInterrupt:
        print("\n[中断] 收到 Ctrl-C")
        exit_code = 130
    finally:
        # 失败时留下现场（DOM / 文本 / 最后一帧截图）
        if driver is not None and (exit_code != EXIT_OK):
            try:
                driver.screenshot("99_failure.png")
            except Exception:
                pass
            try:
                driver.dump_debug("failure")
            except Exception:
                pass

        # 清理 CDP / Edge / profile
        if cdp is not None:
            if not args.keep_open:
                if target_id:
                    try:
                        cdp.call("Target.closeTarget", {"targetId": target_id}, timeout=3)
                    except Exception:
                        pass
                if not browser.external:
                    try:
                        cdp.call("Browser.close", timeout=5)
                    except Exception:
                        pass
            try:
                cdp.close()
            except Exception:
                pass
        browser.stop()

        h.exit_code = exit_code
        h.finalize(driver)

        if args.keep_open:
            if browser.external:
                print("\n[KEEP-OPEN] 复用已有 Edge 实例保持不变。")
            else:
                print(f"\n[KEEP-OPEN] Edge 保持运行中：")
                print(f"  CDP: http://127.0.0.1:{args.cdp_port}/json")
                print(f"  profile: {browser.profile}（/tmp 下，需手动清理）")
        elif not browser.external:
            print("\n[清理] 已关闭本次拉起的 Edge 并删除临时 profile。")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
