#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
素材扫描器 —— 主游戏源码 vs 素材注册表 的交叉检查

作用：让「新增素材」这件事不会静默丢失。
  · 扫描游戏源码里所有的绘制入口（如 NightElf::draw、Ch2GateScene::drawDoor）
  · 与 registry/asset_registry.h 的登记项比对
  · 报出「源码里有、注册表里没有」的入口  → 提醒登记（这就是"自动读取新素材"）
  · 也报出「注册表里有、源码里已消失」的入口 → 防止你重构后注册表腐烂

用法：
    python3 scanner.py                  # 人读报告
    python3 scanner.py --json           # 机器读（给网页画廊用）
    python3 scanner.py --suggest        # 为未登记的入口生成注册项草稿
"""

import os
import re
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL_DIR = os.path.dirname(HERE)
GAME_DIR = os.path.normpath(os.path.join(TOOL_DIR, "..", "..", "v2.0.0", "multiple code files"))
REGISTRY = os.path.join(HERE, "asset_registry.h")

# 绘制入口的函数名模式：draw / drawEnemy / drawStars / drawHPBar ...
DRAW_FN = re.compile(r'\b(?:static\s+)?void\s+(draw[A-Za-z0-9_]*)\s*\(')
# 音效入口：AudioEngine 内的 void sndXxx()
SFX_FN = re.compile(r'\bvoid\s+(snd[A-Za-z0-9_]*)\s*\(\s*\)')
CLASS_DECL = re.compile(r'\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?::[^;{]*)?\{')
# registry 里 source 字段形如 ".../player.h:215 (NightElf::draw)"
SRC_TOKEN = re.compile(r'\(([^)]*)\)')


def strip_comments(text):
    """去掉注释和字符串字面量，避免花括号计数被干扰"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif c == '"' or c == "'":
            q = c
            i += 1
            while i < n and text[i] != q:
                if text[i] == '\\':
                    i += 1
                i += 1
            i += 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def scan_file(path, relpath):
    """返回该文件里的绘制入口：[(Class::method 或 method, 行号)]"""
    try:
        raw = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return []

    # 行号必须在剥离注释前统计，所以记录原文本的行边界
    lines = raw.split('\n')
    clean = strip_comments(raw)

    def line_of(pos):
        return clean.count('\n', 0, pos) + 1

    # 建立「位置 -> 所属类」映射：逐字符扫花括号，维护类栈
    owners = []            # [(start_pos, end_pos, classname)]
    stack = []             # [(brace_depth_entered, classname or None)]
    depth = 0
    pending_class = None

    for m in CLASS_DECL.finditer(clean):
        pending_class = m.group(1)
        # 找到该 class 的起始花括号
        brace = clean.find('{', m.end() - 1)
        if brace >= 0:
            owners.append([brace, None, pending_class])

    # 用简单的括号配对给每个类找结束位置
    for owner in owners:
        start = owner[0]
        d = 0
        i = start
        while i < len(clean):
            if clean[i] == '{':
                d += 1
            elif clean[i] == '}':
                d -= 1
                if d == 0:
                    owner[1] = i
                    break
            i += 1
        if owner[1] is None:
            owner[1] = len(clean)

    def owner_of(pos):
        best = None
        for s, e, name in owners:
            if s <= pos <= e:
                if best is None or (e - s) < best[1] - best[0]:
                    best = (s, e, name)
        return best[2] if best else None

    results = []
    for m in DRAW_FN.finditer(clean):
        fn = m.group(1)
        pos = m.start()
        cls = owner_of(pos)
        token = f"{cls}::{fn}" if cls else fn
        results.append((token, line_of(pos)))
    return results


def scan_audio_sfx():
    """扫描 audio.h 里所有无参数 snd* 音效方法。"""
    path = os.path.join(GAME_DIR, "audio.h")
    try:
        raw = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return []
    clean = strip_comments(raw)
    rel = os.path.relpath(path, os.path.dirname(GAME_DIR))
    out = []
    seen = set()
    for m in SFX_FN.finditer(clean):
        token = "AudioEngine::" + m.group(1)
        if token in seen:
            continue
        seen.add(token)
        out.append({"token": token, "file": rel, "line": clean.count("\n", 0, m.start()) + 1})
    return out


def scan_game():
    """扫描整个游戏源码目录"""
    entries = []
    for root, dirs, files in os.walk(GAME_DIR):
        dirs[:] = [d for d in dirs if not d.startswith('.')]
        for f in sorted(files):
            if not f.endswith(('.h', '.cpp')):
                continue
            if 'archive' in f or 'deprecated' in f:
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.dirname(GAME_DIR))
            for token, line in scan_file(full, rel):
                entries.append({"token": token, "file": rel, "line": line})
    return entries


def read_registry_entries():
    """从注册表目录下所有头文件里抽出 reg("id", ...) 的 id 与 source 字段

    注意：登记项分散在 reg_players.h / reg_enemies.h / ... 等多个文件里，
    所以这里扫描整个 registry 目录，而不是只读 asset_registry.h。
    """
    out = []
    files = []
    if os.path.isdir(HERE):
        for name in sorted(os.listdir(HERE)):
            if name.endswith(".h") and not name.startswith("_"):
                files.append(os.path.join(HERE, name))
    # 草稿区也可能有登记项
    drafts = os.path.join(TOOL_DIR, "drafts")
    if os.path.isdir(drafts):
        for name in sorted(os.listdir(drafts)):
            d = os.path.join(drafts, name)
            if os.path.isdir(d) and not name.startswith((".", "_")):
                draw_h = os.path.join(d, "draw.h")
                if os.path.isfile(draw_h):
                    files.append(draw_h)

    for path in files:
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        rel = os.path.relpath(path, TOOL_DIR)
        # C++ 允许把长字符串拆成相邻字面量跨行书写（"abc" "def"），
        # 先合并它们，否则正则匹配不到 .from("...") 的内容。
        text = re.sub(r'"\s*\n\s*"', '', text)
        text = re.sub(r'"\s+"', '', text)
        for m in re.finditer(
                r'\breg\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\)', text):
            start = m.end()
            nxt = text.find("reg(", start)
            block = text[start: nxt if nxt > 0 else len(text)]
            src = ""
            sm = re.search(r'\.from\(\s*"([^"]*)"\s*\)', block)
            if sm:
                src = sm.group(1)
            tokens = []
            tm = SRC_TOKEN.search(src)
            if tm:
                tokens = [t.strip() for t in tm.group(1).split(",") if t.strip()]
            out.append({
                "id": m.group(1),
                "name": m.group(2),
                "chapter": m.group(3),
                "category": m.group(4),
                "source": src,
                "tokens": tokens,
                "file": rel,
            })

    # 音频登记不写 57 条重复 reg(...)，而是来自 audio_manifest.h 的 X-macro。
    # 这里按同一份清单反推音频条目，才能和 snd* 方法做覆盖交叉检查。
    manifest = os.path.join(HERE, "audio_manifest.h")
    if os.path.isfile(manifest):
        try:
            text = open(manifest, encoding="utf-8", errors="replace").read()
        except OSError:
            text = ""
        # X-macro 的换行续行反斜杠不属于字段内容，先合并成一行。
        text = re.sub(r'\\\s*\n\s*', ' ', text)
        audio_entry = re.compile(
            r'X\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*'
            r'(?:"([^"]+)"|([A-Za-z_][A-Za-z0-9_]*))\s*,\s*([0-9.]+)\s*,\s*'
            r'"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\)')
        for m in audio_entry.finditer(text):
            aid, name, chapter, bgm_key, sfx_key = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
            src = m.group(9)
            tokens = []
            if sfx_key:
                tokens = ["AudioEngine::" + sfx_key]
            else:
                tm = SRC_TOKEN.search(src)
                if tm:
                    tokens = [t.strip() for t in tm.group(1).split(",") if t.strip()]
            out.append({
                "id": aid,
                "name": name,
                "chapter": chapter,
                "category": "audio",
                "source": src,
                "tokens": tokens,
                "file": os.path.relpath(manifest, TOOL_DIR),
            })
    return out


def main():
    game = scan_game()
    regs = read_registry_entries()
    draw_regs = [r for r in regs if r["category"] != "audio"]
    audio_regs = [r for r in regs if r["category"] == "audio"]

    covered_tokens = set()
    for r in draw_regs:
        for t in r["tokens"]:
            covered_tokens.add(t)

    game_tokens = set(e["token"] for e in game)

    uncovered = [e for e in game if e["token"] not in covered_tokens]

    # 注册表指向了源码里已不存在的绘制入口（重构后腐烂）
    stale = []
    for r in draw_regs:
        for t in r["tokens"]:
            if t not in game_tokens:
                stale.append({"id": r["id"], "token": t})

    # ---- 音频覆盖检查 ----
    audio_game = scan_audio_sfx()
    audio_game_tokens = set(e["token"] for e in audio_game)
    audio_registered_tokens = set()
    for r in audio_regs:
        for t in r["tokens"]:
            if t.startswith("AudioEngine::snd"):
                audio_registered_tokens.add(t)
    audio_uncovered = [e for e in audio_game if e["token"] not in audio_registered_tokens]
    audio_stale = []
    for r in audio_regs:
        for t in r["tokens"]:
            if t.startswith("AudioEngine::snd") and t not in audio_game_tokens:
                audio_stale.append({"id": r["id"], "token": t})
    audio_bgm_count = len([r for r in audio_regs if r["id"].startswith("audio.bgm.")])

    as_json = "--json" in sys.argv

    if as_json:
        print(json.dumps({
            "gameDir": GAME_DIR,
            "gameDrawCount": len(game),
            "registeredCount": len(draw_regs),
            "registeredTokens": sorted(covered_tokens),
            "coveredCount": len([e for e in game if e["token"] in covered_tokens]),
            "uncovered": uncovered,
            "stale": stale,
            "audioGameSfxCount": len(audio_game),
            "audioRegisteredSfxCount": len(audio_game_tokens & audio_registered_tokens),
            "audioBgmCount": audio_bgm_count,
            "audioTotal": len(audio_regs),
            "audioUncovered": audio_uncovered,
            "audioStale": audio_stale,
        }, ensure_ascii=False, indent=2))
        return 0

    print("=" * 66)
    print("素材扫描器 —— 游戏源码 vs 素材注册表")
    print("=" * 66)
    print(f"游戏源码: {GAME_DIR}")
    print(f"绘制入口总数: {len(game)}")
    print(f"注册表登记项: {len(regs)}  (覆盖 {len(game) - len(uncovered)} 个入口)")
    print()

    if uncovered:
        print(f"⚠ 未登记的绘制入口 ({len(uncovered)} 个) —— 这些素材在画廊里看不到：")
        by_file = {}
        for e in uncovered:
            by_file.setdefault(e["file"], []).append(e)
        for f in sorted(by_file):
            print(f"\n  {f}")
            for e in sorted(by_file[f], key=lambda x: x["line"]):
                print(f"    {e['line']:5d}  {e['token']}")
    else:
        print("✓ 所有绘制入口都已登记")

    if stale:
        print(f"\n⚠ 注册表指向了已不存在的入口 ({len(stale)} 个) —— 源码重构后需要更新登记：")
        for s in stale:
            print(f"    {s['id']}  →  {s['token']}")

    print()
    print(f"音频入口总数: {len(audio_game)} 个 snd* 音效 + {audio_bgm_count} 条 BGM 试听登记")
    if audio_uncovered:
        print(f"⚠ 未登记的音频入口 ({len(audio_uncovered)} 个) —— 这些音效在画廊里听不到：")
        for e in sorted(audio_uncovered, key=lambda x: x["line"]):
            print(f"    {e['file']}:{e['line']}  {e['token']}")
    else:
        print("✓ 所有 snd* 音效都已登记")
    if audio_stale:
        print(f"⚠ 音频注册表指向了已不存在的方法 ({len(audio_stale)} 个)：")
        for s in audio_stale:
            print(f"    {s['id']}  →  {s['token']}")
    print()

    if "--suggest" in sys.argv and uncovered:
        print("=" * 66)
        print("注册项草稿（复制到 registry/asset_registry.h 后补上绘制实现）")
        print("=" * 66)
        for e in uncovered:
            cid = f"{e['file'].split('/')[-1].replace('.h','')}.{e['token'].replace('::','.')}"
            print(f"""
    // TODO: 补绘制实现
    reg("{cid.lower()}", "{e['token']}", "?", "?")
        .from("{e['file']}:{e['line']} ({e['token']})")
        .tag("待分类")
        .drawFn(nullptr);""")

    return 0


if __name__ == "__main__":
    sys.exit(main())
